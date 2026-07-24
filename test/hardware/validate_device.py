#!/usr/bin/env python3
"""Batterie de validation contre un device arrosage_fw reel, via son serveur
web de test (composant components/web, voir CONFIG_ARROSAGE_ENABLE_WEB_SERVER).

Ne necessite aucune dependance externe (urllib de la stdlib Python 3
uniquement) : a lancer depuis n'importe quel poste sur le meme reseau que
le device, une fois celui-ci flashe et connecte au WiFi.

Usage:
    python validate_device.py <ip_du_device> [--port 80] [--duration 20]

Ce script exerce exactement le meme format JSON que le contrat MQTT
(voir docs/mqtt_contract.md) : ce qui est valide ici est representatif de
ce qu'enverra plus tard le vrai backend sur arrosage/<device_id>/commande.

Ne remplace pas docs/bring_up_checklist.md (verification physique des
capteurs/relais) : ce script valide le comportement logiciel du firmware
(API HTTP, parsing de commandes, etat des vannes), pas le materiel.
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

EXPECTED_STATE_KEYS = {
    "ts",
    "water_level_cm",
    "humidity_pct",
    "temperature_c",
    "battery_v",
}

results = []


def record(name, ok, detail=""):
    results.append((name, ok, detail))
    status = "PASS" if ok else "FAIL"
    print(f"[{status}] {name}" + (f" - {detail}" if detail and not ok else ""))


def http_get(base_url, path, timeout):
    with urllib.request.urlopen(base_url + path, timeout=timeout) as resp:
        return resp.status, resp.read()


def http_post(base_url, path, payload_obj, timeout):
    data = json.dumps(payload_obj).encode("utf-8")
    req = urllib.request.Request(base_url + path, data=data, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def http_post_raw(base_url, path, raw_bytes, timeout):
    req = urllib.request.Request(base_url + path, data=raw_bytes, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def get_state(base_url, timeout):
    status, body = http_get(base_url, "/api/state", timeout)
    if status != 200:
        raise RuntimeError(f"GET /api/state a renvoye {status}")
    return json.loads(body.decode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("ip", help="Adresse IP du device (voir les logs serie ou le routeur)")
    parser.add_argument("--port", type=int, default=80)
    parser.add_argument("--timeout", type=float, default=5.0, help="Timeout HTTP par requete (s)")
    parser.add_argument(
        "--open-duration",
        type=int,
        default=20,
        help="Duree (s) demandee pour les tests d'ouverture - doit etre assez longue pour laisser le temps aux verifications avant l'auto-fermeture",
    )
    args = parser.parse_args()

    if args.open_duration < 3:
        parser.error(
            "--open-duration doit etre >= 3s : une valeur trop courte risque de laisser "
            "la vanne se refermer via son propre timer avant la verification suivante, "
            "ce qui fausserait le resultat du test stop_all (faux PASS)."
        )

    base_url = f"http://{args.ip}:{args.port}"
    print(f"Validation de {base_url} ...\n")

    # 1. GET /api/state renvoie toutes les cles attendues du contrat
    try:
        state = get_state(base_url, args.timeout)
        missing = EXPECTED_STATE_KEYS - state.keys()
        vanne_keys = sorted(k for k in state if k.startswith("vanne_"))
        ok = not missing and len(vanne_keys) > 0
        detail = f"cles manquantes: {missing}" if missing else (
            "aucune cle vanne_* trouvee" if not vanne_keys else f"vannes: {vanne_keys}"
        )
        record("GET /api/state renvoie toutes les cles attendues", ok, detail)
    except Exception as e:
        record("GET /api/state renvoie toutes les cles attendues", False, str(e))
        print("\nEchec de connexion initiale, arret des tests suivants.")
        sys.exit(1)

    valve_key = vanne_keys[0]

    # 2. Fermeture initiale (etat de depart connu) puis ouverture
    http_post(base_url, "/api/command", {"vanne": valve_key, "action": "close"}, args.timeout)
    time.sleep(0.5)

    status, _ = http_post(
        base_url, "/api/command",
        {"vanne": valve_key, "action": "open", "duration_s": args.open_duration},
        args.timeout,
    )
    time.sleep(0.5)
    state = get_state(base_url, args.timeout)
    record(
        f"POST open {valve_key} -> etat passe a 'open'",
        status == 200 and state.get(valve_key) == "open",
        f"HTTP {status}, etat={state.get(valve_key)}",
    )

    # 3. Fermeture explicite
    status, _ = http_post(base_url, "/api/command", {"vanne": valve_key, "action": "close"}, args.timeout)
    time.sleep(0.5)
    state = get_state(base_url, args.timeout)
    record(
        f"POST close {valve_key} -> etat repasse a 'closed'",
        status == 200 and state.get(valve_key) == "closed",
        f"HTTP {status}, etat={state.get(valve_key)}",
    )

    # 4. Arret d'urgence : ouvrir TOUTES les vannes connues (pas seulement la
    # premiere) puis stop_all - sinon un bug qui sauterait un index dans
    # valve_manager_close_all() passerait inapercu (les vannes jamais
    # ouvertes sont "fermees" qu'elles aient ete traitees ou non).
    for key in vanne_keys:
        http_post(base_url, "/api/command", {"vanne": key, "action": "open", "duration_s": args.open_duration},
                  args.timeout)
    time.sleep(0.5)
    state = get_state(base_url, args.timeout)
    all_opened = all(state.get(k) == "open" for k in vanne_keys)
    record(
        "ouverture de toutes les vannes avant le test stop_all",
        all_opened,
        f"etats={[(k, state.get(k)) for k in vanne_keys]}",
    )

    status, _ = http_post(base_url, "/api/command", {"action": "stop_all"}, args.timeout)
    time.sleep(0.5)
    state = get_state(base_url, args.timeout)
    all_closed = all(state.get(k) == "closed" for k in vanne_keys)
    record(
        "POST stop_all -> toutes les vannes fermees",
        status == 200 and all_closed,
        f"HTTP {status}, etats={[(k, state.get(k)) for k in vanne_keys]}",
    )

    # 5. Robustesse du parseur : payloads invalides rejetes en HTTP 400
    invalid_cases = [
        ("JSON malforme", b"{ceci n'est pas du json"),
        ("open sans duration_s", json.dumps({"vanne": valve_key, "action": "open"}).encode()),
        ("action inconnue", json.dumps({"action": "do_something"}).encode()),
        ("payload vide", b""),
    ]
    for label, raw in invalid_cases:
        status, _ = http_post_raw(base_url, "/api/command", raw, args.timeout)
        record(f"payload invalide rejete ({label}) -> HTTP 400", status == 400, f"HTTP {status}")

    # Bilan
    total = len(results)
    passed = sum(1 for _, ok, _ in results if ok)
    print(f"\n{passed}/{total} tests OK")
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()

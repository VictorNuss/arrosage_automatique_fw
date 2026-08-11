# Batterie de test / bring-up materiel

Objectif : valider le firmware progressivement sur le materiel reel, du plus
simple (GPIO seul) au plus complexe (integration MQTT complete), avant de
faire confiance a l'installation en conditions reelles.

## 0. Tests logiques (sans materiel)

Avant toute manipulation materielle, valider la logique pure sur l'hote
(target `linux` d'ESP-IDF, aucune carte necessaire) - necessite un
compilateur natif, voir [`test/run_wsl_tests.sh`](../test/run_wsl_tests.sh)
et le README (section Tests) pour la procedure verifiee sous WSL :

```
bash test/run_wsl_tests.sh
```

Couvre `command_parser` (JSON valides/invalides/malformes/champs manquants)
et `valve_logic` (clamp de duree, recherche de vanne par cle) - 33 tests,
tous verifies au vert. Si ces tests echouent, ne pas continuer sur le
materiel.

## 1. Mode diagnostic embarque

Activer la console de diagnostic avant de flasher le firmware normal :

```
idf.py menuconfig
# -> Arrosage Firmware Configuration -> Activer la console de diagnostic (y)
idf.py -p COM3 build flash monitor
```

Au prompt `arrosage>` :

```
arrosage> valve open 0 10      # ouvre la vanne d'index 0 pendant 10s
arrosage> valve close 0        # ferme la vanne d'index 0
arrosage> sensor                # affiche le JSON de tous les capteurs
```

## 2. GPIO / relais seuls

Via la console diagnostic (`valve open <n> <sec>` / `valve close <n>`) :
verifier au multimetre/a l'oreille que le bon relais s'active pour chaque
index, sans encore se soucier des capteurs ni du reseau.

## 3. Vanne + auto-fermeture par timer

Ouvrir une vanne pour une duree courte (`valve open 0 5`) et verifier
qu'elle se referme automatiquement apres 5s sans intervention. Tester le
clamp de securite en demandant une duree superieure au max configure (ex.
`valve open 2 99999` sur la vanne "Serre", max 600s) : elle doit se refermer
apres 600s, pas 99999s.

## 4. Capteur ultrason seul

`sensor` dans la console, comparer `water_level_cm` a une mesure au metre
ruban (remplir/vider partiellement la cuve). Ajuster
`CONFIG_ARROSAGE_TANK_HEIGHT_CM` si l'ecart ne correspond pas a la hauteur
reelle de la cuve.

## 5. Capteurs sol / temperature seuls

`sensor` dans la console : verifier que `temperature_c` est plausible (DS18B20).
Pour `humidity_pct`, tremper le capteur dans l'eau puis le laisser a l'air
libre, ajuster `kRawDry`/`kRawWet` dans
`components/sensors/priv_include/soil_humidity_sensor.h` si les valeurs
extremes (0% / 100%) ne sont pas atteintes.

## 6. WiFi seul

Desactiver le mode diagnostic (`ARROSAGE_ENABLE_DIAG_CONSOLE=n`), configurer
SSID/mot de passe via `idf.py menuconfig`, verifier/adapter l'IP fixe
(`ARROSAGE_WIFI_STATIC_IP` et associes - **choisir une IP hors de la plage
DHCP du routeur** pour eviter tout conflit avec un autre appareil), flasher,
verifier dans les logs (`idf.py monitor`) l'obtention de cette IP exacte
("IP obtenue : ..."). Verifier aussi que le device reste bien joignable
(`ping <ip_fixe>`) apres un redemarrage.

## 7. Serveur web de test

Des que le device a une IP (etape precedente), le serveur web de test est
deja actif (`CONFIG_ARROSAGE_ENABLE_WEB_SERVER=y` par defaut) - pas besoin
d'attendre que MQTT soit configure pour commencer a tester :

```
http://<ip_du_device>/
```

Verifier que la page affiche les capteurs et les vannes, tester
ouverture/fermeture/arret d'urgence depuis les boutons. C'est le meme
parseur de commande et la meme table de vannes que MQTT : ce qui fonctionne
ici fonctionnera a l'identique via le backend.

## 8. Synchronisation NTP

Toujours dans les logs : verifier le message "Heure synchronisee via NTP".
Sans cela, le champ `ts` publie refletera l'epoque par defaut (1970).

## 9. MQTT etat seul

Sur une machine avec `mosquitto-clients` installe :

```
mosquitto_sub -h <broker_ip> -t 'arrosage/<device_id>/etat' -v
```

Verifier qu'un message JSON complet (toutes les cles du contrat) arrive
toutes les ~60s.

## 10. MQTT commande seul

```
mosquitto_pub -h <broker_ip> -t 'arrosage/<device_id>/commande' \
  -m '{"vanne":"vanne_1","action":"open","duration_s":10}'

mosquitto_pub -h <broker_ip> -t 'arrosage/<device_id>/commande' \
  -m '{"vanne":"vanne_1","action":"close"}'

mosquitto_pub -h <broker_ip> -t 'arrosage/<device_id>/commande' \
  -m '{"action":"stop_all"}'
```

Verifier dans les logs que chaque commande est bien recue/parsee, puis
envoyer volontairement un payload invalide (`-m 'n''importe quoi'`) pour
verifier qu'il est ignore proprement (log warning, pas de crash).

## 11. Vue debug globale et integration complete

```
mosquitto_sub -h <broker_ip> -t 'arrosage/#' -v
```

Faire tourner l'installation complete (mode normal, `ARROSAGE_ENABLE_DIAG_CONSOLE=n`)
plusieurs heures avec coupures WiFi/broker provoquees manuellement, en
surveillant :
- la reconnexion automatique (WiFi et MQTT) apres coupure ;
- l'absence de fuite memoire (`esp_get_free_heap_size()` loggue
  periodiquement, a ajouter temporairement si besoin) ;
- que l'etat des vannes publie correspond bien a leur etat physique reel
  apres une commande, y compris apres une auto-fermeture par timer.

## 12. Validation automatisee (script Python)

Une fois les etapes 6 a 10 franchies au moins une fois manuellement, le
script suivant permet de rejouer automatiquement l'essentiel des
verifications logicielles (etat complet, ouverture/fermeture, arret
d'urgence, rejet des payloads invalides) a chaque nouveau flash, sans tout
reverifier a la main :

```
python test/hardware/validate_device.py <ip_du_device>
```

Ne remplace pas la verification physique (etapes 2 a 5) : ce script ne
verifie que le comportement logiciel du firmware via son serveur web de
test, pas la realite des mesures ni des relais.

## 13. OTA (mise a jour firmware par WiFi)

Active par defaut (`CONFIG_ARROSAGE_ENABLE_OTA=y`) ; a sauter uniquement si
desactive volontairement. Necessite d'avoir deja reflashe le device une fois
avec la table de partitions OTA (voir README.md section 7, `idf.py
erase-flash` puis `flash` initial).

1. Verifier qu'un premier upload reussit et que le device redemarre sur la
   nouvelle image :
   ```
   curl -X POST --data-binary @build/arrosage_fw.bin http://<ip_du_device>/api/ota
   ```
2. Verifier dans les logs serie que l'image demarre normalement (WiFi, MQTT,
   pas de boot loop).
3. Tester le rollback de securite : flasher volontairement une image qui
   plante avant `ota_confirm_boot_ok()` (ex. ajouter temporairement un
   `abort()` juste apres `net_events_init()` dans `app_main.cpp`), l'envoyer
   via `/api/ota`, et verifier qu'apres le crash-boot-loop le bootloader
   revient automatiquement sur l'image precedente (visible dans les logs de
   boot : message de rollback) plutot que de rester bloque en boucle de crash.

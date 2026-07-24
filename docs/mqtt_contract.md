# Contrat MQTT

Le broker est un Mosquitto local, sans TLS, acces anonyme.

## Topic etat (device -> serveur) — FIGE, ne pas devier

```
arrosage/<device_id>/etat
```

Publie toutes les `CONFIG_ARROSAGE_STATE_PUBLISH_INTERVAL_S` secondes (60s
par defaut), QoS 1, **retain=true** (le dashboard voit l'etat immediatement
a la reconnexion sans attendre jusqu'a 60s).

Payload JSON plat, une cle par mesure, **toutes les cles sont toujours
presentes** (y compris en cas d'echec de lecture d'un capteur - voir
`sensor_manager_collect()`, qui republie alors la derniere valeur connue) :

```json
{
  "ts": "2026-07-16T10:00:00Z",
  "water_level_cm": 34.5,
  "humidity_pct": 62.1,
  "temperature_c": 21.3,
  "battery_v": 3.98,
  "vanne_1": "open",
  "vanne_2": "closed",
  "vanne_3": "closed"
}
```

Notes d'implementation :
- `ts` : ISO8601 UTC, construit apres synchronisation NTP (voir
  `components/net/time_sync.cpp`). Peut refleter l'epoque par defaut
  (1970-01-01) si publie avant la premiere synchronisation reussie.
- `water_level_cm` : calcule a partir de la distance mesuree par le capteur
  ultrason (`water_level_cm = hauteur_cuve_cm - distance_mesuree_cm`),
  clampe a `[0, hauteur_cuve_cm]`. La hauteur de cuve est configurable via
  `idf.py menuconfig` -> `CONFIG_ARROSAGE_TANK_HEIGHT_CM`, a **calibrer
  selon le montage physique reel** de chaque installation.
- `humidity_pct` / `temperature_c` : capteurs materiels **pas encore
  definitivement choisis** par l'equipe firmware au moment de cette
  reecriture. Implementation par defaut : capteur capacitif ADC (sol) +
  DS18B20 1-Wire (temperature), isoles derriere l'interface `Sensor`
  (`components/sensors/include/sensor.h`) pour rester faciles a remplacer.
  Le capteur capacitif necessite une calibration dry/wet sur site (voir
  `components/sensors/priv_include/soil_humidity_sensor.h`, constantes
  `kRawDry`/`kRawWet`, ajustables via le mode diagnostic).
- `battery_v` : le device est **alimente secteur**, il n'y a pas de vraie
  batterie. La cle reste presente (contrat oblige) mais republie une
  constante `0.0` (voir `BatteryPlaceholderSensor`).
- **Ambiguite connue sur la valeur par defaut avant la premiere lecture** :
  tant qu'un capteur n'a jamais ete lu avec succes (capteur debranche/en
  panne des le boot), `sensor_manager` republie `0.0` - une valeur qui, pour
  `water_level_cm`, est indiscernable d'une cuve reellement vide. Le contrat
  etant fige (une cle par mesure, pas de champ de statut additionnel), cette
  ambiguite ne peut pas etre levee cote firmware sans le faire evoluer ; a
  garder en tete cote backend/dashboard si une logique de securite se base
  sur `water_level_cm == 0`.

## Topic commande (serveur -> device) — proposition, a valider avec le backend

```
arrosage/<device_id>/commande
```

Le device y souscrit en QoS 1 des la connexion MQTT etablie.

**Exigence dure pour le backend : ne jamais publier sur ce topic avec
`retain=true`.** Une commande `open` retenue serait rejouee automatiquement
a chaque reconnexion/redemarrage du device (ex. apres une coupure secteur),
ce qui ouvrirait une vanne de facon non voulue.

Format propose (voir `components/command/command_parser.cpp` pour les
regles de validation exactes) :

```json
{"vanne": "vanne_1", "action": "open", "duration_s": 600}
{"vanne": "vanne_2", "action": "close"}
{"action": "stop_all"}
```

- `vanne` : cle du contrat etat (`vanne_1`, `vanne_2`, `vanne_3`, ...) —
  **pas** le nom humain de la vanne (ex "Pelouse").
- `action` : `"open"` | `"close"` | `"stop_all"`.
- `duration_s` : obligatoire et strictement positif pour `"open"` — **pas de
  duree implicite par defaut** : un payload `open` sans `duration_s` valide
  est rejete plutot que de deviner une duree (un arrosage est
  securite-critique). La duree est de toute facon re-clampee cote firmware
  a la duree max configuree pour la vanne (voir
  `components/valve/valve_config.cpp`).
- Tout payload malforme, avec un champ manquant ou d'un type incorrect est
  ignore (loggue en warning), sans jamais faire planter le firmware.

Ce format n'est pas encore un contrat fige : **a valider avec l'equipe
backend avant mise en production**, notamment si le backend a deja une
convention differente pour un eventuel arret d'urgence ou pour cibler
plusieurs vannes en un seul message.

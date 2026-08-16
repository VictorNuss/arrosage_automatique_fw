# Contrat MQTT

Le broker est un Mosquitto local, sans TLS, acces anonyme.

## Topic etat (device -> serveur) — VERSION 2, evenementielle

> Remplace l'ancienne version (JSON plat unique, snapshot periodique
> complet toutes les `CONFIG_ARROSAGE_STATE_PUBLISH_INTERVAL_S`) suite a un
> retour de terrain : le snapshot periodique obligeait a republier une
> **valeur bidon** (0.0 ou derniere valeur connue) des qu'un capteur n'avait
> rien de frais a rapporter, ce qui rendait le flux MQTT peu fiable. **A
> valider avec l'equipe backend/dashboard avant mise en prod** : ce
> changement casse la version precedente du contrat (topic et format de
> payload differents).

```
arrosage/<device_id>/etat/<key>
```

Un topic par cle du contrat (`water_level_cm`, `humidity_pct`,
`temperature_c`, `battery_v`, `vanne_1`, `vanne_2`, `vanne_3`, ...), publie
**uniquement quand une donnee reelle et fraiche existe** :
- Capteur : a chaque lecture materielle reussie (voir
  `sensor_manager_collect()`) - jamais en cas d'echec de lecture, jamais de
  valeur par defaut avant la premiere lecture reussie.
- Vanne : a chaque changement d'etat reel (commande `open`/`close` executee
  avec succes, fermeture automatique en fin de duree) - voir
  `components/valve/valve_manager.cpp`.

QoS 1, **retain=true** sur chaque sous-topic (un abonnement a
`arrosage/<device_id>/etat/#` recoit donc immediatement la derniere valeur
connue de chaque cle a la reconnexion, meme sans nouvel evenement).

Payload capteur :
```json
{"value": 34.5}
```
Payload vanne :
```json
{"state": "open"}
```

Notes d'implementation :
- Pas de champ `ts` dans le payload : l'horodatage MQTT natif du message (ou
  a defaut l'heure de reception cote backend) fait foi. A ajouter si le
  backend a besoin d'un horodatage embarque explicite (a valider avec eux).
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
  batterie. Republie une constante `0.0` (voir `BatteryPlaceholderSensor`)
  des le premier cycle, comme n'importe quel autre capteur "reussi".
- **L'ambiguite de l'ancienne version (0.0 = jamais lu OU cuve vide) est
  levee** : tant qu'un capteur n'a jamais ete lu avec succes, sa cle n'est
  simplement jamais publiee (ni au demarrage, ni via `get_status`, voir plus
  bas) plutot que de contenir une valeur bidon.
- La page de test locale (`GET /api/state`, voir `state_json.cpp`) garde
  l'ancien format en JSON plat complet (toutes les cles presentes, 0.0 par
  defaut) - pratique pour un affichage immediat dans un navigateur, mais ce
  n'est **plus** le format publie sur MQTT.

### Demander un etat complet : commande `get_status`

Comme le flux `etat` est purement evenementiel (aucune republication
periodique), un backend qui redemarre (ou perd sa base d'etat) peut forcer
une republication immediate de tout ce que le device connait en envoyant
`{"action": "get_status"}` sur le topic `commande` (voir plus bas). Le
device republie alors :
- L'etat courant de **toutes** les vannes (toujours connu, jamais omis).
- La derniere valeur connue de **chaque capteur deja lu avec succes au
  moins une fois** - un capteur jamais lu avec succes depuis le boot reste
  absent de la reponse (meme logique anti-valeur-bidon que ci-dessus).

Le retain=true sur chaque sous-topic couvre deja la plupart des cas de
reconnexion (le broker rejoue automatiquement la derniere valeur connue par
cle) ; `get_status` reste utile si le backend a perdu son propre etat
independamment du broker (ex. sa base de donnees a ete reinitialisee).

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
{"action": "get_status"}
```

- `vanne` : cle du contrat etat (`vanne_1`, `vanne_2`, `vanne_3`, ...) —
  **pas** le nom humain de la vanne (ex "Pelouse").
- `action` : `"open"` | `"close"` | `"stop_all"` | `"get_status"` (voir
  "Demander un etat complet" plus haut).
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

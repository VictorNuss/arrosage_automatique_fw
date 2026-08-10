# arrosage_fw — Firmware ESP32 du contrôleur d'arrosage connecté

Firmware ESP-IDF natif (FreeRTOS explicite : tasks, queues, mutex, software
timers) pour un contrôleur d'arrosage ESP32 : lit des capteurs (niveau d'eau,
humidité du sol, température, tension "batterie") et pilote des vannes par
relais, en respectant un contrat MQTT déjà en production côté backend
(Mosquitto + TimescaleDB + dashboard, hors de ce repo).

Deux documents complètent ce README :
- [`docs/mqtt_contract.md`](docs/mqtt_contract.md) — le contrat MQTT exact
  (topics, payloads, QoS/retain) et ce qui reste à valider avec le backend.
- [`docs/bring_up_checklist.md`](docs/bring_up_checklist.md) — la procédure
  de validation progressive sur le matériel réel (bring-up).

## 1. Prérequis

- **ESP-IDF v6.0.2** (ou compatible ≥5.3, requis par le composant MQTT
  managé). Sur cette machine il est déjà installé dans
  `C:\esp\v6.0.2\esp-idf` (installé via l'Espressif IDE Manager / EIM).
  Si ce n'est pas encore fait sur une autre machine :
  ```
  cd C:\esp\v6.0.2\esp-idf
  ./install.ps1 esp32        # PowerShell — installe le venv Python + toolchain xtensa
  ```
  (`install.bat` sous cmd.exe, `install.sh` sous un vrai shell POSIX/Linux/WSL
  — ne fonctionne **pas** sous Git Bash/MSYS sur Windows, utiliser
  PowerShell ou cmd.exe pour cette étape).
- Un **accès reseau** est necessaire au premier `idf.py build` : deux
  dependances (`espressif/mqtt`, `espressif/cjson`) sont recuperees depuis le
  [registre de composants ESP-IDF](https://components.espressif.com) et
  mises en cache localement dans `managed_components/` (ignore par git).
- Extension VSCode recommandee : **Espressif IDF** (`espressif.esp-idf-extension`,
  voir `.vscode/extensions.json`). Ce projet n'utilise plus PlatformIO.

## 2. Activer l'environnement ESP-IDF

Chaque nouvelle fenêtre de terminal doit sourcer l'environnement avant
d'utiliser `idf.py` :

```powershell
# PowerShell
& "C:\esp\v6.0.2\esp-idf\export.ps1"
```
```
:: cmd.exe
C:\esp\v6.0.2\esp-idf\export.bat
```

## 3. Configurer le device

```
idf.py menuconfig
```
Section **"Arrosage Firmware Configuration"** (voir
[`main/Kconfig.projbuild`](main/Kconfig.projbuild)) :

| Option | Rôle | Défaut |
|---|---|---|
| `CONFIG_ARROSAGE_DEVICE_ID` | Identifiant fixe du device, utilisé dans les topics MQTT (`arrosage/<device_id>/...`) et comme `client_id` MQTT | `arrosage-01` |
| `CONFIG_ARROSAGE_WIFI_SSID` / `..._WIFI_PASSWORD` | Identifiants WiFi | `myssid` / `mypassword` |
| `CONFIG_ARROSAGE_MQTT_BROKER_URI` | URI du broker Mosquitto local (pas de TLS) | `mqtt://192.168.1.10:1883` |
| `CONFIG_ARROSAGE_NTP_SERVER` | Serveur NTP pour l'horodatage `ts` | `pool.ntp.org` |
| `CONFIG_ARROSAGE_STATE_PUBLISH_INTERVAL_S` | Intervalle de publication de l'état | `60` |
| `CONFIG_ARROSAGE_TANK_HEIGHT_CM` | Hauteur de la cuve (calibration du capteur ultrason) | `100` |
| `CONFIG_ARROSAGE_ENABLE_DIAG_CONSOLE` | Active la console de diagnostic REPL au lieu du mode normal | `n` |
| *(sous-menu "Broches (GPIO / ADC)")* `ARROSAGE_VALVE1/2/3_GPIO` | GPIO des relais vannes 1/2/3 | `25` / `26` / `27` |
| `ARROSAGE_WATER_LEVEL_TRIG_GPIO` / `..._ECHO_GPIO` | GPIO trig/echo du capteur ultrason | `32` / `33` |
| `ARROSAGE_TEMPERATURE_GPIO` | GPIO de la sonde DS18B20 (1-Wire) | `4` |
| `ARROSAGE_SOIL_HUMIDITY_ADC_CHANNEL` | Canal ADC1 du capteur d'humidité du sol (pas le numéro de GPIO) | `6` (= GPIO34) |

Ces valeurs sont écrites dans `sdkconfig` (généré, ignoré par git — seul
`sdkconfig.defaults` est versionné).

**Aucun secret WiFi/MQTT n'est en dur dans le code source** : tout passe par
`menuconfig`/`sdkconfig`.

## 4. Compiler, flasher, surveiller

```
idf.py build
idf.py -p COM3 flash monitor
```
(`COM3` = port série de la carte upesy_wroom/esp32dev, à adapter).

`Ctrl+]` quitte le monitor série.

## 5. Structure du projet

```
main/                  point d'entree (app_main), Kconfig.projbuild
components/
  valve/                pilotage GPIO + esp_timer d'auto-fermeture des vannes
  valve_logic/           logique pure (clamp de duree, recherche par cle) - sans materiel, testable sur l'hote
  sensors/               interface Sensor + drivers (ultrason, ADC sol, DS18B20, placeholder batterie)
  command/               parsing JSON du topic commande (pur, testable sur l'hote)
  net/                    WiFi, SNTP, client MQTT
  app_tasks/              command_task, sensor_task, state_json (JSON d'etat partage), console de diagnostic
  web/                    serveur HTTP local de test (page + API JSON, meme format que le contrat MQTT)
  ota/                    ecriture de la partition OTA inactive + rollback de securite (voir §7)
test/
  main/                  tests Unity (target idf.py "linux", sans materiel)
  hardware/               script Python de validation contre un device reel (voir §6)
docs/                  contrat MQTT detaille + checklist de bring-up
Doxyfile               config Doxygen (voir §9) - genere docs/doxygen/html/
generate_doxygen.sh    raccourci pour lancer `doxygen Doxyfile`
```

### Ajouter une vanne (jusqu'à 5 à terme)

Une seule ligne à ajouter dans
[`components/valve/valve_config.cpp`](components/valve/valve_config.cpp) :

```cpp
{"Verger", "vanne_4", GPIO_NUM_14, 1200},
```
(nom humain, clé du contrat MQTT, GPIO du relais, durée max d'ouverture en
secondes). Aucun autre fichier à modifier — `VALVE_COUNT`, la boucle
d'initialisation, le JSON d'état et les commandes suivent automatiquement.
Pour rendre le GPIO de cette 4e/5e vanne configurable via `menuconfig` comme
les trois premières, ajouter une entrée `ARROSAGE_VALVE4_GPIO` dans
[`main/Kconfig.projbuild`](main/Kconfig.projbuild) et l'utiliser ici à la
place du `GPIO_NUM_14` en dur.

### Remplacer un capteur

Chaque capteur implémente l'interface `Sensor`
([`components/sensors/include/sensor.h`](components/sensors/include/sensor.h)) :
`init()`, `read(float*)`, `key()`. Pour remplacer un driver (ex. le capteur
d'humidité du sol pas encore définitivement choisi), il suffit d'écrire une
nouvelle classe qui implémente cette interface et de la substituer dans
[`components/sensors/sensor_manager.cpp`](components/sensors/sensor_manager.cpp) —
rien d'autre à toucher (le reste du firmware ne connaît que l'interface).

### GPIO utilisés (configurables via `idf.py menuconfig` → *Broches (GPIO / ADC)*)

| Fonction | GPIO par défaut | Option Kconfig |
|---|---|---|
| Vanne 1 / 2 / 3 (relais) | 25 / 26 / 27 | `ARROSAGE_VALVE1/2/3_GPIO` |
| Ultrason JSN-SR04M-2 (trig / echo) | 32 / 33 | `ARROSAGE_WATER_LEVEL_TRIG/ECHO_GPIO` |
| DS18B20 (1-Wire, température) | 4 | `ARROSAGE_TEMPERATURE_GPIO` |
| Capacitif sol (ADC1, humidité) | 34 (canal `6`) | `ARROSAGE_SOIL_HUMIDITY_ADC_CHANNEL` |

Adapter le câblage réel ne nécessite donc plus de toucher au code — un
simple `idf.py menuconfig` suffit (sauf pour une 4e/5e vanne au-delà des
trois configurables par défaut, voir ci-dessus).

## 6. Serveur web de test

Activé par défaut (`CONFIG_ARROSAGE_ENABLE_WEB_SERVER=y`), un petit serveur
HTTP (port 80, sans authentification — réseau local de confiance uniquement,
même modèle que le broker Mosquitto anonyme) tourne en permanence à côté du
reste du firmware :

- `http://<ip_du_device>/` — page de test : état courant des capteurs/vannes
  (rafraîchi toutes les 3s), boutons ouvrir/fermer par vanne, arrêt d'urgence.
- `GET /api/state` — le même JSON que le contrat MQTT état.
- `POST /api/command` — accepte exactement le même JSON que le contrat MQTT
  commande (`docs/mqtt_contract.md`) ; la commande passe par le même parseur
  et la même queue que celles reçues via MQTT.

L'IP du device apparaît dans les logs série (`idf.py monitor`) après
connexion WiFi, ou dans la liste des clients de la box/routeur.

## 7. Mise à jour du firmware par WiFi (OTA)

**Activée par défaut** (`CONFIG_ARROSAGE_ENABLE_OTA=y`, dépend de
`CONFIG_ARROSAGE_ENABLE_WEB_SERVER=y`), même modèle de confiance que le
reste du serveur web ("réseau local de confiance uniquement"). À la
différence des autres fonctions du serveur (lecture d'état, pilotage des
vannes) toutefois, **une mise à jour de firmware acceptée sans
authentification équivaut à une prise de contrôle totale du device par
quiconque sur le réseau local** — désactiver `CONFIG_ARROSAGE_ENABLE_OTA`
via `idf.py menuconfig` si le réseau n'est pas de confiance.

### ⚠️ Migration de table de partitions requise (une seule fois)

L'OTA nécessite une table de partitions à plusieurs slots
(`CONFIG_PARTITION_TABLE_TWO_OTA`, déjà dans `sdkconfig.defaults`),
**incompatible avec un device déjà flashé avec l'ancienne table
"single app"**. Sur un device existant, un reflash complet est nécessaire
une seule fois avant de pouvoir utiliser l'OTA :

```
idf.py -p COM3 erase-flash
idf.py -p COM3 build flash
```
(un simple `flash` sans `erase-flash` préalable peut échouer ou laisser des
données de partition incohérentes si l'ancienne table est encore présente).

### Utilisation

Une fois l'OTA activée et ce reflash initial fait :

- Depuis la page de test (`http://<ip_du_device>/`) : section "Mise à jour
  firmware (OTA)", sélectionner `build/arrosage_fw.bin` et envoyer.
- En ligne de commande :
  ```
  curl -X POST --data-binary @build/arrosage_fw.bin http://<ip_du_device>/api/ota
  ```

Le device valide l'image reçue, bascule dessus, puis redémarre
automatiquement. Un **rollback de sécurité** est en place
(`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`) : `main/app_main.cpp` confirme le
bon démarrage de toute image (OTA ou flashée par USB) via
`ota_confirm_boot_ok()` une fois l'initialisation terminée ; si une image
fraîchement flashée redémarre de façon inattendue avant cette confirmation
(crash, coupure), le bootloader revient automatiquement sur l'image
précédente au prochain démarrage plutôt que de rester bloqué sur une image
défectueuse.

Voir [`components/ota/`](components/ota/) pour l'implémentation
(wrapper autour de `esp_ota_ops`, indépendant du transport HTTP).

## 8. Tests

### Logique pure (sans matériel)

La cible `linux` d'ESP-IDF compile les tests pour l'hôte (pas pour la carte
ESP32) : il faut donc un **compilateur natif** (gcc/g++), absent de Windows
par défaut. Testé et fonctionnel sous **WSL** (Debian et Ubuntu) :

```bash
# Dans un terminal WSL (distro avec gcc/g++/cmake/ninja-build installés) :
bash test/run_wsl_tests.sh
```

Ou directement depuis Windows (PowerShell/Git Bash) :
```
MSYS_NO_PATHCONV=1 wsl -d <distro> -- bash /mnt/c/.../arrosage_fw/test/run_wsl_tests.sh
```
(`MSYS_NO_PATHCONV=1` est nécessaire sous Git Bash pour éviter que MSYS ne
corrompe le chemin `/mnt/c/...`.)

Le script [`test/run_wsl_tests.sh`](test/run_wsl_tests.sh) réutilise le
checkout ESP-IDF déjà présent côté Windows (`IDF_PATH`, à adapter en variable
d'environnement si besoin) et n'installe que l'environnement Python d'ESP-IDF
côté WSL (`python tools/idf_tools.py install-python-env` — pas besoin des
toolchains croisés xtensa/riscv, uniquement utiles pour flasher un vrai
ESP32) ; il faut par contre `gcc`, `g++`, `cmake` et `ninja-build` installés
nativement dans la distro WSL utilisée (`sudo apt install build-essential
cmake ninja-build`).

Sans WSL, sous Windows natif, il faudrait un compilateur MinGW-w64 ou MSVC
(non testé) :
- MSYS2/MinGW-w64 : `pacman -S mingw-w64-x86_64-gcc`, puis s'assurer que
  `gcc`/`g++` sont dans le `PATH` du terminal utilisé.
- Ou Visual Studio Build Tools (composant "Desktop development with C++").

**33/33 tests passent** : `command_parser` (JSON valides/invalides/malformés,
troncature de clé, types incorrects) et `valve_logic` (clamp de sécurité,
recherche de vanne par clé) — voir [`test/main/`](test/main/).

### Contre un device réel (script Python)

Une fois le firmware flashé et connecté au WiFi (serveur web de test actif,
§6), depuis n'importe quel poste sur le même réseau :

```
python test/hardware/validate_device.py <ip_du_device>
```

Aucune dépendance externe (stdlib Python 3 uniquement). Valide
automatiquement : toutes les clés attendues dans `/api/state`, ouverture et
fermeture d'une vanne, arrêt d'urgence (`stop_all`), et rejet en HTTP 400 des
payloads de commande invalides. Complète — sans le remplacer — le bring-up
manuel ci-dessous (ce script ne vérifie pas la réalité physique des mesures
ni des relais, seulement le comportement logiciel du firmware).

### Sur le matériel réel

Voir [`docs/bring_up_checklist.md`](docs/bring_up_checklist.md) : ordre de
validation progressif (GPIO seul → capteurs → WiFi → MQTT → intégration),
console de diagnostic embarquée (`valve open/close`, `sensor`), et commandes
`mosquitto_pub`/`mosquitto_sub` prêtes à copier-coller.

## 9. Contrat MQTT — résumé (détails dans `docs/mqtt_contract.md`)

- `arrosage/<device_id>/etat` (device → serveur, ~60s, QoS1, retain) :
  **contrat figé**, ne pas modifier les clés du JSON.
- `arrosage/<device_id>/commande` (serveur → device) : format **proposé**,
  pas encore validé par l'équipe backend — voir le document dédié avant
  toute mise en production.

## 10. Documentation Doxygen (référence API)

Chaque header public (`components/*/include/*.h`) est annoté en Doxygen
(`@brief`/`@param`/`@return`), organisé par module (vannes, capteurs,
commandes, réseau, tâches, serveur web). Génération locale :

```
doxygen Doxyfile
```
(ou `bash generate_doxygen.sh`, qui fonctionne aussi depuis WSL si Doxygen
n'est pas installé nativement sous Windows : `sudo apt install doxygen
graphviz`). Résultat dans `docs/doxygen/html/index.html` (non versionné,
généré localement — voir `.gitignore`), avec le README comme page d'accueil,
un module par composant, et des graphes de classes (Graphviz) pour les
capteurs.

## 11. Limitations connues / à faire

- `humidity_pct` (capacitif ADC) et `temperature_c` (DS18B20) : choix
  matériel par défaut, pas encore définitivement arrêté — calibration
  dry/wet du capteur sol à faire sur site (constantes `kRawDry`/`kRawWet`
  dans `components/sensors/priv_include/soil_humidity_sensor.h`).
- `battery_v` : toujours `0.0` (device alimenté secteur, pas de batterie
  réelle) — la clé reste présente pour respecter le contrat.
- Pas de TLS (broker local anonyme, contrainte de départ). L'OTA (§7) est
  activée par défaut mais sans authentification, comme le reste du serveur
  web — à désactiver explicitement si le réseau local n'est pas de confiance.
- `components/ota/` n'est couvert par aucun test Unity host (comme le reste
  des modules liés au matériel/flash — voir §8) : validation uniquement via
  bring-up manuel sur le device réel.
- Format du topic commande à faire valider par l'équipe backend.

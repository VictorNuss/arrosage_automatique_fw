cmake_minimum_required(VERSION 3.16)

# Rend visibles les composants du firmware (command, valve_logic...) sans
# construire tout le projet : seuls ceux requis par test/main/CMakeLists.txt
# sont effectivement compiles (pas de dependance driver/esp_timer/wifi ici).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")

# Restreint explicitement l'ensemble des composants construits. Sans cela,
# ESP-IDF tente par defaut de resoudre TOUS les composants connus (y compris
# ceux d'esp-idf/components/) meme pour la cible hote "linux" - un bug/etat
# incomplet de la cible preview "linux" en v6.0.2 fait alors echouer la
# resolution de composants materiels sans rapport avec ce projet de test
# (ex: esp_driver_ana_cmpr requiert esp_hal_ana_cmpr, qui n'est pas
# enregistre pour la cible linux). Lister ici uniquement ce qui est reellement
# necessaire evite ce probleme.
set(COMPONENTS main unity command valve_logic cjson)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_arrosage_fw)

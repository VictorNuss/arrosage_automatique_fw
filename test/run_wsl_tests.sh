#!/bin/bash
# Build + execute les tests Unity host (command_parser, valve_logic) dans
# WSL, en reutilisant le checkout ESP-IDF deja present cote Windows.
#
# Usage (depuis un terminal WSL, distro avec gcc/g++/cmake/ninja installes -
# voir README.md section Tests) :
#   bash run_wsl_tests.sh
#
# Ou directement depuis Windows (PowerShell/Git Bash) :
#   MSYS_NO_PATHCONV=1 wsl -d <distro> -- bash /mnt/c/.../test/run_wsl_tests.sh
# (MSYS_NO_PATHCONV=1 est necessaire sous Git Bash pour eviter que le chemin
# /mnt/c/... soit corrompu par la conversion de chemin automatique de MSYS)
set -e

# A adapter si l'emplacement d'ESP-IDF ou la version de l'environnement
# Python differe sur ta machine (voir `ls ~/.espressif/python_env/`).
export IDF_PATH="${IDF_PATH:-/mnt/c/esp/v6.0.2/esp-idf}"
export IDF_PYTHON_ENV_PATH="${IDF_PYTHON_ENV_PATH:-$HOME/.espressif/python_env/idf6.0_py3.13_env}"
export ESP_IDF_VERSION="${ESP_IDF_VERSION:-6.0.2}"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== set-target linux ==="
python "$IDF_PATH/tools/idf.py" --preview set-target linux

echo "=== build ==="
python "$IDF_PATH/tools/idf.py" build

echo "=== running unity tests ==="
./build/test_arrosage_fw.elf

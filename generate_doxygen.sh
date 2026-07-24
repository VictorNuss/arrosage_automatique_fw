#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
doxygen Doxyfile
echo "=== generated ==="
find docs/doxygen/html -maxdepth 1 -type f | sort | head -20

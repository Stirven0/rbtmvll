#!/usr/bin/env bash
# =============================================================================
# build_firmware.sh - Volver al firmware normal y compilar
# =============================================================================
set -eo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROJECT_DIR}"

# Verificar que el entorno ESP-IDF esté cargado
if ! command -v idf.py &>/dev/null; then
    echo "ERROR: idf.py no está en el PATH."
    echo "       Primero active el entorno ESP-IDF:"
    echo "       source \$HOME/.espressif/tools/activate_idf_v6.1.sh"
    exit 1
fi

# Cambiar app_main_test.c -> app_main.c en CMakeLists.txt
echo "[FIRMWARE] Restaurando app_main.c..."
sed -i 's/^    "app_main_test\.c"$/    #"app_main_test.c"/' main/CMakeLists.txt
sed -i 's/^    #"app_main\.c"$/    "app_main.c"/' main/CMakeLists.txt

echo "[FIRMWARE] Limpiando y compilando..."
idf.py fullclean build

echo ""
echo "============================================================="
echo "  FIRMWARE NORMAL LISTO PARA FLASHEAR"
echo "============================================================="
echo ""
echo "  idf.py flash        # flashear (te pedirá el puerto serial)"
echo "  idf.py monitor      # ver salida por serial"

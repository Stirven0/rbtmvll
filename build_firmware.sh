#!/usr/bin/env bash
# =============================================================================
# build_firmware.sh - Volver al firmware normal y compilar
# =============================================================================
set -eo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROJECT_DIR}"

# Activar entorno ESP-IDF si no está cargado en este subshell
if ! command -v idf.py &>/dev/null; then
    for idf_env in \
        "$HOME/.espressif/tools/activate_idf_v6.1.sh" \
        "$HOME/esp/esp-idf/export.sh" \
        "$HOME/esp-idf/export.sh"; do
        if [[ -f "$idf_env" ]]; then
            echo "[ENV] Activando entorno ESP-IDF: $idf_env"
            exec bash -c "source '$idf_env' && '$0' '$@'"
        fi
    done
    echo "ERROR: idf.py no está en el PATH y no se encontró el entorno ESP-IDF."
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

#!/usr/bin/env bash
# =============================================================================
# build_test.sh - Cambiar al menú de pruebas y compilar
# =============================================================================
set -eo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROJECT_DIR}"

# Activar entorno ESP-IDF si no está cargado
if ! command -v idf.py &>/dev/null; then
    for idf_env in \
        "$HOME/.espressif/tools/activate_idf_v6.1.sh" \
        "$HOME/esp/esp-idf/export.sh" \
        "$HOME/esp-idf/export.sh"; do
        if [[ -f "$idf_env" ]]; then
            echo "[ENV] Activando entorno ESP-IDF: $idf_env"
            source "$idf_env"
            break
        fi
    done
fi

if ! command -v idf.py &>/dev/null; then
    echo "ERROR: idf.py no está en el PATH."
    echo "       Active el entorno ESP-IDF primero:"
    echo "       source \$HOME/.espressif/tools/activate_idf_v6.1.sh"
    exit 1
fi

# Cambiar app_main.c -> app_main_test.c en CMakeLists.txt
echo "[TEST] Configurando menú de pruebas..."
sed -i 's/^    "app_main\.c"$/    #"app_main.c"/' main/CMakeLists.txt
sed -i 's/^    #"app_main_test\.c"$/    "app_main_test.c"/' main/CMakeLists.txt

echo "[TEST] Limpiando y compilando..."
idf.py fullclean build

echo ""
echo "============================================================="
echo "  LISTO PARA FLASHEAR EL MENÚ DE PRUEBAS"
echo "============================================================="
echo ""
echo "  idf.py flash        # flashear (te pedirá el puerto serial)"
echo "  idf.py monitor      # ver resultados por serial (115200 baudios)"
echo ""
echo "Para volver al firmware normal: ./build_firmware.sh"

#!/usr/bin/env bash
# =============================================================================
# build.sh - Script de construcción para IndoorWatch (ESP-IDF)
# =============================================================================
set -eo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${PROJECT_DIR}"

# Activar entorno ESP-IDF si no está cargado
if ! command -v idf.py &>/dev/null; then
    for idf_env in \
        "$HOME/.espressif/tools/activate_idf_v6.1.sh" \
        "$HOME/esp/esp-idf/export.sh" \
        "$HOME/esp-idf/export.sh" \
        "/opt/esp-idf/export.sh"; do
        if [[ -f "$idf_env" ]]; then
            echo "[ENV] Activando entorno ESP-IDF: $idf_env"
            # shellcheck source=/dev/null
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

ACTION="${1:-build}"

case "${ACTION}" in
    build)
        echo "[BUILD] Construyendo proyecto IndoorWatch (ESP32 / ESP-WROOM-32)..."
        idf.py build
        ;;
    fullclean)
        echo "[FULLCLEAN] Limpiando todo y reconstruyendo..."
        idf.py fullclean
        idf.py build
        ;;
    flash)
        echo "[FLASH] Flasheando al dispositivo..."
        read -rp "Puerto serial (ej. /dev/ttyUSB0): " PORT
        idf.py -p "${PORT}" flash
        ;;
    monitor)
        echo "[MONITOR] Abriendo monitor serie (Ctrl-] para salir)..."
        read -rp "Puerto serial (ej. /dev/ttyUSB0): " PORT
        idf.py -p "${PORT}" monitor
        ;;
    flash-monitor)
        echo "[FLASH+MONITOR] Flasheando y abriendo monitor..."
        read -rp "Puerto serial (ej. /dev/ttyUSB0): " PORT
        idf.py -p "${PORT}" flash monitor
        ;;
    menuconfig)
        echo "[MENUCONFIG] Abriendo configuración de ESP-IDF..."
        idf.py menuconfig
        ;;
    size)
        echo "[SIZE] Analizando tamaño de binario..."
        idf.py size
        ;;
    clean)
        echo "[CLEAN] Limpiando archivos de compilación..."
        idf.py clean
        ;;
    *)
        echo "Uso: $0 {build|fullclean|flash|monitor|flash-monitor|menuconfig|size|clean}"
        exit 1
        ;;
esac

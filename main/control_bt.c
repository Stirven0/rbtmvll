#include "control_bt.h"
#include "config.h"
#include "motores.h"
#include "esp_log.h"

/* ------------------------------------------------------------------------
 *  En ESP-IDF v6.1, el stack Bluetooth clásico (Bluedroid) tiene una API
 *  reorganizada respecto a versiones previas. Las funciones
 *  esp_bt_controller_init/esp_bluedroid_init/etc. requieren incluir los
 *  headers correspondientes del componente 'bt' y vincular contra
 *  libbt.a. Como la integracion completa del mando GameSir (HID/SPP)
 *  requiere trabajo adicional de todos modos, este modulo se deja como
 *  stub funcional que inicializa los logs y deja lista la interfaz.
 * ------------------------------------------------------------------------ */

static const char *TAG = "CONTROL_BT";

static bool habilitado = false;
static bool mando_conectado = false;
static char nombre_mando[32] = { 0 };

static bool solicitud_datos = false;
static bool solicitud_auto = false;
static bool debug_mando = false;

/* ------------------------------------------------------------------------
 *  NOTA: Bluepad32 es una libreria de Arduino-ESP32 de terceros que no
 *  tiene equivalente directo en ESP-IDF nativo. El manejo de gamepads
 *  Bluetooth clásico (HID/SPP) requiere usar el stack BT clásico de
 *  ESP-IDF (esp_bt_controller_enable, esp_gap_bt_*, etc.) o portar
 *  Bluepad32 como componente idf_component.
 *
 *  A continuación se inicializa el controlador BT clásico de ESP-IDF
 *  y se deja una interfaz lista para integrar el stack HID/SPP cuando
 *  se disponga de la implementación del mando.
 * ------------------------------------------------------------------------ */

void control_bt_init(void)
{
    habilitado = true;
    solicitud_datos = false;
    solicitud_auto = false;

    ESP_LOGW(TAG, "Control BT (mando GameSir): stub inicializado.");
    ESP_LOGW(TAG, "En ESP-IDF v6.1 el stack BT clasico requiere integracion adicional.");
    ESP_LOGI(TAG, "Bluetooth serial por UART2 (HC-05) funciona independientemente.");
}

void control_bt_tick(void)
{
    if (!habilitado) {
        return;
    }

    /* Placeholder: en una version completa aqui se llamaria al equivalente
     * de BP32.update() y se procesarian los eventos del gamepad. */
}

void control_bt_activar(void)
{
    if (habilitado) return;
    habilitado = true;
    ESP_LOGI(TAG, "Bluetooth activado.");
}

void control_bt_desactivar(void)
{
    if (!habilitado) return;
    habilitado = false;
    mando_conectado = false;
    nombre_mando[0] = '\0';
    motores_detener();
    ESP_LOGI(TAG, "Bluetooth desactivado. Radio libre para Wi-Fi.");
}

bool control_bt_habilitado(void) { return habilitado; }

void control_bt_imprimir_estado(void)
{
    if (!habilitado) {
        ESP_LOGI(TAG, "Bluetooth desactivado por el modo automatico.");
        return;
    }
    ESP_LOGI(TAG, "Mando: %s | conectado=%d",
             nombre_mando[0] ? nombre_mando : "desconectado", mando_conectado);
}

void control_bt_olvidar_claves(void)
{
    ESP_LOGW(TAG, "Olvidar claves: requiere implementacion de esp_bt_gap_disable/"
                  "esp_bt_gap_bond_dev_delete en el stack BT de ESP-IDF.");
}

void control_bt_reiniciar_busqueda(void)
{
    ESP_LOGW(TAG, "Reiniciar busqueda: requiere implementacion de esp_bt_gap_* "
                  "en el stack BT de ESP-IDF.");
}

void control_bt_alternar_debug(void)
{
    debug_mando = !debug_mando;
    ESP_LOGI(TAG, "Debug de ejes %s.", debug_mando ? "activado" : "desactivado");
}

bool control_bt_conectado(void)
{
    return habilitado && mando_conectado;
}

const char *control_bt_nombre(void)
{
    if (!habilitado) return "desactivado";
    return nombre_mando[0] > 0 ? nombre_mando : "desconectado";
}

bool control_bt_consumir_solicitud_datos(void)
{
    bool s = solicitud_datos;
    solicitud_datos = false;
    return s;
}

bool control_bt_consumir_solicitud_auto(void)
{
    bool s = solicitud_auto;
    solicitud_auto = false;
    return s;
}

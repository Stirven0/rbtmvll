/* ========================================================================
 *  IndoorWatch - Test: Bluetooth Clásico - Mando GameSir
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Inicializa el stack BT clásico de ESP-IDF y espera conexión de un mando.
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_bluetooth
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_bt.h"

static const char *TAG = "TEST_BT";

static void bt_init_stack(void)
{
    esp_err_t ret;

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
        return;
    }

    /* Liberar memoria BLE (solo usamos BT clásico) */
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo liberando BLE: %s", esp_err_to_name(ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo inicializando controlador BT: %s",
                 esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo habilitando BT clásico: %s",
                 esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo inicializando Bluedroid: %s",
                 esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo habilitando Bluedroid: %s",
                 esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Stack BT clásico inicializado.");
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Bluetooth Clásico - Mando GameSir");
    ESP_LOGI(TAG, " Esperando conexión...");
    ESP_LOGI(TAG, "==============================================");

    bt_init_stack();

    /* Mostrar dirección Bluetooth local */
    const uint8_t *bd_addr = esp_bt_dev_get_address();
    ESP_LOGI(TAG, "Dirección BT local: %02X:%02X:%02X:%02X:%02X:%02X",
             bd_addr[0], bd_addr[1], bd_addr[2],
             bd_addr[3], bd_addr[4], bd_addr[5]);

    ESP_LOGI(TAG, "Encienda el mando GameSir en modo emparejamiento.");
    ESP_LOGI(TAG, "Android: Home + A (amarillo) | Switch: Home + Y (rojo)");

    while (1) {
        ESP_LOGI(TAG, "Esperando mando Bluetooth...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

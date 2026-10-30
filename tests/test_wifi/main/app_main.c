/* ========================================================================
 *  IndoorWatch - Test: Conectividad Wi-Fi
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  IMPORTANTE: Editar secrets.h con tu SSID y contraseña Wi-Fi antes de compilar.
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_wifi
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "TEST_WIFI";

#define WIFI_SSID     "HONOR"
#define WIFI_PASSWORD "12345678"

static esp_netif_t *netif_sta = NULL;

static void evento_wifi(void *arg, esp_event_base_t base,
                        int32_t id_evento, void *datos_evento)
{
    if (base == WIFI_EVENT && id_evento == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi iniciado. Conectando...");
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id_evento == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Desconectado. Reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id_evento == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)datos_evento;
        ESP_LOGI(TAG, "✅ Conectado! IP: " IPSTR, IP2STR(&evt->ip_info.ip));
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Conectividad Wi-Fi");
    ESP_LOGI(TAG, " SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "==============================================");

    /* Inicializar NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &evento_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &evento_wifi, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Esperando conexión Wi-Fi...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

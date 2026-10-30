/* ========================================================================
 *  IndoorWatch - Test: Sensor Flama IR (GPIO 34)
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_flama
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "TEST_FLAMA";

#define PIN_FLAMA 34   /* LM393: LOW = fuego detectado */

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Sensor Flama IR - GPIO 34");
    ESP_LOGI(TAG, " Lectura cada 500 ms");
    ESP_LOGI(TAG, " Acercar una fuente IR al sensor para detectar");
    ESP_LOGI(TAG, "==============================================");

    gpio_set_direction(PIN_FLAMA, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_FLAMA, GPIO_FLOATING);

    while (1) {
        int nivel = gpio_get_level(PIN_FLAMA);
        if (nivel == 0) {
            ESP_LOGW(TAG, "🔥 FLAMA DETECTADA (GPIO%d = LOW)", PIN_FLAMA);
        } else {
            ESP_LOGI(TAG, "✅ Sin flama (GPIO%d = HIGH)", PIN_FLAMA);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

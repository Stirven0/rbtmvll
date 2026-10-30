/* ========================================================================
 *  IndoorWatch - Test: HC-SR04 Ultrasonido
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Pines: TRIG=GPIO 32, ECHO=GPIO 33
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_ultrasonido
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "TEST_SONAR";

#define PIN_TRIG  32
#define PIN_ECHO  33

#define TIMEOUT_US 30000  /* ~5 m */

static float medir_distancia(void)
{
    /* Pulso TRIG de 10 us */
    gpio_set_level(PIN_TRIG, 0);
    esp_rom_delay_us(2);
    gpio_set_level(PIN_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level(PIN_TRIG, 0);

    uint32_t inicio = esp_timer_get_time();

    /* Esperar flanco alto de ECHO */
    while (gpio_get_level(PIN_ECHO) == 0) {
        if ((esp_timer_get_time() - inicio) >= TIMEOUT_US) {
            return -1.0f;  /* sin eco */
        }
    }
    uint32_t t_alto = esp_timer_get_time();

    /* Esperar flanco bajo de ECHO */
    while (gpio_get_level(PIN_ECHO) == 1) {
        if ((esp_timer_get_time() - t_alto) >= TIMEOUT_US) {
            return -1.0f;  /* sin eco */
        }
    }
    uint32_t duracion = esp_timer_get_time() - t_alto;

    /* distancia cm = duracion_us / 58 */
    return (float)duracion / 58.0f;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: HC-SR04 Ultrasonido");
    ESP_LOGI(TAG, " TRIG=GPIO 32 | ECHO=GPIO 33");
    ESP_LOGI(TAG, " Lectura cada 500 ms");
    ESP_LOGI(TAG, "==============================================");

    /* HC-SR04 */
    gpio_set_direction(PIN_TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_ECHO, GPIO_MODE_INPUT);
    gpio_set_level(PIN_TRIG, 0);

    while (1) {
        float dist = medir_distancia();
        if (dist > 0.0f) {
            ESP_LOGI(TAG, "Distancia: %.1f cm", dist);
        } else {
            ESP_LOGW(TAG, "Sin eco");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

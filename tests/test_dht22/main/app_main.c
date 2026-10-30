/* ========================================================================
 *  IndoorWatch - Test: DHT22 (GPIO 13)
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_dht22
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const char *TAG = "TEST_DHT22";

#define PIN_DHT22 13

static bool dht22_leer(float *temperatura, float *humedad)
{
    uint8_t datos[5] = { 0 };

    gpio_set_direction(PIN_DHT22, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_DHT22, 0);
    esp_rom_delay_us(1000);
    gpio_set_level(PIN_DHT22, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(PIN_DHT22, GPIO_MODE_INPUT);

    int espera = 0;
    while (gpio_get_level(PIN_DHT22) == 0) {
        if (espera++ > 100) return false;
        esp_rom_delay_us(1);
    }
    espera = 0;
    while (gpio_get_level(PIN_DHT22) == 1) {
        if (espera++ > 100) return false;
        esp_rom_delay_us(1);
    }

    for (int i = 0; i < 40; i++) {
        espera = 0;
        while (gpio_get_level(PIN_DHT22) == 0) {
            if (espera++ > 100) return false;
            esp_rom_delay_us(1);
        }
        esp_rom_delay_us(30);
        uint8_t bit = gpio_get_level(PIN_DHT22) ? 1 : 0;
        datos[i / 8] <<= 1;
        datos[i / 8] |= bit;
        espera = 0;
        while (gpio_get_level(PIN_DHT22) == 1) {
            if (espera++ > 100) return false;
            esp_rom_delay_us(1);
        }
    }

    uint8_t checksum_calc = (uint8_t)(datos[0] + datos[1] + datos[2] + datos[3]);
    if (datos[4] != checksum_calc) {
        ESP_LOGW(TAG, "Checksum invalido: leído=0x%02X calculado=0x%02X",
                 datos[4], checksum_calc);
        return false;
    }

    *humedad  = (datos[0] * 256 + datos[1]) / 10.0f;
    *temperatura = ((datos[2] & 0x7F) * 256 + datos[3]) / 10.0f;
    if (datos[2] & 0x80) {
        *temperatura = -(*temperatura);
    }
    return true;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: DHT22 - GPIO 13");
    ESP_LOGI(TAG, " Lectura cada 2 segundos");
    ESP_LOGI(TAG, "==============================================");

    gpio_set_direction(PIN_DHT22, GPIO_MODE_INPUT);

    int cuenta = 0;
    while (1) {
        float t = 0.0f, h = 0.0f;
        if (dht22_leer(&t, &h)) {
            bool valida = t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;
            if (valida) {
                ESP_LOGI(TAG, "[%02d] T=%.1f C | H=%.1f %% | DHT=OK",
                         ++cuenta, t, h);
            } else {
                ESP_LOGW(TAG, "[%02d] Lectura fuera de rango: T=%.1f H=%.1f",
                         ++cuenta, t, h);
            }
        } else {
            ESP_LOGE(TAG, "[%02d] Error de lectura DHT22", ++cuenta);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

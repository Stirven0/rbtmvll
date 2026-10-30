/* ========================================================================
 *  IndoorWatch - Test: Sensor Flama IR (GPIO 34, ADC1_CHANNEL_6)
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
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "TEST_FLAMA";

#define PIN_FLAMA 34
#define ADC_CHANNEL_FLAMA ADC_CHANNEL_6   /* GPIO 34 */
#define UMBRAL_FLAMA_ADC 2000             /* Umbral: calibrar según el sensor */

static adc_oneshot_unit_handle_t adc_handle = NULL;

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Sensor Flama IR - GPIO 34 (ADC)");
    ESP_LOGI(TAG, " Lectura analógica cada 500 ms");
    ESP_LOGI(TAG, " Umbral: %d ADC", UMBRAL_FLAMA_ADC);
    ESP_LOGI(TAG, "==============================================");

    /* Configurar ADC oneshot */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_FLAMA, &chan_cfg);

    while (1) {
        int raw = 0;
        adc_oneshot_read(adc_handle, ADC_CHANNEL_FLAMA, &raw);

        if (raw > UMBRAL_FLAMA_ADC) {
            ESP_LOGW(TAG, "🔥 FLAMA DETECTADA: %d ADC (umbral: %d)",
                     raw, UMBRAL_FLAMA_ADC);
        } else {
            ESP_LOGI(TAG, "✅ Sin flama: %d ADC", raw);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

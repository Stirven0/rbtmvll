/* ========================================================================
 *  IndoorWatch - Test: Sensor MQ-135 (GPIO 35, ADC1_CHANNEL_7)
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_mq
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "TEST_MQ";

#define PIN_MQ_ANALOG 35
#define ADC_CHANNEL_MQ ADC_CHANNEL_7   /* GPIO 35 */
#define UMBRAL_GAS_ADC 2700

static adc_oneshot_unit_handle_t adc_handle = NULL;

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Sensor MQ-135 - GPIO 35 (ADC1)");
    ESP_LOGI(TAG, " Lectura cada 1 segundo");
    ESP_LOGI(TAG, " Umbral de alerta: %d ADC", UMBRAL_GAS_ADC);
    ESP_LOGI(TAG, "==============================================");

    /* Configurar ADC oneshot */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_MQ, &chan_cfg);

    while (1) {
        int raw = 0;
        adc_oneshot_read(adc_handle, ADC_CHANNEL_MQ, &raw);

        if (raw > UMBRAL_GAS_ADC) {
            ESP_LOGW(TAG, "Gas: %d ADC  ⚠️  ELEVADO", raw);
        } else {
            ESP_LOGI(TAG, "Gas: %d ADC  ✅  normal", raw);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

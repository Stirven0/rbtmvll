/* ========================================================================
 *  IndoorWatch - Test: HC-SR04 + Servo SG90
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Pines: TRIG=GPIO 32, ECHO=GPIO 33, Servo=GPIO 26
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_ultrasonido
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_rom_sys.h"

static const char *TAG = "TEST_SONAR";

#define PIN_TRIG  32
#define PIN_ECHO  33
#define PIN_SERVO 26

#define ANGULO_MIN  30
#define ANGULO_CENTRO 90
#define ANGULO_MAX  150

#define TIMEOUT_US 30000  /* ~5 m */

static const uint8_t ANGULOS[3] = { ANGULO_MIN, ANGULO_CENTRO, ANGULO_MAX };
static const char *NOMBRES[3] = { "IZQUIERDA", "FRENTE", "DERECHA" };

static uint32_t angulo_a_duty(uint8_t angulo)
{
    if (angulo > 180) angulo = 180;
    uint32_t duty_min = 205;   /* 500 us / 20000 us * 8192 */
    uint32_t duty_max = 983;   /* 2400 us / 20000 us * 8192 */
    return duty_min + ((uint32_t)angulo * (duty_max - duty_min)) / 180;
}

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
    ESP_LOGI(TAG, " Test: HC-SR04 + Servo SG90");
    ESP_LOGI(TAG, " TRIG=GPIO 32 | ECHO=GPIO 33 | Servo=GPIO 26");
    ESP_LOGI(TAG, " Barrido: Izquierda-Frente-Derecha");
    ESP_LOGI(TAG, "==============================================");

    /* HC-SR04 */
    gpio_set_direction(PIN_TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_ECHO, GPIO_MODE_INPUT);
    gpio_set_level(PIN_TRIG, 0);

    /* Servo via LEDC */
    ledc_timer_config_t temporizador = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&temporizador);

    ledc_channel_config_t canal = {
        .gpio_num = PIN_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = angulo_a_duty(ANGULO_CENTRO),
        .hpoint = 0,
    };
    ledc_channel_config(&canal);

    uint8_t indice = 0;
    while (1) {
        /* Mover servo al ángulo actual */
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0,
                      angulo_a_duty(ANGULOS[indice]));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(350));  /* tiempo de asentamiento */

        /* Medir distancia */
        float dist = medir_distancia();
        if (dist > 0.0f) {
            ESP_LOGI(TAG, "[%s] %.1f cm", NOMBRES[indice], dist);
        } else {
            ESP_LOGW(TAG, "[%s] sin eco", NOMBRES[indice]);
        }

        /* Siguiente ángulo */
        indice = (indice + 1) % 3;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

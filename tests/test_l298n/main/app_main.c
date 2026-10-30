/* ========================================================================
 *  IndoorWatch - Test: Puente H L298N (motores)
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Pines: IN1=GPIO 18, IN2=GPIO 19, IN3=GPIO 22, IN4=GPIO 23, ENA=GPIO 25
 *  ENB: ignorado (jumper en placa)
 *
 *  Compilar y flashear este test de forma independiente:
 *    cd tests/test_l298n
 *    idf.py build
 *    idf.py -p /dev/ttyUSB0 flash
 *    idf.py -p /dev/ttyUSB0 monitor
 * ======================================================================== */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "TEST_MOTORES";

#define PIN_IN1  18
#define PIN_IN2  19
#define PIN_IN3  22
#define PIN_IN4  23
#define PIN_ENA  25  /* Con jumper (velocidad fija) */
/* ENB: no usado (jumper en placa) */

static void motor_set(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
{
    gpio_set_level(PIN_IN1, in1);
    gpio_set_level(PIN_IN2, in2);
    gpio_set_level(PIN_IN3, in3);
    gpio_set_level(PIN_IN4, in4);
}

static void adelante(void)
{
    motor_set(1, 0, 1, 0);
    ESP_LOGI(TAG, "ADELANTE");
}

static void atras(void)
{
    motor_set(0, 1, 0, 1);
    ESP_LOGI(TAG, "ATRAS");
}

static void izquierda(void)
{
    motor_set(0, 1, 1, 0);  /* Giro sobre el centro */
    ESP_LOGI(TAG, "IZQUIERDA");
}

static void derecha(void)
{
    motor_set(1, 0, 0, 1);  /* Giro sobre el centro */
    ESP_LOGI(TAG, "DERECHA");
}

static void detener(void)
{
    motor_set(0, 0, 0, 0);
    ESP_LOGI(TAG, "DETENIDO");
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " Test: Puente H L298N");
    ESP_LOGI(TAG, " IN1=GPIO 18, IN2=GPIO 19, IN3=GPIO 22, IN4=GPIO 23");
    ESP_LOGI(TAG, " ENA=GPIO 25 (jumper) | ENB=no usado");
    ESP_LOGI(TAG, "==============================================");

    gpio_set_direction(PIN_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_IN4, GPIO_MODE_OUTPUT);

    detener();
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        adelante();
        vTaskDelay(pdMS_TO_TICKS(1500));
        detener();
        vTaskDelay(pdMS_TO_TICKS(500));

        atras();
        vTaskDelay(pdMS_TO_TICKS(1500));
        detener();
        vTaskDelay(pdMS_TO_TICKS(500));

        izquierda();
        vTaskDelay(pdMS_TO_TICKS(1500));
        detener();
        vTaskDelay(pdMS_TO_TICKS(500));

        derecha();
        vTaskDelay(pdMS_TO_TICKS(1500));
        detener();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

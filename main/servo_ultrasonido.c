#include "servo_ultrasonido.h"
#include "config.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "SERVO_SONAR";

static const uint8_t ANGULOS_ESCANEO[3] = {
    SERVO_ANGULO_MIN, SERVO_ANGULO_CENTRO, SERVO_ANGULO_MAX
};
static const char *NOMBRES_ANGULO[3] = { "IZQUIERDA", "FRENTE", "DERECHA" };

typedef enum {
    ESTADO_REPOSO = 0,
    ESTADO_MOVIENDO,
    ESTADO_ESPERANDO_ECO_ALTO,
    ESTADO_ESPERANDO_ECO_BAJO,
    ESTADO_PAUSA,
} estado_medicion_t;

static estado_medicion_t estado = ESTADO_REPOSO;
static float distancias_cm[3] = { NAN, NAN, NAN };
static uint8_t indice = 1;
static uint8_t angulo_actual = SERVO_ANGULO_CENTRO;
static bool escaner_activo = false;

static uint32_t fin_movimiento_ms = 0;
static uint32_t fin_pausa_ms = 0;
static uint32_t inicio_pulso_us = 0;
static uint32_t eco_inicio_us = 0;
static uint32_t ultima_medicion_ms = 0;

static uint32_t ahora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
static uint32_t ahora_us(void)
{
    return (uint32_t)esp_timer_get_time();
}

/* Mapear ángulo 0-180 a duty de LEDC (500-2400 us en periodo 20 ms, 13 bits) */
static uint32_t angulo_a_duty(uint8_t angulo)
{
    if (angulo > 180) angulo = 180;
    /* duty_min = 500us/20000us * 8192 = ~205
     * duty_max = 2400us/20000us * 8192 = ~983 */
    uint32_t duty_min = 205;
    uint32_t duty_max = 983;
    return duty_min + ((uint32_t)angulo * (duty_max - duty_min)) / 180;
}

static void mover_a(uint8_t angulo)
{
    uint32_t duty = angulo_a_duty(angulo);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    angulo_actual = angulo;
    estado = ESTADO_MOVIENDO;
    fin_movimiento_ms = ahora_ms() + SERVO_ASENTADO_MS;
}

static void iniciar_pulso(void)
{
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);
    esp_rom_delay_us(2);
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 1);
    esp_rom_delay_us(10);
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);

    inicio_pulso_us = ahora_us();
    estado = ESTADO_ESPERANDO_ECO_ALTO;
}

static void finalizar_medicion(float distancia_cm)
{
    distancias_cm[indice] = distancia_cm;
    ultima_medicion_ms = ahora_ms();

    if (distancia_cm > 0.0f && !isnan(distancia_cm)) {
        ESP_LOGI(TAG, "[SONAR] %s: %.1f cm",
                 NOMBRES_ANGULO[indice], distancia_cm);
    } else {
        ESP_LOGW(TAG, "[SONAR] %s: sin eco", NOMBRES_ANGULO[indice]);
    }

    if (escaner_activo) {
        indice = (indice + 1) % 3;
        estado = ESTADO_PAUSA;
        fin_pausa_ms = ahora_ms() + SERVO_PAUSA_MS;
    } else {
        indice = 1;
        estado = ESTADO_REPOSO;
    }
}

static void tick_estado(void)
{
    uint32_t ahora = ahora_ms();
    uint32_t ahora_us_val = ahora_us();

    switch (estado) {
    case ESTADO_REPOSO:
        if (!escaner_activo &&
            (ahora - ultima_medicion_ms >= SERVO_MEDICION_MANUAL_MS)) {
            iniciar_pulso();
        }
        break;

    case ESTADO_MOVIENDO:
        if ((int32_t)(ahora - fin_movimiento_ms) >= 0) {
            iniciar_pulso();
        }
        break;

    case ESTADO_ESPERANDO_ECO_ALTO:
        if (gpio_get_level((gpio_num_t)PIN_HCSR04_ECHO) == 1) {
            eco_inicio_us = ahora_us_val;
            estado = ESTADO_ESPERANDO_ECO_BAJO;
        } else if (ahora_us_val - inicio_pulso_us >= ULTRASONIDO_TIMEOUT_US) {
            finalizar_medicion(NAN);
        }
        break;

    case ESTADO_ESPERANDO_ECO_BAJO:
        if (gpio_get_level((gpio_num_t)PIN_HCSR04_ECHO) == 0) {
            uint32_t duracion_us = ahora_us_val - eco_inicio_us;
            finalizar_medicion((float)duracion_us / 58.0f);
        } else if (ahora_us_val - eco_inicio_us >= ULTRASONIDO_TIMEOUT_US) {
            finalizar_medicion(NAN);
        }
        break;

    case ESTADO_PAUSA:
        if ((int32_t)(ahora - fin_pausa_ms) >= 0) {
            if (escaner_activo) {
                mover_a(ANGULOS_ESCANEO[indice]);
            } else {
                estado = ESTADO_REPOSO;
            }
        }
        break;
    }
}

void servo_ultrasonido_init(void)
{
    /* HC-SR04 */
    gpio_set_direction((gpio_num_t)PIN_HCSR04_TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_HCSR04_ECHO, GPIO_MODE_INPUT);
    gpio_set_level((gpio_num_t)PIN_HCSR04_TRIG, 0);

    /* Servo SG90 via LEDC */
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
        .duty = angulo_a_duty(SERVO_ANGULO_CENTRO),
        .hpoint = 0,
    };
    ledc_channel_config(&canal);

    mover_a(SERVO_ANGULO_CENTRO);

    angulo_actual = SERVO_ANGULO_CENTRO;
    indice = 1;
    escaner_activo = false;
    estado = ESTADO_MOVIENDO;
    fin_movimiento_ms = ahora_ms() + SERVO_ASENTADO_MS;

    ESP_LOGI(TAG, "SG90 en GPIO%d. HC-SR04 TRIG=GPIO%d ECHO=GPIO%d. "
                  "Barrido %d-%d grados.",
             PIN_SERVO, PIN_HCSR04_TRIG, PIN_HCSR04_ECHO,
             SERVO_ANGULO_MIN, SERVO_ANGULO_MAX);
}

void servo_ultrasonido_tick(bool escanear)
{
    if (escanear && !escaner_activo) {
        escaner_activo = true;
        indice = 0;
        mover_a(ANGULOS_ESCANEO[indice]);
    } else if (!escanear && escaner_activo) {
        escaner_activo = false;
        indice = 1;
        mover_a(SERVO_ANGULO_CENTRO);
    }

    tick_estado();
}

void servo_ultrasonido_centrar(void)
{
    escaner_activo = false;
    indice = 1;
    mover_a(SERVO_ANGULO_CENTRO);
}

float servo_ultrasonido_distancia_cm(uint8_t posicion)
{
    if (posicion >= 3) return NAN;
    return distancias_cm[posicion];
}

float servo_ultrasonido_distancia_frente_cm(void)
{
    return distancias_cm[1];
}

float servo_ultrasonido_distancia_izquierda_cm(void)
{
    return distancias_cm[0];
}

float servo_ultrasonido_distancia_derecha_cm(void)
{
    return distancias_cm[2];
}

bool servo_ultrasonido_obstaculo_frente(void)
{
    float frente = distancias_cm[1];
    return !isnan(frente) && frente > 0.0f && frente < UMBRAL_OBSTACULO_CM;
}

uint8_t servo_ultrasonido_angulo_actual(void) { return angulo_actual; }
uint8_t servo_ultrasonido_indice_actual(void) { return indice; }
bool servo_ultrasonido_escaner_activo(void) { return escaner_activo; }

const char *servo_ultrasonido_direccion_mas_libre(void)
{
    float izquierda = distancias_cm[0];
    float derecha = distancias_cm[2];
    bool izq_valida = !isnan(izquierda) && izquierda > 0.0f;
    bool der_valida = !isnan(derecha) && derecha > 0.0f;

    if (!izq_valida && !der_valida) return "SIN DATOS";
    if (izq_valida && (!der_valida || izquierda > derecha + 5.0f)) {
        return "IZQUIERDA";
    }
    if (der_valida && (!izq_valida || derecha > izquierda + 5.0f)) {
        return "DERECHA";
    }
    return "FRENTE";
}

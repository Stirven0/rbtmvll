#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================
 *  IndoorWatch - Configuración central del firmware (ESP-IDF)
 *  Rover IoT de Monitoreo Ambiental en Interiores
 * ======================================================================== */

/* ------------------------------------------------------------------------
 *  Pines oficiales - Etapa 1 (ya cableados en protoboard)
 * ------------------------------------------------------------------------ */
#define PIN_DHT22        13   /* DHT22 DATA, alimentado a 3.3 V       */
#define PIN_MQ_ANALOG    35   /* MQ-2/MQ-135 AO, ADC1 (solo entrada)  */
#define PIN_FLAMA        34   /* Flama IR DO, ADC1 (solo entrada)     */
#define PIN_BATERIA      36   /* Divisor 20k/10k, ADC1 (solo entrada) */

#define PIN_HCSR04_TRIG  32   /* HC-SR04 TRIG                         */
#define PIN_HCSR04_ECHO  33   /* HC-SR04 ECHO (divisor 1k / 2k)       */

#define PIN_L298N_IN1    18
#define PIN_L298N_IN2    19
#define PIN_L298N_IN3    22
#define PIN_L298N_IN4    23
#define PIN_L298N_ENA    25   /* PWM opcional (jumper puesto)         */
/* PIN_L298N_ENB no se usa: la placa L298N lleva jumper en ENB        */

#define MOTOR_A_INVERTIDO    false
#define MOTOR_B_INVERTIDO    false

#define PIN_SERVO        26   /* SG90 señal PWM (GPIO 26)             */

/* ------------------------------------------------------------------------
 *  Umbrales de alerta (calibrados en campo)
 * ------------------------------------------------------------------------ */
#define UMBRAL_TEMP_C        40.0f   /* Alerta si temperatura > 40.0 °C   */
#define UMBRAL_GAS_ADC       2700    /* Línea base ~2000 en aire limpio   */
#define FLAMA_NIVEL_ACTIVO   0       /* LM393: 0 (LOW) = fuego detectado  */
#define UMBRAL_OBSTACULO_CM  20.0f

/* ------------------------------------------------------------------------
 *  Tiempos (ms) no bloqueantes
 * ------------------------------------------------------------------------ */
#define INTERVALO_SENSORES_MS         2500UL   /* DHT22 >= 2 s                 */
#define INTERVALO_TELEGRAM_MS         3000UL
#define COOLDOWN_ALERTA_MS            60000UL  /* Repetición                   */
#define INTERVALO_RECONEXION_WIFI_MS  15000UL
#define INTERVALO_REINTENTO_ENVIO_MS  10000UL
#define DURACION_MOVIMIENTO_MANUAL_MS 1500UL
#define DURACION_CONTROL_CONTINUO_MS  5000UL   /* Paro de seguridad            */
#define MANDO_TIMEOUT_MS              500UL
#define INTERVALO_TELEMETRIA_MS       30000UL
#define COOLDOWN_OBSTACULO_MS         30000UL
#define SERVO_ASENTADO_MS             350UL
#define SERVO_PAUSA_MS                80UL
#define SERVO_MEDICION_MANUAL_MS      1000UL
#define ULTRASONIDO_TIMEOUT_US        30000UL  /* ~5 m                         */

/* ------------------------------------------------------------------------
 *  Adquisición y validación
 * ------------------------------------------------------------------------ */
#define MUESTRAS_GAS          8
#define TEMP_MIN_VALIDA_C   -40.0f
#define TEMP_MAX_VALIDA_C    80.0f

/* ------------------------------------------------------------------------
 *  Mando Bluetooth directo (GameSir + Bluepad32)
 * ------------------------------------------------------------------------ */
#define MANDO_ZONA_MUERTA     80
#define MODO_INICIAL_AUTOMATICO  true
#define MANDO_EJE_Y_INVERTIDO    false
#define MANDO_EJE_X_INVERTIDO    false

/* ------------------------------------------------------------------------
 *  Servo SG90 + HC-SR04
 * ------------------------------------------------------------------------ */
#define SERVO_ANGULO_MIN    30
#define SERVO_ANGULO_CENTRO 90
#define SERVO_ANGULO_MAX    150

/* ------------------------------------------------------------------------
 *  Canales ADC1 (ESP32)
 * ------------------------------------------------------------------------ */
#define ADC_CHANNEL_MQ      ADC_CHANNEL_7   /* GPIO 35                    */
#define ADC_CHANNEL_FLAMA   ADC_CHANNEL_6   /* GPIO 34                    */
#define ADC_CHANNEL_BATERIA ADC_CHANNEL_0   /* GPIO 36                    */

/* ------------------------------------------------------------------------
 *  Tipos compartidos
 * ------------------------------------------------------------------------ */
typedef enum {
    MODO_MANUAL = 0,
    MODO_AUTOMATICO = 1,
} modo_operacion_t;

typedef enum {
    DIRECCION_DETENIDO = 0,
    DIRECCION_ADELANTE,
    DIRECCION_ATRAS,
    DIRECCION_IZQUIERDA,
    DIRECCION_DERECHA,
} direccion_movimiento_t;

typedef struct {
    float  temperatura_c;
    float  humedad_pct;
    int    gas_adc;
    int    gas_adc_crudo;
    bool   flama_detectada;
    bool   dht_valida;
    uint32_t ultima_actualizacion_ms;
} lecturas_ambientales_t;

void config_inicializar(void);

/* Pines helpers */
uint8_t config_pin_dht22(void);
uint8_t config_pin_mq_analog(void);
uint8_t config_pin_flama(void);
uint8_t config_pin_bateria(void);
uint8_t config_pin_hcsr04_trig(void);
uint8_t config_pin_hcsr04_echo(void);
uint8_t config_pin_l298n_in1(void);
uint8_t config_pin_l298n_in2(void);
uint8_t config_pin_l298n_in3(void);
uint8_t config_pin_l298n_in4(void);
uint8_t config_pin_l298n_ena(void);
uint8_t config_pin_servo(void);

uint32_t config_intervalo_sensores_ms(void);
float    config_umbral_temp_c(void);
int      config_umbral_gas_adc(void);

#endif /* CONFIG_H */

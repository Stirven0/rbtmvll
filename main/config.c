#include "config.h"
#include "esp_log.h"

static const char *TAG = "CONFIG";

void config_inicializar(void)
{
    ESP_LOGI(TAG, "Config cargada: ESP32 (esp32), ESP-WROOM-32");
    ESP_LOGI(TAG, "  DHT22=GPIO%d, MQ=GPIO%d, FLAMA=GPIO%d, BATERIA=GPIO%d",
             PIN_DHT22, PIN_MQ_ANALOG, PIN_FLAMA, PIN_BATERIA);
    ESP_LOGI(TAG, "  HC-SR04 TRIG=GPIO%d ECHO=GPIO%d", PIN_HCSR04_TRIG, PIN_HCSR04_ECHO);
    ESP_LOGI(TAG, "  L298N IN1=%d IN2=%d IN3=%d IN4=%d ENA=%d ENB=%d",
             PIN_L298N_IN1, PIN_L298N_IN2, PIN_L298N_IN3, PIN_L298N_IN4,
             PIN_L298N_ENA, PIN_L298N_ENB);
    ESP_LOGI(TAG, "  BT Serial RX=GPIO%d TX=GPIO%d @ %lu baudios",
             PIN_BT_RX, PIN_BT_TX, BT_BAUDIOS);
    ESP_LOGI(TAG, "  OLED I2C SDA=GPIO%d SCL=GPIO%d dir=0x%02X",
             PIN_OLED_SDA, PIN_OLED_SCL, OLED_DIRECCION);
    ESP_LOGI(TAG, "  Servo SG90=GPIO%d", PIN_SERVO);
}

/* --- Getters de pines --- */
uint8_t config_pin_dht22(void)       { return PIN_DHT22; }
uint8_t config_pin_mq_analog(void)   { return PIN_MQ_ANALOG; }
uint8_t config_pin_flama(void)       { return PIN_FLAMA; }
uint8_t config_pin_bateria(void)     { return PIN_BATERIA; }
uint8_t config_pin_hcsr04_trig(void) { return PIN_HCSR04_TRIG; }
uint8_t config_pin_hcsr04_echo(void) { return PIN_HCSR04_ECHO; }
uint8_t config_pin_l298n_in1(void)   { return PIN_L298N_IN1; }
uint8_t config_pin_l298n_in2(void)   { return PIN_L298N_IN2; }
uint8_t config_pin_l298n_in3(void)   { return PIN_L298N_IN3; }
uint8_t config_pin_l298n_in4(void)   { return PIN_L298N_IN4; }
uint8_t config_pin_l298n_ena(void)   { return PIN_L298N_ENA; }
uint8_t config_pin_l298n_enb(void)   { return PIN_L298N_ENB; }
uint8_t config_pin_bt_rx(void)       { return PIN_BT_RX; }
uint8_t config_pin_bt_tx(void)       { return PIN_BT_TX; }
uint8_t config_pin_oled_sda(void)    { return PIN_OLED_SDA; }
uint8_t config_pin_oled_scl(void)    { return PIN_OLED_SCL; }
uint8_t config_pin_servo(void)       { return PIN_SERVO; }

/* --- Getters de configuración --- */
uint32_t config_intervalo_sensores_ms(void) { return INTERVALO_SENSORES_MS; }
float    config_umbral_temp_c(void)         { return UMBRAL_TEMP_C; }
int      config_umbral_gas_adc(void)        { return UMBRAL_GAS_ADC; }

#include "sensores.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#include <math.h>

static const char *TAG = "SENSORES";

static lecturas_ambientales_t lecturas = { 0 };
static uint32_t ultima_lectura_ms = 0;

/* Handle del ADC oneshot (ADC_UNIT_1 para MQ y flama) */
static adc_oneshot_unit_handle_t adc_handle = NULL;

/* ------------------------------------------------------------------------
 *  DHT22 bit-banging sobre GPIO (un solo hilo de datos)
 * ------------------------------------------------------------------------ */
static bool dht22_leer(gpio_num_t pin, float *temperatura, float *humedad)
{
    uint8_t datos[5] = { 0 };

    /* 1. Maestro tira bajo 1-10 ms, luego alto 20-40 us, suelta el bus */
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    esp_rom_delay_us(1000);        /* 1 ms minimo */
    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    /* 2. Esperar respuesta del DHT22: bajo ~80 us, alto ~80 us */
    int tiempo_espera = 0;
    while (gpio_get_level(pin) == 0) {
        if (tiempo_espera++ > 100) { return false; }
        esp_rom_delay_us(1);
    }
    tiempo_espera = 0;
    while (gpio_get_level(pin) == 1) {
        if (tiempo_espera++ > 100) { return false; }
        esp_rom_delay_us(1);
    }

    /* 3. Leer 40 bits (5 bytes): humedad int, humedad dec, temp int, temp dec, checksum */
    for (int i = 0; i < 40; i++) {
        /* Flanco bajo de inicio de bit */
        tiempo_espera = 0;
        while (gpio_get_level(pin) == 0) {
            if (tiempo_espera++ > 100) { return false; }
            esp_rom_delay_us(1);
        }
        /* Muestra alrededor de 30 us dentro del pulso alto */
        esp_rom_delay_us(30);
        uint8_t bit = gpio_get_level(pin) ? 1 : 0;
        datos[i / 8] <<= 1;
        datos[i / 8] |= bit;
        /* Esperar a que el bus vuelva bajo (fin del bit) */
        tiempo_espera = 0;
        while (gpio_get_level(pin) == 1) {
            if (tiempo_espera++ > 100) { return false; }
            esp_rom_delay_us(1);
        }
    }

    /* 4. Validar checksum */
    uint8_t checksum_calc = (uint8_t)(datos[0] + datos[1] + datos[2] + datos[3]);
    if (datos[4] != checksum_calc) {
        ESP_LOGW(TAG, "DHT22 checksum invalido: leído=0x%02X calculado=0x%02X",
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

static int leer_adc_promediado(adc_channel_t canal, uint8_t muestras)
{
    long suma = 0;
    for (uint8_t i = 0; i < muestras; ++i) {
        int raw = 0;
        adc_oneshot_read(adc_handle, canal, &raw);
        suma += raw;
    }
    return (int)(suma / muestras);
}

void sensores_init(void)
{
    /* ADC oneshot: 12 bits, atenuación 11 dB (rango ~0-3.3V) para MQ y flama */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle,
                                                (adc_channel_t)ADC_CHANNEL_MQ,
                                                &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle,
                                                (adc_channel_t)ADC_CHANNEL_FLAMA,
                                                &chan_cfg));

    ultima_lectura_ms = 0;  /* primera lectura tras INTERVALO_SENSORES_MS */

    ESP_LOGI(TAG, "DHT22=GPIO%d, MQ=GPIO%d, FLAMA=GPIO%d (ADC)",
             PIN_DHT22, PIN_MQ_ANALOG, PIN_FLAMA);
}

bool sensores_tick(void)
{
    uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (ultima_lectura_ms == 0 ||
        ahora - ultima_lectura_ms >= INTERVALO_SENSORES_MS) {
        ultima_lectura_ms = ahora;

        /* MQ: promedio de varias muestras */
        lecturas.gas_adc = leer_adc_promediado((adc_channel_t)ADC_CHANNEL_MQ,
                                                MUESTRAS_GAS);
        lecturas.gas_adc_crudo = lecturas.gas_adc;

        /* Flama: lectura analógica por ADC */
        lecturas.flama_adc = leer_adc_promediado((adc_channel_t)ADC_CHANNEL_FLAMA,
                                                  MUESTRAS_FLAMA);
        /* Nota: según el sensor de flama, ajusta el sentido de la comparación.
         * Aquí asumimos que lectura alta = flama detectada. */
        lecturas.flama_detectada = (lecturas.flama_adc > UMBRAL_FLAMA_ADC);

        /* DHT22 */
        float t = 0.0f, h = 0.0f;
        if (dht22_leer((gpio_num_t)PIN_DHT22, &t, &h)) {
            bool valida = !isnan(t) && !isnan(h) &&
                          t >= TEMP_MIN_VALIDA_C && t <= TEMP_MAX_VALIDA_C &&
                          h >= 0.0f && h <= 100.0f;
            if (valida) {
                lecturas.temperatura_c = t;
                lecturas.humedad_pct = h;
                lecturas.dht_valida = true;
            } else {
                lecturas.dht_valida = false;
                ESP_LOGW(TAG, "DHT22 lectura fuera de rango: T=%.1f H=%.1f", t, h);
            }
        } else {
            lecturas.dht_valida = false;
            ESP_LOGW(TAG, "DHT22 fallo de lectura");
        }

        lecturas.ultima_actualizacion_ms = ahora;
        return true;
    }
    return false;
}

const lecturas_ambientales_t *sensores_obtener_puntero(void)
{
    return &lecturas;
}

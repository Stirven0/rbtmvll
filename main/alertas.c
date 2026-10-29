#include "alertas.h"
#include "config.h"
#include "telegram_bot.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_timer.h"

static const char *TAG = "ALERTAS";

#define ALERTA_TEMP  0x01
#define ALERTA_GAS   0x02
#define ALERTA_FLAMA 0x04

static uint8_t mascara_notificada = 0;
static bool condicion_temp_activa = false;
static uint32_t ultimo_envio_ms = 0;
static uint32_t ultimo_intento_ms = 0;
static bool intento_fallido = false;

static uint32_t ahora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint8_t evaluar_alertas(const lecturas_ambientales_t *lec)
{
    uint8_t mascara = 0;

    if (lec->dht_valida) {
        condicion_temp_activa = (lec->temperatura_c > UMBRAL_TEMP_C);
    }
    if (condicion_temp_activa) {
        mascara |= ALERTA_TEMP;
    }
    if (lec->gas_adc > UMBRAL_GAS_ADC) {
        mascara |= ALERTA_GAS;
    }
    if (lec->flama_detectada) {
        mascara |= ALERTA_FLAMA;
    }
    return mascara;
}

static void construir_alerta(char *buffer, size_t tam,
                             uint8_t mascara, const lecturas_ambientales_t *lec)
{
    int pos = 0;
    pos += snprintf(buffer + pos, tam - pos,
                    "🚨 *ALERTA AMBIENTAL — IndoorWatch*\n\n");

    if (mascara & ALERTA_TEMP) {
        pos += snprintf(buffer + pos, tam - pos,
            "🌡️ *Temperatura crítica:* %.1f °C (límite %.1f °C)\n",
            lec->temperatura_c, UMBRAL_TEMP_C);
        if (!lec->dht_valida) {
            pos += snprintf(buffer + pos, tam - pos,
                "⚠️ DHT22 sin lectura actual; se muestra la última válida.\n");
        }
    }

    if (mascara & ALERTA_GAS) {
        pos += snprintf(buffer + pos, tam - pos,
            "💨 *Gas/humo elevado:* %d ADC (límite %d ADC)\n",
            lec->gas_adc, UMBRAL_GAS_ADC);
    }

    if (mascara & ALERTA_FLAMA) {
        pos += snprintf(buffer + pos, tam - pos,
            "🔥 *Flama detectada:* sensor IR en LOW\n");
    }

    pos += snprintf(buffer + pos, tam - pos,
        "\nAlerta automática. Se repetirá cada %lu s mientras la condición persista.",
        (unsigned long)COOLDOWN_ALERTA_MS / 1000UL);
}

static void construir_normalizacion(char *buffer, size_t tam,
                                    uint8_t mascara, const lecturas_ambientales_t *lec)
{
    int pos = 0;
    pos += snprintf(buffer + pos, tam - pos,
                    "✅ *Valores normalizados — IndoorWatch*\n\n");

    if (mascara & ALERTA_TEMP) {
        pos += snprintf(buffer + pos, tam - pos, "🌡️ Temperatura: ");
        if (lec->dht_valida) {
            pos += snprintf(buffer + pos, tam - pos, "%.1f °C\n", lec->temperatura_c);
        } else {
            pos += snprintf(buffer + pos, tam - pos, "sin lectura válida\n");
        }
    }

    if (mascara & ALERTA_GAS) {
        pos += snprintf(buffer + pos, tam - pos, "💨 Gas/humo: %d ADC\n", lec->gas_adc);
    }

    if (mascara & ALERTA_FLAMA) {
        pos += snprintf(buffer + pos, tam - pos, "🔥 Flama: sin detección\n");
    }

    pos += snprintf(buffer + pos, tam - pos, "\nEl sistema continúa monitoreando.");
}

void alertas_init(void)
{
    mascara_notificada = 0;
    condicion_temp_activa = false;
    ultimo_envio_ms = 0;
    ultimo_intento_ms = 0;
    intento_fallido = false;

    ESP_LOGI(TAG, "Umbrales: temp > %.1f C, gas > %d, flama LOW.",
             UMBRAL_TEMP_C, UMBRAL_GAS_ADC);
}

void alertas_tick(const lecturas_ambientales_t lecturas)
{
    uint8_t mascara = evaluar_alertas(&lecturas);
    uint32_t ahora = ahora_ms();

    bool puede_intentar = !intento_fallido ||
        (ahora - ultimo_intento_ms >= INTERVALO_REINTENTO_ENVIO_MS);

    /* Transición de alerta activa a normal */
    if (mascara == 0 && mascara_notificada != 0) {
        if (!puede_intentar) return;
        ultimo_intento_ms = ahora;

        char msg[256];
        construir_normalizacion(msg, sizeof(msg), mascara_notificada, &lecturas);
        if (telegram_enviar_mensaje(msg)) {
            ESP_LOGI(TAG, "Normalización notificada.");
            mascara_notificada = 0;
            ultimo_envio_ms = ahora;
            intento_fallido = false;
        } else {
            intento_fallido = true;
            ESP_LOGW(TAG, "No se pudo enviar normalización; se reintentará.");
        }
        return;
    }

    if (mascara == 0) return;

    bool cambio = (mascara != mascara_notificada);
    bool cooldown_vencido = (ultimo_envio_ms == 0) ||
        (ahora - ultimo_envio_ms >= COOLDOWN_ALERTA_MS);

    if ((!cambio && !cooldown_vencido) || !puede_intentar) {
        return;
    }

    ultimo_intento_ms = ahora;

    char msg[256];
    construir_alerta(msg, sizeof(msg), mascara, &lecturas);
    if (telegram_enviar_mensaje(msg)) {
        ESP_LOGI(TAG, "Alerta enviada (máscara=0x%02X).", mascara);
        mascara_notificada = mascara;
        ultimo_envio_ms = ahora;
        intento_fallido = false;
    } else {
        ESP_LOGW(TAG, "No se pudo enviar alerta; se reintentará.");
        intento_fallido = true;
    }
}

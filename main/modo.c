#include "modo.h"
#include "config.h"
#include "control_bt.h"
#include "motores.h"
#include "servo_ultrasonido.h"
#include "telegram_bot.h"
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "MODO";

static modo_operacion_t modo = MODO_AUTOMATICO;

static void aplicar_modo(modo_operacion_t nuevo, bool avisar_telegram)
{
    motores_detener();

    if (nuevo == MODO_MANUAL) {
        if (avisar_telegram) {
            telegram_enviar_mensaje(
                "🎮 *Modo MANUAL*\n\nWi-Fi se apagará para liberar la radio. "
                "Usa el GameSir: B = datos, Y = volver a automático.");
        }
        control_bt_activar();
        wifi_apagar();
        servo_ultrasonido_centrar();
        ESP_LOGI(TAG, "MANUAL: Wi-Fi OFF | Bluetooth ON | B=datos, Y=auto.");
    } else {
        control_bt_desactivar();
        wifi_encender();
        servo_ultrasonido_centrar();
        ESP_LOGI(TAG, "AUTOMATICO: Wi-Fi ON | Bluetooth OFF | servo escaneando.");
    }

    modo = nuevo;
}

void modo_init(void)
{
    modo_operacion_t inicial = MODO_INICIAL_AUTOMATICO ? MODO_AUTOMATICO
                                                       : MODO_MANUAL;
    modo = inicial;
    aplicar_modo(inicial, false);
}

bool modo_cambiar(modo_operacion_t nuevo)
{
    if (nuevo == modo) {
        return false;
    }
    aplicar_modo(nuevo, true);
    return true;
}

modo_operacion_t modo_actual(void) { return modo; }

const char *modo_nombre(void)
{
    return modo == MODO_MANUAL ? "MANUAL" : "AUTOMATICO";
}

bool modo_es_manual(void)  { return modo == MODO_MANUAL; }
bool modo_es_automatico(void) { return modo == MODO_AUTOMATICO; }

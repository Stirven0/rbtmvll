#include "wifi_manager.h"
#include "config.h"
#include "secrets.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"

static const char *TAG = "WIFI";

static bool habilitado = false;
static bool conectado_previo = false;
static uint32_t ultimo_intento_ms = 0;

static esp_netif_t *netif_sta = NULL;

/* Handler de eventos Wi-Fi */
static void evento_wifi(void *arg, esp_event_base_t base,
                        int32_t id_evento, void *datos_evento)
{
    if (base == WIFI_EVENT && id_evento == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id_evento == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi desconectado. Reintentando...");
        if (habilitado) {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id_evento == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evento = (ip_event_got_ip_t *)datos_evento;
        ESP_LOGI(TAG, "Wi-Fi conectado. IP: " IPSTR, IP2STR(&evento->ip_info.ip));
    }
}

void wifi_encender(void)
{
    if (habilitado) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &evento_wifi,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &evento_wifi,
                                                        NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    habilitado = true;
    conectado_previo = false;
    ultimo_intento_ms = 0;

    ESP_LOGI(TAG, "Encendiendo Wi-Fi. Conectando a: %s", WIFI_SSID);
}

void wifi_apagar(void)
{
    if (!habilitado) return;

    esp_wifi_stop();
    esp_wifi_deinit();
    if (netif_sta) {
        esp_netif_destroy_default_wifi(netif_sta);
        netif_sta = NULL;
    }

    habilitado = false;
    conectado_previo = false;
    ESP_LOGI(TAG, "Wi-Fi apagado. Radio libre para Bluetooth.");
}

void wifi_init(void)
{
    wifi_encender();
}

void wifi_tick(void)
{
    if (!habilitado) return;

    uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000ULL);
    bool conectado = wifi_conectado();

    if (conectado && !conectado_previo) {
        ESP_LOGI(TAG, "Wi-Fi conectado. IP: %s | RSSI: %d dBm",
                 wifi_ip(), wifi_rssi());
    } else if (!conectado && conectado_previo) {
        ESP_LOGW(TAG, "Conexion Wi-Fi perdida. Se reintentara automaticamente.");
    }

    if (!conectado &&
        (ahora - ultimo_intento_ms >= INTERVALO_RECONEXION_WIFI_MS)) {
        ultimo_intento_ms = ahora;
        ESP_LOGI(TAG, "Reintentando conexion Wi-Fi...");
        esp_wifi_connect();
    }

    conectado_previo = conectado;
}

bool wifi_habilitado(void) { return habilitado; }

bool wifi_conectado(void)
{
    if (!habilitado) return false;
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
}

const char *wifi_ip(void)
{
    static char ip_str[16];
    if (!habilitado || !netif_sta) {
        snprintf(ip_str, sizeof(ip_str), "apagado");
        return ip_str;
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif_sta, &info) == ESP_OK) {
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&info.ip));
    } else {
        snprintf(ip_str, sizeof(ip_str), "sin IP");
    }
    return ip_str;
}

int wifi_rssi(void)
{
    if (!habilitado) return 0;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

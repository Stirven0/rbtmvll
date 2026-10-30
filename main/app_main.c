#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "config.h"
#include "sensores.h"
#include "motores.h"
#include "control_bt.h"
#include "wifi_manager.h"
#include "telegram_bot.h"
#include "alertas.h"
#include "modo.h"
#include "comandos.h"

static const char *TAG = "APP_MAIN";

#define TAILA_PILA_SENSORES     4096
#define TAILA_PILA_MOTORES      3072
#define TAILA_PILA_CONTROL_BT   4096
#define TAILA_PILA_WIFI_TG      6144
#define TAILA_PILA_ALERTAS      3072
#define TAILA_PILA_COMANDOS_USB 3072

#define PRIO_SENSORES           5
#define PRIO_MOTORES            6
#define PRIO_CONTROL_BT         5
#define PRIO_WIFI_TG            5
#define PRIO_ALERTAS            5
#define PRIO_COMANDOS_USB       5

/* Mutex para proteger las lecturas de sensores compartidas entre tareas */
static SemaphoreHandle_t xSensoresMutex = NULL;

/* Cola de comandos recibidos por USB/Serial y Bluetooth serial */
static QueueHandle_t xComandosQueue = NULL;

typedef struct {
    char comando[16];
    char origen[8];   /* "USB" o "BT" */
} comando_t;

/* Re-exportamos las lecturas para que las tareas puedan usarlas */
extern const lecturas_ambientales_t *sensores_obtener_puntero(void);

static void tarea_sensores(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea sensores iniciada");
    sensores_init();

    while (1) {
        if (sensores_tick()) {
            if (xSemaphoreTake(xSensoresMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                /* sensores_actualizar_lectura interna */
                xSemaphoreGive(xSensoresMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void tarea_motores(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea motores iniciada");
    motores_init();

    while (1) {
        motores_tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void tarea_control_bt(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea control Bluetooth iniciada");
    control_bt_init();

    while (1) {
        control_bt_tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void tarea_wifi_telegram(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea Wi-Fi/Telegram iniciada");
    wifi_init();
    telegram_init();

    while (1) {
        wifi_tick();

        if (wifi_conectado()) {
            const lecturas_ambientales_t *lec = sensores_obtener_puntero();
            if (lec && xSemaphoreTake(xSensoresMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                telegram_tick(*lec);
                xSemaphoreGive(xSensoresMutex);
            } else {
                telegram_tick((lecturas_ambientales_t){0});
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void tarea_alertas(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea alertas iniciada");
    alertas_init();

    while (1) {
        const lecturas_ambientales_t *lec = sensores_obtener_puntero();
        if (lec && xSemaphoreTake(xSensoresMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            alertas_tick(*lec);
            xSemaphoreGive(xSensoresMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void tarea_comandos_usb(void *pvParameter)
{
    ESP_LOGI(TAG, "Tarea comandos USB iniciada");

    while (1) {
        comandos_procesar_serial("USB");
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, " IndoorWatch - Rover IoT de monitoreo (ESP-IDF)");
    ESP_LOGI(TAG, " Etapa 1 + motores L298N + mando Bluetooth");
    ESP_LOGI(TAG, "==============================================");

    /* Inicializar NVS (requerido por Wi-Fi y BT) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Mostrar información del chip */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: %s con %d core(s)", CONFIG_IDF_TARGET, chip_info.cores);
    ESP_LOGI(TAG, "Revisión: %d", chip_info.revision);
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %u MB", flash_size / (1024 * 1024));

    /* Crear primitivas de sincronización */
    xSensoresMutex = xSemaphoreCreateMutex();
    xComandosQueue = xQueueCreate(16, sizeof(comando_t));

    config_inicializar();

    /* Crear tareas FreeRTOS (una por módulo del loop original) */
    xTaskCreate(tarea_sensores,      "sensores",      TAILA_PILA_SENSORES,      NULL, PRIO_SENSORES,      NULL);
    xTaskCreate(tarea_motores,       "motores",       TAILA_PILA_MOTORES,       NULL, PRIO_MOTORES,       NULL);
    xTaskCreate(tarea_control_bt,    "control_bt",    TAILA_PILA_CONTROL_BT,    NULL, PRIO_CONTROL_BT,    NULL);
    xTaskCreate(tarea_wifi_telegram, "wifi_telegram", TAILA_PILA_WIFI_TG,       NULL, PRIO_WIFI_TG,       NULL);
    xTaskCreate(tarea_alertas,       "alertas",       TAILA_PILA_ALERTAS,       NULL, PRIO_ALERTAS,       NULL);
    xTaskCreate(tarea_comandos_usb,  "comandos_usb",  TAILA_PILA_COMANDOS_USB,  NULL, PRIO_COMANDOS_USB,  NULL);

    ESP_LOGI(TAG, "Sistema listo. Todas las tareas iniciadas.");
    ESP_LOGI(TAG, "Consola USB: 115200 baudios | Bluetooth clasico: mando GameSir");
}

/* ========================================================================
 *  IndoorWatch - Menú de pruebas por módulo (ESP-IDF)
 *  ESP32 (esp32) - ESP-WROOM-32
 *
 *  Compilar y flashear este archivo en lugar de app_main.c para probar
 *  cada módulo por separado. Los resultados se imprimen por USB/UART0
 *  (115200 baudios). Usar: idf.py monitor
 * ======================================================================== */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#include "servo_ultrasonido.h"

static void imprimir_encabezado(const char *modulo)
{
    printf("\n============================================================\n");
    printf(" TEST: %s\n", modulo);
    printf("============================================================\n");
    fflush(stdout);
}

static void test_dht22(void)
{
    imprimir_encabezado("DHT22 - GPIO 13");

    sensores_init();

    printf("Leyendo DHT22 cada 2 segundos (Ctrl-C para salir)...\n");
    fflush(stdout);

    for (int i = 0; i < 10; i++) {
        if (sensores_tick()) {
            const lecturas_ambientales_t *lec = sensores_obtener_puntero();
            if (lec) {
                printf("[%2d] T=%.1f C | H=%.1f %% | DHT=%s\n",
                       i + 1,
                       lec->temperatura_c,
                       lec->humedad_pct,
                       lec->dht_valida ? "OK" : "ERROR");
            }
        } else {
            printf("[%2d] Esperando siguiente lectura...\n", i + 1);
        }
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    printf("Test DHT22 finalizado.\n\n");
}

static void test_flama(void)
{
    imprimir_encabezado("Sensor Flama IR - GPIO 34");

    sensores_init();

    printf("Leyendo sensor de flama cada 500 ms (Ctrl-C para salir)...\n");
    printf("Acercar una llama o fuente IR al sensor para detectar.\n");
    fflush(stdout);

    for (int i = 0; i < 20; i++) {
        if (sensores_tick()) {
            const lecturas_ambientales_t *lec = sensores_obtener_puntero();
            if (lec) {
                printf("[%2d] Flama: %s | Gas: %d ADC\n",
                       i + 1,
                       lec->flama_detectada ? "🔥 FUEGO" : "✅ OK",
                       lec->gas_adc);
            }
        }
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("Test Flama finalizado.\n\n");
}

static void test_mq(void)
{
    imprimir_encabezado("Sensor MQ-135 - GPIO 35 (ADC1)");

    sensores_init();

    printf("Leyendo sensor de gas cada 1 segundo (Ctrl-C para salir)...\n");
    printf("Umbral de alerta: %d ADC\n", UMBRAL_GAS_ADC);
    fflush(stdout);

    for (int i = 0; i < 15; i++) {
        if (sensores_tick()) {
            const lecturas_ambientales_t *lec = sensores_obtener_puntero();
            if (lec) {
                printf("[%2d] Gas: %d ADC %s\n",
                       i + 1,
                       lec->gas_adc,
                       lec->gas_adc > UMBRAL_GAS_ADC ? "⚠️  ELEVADO" : "✅ normal");
            }
        }
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("Test MQ-135 finalizado.\n\n");
}

static void test_ultrasonido(void)
{
    imprimir_encabezado("HC-SR04 - TRIG GPIO 32 / ECHO GPIO 33");

    servo_ultrasonido_init();

    printf("Leyendo distancia cada 500 ms (Ctrl-C para salir)...\n");
    fflush(stdout);

    for (int i = 0; i < 20; i++) {
        servo_ultrasonido_tick(true);  /* escanear: izquierda-frente-derecha */
        float frente = servo_ultrasonido_distancia_frente_cm();
        float izq    = servo_ultrasonido_distancia_izquierda_cm();
        float der    = servo_ultrasonido_distancia_derecha_cm();

        printf("[%2d] Frente: %.1f cm | Izq: %.1f cm | Der: %.1f cm | "
               "Obstáculo: %s\n",
               i + 1,
               frente, izq, der,
               servo_ultrasonido_obstaculo_frente() ? "SÍ" : "no");

        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    printf("Test Ultrasonido finalizado.\n\n");
}

static void test_l298n(void)
{
    imprimir_encabezado("L298N Puente H - IN: 18,19,22,23 / ENA: 25");

    motores_init();

    printf("Secuencia de prueba de motores...\n");
    fflush(stdout);

    printf("-> Adelante 1.5 s\n");
    motores_mover(DIRECCION_ADELANTE, 1500);
    vTaskDelay(pdMS_TO_TICKS(1700));

    printf("-> Atrás 1.5 s\n");
    motores_mover(DIRECCION_ATRAS, 1500);
    vTaskDelay(pdMS_TO_TICKS(1700));

    printf("-> Izquierda 1.5 s\n");
    motores_mover(DIRECCION_IZQUIERDA, 1500);
    vTaskDelay(pdMS_TO_TICKS(1700));

    printf("-> Derecha 1.5 s\n");
    motores_mover(DIRECCION_DERECHA, 1500);
    vTaskDelay(pdMS_TO_TICKS(1700));

    printf("-> Detener\n");
    motores_detener();
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("Test L298N finalizado.\n\n");
}

static void test_wifi(void)
{
    imprimir_encabezado("Conectividad Wi-Fi");

    wifi_init();

    printf("Conectando a Wi-Fi...\n");
    fflush(stdout);

    /* Esperar hasta 30 segundos a que se conecte */
    for (int i = 0; i < 60; i++) {
        if (wifi_conectado()) {
            printf("✅ Conectado!\n");
            printf("   IP: %s\n", wifi_ip());
            printf("   RSSI: %d dBm\n", wifi_rssi());
            printf("\nTest Wi-Fi finalizado.\n\n");
            return;
        }
        printf("   Esperando conexión... (%d s)\n", (i + 1) * 5);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    printf("❌ No se pudo conectar a Wi-Fi.\n\n");
}

static void test_bluetooth(void)
{
    imprimir_encabezado("Bluetooth Clásico - Mando GameSir");

    control_bt_init();

    printf("Inicializando Bluetooth clásico...\n");
    printf("Encienda el mando GameSir en modo emparejamiento.\n");
    printf("Esperando conexión (Ctrl-C para salir)...\n");
    fflush(stdout);

    for (int i = 0; i < 60; i++) {
        control_bt_tick();

        if (control_bt_conectado()) {
            printf("✅ Mando conectado: %s\n", control_bt_nombre());
            printf("   Presione botones del mando para verificar.\n");
            fflush(stdout);

            /* Monitorear actividad del mando durante 30 segundos */
            for (int j = 0; j < 60; j++) {
                control_bt_tick();
                if (control_bt_consumir_solicitud_datos()) {
                    printf("   📋 Solicitud de datos del mando\n");
                    fflush(stdout);
                }
                if (control_bt_consumir_solicitud_auto()) {
                    printf("   🔄 Solicitud de modo automático\n");
                    fflush(stdout);
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            break;
        }

        if ((i % 4) == 0) {
            printf("   Esperando mando... (%d s)\n", i * 5);
            fflush(stdout);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    if (!control_bt_conectado()) {
        printf("❌ No se conectó ningún mando.\n");
    }

    printf("Test Bluetooth finalizado.\n\n");
}

void test_menu_run(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    /* Inicializar NVS (requerido por Wi-Fi y BT) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("\n============================================================\n");
    printf(" IndoorWatch - Menú de Pruebas por Módulo\n");
    printf(" ESP32 (esp32) - ESP-WROOM-32\n");
    printf(" Core(s): %d\n", chip_info.cores);
    printf("============================================================\n");
    fflush(stdout);

    while (1) {
        printf("\nSeleccione el módulo a probar:\n");
        printf("  1 - DHT22 (GPIO 13)\n");
        printf("  2 - Sensor Flama IR (GPIO 34)\n");
        printf("  3 - Sensor MQ-135 (GPIO 35)\n");
        printf("  4 - Ultrasonido HC-SR04 (TRIG 32 / ECHO 33)\n");
        printf("  5 - Puente H L298N (motores)\n");
        printf("  6 - Conectividad Wi-Fi\n");
        printf("  7 - Bluetooth - Mando GameSir\n");
        printf("  0 - Salir (reiniciar)\n");
        printf("> ");
        fflush(stdout);

        /* Leer opción por serial */
        char opcion[8] = {0};
        int leidos = 0;
        while (leidos < (int)sizeof(opcion) - 1) {
            uint8_t byte;
            int n = read(STDIN_FILENO, &byte, 1);
            if (n > 0) {
                char c = (char)byte;
                if (c == '\n' || c == '\r') {
                    break;
                }
                opcion[leidos++] = c;
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        int op = atoi(opcion);
        printf("\n");

        switch (op) {
        case 1:
            test_dht22();
            break;
        case 2:
            test_flama();
            break;
        case 3:
            test_mq();
            break;
        case 4:
            test_ultrasonido();
            break;
        case 5:
            test_l298n();
            break;
        case 6:
            test_wifi();
            break;
        case 7:
            test_bluetooth();
            break;
        case 0:
            printf("Reiniciando...\n");
            fflush(stdout);
            esp_restart();
            break;
        default:
            printf("Opción no válida.\n");
            break;
        }
    }
}

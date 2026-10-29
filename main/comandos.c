#include "comandos.h"
#include "config.h"
#include "control_bt.h"
#include "motores.h"
#include <ctype.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "COMANDOS";

#define UART_NUM_USB UART_NUM_0
#define UART_NUM_BT  UART_NUM_2

#define TAM_BUFFER 32
#define TIEMPO_SILENCIO_MS 60  /* silencio para procesar letra sin Enter */

/* Estado del parser por puerto */
typedef struct {
    uint8_t buffer[TAM_BUFFER];
    uint8_t longitud;
    uint32_t ultimo_char_ms;
    bool descartar;
} parser_t;

static parser_t parser_usb = { 0 };
static parser_t parser_bt  = { 0 };

static uint32_t ahora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void imprimir_linea_uart(uart_port_t puerto, bool eco,
                                const char *texto)
{
    if (eco && puerto == UART_NUM_BT) {
        uart_write_bytes(puerto, texto, strlen(texto));
        uart_write_bytes(puerto, "\r\n", 2);
    } else {
        ESP_LOGI(TAG, "%s", texto);
    }
}

static void imprimir_estado_uart(uart_port_t puerto, bool eco)
{
    char estado[96];
    snprintf(estado, sizeof(estado),
             "Estado motores: %s | Mando: %s",
             motores_estado_texto(),
             control_bt_conectado() ? control_bt_nombre() : "desconectado");
    imprimir_linea_uart(puerto, eco, estado);
}

static void imprimir_ayuda_uart(uart_port_t puerto, bool eco)
{
    imprimir_linea_uart(puerto, eco,
        "Comandos: a=adelante r=atras i=izquierda d=derecha");
    imprimir_linea_uart(puerto, eco,
        "s=detener p=demo m=estado h=ayuda");
    imprimir_linea_uart(puerto, eco,
        "Mando: b=olvidar claves e=rebuscar x=debug");
    imprimir_linea_uart(puerto, eco,
        "a/r/i/d siguen hasta s o hasta 5 s sin una nueva orden.");
}

static void ejecutar_comando(const char *token, uart_port_t puerto, bool eco)
{
    if (token[0] == '\0') return;

    if (strlen(token) == 1) {
        char c = token[0];
        switch (c) {
        case 'a':
            motores_control_continuo(DIRECCION_ADELANTE);
            if (eco) imprimir_linea_uart(puerto, eco, "OK: adelante (s=detener)");
            return;
        case 'r':
            motores_control_continuo(DIRECCION_ATRAS);
            if (eco) imprimir_linea_uart(puerto, eco, "OK: atras (s=detener)");
            return;
        case 'i':
            motores_control_continuo(DIRECCION_IZQUIERDA);
            if (eco) imprimir_linea_uart(puerto, eco, "OK: izquierda (s=detener)");
            return;
        case 'd':
            motores_control_continuo(DIRECCION_DERECHA);
            if (eco) imprimir_linea_uart(puerto, eco, "OK: derecha (s=detener)");
            return;
        case 's':
            motores_detener();
            if (eco) imprimir_linea_uart(puerto, eco, "OK: detenido");
            return;
        case 'p':
            motores_demo();
            if (eco) imprimir_linea_uart(puerto, eco, "OK: demo iniciada");
            return;
        case 'm':
            imprimir_estado_uart(puerto, eco);
            return;
        case 'b':
            control_bt_olvidar_claves();
            if (eco) imprimir_linea_uart(puerto, eco, "OK: claves del mando olvidadas");
            return;
        case 'e':
            control_bt_reiniciar_busqueda();
            if (eco) imprimir_linea_uart(puerto, eco, "OK: busqueda del mando reiniciada");
            return;
        case 'x':
            control_bt_alternar_debug();
            if (eco) imprimir_linea_uart(puerto, eco, "OK: debug del mando alternado");
            return;
        case 'h':
        case '?':
            imprimir_ayuda_uart(puerto, eco);
            return;
        default:
            return;
        }
    }

    if (strcmp(token, "adelante") == 0) {
        motores_control_continuo(DIRECCION_ADELANTE);
        if (eco) imprimir_linea_uart(puerto, eco, "OK: adelante (s=detener)");
    } else if (strcmp(token, "atras") == 0) {
        motores_control_continuo(DIRECCION_ATRAS);
        if (eco) imprimir_linea_uart(puerto, eco, "OK: atras (s=detener)");
    } else if (strcmp(token, "izquierda") == 0) {
        motores_control_continuo(DIRECCION_IZQUIERDA);
        if (eco) imprimir_linea_uart(puerto, eco, "OK: izquierda (s=detener)");
    } else if (strcmp(token, "derecha") == 0) {
        motores_control_continuo(DIRECCION_DERECHA);
        if (eco) imprimir_linea_uart(puerto, eco, "OK: derecha (s=detener)");
    } else if (strcmp(token, "detener") == 0 || strcmp(token, "stop") == 0) {
        motores_detener();
        if (eco) imprimir_linea_uart(puerto, eco, "OK: detenido");
    } else if (strcmp(token, "demo") == 0) {
        motores_demo();
        if (eco) imprimir_linea_uart(puerto, eco, "OK: demo iniciada");
    } else if (strcmp(token, "estado") == 0) {
        imprimir_estado_uart(puerto, eco);
    } else if (strcmp(token, "ayuda") == 0) {
        imprimir_ayuda_uart(puerto, eco);
    }
    /* Cualquier otra cadena se ignora. */
}

static void procesar_buffer(parser_t *parser, uart_port_t puerto,
                            bool eco, const char *origen)
{
    parser->buffer[parser->longitud] = '\0';

    char token[TAM_BUFFER + 1];
    strncpy(token, (const char *)parser->buffer, TAM_BUFFER);
    token[TAM_BUFFER] = '\0';

    /* trim y lowercase */
    char *p = token;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == ' ') { p[--len] = '\0'; }
    for (size_t i = 0; i < len; i++) p[i] = (char)tolower((unsigned char)p[i]);

    parser->longitud = 0;
    parser->buffer[0] = '\0';

    if (p[0] == '\0') return;

    ESP_LOGI(TAG, "[%s] Comando: %s", origen, p);
    ejecutar_comando(p, puerto, eco);
}

static void tick_parser(parser_t *parser, uart_port_t puerto,
                        bool eco, const char *origen)
{
    uint8_t byte;
    int leidos;

    while ((leidos = uart_read_bytes(puerto, &byte, 1, 0)) > 0) {
        char c = (char)byte;

        if (c == '\r' || c == '\n') {
            if (!parser->descartar) {
                procesar_buffer(parser, puerto, eco, origen);
            }
            parser->descartar = false;
            parser->longitud = 0;
            parser->buffer[0] = '\0';
            parser->ultimo_char_ms = ahora_ms();
            continue;
        }

        if (parser->descartar) {
            parser->ultimo_char_ms = ahora_ms();
            continue;
        }

        if (c == '\t') c = ' ';

        if (parser->longitud < TAM_BUFFER - 1) {
            parser->buffer[parser->longitud++] = (uint8_t)c;
        } else {
            parser->descartar = true;
            parser->longitud = 0;
            parser->buffer[0] = '\0';
        }
        parser->ultimo_char_ms = ahora_ms();
    }

    /* Procesar letra sola tras silencio (sin Enter) */
    if (!parser->descartar && parser->longitud > 0 &&
        (ahora_ms() - parser->ultimo_char_ms >= TIEMPO_SILENCIO_MS)) {
        procesar_buffer(parser, puerto, eco, origen);
    }
}

void comandos_procesar_serial(const char *origen)
{
    tick_parser(&parser_usb, UART_NUM_USB, false, origen);
}

void comandos_procesar_uart2(const char *origen, bool eco)
{
    tick_parser(&parser_bt, UART_NUM_BT, eco, origen);
}

void comandos_reset_uart2(void)
{
    parser_bt.longitud = 0;
    parser_bt.buffer[0] = '\0';
    parser_bt.descartar = false;
    parser_bt.ultimo_char_ms = 0;
}

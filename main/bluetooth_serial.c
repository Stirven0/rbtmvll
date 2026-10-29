#include "bluetooth_serial.h"
#include "comandos.h"
#include "config.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "BT_SERIAL";

static bool inicializado = false;

/* Puerto UART2 para el módulo HC-05 / HC-06 / HM-10 */
#define UART_NUM_BT UART_NUM_2
#define UART_BT_BAUD  BT_BAUDIOS
#define UART_BT_RX_BUF_SIZE 256
#define UART_BT_TX_BUF_SIZE 256

void bluetooth_serial_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BT_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_BT,
                                        UART_BT_RX_BUF_SIZE,
                                        UART_BT_TX_BUF_SIZE,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_BT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_BT,
                                 (int)PIN_BT_TX,
                                 (int)PIN_BT_RX,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    inicializado = true;

    ESP_LOGI(TAG, "Modulo serial listo: RX=GPIO%d TX=GPIO%d @ %lu baudios.",
             PIN_BT_RX, PIN_BT_TX, BT_BAUDIOS);
    ESP_LOGI(TAG, "Empareja el celular y envia: a/r/i/d/s/p");
    ESP_LOGI(TAG, "Ayuda: h | estado: m");
}

void bluetooth_serial_tick(void)
{
    if (!inicializado) {
        return;
    }

    /* Procesa comandos disponibles en el buffer de UART2 */
    comandos_procesar_uart2("BT", true);
}

void bluetooth_serial_descartar(void)
{
    /* Descarta todo lo que haya en el buffer de UART2 */
    uint8_t dato;
    while (uart_read_bytes(UART_NUM_BT, &dato, 1, 0) > 0) {
        /* descarta */
    }
    comandos_reset_uart2();
}

const char *bluetooth_serial_estado(void)
{
    static char estado[96];
    snprintf(estado, sizeof(estado),
             "modulo serial listo (RX=GPIO%u, TX=GPIO%u, %lu baudios)",
             PIN_BT_RX, PIN_BT_TX, BT_BAUDIOS);
    return estado;
}

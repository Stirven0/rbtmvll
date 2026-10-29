#include "oled.h"
#include "config.h"
#include "control_bt.h"
#include "modo.h"
#include "servo_ultrasonido.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

static const char *TAG = "OLED";

#define I2C_NUM_OLED I2C_NUM_0
#define OLED_ADDR    OLED_DIRECCION

/* Comandos SSD1306 */
#define CMD_DISPLAY_OFF          0xAE
#define CMD_DISPLAY_ON           0xAF
#define CMD_SET_DISPLAY_CLK_DIV  0xD5
#define CMD_SET_MULTIPLEX        0xA8
#define CMD_SET_DISPLAY_OFFSET   0xD3
#define CMD_SET_START_LINE       0x40
#define CMD_CHARGE_PUMP          0x8D
#define CMD_MEMORY_MODE          0x20
#define CMD_SEG_REMAP            0xA1
#define CMD_COM_SCAN_DIR         0xC8
#define CMD_SET_COMPINS          0xDA
#define CMD_SET_CONTRAST         0x81
#define CMD_SET_PRECHARGE        0xD9
#define CMD_SET_VCOM_DETECT      0xDB
#define CMD_DISPLAY_ALL_ON       0xA4
#define CMD_NORMAL_DISPLAY       0xA6
#define CMD_DEACTIVATE_SCROLL    0x2E
#define CMD_COLUMN_ADDR          0x21
#define CMD_PAGE_ADDR            0x22

static i2c_master_dev_handle_t oled_dev = NULL;
static bool disponible = false;
static bool forzar_refresco = true;
static uint32_t ultimo_refresco_ms = 0;
static uint32_t mostrar_datos_hasta_ms = 0;

static uint32_t ahora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* Envía un comando al SSD1306 */
static bool oled_enviar_comando(uint8_t cmd)
{
    uint8_t datos[2] = { 0x00, cmd };
    return i2c_master_transmit(oled_dev, datos, sizeof(datos), 100) == ESP_OK;
}

/* Envía datos al SSD1306 */
static bool oled_enviar_datos(const uint8_t *datos, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (!buf) return false;
    buf[0] = 0x40;  /* Co=0, D/C#=1 (datos) */
    memcpy(buf + 1, datos, len);
    bool ok = i2c_master_transmit(oled_dev, buf, len + 1, 100) == ESP_OK;
    free(buf);
    return ok;
}

static void oled_ini_display(void)
{
    oled_enviar_comando(CMD_DISPLAY_OFF);
    oled_enviar_comando(CMD_SET_DISPLAY_CLK_DIV);
    oled_enviar_comando(0x80);
    oled_enviar_comando(CMD_SET_MULTIPLEX);
    oled_enviar_comando(OLED_ALTO - 1);
    oled_enviar_comando(CMD_SET_DISPLAY_OFFSET);
    oled_enviar_comando(0x00);
    oled_enviar_comando(CMD_SET_START_LINE | 0x00);
    oled_enviar_comando(CMD_CHARGE_PUMP);
    oled_enviar_comando(0x14);
    oled_enviar_comando(CMD_MEMORY_MODE);
    oled_enviar_comando(0x00);
    oled_enviar_comando(CMD_SEG_REMAP | 0x1);
    oled_enviar_comando(CMD_COM_SCAN_DIR);
    oled_enviar_comando(CMD_SET_COMPINS);
    oled_enviar_comando(0x12);
    oled_enviar_comando(CMD_SET_CONTRAST);
    oled_enviar_comando(0xCF);
    oled_enviar_comando(CMD_SET_PRECHARGE);
    oled_enviar_comando(0xF1);
    oled_enviar_comando(CMD_SET_VCOM_DETECT);
    oled_enviar_comando(0x40);
    oled_enviar_comando(CMD_DISPLAY_ALL_ON);
    oled_enviar_comando(CMD_NORMAL_DISPLAY);
    oled_enviar_comando(CMD_DEACTIVATE_SCROLL);
    oled_enviar_comando(CMD_DISPLAY_ON);
}

static void oled_limpiar(void)
{
    static uint8_t linea[OLED_ANCHO];
    memset(linea, 0, sizeof(linea));
    for (uint8_t pagina = 0; pagina < OLED_ALTO / 8; pagina++) {
        oled_enviar_comando(CMD_PAGE_ADDR | pagina);
        oled_enviar_comando(CMD_COLUMN_ADDR & 0x0F);       /* columna baja */
        oled_enviar_comando(0x10 | ((CMD_COLUMN_ADDR >> 4) & 0x0F)); /* columna alta */
        oled_enviar_datos(linea, OLED_ANCHO);
    }
}

static void oled_imprimir_linea(uint8_t linea, const char *texto)
{
    /* Posicionar cursor */
    oled_enviar_comando(CMD_PAGE_ADDR | linea);
    oled_enviar_comando(0x00);   /* columna baja */
    oled_enviar_comando(0x10);   /* columna alta */

    /* Render de texto simple (5x7 pixels, ASCII 32-127) */
    static uint8_t fila_datos[OLED_ANCHO];
    memset(fila_datos, 0, sizeof(fila_datos));

    uint8_t col = 0;
    for (const char *p = texto; *p && col + 6 < OLED_ANCHO; p++) {
        uint8_t c = (uint8_t)(*p);
        if (c < 32) c = 32;
        c -= 32;

        /* Fuente 5x7 simplificada: cada carácter = 6 bytes de ancho (5 px + 1 espacio) */
        /* Usamos un bitmap muy reducido: solo mostramos el carácter como bloque */
        /* Para una implementación completa se necesita una tabla de fuentes */
        for (uint8_t i = 0; i < 5 && col < OLED_ANCHO; i++) {
            fila_datos[col++] = 0xFF;  /* placeholder: bloque sólido */
        }
        col++; /* espacio entre caracteres */
    }
    oled_enviar_datos(fila_datos, col);
}

void oled_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_OLED,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .device_address = OLED_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &oled_dev));

    oled_ini_display();
    oled_limpiar();

    disponible = true;
    forzar_refresco = true;

    ESP_LOGI(TAG, "SSD1306 %dx%d lista en 0x%02X (SDA=GPIO%d, SCL=GPIO%d).",
             OLED_ANCHO, OLED_ALTO, OLED_ADDR, PIN_OLED_SDA, PIN_OLED_SCL);
}

void oled_tick(const lecturas_ambientales_t lecturas)
{
    if (!disponible) return;

    uint32_t ahora = ahora_ms();
    if (!forzar_refresco &&
        (ahora - ultimo_refresco_ms < OLED_REFRESCO_MS)) {
        return;
    }
    ultimo_refresco_ms = ahora;
    forzar_refresco = false;

    oled_limpiar();

    if (modo_actual() == MODO_MANUAL) {
        oled_imprimir_linea(0, "MODO MANUAL");
        oled_imprimir_linea(1, "WiFi: OFF");
        char bt_line[40];
        snprintf(bt_line, sizeof(bt_line), "BT: %s",
                 control_bt_conectado() ? control_bt_nombre() : "SIN MANDO");
        oled_imprimir_linea(2, bt_line);
        oled_imprimir_linea(3, "B: datos   Y: auto");
        oled_imprimir_linea(4, "Servo centrado");
        oled_imprimir_linea(5, "USB: manual/auto");
    } else {
        oled_imprimir_linea(0, "MODO AUTOMATICO");
        char wifi_line[48];
        snprintf(wifi_line, sizeof(wifi_line), "WiFi: %s",
                 wifi_conectado() ? wifi_ip() : "conectando");
        oled_imprimir_linea(1, wifi_line);
        oled_imprimir_linea(2, "BT: OFF");

        char servo_line[48];
        uint8_t ang = servo_ultrasonido_angulo_actual();
        float dist = servo_ultrasonido_distancia_frente_cm();
        snprintf(servo_line, sizeof(servo_line),
                 "Servo: %u  Dist: %.0f cm", ang, dist);
        oled_imprimir_linea(3, servo_line);

        bool obst = servo_ultrasonido_obstaculo_frente();
        char obst_line[48];
        snprintf(obst_line, sizeof(obst_line),
                 "Obst: %s  Libre: %s",
                 obst ? "SI" : "NO",
                 servo_ultrasonido_direccion_mas_libre());
        oled_imprimir_linea(4, obst_line);

        char tele_line[48];
        snprintf(tele_line, sizeof(tele_line),
                 "Telegram: %lu s",
                 (unsigned long)INTERVALO_TELEMETRIA_MS / 1000UL);
        oled_imprimir_linea(5, tele_line);
    }
}

void oled_mostrar_datos(void)
{
    mostrar_datos_hasta_ms = ahora_ms() + OLED_TIEMPO_DATOS_MS;
    forzar_refresco = true;
}

void oled_mostrar_modo(modo_operacion_t modo)
{
    (void)modo;
    mostrar_datos_hasta_ms = 0;
    forzar_refresco = true;
}

bool oled_disponible(void) { return disponible; }

#include "telegram_bot.h"
#include "config.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "motores.h"
#include "control_bt.h"
#include "bluetooth_serial.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "TELEGRAM";

#define API_TELEGRAM_BASE "https://api.telegram.org/bot" BOT_TOKEN

static uint32_t ultima_consulta_ms = 0;
static int32_t ultimo_update_id = 0;

/* ------------------------------------------------------------------------
 *  Mini-parser JSON: suficiente para los campos de Telegram que usamos
 * ------------------------------------------------------------------------ */

/* Extrae el valor string de una clave "clave":"valor" (o clave:valor) */
static bool json_get_string(const char *json, const char *key,
                            char *out, size_t out_sz)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p) {
        /* probar sin comillas en el valor */
        snprintf(search, sizeof(search), "\"%s\":", key);
        p = strstr(json, search);
        if (!p) return false;
        p += strlen(search);
        /* saltar espacios */
        while (*p == ' ') p++;
        if (*p != '"') return false;
        p++;
    } else {
        p += strlen(search);
    }

    size_t i = 0;
    while (*p && *p != '"' && i < out_sz - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

/* Extrae un número entero de "clave":numero */
static bool json_get_int(const char *json, const char *key, int32_t *out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    char *end = NULL;
    long val = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (int32_t)val;
    return true;
}

/* Extrae el valor de una clave booleana */
/* Busca la posición de inicio de un array "clave":[ ... */
static const char *json_find_array(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":[", key);
    return strstr(json, search);
}

/* ------------------------------------------------------------------------
 *  Helper: envía POST JSON a un endpoint de Telegram
 * ------------------------------------------------------------------------ */
static bool telegram_post_json(const char *endpoint, const char *json_payload)
{
    if (!wifi_conectado()) {
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/%s", API_TELEGRAM_BASE, endpoint);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    bool ok = (err == ESP_OK &&
               esp_http_client_get_status_code(client) == 200);

    if (!ok) {
        ESP_LOGW(TAG, "Telegram POST fallo: err=%s codigo=%d",
                 esp_err_to_name(err), esp_http_client_get_status_code(client));
    }

    esp_http_client_cleanup(client);
    return ok;
}

/* ------------------------------------------------------------------------
 *  Helper: envía GET a un endpoint de Telegram (getUpdates)
 * ------------------------------------------------------------------------ */
static bool telegram_get_json(const char *endpoint, char *respuesta, size_t tam)
{
    if (!wifi_conectado()) {
        return false;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s/%s", API_TELEGRAM_BASE, endpoint);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    bool ok = (err == ESP_OK &&
               esp_http_client_get_status_code(client) == 200);

    if (ok) {
        int len = esp_http_client_read_response(client, respuesta, tam - 1);
        if (len > 0) {
            respuesta[len] = '\0';
        } else {
            respuesta[0] = '\0';
            ok = false;
        }
    } else {
        ESP_LOGW(TAG, "Telegram GET fallo: err=%s",
                 esp_err_to_name(err));
        respuesta[0] = '\0';
    }

    esp_http_client_cleanup(client);
    return ok;
}

/* ------------------------------------------------------------------------
 *  Formateo de reportes
 * ------------------------------------------------------------------------ */
static void formatear_uptime(char *buffer, size_t tam)
{
    uint32_t total_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    snprintf(buffer, tam, "%02lu:%02lu:%02lu",
             (unsigned long)(total_s / 3600UL),
             (unsigned long)((total_s / 60UL) % 60UL),
             (unsigned long)(total_s % 60UL));
}

static void reporte_estado(char *buffer, size_t tam,
                           const lecturas_ambientales_t *lec)
{
    char uptime[16];
    formatear_uptime(uptime, sizeof(uptime));

    int pos = 0;
    pos += snprintf(buffer + pos, tam - pos,
                    "📊 *IndoorWatch · Estado ambiental*\n\n");

    if (lec->ultima_actualizacion_ms == 0) {
        pos += snprintf(buffer + pos, tam - pos,
                        "⏳ Esperando la primera lectura de los sensores...\n\n");
    }

    if (lec->dht_valida) {
        pos += snprintf(buffer + pos, tam - pos,
                        "🌡️ Temperatura: %.1f °C\n💧 Humedad: %.1f %%\n",
                        lec->temperatura_c, lec->humedad_pct);
    } else {
        pos += snprintf(buffer + pos, tam - pos,
                        "🌡️ Temperatura: sin lectura válida\n"
                        "💧 Humedad: sin lectura válida\n");
    }

    pos += snprintf(buffer + pos, tam - pos,
                    "💨 Gas/humo: %d ADC %s\n",
                    lec->gas_adc,
                    lec->gas_adc > UMBRAL_GAS_ADC ? "⚠️ (elevado)" : "✅ (normal)");

    pos += snprintf(buffer + pos, tam - pos,
                    "🔥 Flama IR: %s\n",
                    lec->flama_detectada ? "🚨 FUEGO DETECTADO" : "✅ sin detección");

    pos += snprintf(buffer + pos, tam - pos,
                    "⏱️ Última lectura: hace %lu s\n"
                    "🚗 Motores: %s\n"
                    "🎮 Mando: %s\n"
                    "📡 BT Serial: %s\n"
                    "📶 Wi-Fi: %d dBm\n"
                    "🕒 Uptime: %s\n",
                    lec->ultima_actualizacion_ms ?
                        (unsigned long)((uint32_t)(esp_timer_get_time()/1000ULL) - lec->ultima_actualizacion_ms) / 1000UL : 0,
                    motores_estado_texto(),
                    control_bt_conectado() ? control_bt_nombre() : "desconectado",
                    bluetooth_serial_estado(),
                    wifi_rssi(), uptime);
}

static void enviar_ayuda(void)
{
    const char *msg =
        "👋 *IndoorWatch — Rover IoT*\n\n"
        "Monitoreo ambiental en interiores con ESP32.\n\n"
        "*Comandos disponibles:*\n"
        "/estado — Lecturas actuales\n"
        "/sensores — Igual que /estado\n"
        "/umbrales — Límites de alerta\n"
        "/ping — Estado del sistema\n"
        "/motores — Control de motores\n"
        "/demo — Prueba de motores\n"
        "/mando — Estado del mando GameSir\n"
        "/bluetooth — Respaldo por Bluetooth serial\n"
        "/id — ID de este chat\n"
        "/ayuda — Esta ayuda";
    telegram_enviar_mensaje(msg);
}

static void enviar_ayuda_motores(void)
{
    char msg[768];
    int pos = 0;
    pos += snprintf(msg + pos, sizeof(msg) - pos,
        "🚗 *Control de motores — L298N*\n\n"
        "/demo — Secuencia automática de prueba\n"
        "/adelante — Avanzar 1.5 s\n"
        "/atras — Retroceder 1.5 s\n"
        "/izquierda — Girar a la izquierda 1.5 s\n"
        "/derecha — Girar a la derecha 1.5 s\n"
        "/detener — Parar inmediatamente\n\n"
        "Los movimientos manuales duran 1.5 s y luego se detienen solos.\n\n"
        "🎮 Mando: stick izquierdo arriba/abajo, stick derecho izquierda/derecha; "
        "botón A = paro.\n"
        "📡 Sin mando, por USB/Bluetooth serial: a/r/i/d/s/p/m/h.\n"
        "a/r/i/d siguen hasta s o hasta 5 s sin una nueva orden.");
    telegram_enviar_mensaje(msg);
}

/* ------------------------------------------------------------------------
 *  Procesamiento de un mensaje entrante (JSON manual)
 * ------------------------------------------------------------------------ */
static void procesar_mensaje(const char *json_mensaje,
                             const lecturas_ambientales_t *lec)
{
    /* Extraer chat.id */
    char chat_id_str[32] = { 0 };
    if (!json_get_string(json_mensaje, "id", chat_id_str, sizeof(chat_id_str))) {
        /* buscar dentro de "chat":{..."id":...} */
        const char *chat_obj = strstr(json_mensaje, "\"chat\":{");
        if (!chat_obj) return;
        if (!json_get_string(chat_obj, "id", chat_id_str, sizeof(chat_id_str)))
            return;
    }

    /* Seguridad: solo el chat configurado */
    if (strcmp(chat_id_str, CHAT_ID) != 0) {
        ESP_LOGW(TAG, "Mensaje ignorado de chat no autorizado: %s", chat_id_str);
        return;
    }

    /* Extraer text */
    char texto[128] = { 0 };
    if (!json_get_string(json_mensaje, "text", texto, sizeof(texto))) {
        return;
    }
    if (texto[0] == '\0') return;

    /* Normalizar: quitar @botname, lower case */
    char comando[64];
    strncpy(comando, texto, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    char *arroba = strchr(comando, '@');
    if (arroba) *arroba = '\0';
    for (char *p = comando; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* trim */
    while (*comando == ' ') memmove(comando, comando + 1, strlen(comando));

    ESP_LOGI(TAG, "Telegram comando: %s", comando);

    if (strcmp(comando, "/start") == 0 || strcmp(comando, "/ayuda") == 0 ||
        strcmp(comando, "/help") == 0) {
        enviar_ayuda();
    } else if (strcmp(comando, "/estado") == 0 ||
               strcmp(comando, "/sensores") == 0) {
        char reporte[768];
        reporte_estado(reporte, sizeof(reporte), lec);
        telegram_enviar_mensaje(reporte);
    } else if (strcmp(comando, "/umbrales") == 0) {
        char info[256];
        snprintf(info, sizeof(info),
                 "⚙️ *Umbrales de alerta*\n\n"
                 "🌡️ Temperatura: > %.1f °C\n"
                 "💨 Gas/humo: > %d ADC\n"
                 "🔥 Flama: activa en LOW\n"
                 "⏳ Cooldown de alertas: %lu s\n"
                 "📊 Lectura de sensores: cada %.1f s",
                 UMBRAL_TEMP_C, UMBRAL_GAS_ADC,
                 (unsigned long)COOLDOWN_ALERTA_MS / 1000UL,
                 INTERVALO_SENSORES_MS / 1000.0f);
        telegram_enviar_mensaje(info);
    } else if (strcmp(comando, "/ping") == 0) {
        char uptime[16];
        formatear_uptime(uptime, sizeof(uptime));
        char pong[256];
        snprintf(pong, sizeof(pong),
                 "🏓 *Pong*\n\n🕒 Uptime: %s\n"
                 "📶 Wi-Fi: %d dBm\n🌐 IP: %s\n"
                 "🧠 Heap libre: %lu bytes",
                 uptime, wifi_rssi(), wifi_ip(),
                 (unsigned long)esp_get_free_heap_size());
        telegram_enviar_mensaje(pong);
    } else if (strcmp(comando, "/id") == 0) {
        char id_msg[128];
        snprintf(id_msg, sizeof(id_msg), "🆔 Chat ID autorizado: %s", CHAT_ID);
        telegram_enviar_mensaje(id_msg);
    } else if (strcmp(comando, "/mando") == 0) {
        if (control_bt_conectado()) {
            char msg[128];
            snprintf(msg, sizeof(msg), "🎮 Mando conectado: %s", control_bt_nombre());
            telegram_enviar_mensaje(msg);
        } else {
            telegram_enviar_mensaje(
                "🎮 Mando desconectado. Enciéndelo en modo Android (Home + A) "
                "o Switch (Home + Y) y espera el parpadeo rápido.");
        }
    } else if (strcmp(comando, "/bluetooth") == 0) {
        const char *info =
            "📡 *Bluetooth serial*\n\n"
            "Módulo: HC-05 / HC-06 / HM-10 (UART).\n"
            "Cableado: TX del módulo → GPIO16, RX → GPIO17, GND común.\n"
            "Baudios: 9600.\n\n"
            "1. Empareja el celular con el módulo (PIN 1234 o 0000).\n"
            "2. Abre una app de terminal Bluetooth.\n"
            "3. Envía una sola letra: a/r/i/d/s/p.\n\n"
            "a=adelante, r=atrás, i=izquierda, d=derecha, "
            "s=detener, p=demo, m=estado, h=ayuda.";
        telegram_enviar_mensaje(info);
    } else if (strcmp(comando, "/motores") == 0 ||
               strcmp(comando, "/control") == 0) {
        enviar_ayuda_motores();
    } else if (strcmp(comando, "/demo") == 0 ||
               strcmp(comando, "/prueba") == 0) {
        telegram_enviar_mensaje(
            "🚗 Demo iniciada: adelante, atrás, izquierda y derecha.");
        motores_demo();
    } else if (strcmp(comando, "/adelante") == 0) {
        telegram_enviar_mensaje("⬆️ Adelante durante 1.5 s.");
        motores_mover(DIRECCION_ADELANTE, DURACION_MOVIMIENTO_MANUAL_MS);
    } else if (strcmp(comando, "/atras") == 0) {
        telegram_enviar_mensaje("⬇️ Atrás durante 1.5 s.");
        motores_mover(DIRECCION_ATRAS, DURACION_MOVIMIENTO_MANUAL_MS);
    } else if (strcmp(comando, "/izquierda") == 0) {
        telegram_enviar_mensaje("⬅️ Izquierda durante 1.5 s.");
        motores_mover(DIRECCION_IZQUIERDA, DURACION_MOVIMIENTO_MANUAL_MS);
    } else if (strcmp(comando, "/derecha") == 0) {
        telegram_enviar_mensaje("➡️ Derecha durante 1.5 s.");
        motores_mover(DIRECCION_DERECHA, DURACION_MOVIMIENTO_MANUAL_MS);
    } else if (strcmp(comando, "/detener") == 0 ||
               strcmp(comando, "/stop") == 0 ||
               strcmp(comando, "/frenar") == 0) {
        motores_detener();
        telegram_enviar_mensaje("🛑 Motores detenidos.");
    } else if (comando[0] == '/') {
        telegram_enviar_mensaje(
            "❓ Comando no reconocido. Usa /ayuda para ver las opciones.");
    }
}

void telegram_init(void)
{
    /* Prototipo: no se valida la cadena de certificados de Telegram.
     * Para producción, fijar el certificado raíz correspondiente. */
    ESP_LOGI(TAG, "Cliente Telegram listo (modo prototipo).");
    ultima_consulta_ms = 0;
    ultimo_update_id = 0;
}

bool telegram_enviar_mensaje(const char *texto)
{
    /* Construir JSON manualmente: {"chat_id":"...","text":"...","parse_mode":"Markdown"} */
    char json[512];
    int n = snprintf(json, sizeof(json),
                     "{\"chat_id\":\"%s\",\"text\":\"%s\",\"parse_mode\":\"Markdown\"}",
                     CHAT_ID, texto);
    if (n < 0 || (size_t)n >= sizeof(json)) {
        ESP_LOGE(TAG, "JSON de salida demasiado largo");
        return false;
    }

    return telegram_post_json("sendMessage", json);
}

void telegram_tick(const lecturas_ambientales_t lecturas)
{
    if (!wifi_conectado()) {
        return;
    }

    uint32_t ahora = (uint32_t)(esp_timer_get_time() / 1000ULL);

    /* Polling no bloqueante de comandos (un mensaje por consulta) */
    if (ahora - ultima_consulta_ms < INTERVALO_TELEGRAM_MS) {
        return;
    }
    ultima_consulta_ms = ahora;

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint),
             "getUpdates?offset=%ld&limit=1&timeout=0",
             (long)(ultimo_update_id + 1));

    char respuesta[2048];
    if (!telegram_get_json(endpoint, respuesta, sizeof(respuesta))) {
        return;
    }

    /* Encontrar el array "result" */
    const char *result_arr = json_find_array(respuesta, "result");
    if (!result_arr) return;

    /* El array comienza después de "result":[
    result_arr apunta a la "r" de "result", necesitamos saltar al '[' */
    const char *bracket = strchr(result_arr, '[');
    if (!bracket) return;
    bracket++; /* ahora apunta al inicio del array o a ']' si vacío */

    if (*bracket == ']') {
        /* Array vacío, no hay mensajes */
        return;
    }

    /* Procesar cada objeto del array (solo procesamos el primero por simplicidad,
       pero iteramos por todos los objetos { } separados por comas) */
    const char *p = bracket;
    while (*p) {
        /* Saltar espacios y comas */
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r') p++;
        if (*p == ']' || *p == '\0') break;
        if (*p != '{') break;

        /* Encontrar el cierre del objeto */
        int depth = 0;
        const char *obj_start = p;
        const char *obj_end = NULL;
        while (*p) {
            if (*p == '{') depth++;
            else if (*p == '}') {
                depth--;
                if (depth == 0) { obj_end = p + 1; break; }
            }
            p++;
        }
        if (!obj_end) break;

        size_t obj_len = obj_end - obj_start;
        if (obj_len > 0 && obj_len < 1024) {
            char obj_buf[1024];
            memcpy(obj_buf, obj_start, obj_len);
            obj_buf[obj_len] = '\0';

            /* Extraer update_id */
            int32_t uid = 0;
            if (json_get_int(obj_buf, "update_id", &uid)) {
                if (uid > ultimo_update_id) {
                    ultimo_update_id = uid;
                }
            }

            /* Buscar el objeto "message" dentro de este update */
            const char *msg_obj = strstr(obj_buf, "\"message\":{");
            if (msg_obj) {
                /* Extraer hasta el cierre del message */
                int depth = 1;
                const char *m = msg_obj + strlen("\"message\":");
                const char *msg_end = NULL;
                while (*m) {
                    if (*m == '{') depth++;
                    else if (*m == '}') {
                        depth--;
                        if (depth == 0) { msg_end = m + 1; break; }
                    }
                    m++;
                }
                if (msg_end) {
                    size_t msg_len = msg_end - msg_obj;
                    if (msg_len < 1024) {
                        char msg_buf[1024];
                        memcpy(msg_buf, msg_obj, msg_len);
                        msg_buf[msg_len] = '\0';
                        procesar_mensaje(msg_buf, &lecturas);
                    }
                }
            }
        }
        p = obj_end;
    }
}

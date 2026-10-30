#include "motores.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "MOTORES";

static direccion_movimiento_t direccion_actual = DIRECCION_DETENIDO;
static uint32_t fin_movimiento_ms = 0;
static uint32_t fin_control_continuo_ms = 0;
static bool movimiento_temporal_activo = false;
static bool control_continuo_activo = false;
static bool demo_activa = false;
static uint8_t paso_demo = 0;
static uint32_t fin_paso_demo_ms = 0;

typedef struct {
    direccion_movimiento_t direccion;
    uint32_t duracion_ms;
} paso_demo_t;

static const paso_demo_t secuencia_demo[] = {
    { DIRECCION_ADELANTE,     1200 },
    { DIRECCION_DETENIDO,      400 },
    { DIRECCION_ATRAS,        1200 },
    { DIRECCION_DETENIDO,      400 },
    { DIRECCION_IZQUIERDA,     900 },
    { DIRECCION_DETENIDO,      400 },
    { DIRECCION_DERECHA,       900 },
    { DIRECCION_DETENIDO,      600 },
};

static bool tiempo_vencido(uint32_t ahora, uint32_t objetivo)
{
    /* Funciona incluso con overflow de millis/esp_timer */
    return (int32_t)(ahora - objetivo) >= 0;
}

static uint32_t ahora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void aplicar_motor(uint8_t pin_avance, uint8_t pin_retroceso,
                          bool avance, bool invertido)
{
    (void)invertido;  /* false / false eliminados: los motores están cableados correctamente */
    gpio_set_level((gpio_num_t)pin_avance,   avance ? 1 : 0);
    gpio_set_level((gpio_num_t)pin_retroceso, avance ? 0 : 1);
}

static void aplicar_direccion(direccion_movimiento_t direccion)
{
    switch (direccion) {
    case DIRECCION_ADELANTE:
        aplicar_motor(PIN_L298N_IN1, PIN_L298N_IN2, true,  false);
        aplicar_motor(PIN_L298N_IN3, PIN_L298N_IN4, true,  false);
        break;
    case DIRECCION_ATRAS:
        aplicar_motor(PIN_L298N_IN1, PIN_L298N_IN2, false, false);
        aplicar_motor(PIN_L298N_IN3, PIN_L298N_IN4, false, false);
        break;
    case DIRECCION_IZQUIERDA:
        /* Giro sobre el centro: motor A atrás, motor B adelante */
        aplicar_motor(PIN_L298N_IN1, PIN_L298N_IN2, false, false);
        aplicar_motor(PIN_L298N_IN3, PIN_L298N_IN4, true,  false);
        break;
    case DIRECCION_DERECHA:
        /* Giro sobre el centro: motor A adelante, motor B atrás */
        aplicar_motor(PIN_L298N_IN1, PIN_L298N_IN2, true,  false);
        aplicar_motor(PIN_L298N_IN3, PIN_L298N_IN4, false, false);
        break;
    case DIRECCION_DETENIDO:
    default:
        gpio_set_level((gpio_num_t)PIN_L298N_IN1, 0);
        gpio_set_level((gpio_num_t)PIN_L298N_IN2, 0);
        gpio_set_level((gpio_num_t)PIN_L298N_IN3, 0);
        gpio_set_level((gpio_num_t)PIN_L298N_IN4, 0);
        break;
    }
    direccion_actual = direccion;
}

static void iniciar_paso_demo(void)
{
    const paso_demo_t *paso = &secuencia_demo[paso_demo];
    aplicar_direccion(paso->direccion);
    fin_paso_demo_ms = ahora_ms() + paso->duracion_ms;
    ESP_LOGI(TAG, "Demo paso %u/%u: %s durante %lu ms",
             paso_demo + 1, (unsigned)(sizeof(secuencia_demo) / sizeof(secuencia_demo[0])),
             motores_estado_texto(), paso->duracion_ms);
}

void motores_init(void)
{
    gpio_set_direction((gpio_num_t)PIN_L298N_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_L298N_IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_L298N_IN3, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)PIN_L298N_IN4, GPIO_MODE_OUTPUT);

    /* ENA con jumper (velocidad fija). ENB no se usa: la placa tiene jumper. */
    aplicar_direccion(DIRECCION_DETENIDO);

    ESP_LOGI(TAG, "L298N listo: IN1=%d IN2=%d IN3=%d IN4=%d ENA=%d "
                  "(ENB con jumper en placa).",
             PIN_L298N_IN1, PIN_L298N_IN2, PIN_L298N_IN3, PIN_L298N_IN4,
             PIN_L298N_ENA);
    ESP_LOGI(TAG, "USB: a=adelante r=atras i=izquierda d=derecha "
                  "s=detener p=demo");
}

void motores_mover(direccion_movimiento_t direccion, uint32_t duracion_ms)
{
    demo_activa = false;
    control_continuo_activo = false;
    fin_control_continuo_ms = 0;

    if (direccion == DIRECCION_DETENIDO) {
        motores_detener();
        return;
    }

    aplicar_direccion(direccion);
    movimiento_temporal_activo = true;
    fin_movimiento_ms = ahora_ms() + duracion_ms;

    ESP_LOGI(TAG, "%s durante %lu ms", motores_estado_texto(), duracion_ms);
}

void motores_control_continuo(direccion_movimiento_t direccion)
{
    if (direccion == DIRECCION_DETENIDO) {
        motores_detener();
        return;
    }

    demo_activa = false;
    movimiento_temporal_activo = false;
    control_continuo_activo = true;

    /* Cada orden refresca el paro de seguridad */
    fin_control_continuo_ms = ahora_ms() + DURACION_CONTROL_CONTINUO_MS;

    if (direccion == direccion_actual) {
        return;
    }

    aplicar_direccion(direccion);
    ESP_LOGI(TAG, "Control continuo: %s", motores_estado_texto());
}

void motores_detener(void)
{
    bool estaba_activo = demo_activa || movimiento_temporal_activo ||
                         control_continuo_activo ||
                         direccion_actual != DIRECCION_DETENIDO;

    demo_activa = false;
    movimiento_temporal_activo = false;
    control_continuo_activo = false;
    fin_control_continuo_ms = 0;
    aplicar_direccion(DIRECCION_DETENIDO);

    if (estaba_activo) {
        ESP_LOGI(TAG, "Detenido.");
    }
}

void motores_demo(void)
{
    demo_activa = true;
    movimiento_temporal_activo = false;
    control_continuo_activo = false;
    fin_control_continuo_ms = 0;
    paso_demo = 0;
    ESP_LOGI(TAG, "Iniciando secuencia de demostracion.");
    iniciar_paso_demo();
}

void motores_tick(void)
{
    uint32_t ahora = ahora_ms();

    if (demo_activa) {
        if (!tiempo_vencido(ahora, fin_paso_demo_ms)) {
            return;
        }
        ++paso_demo;
        if (paso_demo >= (uint8_t)(sizeof(secuencia_demo) / sizeof(secuencia_demo[0]))) {
            demo_activa = false;
            aplicar_direccion(DIRECCION_DETENIDO);
            ESP_LOGI(TAG, "Demostracion terminada.");
            return;
        }
        iniciar_paso_demo();
        return;
    }

    if (control_continuo_activo &&
        tiempo_vencido(ahora, fin_control_continuo_ms)) {
        control_continuo_activo = false;
        fin_control_continuo_ms = 0;
        aplicar_direccion(DIRECCION_DETENIDO);
        ESP_LOGI(TAG, "Sin ordenes de control continuo; detenido.");
        return;
    }

    if (movimiento_temporal_activo && tiempo_vencido(ahora, fin_movimiento_ms)) {
        movimiento_temporal_activo = false;
        aplicar_direccion(DIRECCION_DETENIDO);
        ESP_LOGI(TAG, "Movimiento terminado.");
    }
}

bool motores_demo_activa(void)
{
    return demo_activa;
}

const char *motores_estado_texto(void)
{
    switch (direccion_actual) {
    case DIRECCION_ADELANTE:   return "ADELANTE";
    case DIRECCION_ATRAS:      return "ATRAS";
    case DIRECCION_IZQUIERDA:  return "IZQUIERDA";
    case DIRECCION_DERECHA:    return "DERECHA";
    case DIRECCION_DETENIDO:
    default:                   return "DETENIDO";
    }
}

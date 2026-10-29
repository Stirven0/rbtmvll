# IndoorWatch — Rover IoT de Monitoreo Ambiental (ESP-IDF)

Migración del firmware de Arduino IDE → ESP-IDF para **ESP32 original (esp32)** en placa **ESP-WROOM-32**.

## Hardware

| Componente | Pin ESP32 | Notas |
|------------|-----------|-------|
| DHT22 (temp/hum) | GPIO 13 | Bit-banging GPIO |
| MQ-135 / MQ-2 gas | GPIO 35 | ADC1_CHANNEL_7, 12 bits, 11 dB |
| Sensor flama IR | GPIO 34 | ADC1_CHANNEL_6, entrada digital, LOW = fuego |
| Batería (divisor 20k/10k) | GPIO 36 | ADC1_CHANNEL_0 |
| HC-SR04 TRIG | GPIO 32 | |
| HC-SR04 ECHO | GPIO 33 | Divisor 1 kΩ / 2 kΩ |
| Servo SG90 | GPIO 26 | LEDC PWM, 5 V externo |
| L298N IN1..IN4 | GPIO 18, 19, 22, 23 | Velocidad fija (ENA jumper) |
| L298N ENB | — | No usado (jumper en placa) |
| Mando Bluetooth clásico | BT clásico ESP32 | GameSir (stack BT clásico ESP-IDF, stub) |

## Estructura del proyecto (ESP-IDF)

```
rbtmvll/
├── CMakeLists.txt            # Proyecto ESP-IDF raíz
├── sdkconfig.defaults        # Configuración reproducible
├── partitions.csv            # Tabla de particiones (sin OTA, 4 MB flash)
├── build.sh                  # Wrapper de build/flash/monitor
├── .gitignore
├── README.md
├── main/
│   ├── CMakeLists.txt        # Registro de componentes y dependencias
│   ├── app_main.c            # Punto de entrada + tareas FreeRTOS
│   ├── config.h / config.c   # Pines, umbrales, constantes
│   ├── sensores.*            # DHT22, MQ, flama (bit-banging + ADC)
│   ├── motores.*             # L298N driver H
│   ├── control_bt.*          # Mando Bluetooth clásico (stack BT ESP-IDF)
│   ├── wifi_manager.*        # Wi-Fi STA + reconexión
│   ├── telegram_bot.*        # Telegram Bot API (HTTP)
│   ├── alertas.*             # Umbrales temp/gas/flama + Telegram
│   ├── modo.*                # AUTO / MANUAL
│   ├── comandos.*            # Parser de comandos serial (USB)
│   ├── servo_ultrasonido.*   # SG90 + HC-SR04 (LEDC + GPIO)
│   └── secrets.example.h     # Plantilla de credenciales
```

## Prerrequisitos

1. Instalar ESP-IDF (versión 6.x probada con este proyecto):
   ```bash
   # Ejemplo usando ESP-IDF Manager (EIM)
   ~/.espressif/tools/activate_idf_v6.1.sh
   ```

2. Cargar el entorno ESP-IDF en cada terminal:
   ```bash
   source $HOME/.espressif/tools/activate_idf_v6.1.sh
   ```

3. Verificar que `idf.py` funciona:
   ```bash
   idf.py --version
   ```

## Credenciales

Copia la plantilla y completa con tus valores reales:

```bash
cp main/secrets.example.h main/secrets.h
# Edita main/secrets.h con tu SSID, contraseña Wi-Fi, token de Telegram y chat ID
```

**Importante:** `main/secrets.h` está en `.gitignore` y nunca debe subirse al repositorio.

## Construcción

Desde el directorio del proyecto:

```bash
# Cargar entorno ESP-IDF
. $HOME/esp/esp-idf/export.sh

# Construir
./build.sh build

# Limpieza completa + reconstruir
./build.sh fullclean

# Flashear (te pedirá el puerto serial)
./build.sh flash

# Monitor serie
./build.sh monitor

# Flashear + monitor
./build.sh flash-monitor

# Configuración interactiva
./build.sh menuconfig
```

## Arquitectura de tareas FreeRTOS

Cada módulo del loop original de Arduino se ejecuta como una tarea independiente:

| Tarea | Prioridad | Función |
|-------|-----------|---------|
| sensores | 5 | Lectura DHT22, MQ, flama cada 2.5 s |
| motores | 6 | Control L298N (tick cada 10 ms) |
| control_bt | 5 | Mando Bluetooth clásico (stack BT) |
| wifi_telegram | 5 | Wi-Fi + polling de Telegram |
| alertas | 5 | Evaluación de umbrales + envío Telegram |
| servo_sonar | 5 | Servo SG90 + ultrasonido HC-SR04 |
| comandos_usb | 5 | Comandos por USB/UART0 |

Los datos de sensores se comparten mediante un mutex (`xSensoresMutex`).

## Estado de la migración

- ✅ Estructura de proyecto ESP-IDF (CMake, particiones, sdkconfig)
- ✅ Sensores: DHT22 (bit-banging GPIO), MQ (ADC1 GPIO 35), flama (GPIO 34)
- ✅ Motores: L298N control continuo/temporal/demo (ENB ignorado, jumper en placa)
- ✅ Wi-Fi: STA + reconexión no bloqueante
- ✅ Telegram: Bot API vía HTTP
- ✅ Alertas: temp/gas/flama con cooldown
- ✅ Modos: AUTO / MANUAL
- ✅ Comandos serial: parser USB/UART0
- ✅ Servo + ultrasonido: SG90 (LEDC GPIO 26) + HC-SR04 (GPIO)
- ⚠️ Control Bluetooth clásico (mando GameSir): stack BT inicializado como stub. La integración completa del gamepad requiere stack HID/SPP adicional sobre el componente `bt` de ESP-IDF v6.1.
- ✅ Compilación verificada con ESP-IDF v6.1 (target `esp32`)

## Pruebas por módulo

Se incluye un menú de pruebas interactivo para validar cada módulo por separado:

### Archivos de prueba
- `main/test_menu.c` — Menú interactivo por serial (USB/UART0, 115200 baudios)
- `main/app_main_test.c` — Punto de entrada para el menú de pruebas
- `build_test.sh` — Cambia al menú de pruebas y compila
- `build_firmware.sh` — Vuelve al firmware normal y compila

### Cómo usar las pruebas

```bash
# 1. Activar entorno ESP-IDF
source $HOME/.espressif/tools/activate_idf_v6.1.sh

# 2. Cambiar al menú de pruebas y compilar
./build_test.sh

# 3. Flashear al ESP32
idf.py flash

# 4. Abrir el monitor para ver resultados y seleccionar pruebas
idf.py monitor

# 5. En el menú, elegir el módulo a probar:
#    1 - DHT22 (GPIO 13)
#    2 - Sensor Flama IR (GPIO 34)
#    3 - Sensor MQ-135 (GPIO 35)
#    4 - Ultrasonido HC-SR04 (TRIG 32 / ECHO 33)
#    5 - Puente H L298N (motores)
#    6 - Conectividad Wi-Fi
#    7 - Bluetooth - Mando GameSir
#    0 - Salir (reiniciar)

# 6. Volver al firmware normal
./build_firmware.sh
```

### Orden sugerido de pruebas

1. **DHT22** — Verifica lectura de temperatura y humedad cada 2 s
2. **Flama IR** — Verifica detección de llama/fuego (sensor en LOW = detectado)
3. **MQ-135** — Verifica niveles de gas/humo (umbral: 2700 ADC)
4. **HC-SR04** — Verifica distancia en 3 posiciones (izquierda/frente/derecha)
5. **L298N** — Secuencia: adelante, atrás, izquierda, derecha, detener
6. **Wi-Fi** — Verifica conexión a red y obtención de IP
7. **Bluetooth** — Verifica emparejamiento con mando GameSir

## Notas de producción

- Sin OTA, sin PSRAM, sin Secure Boot (configuración actual).
- Para producción: agregar tabla de particiones OTA, habilitar Secure Boot v2 y flash encryption, revisar tamaños de pila de tareas FreeRTOS, y validar el certificado raíz de Telegram en `esp_http_client`.

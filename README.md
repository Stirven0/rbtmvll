# IndoorWatch — Rover IoT de Monitoreo Ambiental (ESP-IDF)

Migración del firmware de Arduino IDE → ESP-IDF para **ESP32 original (esp32)** en placa **ESP-WROOM-32**.

## Hardware

| Componente | Pin ESP32 | Notas |
|------------|-----------|-------|
| DHT22 (temp/hum) | GPIO 13 | Bit-banging GPIO |
| MQ-135 / MQ-2 gas | GPIO 35 | ADC1_CHANNEL_7, 12 bits, 11 dB |
| Sensor flama IR | GPIO 34 | ADC1_CHANNEL_6, analógico (umbral calibrable) |
| HC-SR04 TRIG | GPIO 32 | Ultrasonido |
| HC-SR04 ECHO | GPIO 33 | Divisor 1 kΩ / 2 kΩ |
| L298N IN1..IN4 | GPIO 18, 19, 22, 23 | Driver H, velocidad fija (ENA jumper) |
| L298N ENA | GPIO 25 | Con jumper (velocidad fija) |
| L298N ENB | — | No usado (jumper en placa) |
| Mando Bluetooth clásico | BT clásico ESP32 | GameSir (stack BT clásico ESP-IDF, stub) |

**No se usa:** Servo SG90, monitoreo de batería (GPIO 36), OLED SSD1306, Bluetooth serial HC-05.

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
- ✅ Sensores: DHT22 (bit-banging GPIO), MQ (ADC1 GPIO 35), flama (ADC1 GPIO 34)
- ✅ Motores: L298N control continuo/temporal/demo (ENB ignorado, jumper en placa)
- ✅ Wi-Fi: STA + reconexión no bloqueante
- ✅ Telegram: Bot API vía HTTP
- ✅ Alertas: temp/gas/flama con cooldown
- ✅ Modos: AUTO / MANUAL
- ✅ Comandos serial: parser USB/UART0
- ✅ Ultrasonido: HC-SR04 (GPIO 32/33)
- ⚠️ Control Bluetooth clásico (mando GameSir): stack BT inicializado como stub. La integración completa del gamepad requiere stack HID/SPP adicional sobre el componente `bt` de ESP-IDF v6.1.
- ✅ Compilación verificada con ESP-IDF v6.1 (target `esp32`)

## Pruebas por módulo (sub-proyectos independientes)

Cada módulo se puede probar de forma independiente como un sub-proyecto ESP-IDF separado bajo `tests/`. Cada sub-proyecto tiene su propio `CMakeLists.txt` y se compila y flashea por separado.

### Estructura

```
tests/
├── test_dht22/        # DHT22 - GPIO 13 (temp/hum)
├── test_flama/        # Sensor Flama IR - GPIO 34
├── test_mq/           # MQ-135 - GPIO 35 (gas/humo)
├── test_ultrasonido/  # HC-SR04 (TRIG 32, ECHO 33)
├── test_l298n/        # Puente H L298N - IN: 18,19,22,23 / ENA: 25
├── test_wifi/         # Conectividad Wi-Fi STA
└── test_bluetooth/    # Bluetooth Clásico - Mando GameSir
```

### Cómo ejecutar cada prueba

```bash
# 1. Activar entorno ESP-IDF
source $HOME/.espressif/tools/activate_idf_v6.1.sh

# 2. Entrar al directorio del test deseado
cd tests/test_dht22

# 3. Compilar
idf.py build

# 4. Flashear (te pedirá el puerto serial, ej. /dev/ttyUSB0)
idf.py flash

# 5. Ver resultados por serial
idf.py monitor

# 6. Volver al directorio raíz cuando termines
cd ../..
```

### Orden sugerido de pruebas

| # | Test | Módulo | Pines | Qué verifica |
|---|------|--------|-------|--------------|
| 1 | `test_dht22` | DHT22 | GPIO 13 | Temperatura y humedad cada 2 s |
| 2 | `test_flama` | Flama IR | GPIO 34 | Detección de fuego (LOW = detectado) |
| 3 | `test_mq` | MQ-135 | GPIO 35 | Niveles de gas cada 1 s (umbral 2700 ADC) |
| 4 | `test_ultrasonido` | HC-SR04 | TRIG 32, ECHO 33 | Distancia cada ~500 ms |
| 5 | `test_l298n` | L298N Motores | IN: 18,19,22,23 / ENA: 25 | Secuencia: adelante, atrás, izq, der, detener |
| 6 | `test_wifi` | Wi-Fi | — | Conexión a red y obtención de IP |
| 7 | `test_bluetooth` | BT Clásico | ESP32 BT | Emparejamiento con mando GameSir |

### Notas específicas

**test_wifi:** Antes de compilar, edita `tests/test_wifi/main/app_main.c` y coloca tu SSID y contraseña Wi-Fi reales en `WIFI_SSID` y `WIFI_PASSWORD`.

**test_bluetooth:** Inicializa el stack BT clásico de ESP-IDF y muestra la dirección BD local. Espera conexión de mando GameSir.

**test_ultrasonido:** Mide distancia cada 500 ms mediante el pulso TRIG y la lectura del eco en ECHO.

## Notas de producción

- Sin OTA, sin PSRAM, sin Secure Boot (configuración actual).
- Para producción: agregar tabla de particiones OTA, habilitar Secure Boot v2 y flash encryption, revisar tamaños de pila de tareas FreeRTOS, y validar el certificado raíz de Telegram en `esp_http_client`.

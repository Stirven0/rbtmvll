# IndoorWatch — Rover IoT de Monitoreo Ambiental (ESP-IDF)

Migración del firmware de Arduino IDE → ESP-IDF para **ESP32 original (esp32)** en placa **ESP-WROOM-32**.

## Hardware

| Componente | Pin ESP32 | Notas |
|------------|-----------|-------|
| DHT22 (temp/hum) | GPIO 13 | Bit-banging GPIO |
| MQ-135 / MQ-2 gas | GPIO 34 | ADC1_CHANNEL_6, 12 bits, 11 dB |
| Sensor flama IR | GPIO 35 | Entrada digital, LOW = fuego |
| Batería (divisor 20k/10k) | GPIO 36 | ADC1_CHANNEL_0 |
| HC-SR04 TRIG | GPIO 32 | |
| HC-SR04 ECHO | GPIO 33 | Divisor 1 kΩ / 2 kΩ |
| Servo SG90 | GPIO 27 | LEDC PWM, 5 V externo |
| L298N IN1..IN4 | GPIO 18, 19, 22, 23 | Velocidad fija (ENA/ENB jumper) |
| L298N ENA/ENB | GPIO 25, 26 | Jumper puestos |
| BT Serial HC-05/06 | UART2: RX=16, TX=17 | 9600 baudios |
| OLED SSD1306 I2C | SDA=21, SCL=4 | 0x3C, 128×64 |
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
│   ├── bluetooth_serial.*    # HC-05 por UART2
│   ├── control_bt.*          # Mando Bluetooth clásico (stack BT ESP-IDF)
│   ├── wifi_manager.*        # Wi-Fi STA + reconexión
│   ├── telegram_bot.*        # Telegram Bot API (HTTP + cJSON)
│   ├── alertas.*             # Umbrales temp/gas/flama + Telegram
│   ├── modo.*                # AUTO / MANUAL
│   ├── comandos.*            # Parser de comandos serial (USB + BT)
│   ├── oled.*                # SSD1306 por I2C master
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
| bt_serial | 5 | Comandos por UART2 (HC-05) |
| control_bt | 5 | Mando Bluetooth clásico (stack BT) |
| wifi_telegram | 5 | Wi-Fi + polling de Telegram |
| alertas | 5 | Evaluación de umbrales + envío Telegram |
| oled | 4 | Refresco pantalla SSD1306 cada 500 ms |
| servo_sonar | 5 | Servo SG90 + ultrasonido HC-SR04 |
| comandos_usb | 5 | Comandos por USB/UART0 |

Los datos de sensores se comparten mediante un mutex (`xSensoresMutex`).

## Estado de la migración

- ✅ Estructura de proyecto ESP-IDF (CMake, particiones, sdkconfig)
- ✅ Sensores: DHT22 (bit-banging), MQ (ADC1), flama (GPIO)
- ✅ Motores: L298N control continuo/temporal/demo
- ✅ Bluetooth serial: HC-05 por UART2
- ✅ Wi-Fi: STA + reconexión no bloqueante
- ✅ Telegram: Bot API vía HTTP + cJSON
- ✅ Alertas: temp/gas/flama con cooldown
- ✅ Modos: AUTO / MANUAL
- ✅ Comandos serial: parser para USB y BT
- ✅ OLED: SSD1306 por I2C master
- ✅ Servo + ultrasonido: SG90 (LEDC) + HC-SR04 (GPIO)
- ⚠️ Control Bluetooth clásico (mando GameSir): stack BT inicializado como stub. La integración completa del gamepad requiere stack HID/SPP adicional sobre el componente `bt` de ESP-IDF v6.1.
- ✅ Compilación verificada con ESP-IDF v6.1 (target `esp32`)

## Notas de producción

- Sin OTA, sin PSRAM, sin Secure Boot (configuración actual).
- Para producción: agregar tabla de particiones OTA, habilitar Secure Boot v2 y flash encryption, revisar tamaños de pila de tareas FreeRTOS, y validar el certificado raíz de Telegram en `esp_http_client`.

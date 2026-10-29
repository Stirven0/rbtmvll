idf_component_register(
    SRCS "app_main.c"
         "config.c"
         "sensores.c"
         "motores.c"
         "bluetooth_serial.c"
         "control_bt.c"
         "wifi_manager.c"
         "telegram_bot.c"
         "alertas.c"
         "modo.c"
         "comandos.c"
         "oled.c"
         "servo_ultrasonido.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event esp_http_client nvs_flash
             esp_timer spi_flash esp_adc esp_driver_gpio esp_driver_uart bt
             esp_driver_i2c esp_driver_ledc
)

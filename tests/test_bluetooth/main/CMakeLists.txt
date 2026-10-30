idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES driver bt
    PRIV_INCLUDE_DIRS "${IDF_PATH}/components/bt/include/esp32/include"
                      "${IDF_PATH}/components/bt/host/bluedroid/api/include/api"
)

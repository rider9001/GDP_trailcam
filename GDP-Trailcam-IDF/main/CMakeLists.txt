set(srcs
    "main.c"
    "SDSPI.c"
    "Camera.c"
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sd_card
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES nvs_flash esp_psram
                       WHOLE_ARCHIVE)
set(srcs
    "main.cpp"
    "SDSPI.c"
    "Camera.c"
    "motion_analysis.c"
    "image_types.c"
    "image_cropping.c"
    "model.c"
    "image_classifier.cpp"
    )

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sd_card
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES nvs_flash esp_psram
                       WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++11)
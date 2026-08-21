idf_component_register(
    SRCS "pwr_key.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_timer
)

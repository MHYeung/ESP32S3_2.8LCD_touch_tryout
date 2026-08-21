idf_component_register(
    SRCS "touch_cst328.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio
)

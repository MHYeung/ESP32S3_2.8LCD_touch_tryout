idf_component_register(
    SRCS "lcd_st7789.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc
)

idf_component_register(
    SRCS "sensor_hub.c"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash nvs_helper
)

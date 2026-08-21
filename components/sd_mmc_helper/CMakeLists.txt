idf_component_register(
    SRCS "sd_mmc_helper.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs esp_driver_sdmmc
)

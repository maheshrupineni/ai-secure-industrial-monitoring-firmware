idf_component_register(
    SRCS "vibration_sensor.c" "vibration_rms.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_timer freertos
)

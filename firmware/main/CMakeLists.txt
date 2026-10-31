idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES current_input vibration_sensor temperature_sensor monitor_types sensor_sim data_validation fault_manager anomaly_detector communication firmware_update log esp_timer freertos
)

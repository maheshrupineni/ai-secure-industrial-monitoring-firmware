idf_component_register(
    SRCS "firmware_update.c" "https_download.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update freertos log esp_https_ota esp_http_client mbedtls communication esp_timer esp_system esp_app_format esp_partition
)

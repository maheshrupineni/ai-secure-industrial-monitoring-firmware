idf_component_register(
    SRCS "communication.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash mqtt freertos log esp_timer mbedtls
)

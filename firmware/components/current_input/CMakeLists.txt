idf_component_register(
    SRCS "current_input.c" "current_scale.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc
)

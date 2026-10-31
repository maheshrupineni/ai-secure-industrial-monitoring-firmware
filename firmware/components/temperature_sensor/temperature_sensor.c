#include "temperature_sensor.h"
#include <math.h>
#include <stddef.h>
#include "ds18b20.h"
#include "onewire_bus.h"
#include "onewire_bus_impl_uart.h"

static onewire_bus_handle_t bus;
static ds18b20_device_handle_t device;

esp_err_t temperature_sensor_init(void)
{
    if (device != NULL) {
        return ESP_OK;
    }
    if (bus == NULL) {
        const onewire_bus_config_t config = {
            .bus_gpio_num = 4,
            .flags.en_pull_up = true
        };
        const onewire_bus_uart_config_t uart = {.uart_port_num = 1};
        esp_err_t result = onewire_new_bus_uart(&config, &uart, &bus);
        if (result != ESP_OK) {
            return result;
        }
    }
    /* Single-device bus only; driver uses Skip ROM addressing. */
    const ds18b20_config_t config = {};
    return ds18b20_new_device_from_bus(bus, &config, &device);
}

esp_err_t temperature_sensor_read(float *temperature_c)
{
    if (temperature_c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *temperature_c = NAN;
    if (device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = ds18b20_trigger_temperature_conversion(device);
    if (result != ESP_OK) {
        return result;
    }
    float measured = NAN;
    result = ds18b20_get_temperature(device, &measured);
    if (result != ESP_OK) {
        return result;
    }
    if (!isfinite(measured) || measured < -55.0f || measured > 125.0f) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *temperature_c = measured;
    return ESP_OK;
}

#include "vibration_sensor.h"
#include "vibration_rms.h"
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;
static bool ready;

static esp_err_t write_register(uint8_t address, uint8_t value)
{
    const uint8_t data[2] = {address, value};
    return i2c_master_transmit(device, data, sizeof(data), 20);
}
static esp_err_t read_registers(uint8_t address, uint8_t *data, size_t count)
{
    return i2c_master_transmit_receive(device, &address, 1, data, count, 20);
}
esp_err_t vibration_sensor_init(void)
{
    if (ready) return ESP_OK;
    esp_err_t result;
    if (bus == NULL) {
        const i2c_master_bus_config_t config = {
            .i2c_port = 0, .sda_io_num = 21, .scl_io_num = 22,
            .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true
        };
        result = i2c_new_master_bus(&config, &bus);
        if (result != ESP_OK) return result;
    }
    if (device == NULL) {
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x68, .scl_speed_hz = 100000
        };
        result = i2c_master_bus_add_device(bus, &config, &device);
        if (result != ESP_OK) return result;
    }
    uint8_t identity = 0;
    result = read_registers(0x75, &identity, 1);
    if (result != ESP_OK) return result;
    if (identity != 0x68) return ESP_ERR_INVALID_RESPONSE;
    const uint8_t registers[][2] = {
        {0x6B, 0x00}, /* wake */
        {0x1A, 0x03}, /* accelerometer low-pass ~44Hz */
        {0x19, 0x09}, /* 1kHz / (9+1) -> 100Hz output */
        {0x1C, 0x00}  /* +/-2g, 16384 counts/g */
    };
    for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        result = write_register(registers[i][0], registers[i][1]);
        if (result != ESP_OK) return result;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
    ready = true;
    return ESP_OK;
}
esp_err_t vibration_sensor_read(float *rms_g)
{
    if (rms_g == NULL) return ESP_ERR_INVALID_ARG;
    *rms_g = NAN;
    if (!ready) return ESP_ERR_INVALID_STATE;
    float values[16][3];
    TickType_t wake = xTaskGetTickCount();
    TickType_t spacing = pdMS_TO_TICKS(10);
    if (spacing == 0) return ESP_ERR_INVALID_STATE;
    int64_t start = esp_timer_get_time(), previous = start;
    for (size_t i = 0; i < 16U; ++i) {
        uint8_t raw[6];
        esp_err_t result = read_registers(0x3B, raw, sizeof(raw));
        if (result != ESP_OK) return result;
        int64_t now = esp_timer_get_time();
        if (now - start > 250000 || (i > 0 && now - previous > 30000)) {
            return ESP_ERR_TIMEOUT;
        }
        previous = now;
        for (size_t axis = 0; axis < 3U; ++axis) {
            uint16_t word = ((uint16_t)raw[axis * 2U] << 8) | raw[axis * 2U + 1U];
            int32_t signed_value = word >= 0x8000U ? (int32_t)word - 65536 : word;
            if (signed_value <= -32760 || signed_value >= 32760) return ESP_ERR_INVALID_RESPONSE;
            values[i][axis] = (float)signed_value / 16384.0f;
        }
        if (i < 15U && xTaskDelayUntil(&wake, spacing) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }
    return vibration_rms_compute(values, 16U, rms_g) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

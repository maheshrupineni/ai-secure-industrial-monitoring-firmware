#ifndef INDUSTRIAL_TEMPERATURE_SENSOR_H
#define INDUSTRIAL_TEMPERATURE_SENSOR_H
#include "esp_err.h"
/* Single DS18B20 on GPIO 4, externally powered, with external 4.7k pull-up.
 * Initialization and reads must be owned by the acquisition task only.
 * read() waits for conversion; output is NAN on failure. */
esp_err_t temperature_sensor_init(void);
esp_err_t temperature_sensor_read(float *temperature_c);
#endif

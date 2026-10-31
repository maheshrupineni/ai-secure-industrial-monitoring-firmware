#ifndef INDUSTRIAL_VIBRATION_SENSOR_H
#define INDUSTRIAL_VIBRATION_SENSOR_H
#include "esp_err.h"
/* Acquisition-task ownership only. MPU6050: I2C0, SDA21, SCL22, address 0x68.
 * read waits for a 16-sample window, nominally spaced 10ms; NAN on error. */
esp_err_t vibration_sensor_init(void);
esp_err_t vibration_sensor_read(float *rms_g);
#endif

#ifndef INDUSTRIAL_CURRENT_INPUT_H
#define INDUSTRIAL_CURRENT_INPUT_H
#include "esp_err.h"
/* ADC1 channel6/GPIO34, 12-bit, acquisition-task ownership only.
 * Potentiometer input represents a 0..10A demo level, not measured RMS.
 * Mean of 8 ADC codes; output NAN on failure. */
esp_err_t current_input_init(void);
esp_err_t current_input_read(float *out_demo_a);
#endif

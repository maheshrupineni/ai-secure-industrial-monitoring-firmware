#ifndef INDUSTRIAL_CURRENT_SCALE_H
#define INDUSTRIAL_CURRENT_SCALE_H
#include <stdbool.h>
/* Demo mapping only: 12-bit ADC code [0,4095] -> [0,10] A-equivalent.
 * This is not calibrated voltage/current conversion or RMS estimation. */
bool current_scale_from_adc(float raw_code, float *out_demo_a);
#endif

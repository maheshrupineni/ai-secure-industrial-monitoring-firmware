#include "current_scale.h"
#include <math.h>
#include <stddef.h>
bool current_scale_from_adc(float raw_code, float *out_demo_a)
{
    if (out_demo_a == NULL) return false;
    *out_demo_a = NAN;
    if (!isfinite(raw_code) || raw_code < 0.0f || raw_code > 4095.0f) return false;
    *out_demo_a = raw_code * (10.0f / 4095.0f);
    return true;
}

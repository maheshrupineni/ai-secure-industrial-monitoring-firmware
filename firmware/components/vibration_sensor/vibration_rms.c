#include "vibration_rms.h"
#include <math.h>
#include <float.h>
bool vibration_rms_compute(const float (*xyz)[3], size_t count, float *out_g)
{
    if (out_g == NULL) return false;
    *out_g = NAN;
    if (xyz == NULL || count < 2U) return false;
    double mean[3] = {0}, m2[3] = {0};
    for (size_t i = 0; i < count; ++i) {
        for (size_t axis = 0; axis < 3U; ++axis) {
            if (!isfinite(xyz[i][axis])) return false;
            double delta = (double)xyz[i][axis] - mean[axis];
            mean[axis] += delta / (double)(i + 1U);
            m2[axis] += delta * ((double)xyz[i][axis] - mean[axis]);
        }
    }
    double energy = (m2[0] + m2[1] + m2[2]) / (double)count;
    if (!isfinite(energy) || energy < 0.0) return false;
    double value = sqrt(energy);
    if (!isfinite(value) || value > FLT_MAX) return false;
    *out_g = (float)value;
    return true;
}

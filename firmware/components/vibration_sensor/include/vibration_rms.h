#ifndef INDUSTRIAL_VIBRATION_RMS_H
#define INDUSTRIAL_VIBRATION_RMS_H
#include <stdbool.h>
#include <stddef.h>
/* Interleaved XYZ samples in g. Removes the window mean of each axis.
 * Output is sqrt(var(X)+var(Y)+var(Z)), using population variance.
 * This is a window statistic, not an industrial bandwidth specification. */
bool vibration_rms_compute(const float (*xyz)[3], size_t count, float *out_g);
#endif

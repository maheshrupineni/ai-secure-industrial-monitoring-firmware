#ifndef INDUSTRIAL_SENSOR_SIM_H
#define INDUSTRIAL_SENSOR_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "monitor_types.h"

/*
 * Generate one synthetic normal-operation sample.
 *
 * sequence:     Caller-supplied acquisition counter.
 * timestamp_ms: Monotonic acquisition time since startup, in milliseconds.
 * out_sample:   Writable destination owned by the caller.
 *
 * Returns true when the output record has been written.
 * Returns false if out_sample is NULL.
 *
 * This function does not block, allocate memory, or retain the pointer.
 * Identical inputs produce identical output values.
 * RMS features are generated directly, not calculated from waveforms.
 */
bool sensor_sim_generate(uint32_t sequence,
                         uint64_t timestamp_ms,
                         sensor_sample_t *out_sample);

#endif /* INDUSTRIAL_SENSOR_SIM_H */

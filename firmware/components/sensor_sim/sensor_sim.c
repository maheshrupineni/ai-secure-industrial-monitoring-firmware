#include "sensor_sim.h"

#include <stddef.h>

/* Development-only scenarios. Enable at most one. */
#define SENSOR_SIM_FAULT_SCENARIO_ENABLED 0
#define SENSOR_SIM_AI_SCENARIO_ENABLED    0

#if SENSOR_SIM_FAULT_SCENARIO_ENABLED && SENSOR_SIM_AI_SCENARIO_ENABLED
#error "Enable only one sensor simulation scenario"
#endif

static float triangle_wave(uint32_t sequence, uint32_t half_period)
{
    const uint32_t period = half_period * 2U;
    const uint32_t position = sequence % period;
    const uint32_t height =
        position <= half_period ? position : period - position;

    return (float)height / (float)half_period;
}

bool sensor_sim_generate(uint32_t sequence,
                         uint64_t timestamp_ms,
                         sensor_sample_t *out_sample)
{
    if (out_sample == NULL) {
        return false;
    }

    const float load = triangle_wave(sequence, 10U);
    const float thermal_cycle = triangle_wave(sequence, 30U);

    out_sample->sequence = sequence;
    out_sample->timestamp_ms = timestamp_ms;
    out_sample->temperature_c = 40.0f + 5.0f * thermal_cycle;
    out_sample->vibration_rms_g = 0.020f + 0.010f * load;
    out_sample->current_rms_a = 2.0f + 1.0f * load;

#if SENSOR_SIM_FAULT_SCENARIO_ENABLED
    if (sequence >= 5U && sequence < 10U) {
        out_sample->temperature_c = 65.0f;
    } else if (sequence >= 10U && sequence < 15U) {
        out_sample->temperature_c = 85.0f;
    }
#endif

#if SENSOR_SIM_AI_SCENARIO_ENABLED
    if (sequence >= 5U && sequence < 10U) {
        out_sample->temperature_c = 54.0f;
        out_sample->vibration_rms_g = 0.035f;
        out_sample->current_rms_a = 3.5f;
    }
#endif

    return true;
}
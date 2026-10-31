#include <math.h>
#include <stddef.h>

#include "data_validation.h"

/* Provisional plausibility limits for the simulated sensor channels. */
#define TEMPERATURE_MIN_C    (-40.0f)
#define TEMPERATURE_MAX_C    150.0f
#define VIBRATION_MIN_G      0.0f
#define VIBRATION_MAX_G      16.0f
#define CURRENT_MIN_A        0.0f
#define CURRENT_MAX_A        100.0f

bool data_validation_check(const sensor_sample_t *sample,
                           uint64_t now_ms,
                           uint64_t max_age_ms,
                           validated_sample_t *out_result)
{
    if ((sample == NULL) || (out_result == NULL)) {
        return false;
    }

    out_result->sample = *sample;
    out_result->issues = VALIDATION_OK;

    if (!isfinite(sample->temperature_c) ||
        (sample->temperature_c < TEMPERATURE_MIN_C) ||
        (sample->temperature_c > TEMPERATURE_MAX_C)) {
        out_result->issues |= VALIDATION_TEMPERATURE_INVALID;
    }

    if (!isfinite(sample->vibration_rms_g) ||
        (sample->vibration_rms_g < VIBRATION_MIN_G) ||
        (sample->vibration_rms_g > VIBRATION_MAX_G)) {
        out_result->issues |= VALIDATION_VIBRATION_INVALID;
    }

    if (!isfinite(sample->current_rms_a) ||
        (sample->current_rms_a < CURRENT_MIN_A) ||
        (sample->current_rms_a > CURRENT_MAX_A)) {
        out_result->issues |= VALIDATION_CURRENT_INVALID;
    }

    if (sample->timestamp_ms > now_ms) {
        out_result->issues |= VALIDATION_TIMESTAMP_INVALID;
    } else if ((now_ms - sample->timestamp_ms) > max_age_ms) {
        out_result->issues |= VALIDATION_SAMPLE_STALE;
    }

    return true;
}

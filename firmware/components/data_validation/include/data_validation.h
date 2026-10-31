#ifndef INDUSTRIAL_DATA_VALIDATION_H
#define INDUSTRIAL_DATA_VALIDATION_H

#include <stdbool.h>
#include <stdint.h>

#include "monitor_types.h"

/* Independent flags; multiple issues can be combined. */
enum {
    VALIDATION_OK                  = 0,
    VALIDATION_TEMPERATURE_INVALID = 1U << 0,
    VALIDATION_VIBRATION_INVALID   = 1U << 1,
    VALIDATION_CURRENT_INVALID     = 1U << 2,
    VALIDATION_SAMPLE_STALE        = 1U << 3,
    VALIDATION_TIMESTAMP_INVALID   = 1U << 4
};

/* Preserve the original measurement alongside its quality report. */
typedef struct {
    sensor_sample_t sample;
    uint32_t issues;
} validated_sample_t;

/*
 * Evaluate one sample using caller-supplied monotonic time.
 *
 * sample:     Input record owned by the caller.
 * now_ms:     Current time from the same clock as timestamp_ms.
 * max_age_ms: Maximum permitted sample age, inclusive.
 * out_result: Separate writable result record owned by the caller.
 *
 * Input and output storage must not overlap.
 *
 * Returns false if either record pointer is NULL.
 * Otherwise writes the result and returns true, even when the
 * sample has validation issues. Inspect out_result->issues.
 *
 * Does not block, allocate memory, or retain pointers.
 * Operating fault thresholds are not evaluated here.
 */
bool data_validation_check(const sensor_sample_t *sample,
                           uint64_t now_ms,
                           uint64_t max_age_ms,
                           validated_sample_t *out_result);

#endif /* INDUSTRIAL_DATA_VALIDATION_H */

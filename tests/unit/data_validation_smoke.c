#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "data_validation.h"

static unsigned int checks_passed = 0U;

static void check(bool condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(EXIT_FAILURE);
    }

    checks_passed++;
    printf("PASS: %s\n", description);
}

int main(void)
{
    sensor_sample_t sample = {
        .sequence = 1U,
        .timestamp_ms = 1000U,
        .temperature_c = 40.0f,
        .vibration_rms_g = 0.020f,
        .current_rms_a = 2.0f
    };
    validated_sample_t result = {0};

    check(data_validation_check(&sample, 1100U, 500U, &result) &&
          result.issues == VALIDATION_OK,
          "normal sample accepted");

    check(data_validation_check(&sample, 1500U, 500U, &result) &&
          result.issues == VALIDATION_OK,
          "maximum permitted age accepted");

    check(data_validation_check(&sample, 1501U, 500U, &result) &&
          result.issues == VALIDATION_SAMPLE_STALE,
          "sample one millisecond too old flagged");

    check(data_validation_check(&sample, 999U, 500U, &result) &&
          result.issues == VALIDATION_TIMESTAMP_INVALID,
          "future timestamp flagged");

    sample.temperature_c = NAN;
    sample.current_rms_a = -1.0f;

    check(data_validation_check(&sample, 1100U, 500U, &result) &&
          result.issues == (VALIDATION_TEMPERATURE_INVALID |
                            VALIDATION_CURRENT_INVALID),
          "NaN and negative current produce combined flags");

    check(!data_validation_check(NULL, 1100U, 500U, &result),
          "NULL input rejected");

    check(!data_validation_check(&sample, 1100U, 500U, NULL),
          "NULL output rejected");

    printf("All %u checks passed.\n", checks_passed);
    return EXIT_SUCCESS;
}

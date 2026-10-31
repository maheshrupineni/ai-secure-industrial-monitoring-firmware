#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "anomaly_detector.h"

static unsigned checks;

static void check(bool condition)
{
    if (!condition) {
        fprintf(stderr, "Invalid-input check %u failed\n", checks + 1U);
        exit(EXIT_FAILURE);
    }
    ++checks;
}

int main(void)
{
    validated_sample_t input = {
        .sample = {
            .temperature_c = 42.5f,
            .vibration_rms_g = 0.025f,
            .current_rms_a = 2.5f
        },
        .issues = VALIDATION_OK
    };
    anomaly_detection_t output = {0};

    check(anomaly_detector_evaluate(&input, &output));
    check(!anomaly_detector_evaluate(&input, NULL));

    output.score = 123.0f;
    output.is_anomaly = true;
    check(!anomaly_detector_evaluate(NULL, &output) &&
          output.score == 0.0f && !output.is_anomaly);

    input.issues = VALIDATION_SAMPLE_STALE;
    check(!anomaly_detector_evaluate(&input, &output));

    input.issues = VALIDATION_OK;
    input.sample.temperature_c = NAN;
    check(!anomaly_detector_evaluate(&input, &output));

    input.sample.temperature_c = INFINITY;
    check(!anomaly_detector_evaluate(&input, &output));

    input.sample.temperature_c = FLT_MAX;
    check(!anomaly_detector_evaluate(&input, &output) &&
          output.score == 0.0f && !output.is_anomaly);

    fprintf(stderr, "C API checks passed: %u\n", checks);

    float temperature;
    float vibration;
    float current;

    while (1) {
        const int count = scanf("%f %f %f",
                                &temperature, &vibration, &current);
        if (count == EOF) {
            break;
        }
        if (count != 3) {
            fprintf(stderr, "Malformed input\n");
            return EXIT_FAILURE;
        }

        input = (validated_sample_t){
            .sample = {
                .temperature_c = temperature,
                .vibration_rms_g = vibration,
                .current_rms_a = current
            },
            .issues = VALIDATION_OK
        };

        if (!anomaly_detector_evaluate(&input, &output)) {
            fprintf(stderr, "Scoring failed\n");
            return EXIT_FAILURE;
        }

        printf("%.9g %u\n", (double)output.score,
               output.is_anomaly ? 1U : 0U);
    }

    return EXIT_SUCCESS;
}

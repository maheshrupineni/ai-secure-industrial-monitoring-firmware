#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sensor_sim.h"
#include "data_validation.h"
#include "fault_manager.h"
#include "anomaly_detector.h"
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL at line %d\n", __LINE__); return EXIT_FAILURE; } } while (0)
int main(void)
{
    fault_manager_t manager;
    CHECK(fault_manager_init(&manager));
    for (uint32_t i = 0; i <= 60U; ++i) {
        sensor_sample_t sample;
        validated_sample_t valid;
        anomaly_detection_t ai;
        CHECK(sensor_sim_generate(i, (uint64_t)i * 1000U, &sample));
        CHECK(sample.sequence == i && sample.timestamp_ms == (uint64_t)i * 1000U);
        CHECK(data_validation_check(&sample, sample.timestamp_ms, 500U, &valid));
        CHECK(valid.issues == VALIDATION_OK);
        CHECK(fault_manager_update(&manager, &valid));
        CHECK(manager.state == MONITOR_STATE_NORMAL);
        CHECK(anomaly_detector_evaluate(&valid, &ai));
        /* Generator is a plumbing fixture, not this sensor model's normal distribution. */
        CHECK(isfinite(ai.score));
    }
    /* Classify the captured baseline explicitly using the updated model. */
    sensor_sample_t baseline = {.sequence = 61U, .timestamp_ms = 61000U,
        .temperature_c = 42.5f, .vibration_rms_g = 0.0f, .current_rms_a = 2.444f};
    validated_sample_t baseline_valid;
    anomaly_detection_t baseline_ai;
    CHECK(data_validation_check(&baseline, 61000U, 500U, &baseline_valid));
    CHECK(anomaly_detector_evaluate(&baseline_valid, &baseline_ai) && !baseline_ai.is_anomaly);
    baseline.temperature_c = 65.0f;
    CHECK(data_validation_check(&baseline, 61000U, 500U, &baseline_valid));
    CHECK(anomaly_detector_evaluate(&baseline_valid, &baseline_ai) && baseline_ai.is_anomaly);
    sensor_sample_t shifted = {.sequence = 61U, .timestamp_ms = 61000U,
        .temperature_c = 54.0f, .vibration_rms_g = 0.035f, .current_rms_a = 3.5f};
    validated_sample_t valid;
    anomaly_detection_t ai;
    CHECK(data_validation_check(&shifted, 61000U, 500U, &valid));
    CHECK(valid.issues == VALIDATION_OK);
    CHECK(fault_manager_update(&manager, &valid));
    CHECK(manager.state == MONITOR_STATE_NORMAL);
    CHECK(anomaly_detector_evaluate(&valid, &ai) && ai.is_anomaly);
    CHECK(data_validation_check(&shifted, 61501U, 500U, &valid));
    CHECK(valid.issues & VALIDATION_SAMPLE_STALE);
    CHECK(!fault_manager_update(&manager, &valid));
    CHECK(!anomaly_detector_evaluate(&valid, &ai));
    shifted.temperature_c = NAN;
    CHECK(data_validation_check(&shifted, 61000U, 500U, &valid));
    CHECK(valid.issues & VALIDATION_TEMPERATURE_INVALID);
    CHECK(!fault_manager_update(&manager, &valid));
    CHECK(!anomaly_detector_evaluate(&valid, &ai));
    puts("PASS: host component integration: 61 generator plumbing samples, captured normal/65C classification, shifted anomaly, stale and NaN rejection");
    return EXIT_SUCCESS;
}

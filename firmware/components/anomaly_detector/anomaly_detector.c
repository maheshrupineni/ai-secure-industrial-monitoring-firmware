#include "anomaly_detector.h"
#include "anomaly_model.h"

#include <math.h>
#include <stddef.h>

bool anomaly_detector_evaluate(const validated_sample_t *input,
                               anomaly_detection_t *output)
{
    if (output == NULL) {
        return false;
    }

    *output = (anomaly_detection_t){0};

    if (input == NULL || input->issues != VALIDATION_OK) {
        return false;
    }

    const float features[ANOMALY_MODEL_FEATURE_COUNT] = {
        input->sample.temperature_c,
        input->sample.vibration_rms_g,
        input->sample.current_rms_a
    };

    float squared_distance = 0.0f;

    for (unsigned index = 0U;
         index < ANOMALY_MODEL_FEATURE_COUNT; ++index) {
        if (!isfinite(features[index])) {
            return false;
        }

        const float standardized =
            (features[index] - anomaly_model_means[index]) /
            anomaly_model_stddevs[index];

        squared_distance += standardized * standardized;
    }

    const float score =
        squared_distance / (float)ANOMALY_MODEL_FEATURE_COUNT;

    if (!isfinite(score)) {
        return false;
    }

    output->score = score;
    output->is_anomaly = score > ANOMALY_MODEL_THRESHOLD;
    return true;
}

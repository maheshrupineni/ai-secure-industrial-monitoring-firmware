#ifndef INDUSTRIAL_ANOMALY_DETECTOR_H
#define INDUSTRIAL_ANOMALY_DETECTOR_H

#include <stdbool.h>

#include "data_validation.h"

typedef struct {
    float score;
    bool is_anomaly;
} anomaly_detection_t;

/*
 * Evaluate one recently validated sensor record.
 *
 * Returns true when scoring succeeds.
 * Returns false for NULL arguments, validation issues, non-finite
 * features or a non-finite score.
 *
 * A non-NULL output is cleared before validation. Output values are
 * meaningful only when this function returns true.
 *
 * Input and output storage must not overlap.
 * The caller owns freshness checks at consumption time.
 * No allocation, blocking, retained pointers or mutable global state.
 *
 * The score is a statistical distance, not a failure probability.
 */
bool anomaly_detector_evaluate(const validated_sample_t *input,
                               anomaly_detection_t *output);

#endif /* INDUSTRIAL_ANOMALY_DETECTOR_H */

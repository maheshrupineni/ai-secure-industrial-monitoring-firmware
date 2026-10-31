#ifndef INDUSTRIAL_PIPELINE_MESSAGES_H
#define INDUSTRIAL_PIPELINE_MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

#include "data_validation.h"
#include "monitor_types.h"

/*
 * A processing stage must distinguish an unavailable result
 * from a completed result.
 */
typedef enum {
    PROCESSING_NOT_IMPLEMENTED = 0,
    PROCESSING_INPUT_INVALID,
    PROCESSING_COMPLETE
} processing_status_t;

/*
 * Fault-task output.
 *
 * The task owns state decisions. Until fault logic exists,
 * state remains STARTUP and status is NOT_IMPLEMENTED.
 *
 * validation_issues preserves input quality information.
 */
typedef struct {
    uint32_t sequence;
    uint64_t timestamp_ms;
    monitor_state_t state;
    processing_status_t status;
    uint32_t validation_issues;
} fault_result_t;

/*
 * AI-task output.
 *
 * score and is_anomaly are meaningful only when status
 * is PROCESSING_COMPLETE. Zero is not evidence of normality
 * when processing has not been implemented.
 */
typedef struct {
    uint32_t sequence;
    uint64_t timestamp_ms;
    processing_status_t status;
    float score;
    bool is_anomaly;
} anomaly_result_t;

/*
 * Latest information owned by the communication task.
 *
 * Each has_* flag means that a record has arrived.
 * It does not mean processing completed successfully.
 *
 * The task must compare sequence numbers before presenting
 * measurement, fault, and AI records as one matched set.
 */
typedef struct {
    bool has_sample;
    bool has_fault;
    bool has_anomaly;

    validated_sample_t sample;
    fault_result_t fault;
    anomaly_result_t anomaly;
} communication_snapshot_t;

#endif /* INDUSTRIAL_PIPELINE_MESSAGES_H */

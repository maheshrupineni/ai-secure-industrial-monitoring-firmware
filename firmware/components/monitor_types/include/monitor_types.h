#ifndef INDUSTRIAL_MONITOR_TYPES_H
#define INDUSTRIAL_MONITOR_TYPES_H

#include <stdint.h>

/*
 * Operating states.
 * The fault manager will own state transitions.
 */
typedef enum {
    MONITOR_STATE_STARTUP = 0,
    MONITOR_STATE_NORMAL,
    MONITOR_STATE_WARNING,
    MONITOR_STATE_FAULT,
    MONITOR_STATE_RECOVERY
} monitor_state_t;

/*
 * One acquisition record.
 * Values require validation before use in detection.
 */
typedef struct {
    uint32_t sequence;
    uint64_t timestamp_ms;
    float temperature_c;
    float vibration_rms_g;
    float current_rms_a;
} sensor_sample_t;

#endif /* INDUSTRIAL_MONITOR_TYPES_H */

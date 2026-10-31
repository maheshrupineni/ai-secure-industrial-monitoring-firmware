#include "fault_manager.h"

#include <math.h>
#include <stddef.h>

static void reset_counts(fault_manager_t *manager)
{
    manager->warning_count = 0U;
    manager->fault_count = 0U;
    manager->clear_count = 0U;
}

static uint32_t count_condition(uint32_t count, bool condition,
                                uint32_t limit)
{
    if (!condition) {
        return 0U;
    }

    return count < limit ? count + 1U : limit;
}

bool fault_manager_init(fault_manager_t *manager)
{
    if (manager == NULL) {
        return false;
    }

    manager->state = MONITOR_STATE_STARTUP;
    reset_counts(manager);
    return true;
}

bool fault_manager_update(fault_manager_t *manager,
                          const validated_sample_t *input)
{
    if (manager == NULL) {
        return false;
    }

    if (input == NULL) {
        reset_counts(manager);
        return false;
    }

    const sensor_sample_t *sample = &input->sample;

    if (input->issues != VALIDATION_OK ||
        !isfinite(sample->temperature_c) ||
        !isfinite(sample->vibration_rms_g) ||
        !isfinite(sample->current_rms_a)) {
        reset_counts(manager);
        return false;
    }

    const bool warning =
        sample->temperature_c >= FAULT_MANAGER_TEMP_WARNING_C ||
        sample->vibration_rms_g >= FAULT_MANAGER_VIB_WARNING_G ||
        sample->current_rms_a >= FAULT_MANAGER_CURRENT_WARNING_A;

    const bool fault =
        sample->temperature_c >= FAULT_MANAGER_TEMP_FAULT_C ||
        sample->vibration_rms_g >= FAULT_MANAGER_VIB_FAULT_G ||
        sample->current_rms_a >= FAULT_MANAGER_CURRENT_FAULT_A;

    const bool clear =
        sample->temperature_c < FAULT_MANAGER_TEMP_CLEAR_C &&
        sample->vibration_rms_g < FAULT_MANAGER_VIB_CLEAR_G &&
        sample->current_rms_a < FAULT_MANAGER_CURRENT_CLEAR_A;

    manager->warning_count =
        count_condition(manager->warning_count, warning,
                        FAULT_MANAGER_WARNING_SAMPLES);
    manager->fault_count =
        count_condition(manager->fault_count, fault,
                        FAULT_MANAGER_FAULT_SAMPLES);
    manager->clear_count =
        count_condition(manager->clear_count, clear,
                        FAULT_MANAGER_CLEAR_SAMPLES);

    const monitor_state_t previous = manager->state;

    switch (manager->state) {
    case MONITOR_STATE_STARTUP:
        if (manager->fault_count >= FAULT_MANAGER_FAULT_SAMPLES) {
            manager->state = MONITOR_STATE_FAULT;
        } else if (manager->warning_count >= FAULT_MANAGER_WARNING_SAMPLES) {
            manager->state = MONITOR_STATE_WARNING;
        } else if (clear) {
            manager->state = MONITOR_STATE_NORMAL;
        }
        break;

    case MONITOR_STATE_NORMAL:
        if (manager->fault_count >= FAULT_MANAGER_FAULT_SAMPLES) {
            manager->state = MONITOR_STATE_FAULT;
        } else if (manager->warning_count >= FAULT_MANAGER_WARNING_SAMPLES) {
            manager->state = MONITOR_STATE_WARNING;
        }
        break;

    case MONITOR_STATE_WARNING:
        if (manager->fault_count >= FAULT_MANAGER_FAULT_SAMPLES) {
            manager->state = MONITOR_STATE_FAULT;
        } else if (manager->clear_count >= FAULT_MANAGER_CLEAR_SAMPLES) {
            manager->state = MONITOR_STATE_RECOVERY;
        }
        break;

    case MONITOR_STATE_FAULT:
        if (manager->clear_count >= FAULT_MANAGER_CLEAR_SAMPLES) {
            manager->state = MONITOR_STATE_RECOVERY;
        }
        break;

    case MONITOR_STATE_RECOVERY:
        if (fault) {
            manager->state = MONITOR_STATE_FAULT;
        } else if (warning) {
            manager->state = MONITOR_STATE_WARNING;
        } else if (manager->clear_count >= FAULT_MANAGER_CLEAR_SAMPLES) {
            manager->state = MONITOR_STATE_NORMAL;
        }
        break;

    default:
        manager->state = MONITOR_STATE_STARTUP;
        reset_counts(manager);
        return false;
    }

    if (manager->state != previous) {
        reset_counts(manager);
    }

    return true;
}

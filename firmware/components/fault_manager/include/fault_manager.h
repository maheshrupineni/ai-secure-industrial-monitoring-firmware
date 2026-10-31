#ifndef INDUSTRIAL_FAULT_MANAGER_H
#define INDUSTRIAL_FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "data_validation.h"
#include "monitor_types.h"

/*
 * Demonstration thresholds only.
 * Warning/fault comparisons are inclusive.
 * Clearing requires all measurements strictly below clear limits.
 */
#define FAULT_MANAGER_TEMP_WARNING_C       60.0f
#define FAULT_MANAGER_TEMP_FAULT_C         80.0f
#define FAULT_MANAGER_TEMP_CLEAR_C         55.0f

#define FAULT_MANAGER_VIB_WARNING_G         0.050f
#define FAULT_MANAGER_VIB_FAULT_G           0.100f
#define FAULT_MANAGER_VIB_CLEAR_G           0.040f

#define FAULT_MANAGER_CURRENT_WARNING_A     5.0f
#define FAULT_MANAGER_CURRENT_FAULT_A       8.0f
#define FAULT_MANAGER_CURRENT_CLEAR_A       4.5f

#define FAULT_MANAGER_WARNING_SAMPLES       3U
#define FAULT_MANAGER_FAULT_SAMPLES         2U
#define FAULT_MANAGER_CLEAR_SAMPLES         5U

/*
 * One instance owned exclusively by the fault task.
 * Initialize before the first update.
 */
typedef struct {
    monitor_state_t state;
    uint32_t warning_count;
    uint32_t fault_count;
    uint32_t clear_count;
} fault_manager_t;

/* Returns false for NULL; otherwise initializes STARTUP and zero counts. */
bool fault_manager_init(fault_manager_t *manager);

/*
 * Process one validated record.
 *
 * Returns false for NULL arguments or invalid input.
 * Invalid input resets consecutive-sample counters while preserving state.
 * Returns true when a usable measurement has been evaluated, even when
 * persistence requirements mean no state transition occurs.
 *
 * Input must have passed data_validation_check recently.
 * The caller owns freshness checks at consumption time and must supply
 * each acquisition record only once, in acquisition order.
 *
 * No allocation, blocking, pointer retention, or network operations.
 */
bool fault_manager_update(fault_manager_t *manager,
                          const validated_sample_t *input);

#endif /* INDUSTRIAL_FAULT_MANAGER_H */

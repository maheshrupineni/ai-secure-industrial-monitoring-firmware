#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "fault_manager.h"

static unsigned checks_passed;

static void check(bool condition, const char *description)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(EXIT_FAILURE);
    }

    printf("PASS: %s\n", description);
    ++checks_passed;
}

static validated_sample_t measurement(float temperature,
                                      float vibration,
                                      float current)
{
    validated_sample_t result = {
        .sample = {
            .sequence = 0U,
            .timestamp_ms = 1000U,
            .temperature_c = temperature,
            .vibration_rms_g = vibration,
            .current_rms_a = current
        },
        .issues = VALIDATION_OK
    };
    return result;
}

static bool feed(fault_manager_t *manager,
                 validated_sample_t sample, unsigned count)
{
    for (unsigned i = 0U; i < count; ++i) {
        if (!fault_manager_update(manager, &sample)) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    fault_manager_t manager;
    validated_sample_t normal = measurement(40.0f, 0.020f, 2.0f);
    validated_sample_t warning = measurement(60.0f, 0.020f, 2.0f);
    validated_sample_t fault = measurement(80.0f, 0.020f, 2.0f);
    validated_sample_t invalid = normal;

    check(!fault_manager_init(NULL), "NULL initialization rejected");
    check(fault_manager_init(&manager) &&
          manager.state == MONITOR_STATE_STARTUP,
          "initial state is STARTUP");

    check(feed(&manager, normal, 1U) &&
          manager.state == MONITOR_STATE_NORMAL,
          "clear startup sample enters NORMAL");

    check(feed(&manager, warning, 2U) &&
          manager.state == MONITOR_STATE_NORMAL,
          "two warning samples do not confirm warning");
    check(feed(&manager, normal, 1U) &&
          feed(&manager, warning, 2U) &&
          manager.state == MONITOR_STATE_NORMAL,
          "normal sample interrupts warning persistence");
    check(feed(&manager, warning, 1U) &&
          manager.state == MONITOR_STATE_WARNING,
          "third consecutive sample at 60 C confirms WARNING");

    check(feed(&manager, fault, 1U) &&
          manager.state == MONITOR_STATE_WARNING,
          "one fault sample does not confirm fault");
    check(feed(&manager, fault, 1U) &&
          manager.state == MONITOR_STATE_FAULT,
          "second sample at 80 C confirms FAULT");

    invalid.issues = VALIDATION_SAMPLE_STALE;
    check(!fault_manager_update(&manager, &invalid) &&
          manager.state == MONITOR_STATE_FAULT,
          "invalid sample cannot clear FAULT");

    check(feed(&manager, normal, 4U) &&
          manager.state == MONITOR_STATE_FAULT,
          "four clear samples do not release FAULT");
    check(!fault_manager_update(&manager, &invalid) &&
          feed(&manager, normal, 4U) &&
          manager.state == MONITOR_STATE_FAULT,
          "invalid sample interrupts clear persistence");
    check(feed(&manager, normal, 1U) &&
          manager.state == MONITOR_STATE_RECOVERY,
          "fifth consecutive clear sample enters RECOVERY");
    check(feed(&manager, normal, 4U) &&
          manager.state == MONITOR_STATE_RECOVERY,
          "recovery requires a new set of clear samples");
    check(feed(&manager, normal, 1U) &&
          manager.state == MONITOR_STATE_NORMAL,
          "five additional clear samples restore NORMAL");

    check(fault_manager_init(&manager) &&
          feed(&manager, measurement(40.0f, 0.050f, 2.0f), 3U) &&
          manager.state == MONITOR_STATE_WARNING,
          "vibration warning boundary is inclusive");

    check(fault_manager_init(&manager) &&
          feed(&manager, measurement(40.0f, 0.020f, 5.0f), 3U) &&
          manager.state == MONITOR_STATE_WARNING,
          "current warning boundary is inclusive");

    check(fault_manager_init(&manager) &&
          feed(&manager, measurement(40.0f, 0.100f, 2.0f), 2U) &&
          manager.state == MONITOR_STATE_FAULT,
          "vibration fault boundary is inclusive");

    check(fault_manager_init(&manager) &&
          feed(&manager, measurement(40.0f, 0.020f, 8.0f), 2U) &&
          manager.state == MONITOR_STATE_FAULT,
          "current fault boundary is inclusive");

    check(feed(&manager, measurement(55.0f, 0.020f, 2.0f), 6U) &&
          manager.state == MONITOR_STATE_FAULT,
          "temperature at clear boundary cannot clear fault");
    check(feed(&manager, measurement(40.0f, 0.040f, 2.0f), 6U) &&
          manager.state == MONITOR_STATE_FAULT,
          "vibration at clear boundary cannot clear fault");
    check(feed(&manager, measurement(40.0f, 0.020f, 4.5f), 6U) &&
          manager.state == MONITOR_STATE_FAULT,
          "current at clear boundary cannot clear fault");

    check(feed(&manager, normal, 5U) &&
          manager.state == MONITOR_STATE_RECOVERY &&
          feed(&manager, warning, 1U) &&
          manager.state == MONITOR_STATE_WARNING,
          "warning during recovery returns to WARNING");

    check(feed(&manager, normal, 5U) &&
          manager.state == MONITOR_STATE_RECOVERY &&
          feed(&manager, fault, 1U) &&
          manager.state == MONITOR_STATE_FAULT,
          "fault during recovery immediately returns to FAULT");

    invalid = normal;
    invalid.sample.temperature_c = NAN;
    check(!fault_manager_update(&manager, &invalid) &&
          manager.state == MONITOR_STATE_FAULT,
          "NaN rejected even with a clear validation flag");

    check(!fault_manager_update(NULL, &normal),
          "NULL manager rejected");
    check(!fault_manager_update(&manager, NULL) &&
          manager.state == MONITOR_STATE_FAULT,
          "NULL input rejected while preserving state");

    printf("All %u checks passed.\n", checks_passed);
    return EXIT_SUCCESS;
}

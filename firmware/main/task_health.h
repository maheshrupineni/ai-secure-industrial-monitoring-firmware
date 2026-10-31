#ifndef INDUSTRIAL_TASK_HEALTH_H
#define INDUSTRIAL_TASK_HEALTH_H

#include <inttypes.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * Development diagnostic for a live task.
 *
 * handle must identify a task that has not been deleted.
 * allocated_bytes is the stack allocation used at task creation.
 *
 * The watermark reports historical minimum unused stack bytes.
 * This function does not change the task's stack allocation.
 */
static inline void task_health_log(TaskHandle_t handle,
                                    const char *name,
                                    uint32_t allocated_bytes)
{
    if ((handle == NULL) || (name == NULL)) {
        ESP_LOGE("task_health", "Missing task handle or name");
        return;
    }

    const UBaseType_t minimum_free =
        uxTaskGetStackHighWaterMark(handle);

    ESP_LOGI("task_health",
             "STACK task=%s allocated_bytes=%" PRIu32
             " min_free_bytes=%" PRIu32,
             name,
             allocated_bytes,
             (uint32_t)minimum_free);
}

#endif /* INDUSTRIAL_TASK_HEALTH_H */

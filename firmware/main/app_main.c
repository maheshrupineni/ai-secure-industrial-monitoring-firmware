#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "anomaly_detector.h"
#include <math.h>
#include <stdio.h>

#include "communication.h"
#include "communication_config.h"
#include "data_validation.h"
#include "fault_manager.h"
#include "firmware_update.h"
#include "monitor_test_config.h"
#include "pipeline_messages.h"
#include "sensor_sim.h"
#include "temperature_sensor.h"
#include "vibration_sensor.h"
#include "current_input.h"
#include "task_health.h"

#define SAMPLE_INTERVAL_MS  1000U
#define SAMPLE_MAX_AGE_MS   500U
#define RECEIVE_TIMEOUT_MS  3000U
#define QUEUE_LENGTH        8U
#define TASK_STACK_BYTES    4096U
#define MONITOR_TASK_COUNT  6U

static const char *TAG = "industrial_monitor";

static QueueHandle_t raw_queue;
static QueueHandle_t logging_queue;
static QueueHandle_t fault_queue;
static QueueHandle_t anomaly_queue;
static QueueHandle_t communication_sample;
static QueueHandle_t communication_fault;
static QueueHandle_t communication_anomaly;

typedef struct {
    const char *name;
    TaskHandle_t handle;
} task_record_t;

static task_record_t task_records[MONITOR_TASK_COUNT];
static size_t task_record_count;

static uint64_t monitor_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static const char *state_name(monitor_state_t state)
{
    switch (state) {
    case MONITOR_STATE_STARTUP:  return "STARTUP";
    case MONITOR_STATE_NORMAL:   return "NORMAL";
    case MONITOR_STATE_WARNING:  return "WARNING";
    case MONITOR_STATE_FAULT:    return "FAULT";
    case MONITOR_STATE_RECOVERY: return "RECOVERY";
    default:                    return "UNKNOWN";
    }
}

static const char *processing_status_name(processing_status_t status)
{
    switch (status) {
    case PROCESSING_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
    case PROCESSING_INPUT_INVALID:   return "INPUT_INVALID";
    case PROCESSING_COMPLETE:        return "COMPLETE";
    default:                         return "UNKNOWN";
    }
}

static QueueHandle_t create_queue(UBaseType_t length,
                                   UBaseType_t item_size)
{
    QueueHandle_t queue = xQueueCreate(length, item_size);
    if (queue == NULL) {
        ESP_LOGE(TAG, "Startup failed: queue allocation");
        abort();
    }
    return queue;
}

static void send_validated(QueueHandle_t queue,
                           const validated_sample_t *record,
                           const char *destination)
{
    if (xQueueSend(queue, record, pdMS_TO_TICKS(100U)) != pdPASS) {
        ESP_LOGW(TAG, "%s queue full: dropped seq=%" PRIu32,
                 destination, record->sample.sequence);
    }
}

static void acquisition_task(void *parameters)
{
    (void)parameters;
    uint32_t sequence = 0U;
    uint32_t dropped = 0U;
    const esp_err_t initialization = temperature_sensor_init();
    if (initialization != ESP_OK) {
        ESP_LOGE(TAG, "Temperature driver initialization failed: %s; reboot after fixing configuration",
                 esp_err_to_name(initialization));
    }
    const esp_err_t vibration_initialization = vibration_sensor_init();
    if (vibration_initialization != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 initialization failed: %s; reboot after fixing configuration",
                 esp_err_to_name(vibration_initialization));
    }
    const esp_err_t current_initialization = current_input_init();
    if (current_initialization != ESP_OK) {
        ESP_LOGE(TAG, "Current proxy initialization failed: %s; reboot after fixing configuration",
                 esp_err_to_name(current_initialization));
    }
    ESP_LOGI(TAG, "Sources: temperature=DS18B20 GPIO4; vibration=MPU6050 I2C21/22; current=ADC1 GPIO34 demo proxy, NOT measured RMS");
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(SAMPLE_INTERVAL_MS);

    while (1) {
        float temperature_c = NAN;
        const esp_err_t result = temperature_sensor_read(&temperature_c);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Temperature read failed seq=%" PRIu32 ": %s",
                     sequence, esp_err_to_name(result));
        }
        float vibration_g = NAN;
        const esp_err_t vibration_result = vibration_sensor_read(&vibration_g);
        if (vibration_result != ESP_OK) {
            ESP_LOGW(TAG, "Vibration read failed seq=%" PRIu32 ": %s",
                     sequence, esp_err_to_name(vibration_result));
        }
        float current_demo_a = NAN;
        const esp_err_t current_result = current_input_read(&current_demo_a);
        if (current_result != ESP_OK) {
            ESP_LOGW(TAG, "Current proxy read failed seq=%" PRIu32 ": %s",
                     sequence, esp_err_to_name(current_result));
        }
        sensor_sample_t sample = {0};
        /* Conversion blocks: timestamp completion, not conversion start.
         * Current is an ADC demo level; vibration summarizes a 16-sample window. */
        if (!sensor_sim_generate(sequence, monitor_time_ms(), &sample)) {
            ESP_LOGE(TAG, "Sensor generation failed");
        } else {
            sample.temperature_c = temperature_c;
            sample.vibration_rms_g = vibration_g;
            /* Legacy field name retained; this input is NOT measured RMS. */
            sample.current_rms_a = current_demo_a;
            if (xQueueSend(raw_queue, &sample, 0) != pdPASS) {
                dropped++;
                ESP_LOGW(TAG, "Raw queue full: dropped seq=%" PRIu32
                         " total_dropped=%" PRIu32, sequence, dropped);
            }
        }
        sequence++;
        if (xTaskDelayUntil(&last_wake, interval) != pdTRUE) {
            ESP_LOGW(TAG, "Acquisition schedule overrun");
            vTaskDelay(interval);
            last_wake = xTaskGetTickCount();
        }
    }
}

static void validation_task(void *parameters)
{
    (void)parameters;
    sensor_sample_t sample = {0};

#if MONITOR_TEST_VALIDATION_STALL_ENABLED
    bool stall_injected = false;
#endif

    while (1) {
        if (xQueueReceive(raw_queue, &sample,
                         pdMS_TO_TICKS(RECEIVE_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGW(TAG, "Validation: no raw sample within 3000 ms");
            continue;
        }

#if MONITOR_TEST_VALIDATION_STALL_ENABLED
        if (!stall_injected &&
            sample.sequence == MONITOR_TEST_STALL_SEQUENCE) {
            stall_injected = true;
            ESP_LOGW(TAG, "TEST: pausing validation for %" PRIu32 " ms",
                     (uint32_t)MONITOR_TEST_STALL_DURATION_MS);
            vTaskDelay(pdMS_TO_TICKS(MONITOR_TEST_STALL_DURATION_MS));
            ESP_LOGW(TAG, "TEST: validation resumed");
        }
#endif

        validated_sample_t result = {0};
        if (!data_validation_check(&sample, monitor_time_ms(),
                                   SAMPLE_MAX_AGE_MS, &result)) {
            ESP_LOGE(TAG, "Validation rejected its arguments");
            continue;
        }

        send_validated(logging_queue, &result, "Logging");
        send_validated(fault_queue, &result, "Fault");
        send_validated(anomaly_queue, &result, "AI");

        if (xQueueOverwrite(communication_sample, &result) != pdPASS) {
            ESP_LOGE(TAG, "Communication sample mailbox write failed");
        }
    }
}

static void logging_task(void *parameters)
{
    (void)parameters;
    validated_sample_t result = {0};

    while (1) {
        if (xQueueReceive(logging_queue, &result,
                         pdMS_TO_TICKS(RECEIVE_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGW(TAG, "Logging: no validated record within 3000 ms");
            continue;
        }
        if (result.issues != VALIDATION_OK) {
            ESP_LOGW(TAG, "INVALID seq=%" PRIu32 " issues=0x%08" PRIx32,
                     result.sample.sequence, result.issues);
            continue;
        }
        ESP_LOGI(TAG, "VALID seq=%" PRIu32 " time_ms=%" PRIu64
                 " temp_c=%.2f vibration_g=%.3f current_a=%.2f",
                 result.sample.sequence, result.sample.timestamp_ms,
                 (double)result.sample.temperature_c,
                 (double)result.sample.vibration_rms_g,
                 (double)result.sample.current_rms_a);
    }
}

static void fault_task(void *parameters)
{
    (void)parameters;
    validated_sample_t input = {0};
    fault_manager_t manager;
    uint32_t previous_sequence = 0U;
    bool has_previous = false;

    if (!fault_manager_init(&manager)) {
        ESP_LOGE(TAG, "Fault manager initialization failed");
        abort();
    }

    while (1) {
        if (xQueueReceive(fault_queue, &input,
                         pdMS_TO_TICKS(RECEIVE_TIMEOUT_MS)) != pdTRUE) {
            (void)fault_manager_update(&manager, NULL);
            ESP_LOGW(TAG, "Fault task: input timeout; persistence reset");
            continue;
        }

        const uint32_t sequence = input.sample.sequence;
        const bool consecutive =
            !has_previous || sequence == (uint32_t)(previous_sequence + 1U);

        if (!consecutive) {
            (void)fault_manager_update(&manager, NULL);
            ESP_LOGW(TAG, "Fault input sequence discontinuity: previous=%"
                     PRIu32 " current=%" PRIu32,
                     previous_sequence, sequence);
        }

        previous_sequence = sequence;
        has_previous = true;

        validated_sample_t checked = {0};
        if (!data_validation_check(&input.sample, monitor_time_ms(),
                                   SAMPLE_MAX_AGE_MS, &checked)) {
            ESP_LOGE(TAG, "Fault input revalidation failed");
            continue;
        }

        checked.issues |= input.issues;

        const monitor_state_t previous_state = manager.state;
        const bool evaluated = fault_manager_update(&manager, &checked);

        const fault_result_t result = {
            .sequence = sequence,
            .timestamp_ms = input.sample.timestamp_ms,
            .state = manager.state,
            .status = evaluated ? PROCESSING_COMPLETE
                                : PROCESSING_INPUT_INVALID,
            .validation_issues = checked.issues
        };

        if (manager.state != previous_state) {
            ESP_LOGI(TAG, "STATE seq=%" PRIu32 " %s -> %s",
                     sequence, state_name(previous_state),
                     state_name(manager.state));
        }

        if (xQueueOverwrite(communication_fault, &result) != pdPASS) {
            ESP_LOGE(TAG, "Fault result mailbox write failed");
        }
    }
}

static void anomaly_task(void *parameters)
{
    (void)parameters;
    validated_sample_t input = {0};

    while (1) {
        if (xQueueReceive(anomaly_queue, &input,
                         pdMS_TO_TICKS(RECEIVE_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGW(TAG, "AI task: input timeout");
            continue;
        }

        validated_sample_t checked = {0};
        if (!data_validation_check(&input.sample, monitor_time_ms(),
                                   SAMPLE_MAX_AGE_MS, &checked)) {
            ESP_LOGE(TAG, "AI input revalidation failed");
            continue;
        }

        checked.issues |= input.issues;
        anomaly_detection_t detection = {0};
        const bool evaluated =
            anomaly_detector_evaluate(&checked, &detection);

        const anomaly_result_t result = {
            .sequence = input.sample.sequence,
            .timestamp_ms = input.sample.timestamp_ms,
            .status = evaluated ? PROCESSING_COMPLETE
                                : PROCESSING_INPUT_INVALID,
            .score = detection.score,
            .is_anomaly = detection.is_anomaly
        };

        if (xQueueOverwrite(communication_anomaly, &result) != pdPASS) {
            ESP_LOGE(TAG, "AI result mailbox write failed");
        }
    }
}

static void publish_snapshot(const communication_snapshot_t *snapshot)
{
    const validated_sample_t *record = &snapshot->sample;
    const sensor_sample_t *sample = &record->sample;
    const uint64_t now = monitor_time_ms();

    if (record->issues != VALIDATION_OK ||
        snapshot->fault.status != PROCESSING_COMPLETE ||
        snapshot->anomaly.status != PROCESSING_COMPLETE ||
        sample->timestamp_ms > now ||
        now - sample->timestamp_ms > SAMPLE_MAX_AGE_MS ||
        !isfinite(sample->temperature_c) ||
        !isfinite(sample->vibration_rms_g) ||
        !isfinite(sample->current_rms_a) ||
        !isfinite(snapshot->anomaly.score)) {
        ESP_LOGW(TAG, "TELEMETRY skipped invalid/stale seq=%" PRIu32,
                 sample->sequence);
        return;
    }

    firmware_update_note_healthy(sample->sequence);

    if (!communication_is_connected()) {
        ESP_LOGW(TAG, "TELEMETRY skipped offline seq=%" PRIu32,
                 sample->sequence);
        return;
    }

    char payload[COMMUNICATION_MAX_PAYLOAD_BYTES + 1U];
    const int length = snprintf(
        payload, sizeof(payload),
        "{\"schema_version\":1,\"device_id\":\"%s\","
        "\"sequence\":%" PRIu32 ",\"timestamp_ms\":%" PRIu64 ","
        "\"temperature_c\":%.3f,\"vibration_rms_g\":%.6f,"
        "\"current_rms_a\":%.3f,\"state\":\"%s\","
        "\"fault_status\":\"%s\",\"validation_issues\":%" PRIu32 ","
        "\"ai_status\":\"%s\",\"anomaly_score\":%.6f,"
        "\"is_anomaly\":%s}",
        COMM_DEVICE_ID, sample->sequence, sample->timestamp_ms,
        (double)sample->temperature_c,
        (double)sample->vibration_rms_g,
        (double)sample->current_rms_a,
        state_name(snapshot->fault.state),
        processing_status_name(snapshot->fault.status),
        snapshot->fault.validation_issues,
        processing_status_name(snapshot->anomaly.status),
        (double)snapshot->anomaly.score,
        snapshot->anomaly.is_anomaly ? "true" : "false");

    if (length < 0 || (size_t)length >= sizeof(payload)) {
        ESP_LOGE(TAG, "Telemetry formatting failed or exceeded capacity");
        return;
    }

    const int message_id = communication_enqueue(payload, (size_t)length);
    if (message_id < 0) {
        ESP_LOGW(TAG, "TELEMETRY enqueue rejected seq=%" PRIu32
                 " result=%d", sample->sequence, message_id);
    } else {
        ESP_LOGI(TAG, "TELEMETRY queued seq=%" PRIu32 " msg_id=%d",
                 sample->sequence, message_id);
    }
}

static void communication_task(void *parameters)
{
    (void)parameters;
    communication_snapshot_t snapshot = {0};
    uint32_t last_reported_sequence = 0U;
    bool has_reported = false;

    while (1) {
        communication_test_poll();

        if (xQueueReceive(communication_sample,
                          &snapshot.sample, 0) == pdTRUE) {
            snapshot.has_sample = true;
        }
        if (xQueueReceive(communication_fault,
                          &snapshot.fault, 0) == pdTRUE) {
            snapshot.has_fault = true;
        }
        if (xQueueReceive(communication_anomaly,
                          &snapshot.anomaly, 0) == pdTRUE) {
            snapshot.has_anomaly = true;
        }

        if (snapshot.has_sample && snapshot.has_fault &&
            snapshot.has_anomaly) {
            const uint32_t sequence = snapshot.sample.sample.sequence;
            const bool matched =
                sequence == snapshot.fault.sequence &&
                sequence == snapshot.anomaly.sequence;

            if (matched &&
                (!has_reported || sequence != last_reported_sequence)) {
                ESP_LOGI(TAG, "LOCAL_SNAPSHOT seq=%" PRIu32
                         " state=%s fault=%s issues=0x%08" PRIx32
                         " ai=%s score=%.6f anomaly=%d"
                         " transport=MQTT",
                         sequence, state_name(snapshot.fault.state),
                         processing_status_name(snapshot.fault.status),
                         snapshot.fault.validation_issues,
                         processing_status_name(snapshot.anomaly.status),
                         (double)snapshot.anomaly.score,
                         (int)snapshot.anomaly.is_anomaly);
                publish_snapshot(&snapshot);

                last_reported_sequence = sequence;
                has_reported = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

static void create_monitor_task(TaskFunction_t function,
                                const char *name,
                                UBaseType_t priority)
{
    if (task_record_count >= MONITOR_TASK_COUNT) {
        ESP_LOGE(TAG, "Startup failed: task registry full");
        abort();
    }
    task_record_t *record = &task_records[task_record_count];
    record->name = name;

    if (xTaskCreate(function, name, TASK_STACK_BYTES,
                    NULL, priority, &record->handle) != pdPASS) {
        ESP_LOGE(TAG, "Startup failed: cannot create task %s", name);
        abort();
    }
    task_record_count++;
}

static void report_task_stacks(void)
{
    for (size_t index = 0U; index < task_record_count; index++) {
        task_health_log(task_records[index].handle,
                        task_records[index].name, TASK_STACK_BYTES);
    }
}

void app_main(void)
{
    firmware_update_boot_check();
    ESP_LOGI(TAG, "ESP32 Industrial Monitor");
    ESP_LOGI(TAG, "Validation stall test enabled: %d",
             MONITOR_TEST_VALIDATION_STALL_ENABLED);
    ESP_LOGI(TAG, "Fault thresholds are demonstration settings");
    ESP_LOGI(TAG, "Statistical inference active; MQTT telemetry configured");

    raw_queue = create_queue(QUEUE_LENGTH, sizeof(sensor_sample_t));
    logging_queue = create_queue(QUEUE_LENGTH, sizeof(validated_sample_t));
    fault_queue = create_queue(QUEUE_LENGTH, sizeof(validated_sample_t));
    anomaly_queue = create_queue(QUEUE_LENGTH, sizeof(validated_sample_t));

    communication_sample = create_queue(1U, sizeof(validated_sample_t));
    communication_fault = create_queue(1U, sizeof(fault_result_t));
    communication_anomaly = create_queue(1U, sizeof(anomaly_result_t));

    communication_start();

    create_monitor_task(logging_task, "monitor_logging", 1U);
    create_monitor_task(communication_task, "monitor_comm", 1U);
    create_monitor_task(fault_task, "monitor_fault", 2U);
    create_monitor_task(anomaly_task, "monitor_ai", 2U);
    create_monitor_task(validation_task, "monitor_valid", 3U);
    create_monitor_task(acquisition_task, "monitor_acq", 4U);

    ESP_LOGI(TAG, "All six monitoring tasks created");
    firmware_update_start_demo();

    vTaskDelay(pdMS_TO_TICKS(15000U));
    ESP_LOGI(TAG, "Stack assessment: first observation");
    report_task_stacks();

    vTaskDelay(pdMS_TO_TICKS(15000U));
    ESP_LOGI(TAG, "Stack assessment: second observation");
    report_task_stacks();
}

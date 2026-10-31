#include "firmware_update.h"
#include "firmware_update_config.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "communication.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "https_update";

/* The rejected image remains in flash after rollback. Do not reinstall it. */
static bool image_matches_pin(const esp_partition_t *partition)
{
    uint8_t digest[32];
    char hex[65];
    static const char digits[] = "0123456789abcdef";
    if (partition == NULL || esp_partition_get_sha256(partition, digest) != ESP_OK) {
        return false;
    }
    for (size_t i = 0; i < sizeof(digest); i++) {
        hex[2 * i] = digits[digest[i] >> 4];
        hex[2 * i + 1] = digits[digest[i] & 15U];
    }
    hex[64] = '\0';
    return strcmp(hex, MONITOR_OTA_EXPECTED_IMAGE_SHA256) == 0;
}

static void download_task(void *argument)
{
    (void)argument;
    esp_https_ota_handle_t handle = NULL;
    esp_err_t result = ESP_FAIL;
    const int64_t ready_deadline = esp_timer_get_time() + 120000000LL;

    /* Wait only in this task; monitoring continues independently. */
    while (!communication_is_connected() || time(NULL) < 1767225600) {
        if (esp_timer_get_time() >= ready_deadline) {
            ESP_LOGW(TAG, "Update readiness timed out; keeping current image");
            goto done;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running == NULL) {
        goto done;
    }
    result = esp_ota_get_state_partition(running, &state);
    if ((result != ESP_OK && result != ESP_ERR_NOT_FOUND) ||
        (result == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY)) {
        ESP_LOGW(TAG, "Current image not ready for another update");
        goto done;
    }

    const esp_partition_t *rejected = esp_ota_get_last_invalid_partition();
    if (image_matches_pin(rejected)) {
        ESP_LOGW(TAG, "Previously rejected candidate detected; skipping reinstall");
        goto done;
    }

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL || target->address == running->address) {
        ESP_LOGE(TAG, "No inactive OTA target available");
        goto done;
    }

    const esp_app_desc_t *current = esp_app_get_description();
    if (strcmp(current->version, MONITOR_OTA_EXPECTED_VERSION) == 0) {
        ESP_LOGI(TAG, "Expected version already running; update skipped");
        goto done;
    }
    if (strncmp(MONITOR_OTA_URL, "https://", 8) != 0) {
        ESP_LOGE(TAG, "Update URL must use HTTPS");
        goto done;
    }

    const esp_http_client_config_t http = {
        .url = MONITOR_OTA_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .disable_auto_redirect = true
    };
    const esp_https_ota_config_t configuration = {
        .http_config = &http
    };
    ESP_LOGI(TAG, "Downloading candidate version %s", MONITOR_OTA_EXPECTED_VERSION);
    result = esp_https_ota_begin(&configuration, &handle);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS OTA begin failed: %s", esp_err_to_name(result));
        goto done;
    }

    esp_app_desc_t candidate;
    result = esp_https_ota_get_img_desc(handle, &candidate);
    if (result != ESP_OK ||
        memchr(candidate.project_name, '\0', sizeof(candidate.project_name)) == NULL ||
        memchr(candidate.version, '\0', sizeof(candidate.version)) == NULL ||
        strcmp(candidate.project_name, current->project_name) != 0 ||
        strcmp(candidate.version, MONITOR_OTA_EXPECTED_VERSION) != 0) {
        ESP_LOGE(TAG, "Candidate identity rejected");
        goto abort_download;
    }
    ESP_LOGI(TAG, "Candidate accepted: project=%s version=%s",
             candidate.project_name, candidate.version);

    const int64_t download_deadline = esp_timer_get_time() + 180000000LL;
    int last_report = 0;
    do {
        if (esp_timer_get_time() >= download_deadline) {
            ESP_LOGE(TAG, "Download deadline exceeded");
            goto abort_download;
        }
        result = esp_https_ota_perform(handle);
        const int received = esp_https_ota_get_image_len_read(handle);
        if (received - last_report >= 65536) {
            ESP_LOGI(TAG, "Downloaded %d bytes", received);
            last_report = received;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (result == ESP_ERR_HTTPS_OTA_IN_PROGRESS);

    if (result != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "Download incomplete or failed: %s", esp_err_to_name(result));
        goto abort_download;
    }

    uint8_t digest[32];
    char hex_digest[65];
    static const char digits[] = "0123456789abcdef";
    result = esp_partition_get_sha256(target, digest);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Downloaded image digest validation failed: %s",
                 esp_err_to_name(result));
        goto abort_download;
    }
    for (size_t index = 0; index < sizeof(digest); index++) {
        hex_digest[index * 2] = digits[digest[index] >> 4];
        hex_digest[index * 2 + 1] = digits[digest[index] & 15U];
    }
    hex_digest[64] = '\0';
    if (strcmp(hex_digest, MONITOR_OTA_EXPECTED_IMAGE_SHA256) != 0) {
        ESP_LOGE(TAG, "Candidate digest differs from the pinned image");
        goto abort_download;
    }
    ESP_LOGI(TAG, "Pinned candidate image digest verified");

    result = esp_https_ota_finish(handle);
    handle = NULL; /* finish releases the handle even on failure. */
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Image validation/finalization failed: %s", esp_err_to_name(result));
        goto done;
    }
    ESP_LOGI(TAG, "OTA image ready; rebooting into candidate");
    esp_restart();
    goto done;

abort_download:
    if (handle != NULL) {
        const esp_err_t cleanup = esp_https_ota_abort(handle);
        handle = NULL;
        if (cleanup != ESP_OK) {
            ESP_LOGW(TAG, "OTA cleanup: %s", esp_err_to_name(cleanup));
        }
    }
done:
    ESP_LOGI(TAG, "Update task ended; monitoring continues");
    vTaskDelete(NULL);
}

void firmware_update_start_demo(void)
{
    static bool started;
    if (started) {
        return;
    }
    started = true;
    if (!MONITOR_OTA_DEMO_ENABLED) {
        ESP_LOGI(TAG, "Automatic OTA demonstration disabled");
        return;
    }
    if (xTaskCreate(download_task, "https_update", 8192,
                    NULL, 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Cannot allocate update task; keeping current image");
    }
}

#include "firmware_update.h"
#include "firmware_update_config.h"

#include <stdbool.h>

#include "esp_err.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define HEALTHY_BIT BIT0
#define REQUIRED_RECORDS 10U

static const char *TAG = "firmware_update";
static EventGroupHandle_t health_events;

static void verification_task(void *argument)
{
    (void)argument;

    const EventBits_t bits = xEventGroupWaitBits(
        health_events, HEALTHY_BIT, pdFALSE, pdTRUE,
        pdMS_TO_TICKS(60000));

    if ((bits & HEALTHY_BIT) != 0U) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
        ESP_LOGI(TAG, "Updated image confirmed: pipeline health check passed");
    } else {
        ESP_LOGE(TAG, "Update health check timed out; requesting rollback");
        const esp_err_t result =
            esp_ota_mark_app_invalid_rollback_and_reboot();
        ESP_LOGE(TAG, "Rollback could not reboot: %s",
                 esp_err_to_name(result));
    }

    vTaskDelete(NULL);
}

void firmware_update_boot_check(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }

    ESP_LOGI(TAG, "Running version: %s", esp_app_get_description()->version);
    ESP_LOGI(TAG, "Running partition: %s", running->label);

    esp_ota_img_states_t state;
    const esp_err_t result =
        esp_ota_get_state_partition(running, &state);

    if (result == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "No OTA state recorded; baseline image");
        return;
    }
    ESP_ERROR_CHECK(result);

    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Image does not require pending-update verification");
        return;
    }

    health_events = xEventGroupCreate();
    if (health_events == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    ESP_LOGW(TAG, "Pending update: waiting for 10 healthy records within 60 seconds");

    if (xTaskCreate(verification_task, "ota_verify",
                    4096, NULL, 2, NULL) != pdPASS) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

void firmware_update_note_healthy(uint32_t sequence)
{
    static bool have_previous;
    static uint32_t previous;
    static unsigned count;

    if (MONITOR_OTA_TEST_REJECT_HEALTH ||
        health_events == NULL || count >= REQUIRED_RECORDS) {
        return;
    }

    if (!have_previous || sequence != previous + 1U) {
        count = 0U;
    }
    previous = sequence;
    have_previous = true;
    count++;

    if (count == REQUIRED_RECORDS) {
        xEventGroupSetBits(health_events, HEALTHY_BIT);
    }
}

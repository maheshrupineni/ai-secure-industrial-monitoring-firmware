#include "communication.h"
#include "communication_config.h"

#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#define WIFI_READY_BIT BIT0
#define MQTT_READY_BIT BIT1
#define READY_BITS (WIFI_READY_BIT | MQTT_READY_BIT)

static const char *TAG = "communication";
static EventGroupHandle_t connection_events;
static esp_mqtt_client_handle_t mqtt_client;


static void wifi_event_handler(void *argument, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)argument;

    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        const esp_err_t result = esp_wifi_connect();
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Wi-Fi connect request: %s",
                     esp_err_to_name(result));
        }
    } else if (base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(connection_events, READY_BITS);
        ESP_LOGW(TAG, "Wi-Fi disconnected; reconnecting");

        const esp_err_t result = esp_wifi_connect();
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Wi-Fi reconnect request: %s",
                     esp_err_to_name(result));
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        xEventGroupSetBits(connection_events, WIFI_READY_BIT);
        ESP_LOGI(TAG, "Wi-Fi IP: " IPSTR, IP2STR(&event->ip_info.ip));
        /* MQTT starts after time synchronization in its setup task. */
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        xEventGroupClearBits(connection_events, READY_BITS);
        ESP_LOGW(TAG, "Wi-Fi IP lost");
    }
}

static void mqtt_event_handler(void *argument, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)argument;
    (void)base;
    const esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(connection_events, MQTT_READY_BIT);
        ESP_LOGI(TAG, "MQTT connected over verified TLS");
        ESP_LOGI(TAG, "Telemetry topic: %s", COMM_TELEMETRY_TOPIC);
        break;

    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(connection_events, MQTT_READY_BIT);
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT broker acknowledged msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT error; client will retry connection");
        break;

    default:
        break;
    }
}


static void secure_connection_task(void *argument)
{
    (void)argument;

    xEventGroupWaitBits(connection_events, WIFI_READY_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    esp_sntp_config_t time_configuration =
        ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    ESP_ERROR_CHECK(esp_netif_sntp_init(&time_configuration));

    ESP_LOGI(TAG, "Waiting for SNTP time synchronization before TLS");

    for (;;) {
        const esp_err_t result =
            esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));

        if (result == ESP_OK) {
            const time_t now = time(NULL);
            struct tm utc;
            char formatted[32];

            if (gmtime_r(&now, &utc) != NULL &&
                utc.tm_year >= (2026 - 1900) &&
                strftime(formatted, sizeof(formatted),
                         "%Y-%m-%dT%H:%M:%SZ", &utc) != 0U) {
                ESP_LOGI(TAG, "Time synchronized: %s", formatted);
                break;
            }

            ESP_LOGW(TAG, "Synchronized time is implausible; waiting");
        } else {
            ESP_LOGW(TAG, "Time synchronization pending: %s",
                     esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    xEventGroupWaitBits(connection_events, WIFI_READY_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Starting MQTT TLS with certificate verification");
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
    vTaskDelete(NULL);
}

void communication_start(void)
{
    if (connection_events != NULL) {
        ESP_LOGE(TAG, "Communication initialized more than once");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }

    connection_events = xEventGroupCreate();
    if (connection_events == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    /*
     * Fail explicitly rather than automatically erase existing NVS
     * if initialization reports a partition/version problem.
     */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    const wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initialization));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = COMM_WIFI_SSID,
            .password = COMM_WIFI_PASSWORD,
            .channel = COMM_WIFI_CHANNEL,
            .threshold.authmode = WIFI_AUTH_OPEN
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration));

    const esp_mqtt_client_config_t mqtt_configuration = {
        .broker.address.uri = COMM_BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = COMM_DEVICE_ID,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 5000,
        .network.timeout_ms = 15000,
        .outbox.limit = 4096
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_configuration);
    if (mqtt_client == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    if (xTaskCreate(secure_connection_task, "comm_tls_setup",
                    4096, NULL, 2, NULL) != pdPASS) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi/MQTT started; connection is asynchronous");
}

bool communication_is_connected(void)
{
    if (connection_events == NULL) {
        return false;
    }

    return (xEventGroupGetBits(connection_events) & READY_BITS) == READY_BITS;
}

int communication_enqueue(const char *payload, size_t length)
{
    if (payload == NULL || length == 0U ||
        length > COMMUNICATION_MAX_PAYLOAD_BYTES ||
        !communication_is_connected()) {
        return -1;
    }

    return esp_mqtt_client_enqueue(
        mqtt_client, COMM_TELEMETRY_TOPIC, payload,
        (int)length, 1, 0, true);
}


void communication_test_poll(void)
{
#if COMM_TEST_OUTAGE_ENABLED
    static unsigned stage;
    static int64_t deadline_ms;
    const int64_t now_ms = esp_timer_get_time() / 1000;

    if (stage == 0U) {
        if (communication_is_connected()) {
            deadline_ms = now_ms + 5000;
            stage = 1U;
            ESP_LOGW(TAG, "TEST: MQTT interruption scheduled in 5 seconds");
        }
    } else if (stage == 1U && now_ms >= deadline_ms) {
        xEventGroupClearBits(connection_events, MQTT_READY_BIT);
        ESP_ERROR_CHECK(esp_mqtt_client_stop(mqtt_client));
        deadline_ms = esp_timer_get_time() / 1000 + 10000;
        stage = 2U;
        ESP_LOGW(TAG, "TEST: MQTT stopped for 10 seconds");
    } else if (stage == 2U && now_ms >= deadline_ms) {
        ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
        stage = 3U;
        ESP_LOGW(TAG, "TEST: MQTT restarted; waiting for connection");
    }
#endif
}

#ifndef INDUSTRIAL_COMMUNICATION_H
#define INDUSTRIAL_COMMUNICATION_H

#include <stdbool.h>
#include <stddef.h>

#define COMMUNICATION_MAX_PAYLOAD_BYTES 512U

/*
 * Call once from app_main before creating monitoring tasks.
 * Initializes Wi-Fi and MQTT; does not wait for connection.
 * Initialization failures abort through ESP_ERROR_CHECK.
 */
void communication_start(void);

/* True only while Wi-Fi has an IP address and MQTT is connected. */
bool communication_is_connected(void);

/*
 * Queue a payload for QoS 1 publishing without retention.
 * Call only from the communication task.
 * Returns message ID on enqueue success, negative on rejection.
 *
 * Success means queued locally, not delivered to the receiver.
 * Payload must remain valid during this call; MQTT copies it.
 * Disconnected calls are rejected instead of accumulating offline data.
 */
int communication_enqueue(const char *payload, size_t length);

/* Development test: call only from the communication task. */
void communication_test_poll(void);

#endif /* INDUSTRIAL_COMMUNICATION_H */

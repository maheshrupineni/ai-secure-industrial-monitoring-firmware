#ifndef INDUSTRIAL_COMMUNICATION_CONFIG_H
#define INDUSTRIAL_COMMUNICATION_CONFIG_H

/* Public demonstration settings: synthetic telemetry only. */
#define COMM_WIFI_SSID "Wokwi-GUEST"
#define COMM_WIFI_PASSWORD ""
#define COMM_WIFI_CHANNEL 6
#define COMM_BROKER_URI "mqtts://broker.hivemq.com:8883"
#define COMM_DEVICE_ID "monitor-5b90e0b212964632"
#define COMM_TELEMETRY_TOPIC "industrial-monitor-demo/monitor-5b90e0b212964632/telemetry"

/* Disable after verifying the controlled MQTT restart. */
#define COMM_TEST_OUTAGE_ENABLED 0

#endif /* INDUSTRIAL_COMMUNICATION_CONFIG_H */

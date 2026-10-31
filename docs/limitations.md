# Scope and limitations

- Wokwi simulated peripherals only; no physical wiring, sensor calibration or machine data validation.
- Potentiometer ADC level is a current proxy, not AC RMS current. ADC disconnect/floating detection is not implemented.
- MPU6050 window RMS removes constant acceleration/gravity. Nominal timing has no FIFO/data-ready synchronization; jitter, duplicates, aliasing and earlier timeouts limit interpretation.
- DS18B20 UART1 works in the observed setup. Earlier RMT detection failures and transient CRC errors have no proven root cause.
- Temperature conversion plus acceleration acquisition is close to the one-second schedule. Host checks do not prove scheduling deadlines or worst-case CPU load.
- The statistical model uses 60 normal training records, 17 selected calibration records and only two held test conditions. Some intended normal settings may flag. No general accuracy, remaining-life prediction or fault-cause diagnosis is established.
- Public MQTT TLS verifies the server but does not authenticate devices or isolate topics. Offline telemetry is skipped; QoS 1 can duplicate delivery. There is no boot-session identifier or persistent delivery history.
- OTA image checks and health rollback do not substitute for signed firmware, Secure Boot or flash encryption. Historical hosting was deleted. An earlier OTA idle-watchdog warning and several negative TLS/OTA cases remain unresolved.
- CI is prepared, not demonstrated. Physical hardware, long-duration stress, network reliability and industrial safety validation remain separate work.

Completing documentation and release cleanup does not remove these limits or imply production readiness.

# Temperature input

DS18B20 DQ connects to GPIO4 with a 4.7 kΩ pull-up to 3.3 V and common ground. The active driver uses Espressif's single-device DS18B20 API through the 1-Wire UART1 backend. UART0 remains the console. Conversion blocks before reading the temperature. Acquisition timestamps completion and publishes NAN on failure; there is no generated fallback.

Initialization owns persistent bus/device handles and retries are not performed after an initialization failure without reboot. Read errors can recover on subsequent cycles. Values must be finite and within the device range before downstream validation. Simulator observations include steady 42.5°C, response to changes and 65°C warning/anomaly with recovery. Earlier RMT detection and transient CRC errors remain unexplained. This is simulated sensor behavior, not physical validation.

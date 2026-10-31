# Verification record

Author: Mahesh Rupineni. Updated: 31 October 2026.

| Area | Evidence | Scope / remaining work |
|---|---|---|
| Host logic | 7 validation, 26 fault, 7 anomaly API, 10 vibration RMS, 10 current scaling, 10 capture decoder checks; component integration | Portable logic, not ESP peripheral timing |
| Model parity | 120 held-record C/Python decisions match; model export/header consistency | Two mostly constant simulated scenarios |
| ESP32 build | User ran verification with --firmware successfully after model installation | Final cleaned Mac build also passed |
| Temperature | DS18B20 42.5°C baseline, 65°C warning and return verified | UART backend; earlier transient CRC errors remain unexplained |
| Vibration | Stationary zero; manual acceleration changes produce nonzero windows | Occasional timing timeout; no repeatable motor waveform evaluation |
| Current proxy | Potentiometer changes reflected in scaled ADC field; high proxy drove FAULT in an earlier run | Not a physical current sensor |
| Updated model live | Baseline score 0.311085, anomaly=0; 65°C score 4726.563965, anomaly=1 | Limited normal envelope and evaluation data |
| Recovery live | RECOVERY -> NORMAL at sequence 72; NORMAL through sequence 84 with zero validation issues | Same temperature test, not independent field validation |
| MQTT | Verified TLS connection, queued telemetry and matching broker acknowledgements | Acknowledgement is not independent receiver validation; reconnects and offline gaps remain |
| OTA historical | 0.1.0 -> 0.1.1 update/confirmation; deliberate 0.1.2 rejection and rollback observed earlier | Older firmware/candidates; deleted hosting unavailable; no final sensor-image repeat yet |
| CI | Workflow supplied | Host CI passed; first fresh firmware CI exposed an undefined optional DS18B20 sensor-hub flag; explicit disabled Kconfig compatibility setting added; rerun pending |

The latest normal -> 65°C warning/anomaly -> recovery -> normal sequence was supplied as user terminal logs. Historical OTA results were observed earlier in the project, not reproduced by host verification. The cleanup package preserves driver/model bytes and changes only the startup banner among active firmware C sources.

The user ran `python3 scripts/verify.py --firmware` successfully after cleanup. The final fresh Wokwi log showed the ESP32 Industrial Monitor banner, NORMAL snapshots through sequence 12, zero validation issues, score 0.311085, anomaly=0, verified TLS and broker acknowledgements. No sensor errors appeared in that supplied short log. These results do not close the limitations below. Follow demo.md for a combined recording if desired.

Open observations: earlier CPU1 idle-watchdog warning during OTA, transient DS18B20 CRC errors while controls changed, MPU6050 timing timeout, public broker reconnects, and incomplete negative OTA/TLS tests. No hardware test, field accuracy, industrial safety or hard-real-time guarantee is claimed.

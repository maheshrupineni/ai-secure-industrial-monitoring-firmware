# AI-Based Secure Industrial Monitoring Firmware

**Mahesh Rupineni**

[![Verify industrial monitor](https://github.com/maheshrupineni/ai-secure-industrial-monitoring-firmware/actions/workflows/verify.yml/badge.svg)](https://github.com/maheshrupineni/ai-secure-industrial-monitoring-firmware/actions/workflows/verify.yml)

A completed ESP32 monitoring prototype built with ESP-IDF and FreeRTOS, demonstrated in Wokwi. It acquires temperature and acceleration, reads a potentiometer as a current proxy, validates records, evaluates threshold states and a statistical anomaly model, and publishes telemetry over certificate-verified MQTT TLS. HTTPS OTA and rollback support are included with automatic updates disabled.

## Features

- Six monitoring tasks with bounded queues and sequence-matched result mailboxes.
- DS18B20 temperature input, MPU6050 acceleration-window RMS, and ADC current proxy.
- Finite-value, plausibility and timestamp checks before processing.
- STARTUP, NORMAL, WARNING, FAULT and RECOVERY states with persistence and hysteresis.
- Embedded statistical inference with reproducible Python fitting and C/Python parity checks.
- TLS telemetry with asynchronous connection handling and QoS 1 acknowledgements.
- Dual OTA partitions, candidate checks and pending-image health confirmation.
- Portable C tests, Python checks and a prepared GitHub Actions workflow.

This is a completed simulation demonstration within the scope described below. It is not a validated industrial protection or predictive-maintenance product.

## Build and run

Use ESP-IDF v6.1 for the original ESP32, Python 3, a host C compiler, VS Code and the Wokwi Simulator extension. On macOS the host checks select the Apple compiler through xcrun. Activate your ESP-IDF environment first, then from the project root:

```bash
python3 scripts/verify.py --firmware
```

This runs host checks, builds the application and creates `firmware/build/uf2.bin`. Dependencies are resolved using the component manifest and lockfile; internet access is required on a fresh build. Wokwi extension activation/licensing is separate; do not commit a license key.

Open this project folder in VS Code. Choose **View → Command Palette → Wokwi: Start Simulator**, then select `firmware/wokwi.toml` if prompted. Stop and start the simulator after rebuilding so it reloads the new firmware. Run only one instance for this device ID.

Initial controls: temperature **42.5°C**, acceleration **X=0, Y=0, Z=1**, potentiometer value **250** (approximately 2.44 A-equivalent in this simulation). Stationary acceleration yields near-zero vibration because the window mean, including gravity, is removed. A fixed nonzero acceleration is not continuous vibration.

For a separate TLS receiver:

```bash
python3 -m venv python/.capture-venv
./python/.capture-venv/bin/python -m pip install -r python/requirements.txt
./python/.capture-venv/bin/python python/scripts/mqtt_receiver_tls.py
```

Firmware network settings are in `firmware/components/communication/include/communication_config.h`; the receiver uses `simulation/mqtt_settings.json`. Keep host and topic consistent. Public broker messages are visible to others and can be spoofed; certificate verification authenticates the server, not publishing devices. Offline records are skipped, not durably buffered.

## Wiring

All inputs below are simulated components, powered from 3.3 V with common ground.

| Input | ESP32 connection | Configuration |
|---|---|---|
| DS18B20 DQ | GPIO4 | 4.7 kΩ pull-up to 3.3 V; UART1 backend |
| MPU6050 SDA / SCL | GPIO21 / GPIO22 | Address 0x68; AD0 grounded |
| Potentiometer SIG | GPIO34 / ADC1 channel 6 | 12-bit ADC; 0–10 A-equivalent proxy |

The legacy telemetry field `current_rms_a` contains a scaled potentiometer level, **not measured AC RMS current**. Vibration is the square root of summed per-axis population variances over 16 samples, not an industrial vibration spectrum.

## Verification and demonstration

See [verification](docs/verification.md) for observed results and [demo procedure](docs/demo.md) for a combined run. Local host checks and the final cleaned ESP32 firmware build passed. The updated model's baseline, 65°C warning/anomaly and return through RECOVERY to NORMAL were observed in Wokwi. The final cleaned firmware passed a fresh Wokwi baseline check: NORMAL state, zero validation issues, anomaly=0, verified TLS connection and MQTT broker acknowledgements. GitHub Actions passed both host verification and the fresh ESP32 build, including UF2 packaging and firmware artifact upload. [Verified run](https://github.com/maheshrupineni/ai-secure-industrial-monitoring-firmware/actions/runs/41179300683).

## Embedded anomaly model

The score is the mean of three squared standardized features, with an anomaly decision strictly above the calibrated threshold. It is statistical inference, not an LLM or cloud AI service. Current parameters use 60 normal training records and 17 selected calibration records. Evaluation covers 60 constant baseline and 60 constant 65°C records; these two scenarios do not establish broad detection accuracy.

See [model report](docs/sensor_model_recalibration.md). The completed embedded model is included as a generated C header with matching parameters, datasets and evaluation results. Its parameter-generation workflow is reproducible:

```bash
python3 python/scripts/prepare_sensor_datasets.py
python3 python/scripts/train_model.py
python3 python/scripts/export_model.py
python3 python/scripts/evaluate_model.py
python3 scripts/verify.py
```

Preparation verifies reviewed raw-capture hashes and explicit selection rules. The release excludes the legacy plaintext receiver and synthetic dataset generator; the active model is reproduced from reviewed sensor captures.

## OTA and limitations

See [OTA](docs/ota.md) and [limitations](docs/limitations.md). Earlier update and deliberate rollback demonstrations passed with older firmware. Their download repository was deleted, so the configured historical URL is unavailable. Automatic OTA remains off. A final-firmware OTA demonstration needs a newly hosted candidate and reviewed version/image pin; no new hosting or publication is performed by this package.

## Source layout

`firmware/` contains the application, drivers and build configuration; `python/` contains datasets, model and scripts; `tests/` contains host checks; `scripts/` contains verification and source packaging; `docs/` contains current design and evidence. Local backups and accidentally nested project copies are excluded from release packaging.

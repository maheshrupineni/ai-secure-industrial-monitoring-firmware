# Sensor capture and labels

Author: Mahesh Rupineni.

The collector subscribes to the firmware-configured public MQTT TLS topic using a unique client ID and default certificate verification. It records operator-labelled runs; labels do not come from model predictions or fault states. The five reviewed captures are retained under python/datasets/sensor_runs, and selection/exclusions are documented in sensor_model_recalibration.md.

Create a Python virtual environment and install python/requirements.txt as described in README.md. Example for a NEW capture (replace the ID with one not already used):

```bash
./python/.capture-venv/bin/python python/scripts/collect_sensor_run.py --run new_normal_train_01 --split train --label normal --scenario commissioning_normal_variation --samples 60
```

Start or resume exactly one Wokwi instance after SUBSCRIBED. Keep controls within an explicitly chosen normal operating plan for normal labels; abnormal labels are restricted to test captures. The collector refuses reused IDs, retained messages, malformed/oversized payloads, wrong device/schema, invalid-quality/nonfinite features and duplicate/out-of-order sequences. Restart/order failures stop incomplete capture. Complete captures save CSV and JSON metadata; interrupted runs retain partial data. A disconnect message during intentional shutdown after reaching the target is not a failed save.

Run IDs and capture times differ, but payloads have no boot-session identifier. Public topics can be spoofed; TLS server verification does not authenticate the device. Ten decoder tests and actual Mac MQTT captures were observed. Preserve raw data, review exclusions, keep fitting/calibration separate from held evaluation and never tune a model to hide test failures. Collection alone does not train or export a model. Constant-only runs do not provide training variation.

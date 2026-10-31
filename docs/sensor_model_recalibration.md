# Provisional sensor-model recalibration

Author: Mahesh Rupineni
Date: 31 October 2026
Scope: Wokwi commissioning demonstration; no real-machine accuracy claim.
Status: model fitting, export, host checks, Mac firmware build, live temperature baseline/anomaly/recovery checks and final cleanup smoke check passed. See verification.md for evidence and scope.

## Reviewed data and exclusions

The model uses 60 operator-labelled training records from normal_train_01. It uses 17 records from normal_calibration_02 that satisfy the previously specified 41–44C and 2–3A-equivalent proxy normal conditions. The small-motion collection gate is 0–0.04g (all accepted calibration rows actually have 0g). Forty-three replacement calibration records outside the intended temperature/current ranges are excluded, not relabelled abnormal. The entire 60-record normal_calibration_01 capture is quarantined after the operator confirmed accidentally moving temperature above 90C. Those raw captures are preserved unmodified; sensor_selection_audit.csv records inclusion/exclusion reasons and source row/sequence references.

The previous generated datasets are replaced only as active working train/calibration/test files and preserved in the installer's backup. Raw sensor_runs files are not modified or deleted. reviewed_sensor_runs.json pins every reviewed CSV/metadata pair by SHA256; the preparation script and installer stop if reviewed inputs differ. A future data revision requires an explicit new review and selection plan.

The two held test captures are normal_test_01 (60 constant baseline records) and temperature_abnormal_test_01 (60 constant 65C records). Their values and labels are not used for means, standard deviations or threshold fitting. Test rows are not filtered by the normal-range gates. Labels come from the operator's test conditions, not AI predictions or state-machine output. Capture IDs and times differ, but the telemetry has no boot-session identifier; strict independent device-session provenance is not established.

## Method and parameters

The existing diagonal standardized squared-distance method is retained: score is the mean of squared standardized features; anomaly if score strictly exceeds threshold. Mean and sample standard deviation are fitted on training rows only. No invented noise, variance floors or test-based parameter tuning were introduced.

Feature order: temperature_c, vibration_rms_g, current_rms_a.

Training means:
- temperature: 42.46665 C
- vibration: 0.0018648666666666667 g
- current proxy: 2.3570333333333333 A-equivalent

Training standard deviations:
- temperature: 0.18923707890870195
- vibration: 0.005847745836794842
- current proxy: 0.09769859681289716

Threshold: 16.791719635777017 in Python; exported float32 about 16.791719437.

The existing 99th nearest-rank percentile rule on only 17 selected calibration records yields their maximum. Zero flags among those 17 does NOT establish a 1% false-positive rate. The small data volume, manual changes, time correlation, narrow conditions and selected calibration subset limit the result. Training covers 41.5–43C, not every point in the wider intended 41–44C range; some intended normal settings may still be flagged. No general normal-envelope guarantee is made.

## Results and limits

On the fixed held captures:
- 60/60 baseline records accepted.
- 60/60 held 65C records flagged.
- No false positives or false negatives on those two scenarios.

Those are 120 samples of two mostly constant scenarios, not 120 diverse fault cases. The corresponding 100% empirical metrics in evaluation_report.json apply only to these captures. Current-related abnormal scenarios and repeatable vibration waveforms have not been evaluated. These results do not demonstrate predictive maintenance, fault diagnosis, real current measurement or industrial detection accuracy.

The firmware and host C model agreed with Python on all 120 test decisions, with zero scores outside the existing relative/absolute tolerance. Maximum absolute score difference was about 0.000951682239 on a score around 4726; this is expected float32 rounding. The model JSON and C header hash/export consistency check passed. Existing 7 validation, 26 fault, 7 anomaly API, 10 vibration RMS, 10 current scale and 10 capture-decoder checks passed; component integration also passed.

The generator integration fixture is retained for plumbing/finite-score and fault-state tests, but no longer asserts that every historical generated sample is normal for the new sensor model. Explicit captured-baseline and held-65C classifications are added. This is a deliberate change in the test's operating-distribution assumption, not a disabled numerical/fault test. Old tests are preserved in the installer's backup.

## Reproduce and verify

From project root:

```
python3 python/scripts/prepare_sensor_datasets.py
python3 python/scripts/train_model.py
python3 python/scripts/export_model.py
python3 python/scripts/evaluate_model.py
python3 scripts/verify.py
```

Preparation uses the fixed whitelist/rules and reviewed input hashes. Training reads train/calibration only; evaluation reads the fixed test file after fitting/export. Exporter and evaluation metadata now identify Wokwi commissioning captures rather than synthetic generated rows. The collected data are still simulated, not physical measurements.

After installation, stop Wokwi and run `python3 scripts/verify.py --firmware` in the active ESP-IDF v6.1 environment. Start fresh with temperature 42.5C, stationary MPU6050 (X=0,Y=0,Z=1) and initial potentiometer near 2.44A-equivalent. Observed live baseline score was 0.311085 with anomaly=0 (the exact ADC value affects the score). Check a later controlled 65C run separately; do not tune on that already-used held test. More independent normal and abnormal captures are required for credible broader evaluation.

Current remains a potentiometer ADC proxy. The legacy current_rms_a name is not measured AC RMS. Earlier temperature CRC, vibration timing timeout and OTA watchdog observations remain open. The previous absence of errors in a short run does not resolve them. Firmware deployment, GitHub publishing and CI execution are not performed by this package. OTA remains disabled; deleted download URLs remain unavailable.

## Backup and rollback

Installer saves all overwritten files under docs/model_before_sensor_recalibration.<unique>. That includes the old model/header, datasets, scripts and integration test. New-only files are listed in the backup manifest. To reverse the update, restore overwritten files together from the backup and rebuild; restoring only the model header would leave model/dataset/script verification inconsistent. No raw capture is overwritten. Keep backups local and exclude them when eventually publishing.

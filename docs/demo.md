# Combined demonstration

Use one simulation and one continuous recording where practical. No retraining or firmware changes are needed between controls. Use Wokwi terminal values rather than knob angles. This is a functional demonstration, not a new independent model test dataset.

1. Build with `python3 scripts/verify.py --firmware`, then start a fresh Wokwi simulation. Keep 42.5°C, X=0/Y=0/Z=1 and initial potentiometer value 250. Wait for TLS connection. Expect valid NORMAL records, anomaly=0 and broker acknowledgements.
2. Set temperature to 65°C and hold it. After three consecutive valid warning-level samples expect WARNING; the anomaly flag should be 1. Keep the other controls unchanged.
3. Restore 42.5°C and hold it for at least ten valid samples. Expect RECOVERY then NORMAL, anomaly=0. This path has already been observed with the updated model.
4. Move MPU6050 X back and forth briefly within its control range. Some acquisition windows should report nonzero vibration; holding a fixed value should return near zero. Restore X=0/Y=0/Z=1. Do not interpret manual slider movement as a calibrated motor vibration test or require a particular AI decision.
5. Increase the potentiometer until terminal current_a is above 8 A-equivalent; hold for at least two valid records to demonstrate FAULT. Restore near 2.44 and hold for at least ten valid records: FAULT -> RECOVERY -> NORMAL. The AI response is not independently evaluated for this scenario.
6. Stop the simulation and retain the single recording/log. If a driver error interrupts persistence, keep controls steady and inspect validation flags rather than counting wall-clock seconds alone.

The live temperature path is already verified; repeating it is optional for a final continuous demonstration. OTA is a separate historical demonstration with unavailable deleted hosting; do not enable its flag as part of this run.

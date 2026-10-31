# Vibration window input

Author: Mahesh Rupineni.

MPU6050 SDA/SCL connect to GPIO21/22, with AD0 grounded for address 0x68. I2C operates at 100 kHz. The sensor uses ±2g accelerometer range and the configured digital low-pass filter. The acquisition task owns the bus/device handles.

The driver reads 16 XYZ samples at nominal 10 ms intervals, converts signed values using 16384 counts/g, and computes sqrt(varX + varY + varZ), using population variance with the window mean removed. Constant gravity or a fixed tilted setting therefore produces near-zero vibration. Moving a slider can produce a nonzero window, but does not generate a calibrated motor waveform.

I2C errors, clipping or timing bounds produce NAN and validation rejects the record. Window duration is limited to 250 ms and inter-read gaps to 30 ms. Sampling uses task delays, not FIFO/data-ready synchronization, so timing fidelity and aliasing remain unestablished.

Ten pure RMS host checks passed. Mac builds and stationary/moving-slider Wokwi responses were observed. Intermittent timing timeouts remain open; later clean runs do not prove their cause resolved. The recalibrated model accepts the observed stationary baseline, but repeatable abnormal vibration evaluation is still absent.

# Current input proxy

Author: Mahesh Rupineni.

The potentiometer SIG connects to GPIO34 (ADC1 channel 6). The driver averages eight 12-bit raw codes at 12 dB attenuation, then maps [0,4095] to [0,10] A-equivalent. One acquisition task owns the ADC handle. Initialization failures require a reboot after correction; read failures leave NAN for validation. There is no generated fallback.

This is not a calibrated current sensor, voltage measurement or waveform RMS calculation. The legacy current_rms_a name is retained for compatibility and startup identifies it as a proxy. Use terminal current_a rather than knob angle. Floating input and valid endpoint saturation cannot be reliably distinguished here.

Ten host scaling checks passed. Mac firmware builds and manual Wokwi response were observed, including a high-input FAULT transition. The revised model accepts the stationary 42.5°C / near-2.44 proxy baseline; broader current-anomaly accuracy is not established. See verification.md and limitations.md.

import importlib.util
import json
from pathlib import Path
import unittest
p=Path(__file__).resolve().parents[2]/'python/scripts/collect_sensor_run.py'
spec=importlib.util.spec_from_file_location('capture',p)
capture=importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)
class DecodeTests(unittest.TestCase):
    def sample(self, **changes):
        p=dict(schema_version=1, device_id='demo', sequence=1, timestamp_ms=1000,
               validation_issues=0, temperature_c=42.5, vibration_rms_g=0,
               current_rms_a=2.44, ai_status='COMPLETE', is_anomaly=True)
        p.update(changes)
        return json.dumps(p).encode()
    def test_accepts_normal_features_even_when_old_ai_flags_anomaly(self):
        self.assertEqual(capture.decode_sample(self.sample(),'demo')['current_rms_a'],2.44)
    def test_quality(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(validation_issues=2),'demo')
    def test_device(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(),'other')
    def test_nan(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(temperature_c=float('nan')),'demo')
    def test_bool_feature(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(current_rms_a=True),'demo')
    def test_negative_sequence(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(sequence=-1),'demo')
    def test_bad_schema(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(schema_version=2),'demo')
    def test_bounds(self):
        with self.assertRaises(ValueError):capture.decode_sample(self.sample(current_rms_a=11),'demo')
    def test_oversized(self):
        with self.assertRaises(ValueError):capture.decode_sample(b' '*4097,'demo')
    def test_missing(self):
        with self.assertRaises(ValueError):capture.decode_sample(b'{}','demo')
if __name__ == '__main__':unittest.main()

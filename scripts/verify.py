#!/usr/bin/env python3
"""Run host checks without changing firmware, training data or model files."""
import argparse
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def run(args, **kwargs):
    print('+ ' + shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True, **kwargs)

def check_defaults():
    groups = {
        'firmware/components/sensor_sim/sensor_sim.c': ['SENSOR_SIM_FAULT_SCENARIO_ENABLED', 'SENSOR_SIM_AI_SCENARIO_ENABLED'],
        'firmware/main/monitor_test_config.h': ['MONITOR_TEST_VALIDATION_STALL_ENABLED'],
        'firmware/components/communication/include/communication_config.h': ['COMM_TEST_OUTAGE_ENABLED'],
        'firmware/components/firmware_update/include/firmware_update_config.h': ['MONITOR_OTA_DEMO_ENABLED', 'MONITOR_OTA_TEST_REJECT_HEALTH'],
    }
    for path, names in groups.items():
        content = (ROOT / path).read_text()
        for name in names:
            values = re.findall(r'^\s*#define\s+' + name + r'\s+([01])\s*$', content, re.M)
            if values != ['0']:
                raise ValueError(f'{name} must be defined exactly once as 0 for the baseline')
    print('PASS: all six development scenario flags disabled', flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', action='store_true', help='Also build ESP32 firmware and UF2 using the active ESP-IDF environment')
    args = parser.parse_args()
    check_defaults()
    if os.environ.get('HOST_CC'):
        compiler = shlex.split(os.environ['HOST_CC'])
    elif platform.system() == 'Darwin':
        compiler = ['/usr/bin/xcrun', '--sdk', 'macosx', 'clang', '-arch', platform.machine()]
    else:
        compiler = [shutil.which('cc') or 'cc']
    components = ROOT / 'firmware/components'
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror']
    includes = [arg for name in ['monitor_types', 'data_validation', 'fault_manager', 'anomaly_detector', 'sensor_sim'] for arg in ['-I', str(components / name / 'include')]]
    with tempfile.TemporaryDirectory(prefix='industrial-monitor-checks-') as temp:
        work = Path(temp)
        for name, source, component in [
            ('validation', 'data_validation_smoke.c', 'data_validation'),
            ('fault', 'fault_manager_test.c', 'fault_manager'),
            ('anomaly', 'anomaly_detector_host.c', 'anomaly_detector'),
        ]:
            binary = work / name
            run(compiler + flags + includes + [ROOT / 'tests/unit' / source, components / component / (component + '.c'), '-lm', '-o', binary])
            if name == 'anomaly':
                run([sys.executable, ROOT / 'python/scripts/check_c_inference.py', binary])
            else:
                run([binary])
        pipeline = work / 'pipeline'
        sources = [ROOT / 'tests/integration/pipeline_host.c'] + [components / name / (name + '.c') for name in ['sensor_sim', 'data_validation', 'fault_manager', 'anomaly_detector']]
        run(compiler + flags + includes + sources + ['-lm', '-o', pipeline])
        run([pipeline])
        rms = work / 'vibration_rms'
        run(compiler + flags + ['-I', components / 'vibration_sensor/include', ROOT / 'tests/unit/vibration_rms_test.c', components / 'vibration_sensor/vibration_rms.c', '-lm', '-o', rms])
        run([rms])
        current = work / 'current_scale'
        run(compiler + flags + ['-I', components / 'current_input/include', ROOT / 'tests/unit/current_scale_test.c', components / 'current_input/current_scale.c', '-lm', '-o', current])
        run([current])
        # Run the real exporter on temporary copies and compare exact bytes.
        for source in ['python/scripts/export_model.py', 'python/models/anomaly_model.json']:
            target = work / source
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, target)
        run([sys.executable, work / 'python/scripts/export_model.py'])
        header = Path('firmware/components/anomaly_detector/include/anomaly_model.h')
        if (work / header).read_bytes() != (ROOT / header).read_bytes():
            raise ValueError('Exported model header differs; regenerate and review it before continuing')
        print('PASS: model export matches committed header', flush=True)
        for script in sorted((ROOT / 'python/scripts').glob('*.py')):
            compile(script.read_bytes(), str(script), 'exec')
        print('PASS: Python script syntax', flush=True)
    run([sys.executable, '-m', 'unittest', 'discover', '-s', ROOT / 'tests/unit', '-p', 'test_sensor_capture.py'])
    if args.firmware:
        idf = shutil.which('idf.py')
        if not idf:
            raise RuntimeError('idf.py not found; activate ESP-IDF v6.1, then retry --firmware')
        run([idf, '-C', ROOT / 'firmware', '-DPROJECT_VER=0.1.0', 'build'])
        run([idf, '-C', ROOT / 'firmware', 'uf2'])
    print('PASS: requested project checks completed', flush=True)

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
"""Create a source-only release ZIP without altering the working project."""
from pathlib import Path
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DOCS = {'architecture.md', 'verification.md', 'demo.md', 'limitations.md', 'ota.md',
        'sensor_model_recalibration.md', 'sensor_model_collection.md',
        'temperature_integration.md', 'vibration_integration.md', 'current_input_integration.md'}
TOP = {'README.md', '.gitignore'}
TREES = {'firmware', 'python', 'scripts', 'tests', '.github', 'simulation'}

def selected(path):
    rel = path.relative_to(ROOT)
    parts = rel.parts
    if path.is_symlink() or any(p in {'build', 'managed_components', '__pycache__', '.git', '.venv', '.capture-venv', 'node_modules', 'local_backups', 'release'} for p in parts):
        return False
    if any('before_' in p or 'baseline' in p or p == '.DS_Store' for p in parts):
        return False
    if path.suffix in {'.zip', '.bin', '.elf', '.pyc'} or path.name == 'sdkconfig.old':
        return False
    if parts[:2] == ('firmware', 'industrial_monitor_ota'):
        return False
    if parts[0] == 'docs':
        return len(parts) == 2 and parts[1] in DOCS
    if parts[0] == 'simulation':
        return rel.as_posix() == 'simulation/mqtt_settings.json'
    # Legacy plaintext receiver and dataset overwriter are not release entry points.
    if rel.as_posix() in {'python/scripts/mqtt_receiver.py', 'python/scripts/generate_datasets.py'}:
        return False
    return (len(parts) == 1 and parts[0] in TOP) or parts[0] in TREES

def main():
    paths = sorted(p for p in ROOT.rglob('*') if p.is_file() and selected(p))
    required = ['README.md', 'firmware/CMakeLists.txt', 'firmware/sdkconfig',
                'firmware/partitions.csv', 'firmware/diagram.json', 'firmware/wokwi.toml',
                'firmware/dependencies.lock', 'scripts/verify.py', '.github/workflows/verify.yml']
    names = {p.relative_to(ROOT).as_posix() for p in paths}
    for name in required:
        if name not in names:
            raise SystemExit('Missing required release file: ' + name)
    for path in paths:
        text = path.read_bytes().decode('utf-8', errors='ignore')
        if re.search((chr(47) + 'Users' + chr(47) + r'|\bnavya\w*'), text, re.I):
            raise SystemExit('Personal path/name found; review before packaging: ' + str(path.relative_to(ROOT)))
        if path.suffix in {'.c', '.h', '.py', '.md', '.yml', '.toml'} and re.search(r'\bPhase\s+[1-9]\b', text, re.I):
            raise SystemExit('Development-stage wording remains: ' + str(path.relative_to(ROOT)))
    destination = ROOT / 'release'
    destination.mkdir(exist_ok=True)
    target = destination / 'industrial-monitor-source.zip'
    if target.exists():
        raise SystemExit('Release ZIP already exists; rename it before creating a new one.')
    manifest = {}
    with zipfile.ZipFile(target, 'x', zipfile.ZIP_DEFLATED) as archive:
        for path in paths:
            name = path.relative_to(ROOT).as_posix()
            data = path.read_bytes()
            archive.writestr('industrial-monitor/' + name, data)
            manifest[name] = hashlib.sha256(data).hexdigest()
        archive.writestr('industrial-monitor/SOURCE_SHA256.json', json.dumps(manifest, indent=2) + '\n')
    print('Source package created:', target)
    print('Files:', len(paths))
    print('No upload or publication performed. Review verification.md for outstanding checks.')

if __name__ == '__main__':
    main()

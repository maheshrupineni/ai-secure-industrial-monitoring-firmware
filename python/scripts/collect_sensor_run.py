"""Collect one operator-labelled Wokwi run without changing the active model."""
import argparse
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import ssl
import statistics
import time
from urllib.parse import urlparse
import uuid

ROOT = Path(__file__).resolve().parents[2]
FEATURES = ('temperature_c', 'vibration_rms_g', 'current_rms_a')


def decode_sample(raw, expected_device):
    if len(raw) > 4096:
        raise ValueError('oversized payload')
    def reject_constant(value):
        raise ValueError('non-finite JSON number: ' + value)
    p = json.loads(raw.decode('utf-8'), parse_constant=reject_constant)
    if not isinstance(p, dict) or type(p.get('schema_version')) is not int or p['schema_version'] != 1:
        raise ValueError('unsupported payload schema')
    if p.get('device_id') != expected_device:
        raise ValueError('device mismatch')
    for key in ('sequence', 'timestamp_ms'):
        if type(p.get(key)) is not int or p[key] < 0:
            raise ValueError('invalid ' + key)
    if p['sequence'] > 0xffffffff:
        raise ValueError('invalid sequence')
    if type(p.get('validation_issues')) is not int or p['validation_issues'] != 0:
        raise ValueError('invalid input quality')
    for key in FEATURES:
        if type(p.get(key)) not in (int, float) or not math.isfinite(p[key]):
            raise ValueError('invalid feature ' + key)
    if not (-55 <= p['temperature_c'] <= 125 and
            0 <= p['vibration_rms_g'] <= 16 and 0 <= p['current_rms_a'] <= 10):
        raise ValueError('feature outside collection bounds')
    return p


def connection_settings():
    text = (ROOT/'firmware/components/communication/include/communication_config.h').read_text()
    def define(name):
        matches = re.findall(r'^\s*#define\s+' + name + r'\s+"([^"\r\n]+)"\s*$', text, re.M)
        if len(matches) != 1:
            raise ValueError('Missing or ambiguous ' + name)
        return matches[0]
    uri = urlparse(define('COMM_BROKER_URI'))
    if uri.scheme != 'mqtts' or not uri.hostname or uri.username or uri.password:
        raise ValueError('Expected a credential-free mqtts broker URI for this demo')
    return uri.hostname, uri.port or 8883, define('COMM_DEVICE_ID'), define('COMM_TELEMETRY_TOPIC')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True, help='Unique run ID; never reused across splits')
    parser.add_argument('--split', required=True, choices=['train', 'calibration', 'test'])
    parser.add_argument('--label', required=True, choices=['normal', 'abnormal'])
    parser.add_argument('--scenario', required=True, help='Operator-described conditions, not AI predictions')
    parser.add_argument('--samples', type=int, default=60)
    parser.add_argument('--timeout', type=int, default=240, help='Overall wall-clock limit in seconds')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}', args.run):
        parser.error('Use a lowercase run ID with letters, digits, underscores or hyphens')
    if not 20 <= args.samples <= 10000 or not 30 <= args.timeout <= 3600:
        parser.error('samples must be 20..10000 and timeout 30..3600')
    if args.label == 'abnormal' and args.split != 'test':
        parser.error('Train/calibration collect normal conditions only')
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        raise SystemExit('paho-mqtt missing: use the project Python environment with paho-mqtt 2.x installed')
    host, port, device_id, topic = connection_settings()
    parent = ROOT/'python/datasets/sensor_runs'
    if any(parent.glob('*/' + args.run + '.*')):
        raise SystemExit('Run ID already exists; choose a new ID. No files overwritten.')
    folder = parent/args.split
    folder.mkdir(parents=True, exist_ok=True)
    partial = folder/(args.run + '.partial.csv')
    final = folder/(args.run + '.csv')
    meta_path = folder/(args.run + '.json')
    started = datetime.now(timezone.utc).isoformat()
    deadline = time.monotonic() + args.timeout
    rows, seen = [], set()
    rejected = duplicates = retained = 0
    fatal = None
    subscribed = False
    previous = None
    state_counts = {}
    fields = ['run_id', 'split', 'label', 'scenario', 'received_at_utc',
              'sequence', 'timestamp_ms', *FEATURES, 'state']
    stream = partial.open('x', newline='', encoding='utf-8')
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    stream.flush()

    def on_connect(client, userdata, flags, reason_code, properties):
        nonlocal subscribed
        subscribed = False
        if reason_code.is_failure:
            print('Connection rejected:', reason_code, flush=True)
            return
        client.subscribe(topic, qos=1)

    def on_subscribe(client, userdata, mid, codes, properties):
        nonlocal subscribed, fatal
        if any(code.is_failure for code in codes):
            fatal = 'Subscription rejected'
        else:
            subscribed = True
            print('SUBSCRIBED. Start/resume ONE Wokwi simulation now.', flush=True)

    def on_disconnect(client, userdata, flags, reason_code, properties):
        nonlocal subscribed
        subscribed = False
        print('MQTT disconnected; collected rows preserved; waiting for reconnect', flush=True)

    def on_message(client, userdata, message):
        nonlocal rejected, duplicates, retained, fatal, previous
        if len(rows) >= args.samples or fatal:
            return
        if message.retain:
            retained += 1
            return
        try:
            p = decode_sample(message.payload, device_id)
            key = (p['sequence'], p['timestamp_ms'])
            if key in seen:
                duplicates += 1
                return
            if previous is not None and (p['sequence'] <= previous[0] or p['timestamp_ms'] <= previous[1]):
                fatal = 'Device restarted or records arrived out of order; start a new capture run'
                return
            row = dict(zip(FEATURES, (p[k] for k in FEATURES)))
            row.update(run_id=args.run, split=args.split,
                       label=0 if args.label == 'normal' else 1,
                       scenario=args.scenario,
                       received_at_utc=datetime.now(timezone.utc).isoformat(),
                       sequence=p['sequence'], timestamp_ms=p['timestamp_ms'],
                       state=p.get('state', 'UNKNOWN'))
            writer.writerow(row)
            stream.flush()
            rows.append(row)
            seen.add(key)
            previous = key
            state_counts[row['state']] = state_counts.get(row['state'], 0) + 1
            if len(rows) == 1 or len(rows) % 10 == 0:
                print(f"Collected {len(rows)}/{args.samples}: " +
                      ', '.join(f'{k}={p[k]:.5f}' for k in FEATURES), flush=True)
        except (ValueError, UnicodeError, TypeError, KeyError) as error:
            rejected += 1
            if rejected <= 3:
                print('Rejected telemetry:', error, flush=True)

    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
                         client_id='capture-' + uuid.uuid4().hex[:16], protocol=mqtt.MQTTv311)
    client.on_connect, client.on_subscribe = on_connect, on_subscribe
    client.on_message, client.on_disconnect = on_message, on_disconnect
    client.tls_set_context(ssl.create_default_context())
    client.reconnect_delay_set(min_delay=1, max_delay=15)
    print('Collecting operator-labelled', args.label, 'run into', args.split, flush=True)
    print('Labels DO NOT come from AI or the fault-state output. Active model stays unchanged.', flush=True)
    print(f'Connecting to {host}:{port}; collection limit {args.timeout}s', flush=True)
    try:
        client.connect_async(host, port, keepalive=30)
        client.loop_start()
        while len(rows) < args.samples and not fatal and time.monotonic() < deadline:
            time.sleep(0.25)
    except KeyboardInterrupt:
        fatal = 'Interrupted by user'
    finally:
        client.disconnect()
        client.loop_stop()
        stream.close()
    complete = len(rows) == args.samples and not fatal
    stats = {}
    for name in FEATURES:
        values = [r[name] for r in rows]
        stats[name] = {'min': min(values) if values else None,
                       'max': max(values) if values else None,
                       'std': statistics.stdev(values) if len(values) > 1 else None}
    metadata = dict(run_id=args.run, split=args.split, label=args.label, scenario=args.scenario,
                    source='Wokwi simulated sensors; ADC current proxy, not measured RMS',
                    started_at_utc=started, finished_at_utc=datetime.now(timezone.utc).isoformat(),
                    collected_records=len(rows), requested_records=args.samples,
                    status='complete' if complete else 'incomplete',
                    rejected=rejected, duplicates=duplicates, retained=retained,
                    state_counts=state_counts, feature_statistics=stats,
                    stop_reason=fatal or ('target reached' if complete else 'wall-clock timeout'),
                    security='Public broker: TLS verifies server, not device identity. Topic/device ID are not authentication.',
                    limitations='Delivery latency not measured; no physical data or industrial performance claim.')
    meta_path.write_text(json.dumps(metadata, indent=2, allow_nan=False)+'\n')
    if not complete:
        print('INCOMPLETE:', metadata['stop_reason'])
        print('Partial data preserved:', partial)
        raise SystemExit(1)
    partial.rename(final)
    print('SAVED:', final)
    print(json.dumps(stats, indent=2, allow_nan=False))
    constant = [name for name, summary in stats.items() if summary['std'] == 0]
    if constant:
        print('NOTICE: constant features:', ', '.join(constant))
        print('This run alone cannot fit the current statistical model. More representative variation is needed.')
    if args.label == 'normal' and any(state != 'NORMAL' for state in state_counts):
        print('REVIEW REQUIRED: operator normal label conflicts with some fault states. Do not train before review.')
    print('Collection only: no training, model export, firmware change or publication performed.')

if __name__ == '__main__':
    main()

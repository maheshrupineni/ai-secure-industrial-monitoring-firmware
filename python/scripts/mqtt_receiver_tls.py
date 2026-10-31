"""Receive demonstration telemetry over certificate-verified TLS."""
import json
import ssl
import uuid
from datetime import datetime, timezone
from pathlib import Path

import paho.mqtt.client as mqtt

root = Path(__file__).resolve().parents[2]
settings = json.loads(
    (root / "simulation/mqtt_settings.json").read_text()
)
host = settings["broker_host"]
topic = settings["telemetry_topic"]

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Connection rejected: {reason_code}", flush=True)
        return
    print("CONNECTED over certificate-verified TLS", flush=True)
    client.subscribe(topic, qos=1)

def on_subscribe(client, userdata, mid, reason_codes, properties):
    if any(code.is_failure for code in reason_codes):
        print(f"Subscription rejected: {reason_codes}", flush=True)
        return
    print(f"SUBSCRIBED: {topic}", flush=True)
    print("Waiting for telemetry. Control+C stops the receiver.", flush=True)

def on_message(client, userdata, message):
    if len(message.payload) > 4096:
        print("Rejected oversized payload", flush=True)
        return
    try:
        payload = json.loads(message.payload.decode("utf-8"))
        record = {
            "received_at_utc": datetime.now(timezone.utc).isoformat(),
            "topic": message.topic,
            "qos": message.qos,
            "retained": message.retain,
            "payload": payload,
        }
        print(json.dumps(record, allow_nan=False), flush=True)
    except (UnicodeError, ValueError) as error:
        print(f"Rejected malformed payload: {error}", flush=True)

def on_disconnect(client, userdata, flags, reason_code, properties):
    print(f"DISCONNECTED: {reason_code}", flush=True)

def main():
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"receiver-tls-{uuid.uuid4().hex[:12]}",
        protocol=mqtt.MQTTv311,
    )
    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    client.tls_set_context(ssl.create_default_context())
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    print(f"Connecting with TLS to {host}:8883...", flush=True)
    client.connect_async(host, 8883, keepalive=30)
    try:
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        print("\nStopping receiver.", flush=True)
    finally:
        client.disconnect()

if __name__ == "__main__":
    main()

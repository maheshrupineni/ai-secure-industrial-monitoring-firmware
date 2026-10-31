"""Display selected telemetry fields over certificate-verified MQTT TLS."""
import json
import math
from pathlib import Path
import ssl
import uuid

import paho.mqtt.client as mqtt


def reading(value, places):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return "N/A"
    return f"{value:.{places}f}" if math.isfinite(value) else "N/A"


def format_telemetry(payload):
    if not isinstance(payload, dict):
        raise ValueError("telemetry must be a JSON object")
    temperature = reading(payload.get("temperature_c"), 2)
    current = reading(payload.get("current_rms_a"), 2)
    state = payload.get("state")
    if state not in ("STARTUP", "NORMAL", "WARNING", "FAULT", "RECOVERY"):
        state = "UNKNOWN"
    issues = payload.get("validation_issues")
    quality_ok = type(issues) is int and issues == 0
    if not quality_ok:
        state = "INVALID INPUT"
    decision = payload.get("is_anomaly")
    anomaly = "N/A"
    if quality_ok and payload.get("ai_status") == "COMPLETE" and type(decision) is bool:
        anomaly = "YES" if decision else "NO"
    return (f"Temp: {temperature} °C | Current proxy: {current} A-eq | "
            f"Status: {state} | Anomaly: {anomaly} | Transport: MQTT/TLS")


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Connection rejected: {reason_code}", flush=True)
        return
    print("CONNECTED: certificate-verified MQTT/TLS", flush=True)
    client.subscribe(userdata["topic"], qos=1)


def on_subscribe(client, userdata, mid, reason_codes, properties):
    if any(code.is_failure for code in reason_codes):
        print(f"Subscription rejected: {reason_codes}", flush=True)
        return
    print("SUBSCRIBED. Waiting for telemetry. Control+C stops the receiver.", flush=True)


def on_message(client, userdata, message):
    if len(message.payload) > 4096:
        print("Rejected oversized payload", flush=True)
        return
    try:
        payload = json.loads(message.payload.decode("utf-8"))
        print(format_telemetry(payload), flush=True)
    except (UnicodeError, ValueError, TypeError, OverflowError) as error:
        print(f"Rejected malformed payload: {error}", flush=True)


def on_disconnect(client, userdata, flags, reason_code, properties):
    print(f"DISCONNECTED: {reason_code}; no live telemetry connection", flush=True)


def main():
    root = Path(__file__).resolve().parents[2]
    settings = json.loads((root / "simulation/mqtt_settings.json").read_text())
    host = settings["broker_host"]
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"receiver-tls-{uuid.uuid4().hex[:12]}",
        protocol=mqtt.MQTTv311,
        userdata={"topic": settings["telemetry_topic"]},
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

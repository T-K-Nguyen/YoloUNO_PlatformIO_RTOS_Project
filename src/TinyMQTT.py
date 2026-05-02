import asyncio
import json
import threading
import time

import paho.mqtt.client as mqtt
from hbmqtt.broker import Broker

# Local broker bind (listen all interfaces in LAN)
LOCAL_BIND_HOST = "0.0.0.0"
LOCAL_BIND_PORT = 1883

# CoreIoT/ThingsBoard gateway config
THINGSBOARD_HOST = "app.coreiot.io"
THINGSBOARD_PORT = 1883
GATEWAY_ACCESS_TOKEN = "tgTuRMAFIDuBL4cp7HgA"

LOCAL_TELEMETRY_TOPIC = "local/devices/+/telemetry"
LOCAL_ATTRIBUTES_TOPIC = "local/devices/+/attributes"
LOCAL_SUBSCRIBE_TOPIC = "#"
LOCAL_COMMAND_TOPIC_TEMPLATE = "local/devices/{device_name}/commands"

CLOUD_GATEWAY_TELEMETRY_TOPIC = "v1/gateway/telemetry"
CLOUD_GATEWAY_ATTRIBUTES_TOPIC = "v1/gateway/attributes"
CLOUD_GATEWAY_RPC_TOPIC = "v1/gateway/rpc"
CLOUD_GATEWAY_SUBSCRIPTIONS = [
    (CLOUD_GATEWAY_RPC_TOPIC, 0),
    (CLOUD_GATEWAY_ATTRIBUTES_TOPIC, 0),
]

# === Broker Configuration ===
broker_config = {
    "listeners": {
        "default": {
            "type": "tcp",
            "bind": f"{LOCAL_BIND_HOST}:{LOCAL_BIND_PORT}",
        }
    },
    "sys_interval": 10,
    "auth": {
        "allow-anonymous": True,
    },
    "topic-check": {
        "enabled": True,
        "plugins": ["topic_taboo"],
    },
}


def extract_device_name(topic: str) -> str:
    # Expected format: local/devices/<device_name>/<telemetry|attributes>
    parts = topic.split("/")
    if len(parts) >= 4:
        return parts[2]
    return "unknown-device"


def normalize_payload(raw_payload: str):
    try:
        decoded = json.loads(raw_payload)
        if isinstance(decoded, dict):
            return decoded
    except json.JSONDecodeError:
        pass
    return {"raw": raw_payload}


def parse_cloud_command(payload_text: str):
    data = normalize_payload(payload_text)
    if not isinstance(data, dict):
        return None, None

    # Typical gateway downlink payload has top-level device and data.
    device_name = data.get("device")
    command_payload = data.get("data")

    # Fallback shapes: {"deviceA": {...}} or just raw dict payload.
    if not device_name and len(data) == 1:
        only_key = next(iter(data.keys()))
        only_value = data.get(only_key)
        if isinstance(only_value, dict):
            device_name = only_key
            command_payload = only_value

    if not device_name:
        return None, None

    if command_payload is None:
        command_payload = data

    return device_name, command_payload


def start_broker():
    async def broker_coro():
        broker = Broker(broker_config)
        await broker.start()
        print(f"[BROKER] MQTT broker started on {LOCAL_BIND_HOST}:{LOCAL_BIND_PORT}")

    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(broker_coro())
    loop.run_forever()


def run_bridge():
    cloud_client = mqtt.Client(client_id="CoreIoTGatewayPublisher")
    cloud_client.username_pw_set(GATEWAY_ACCESS_TOKEN)

    def on_cloud_connect(client, userdata, flags, rc):
        if rc == 0:
            print("[CLOUD] Connected to CoreIoT gateway.")
            client.subscribe(CLOUD_GATEWAY_SUBSCRIPTIONS)
            print("[CLOUD] Subscribed gateway downlink topics.")
        else:
            print(f"[CLOUD] Connect failed, rc={rc}")

    def on_cloud_subscribe(client, userdata, mid, granted_qos):
        print(f"[CLOUD] Subscribe ACK mid={mid}, qos={granted_qos}")

    def on_cloud_message(client, userdata, msg):
        payload_text = msg.payload.decode("utf-8", errors="replace")
        print(f"[CLOUD] Message topic={msg.topic}, bytes={len(msg.payload)}")

        if msg.topic not in (CLOUD_GATEWAY_RPC_TOPIC, CLOUD_GATEWAY_ATTRIBUTES_TOPIC):
            print(f"[CLOUD] Skip unsupported topic: {msg.topic}")
            return

        device_name, command_payload = parse_cloud_command(payload_text)
        if not device_name:
            print("[CLOUD] Skip command: cannot resolve device name from payload")
            return

        local_topic = LOCAL_COMMAND_TOPIC_TEMPLATE.format(device_name=device_name)
        result = local_client.publish(local_topic, json.dumps(command_payload))
        print(f"[BRIDGE] Forwarded cloud command to {local_topic}, local_rc={result.rc}")

    cloud_client.on_connect = on_cloud_connect
    cloud_client.on_subscribe = on_cloud_subscribe
    cloud_client.on_message = on_cloud_message
    cloud_client.connect(THINGSBOARD_HOST, THINGSBOARD_PORT, 60)
    cloud_client.loop_start()

    local_client = mqtt.Client(client_id="LocalBrokerBridgeSubscriber")

    def on_local_connect(client, userdata, flags, rc):
        if rc == 0:
            print("[BRIDGE] Connected to local broker, subscribing all local topics (#)...")
            client.subscribe(LOCAL_SUBSCRIBE_TOPIC, qos=0)
        else:
            print(f"[BRIDGE] Local broker connect failed, rc={rc}")

    def on_local_subscribe(client, userdata, mid, granted_qos):
        print(f"[BRIDGE] Subscribe ACK mid={mid}, qos={granted_qos}")

    def on_local_message(client, userdata, msg):
        print(f"[BRIDGE] Local message topic={msg.topic}, bytes={len(msg.payload)}")
        payload_text = msg.payload.decode("utf-8", errors="replace")
        values = normalize_payload(payload_text)
        device_name = extract_device_name(msg.topic)

        if msg.topic.endswith("/telemetry"):
            ts = int(time.time() * 1000)
            if isinstance(values, dict) and "ts" in values:
                values.pop("ts")

            gateway_payload = {
                device_name: [
                    {
                        "ts": ts,
                        "values": values,
                    }
                ]
            }
            result = cloud_client.publish(CLOUD_GATEWAY_TELEMETRY_TOPIC, json.dumps(gateway_payload))
            print(f"[BRIDGE] Forwarded telemetry from {device_name}, cloud_rc={result.rc}")
            return

        if msg.topic.endswith("/attributes"):
            if isinstance(values, dict) and "ts" in values:
                values.pop("ts")

            gateway_payload = {
                device_name: values,
            }
            result = cloud_client.publish(CLOUD_GATEWAY_ATTRIBUTES_TOPIC, json.dumps(gateway_payload))
            print(f"[BRIDGE] Forwarded attributes from {device_name}, cloud_rc={result.rc}")
            return

        if msg.topic.endswith("/commands"):
            print(f"[BRIDGE] Local command topic observed (echo), skip uplink: {msg.topic}")
            return

        print(f"[BRIDGE] Skip unsupported topic: {msg.topic}")

    local_client.on_connect = on_local_connect
    local_client.on_subscribe = on_local_subscribe
    local_client.on_message = on_local_message

    # Wait broker startup a moment
    time.sleep(2)
    local_client.connect("127.0.0.1", LOCAL_BIND_PORT, 60)
    local_client.loop_forever()


if __name__ == "__main__":
    broker_thread = threading.Thread(target=start_broker, daemon=True)
    broker_thread.start()

    bridge_thread = threading.Thread(target=run_bridge, daemon=True)
    bridge_thread.start()

    while True:
        time.sleep(1)
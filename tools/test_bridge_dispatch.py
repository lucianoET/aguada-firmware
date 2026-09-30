"""Self-check: a gateway line arriving twice (serial + WiFi mirror, or our own
republish echoed back) is handled once, and MQTT-sourced lines are not republished.
Run: python tools/test_bridge_dispatch.py"""
import collections
import sys
import threading
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import bridge


class FakeClient:
    def __init__(self):
        self.published = []

    def publish(self, topic, payload=None, **kw):
        self.published.append(topic)


def test_dispatch_dedupes_lines():
    b = bridge.Bridge.__new__(bridge.Bridge)
    b.client = FakeClient()
    b._recent_lines = collections.deque(maxlen=256)
    b._dispatch_lock = threading.Lock()
    b._mark_gateway_seen = lambda: None
    seqs = []
    b._handle_sensor = lambda m: seqs.append(m["seq"])

    line = ('{"v":3,"type":"SENSOR","node_id":"0x9EAC","sensor_id":1,'
            '"distance_cm":55,"rssi":-70,"vbat":null,"flags":0,"seq":7,"ts":1}')
    b.dispatch(line)                  # serial
    b.dispatch(line, from_mqtt=True)  # echo / WiFi mirror copy
    b.dispatch(line)
    assert seqs == [7]
    assert b.client.published == [bridge.GATEWAY_RX_TOPIC]

    b.dispatch(line.replace('"seq":7', '"seq":8'), from_mqtt=True)  # WiFi-only packet
    assert seqs == [7, 8]
    assert len(b.client.published) == 1  # never republish what came from MQTT


def test_node_alias_maps_to_canonical():
    idx = bridge.load_reservoir_config(str(Path(__file__).parent / "reservoirs.yaml"))
    assert bridge.NODE_ALIASES.get("0XEE02") == "0XC9C4"
    assert ("0XEE02", 1) not in idx


if __name__ == "__main__":
    test_dispatch_dedupes_lines()
    test_node_alias_maps_to_canonical()
    print("ok")

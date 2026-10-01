"""Self-check: HC-SR04 sensor-error packets publish per-sensor retained availability,
and HA discovery for the measurement entities follows it.
Run: .venv/bin/python tools/test_sensor_availability.py"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import bridge


class FakeClient:
    def __init__(self):
        self.published = []

    def publish(self, topic, payload=None, qos=0, retain=False, **kw):
        self.published.append((topic, payload, retain))


def make_bridge():
    args = argparse.Namespace(
        config=str(Path(__file__).parent / "reservoirs.yaml"),
        mqtt_user="", mqtt_password="", offline_timeout=300,
        influx_url="", gateway_timeout=0, serial_reconnect_delay=2.0,
    )
    b = bridge.Bridge(args)
    fake = FakeClient()
    b.client = fake
    b.tracker = bridge.NodeTracker(fake, 300)
    return b, fake


def avail(fake):
    return [p for p in fake.published if p[0].endswith("/availability")]


def states(fake, topic):
    return [i for i, p in enumerate(fake.published) if p[0] == topic]


def sensor_msg(node, sid, dist, flags, seq):
    return {"v": 3, "type": "SENSOR", "node_id": node, "sensor_id": sid,
            "distance_cm": dist, "rssi": -70, "vbat": None,
            "flags": flags, "seq": seq, "ts": 1}


def test_topic_format():
    assert bridge.mqtt_topic_sensor_avail("0X7758", 1) == "aguada/0X7758/1/availability"


def test_error_then_valid_transitions():
    b, fake = make_bridge()
    tp = "aguada/0X7758/1/availability"
    st = "aguada/0X7758/1/state"

    b._handle_sensor(sensor_msg("0x7758", 1, -1, 4, 1))
    assert avail(fake) == [(tp, "offline", True)]
    assert states(fake, st) == []

    b._handle_sensor(sensor_msg("0x7758", 1, 65535, 0, 2))  # still error
    assert len(avail(fake)) == 1

    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 3))
    assert avail(fake)[1:] == [(tp, "online", True)]
    state_idx = states(fake, st)
    assert len(state_idx) == 1
    online_idx = [i for i, p in enumerate(fake.published) if p[0] == tp][-1]
    assert state_idx[0] < online_idx  # state first, then online

    b._handle_sensor(sensor_msg("0x7758", 1, 103, 0, 4))
    assert len(states(fake, st)) == 2
    assert len(avail(fake)) == 2  # no republish


def test_two_sensors_independent():
    b, fake = make_bridge()
    b._handle_sensor(sensor_msg("0x2EC4", 1, 80, 0, 10))
    b._handle_sensor(sensor_msg("0x2EC4", 2, -1, 4, 11))
    a = avail(fake)
    assert a == [("aguada/0X2EC4/1/availability", "online", True),
                 ("aguada/0X2EC4/2/availability", "offline", True)]
    b._handle_sensor(sensor_msg("0x2EC4", 1, 81, 0, 12))
    assert len(avail(fake)) == 2


def discovery_payloads(fn, *a):
    fake = FakeClient()
    fn(fake, *a)
    return {t: json.loads(p) for t, p, _ in fake.published}


def test_discovery_measurement_entities_use_availability_list():
    b, _ = make_bridge()
    cfg = b.reservoirs[("0X7758", 1)]
    pl = discovery_payloads(bridge.publish_discovery, "0X7758", 1, cfg)
    for uid in ("nivel", "pct", "volume", "distancia"):
        p = pl[f"homeassistant/sensor/aguada_con_{uid}/config"]
        assert p["availability_mode"] == "all"
        assert "availability_topic" not in p
        assert [a["topic"] for a in p["availability"]] == [
            "aguada/0X7758/status", "aguada/0X7758/1/availability"]
        for a in p["availability"]:
            assert a["payload_available"] == "online"
            assert a["payload_not_available"] == "offline"
        assert p["unique_id"] == f"aguada_con_{uid}"
        assert p["state_topic"] == "aguada/0X7758/1/state"
    for uid in ("rssi", "bateria", "last_update"):
        p = pl[f"homeassistant/sensor/aguada_con_{uid}/config"]
        assert p["availability_topic"] == "aguada/0X7758/status"
        assert p["payload_available"] == "online"
        assert "availability" not in p and "availability_mode" not in p


def test_ani_discovery_untouched():
    b, _ = make_bridge()
    cfg = b.reservoirs[("0XA101", 1)]
    pl = discovery_payloads(bridge.publish_ani_discovery, "0XA101", 1, cfg)
    assert pl
    for p in pl.values():
        assert "availability" not in p and "availability_mode" not in p


if __name__ == "__main__":
    test_topic_format()
    test_error_then_valid_transitions()
    test_two_sensors_independent()
    test_discovery_measurement_entities_use_availability_list()
    test_ani_discovery_untouched()
    print("ok")

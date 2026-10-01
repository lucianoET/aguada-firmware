"""Self-check: out-of-range HC-SR04 readings are sensor errors.
Run: .venv/bin/python tools/test_reservoir_config.py"""
import argparse
import json
import shutil
import sys
import tempfile
import types
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import bridge
from test_sensor_availability import sensor_msg

REAL = Path(__file__).parent / "reservoirs.yaml"
REAL_BYTES = REAL.read_bytes()


class FakeClient:
    def __init__(self):
        self.published = []
        self.subscribed = []

    def publish(self, topic, payload=None, qos=0, retain=False, **kw):
        self.published.append((topic, payload, retain))

    def subscribe(self, topic, *a, **kw):
        self.subscribed.append(topic)


def make_bridge(tmpdir):
    """Real Bridge on a TEMP COPY of reservoirs.yaml (never the real file)."""
    copy = Path(tmpdir) / "reservoirs.yaml"
    shutil.copy(REAL, copy)
    args = argparse.Namespace(
        config=str(copy),
        mqtt_user="", mqtt_password="", offline_timeout=300,
        influx_url="", gateway_timeout=0, serial_reconnect_delay=2.0,
    )
    b = bridge.Bridge(args)
    fake = FakeClient()
    b.client = fake
    b.tracker = bridge.NodeTracker(fake, 300)
    return b, fake, copy


def test_out_of_range_is_sensor_error():
    st = "aguada/0X9EAC/2/state"
    av = "aguada/0X9EAC/2/availability"
    with tempfile.TemporaryDirectory() as d:
        b, fake, _ = make_bridge(d)
        b._handle_sensor(sensor_msg("0x9EAC", 2, 299, 0, 1))
        assert [p for p in fake.published if p[0] == av] == [(av, "offline", True)]
        assert [p for p in fake.published if p[0] == st] == []
        b._handle_sensor(sensor_msg("0x9EAC", 2, 270, 0, 2))
        assert len([p for p in fake.published if p[0] == st]) == 1
        assert [p for p in fake.published if p[0] == av][-1] == (av, "online", True)
    with tempfile.TemporaryDirectory() as d:
        b, fake, _ = make_bridge(d)
        b._handle_sensor(sensor_msg("0x9EAC", 2, 280, 0, 1))  # 240 + 10 + 30
        assert len([p for p in fake.published if p[0] == st]) == 1
    with tempfile.TemporaryDirectory() as d:
        b, fake, _ = make_bridge(d)
        b._handle_sensor(sensor_msg("0x9EAC", 2, 281, 0, 1))
        assert [p for p in fake.published if p[0] == st] == []
        assert [p for p in fake.published if p[0] == av] == [(av, "offline", True)]


def read_lines(path):
    return Path(path).read_text(encoding="utf-8").splitlines(keepends=True)


def changed(before, after):
    assert len(before) == len(after)
    return [(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b]


def test_yaml_update_scoped():
    with tempfile.TemporaryDirectory() as d:
        copy = Path(d) / "reservoirs.yaml"
        shutil.copy(REAL, copy)
        mode = copy.stat().st_mode
        before = read_lines(copy)
        assert bridge.update_reservoir_yaml(str(copy), "0XC9C4", "CAV", "level_max_cm", 460) is True
        diff = changed(before, read_lines(copy))
        assert len(diff) == 1 and diff[0][2] == "      level_max_cm: 460\n", diff
        idx = bridge.load_reservoir_config(str(copy))
        assert idx[("0XC9C4", 1)]["level_max_cm"] == 460
        assert idx[("0X7758", 1)]["level_max_cm"] == 450
        assert copy.stat().st_mode == mode

        before = read_lines(copy)
        assert bridge.update_reservoir_yaml(str(copy), "0X2EC4", "CB32", "sensor_offset_cm", 12) is True
        diff = changed(before, read_lines(copy))
        assert len(diff) == 1 and diff[0][2] == "      sensor_offset_cm: 12\n", diff
        idx = bridge.load_reservoir_config(str(copy))
        assert idx[("0X2EC4", 2)]["sensor_offset_cm"] == 12
        assert idx[("0X2EC4", 1)]["sensor_offset_cm"] == 10


def test_yaml_update_not_found():
    with tempfile.TemporaryDirectory() as d:
        copy = Path(d) / "reservoirs.yaml"
        shutil.copy(REAL, copy)
        orig = copy.read_bytes()
        for args in (("0X7758", "CAV", "level_max_cm", 999),      # alias not under node
                     ("0X9999", "CAV", "level_max_cm", 999),      # unknown node
                     ("0X7758", "CON", "no_such_param", 999)):    # param line absent
            assert bridge.update_reservoir_yaml(str(copy), *args) is False
            assert copy.read_bytes() == orig


def cfg_publishes(fake, topic):
    return [p for p in fake.published if p[0] == topic]


def set_msg(topic, payload):
    return types.SimpleNamespace(topic=topic, payload=payload)


def test_number_discovery_and_state():
    with tempfile.TemporaryDirectory() as d:
        b, fake, _ = make_bridge(d)
        b._on_mqtt_connect(fake, None, {}, 0)
        assert "aguada/+/+/config/+/set" in fake.subscribed
        pl = {t: (json.loads(p), r) for t, p, r in fake.published
              if t.startswith("homeassistant/number/")}
        assert not any("aguada_srn_cfg" in t for t in pl)
        want = {
            "level_max_cm": ("Altura útil", "altura_util", "cm", 50, 1000),
            "sensor_offset_cm": ("Offset do sensor", "offset_sensor", "cm", 0, 200),
            "volume_max_L": ("Volume máximo", "volume_maximo", "L", 1, 500000),
        }
        for param, (nm, ent, unit, lo, hi) in want.items():
            p, retained = pl[f"homeassistant/number/aguada_cav_cfg_{param}/config"]
            assert retained is True
            assert p["name"] == f"Castelo de Incêndio - {nm}"
            assert p["unique_id"] == f"aguada_cav_cfg_{param}"
            assert p["default_entity_id"] == f"number.aguada_cav_{ent}"
            assert p["state_topic"] == "aguada/0XC9C4/1/config"
            assert p["value_template"] == "{{ value_json.%s }}" % param
            assert p["command_topic"] == f"aguada/0XC9C4/1/config/{param}/set"
            assert (p["min"], p["max"], p["step"]) == (lo, hi, 1)
            assert p["mode"] == "box" and p["entity_category"] == "config"
            assert p["unit_of_measurement"] == unit and p["icon"]
            assert p["device"]["identifiers"] == ["aguada_cav"]
            for k in ("availability", "availability_topic", "availability_mode"):
                assert k not in p
        st = cfg_publishes(fake, "aguada/0XC9C4/1/config")
        assert len(st) == 1 and st[0][2] is True
        assert json.loads(st[0][1]) == {"level_max_cm": 450, "sensor_offset_cm": 20, "volume_max_L": 80000}
        assert not cfg_publishes(fake, "aguada/0XA101/1/config")


def test_set_command_applies_and_persists():
    with tempfile.TemporaryDirectory() as d:
        b, fake, copy = make_bridge(d)
        before = read_lines(copy)
        cfg = b.reservoirs[("0XC9C4", 1)]
        b._on_mqtt_message(None, None, set_msg("aguada/0XC9C4/1/config/level_max_cm/set", b"460"))
        assert b.reservoirs[("0XC9C4", 1)] is cfg and cfg["level_max_cm"] == 460
        diff = changed(before, read_lines(copy))
        assert len(diff) == 1 and diff[0][2] == "      level_max_cm: 460\n", diff
        last = cfg_publishes(fake, "aguada/0XC9C4/1/config")[-1]
        assert last[2] is True and json.loads(last[1])["level_max_cm"] == 460
        b._handle_sensor(sensor_msg("0xC9C4", 1, 100, 0, 1))
        state = json.loads(cfg_publishes(fake, "aguada/0XC9C4/1/state")[-1][1])
        assert state["level_cm"] == 380


def test_set_command_rejected():
    bad = [("aguada/0XC9C4/1/config/level_max_cm/set", b"5000"),
           ("aguada/0XC9C4/1/config/level_max_cm/set", b"abc"),
           ("aguada/0XC9C4/1/config/level_max_cm/set", b"nan"),
           ("aguada/0XC9C4/1/config/foo/set", b"100"),
           ("aguada/0XA101/1/config/level_max_cm/set", b"100"),
           ("aguada/0X9999/1/config/level_max_cm/set", b"100")]
    with tempfile.TemporaryDirectory() as d:
        b, fake, copy = make_bridge(d)
        orig = copy.read_bytes()
        for topic, payload in bad:
            b._on_mqtt_message(None, None, set_msg(topic, payload))
            cfg = b.reservoirs[("0XC9C4", 1)]
            assert (cfg["level_max_cm"], cfg["sensor_offset_cm"], cfg["volume_max_L"]) == (450, 20, 80000)
            assert copy.read_bytes() == orig
        b2, fake2, _ = make_bridge(d)
        b2._on_mqtt_message(None, None, set_msg("aguada/0XC9C4/1/config/level_max_cm/set", b"5000"))
        last = cfg_publishes(fake2, "aguada/0XC9C4/1/config")[-1]
        assert json.loads(last[1])["level_max_cm"] == 450


if __name__ == "__main__":
    test_out_of_range_is_sensor_error()
    test_yaml_update_scoped()
    test_yaml_update_not_found()
    test_number_discovery_and_state()
    test_set_command_applies_and_persists()
    test_set_command_rejected()
    assert REAL.read_bytes() == REAL_BYTES, "real reservoirs.yaml was modified"
    print("ok")

"""Self-check: out-of-range HC-SR04 readings are sensor errors.
Run: .venv/bin/python tools/test_reservoir_config.py"""
import argparse
import shutil
import sys
import tempfile
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


if __name__ == "__main__":
    test_out_of_range_is_sensor_error()
    test_yaml_update_scoped()
    test_yaml_update_not_found()
    assert REAL.read_bytes() == REAL_BYTES, "real reservoirs.yaml was modified"
    print("ok")

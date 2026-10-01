"""Self-check: seq dedup in _handle_sensor honors the TTL and is reset by HELLO
(node reboot restarts seq).
Run: .venv/bin/python tools/test_seq_dedup.py"""
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_sensor_availability import make_bridge, sensor_msg

ST_CON = "aguada/0X7758/1/state"
ST_CB3 = "aguada/0X2EC4/1/state"
ST_CAV = "aguada/0XC9C4/1/state"


def hello_msg(node, num):
    return {"v": 3, "type": "HELLO", "node_id": node, "num_sensors": num,
            "flags": 0, "vbat": -1, "seq": 0, "fw_version": "test", "ts": 1}


def n_pub(fake, topic):
    return sum(1 for p in fake.published if p[0] == topic)


def test_duplicate_within_ttl_dropped():
    b, fake = make_bridge()
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    assert n_pub(fake, ST_CON) == 1, "duplicate within TTL must be dropped"
    assert ("0X7758", 1, 1) in b._seen_seq


def test_expired_entry_accepted():
    b, fake = make_bridge()
    key = ("0X7758", 1, 1)
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    assert n_pub(fake, ST_CON) == 1
    b._seen_seq[key] -= b._seen_seq_ttl + 1
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    assert n_pub(fake, ST_CON) == 2, "expired entry must be accepted"
    assert abs(b._seen_seq[key] - time.time()) < 5, "timestamp must be refreshed"


def test_hello_resets_node_seq():
    b, fake = make_bridge()
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    assert n_pub(fake, ST_CON) == 1
    b._handle_hello(hello_msg("0x7758", 1))
    assert not [k for k in b._seen_seq if k[0] == "0X7758"], "HELLO must clear node entries"
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    assert n_pub(fake, ST_CON) == 2, "seq 1 after HELLO must be accepted"


def test_hello_scoped_to_node():
    b, fake = make_bridge()
    b._handle_sensor(sensor_msg("0x7758", 1, 102, 0, 1))
    b._handle_sensor(sensor_msg("0x2EC4", 1, 80, 0, 1))
    assert n_pub(fake, ST_CON) == 1 and n_pub(fake, ST_CB3) == 1
    b._handle_hello(hello_msg("0x7758", 1))
    b._handle_sensor(sensor_msg("0x2EC4", 1, 80, 0, 1))
    assert n_pub(fake, ST_CB3) == 1, "HELLO from other node must not clear this node"
    assert ("0X2EC4", 1, 1) in b._seen_seq


def test_hello_alias_clears_canonical():
    b, fake = make_bridge()
    b._handle_sensor(sensor_msg("0xEE02", 1, 102, 0, 5))
    assert n_pub(fake, ST_CAV) == 1
    b._handle_hello(hello_msg("0xee02", 1))
    b._handle_sensor(sensor_msg("0xEE02", 1, 102, 0, 5))
    assert n_pub(fake, ST_CAV) == 2, "HELLO via alias must clear canonical entries"


if __name__ == "__main__":
    test_duplicate_within_ttl_dropped()
    test_hello_scoped_to_node()
    test_expired_entry_accepted()
    test_hello_resets_node_seq()
    test_hello_alias_clears_canonical()
    print("ok")

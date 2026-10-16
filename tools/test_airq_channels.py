"""Self-check: canais AirQ / analisador de AC (sensor_id 0xF0–0xF6) chegam como
HEARTBEAT, viram aguada/{node}/airq/state e só criam entidade no HA quando o
periférico existe (1º valor válido). Run: python tools/test_airq_channels.py"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import bridge

NODE = "0X0494"


class FakeClient:
    def __init__(self):
        self.msgs = []

    def publish(self, topic, payload=None, **kw):
        self.msgs.append((topic, payload))

    def topics(self, prefix):
        return [t for t, _ in self.msgs if t.startswith(prefix)]

    def last_state(self):
        for t, p in reversed(self.msgs):
            if t == f"aguada/{NODE}/airq/state":
                return json.loads(p)
        return None


class FakeTracker:
    def seen(self, node_id):
        pass


def make_bridge():
    b = bridge.Bridge.__new__(bridge.Bridge)
    b.client = FakeClient()
    b.reservoirs = {}
    b.tracker = FakeTracker()
    b._airq = {}
    b._airq_disc = set()
    b._airq_ts = {}
    b._known_relays = {}
    b._relay_discovery_sent = set()
    b._env_discovery_sent = set()
    return b


def hb(sid, dist, reserved=0, rssi=-60):
    return {"v": 3, "type": "HEARTBEAT", "node_id": "0x0494", "sensor_id": sid,
            "distance_cm": dist, "rssi": rssi, "vbat": None, "reserved": reserved,
            "seq": 1, "ts": 1760000000}


def test_co2_creates_entity_once():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF0, 812))
    b._handle_heartbeat(hb(0xF0, 830))
    assert b.client.last_state()["co2"] == 830
    disc = b.client.topics("homeassistant/sensor/aguada_airq_0494_co2/")
    assert len(disc) == 1, disc


def test_absent_peripherals_create_no_entities():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF0, 812))
    assert not b.client.topics("homeassistant/sensor/aguada_airq_0494_lux")
    assert not b.client.topics("homeassistant/sensor/aguada_airq_0494_t_sup")
    assert not b.client.topics("homeassistant/binary_sensor/")


def test_delta_t_from_env_and_supply_probe():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF2, 1000 + 128))           # insuflamento 12.8 °C
    b._handle_heartbeat(hb(0xFE, 1000 + 245, 55))        # ambiente 24.5 °C (AHT21)
    st = b.client.last_state()
    assert st["t_sup"] == 12.8 and st["t_amb"] == 24.5
    assert st["dt_c"] == 11.7, st
    assert b.client.topics("homeassistant/sensor/aguada_airq_0494_dt_c/")


def test_negative_temperature_decodes():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF2, 1000 - 15))            # -1.5 °C (serpentina a congelar)
    assert b.client.last_state()["t_sup"] == -1.5


def test_current_and_compressor_state():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF3, 523))
    st = b.client.last_state()
    assert st["current_a"] == 5.23 and st["compressor_on"] is True
    assert b.client.topics("homeassistant/binary_sensor/aguada_airq_0494_compressor/")
    b._handle_heartbeat(hb(0xF3, 12))                    # 0.12 A: só ventilador/standby
    assert b.client.last_state()["compressor_on"] is False


def test_unplugged_probe_goes_null_without_new_entities():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF1, 1000 + 312))
    n_disc = len(b.client.topics("homeassistant/"))
    b._handle_heartbeat(hb(0xF1, 0xFFFF))
    assert b.client.last_state()["t_ext"] is None
    assert len(b.client.topics("homeassistant/")) == n_disc


def test_delta_t_null_when_compressor_off():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF3, 520))                   # 5.2 A — compressor ligado
    b._handle_heartbeat(hb(0xF2, 1000 + 130))
    b._handle_heartbeat(hb(0xFE, 1000 + 240, 50))
    assert b.client.last_state()["dt_c"] == 11.0
    b._handle_heartbeat(hb(0xF3, 15))                    # 0.15 A — só ventilador
    st = b.client.last_state()
    assert st["compressor_on"] is False and st["dt_c"] is None


def test_stale_ambient_does_not_feed_delta_t():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF2, 1000 + 130))
    b._handle_heartbeat(hb(0xFE, 1000 + 240, 50))
    b._airq_ts[NODE]["t_amb"] -= bridge.AIRQ_STALE_S + 10   # AHT21 calado há muito
    b._handle_heartbeat(hb(0xF2, 1000 + 131))
    st = b.client.last_state()
    assert st["t_amb"] is None and st["dt_c"] is None and st["t_sup"] == 13.1


def test_reserved_extras():
    b = make_bridge()
    b._handle_heartbeat(hb(0xF6, 230, reserved=3))       # TVOC 230 ppb, AQI 3
    b._handle_heartbeat(hb(0xF5, 482, reserved=71))      # 48.2 dB, pico 71 dB
    st = b.client.last_state()
    assert st["tvoc"] == 230 and st["aqi"] == 3
    assert st["sound_db"] == 48.2 and st["sound_peak_db"] == 71
    assert b.client.topics("homeassistant/sensor/aguada_airq_0494_aqi/")


if __name__ == "__main__":
    n = 0
    for name, fn in list(globals().items()):
        if name.startswith("test_") and callable(fn):
            fn(); n += 1
            print(f"ok  {name}")
    print(f"{n} testes OK")

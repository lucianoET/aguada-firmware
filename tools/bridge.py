#!/usr/bin/env python3
"""
Aguada Bridge v3.2
Reads JSON from gateway USB serial → calculates level/volume → publishes MQTT → HA Discovery.

Usage:
    python bridge.py [--port /dev/ttyACM0] [--baud 115200] [--mqtt localhost] [--mqtt-port 1883]
                     [--config reservoirs.yaml] [--offline-timeout 300]
"""

import argparse
import json
import logging
import os
import sys
import time
import threading
from pathlib import Path

import serial
import paho.mqtt.client as mqtt
import yaml

try:
    from influxdb_client import InfluxDBClient, Point
    from influxdb_client.client.write_api import SYNCHRONOUS
    _INFLUX_OK = True
except ImportError:
    _INFLUX_OK = False

# ── Logging ───────────────────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)-7s %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("bridge")

# ── Config ────────────────────────────────────────────────────────────────────

def load_reservoir_config(path: str) -> dict:
    with open(path) as f:
        data = yaml.safe_load(f)
    # Index: (node_id_str, sensor_id_int) → config dict
    index = {}
    for node_id_str, sensors in data.get("reservoirs", {}).items():
        for s in sensors:
            key = (node_id_str.upper(), int(s["sensor_id"]))
            index[key] = s
    return index

# ── Calculations ──────────────────────────────────────────────────────────────

def calculate(distance_cm: int, cfg: dict) -> dict:
    level_max_cm     = cfg["level_max_cm"]
    volume_max_L     = cfg["volume_max_L"]
    sensor_offset_cm = cfg["sensor_offset_cm"]

    level_cm = level_max_cm - (distance_cm - sensor_offset_cm)
    level_cm = max(0, min(level_cm, level_max_cm))

    pct      = round((level_cm / level_max_cm) * 100, 1)
    volume_L = round((level_cm / level_max_cm) * volume_max_L)

    return {"level_cm": level_cm, "pct": pct, "volume_L": volume_L}


def ts_to_iso(ts_value) -> str:
    """Convert unix timestamp (seconds) to ISO-8601 UTC string for HA timestamp sensors."""
    try:
        ts = int(ts_value)
    except Exception:
        ts = int(time.time())
    if ts <= 0:
        ts = int(time.time())
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(ts))


def vbat_to_volts(vbat_raw):
    """Convert battery voltage to volts.

    The protocol uses tenths-of-volts (e.g. 33 == 3.3V, -1 == n/a).
    Some gateway variants may emit JSON null for n/a; accept both.
    """
    if vbat_raw is None:
        return None
    # Spec: -1 means n/a
    if vbat_raw == -1:
        return None
    # Avoid treating bool as int (True/False)
    if isinstance(vbat_raw, bool):
        return None

    try:
        if isinstance(vbat_raw, int):
            if vbat_raw < 0:
                return None
            return round(vbat_raw / 10.0, 1)

        if isinstance(vbat_raw, float):
            if vbat_raw < 0:
                return None
            # Be tolerant: some producers may already emit volts (e.g. 3.3)
            if vbat_raw < 10:
                return round(vbat_raw, 1)
            return round(vbat_raw / 10.0, 1)

        s = str(vbat_raw).strip()
        if not s or s.lower() in {"null", "none"}:
            return None
        f = float(s)
        if f < 0:
            return None
        if f < 10:
            return round(f, 1)
        return round(f / 10.0, 1)
    except Exception:
        return None

# ── MQTT helpers ──────────────────────────────────────────────────────────────

def mqtt_topic_state(node_id: str, sensor_id: int) -> str:
    return f"aguada/{node_id}/{sensor_id}/state"

def mqtt_topic_status(node_id: str) -> str:
    return f"aguada/{node_id}/status"

def mqtt_topic_balance(node_id: str, sensor_id: int) -> str:
    return f"aguada/{node_id}/{sensor_id}/balance"

def mqtt_topic_gateway_health() -> str:
    return "aguada/gateway/health"

def mqtt_topic_gateway_ack() -> str:
    return "aguada/gateway/ack"

def publish_gateway_discovery(client: mqtt.Client):
    """Publish HA MQTT Discovery for gateway USB status/health diagnostics."""
    dev = {
        "name": "Aguada Gateway USB",
        "identifiers": ["aguada_gateway_usb"],
        "manufacturer": "Aguada",
        "model": "ESP32 USB Gateway",
    }
    avail_tp = "aguada/gateway/status"
    state_tp = mqtt_topic_gateway_health()

    online_payload = {
        "name": "Gateway USB - Online",
        "unique_id": "aguada_gateway_usb_online",
        "default_entity_id": "binary_sensor.aguada_gateway_usb_online",
        "state_topic": avail_tp,
        "payload_on": "online",
        "payload_off": "offline",
        "device_class": "connectivity",
        "device": dev,
        "icon": "mdi:lan-connect",
    }
    client.publish(
        "homeassistant/binary_sensor/aguada_gateway_usb_online/config",
        json.dumps(online_payload),
        retain=True,
    )
    log.debug("Gateway Discovery: homeassistant/binary_sensor/aguada_gateway_usb_online/config")

    sensors = [
        {
            "name": "Gateway USB - Uptime",
            "uid": "aguada_gateway_usb_uptime",
            "oid": "aguada_gateway_usb_uptime",
            "vt": "{{ value_json.uptime_s }}",
            "uom": "s",
            "dc": "duration",
            "sc": "measurement",
            "icon": "mdi:timer-outline",
        },
        {
            "name": "Gateway USB - Heap Livre",
            "uid": "aguada_gateway_usb_heap",
            "oid": "aguada_gateway_usb_heap",
            "vt": "{{ value_json.free_heap }}",
            "uom": "B",
            "dc": None,
            "sc": "measurement",
            "icon": "mdi:memory",
        },
        {
            "name": "Gateway USB - Queue Drops",
            "uid": "aguada_gateway_usb_queue_drops",
            "oid": "aguada_gateway_usb_queue_drops",
            "vt": "{{ value_json.queue_drops }}",
            "uom": None,
            "dc": None,
            "sc": "total_increasing",
            "icon": "mdi:package-variant-closed-remove",
        },
        {
            "name": "Gateway USB - CRC Falhas",
            "uid": "aguada_gateway_usb_crc_failures",
            "oid": "aguada_gateway_usb_crc_failures",
            "vt": "{{ value_json.crc_failures }}",
            "uom": None,
            "dc": None,
            "sc": "total_increasing",
            "icon": "mdi:alert-circle-outline",
        },
        {
            "name": "Gateway USB - Cmd Fail",
            "uid": "aguada_gateway_usb_cmd_fail",
            "oid": "aguada_gateway_usb_cmd_fail",
            "vt": "{{ value_json.cmd_fail }}",
            "uom": None,
            "dc": None,
            "sc": "total_increasing",
            "icon": "mdi:close-octagon-outline",
        },
        {
            "name": "Gateway USB - Rádio TX Fail",
            "uid": "aguada_gateway_usb_radio_tx_fail",
            "oid": "aguada_gateway_usb_radio_tx_fail",
            "vt": "{{ value_json.radio_tx_fail }}",
            "uom": None,
            "dc": None,
            "sc": "total_increasing",
            "icon": "mdi:radio-handheld",
        },
        {
            "name": "Gateway USB - Última atualização",
            "uid": "aguada_gateway_usb_last_seen",
            "oid": "aguada_gateway_usb_last_seen",
            "vt": "{{ value_json.ts_seen_iso }}",
            "uom": None,
            "dc": "timestamp",
            "sc": None,
            "icon": "mdi:clock-outline",
        },
    ]

    for e in sensors:
        payload = {
            "name": e["name"],
            "unique_id": e["uid"],
            "default_entity_id": f"sensor.{e['oid']}",
            "state_topic": state_tp,
            "value_template": e["vt"],
            "device": dev,
            "availability_topic": avail_tp,
            "payload_available": "online",
            "payload_not_available": "offline",
            "entity_category": "diagnostic",
        }
        if e.get("uom") is not None:
            payload["unit_of_measurement"] = e["uom"]
        if e.get("dc"):
            payload["device_class"] = e["dc"]
        if e.get("sc"):
            payload["state_class"] = e["sc"]
        if e.get("icon"):
            payload["icon"] = e["icon"]

        disc_topic = f"homeassistant/sensor/{e['uid']}/config"
        client.publish(disc_topic, json.dumps(payload), retain=True)
        log.debug("Gateway Discovery: %s", disc_topic)

def publish_discovery(client: mqtt.Client, node_id: str, sensor_id: int, cfg: dict):
    """Publish HA MQTT Discovery config for all entities of a sensor."""
    alias    = cfg.get("alias", f"{node_id}_{sensor_id}").lower()
    name_pfx = cfg.get("name", alias)
    uid_pfx  = f"aguada_{alias}"
    dev      = {"name": cfg.get("alias", alias), "identifiers": [f"aguada_{alias}"], "manufacturer": "Aguada"}
    state_tp = mqtt_topic_state(node_id, sensor_id)
    avail_tp = mqtt_topic_status(node_id)

    entities = [
        {
            "name": f"{name_pfx} - Nível",
            "uid":  f"{uid_pfx}_nivel",
            "oid":  f"aguada_{alias}_nivel",
            "vt":   "{{ value_json.level_cm }}",
            "uom":  "cm",
            "dc":   "distance",
            "sc":   "measurement",
            "icon": "mdi:water-level",
        },
        {
            "name": f"{name_pfx} - Volume %",
            "uid":  f"{uid_pfx}_pct",
            "oid":  f"aguada_{alias}_pct",
            "vt":   "{{ value_json.pct }}",
            "uom":  "%",
            "dc":   None,
            "sc":   "measurement",
            "icon": "mdi:percent",
        },
        {
            "name": f"{name_pfx} - Volume",
            "uid":  f"{uid_pfx}_volume",
            "oid":  f"aguada_{alias}_volume",
            "vt":   "{{ value_json.volume_L }}",
            "uom":  "L",
            "dc":   None,
            "sc":   "measurement",
            "icon": "mdi:water",
        },
        {
            "name": f"{name_pfx} - Distância",
            "uid":  f"{uid_pfx}_distancia",
            "oid":  f"aguada_{alias}_distancia",
            "vt":   "{{ value_json.distance_cm }}",
            "uom":  "cm",
            "dc":   "distance",
            "sc":   "measurement",
            "icon": "mdi:ruler",
        },
        {
            "name": f"{name_pfx} - RSSI",
            "uid":  f"{uid_pfx}_rssi",
            "oid":  f"aguada_{alias}_rssi",
            "vt":   "{{ value_json.rssi }}",
            "uom":  "dBm",
            "dc":   "signal_strength",
            "sc":   "measurement",
            "icon": "mdi:wifi",
        },
        {
            "name": f"{name_pfx} - Bateria",
            "uid":  f"{uid_pfx}_bateria",
            "oid":  f"aguada_{alias}_bateria",
            "vt":   "{{ value_json.vbat }}",
            "uom":  "V",
            "dc":   "voltage",
            "sc":   "measurement",
            "icon": "mdi:battery",
        },
        {
            "name": f"{name_pfx} - Último dado",
            "uid":  f"{uid_pfx}_last_update",
            "oid":  f"aguada_{alias}_last_update",
            "vt":   "{{ value_json.ts_iso }}",
            "uom":  None,
            "dc":   "timestamp",
            "sc":   None,
            "icon": "mdi:clock-outline",
        },
    ]

    for e in entities:
        payload = {
            "name":                e["name"],
            "unique_id":           e["uid"],
            "default_entity_id":   f"sensor.{e['oid']}",
            "state_topic":         state_tp,
            "value_template":      e["vt"],
            "device":              dev,
            "availability_topic":       avail_tp,
            "payload_available":        "online",
            "payload_not_available":    "offline",
        }
        if e.get("uom") is not None:
            payload["unit_of_measurement"] = e["uom"]
        if e.get("sc"):
            payload["state_class"] = e["sc"]
        if e["dc"]:
            payload["device_class"] = e["dc"]
        if e["icon"]:
            payload["icon"] = e["icon"]

        disc_topic = f"homeassistant/sensor/{e['uid']}/config"
        client.publish(disc_topic, json.dumps(payload), retain=True)
        log.debug("Discovery: %s", disc_topic)


def publish_relay_discovery(client: mqtt.Client, node_id: str):
    """Publish HA MQTT Discovery for a relay node: RSSI, battery, button event."""
    nid_short = node_id.replace("0X", "").replace("0x", "").lower()
    uid_pfx   = f"aguada_relay_{nid_short}"
    dev_name  = f"Relay {node_id}"
    dev       = {"name": dev_name, "identifiers": [uid_pfx], "manufacturer": "Aguada", "model": "ESP32-C3 Relay"}
    state_tp  = f"aguada/{node_id}/relay/state"
    avail_tp  = mqtt_topic_status(node_id)

    sensors = [
        {
            "name": f"{dev_name} - RSSI",
            "uid":  f"{uid_pfx}_rssi",
            "vt":   "{{ value_json.rssi }}",
            "uom":  "dBm",
            "dc":   "signal_strength",
            "sc":   "measurement",
            "icon": "mdi:wifi",
        },
        {
            "name": f"{dev_name} - Bateria",
            "uid":  f"{uid_pfx}_bateria",
            "vt":   "{{ value_json.vbat }}",
            "uom":  "V",
            "dc":   "voltage",
            "sc":   "measurement",
            "icon": "mdi:battery",
        },
    ]

    for e in sensors:
        payload = {
            "name":                e["name"],
            "unique_id":           e["uid"],
            "state_topic":         state_tp,
            "value_template":      e["vt"],
            "unit_of_measurement": e["uom"],
            "state_class":         e["sc"],
            "device_class":        e["dc"],
            "device":              dev,
            "availability_topic":      avail_tp,
            "payload_available":       "online",
            "payload_not_available":   "offline",
        }
        if e.get("icon"):
            payload["icon"] = e["icon"]
        disc_topic = f"homeassistant/sensor/{e['uid']}/config"
        client.publish(disc_topic, json.dumps(payload), retain=True)
        log.debug("Relay Discovery: %s", disc_topic)

    # Button as ON/OFF binary sensor (for HA automations by state trigger)
    btn_state_payload = {
        "name":                 f"{dev_name} - Botão",
        "unique_id":            f"{uid_pfx}_button_state",
        "default_entity_id":    f"binary_sensor.aguada_{nid_short}_button",
        "state_topic":          f"aguada/{node_id}/button/state",
        "payload_on":           "ON",
        "payload_off":          "OFF",
        "icon":                 "mdi:gesture-tap-button",
        "device":               dev,
        "availability_topic":   avail_tp,
        "payload_available":    "online",
        "payload_not_available": "offline",
    }
    client.publish(
        f"homeassistant/binary_sensor/{uid_pfx}_button_state/config",
        json.dumps(btn_state_payload),
        retain=True,
    )
    log.debug("Relay Button ON/OFF Discovery: homeassistant/binary_sensor/%s_button_state/config", uid_pfx)

    # Button — HA event entity (requires HA ≥ 2023.8)
    btn_payload = {
        "name":                 f"{dev_name} - Botão",
        "unique_id":            f"{uid_pfx}_button",
        "state_topic":          f"aguada/{node_id}/button",
        "event_types":          ["press"],
        "device":               dev,
        "availability_topic":   avail_tp,
        "payload_available":    "online",
        "payload_not_available": "offline",
        "icon":                 "mdi:button-pointer",
    }
    client.publish(f"homeassistant/event/{uid_pfx}_button/config", json.dumps(btn_payload), retain=True)
    log.debug("Relay Button Discovery: homeassistant/event/%s_button/config", uid_pfx)


def publish_ani_discovery(client: mqtt.Client, node_id: str, sensor_id: int, cfg: dict):
    """Publish HA MQTT Discovery for an ANI-01 air quality sensor node."""
    alias    = cfg.get("alias", f"{node_id}_aq").lower()
    name_pfx = cfg.get("name", alias)
    uid_pfx  = f"aguada_{alias}"
    dev      = {
        "name":         cfg.get("alias", alias),
        "identifiers":  [f"aguada_{alias}"],
        "manufacturer": "Aguada",
        "model":        "ANI-01 Air Quality Node",
    }
    state_tp = mqtt_topic_state(node_id, sensor_id)
    avail_tp = mqtt_topic_status(node_id)

    entities = [
        {
            "name": f"{name_pfx} - eCO2",
            "uid":  f"{uid_pfx}_eco2",
            "oid":  f"aguada_{alias}_eco2",
            "vt":   "{{ value_json.eco2 }}",
            "uom":  "ppm",
            "dc":   "carbon_dioxide",
            "sc":   "measurement",
            "icon": "mdi:molecule-co2",
        },
        {
            "name": f"{name_pfx} - AQI",
            "uid":  f"{uid_pfx}_aqi",
            "oid":  f"aguada_{alias}_aqi",
            "vt":   "{{ value_json.aqi }}",
            "uom":  None,
            "dc":   "aqi",
            "sc":   "measurement",
            "icon": "mdi:air-filter",
        },
        {
            "name": f"{name_pfx} - RSSI",
            "uid":  f"{uid_pfx}_rssi",
            "oid":  f"aguada_{alias}_rssi",
            "vt":   "{{ value_json.rssi }}",
            "uom":  "dBm",
            "dc":   "signal_strength",
            "sc":   "measurement",
            "icon": "mdi:wifi",
        },
        {
            "name": f"{name_pfx} - Tensão",
            "uid":  f"{uid_pfx}_vcc",
            "oid":  f"aguada_{alias}_vcc",
            "vt":   "{{ value_json.vbat }}",
            "uom":  "V",
            "dc":   "voltage",
            "sc":   "measurement",
            "icon": "mdi:power-plug",
        },
        {
            "name": f"{name_pfx} - Último dado",
            "uid":  f"{uid_pfx}_last_update",
            "oid":  f"aguada_{alias}_last_update",
            "vt":   "{{ value_json.ts_iso }}",
            "uom":  None,
            "dc":   "timestamp",
            "sc":   None,
            "icon": "mdi:clock-outline",
        },
    ]

    for e in entities:
        payload = {
            "name":                e["name"],
            "unique_id":           e["uid"],
            "default_entity_id":   f"sensor.{e['oid']}",
            "state_topic":         state_tp,
            "value_template":      e["vt"],
            "device":              dev,
            "availability_topic":       avail_tp,
            "payload_available":        "online",
            "payload_not_available":    "offline",
        }
        if e.get("uom") is not None:
            payload["unit_of_measurement"] = e["uom"]
        if e.get("sc"):
            payload["state_class"] = e["sc"]
        if e.get("dc"):
            payload["device_class"] = e["dc"]
        if e.get("icon"):
            payload["icon"] = e["icon"]
        disc_topic = f"homeassistant/sensor/{e['uid']}/config"
        client.publish(disc_topic, json.dumps(payload), retain=True)
        log.debug("ANI Discovery: %s", disc_topic)


def publish_env_discovery(client: mqtt.Client, node_id: str):
    """Publish HA MQTT Discovery for relay I2C env sensor (temp + humidity)."""
    nid_short = node_id.replace("0X", "").replace("0x", "").lower()
    uid_pfx   = f"aguada_relay_{nid_short}"
    dev_name  = f"Relay {node_id}"
    dev       = {"name": dev_name, "identifiers": [uid_pfx], "manufacturer": "Aguada", "model": "ESP32-C3 Relay"}
    state_tp  = f"aguada/{node_id}/env/state"
    avail_tp  = mqtt_topic_status(node_id)

    sensors = [
        {
            "name": f"{dev_name} - Temperatura",
            "uid":  f"{uid_pfx}_temp",
            "vt":   "{{ value_json.temp_c }}",
            "uom":  "°C",
            "dc":   "temperature",
            "sc":   "measurement",
            "icon": "mdi:thermometer",
        },
        {
            "name": f"{dev_name} - Umidade",
            "uid":  f"{uid_pfx}_hum",
            "vt":   "{{ value_json.hum_pct }}",
            "uom":  "%",
            "dc":   "humidity",
            "sc":   "measurement",
            "icon": "mdi:water-percent",
        },
    ]

    for e in sensors:
        payload = {
            "name":                e["name"],
            "unique_id":           e["uid"],
            "state_topic":         state_tp,
            "value_template":      e["vt"],
            "unit_of_measurement": e["uom"],
            "state_class":         e["sc"],
            "device_class":        e["dc"],
            "device":              dev,
            "availability_topic":      avail_tp,
            "payload_available":       "online",
            "payload_not_available":   "offline",
        }
        if e.get("icon"):
            payload["icon"] = e["icon"]
        disc_topic = f"homeassistant/sensor/{e['uid']}/config"
        client.publish(disc_topic, json.dumps(payload), retain=True)
        log.debug("Env Discovery: %s", disc_topic)


# ── Online/offline tracking ───────────────────────────────────────────────────

class NodeTracker:
    def __init__(self, client: mqtt.Client, offline_timeout_s: int):
        self._client   = client
        self._timeout  = offline_timeout_s
        self._last_seen: dict[str, float] = {}  # node_id → time.time()
        self._online:   set[str]          = set()

    def seen(self, node_id: str):
        was_online = node_id in self._online
        self._last_seen[node_id] = time.time()
        self._online.add(node_id)
        if not was_online:
            self._client.publish(mqtt_topic_status(node_id), "online", retain=True)
            log.info("Node %s → online", node_id)

    def check_timeouts(self):
        now = time.time()
        for node_id, ts in list(self._last_seen.items()):
            if now - ts > self._timeout and node_id in self._online:
                self._online.discard(node_id)
                self._client.publish(mqtt_topic_status(node_id), "offline", retain=True)
                log.warning("Node %s → offline (no data for %ds)", node_id, self._timeout)

    def sync_statuses(self, node_ids: set[str]):
        """Republish retained status for known nodes on broker reconnect/startup."""
        for node_id in sorted(node_ids):
            if node_id in self._online:
                self._client.publish(mqtt_topic_status(node_id), "online", retain=True)
            else:
                self._client.publish(mqtt_topic_status(node_id), "offline", retain=True)

# ── InfluxDB writer ──────────────────────────────────────────────────────────

class InfluxWriter:
    """Optional InfluxDB 2.x writer — only active when --influx-url is passed."""

    def __init__(self, url: str, token: str, org: str, bucket: str):
        if not _INFLUX_OK:
            raise RuntimeError(
                "influxdb-client not installed. Run: pip install 'influxdb-client'"
            )
        self._bucket = bucket
        self._org    = org
        self._client = InfluxDBClient(url=url, token=token, org=org)
        self._write  = self._client.write_api(write_options=SYNCHRONOUS)
        log.info("InfluxDB: %s  org=%s  bucket=%s", url, org, bucket)

    def write_sensor(self, node_id: str, sensor_id: int, alias: str, data: dict):
        p = (
            Point("reservoir")
            .tag("node_id",   node_id)
            .tag("sensor_id", str(sensor_id))
            .tag("alias",     alias)
            .field("distance_cm", int(data["distance_cm"]))
            .field("level_cm",    float(data["level_cm"]))
            .field("pct",         float(data["pct"]))
            .field("volume_L",    int(data["volume_L"]))
            .field("rssi",        int(data["rssi"]))
        )
        if data.get("vbat") is not None:
            p = p.field("vbat", float(data["vbat"]))
        try:
            self._write.write(bucket=self._bucket, org=self._org, record=p)
        except Exception as e:
            log.warning("InfluxDB write error: %s", e)

    def write_ani(self, node_id: str, sensor_id: int, alias: str, data: dict):
        p = (
            Point("air_quality")
            .tag("node_id",   node_id)
            .tag("sensor_id", str(sensor_id))
            .tag("alias",     alias)
            .field("eco2",    int(data["eco2"]))
            .field("aqi",     int(data["aqi"]))
            .field("rssi",    int(data["rssi"]))
        )
        if data.get("vbat") is not None:
            p = p.field("vbat", float(data["vbat"]))
        try:
            self._write.write(bucket=self._bucket, org=self._org, record=p)
        except Exception as e:
            log.warning("InfluxDB ANI write error: %s", e)

    def close(self):
        try:
            self._client.close()
        except Exception:
            pass


# ── Message handlers ──────────────────────────────────────────────────────────

class Bridge:
    def __init__(self, args):
        self.args      = args
        self.reservoirs = load_reservoir_config(args.config)
        log.info("Loaded %d reservoirs from %s", len(self.reservoirs), args.config)

        import socket as _socket, os as _os
        _cid = f"aguada-bridge-{_socket.gethostname()}-{_os.getpid()}"
        self.client  = mqtt.Client(client_id=_cid, clean_session=True,
                                   protocol=mqtt.MQTTv311)
        self.client.will_set("aguada/gateway/status", "offline", retain=True)
        if args.mqtt_user:
            self.client.username_pw_set(args.mqtt_user, args.mqtt_password)
        self.tracker = NodeTracker(self.client, args.offline_timeout)
        self._configured_nodes: set[str] = {node_id for (node_id, _sid) in self.reservoirs.keys()}
        self._discovery_sent: set = set()
        self._influx: InfluxWriter | None = None
        if args.influx_url:
            self._influx = InfluxWriter(
                url=args.influx_url,
                token=args.influx_token,
                org=args.influx_org,
                bucket=args.influx_bucket,
            )

        # Subscribe to command topics
        self.client.on_connect    = self._on_mqtt_connect
        self.client.on_message    = self._on_mqtt_message
        self.client.on_disconnect = self._on_mqtt_disconnect

        self._serial: serial.Serial | None = None
        self._serial_lock = threading.Lock()
        # Dedup cache: (node_id, sensor_id, seq) → timestamp
        # Prevents double-publish when same packet arrives via relay AND direct
        self._seen_seq: dict[tuple, float] = {}
        self._seen_seq_ttl = 120.0  # seconds
        # Relay / env discovery tracking
        self._known_relays: dict[str, bool] = {}   # node_id → has_env_sensor
        self._relay_discovery_sent: set = set()
        self._env_discovery_sent: set = set()
        self._button_off_timers: dict[str, threading.Timer] = {}

        # Gateway presence tracking (USB serial + JSON activity)
        self._gateway_online = False
        self._gateway_last_seen = 0.0
        self._gateway_timeout_s = args.gateway_timeout
        self._serial_reconnect_delay_s = max(1.0, float(args.serial_reconnect_delay))

        # Last known gateway health payload (retained MQTT JSON). Used to keep
        # ts_seen fresh even if the gateway doesn't periodically emit STATUS.
        self._gateway_health_payload: dict | None = None
        self._gateway_health_last_pub = 0.0
        self._gateway_health_pub_interval_s = 15.0

    def _publish_json(self, topic: str, payload: dict, retain: bool = False):
        self.client.publish(topic, json.dumps(payload), retain=retain)

    def _mark_gateway_seen(self):
        now = time.time()
        self._gateway_last_seen = now
        if not self._gateway_online:
            self._gateway_online = True
            self.client.publish("aguada/gateway/status", "online", retain=True)
            log.info("Gateway → online")

        # Keep retained health payload's last-seen timestamp coherent with real
        # activity, but throttle publishes to avoid flooding.
        if self._gateway_health_payload and (now - self._gateway_health_last_pub) >= self._gateway_health_pub_interval_s:
            ts_now = int(now)
            p = dict(self._gateway_health_payload)
            p["online"] = True
            p["ts_seen"] = ts_now
            p["ts_seen_iso"] = ts_to_iso(ts_now)
            self._publish_json(mqtt_topic_gateway_health(), p, retain=True)
            self._gateway_health_payload = p
            self._gateway_health_last_pub = now

    def _mark_gateway_offline(self, reason: str = ""):
        if self._gateway_online:
            ts_now = int(time.time())
            self._gateway_online = False
            self.client.publish("aguada/gateway/status", "offline", retain=True)
            self._publish_json(
                mqtt_topic_gateway_health(),
                {
                    "type": "GATEWAY_STATUS",
                    "online": False,
                    "reason": reason or "offline",
                    "ts": ts_now,
                    "ts_seen": ts_now,
                    "ts_seen_iso": ts_to_iso(ts_now),
                },
                retain=True,
            )
            if reason:
                log.warning("Gateway → offline (%s)", reason)
            else:
                log.warning("Gateway → offline")

    # ── MQTT ─────────────────────────────────────────────────────────────────

    def _on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            log.info("MQTT connected")
            # Keep retained gateway status coherent with current serial liveness.
            client.publish(
                "aguada/gateway/status",
                "online" if self._gateway_online else "offline",
                retain=True,
            )
            client.subscribe("aguada/cmd/#")
            # Wired Ethernet nodes (e.g. node-eth) publish sensor JSON straight
            # to the broker instead of arriving over the gateway serial link.
            client.subscribe("aguada/raw/#")
            publish_gateway_discovery(client)

            # Re-publish HA Discovery on every (re)connect to survive broker
            # restarts/retained cleanup without requiring node reboot (HELLO).
            for (node_id, sensor_id), cfg in self.reservoirs.items():
                if cfg.get("type") == "air_quality":
                    publish_ani_discovery(client, node_id, sensor_id, cfg)
                    self._discovery_sent.add(("ani", node_id, sensor_id))
                else:
                    publish_discovery(client, node_id, sensor_id, cfg)
                    self._discovery_sent.add((node_id, sensor_id))
            # Re-publish relay/env discovery for known relay nodes
            for node_id, has_env in self._known_relays.items():
                publish_relay_discovery(client, node_id)
                if has_env:
                    publish_env_discovery(client, node_id)
            # Prevent stale retained online states after bridge restart.
            known_nodes = set(self._configured_nodes) | set(self._known_relays.keys())
            self.tracker.sync_statuses(known_nodes)
        else:
            log.error("MQTT connect failed rc=%d", rc)

    def _on_mqtt_disconnect(self, client, userdata, rc):
        log.warning("MQTT disconnected rc=%d, will reconnect", rc)

    def _on_mqtt_message(self, client, userdata, msg):
        """Commands from HA/server → forward to gateway via serial.
        Also routes wired Ethernet node sensor JSON (aguada/raw/#) into the
        existing serial-sensor pipeline."""
        topic = msg.topic
        try:
            payload = json.loads(msg.payload)
        except Exception:
            if topic.startswith("aguada/raw/"):
                log.warning("Malformed JSON on %s", topic)
            return

        if topic.startswith("aguada/raw/"):
            if not isinstance(payload, dict):
                log.warning("Non-dict payload on %s", topic)
                return
            node_id = str(payload.get("node_id", "")).upper()
            if node_id not in self._configured_nodes:
                log.warning("Unknown node_id %r on %s", node_id, topic)
                return
            try:
                self._handle_sensor(payload)
            except Exception:
                log.exception("Handler error for raw topic=%s payload=%s", topic, str(payload)[:200])
            return

        if topic == "aguada/cmd/restart":
            self._serial_write(json.dumps({"cmd": "RESTART", "node_id": payload.get("node_id")}))
        elif topic == "aguada/cmd/config":
            self._serial_write(json.dumps({**payload, "cmd": "CONFIG"}))
        elif topic == "aguada/cmd/ota":
            log.warning("OTA via MQTT not yet implemented")

    def _serial_write(self, line: str):
        with self._serial_lock:
            if self._serial and self._serial.is_open:
                self._serial.write((line + "\n").encode())
                log.debug("→ serial: %s", line)

    def _close_serial(self):
        with self._serial_lock:
            ser = self._serial
            self._serial = None

        if ser is None:
            return

        try:
            if ser.is_open:
                ser.close()
        except Exception as e:
            log.debug("Serial close skipped: %s", e)

    def _open_serial(self) -> bool:
        port_display = self.args.port
        try:
            real_port = os.path.realpath(self.args.port)
            if real_port and real_port != self.args.port:
                port_display = f"{self.args.port} -> {real_port}"
        except Exception:
            pass

        log.info("Opening %s @ %d baud", port_display, self.args.baud)

        try:
            ser = serial.Serial(self.args.port, self.args.baud, timeout=1)
        except serial.SerialException as e:
            log.warning("Unable to open serial %s: %s", self.args.port, e)
            self._mark_gateway_offline("serial unavailable")
            return False

        with self._serial_lock:
            self._serial = ser

        try:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
        except Exception as e:
            log.debug("Serial buffer reset skipped: %s", e)

        # Some USB-CDC boards do not auto-reset reliably on open.
        # Force a short DTR pulse so the gateway always reboots and sends GATEWAY_READY.
        try:
            ser.setDTR(False)
            time.sleep(0.15)
            ser.setDTR(True)
            log.debug("Serial DTR pulse applied")
        except Exception as e:
            log.debug("DTR pulse skipped: %s", e)

        log.info("Waiting 3s for gateway boot...")
        self._drain_startup_serial(ser, 3.0)
        return True

    def _reconnect_serial(self, reason: str):
        self._mark_gateway_offline(reason)
        self._close_serial()
        log.warning(
            "Reconnecting serial in %.1fs (%s)",
            self._serial_reconnect_delay_s,
            reason,
        )
        time.sleep(self._serial_reconnect_delay_s)

    def _publish_button_state_pulse(self, node_id: str):
        """Publish button pulse as ON then OFF shortly after, for HA binary_sensor."""
        topic = f"aguada/{node_id}/button/state"
        # Rising edge for automations
        self.client.publish(topic, "ON", retain=True)

        # Debounce/reset: cancel previous OFF timer and schedule a new one
        prev = self._button_off_timers.get(node_id)
        if prev is not None:
            prev.cancel()

        def _set_off():
            self.client.publish(topic, "OFF", retain=True)

        t = threading.Timer(1.0, _set_off)
        t.daemon = True
        t.start()
        self._button_off_timers[node_id] = t

    # ── Message processing ────────────────────────────────────────────────────

    def _handle_sensor(self, msg: dict):
        node_id   = msg["node_id"].upper()
        sensor_id = int(msg["sensor_id"])
        key       = (node_id, sensor_id)

        # Deduplicate: same packet may arrive via relay AND direct path
        seq_key = (node_id, sensor_id, msg.get("seq", -1))
        now = time.time()
        if seq_key in self._seen_seq:
            log.debug("Dedup: skip duplicate %s sensor=%d seq=%s", node_id, sensor_id, msg.get("seq"))
            return
        self._seen_seq[seq_key] = now
        # Purge old entries
        if len(self._seen_seq) > 512:
            cutoff = now - self._seen_seq_ttl
            self._seen_seq = {k: v for k, v in self._seen_seq.items() if v > cutoff}

        self.tracker.seen(node_id)

        cfg = self.reservoirs.get(key)
        if not cfg:
            log.debug("No reservoir config for %s sensor %d", node_id, sensor_id)
            return

        flags = msg.get("flags", 0)
        try:
            flags = int(flags) if flags is not None else 0
        except Exception:
            flags = 0

        # FLAG_IS_AIR_QUALITY (bit 7 = 0x80): ANI-01 packet — eco2 in distance_cm, aqi in reserved
        if (flags & 0x80) or cfg.get("type") == "air_quality":
            self._handle_ani_sensor(msg, flags, cfg)
            return

        distance_cm_raw = msg.get("distance_cm", -1)
        try:
            distance_cm = int(distance_cm_raw) if distance_cm_raw is not None else -1
        except Exception:
            distance_cm = -1

        # Spec: distance 0xFFFF or flags.sensor_error indicate invalid measurement
        if distance_cm <= 0 or distance_cm == 0xFFFF or (flags & 0x04):
            log.warning("Sensor error: %s/%d dist=%s flags=0x%02x", node_id, sensor_id, distance_cm_raw, flags)
            return

        calcs = calculate(distance_cm, cfg)

        payload = {
            "alias":       cfg.get("alias", ""),
            "distance_cm": distance_cm,
            "level_cm":    calcs["level_cm"],
            "pct":         calcs["pct"],
            "volume_L":    calcs["volume_L"],
            "rssi":        msg.get("rssi", 0),
            "vbat":        vbat_to_volts(msg.get("vbat", -1)),
            "seq":         msg.get("seq", 0),
            "ts":          msg.get("ts", int(time.time())),
        }
        payload["ts_iso"] = ts_to_iso(payload["ts"])

        self.client.publish(mqtt_topic_state(node_id, sensor_id), json.dumps(payload), retain=True)
        log.info("%-6s dist=%3dcm  level=%3dcm  pct=%5.1f%%  vol=%dL",
                 cfg.get("alias", key), distance_cm, calcs["level_cm"],
                 calcs["pct"], calcs["volume_L"])

        if self._influx:
            self._influx.write_sensor(node_id, sensor_id, cfg.get("alias", ""), payload)

    def _handle_ani_sensor(self, msg: dict, flags: int, cfg: dict):
        """Handle PKT_SENSOR with FLAG_IS_AIR_QUALITY: eco2 in distance_cm, aqi in reserved."""
        node_id   = msg["node_id"].upper()
        sensor_id = int(msg["sensor_id"])

        # FLAG_SENSOR_ERROR (bit 2): sensor read failure
        if flags & 0x04:
            log.warning("ANI sensor error: %s/%d flags=0x%02x", node_id, sensor_id, flags)
            return

        eco2 = int(msg.get("distance_cm", 0) or 0)
        aqi  = int(msg.get("reserved", 0) or 0)
        ts   = msg.get("ts", int(time.time()))

        payload = {
            "alias": cfg.get("alias", ""),
            "eco2":  eco2,
            "aqi":   aqi,
            "rssi":  msg.get("rssi", 0),
            "vbat":  vbat_to_volts(msg.get("vbat", -1)),
            "seq":   msg.get("seq", 0),
            "ts":    ts,
        }
        payload["ts_iso"] = ts_to_iso(ts)

        topic = mqtt_topic_state(node_id, sensor_id)
        self.client.publish(topic, json.dumps(payload), retain=True)
        log.info("ANI  %-6s eco2=%4uppm  aqi=%u  rssi=%d",
                 cfg.get("alias", f"{node_id}/{sensor_id}"), eco2, aqi, msg.get("rssi", 0))

        # Publish ANI discovery on first packet
        ani_key = ("ani", node_id, sensor_id)
        if ani_key not in self._discovery_sent:
            publish_ani_discovery(self.client, node_id, sensor_id, cfg)
            self._discovery_sent.add(ani_key)

        if self._influx:
            self._influx.write_ani(node_id, sensor_id, cfg.get("alias", ""), payload)

    def _handle_heartbeat(self, msg: dict):
        node_id   = msg["node_id"].upper()
        sensor_id = int(msg.get("sensor_id", 0))
        self.tracker.seen(node_id)

        # SENSOR_ID_ENV (0xFE): relay I2C env telemetry
        # Encoding: distance_cm = (int)(temp_c*10)+1000, reserved = humidity%
        if sensor_id == 0xFE:
            raw_temp = int(msg.get("distance_cm", 1000))
            hum_pct  = int(msg.get("reserved", 0))
            temp_c   = round((raw_temp - 1000) / 10.0, 1)
            vbat     = vbat_to_volts(msg.get("vbat", -1))
            ts       = msg.get("ts", int(time.time()))

            env_payload = {
                "temp_c":  temp_c,
                "hum_pct": hum_pct,
                "rssi":    msg.get("rssi", 0),
                "vbat":    vbat,
                "ts":      ts,
            }
            self.client.publish(f"aguada/{node_id}/env/state", json.dumps(env_payload), retain=True)
            log.info("ENV    %-6s  T=%5.1f°C  H=%3d%%  rssi=%d",
                     node_id, temp_c, hum_pct, msg.get("rssi", 0))

            # Also refresh relay state (has fresh rssi/vbat)
            relay_payload = {"rssi": msg.get("rssi", 0), "vbat": vbat,
                             "seq": msg.get("seq", 0), "ts": ts}
            self.client.publish(f"aguada/{node_id}/relay/state", json.dumps(relay_payload), retain=True)

            # Publish discovery on first env packet (relay may not be in reservoirs.yaml)
            if node_id not in self._relay_discovery_sent:
                publish_relay_discovery(self.client, node_id)
                self._relay_discovery_sent.add(node_id)
                self._known_relays[node_id] = True
            if node_id not in self._env_discovery_sent:
                publish_env_discovery(self.client, node_id)
                self._env_discovery_sent.add(node_id)
                self._known_relays[node_id] = True
            return

        log.debug("HEARTBEAT %s/%d seq=%s dist=%s", node_id, sensor_id,
                  msg.get("seq"), msg.get("distance_cm"))

        # Regular heartbeat from sensor node: update sensor state if distance valid
        if "distance_cm" in msg and 0 < sensor_id < 0xFE:
            distance_cm = msg["distance_cm"]
            if distance_cm > 0:
                key = (node_id, sensor_id)
                cfg = self.reservoirs.get(key)
                if cfg:
                    calcs = calculate(distance_cm, cfg)
                    payload = {
                        "alias":       cfg.get("alias", ""),
                        "distance_cm": distance_cm,
                        "level_cm":    calcs["level_cm"],
                        "pct":         calcs["pct"],
                        "volume_L":    calcs["volume_L"],
                        "rssi":        msg.get("rssi", 0),
                        "vbat":        None,
                        "seq":         msg.get("seq", 0),
                        "ts":          msg.get("ts", int(time.time())),
                    }
                    payload["ts_iso"] = ts_to_iso(payload["ts"])
                    self.client.publish(mqtt_topic_state(node_id, sensor_id),
                                       json.dumps(payload), retain=True)
                    log.debug("HEARTBEAT update: %s dist=%dcm", cfg.get("alias"), distance_cm)

    def _handle_hello(self, msg: dict):
        node_id   = msg["node_id"].upper()
        num       = int(msg.get("num_sensors", 0))
        flags     = int(msg.get("flags", 0))
        vbat      = vbat_to_volts(msg.get("vbat", -1))
        self.tracker.seen(node_id)
        log.info("HELLO %s num_sensors=%d flags=0x%02x fw=%s",
                 node_id, num, flags, msg.get("fw_version"))

        if num == 0:
            # Relay node — publish relay state and Discovery
            relay_payload = {
                "rssi": msg.get("rssi", 0),
                "vbat": vbat,
                "seq":  msg.get("seq", 0),
                "ts":   msg.get("ts", int(time.time())),
            }
            self.client.publish(f"aguada/{node_id}/relay/state", json.dumps(relay_payload), retain=True)

            if node_id not in self._relay_discovery_sent:
                publish_relay_discovery(self.client, node_id)
                self._relay_discovery_sent.add(node_id)
                self._known_relays.setdefault(node_id, False)
                # Ensure known default state for HA binary_sensor
                self.client.publish(f"aguada/{node_id}/button/state", "OFF", retain=True)

            # FLAG_BTN_HELLO = 0x40: button press (not boot)
            if flags & 0x40:
                log.info("HELLO %s — button press event", node_id)
                self._publish_button_state_pulse(node_id)
                btn_payload = {"event_type": "press", "ts": msg.get("ts", int(time.time()))}
                self.client.publish(f"aguada/{node_id}/button", json.dumps(btn_payload), retain=False)
        else:
            # Sensor node — publish reservoir Discovery
            for sid in range(1, num + 1):
                key = (node_id, sid)
                cfg = self.reservoirs.get(key)
                if cfg and key not in self._discovery_sent:
                    publish_discovery(self.client, node_id, sid, cfg)
                    self._discovery_sent.add(key)

    def _handle_gateway_ready(self, msg: dict):
        log.info("Gateway ready: mac=%s fw=%s", msg.get("mac"), msg.get("fw"))
        self._mark_gateway_seen()
        ready_payload = dict(msg)
        ready_payload["online"] = True
        ready_payload["ts_seen"] = int(time.time())
        ready_payload["ts_seen_iso"] = ts_to_iso(ready_payload["ts_seen"])
        self._publish_json(mqtt_topic_gateway_health(), ready_payload, retain=True)
        self._gateway_health_payload = dict(ready_payload)
        self._gateway_health_last_pub = time.time()
        # Send current time to gateway
        self._serial_write(json.dumps({"cmd": "SETTIME", "ts": int(time.time())}))

    def _handle_gateway_status(self, msg: dict):
        self._mark_gateway_seen()
        health_payload = dict(msg)
        health_payload["online"] = True
        health_payload["ts_seen"] = int(time.time())
        health_payload["ts_seen_iso"] = ts_to_iso(health_payload["ts_seen"])
        self._publish_json(mqtt_topic_gateway_health(), health_payload, retain=True)
        self._gateway_health_payload = dict(health_payload)
        self._gateway_health_last_pub = time.time()
        log.info(
            "Gateway status: up=%ss heap=%s rx=%s drops=%s crc_fail=%s cmd_ok=%s cmd_fail=%s",
            msg.get("uptime_s"),
            msg.get("free_heap"),
            msg.get("rx_packets"),
            msg.get("queue_drops"),
            msg.get("crc_failures"),
            msg.get("cmd_ok"),
            msg.get("cmd_fail"),
        )

    def _handle_cmd_ack(self, msg: dict):
        self._mark_gateway_seen()
        ack_payload = dict(msg)
        ack_payload["ts_seen"] = int(time.time())
        self._publish_json(mqtt_topic_gateway_ack(), ack_payload, retain=False)
        cmd = msg.get("cmd", "?")
        if msg.get("ok"):
            log.info("Gateway ACK: cmd=%s node=%s", cmd, msg.get("node_id", "-"))
        else:
            log.warning(
                "Gateway ACK failed: cmd=%s node=%s reason=%s err=%s",
                cmd,
                msg.get("node_id", "-"),
                msg.get("reason", "-"),
                msg.get("err", "-"),
            )

    def _normalize_serial_line(self, line: str) -> str:
        line = line.strip()
        if not line:
            return ""
        if line.startswith("{"):
            return line
        json_start = line.find("{")
        if json_start >= 0:
            return line[json_start:].strip()
        return line

    def _read_serial_line(self, ser: serial.Serial) -> str:
        raw = ser.readline().decode(errors="replace")
        return self._normalize_serial_line(raw)

    def _drain_startup_serial(self, ser: serial.Serial, duration_s: float):
        deadline = time.time() + duration_s
        while time.time() < deadline:
            try:
                line = self._read_serial_line(ser)
            except serial.SerialException as e:
                log.error("Serial error during startup drain: %s", e)
                self._mark_gateway_offline("serial error")
                return
            if not line:
                continue
            log.debug("↺ startup: %s", line)
            self.dispatch(line)

    def dispatch(self, line: str):
        line = self._normalize_serial_line(line)
        if not line:
            return
        try:
            msg = json.loads(line)
        except Exception:
            log.debug("Non-JSON line: %s", line[:80])
            return

        # Any valid JSON from serial means gateway is alive
        self._mark_gateway_seen()

        t = msg.get("type", "")
        try:
            if   t == "SENSOR":         self._handle_sensor(msg)
            elif t == "HEARTBEAT":      self._handle_heartbeat(msg)
            elif t == "HELLO":          self._handle_hello(msg)
            elif t == "GATEWAY_READY":  self._handle_gateway_ready(msg)
            elif t == "GATEWAY_STATUS": self._handle_gateway_status(msg)
            elif t == "CMD_ACK":        self._handle_cmd_ack(msg)
            else:
                log.debug("Unknown type: %s", t)
        except Exception:
            # Keep the bridge running even if a handler hits unexpected data.
            log.exception("Handler error for type=%s line=%s", t, line[:200])

    # ── Main loop ─────────────────────────────────────────────────────────────

    def run(self):
        # Connect MQTT
        self.client.connect(self.args.mqtt, self.args.mqtt_port, keepalive=60)
        self.client.loop_start()

        log.info("Bridge running. Ctrl+C to stop.")
        last_timeout_check = time.time()

        try:
            while True:
                if not self._serial or not self._serial.is_open:
                    if not self._open_serial():
                        time.sleep(self._serial_reconnect_delay_s)
                        continue
                    last_timeout_check = time.time()

                ser = self._serial
                try:
                    line = self._read_serial_line(ser)
                except serial.SerialException as e:
                    log.error("Serial error: %s", e)
                    self._reconnect_serial(f"serial error: {e}")
                    continue

                if line:
                    log.debug("← %s", line)
                    self.dispatch(line)

                # Check offline timeouts every 30s
                now = time.time()
                if now - last_timeout_check > 30:
                    self.tracker.check_timeouts()
                    if self._gateway_timeout_s > 0 and self._gateway_online and (now - self._gateway_last_seen > self._gateway_timeout_s):
                        self._reconnect_serial(f"no serial data for {self._gateway_timeout_s}s")
                    last_timeout_check = now

        except KeyboardInterrupt:
            log.info("Stopping.")
        finally:
            self._mark_gateway_offline("bridge stopping")
            self._close_serial()
            self.client.loop_stop()
            self.client.disconnect()
            if self._influx:
                self._influx.close()

# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Aguada Bridge v3.2")
    parser.add_argument("--port",            default="/dev/ttyACM0")
    parser.add_argument("--baud",            default=115200, type=int)
    parser.add_argument("--mqtt",            default="localhost")
    parser.add_argument("--mqtt-port",       default=1883, type=int)
    parser.add_argument("--config",          default=str(Path(__file__).parent / "reservoirs.yaml"))
    # Avoid passing secrets via CLI args (they show up in `ps`).
    # Prefer configuring via environment variables.
    parser.add_argument("--mqtt-user",       default=os.getenv("MQTT_USER", ""))
    parser.add_argument("--mqtt-password",   default=os.getenv("MQTT_PASS", ""))
    parser.add_argument("--offline-timeout", default=300, type=int,
                        help="Seconds without data before node is marked offline")
    parser.add_argument("--gateway-timeout", default=0, type=int,
                        help="Seconds without serial JSON before gateway is marked offline (0=disable timeout)")
    parser.add_argument("--serial-reconnect-delay", default=2.0, type=float,
                        help="Seconds to wait before retrying the serial connection after errors/disconnects")
    parser.add_argument("--influx-url",    default="",            help="InfluxDB 2.x URL (ex: http://localhost:8086). Omita para desativar.")
    parser.add_argument("--influx-token",  default="",            help="InfluxDB API token")
    parser.add_argument("--influx-org",    default="aguada",      help="InfluxDB org")
    parser.add_argument("--influx-bucket", default="reservoirs",  help="InfluxDB bucket")
    parser.add_argument("--debug",           action="store_true")
    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    Bridge(args).run()

if __name__ == "__main__":
    main()

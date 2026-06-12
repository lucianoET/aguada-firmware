#!/usr/bin/env python3
"""
Aguada — MQTT → InfluxDB writer
Assina o broker MQTT local onde o gateway WiFi publica,
calcula nível/volume e grava no InfluxDB 2.x.

Uso:
    python mqtt_to_influx.py [--broker 127.0.0.1] [--influx-url http://localhost:8086]

Dependências:
    pip install paho-mqtt influxdb-client pyyaml
"""

import argparse
import json
import logging
import os
import sys
import time
from pathlib import Path

import yaml

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("paho-mqtt não instalado: pip install paho-mqtt", file=sys.stderr)
    sys.exit(1)

try:
    from influxdb_client import InfluxDBClient, Point
    from influxdb_client.client.write_api import SYNCHRONOUS
except ImportError:
    print("influxdb-client não instalado: pip install influxdb-client", file=sys.stderr)
    sys.exit(1)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)-7s %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("mqtt2influx")

# Tópicos assinados
TOPICS = [
    "aguada/+/+/state",      # SENSOR
    "aguada/+/heartbeat",    # HEARTBEAT
    "aguada/+/hello",        # HELLO
    "aguada/gateway/status", # GATEWAY_STATUS
]

# ── Configuração de reservatórios ─────────────────────────────────────────────

def load_reservoirs(path: str) -> dict:
    """Retorna índice (NODE_ID_UPPER, sensor_id_int) → cfg"""
    with open(path) as f:
        data = yaml.safe_load(f)
    index = {}
    for node_id_str, sensors in data.get("reservoirs", {}).items():
        for s in sensors:
            key = (node_id_str.upper(), int(s["sensor_id"]))
            index[key] = s
    log.info("Reservatórios carregados: %d", len(index))
    return index

# ── Cálculo ───────────────────────────────────────────────────────────────────

def calculate(distance_cm: float, cfg: dict) -> dict:
    level_max   = float(cfg["level_max_cm"])
    vol_max     = float(cfg["volume_max_L"])
    offset      = float(cfg["sensor_offset_cm"])
    level_cm    = max(0.0, min(level_max - (distance_cm - offset), level_max))
    pct         = round((level_cm / level_max) * 100, 1)
    volume_L    = int(round((level_cm / level_max) * vol_max))
    return {"level_cm": round(level_cm, 1), "pct": pct, "volume_L": volume_L}

def normalize_vbat(raw) -> float | None:
    if raw is None: return None
    try:
        v = float(raw)
    except (TypeError, ValueError):
        return None
    if v < 0: return None
    if v > 10: v /= 10.0
    return round(v, 2)

# ── InfluxDB writer ───────────────────────────────────────────────────────────

class InfluxWriter:
    def __init__(self, url: str, token: str, org: str, bucket: str):
        self._bucket = bucket
        self._org    = org
        self._client = InfluxDBClient(url=url, token=token, org=org)
        self._write  = self._client.write_api(write_options=SYNCHRONOUS)
        log.info("InfluxDB: %s  org=%s  bucket=%s", url, org, bucket)

    def write_sensor(self, node_id: str, sensor_id: int, alias: str,
                     distance_cm: float, calc: dict,
                     rssi: int | None, vbat: float | None, seq: int | None):
        p = (
            Point("reservoir")
            .tag("node_id",   node_id)
            .tag("sensor_id", str(sensor_id))
            .tag("alias",     alias)
            .field("distance_cm", float(distance_cm))
            .field("level_cm",    float(calc["level_cm"]))
            .field("pct",         float(calc["pct"]))
            .field("volume_L",    int(calc["volume_L"]))
        )
        if rssi is not None:
            p = p.field("rssi", int(rssi))
        if vbat is not None:
            p = p.field("vbat", float(vbat))
        if seq is not None:
            p = p.field("seq", int(seq))
        self._write.write(bucket=self._bucket, org=self._org, record=p)

    def write_gateway(self, mac: str | None, fw: str | None,
                      uptime_s: int | None, free_heap: int | None,
                      rx_packets: int | None, crc_failures: int | None,
                      wifi_rssi: int | None):
        p = Point("gateway").tag("mac", mac or "unknown")
        if uptime_s    is not None: p = p.field("uptime_s",    int(uptime_s))
        if free_heap   is not None: p = p.field("free_heap",   int(free_heap))
        if rx_packets  is not None: p = p.field("rx_packets",  int(rx_packets))
        if crc_failures is not None: p = p.field("crc_failures", int(crc_failures))
        if wifi_rssi   is not None: p = p.field("wifi_rssi",   int(wifi_rssi))
        if fw:                       p = p.tag("fw", fw)
        self._write.write(bucket=self._bucket, org=self._org, record=p)

# ── Processamento de mensagens ────────────────────────────────────────────────

class Processor:
    def __init__(self, reservoirs: dict, influx: InfluxWriter):
        self._res    = reservoirs
        self._influx = influx
        # Dedup: (node_id, sensor_id, seq) → ts
        self._seen: dict[tuple, float] = {}

    def handle(self, topic: str, raw: dict):
        msg_type = str(raw.get("type", "")).upper()

        if msg_type == "SENSOR":
            self._handle_sensor(raw)
        elif msg_type in ("GATEWAY_STATUS", "GATEWAY_READY"):
            self._handle_gateway(raw)
        elif msg_type == "HEARTBEAT":
            log.debug("HEARTBEAT node=%s", raw.get("node_id", "?"))
        elif msg_type == "HELLO":
            log.info("HELLO node=%s fw=%s sensors=%s",
                     raw.get("node_id"), raw.get("fw_version"), raw.get("num_sensors"))

    def _handle_sensor(self, msg: dict):
        node_id_raw = str(msg.get("node_id", "")).upper()
        sensor_id   = int(msg.get("sensor_id", 1))
        distance    = msg.get("distance_cm")
        flags       = int(msg.get("flags", 0) or 0)
        seq         = msg.get("seq")

        if distance is None or (flags & 0x04):  # FLAG_SENSOR_ERROR
            log.debug("Leitura ignorada: node=%s flags=0x%02X", node_id_raw, flags)
            return

        try:
            distance = float(distance)
        except (TypeError, ValueError):
            return
        if distance < 0:
            return

        # Dedup por seq
        if seq is not None:
            key = (node_id_raw, sensor_id, int(seq))
            now = time.time()
            if key in self._seen:
                return
            self._seen[key] = now
            if len(self._seen) > 512:
                cutoff = now - 120
                self._seen = {k: v for k, v in self._seen.items() if v > cutoff}

        cfg = self._res.get((node_id_raw, sensor_id))
        if not cfg:
            log.warning("Reservatório não configurado: %s sensor=%d", node_id_raw, sensor_id)
            return

        calc = calculate(distance, cfg)
        rssi = int(msg["rssi"]) if msg.get("rssi") is not None else None
        vbat = normalize_vbat(msg.get("vbat"))
        alias = cfg.get("alias", f"{node_id_raw}/{sensor_id}")

        try:
            self._influx.write_sensor(
                node_id_raw, sensor_id, alias,
                distance, calc, rssi, vbat,
                int(seq) if seq is not None else None,
            )
            log.info("%-6s  dist=%5.1f cm  nível=%5.1f cm  %5.1f%%  %dL  rssi=%s",
                     alias, distance, calc["level_cm"], calc["pct"], calc["volume_L"],
                     f"{rssi}dBm" if rssi is not None else "n/a")
        except Exception as e:
            log.error("InfluxDB write falhou: %s", e)

    def _handle_gateway(self, msg: dict):
        try:
            self._influx.write_gateway(
                mac=msg.get("mac"),
                fw=msg.get("fw") or msg.get("fw_version"),
                uptime_s=msg.get("uptime_s"),
                free_heap=msg.get("free_heap"),
                rx_packets=msg.get("rx_packets"),
                crc_failures=msg.get("crc_failures"),
                wifi_rssi=msg.get("wifi_rssi"),
            )
            log.info("Gateway: uptime=%ss heap=%s rx=%s crc_fail=%s wifi=%sdBm",
                     msg.get("uptime_s"), msg.get("free_heap"),
                     msg.get("rx_packets"), msg.get("crc_failures"),
                     msg.get("wifi_rssi"))
        except Exception as e:
            log.error("InfluxDB gateway write falhou: %s", e)

# ── Loop MQTT ─────────────────────────────────────────────────────────────────

def run(args):
    reservoirs = load_reservoirs(args.config)
    influx = InfluxWriter(
        url=args.influx_url,
        token=args.influx_token,
        org=args.influx_org,
        bucket=args.influx_bucket,
    )
    proc = Processor(reservoirs, influx)

    def format_reason_code(reason_code) -> str:
        if reason_code is None:
            return "unknown"
        try:
            return str(int(reason_code))
        except (TypeError, ValueError):
            return str(reason_code)

    def on_connect(client, userdata, flags, rc, *a):
        if rc == 0:
            log.info("MQTT conectado: %s:%d", args.broker, args.mqtt_port)
            for t in TOPICS:
                client.subscribe(t)
                log.info("Assinado: %s", t)
        else:
            log.error("MQTT erro rc=%s", format_reason_code(rc))

    def on_disconnect(client, userdata, *callback_args):
        disconnect_flags = None
        reason_code = None

        if callback_args:
            first_arg = callback_args[0]
            if hasattr(first_arg, "is_disconnect_packet_from_server"):
                disconnect_flags = first_arg
                if len(callback_args) > 1:
                    reason_code = callback_args[1]
            else:
                reason_code = first_arg
                if len(callback_args) > 1 and hasattr(callback_args[1], "is_disconnect_packet_from_server"):
                    disconnect_flags = callback_args[1]

        if disconnect_flags is not None:
            log.warning(
                "MQTT desconectado rc=%s flags=%s",
                format_reason_code(reason_code),
                disconnect_flags,
            )
        else:
            log.warning("MQTT desconectado rc=%s", format_reason_code(reason_code))

    def on_message(client, userdata, msg):
        try:
            raw = json.loads(msg.payload.decode("utf-8", errors="replace"))
        except json.JSONDecodeError:
            return
        try:
            proc.handle(msg.topic, raw)
        except Exception as e:
            log.error("Erro ao processar mensagem: %s", e)

    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id="aguada-mqtt2influx")
    except AttributeError:
        client = mqtt.Client(client_id="aguada-mqtt2influx")

    client.on_connect    = on_connect
    client.on_disconnect = on_disconnect
    client.on_message    = on_message
    client.reconnect_delay_set(min_delay=2, max_delay=30)

    while True:
        try:
            client.connect(args.broker, args.mqtt_port, keepalive=60)
            client.loop_forever()
        except Exception as e:
            log.error("Conexão MQTT falhou: %s — tentando em 10s", e)
            time.sleep(10)

# ── Entrypoint ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    SCRIPT_DIR = Path(__file__).parent

    parser = argparse.ArgumentParser(description="Aguada MQTT→InfluxDB")
    parser.add_argument("--broker",       default=os.environ.get("MQTT_HOST",  "127.0.0.1"))
    parser.add_argument("--mqtt-port",    type=int, default=int(os.environ.get("MQTT_PORT", 1883)))
    parser.add_argument("--config",       default=str(SCRIPT_DIR / "reservoirs.yaml"))
    parser.add_argument("--influx-url",   default=os.environ.get("INFLUX_URL",   "http://localhost:8086"))
    parser.add_argument("--influx-token", default=os.environ.get("INFLUX_TOKEN", "aguada-admin-token-2024"))
    parser.add_argument("--influx-org",   default=os.environ.get("INFLUX_ORG",   "aguada"))
    parser.add_argument("--influx-bucket",default=os.environ.get("INFLUX_BUCKET","reservoirs"))
    args = parser.parse_args()

    log.info("Aguada MQTT→InfluxDB iniciando")
    log.info("  MQTT:   %s:%d", args.broker, args.mqtt_port)
    log.info("  Influx: %s  org=%s  bucket=%s", args.influx_url, args.influx_org, args.influx_bucket)
    run(args)

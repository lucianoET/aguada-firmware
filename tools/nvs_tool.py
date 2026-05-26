#!/usr/bin/env python3
"""
Aguada NVS Tool
Sends configuration commands to nodes via the gateway serial port.

Usage:
    # Set node 0x2EC4 to 2 sensors with specific pins:
    python nvs_tool.py --port /dev/ttyACM0 config 0x2EC4 \
        --num-sensors 2 \
        --measure 30 --send 120 --heartbeat 60

    # Restart a node:
    python nvs_tool.py --port /dev/ttyACM0 restart 0x7758
"""

import argparse
import json
import serial
import time


def open_port(port: str, baud: int = 115200) -> serial.Serial:
    ser = serial.Serial(port, baud, timeout=2)
    time.sleep(0.1)
    return ser


def send_json(ser: serial.Serial, obj: dict):
    line = json.dumps(obj) + "\n"
    ser.write(line.encode())
    print(f"→ {line.strip()}")
    time.sleep(0.5)
    # Read any response
    while ser.in_waiting:
        resp = ser.readline().decode(errors="replace").strip()
        if resp:
            print(f"← {resp}")


def cmd_restart(args):
    ser = open_port(args.port)
    send_json(ser, {"cmd": "RESTART", "node_id": args.node_id})
    ser.close()


def cmd_config(args):
    ser = open_port(args.port)

    sensors = []
    for i in range(1, args.num_sensors + 1):
        sensors.append({
            "id":       i,
            "trig_pin": args.trig1 if i == 1 else args.trig2,
            "echo_pin": args.echo1 if i == 1 else args.echo2,
            "enabled":  True,
        })

    payload = {
        "cmd":                "CONFIG",
        "node_id":            args.node_id,
        "num_sensors":        args.num_sensors,
        "sensor":             sensors,
        "interval_measure_s": args.measure,
        "interval_send_s":    args.send,
        "heartbeat_s":        args.heartbeat,
        "filter_window":      args.filter_window,
        "filter_outlier_cm":  args.outlier,
        "filter_threshold_cm":args.threshold,
        "vbat_pin":           args.vbat_pin,
        "vbat_enabled":       args.vbat_enabled,
    }
    send_json(ser, payload)
    ser.close()


def main():
    parser = argparse.ArgumentParser(description="Aguada NVS Tool")
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", default=115200, type=int)

    sub = parser.add_subparsers(dest="command", required=True)

    # restart
    p_rst = sub.add_parser("restart", help="Restart a node")
    p_rst.add_argument("node_id")
    p_rst.set_defaults(func=cmd_restart)

    # config
    p_cfg = sub.add_parser("config", help="Send config to a node")
    p_cfg.add_argument("node_id")
    p_cfg.add_argument("--num-sensors",   default=1,  type=int)
    p_cfg.add_argument("--trig1",         default=1,  type=int)
    p_cfg.add_argument("--echo1",         default=0,  type=int)
    p_cfg.add_argument("--trig2",         default=3,  type=int)
    p_cfg.add_argument("--echo2",         default=2,  type=int)
    p_cfg.add_argument("--measure",       default=30,  type=int)
    p_cfg.add_argument("--send",          default=120, type=int)
    p_cfg.add_argument("--heartbeat",     default=60,  type=int)
    p_cfg.add_argument("--filter-window", default=5,   type=int)
    p_cfg.add_argument("--outlier",       default=10,  type=int)
    p_cfg.add_argument("--threshold",     default=2,   type=int)
    p_cfg.add_argument("--vbat-pin",      default=0,   type=int,
                       help="GPIO pin for vbat ADC (0=unchanged)")
    p_cfg.add_argument("--vbat-enabled",  action="store_true",
                       help="Enable vbat ADC reading")
    p_cfg.set_defaults(func=cmd_config)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()

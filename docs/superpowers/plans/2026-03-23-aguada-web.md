# Aguada Web Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Criar sistema web standalone em Docker para monitoramento de reservatórios de água do CMASM — lê dados do gateway ESP32-S3 via USB serial, armazena em SQLite, e serve dashboard web em tempo real com 5 páginas, incluindo geração de relatório PDF diário.

**Architecture:** Bridge thread Python lê JSON do serial (ou simula), calcula nível/volume, grava em SQLite e notifica clientes via WebSocket. FastAPI serve API REST + WebSocket. Nginx serve frontend HTML estático. Dois containers Docker Compose.

**Tech Stack:** Python 3.12, FastAPI, aiosqlite, pyserial, WeasyPrint, APScheduler, paho-mqtt (opcional). Frontend: HTML + Tailwind CSS CDN + Alpine.js + Chart.js. Nginx. Docker Compose.

**Spec:** `docs/superpowers/specs/2026-03-23-aguada-web-design.md`

---

## Mapa de Arquivos

```
aguada-web/
├── backend/
│   ├── main.py           # FastAPI app: rotas REST, WebSocket, startup bridge + scheduler
│   ├── bridge.py         # Thread serial: lê JSON → calcula → SQLite → notifica WS
│   ├── db.py             # Schema SQLite, init_db(), queries (get_state, insert_reading, etc.)
│   ├── calc.py           # Funções puras: level_cm, pct, volume_l, consumption_events, decimate
│   ├── report.py         # Geração HTML de relatório + WeasyPrint PDF + scheduler job
│   └── reservoirs.yaml   # Parâmetros dos reservatórios (chaves lowercase: "0x7758")
├── frontend/
│   ├── index.html        # Dashboard tempo real (WebSocket)
│   ├── scada.html        # Diagrama SCADA SVG
│   ├── history.html      # Gráficos históricos (Chart.js)
│   ├── consumption.html  # Balanço hídrico (Chart.js)
│   └── report.html       # Preview + download PDF
├── tests/
│   ├── test_calc.py      # Testes unitários de calc.py
│   ├── test_db.py        # Testes de db.py com SQLite em memória
│   ├── test_api.py       # Testes de integração das rotas FastAPI
│   └── conftest.py       # Fixtures compartilhadas (app TestClient, db em memória)
├── docker-compose.yml
├── Dockerfile
├── nginx.conf
├── requirements.txt
└── .env.example
```

**Regras de responsabilidade:**
- `calc.py` — funções puras, sem I/O. Testável sem banco.
- `db.py` — todo acesso ao SQLite. Sem lógica de negócio.
- `bridge.py` — orquestra: serial → calc → db → ws notify. Não faz cálculos diretos.
- `main.py` — rotas, WebSocket manager, startup/shutdown. Não acessa banco diretamente.
- `report.py` — gera HTML string + chama WeasyPrint. Recebe dados via parâmetro.

---

## Task 1: Estrutura do projeto e dependências

**Files:**
- Create: `aguada-web/requirements.txt`
- Create: `aguada-web/.env.example`
- Create: `aguada-web/backend/reservoirs.yaml`

- [ ] **Step 1.1: Criar pasta principal**

```bash
mkdir -p aguada-web/backend aguada-web/frontend aguada-web/tests aguada-web/data/reports
cd aguada-web
```

- [ ] **Step 1.2: Criar `requirements.txt`**

```
fastapi==0.115.6
uvicorn[standard]==0.32.1
aiosqlite==0.20.0
pyserial==3.5
pyyaml==6.0.2
paho-mqtt==2.1.0
weasyprint==62.3
apscheduler==3.10.4
httpx==0.27.2
pytest==8.3.4
pytest-asyncio==0.24.0
```

- [ ] **Step 1.3: Criar `.env.example`**

```
SERIAL_PORT=/dev/ttyUSB0
DATA_DIR=/data
TZ=America/Sao_Paulo
MQTT_HOST=
MQTT_PORT=1883
MQTT_USER=
MQTT_PASS=
```

- [ ] **Step 1.4: Criar `backend/reservoirs.yaml` com chaves lowercase**

Copiar conteúdo de `tools/reservoirs.yaml` convertendo todas as chaves para lowercase e `volume_max_L` para `volume_max_l`:

```yaml
reservoirs:
  "0x7758":
    - sensor_id: 1
      alias: CON
      name: Castelo de Consumo
      level_max_cm: 450
      volume_max_l: 80000
      sensor_offset_cm: 20

  "0xee02":
    - sensor_id: 1
      alias: CAV
      name: Castelo de Incêndio
      level_max_cm: 450
      volume_max_l: 80000
      sensor_offset_cm: 20

  "0x2ec4":
    - sensor_id: 1
      alias: CB31
      name: Casa de Bombas Nº3-1
      level_max_cm: 200
      volume_max_l: 40000
      sensor_offset_cm: 10
    - sensor_id: 2
      alias: CB32
      name: Casa de Bombas Nº3-2
      level_max_cm: 200
      volume_max_l: 40000
      sensor_offset_cm: 10

  "0x9eac":
    - sensor_id: 1
      alias: CIE1
      name: Cisterna IE-1
      level_max_cm: 200
      volume_max_l: 245000
      sensor_offset_cm: 10
    - sensor_id: 2
      alias: CIE2
      name: Cisterna IE-2
      level_max_cm: 200
      volume_max_l: 245000
      sensor_offset_cm: 10

  "0x3456":
    - sensor_id: 1
      alias: CBIF1
      name: Casa de Bombas IF-1
      level_max_cm: 200
      volume_max_l: 40000
      sensor_offset_cm: 10
    - sensor_id: 2
      alias: CBIF2
      name: Casa de Bombas IF-2
      level_max_cm: 200
      volume_max_l: 40000
      sensor_offset_cm: 10
```

- [ ] **Step 1.5: Instalar dependências localmente para desenvolvimento**

```bash
cd aguada-web
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

- [ ] **Step 1.6: Commit**

```bash
git add aguada-web/
git commit -m "feat(aguada-web): scaffold project structure and dependencies"
```

---

## Task 2: Cálculos puros (`calc.py`)

**Files:**
- Create: `aguada-web/backend/calc.py`
- Create: `aguada-web/tests/test_calc.py`

- [ ] **Step 2.1: Escrever testes de `calc.py`**

```python
# tests/test_calc.py
import pytest
from backend.calc import calc_level, calc_consumption_events, decimate_readings

def test_calc_level_normal():
    result = calc_level(distance_cm=215, level_max_cm=450, sensor_offset_cm=20)
    # level = clamp(450 - (215 - 20), 0, 450) = clamp(255, 0, 450) = 255
    assert result["level_cm"] == pytest.approx(255.0)
    assert result["pct"] == pytest.approx(255 / 450 * 100, rel=1e-4)
    assert result["volume_l"] == pytest.approx(255 / 450 * 80000, rel=1e-4)

def test_calc_level_clamp_zero():
    # sensor muito longe → level negativo → clamp para 0
    result = calc_level(distance_cm=500, level_max_cm=200, sensor_offset_cm=10)
    assert result["level_cm"] == 0.0
    assert result["pct"] == 0.0

def test_calc_level_clamp_max():
    # sensor muito perto → level > max → clamp para max
    result = calc_level(distance_cm=0, level_max_cm=200, sensor_offset_cm=10)
    assert result["level_cm"] == 200.0
    assert result["pct"] == 100.0

def test_calc_level_error_distance():
    # distance_cm = None (erro do sensor)
    result = calc_level(distance_cm=None, level_max_cm=200, sensor_offset_cm=10)
    assert result["level_cm"] is None
    assert result["pct"] is None

def test_consumption_events_classifies_consumption():
    # delta < -50 → consumption
    readings = [
        {"ts": 3600, "volume_l": 45000},
        {"ts": 7100, "volume_l": 43000},
    ]
    events = calc_consumption_events(readings, date="2026-03-23")
    assert len(events) == 1
    assert events[0]["type"] == "consumption"
    assert events[0]["delta_l"] == pytest.approx(-2000)

def test_consumption_events_classifies_supply():
    # delta > 50 → supply
    readings = [
        {"ts": 3600, "volume_l": 43000},
        {"ts": 7100, "volume_l": 50000},
    ]
    events = calc_consumption_events(readings, date="2026-03-23")
    assert events[0]["type"] == "supply"

def test_consumption_events_classifies_stable():
    # |delta| <= 50 → stable
    readings = [
        {"ts": 3600, "volume_l": 45000},
        {"ts": 7100, "volume_l": 45030},
    ]
    events = calc_consumption_events(readings, date="2026-03-23")
    assert events[0]["type"] == "stable"

def test_decimate_passthrough_if_under_limit():
    readings = [{"ts": i, "volume_l": i * 10, "level_cm": i, "pct": i * 0.1}
                for i in range(100)]
    result = decimate_readings(readings, max_points=500)
    assert result == readings

def test_decimate_reduces_to_max_points():
    readings = [{"ts": i, "volume_l": i * 10, "level_cm": i, "pct": i * 0.1}
                for i in range(1000)]
    result = decimate_readings(readings, max_points=500)
    assert len(result) <= 500
```

- [ ] **Step 2.2: Rodar testes — verificar que falham**

```bash
cd aguada-web
source .venv/bin/activate
python -m pytest tests/test_calc.py -v 2>&1 | head -30
```

Esperado: `ModuleNotFoundError` ou `ImportError` — `calc.py` não existe ainda.

- [ ] **Step 2.3: Criar `backend/calc.py`**

```python
# backend/calc.py
"""Funções puras de cálculo — sem I/O, sem banco."""
from __future__ import annotations
import math
from typing import Optional


def calc_level(
    distance_cm: Optional[float],
    level_max_cm: float,
    sensor_offset_cm: float,
    volume_max_l: float = 0,
) -> dict:
    """Calcula level_cm, pct e volume_l a partir da distância medida."""
    if distance_cm is None:
        return {"level_cm": None, "pct": None, "volume_l": None}

    level = level_max_cm - (distance_cm - sensor_offset_cm)
    level = max(0.0, min(float(level_max_cm), level))
    pct = level / level_max_cm * 100.0
    volume = pct / 100.0 * volume_max_l
    return {"level_cm": round(level, 1), "pct": round(pct, 2), "volume_l": round(volume, 1)}


def calc_consumption_events(readings: list[dict], date: str) -> list[dict]:
    """
    Agrupa leituras por hora e retorna eventos de consumo/abastecimento.
    Cada reading deve ter: {ts (unix int), volume_l}.
    """
    from collections import defaultdict
    import datetime

    buckets: dict[int, list[dict]] = defaultdict(list)
    for r in readings:
        hour = datetime.datetime.fromtimestamp(r["ts"]).hour
        buckets[hour].append(r)

    events = []
    for hour in sorted(buckets.keys()):
        pts = sorted(buckets[hour], key=lambda x: x["ts"])
        vol_start = pts[0]["volume_l"]
        vol_end = pts[-1]["volume_l"]
        delta = vol_end - vol_start
        if delta < -50:
            etype = "consumption"
        elif delta > 50:
            etype = "supply"
        else:
            etype = "stable"
        events.append({
            "hour": f"{hour:02d}:00",
            "vol_start": round(vol_start, 1),
            "vol_end": round(vol_end, 1),
            "delta_l": round(delta, 1),
            "type": etype,
        })
    return events


def decimate_readings(readings: list[dict], max_points: int = 500) -> list[dict]:
    """
    Decimação por média de intervalo.
    Se len(readings) <= max_points, retorna sem modificação.
    Caso contrário, divide em max_points intervalos e retorna a média de volume_l por intervalo.
    """
    n = len(readings)
    if n <= max_points:
        return readings

    bucket_size = n / max_points
    result = []
    for i in range(max_points):
        start = int(i * bucket_size)
        end = int((i + 1) * bucket_size)
        bucket = readings[start:end]
        if not bucket:
            continue
        mid = bucket[len(bucket) // 2]
        avg_volume = sum(r["volume_l"] for r in bucket) / len(bucket)
        avg_level = sum(r["level_cm"] for r in bucket if r.get("level_cm")) / len(bucket)
        avg_pct = sum(r["pct"] for r in bucket if r.get("pct")) / len(bucket)
        result.append({
            "ts": mid["ts"],
            "volume_l": round(avg_volume, 1),
            "level_cm": round(avg_level, 1),
            "pct": round(avg_pct, 2),
        })
    return result
```

- [ ] **Step 2.4: Rodar testes — verificar que passam**

```bash
python -m pytest tests/test_calc.py -v
```

Esperado: todos os testes `PASSED`.

- [ ] **Step 2.5: Commit**

```bash
git add aguada-web/backend/calc.py aguada-web/tests/test_calc.py
git commit -m "feat(aguada-web): calc.py — level, consumption events, decimate"
```

---

## Task 3: Banco de dados (`db.py`)

**Files:**
- Create: `aguada-web/backend/db.py`
- Create: `aguada-web/tests/test_db.py`
- Create: `aguada-web/tests/conftest.py`

- [ ] **Step 3.1: Criar `tests/conftest.py`**

```python
# tests/conftest.py
import asyncio
import pytest
import pytest_asyncio
import aiosqlite
from backend.db import init_db

@pytest.fixture
def event_loop():
    loop = asyncio.new_event_loop()
    yield loop
    loop.close()

@pytest_asyncio.fixture
async def db(tmp_path):
    db_path = str(tmp_path / "test.db")
    async with aiosqlite.connect(db_path) as conn:
        await init_db(conn)
        yield conn
```

- [ ] **Step 3.2: Escrever testes de `db.py`**

```python
# tests/test_db.py
import time
import pytest
import pytest_asyncio
from backend.db import init_db, insert_reading, upsert_state, get_all_states, get_history

@pytest.mark.asyncio
async def test_init_db_creates_tables(db):
    async with db.execute("SELECT name FROM sqlite_master WHERE type='table'") as cur:
        tables = {row[0] async for row in cur}
    assert "readings" in tables
    assert "reservoir_state" in tables

@pytest.mark.asyncio
async def test_insert_reading(db):
    now = int(time.time())
    await insert_reading(db, {
        "ts": now, "node_id": "0x7758", "sensor_id": 1, "alias": "CON",
        "distance_cm": 215, "level_cm": 255, "volume_l": 45333, "pct": 56.7,
        "rssi": -62, "vbat": 3.3, "seq": 1
    })
    async with db.execute("SELECT alias FROM readings") as cur:
        rows = [row async for row in cur]
    assert len(rows) == 1
    assert rows[0][0] == "CON"

@pytest.mark.asyncio
async def test_upsert_state_creates_and_updates(db):
    now = int(time.time())
    state = {
        "alias": "CON", "node_id": "0x7758", "sensor_id": 1,
        "name": "Castelo de Consumo", "ts": now,
        "level_cm": 255, "volume_l": 45333, "pct": 56.7,
        "level_max_cm": 450, "volume_max_l": 80000, "rssi": -62,
    }
    await upsert_state(db, state)
    async with db.execute("SELECT pct FROM reservoir_state WHERE alias='CON'") as cur:
        row = await cur.fetchone()
    assert abs(row[0] - 56.7) < 0.01

    # update
    state["pct"] = 80.0
    await upsert_state(db, state)
    async with db.execute("SELECT pct FROM reservoir_state WHERE alias='CON'") as cur:
        row = await cur.fetchone()
    assert abs(row[0] - 80.0) < 0.01

@pytest.mark.asyncio
async def test_get_all_states_returns_online_flag(db):
    now = int(time.time())
    await upsert_state(db, {
        "alias": "CON", "node_id": "0x7758", "sensor_id": 1,
        "name": "Castelo de Consumo", "ts": now,
        "level_cm": 255, "volume_l": 45333, "pct": 56.7,
        "level_max_cm": 450, "volume_max_l": 80000, "rssi": -62,
    })
    states = await get_all_states(db)
    assert len(states) == 1
    assert states[0]["online"] is True

@pytest.mark.asyncio
async def test_get_history_returns_readings(db):
    now = int(time.time())
    for i in range(5):
        await insert_reading(db, {
            "ts": now - i * 100, "node_id": "0x7758", "sensor_id": 1, "alias": "CON",
            "distance_cm": 215 + i, "level_cm": 255 - i, "volume_l": 45000 - i * 100,
            "pct": 56.0, "rssi": -62, "vbat": 3.3, "seq": i
        })
    rows = await get_history(db, alias="CON", since_ts=now - 600)
    assert len(rows) == 5
```

- [ ] **Step 3.3: Rodar testes — verificar que falham**

```bash
python -m pytest tests/test_db.py -v 2>&1 | head -20
```

Esperado: `ImportError` — `db.py` não existe.

- [ ] **Step 3.4: Criar `backend/db.py`**

```python
# backend/db.py
"""Acesso ao SQLite — schema, inserts, queries. Sem lógica de negócio."""
import time
import aiosqlite

SCHEMA = """
CREATE TABLE IF NOT EXISTS readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts          INTEGER NOT NULL,
    node_id     TEXT    NOT NULL,
    sensor_id   INTEGER NOT NULL,
    alias       TEXT    NOT NULL,
    distance_cm REAL,
    level_cm    REAL,
    volume_l    REAL,
    pct         REAL,
    rssi        INTEGER,
    vbat        REAL,
    seq         INTEGER
);
CREATE INDEX IF NOT EXISTS idx_readings_ts    ON readings(ts DESC);
CREATE INDEX IF NOT EXISTS idx_readings_alias ON readings(alias, ts DESC);

CREATE TABLE IF NOT EXISTS reservoir_state (
    alias        TEXT PRIMARY KEY,
    node_id      TEXT,
    sensor_id    INTEGER,
    name         TEXT,
    ts           INTEGER,
    level_cm     REAL,
    volume_l     REAL,
    pct          REAL,
    level_max_cm REAL,
    volume_max_l REAL,
    rssi         INTEGER,
    online       INTEGER DEFAULT 1
);
"""

async def init_db(conn: aiosqlite.Connection) -> None:
    await conn.executescript(SCHEMA)
    await conn.commit()


async def insert_reading(conn: aiosqlite.Connection, r: dict) -> None:
    await conn.execute(
        """INSERT INTO readings
           (ts, node_id, sensor_id, alias, distance_cm, level_cm, volume_l, pct, rssi, vbat, seq)
           VALUES (:ts, :node_id, :sensor_id, :alias, :distance_cm, :level_cm, :volume_l,
                   :pct, :rssi, :vbat, :seq)""",
        r,
    )
    await conn.commit()


async def upsert_state(conn: aiosqlite.Connection, s: dict) -> None:
    await conn.execute(
        """INSERT INTO reservoir_state
           (alias, node_id, sensor_id, name, ts, level_cm, volume_l, pct,
            level_max_cm, volume_max_l, rssi)
           VALUES (:alias, :node_id, :sensor_id, :name, :ts, :level_cm, :volume_l, :pct,
                   :level_max_cm, :volume_max_l, :rssi)
           ON CONFLICT(alias) DO UPDATE SET
               node_id=excluded.node_id, sensor_id=excluded.sensor_id,
               ts=excluded.ts, level_cm=excluded.level_cm, volume_l=excluded.volume_l,
               pct=excluded.pct, rssi=excluded.rssi""",
        s,
    )
    await conn.commit()


async def get_all_states(conn: aiosqlite.Connection) -> list[dict]:
    now = int(time.time())
    conn.row_factory = aiosqlite.Row
    async with conn.execute("SELECT * FROM reservoir_state") as cur:
        rows = await cur.fetchall()
    result = []
    for row in rows:
        d = dict(row)
        d["online"] = (now - (d["ts"] or 0)) < 600
        result.append(d)
    return result


async def get_history(
    conn: aiosqlite.Connection, alias: str, since_ts: int
) -> list[dict]:
    conn.row_factory = aiosqlite.Row
    async with conn.execute(
        "SELECT ts, level_cm, volume_l, pct FROM readings WHERE alias=? AND ts>=? ORDER BY ts ASC",
        (alias, since_ts),
    ) as cur:
        rows = await cur.fetchall()
    return [dict(r) for r in rows]


async def get_readings_for_date(
    conn: aiosqlite.Connection, alias: str, date_str: str
) -> list[dict]:
    """Retorna todas as leituras de um alias em um dia (YYYY-MM-DD, fuso UTC)."""
    import datetime
    day = datetime.date.fromisoformat(date_str)
    ts_start = int(datetime.datetime(day.year, day.month, day.day, 0, 0, 0).timestamp())
    ts_end = ts_start + 86400
    conn.row_factory = aiosqlite.Row
    async with conn.execute(
        "SELECT ts, volume_l, level_cm, pct FROM readings WHERE alias=? AND ts>=? AND ts<? ORDER BY ts ASC",
        (alias, ts_start, ts_end),
    ) as cur:
        rows = await cur.fetchall()
    return [dict(r) for r in rows]
```

- [ ] **Step 3.5: Criar `tests/__init__.py` e `backend/__init__.py` para imports**

```bash
touch aguada-web/tests/__init__.py aguada-web/backend/__init__.py
```

- [ ] **Step 3.6: Rodar testes**

```bash
python -m pytest tests/test_db.py tests/test_calc.py -v
```

Esperado: todos `PASSED`.

- [ ] **Step 3.7: Commit**

```bash
git add aguada-web/backend/db.py aguada-web/tests/test_db.py aguada-web/tests/conftest.py aguada-web/backend/__init__.py aguada-web/tests/__init__.py
git commit -m "feat(aguada-web): db.py — schema SQLite, insert, upsert, queries"
```

---

## Task 4: Bridge serial (`bridge.py`)

**Files:**
- Create: `aguada-web/backend/bridge.py`

Nota: Bridge tem I/O serial e threads — não tem teste unitário isolado. Testado via integração no Task 6.

- [ ] **Step 4.1: Criar `backend/bridge.py`**

```python
# backend/bridge.py
"""
Bridge serial → SQLite + WebSocket notify.
Roda como thread daemon iniciada pelo FastAPI startup.
"""
import asyncio
import json
import logging
import os
import random
import threading
import time
from pathlib import Path
from typing import Callable, Optional

import yaml

from .calc import calc_level
from .db import insert_reading, upsert_state

logger = logging.getLogger("bridge")

# Carrega reservoirs.yaml uma vez
_yaml_path = Path(__file__).parent / "reservoirs.yaml"
_raw = yaml.safe_load(_yaml_path.read_text())["reservoirs"]

# Índice: (node_id_lower, sensor_id) → dict de parâmetros
RESERVOIR_INDEX: dict[tuple[str, int], dict] = {}
for node_id, sensors in _raw.items():
    for s in sensors:
        RESERVOIR_INDEX[(node_id.lower(), s["sensor_id"])] = s


def _process_message(raw: dict) -> Optional[dict]:
    """Valida e enriquece uma mensagem do gateway. Retorna dict pronto para DB ou None."""
    try:
        node_id = raw.get("node", "").lower()
        sensor_id = int(raw.get("sensor", 0))
        distance_cm = raw.get("dist")
        rssi = raw.get("rssi")
        vbat_raw = raw.get("vbat")
        seq = raw.get("seq", 0)
        ts = raw.get("ts") or int(time.time())
        vbat = vbat_raw / 10.0 if vbat_raw is not None else None

        params = RESERVOIR_INDEX.get((node_id, sensor_id))
        if params is None:
            logger.warning("node_id=%s sensor=%d não encontrado no reservoirs.yaml", node_id, sensor_id)
            return None

        calc = calc_level(
            distance_cm=distance_cm,
            level_max_cm=params["level_max_cm"],
            sensor_offset_cm=params["sensor_offset_cm"],
            volume_max_l=params["volume_max_l"],
        )

        return {
            "ts": ts,
            "node_id": node_id,
            "sensor_id": sensor_id,
            "alias": params["alias"],
            "distance_cm": distance_cm,
            "level_cm": calc["level_cm"],
            "volume_l": calc["volume_l"],
            "pct": calc["pct"],
            "rssi": rssi,
            "vbat": vbat,
            "seq": seq,
            # para upsert_state
            "name": params["name"],
            "level_max_cm": params["level_max_cm"],
            "volume_max_l": params["volume_max_l"],
        }
    except Exception as e:
        logger.error("Erro ao processar mensagem: %s — %s", raw, e)
        return None


class Bridge:
    def __init__(self, db_path: str, notify_cb: Callable[[dict], None]):
        self.db_path = db_path
        self.notify_cb = notify_cb  # callback assíncrono chamado com o estado atualizado
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None

    def start(self, loop: asyncio.AbstractEventLoop) -> None:
        self._loop = loop
        serial_port = os.getenv("SERIAL_PORT", "/dev/ttyUSB0")
        self._thread = threading.Thread(
            target=self._run, args=(serial_port,), daemon=True, name="bridge"
        )
        self._thread.start()
        logger.info("Bridge iniciado (porta=%s)", serial_port)

    def _run(self, serial_port: str) -> None:
        try:
            import serial
            ser = serial.Serial(serial_port, 115200, timeout=2)
            logger.info("Serial aberto: %s", serial_port)
            self._read_loop(ser)
        except Exception as e:
            logger.warning("Serial indisponível (%s) — modo simulação", e)
            self._sim_loop()

    def _read_loop(self, ser) -> None:
        while True:
            try:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                raw = json.loads(line)
                self._handle(raw)
            except json.JSONDecodeError:
                pass
            except Exception as e:
                logger.error("Erro no read_loop: %s", e)
                time.sleep(1)

    def _sim_loop(self) -> None:
        """Gera leituras sintéticas para todos os reservatórios a cada 30s."""
        logger.info("Modo simulação ativo")
        # Estado simulado: começa em 60% e deriva
        state = {alias: 60.0 for (_, _), params in RESERVOIR_INDEX.items()
                 for alias in [params["alias"]]}
        # deduplica aliases
        aliases = list({params["alias"]: params for params in RESERVOIR_INDEX.values()}.items())

        while True:
            ts = int(time.time())
            for alias, params in aliases:
                node_id = next(k[0] for k, v in RESERVOIR_INDEX.items() if v["alias"] == alias)
                sensor_id = next(k[1] for k, v in RESERVOIR_INDEX.items() if v["alias"] == alias)
                # deriva aleatória: -2% a +1%
                state[alias] = max(5.0, min(100.0, state[alias] + random.uniform(-2, 1)))
                pct = state[alias]
                level = pct / 100 * params["level_max_cm"]
                volume = pct / 100 * params["volume_max_l"]
                distance = params["level_max_cm"] - level + params["sensor_offset_cm"]
                raw = {
                    "node": node_id, "sensor": sensor_id,
                    "dist": round(distance, 1),
                    "rssi": random.randint(-80, -50),
                    "vbat": random.choice([32, 33, 34]),
                    "seq": random.randint(0, 65535),
                    "ts": ts,
                }
                self._handle(raw)
            time.sleep(30)

    def _handle(self, raw: dict) -> None:
        record = _process_message(raw)
        if record is None:
            return
        # agenda corrotinas no loop do asyncio
        asyncio.run_coroutine_threadsafe(self._save_and_notify(record), self._loop)

    async def _save_and_notify(self, record: dict) -> None:
        import aiosqlite
        async with aiosqlite.connect(self.db_path) as conn:
            await insert_reading(conn, record)
            await upsert_state(conn, record)
        self.notify_cb(record)
```

- [ ] **Step 4.2: Verificar que o módulo importa sem erros**

```bash
cd aguada-web
python -c "from backend.bridge import Bridge, RESERVOIR_INDEX; print('OK, reservatórios:', len(RESERVOIR_INDEX))"
```

Esperado: `OK, reservatórios: 7`

- [ ] **Step 4.3: Commit**

```bash
git add aguada-web/backend/bridge.py
git commit -m "feat(aguada-web): bridge.py — serial reader + sim mode + process message"
```

---

## Task 5: FastAPI app (`main.py`)

**Files:**
- Create: `aguada-web/backend/main.py`
- Create: `aguada-web/tests/test_api.py`

- [ ] **Step 5.1: Criar `backend/main.py`**

```python
# backend/main.py
import asyncio
import json
import logging
import os
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Optional

import aiosqlite
from apscheduler.schedulers.asyncio import AsyncIOScheduler
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Query
from fastapi.responses import FileResponse, JSONResponse

from .bridge import Bridge
from .db import init_db, get_all_states, get_history, get_readings_for_date
from .calc import calc_consumption_events, decimate_readings
from .report import generate_daily_report_pdf

logger = logging.getLogger("main")
logging.basicConfig(level=logging.INFO)

DATA_DIR = Path(os.getenv("DATA_DIR", "./data"))
DB_PATH = str(DATA_DIR / "aguada.db")
REPORTS_DIR = DATA_DIR / "reports"

# ── WebSocket manager ──────────────────────────────────────────────────────────
class WSManager:
    def __init__(self):
        self._clients: list[WebSocket] = []
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def set_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        """Deve ser chamado no lifespan, com o loop do asyncio principal."""
        self._loop = loop

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self._clients.append(ws)

    def disconnect(self, ws: WebSocket):
        self._clients.remove(ws)

    def broadcast(self, data: dict):
        """Chamado pela bridge thread — agenda envio no loop asyncio principal."""
        if self._loop is None:
            return
        msg = json.dumps({"type": "reading", "data": data})
        self._loop.call_soon_threadsafe(
            lambda: asyncio.ensure_future(self._send_all(msg), loop=self._loop)
        )

    async def _send_all(self, msg: str):
        dead = []
        for ws in list(self._clients):
            try:
                await ws.send_text(msg)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self._clients.remove(ws)

    async def send_snapshot(self, ws: WebSocket):
        async with aiosqlite.connect(DB_PATH) as conn:
            states = await get_all_states(conn)
        await ws.send_text(json.dumps({"type": "snapshot", "data": states}))


ws_manager = WSManager()
bridge: Optional[Bridge] = None
scheduler: Optional[AsyncIOScheduler] = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global bridge, scheduler
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

    async with aiosqlite.connect(DB_PATH) as conn:
        await init_db(conn)

    loop = asyncio.get_event_loop()
    ws_manager.set_loop(loop)  # injeta loop antes de iniciar bridge

    bridge = Bridge(db_path=DB_PATH, notify_cb=ws_manager.broadcast)
    bridge.start(loop)

    scheduler = AsyncIOScheduler(timezone=os.getenv("TZ", "America/Sao_Paulo"))
    scheduler.add_job(_daily_report_job, "cron", hour=6, minute=0)
    scheduler.start()

    yield

    scheduler.shutdown()


async def _daily_report_job():
    import datetime
    date_str = (datetime.date.today() - datetime.timedelta(days=1)).isoformat()
    out_path = REPORTS_DIR / f"{date_str}.pdf"
    async with aiosqlite.connect(DB_PATH) as conn:
        await generate_daily_report_pdf(conn, date_str, str(out_path))
    logger.info("Relatório diário gerado: %s", out_path)


app = FastAPI(title="Aguada Web", lifespan=lifespan)


@app.get("/api/reservoirs")
async def get_reservoirs():
    async with aiosqlite.connect(DB_PATH) as conn:
        return await get_all_states(conn)


@app.get("/api/history/{alias}")
async def get_history_route(alias: str, period: str = "24h"):
    periods = {"24h": 86400, "7d": 7 * 86400, "30d": 30 * 86400}
    seconds = periods.get(period)
    if seconds is None:
        raise HTTPException(400, "period deve ser 24h, 7d ou 30d")
    since = int(time.time()) - seconds
    async with aiosqlite.connect(DB_PATH) as conn:
        rows = await get_history(conn, alias=alias.upper(), since_ts=since)
    return decimate_readings(rows, max_points=500)


@app.get("/api/consumption")
async def get_consumption(
    alias: str = Query(...),
    date: str = Query(...),
):
    async with aiosqlite.connect(DB_PATH) as conn:
        readings = await get_readings_for_date(conn, alias=alias.upper(), date_str=date)
    if not readings:
        return {"date": date, "alias": alias.upper(), "summary": {}, "events": []}

    events = calc_consumption_events(readings, date=date)
    consumed = sum(abs(e["delta_l"]) for e in events if e["type"] == "consumption")
    supplied = sum(e["delta_l"] for e in events if e["type"] == "supply")
    return {
        "date": date,
        "alias": alias.upper(),
        "summary": {
            "consumed_l": round(consumed, 1),
            "supplied_l": round(supplied, 1),
            "balance_l": round(supplied - consumed, 1),
        },
        "events": events,
    }


@app.get("/api/report/daily")
async def get_report_data(date: str = Query(...)):
    async with aiosqlite.connect(DB_PATH) as conn:
        states = await get_all_states(conn)
        report_data = []
        for s in states:
            alias = s["alias"]
            readings = await get_readings_for_date(conn, alias=alias, date_str=date)
            events = calc_consumption_events(readings, date=date) if readings else []
            report_data.append({**s, "events": events, "readings_count": len(readings)})
    return {"date": date, "reservoirs": report_data}


@app.get("/api/report/daily.pdf")
async def get_report_pdf(date: str = Query(...)):
    out_path = REPORTS_DIR / f"{date}.pdf"
    if not out_path.exists():
        async with aiosqlite.connect(DB_PATH) as conn:
            await generate_daily_report_pdf(conn, date, str(out_path))
    if not out_path.exists():
        raise HTTPException(404, "Relatório não disponível para esta data")
    return FileResponse(str(out_path), media_type="application/pdf",
                        filename=f"aguada-{date}.pdf")


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws_manager.connect(ws)
    await ws_manager.send_snapshot(ws)
    try:
        while True:
            await ws.receive_text()  # mantém conexão viva
    except WebSocketDisconnect:
        ws_manager.disconnect(ws)
```

- [ ] **Step 5.2: Escrever `tests/test_api.py`**

```python
# tests/test_api.py
import time
import pytest
from httpx import AsyncClient, ASGITransport
from backend.main import app
from backend.db import init_db, insert_reading, upsert_state
import aiosqlite
import os

# Usa banco em memória para testes
TEST_DB = ":memory:"

@pytest.fixture(autouse=True)
def set_test_db(tmp_path, monkeypatch):
    db_path = str(tmp_path / "test.db")
    monkeypatch.setattr("backend.main.DB_PATH", db_path)
    monkeypatch.setattr("backend.main.DATA_DIR", tmp_path)
    monkeypatch.setattr("backend.main.REPORTS_DIR", tmp_path / "reports")
    (tmp_path / "reports").mkdir()

@pytest.mark.asyncio
async def test_get_reservoirs_empty(set_test_db):
    import backend.main as m
    async with aiosqlite.connect(m.DB_PATH) as conn:
        await init_db(conn)
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        r = await client.get("/api/reservoirs")
    assert r.status_code == 200
    assert r.json() == []

@pytest.mark.asyncio
async def test_get_history_bad_period(set_test_db):
    import backend.main as m
    async with aiosqlite.connect(m.DB_PATH) as conn:
        await init_db(conn)
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        r = await client.get("/api/history/CON?period=99d")
    assert r.status_code == 400

@pytest.mark.asyncio
async def test_consumption_missing_params(set_test_db):
    import backend.main as m
    async with aiosqlite.connect(m.DB_PATH) as conn:
        await init_db(conn)
    async with AsyncClient(transport=ASGITransport(app=app), base_url="http://test") as client:
        r = await client.get("/api/consumption?alias=CON")
    assert r.status_code == 422  # FastAPI valida Query(...) obrigatório
```

- [ ] **Step 5.3: Rodar testes da API**

```bash
python -m pytest tests/test_api.py -v
```

Esperado: todos `PASSED`. (Os testes de lifespan/bridge são ignorados — bridge testa a si mesmo via simulação)

- [ ] **Step 5.4: Verificar que o servidor sobe localmente**

```bash
cd aguada-web
DATA_DIR=./data SERIAL_PORT=/dev/null uvicorn backend.main:app --reload --port 8000
```

Esperado: servidor inicia em modo simulação (serial `/dev/null` falha graciosamente), bridge logs simulação.

- [ ] **Step 5.5: Commit**

```bash
git add aguada-web/backend/main.py aguada-web/tests/test_api.py
git commit -m "feat(aguada-web): main.py — FastAPI rotas REST + WebSocket + bridge startup"
```

---

## Task 6: Relatório PDF (`report.py`)

**Files:**
- Create: `aguada-web/backend/report.py`

- [ ] **Step 6.1: Criar `backend/report.py`**

```python
# backend/report.py
"""Geração de relatório HTML + PDF com WeasyPrint."""
from __future__ import annotations
import aiosqlite
from .db import get_all_states, get_readings_for_date
from .calc import calc_consumption_events


def _build_html(date: str, reservoirs: list[dict]) -> str:
    rows_html = ""
    alerts_html = ""

    for r in reservoirs:
        alias = r.get("alias", "")
        name = r.get("name", "")
        pct = r.get("pct") or 0
        level = r.get("level_cm") or 0
        volume = r.get("volume_l") or 0
        volume_max = r.get("volume_max_l") or 1
        events = r.get("events", [])
        consumed = sum(abs(e["delta_l"]) for e in events if e["type"] == "consumption")
        supplied = sum(e["delta_l"] for e in events if e["type"] == "supply")
        color = "#16a34a" if pct >= 80 else "#2563eb" if pct >= 50 else "#d97706" if pct >= 20 else "#dc2626"

        rows_html += f"""
        <tr>
          <td><strong>{alias}</strong></td>
          <td>{name}</td>
          <td style="color:{color};font-weight:bold">{pct:.1f}%</td>
          <td>{level:.0f} cm</td>
          <td>{volume:,.0f} L / {volume_max:,.0f} L</td>
          <td style="color:#dc2626">-{consumed:,.0f} L</td>
          <td style="color:#16a34a">+{supplied:,.0f} L</td>
        </tr>"""

        if pct < 20:
            alerts_html += f'<li style="color:#dc2626">⚠️ {alias} ({name}) abaixo de 20%: {pct:.1f}%</li>'

    return f"""<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <style>
    body {{ font-family: Arial, sans-serif; margin: 30px; color: #1e293b; }}
    h1 {{ color: #1e40af; border-bottom: 2px solid #1e40af; padding-bottom: 8px; }}
    h2 {{ color: #374151; margin-top: 24px; }}
    table {{ width: 100%; border-collapse: collapse; margin-top: 12px; }}
    th {{ background: #1e40af; color: white; padding: 8px 12px; text-align: left; font-size: 12px; }}
    td {{ padding: 8px 12px; border-bottom: 1px solid #e2e8f0; font-size: 13px; }}
    tr:nth-child(even) td {{ background: #f8fafc; }}
    .alerts {{ background: #fef2f2; border: 1px solid #fca5a5; border-radius: 6px; padding: 12px; margin-top: 16px; }}
    .footer {{ margin-top: 40px; font-size: 11px; color: #94a3b8; text-align: center; }}
  </style>
</head>
<body>
  <h1>💧 CMASM — Aguada — Relatório Diário</h1>
  <p><strong>Data:</strong> {date} &nbsp;|&nbsp; <strong>Sistema:</strong> Aguada Web v1.0</p>

  <h2>Resumo dos Reservatórios</h2>
  <table>
    <thead>
      <tr>
        <th>Alias</th><th>Reservatório</th><th>Nível %</th>
        <th>Nível cm</th><th>Volume</th><th>Consumo 24h</th><th>Abastec. 24h</th>
      </tr>
    </thead>
    <tbody>{rows_html}</tbody>
  </table>

  {"<div class='alerts'><h3>⚠️ Alertas</h3><ul>" + alerts_html + "</ul></div>" if alerts_html else ""}

  <div class="footer">Gerado automaticamente por Aguada Web — CMASM Ilha do Engenho</div>
</body>
</html>"""


async def generate_daily_report_pdf(
    conn: aiosqlite.Connection, date: str, out_path: str
) -> None:
    from weasyprint import HTML
    states = await get_all_states(conn)
    enriched = []
    for s in states:
        readings = await get_readings_for_date(conn, alias=s["alias"], date_str=date)
        events = calc_consumption_events(readings, date=date) if readings else []
        enriched.append({**s, "events": events})

    html = _build_html(date, enriched)
    HTML(string=html).write_pdf(out_path)
```

- [ ] **Step 6.2: Verificar importação**

```bash
python -c "from backend.report import generate_daily_report_pdf; print('OK')"
```

Esperado: `OK`

- [ ] **Step 6.3: Commit**

```bash
git add aguada-web/backend/report.py
git commit -m "feat(aguada-web): report.py — HTML template + WeasyPrint PDF"
```

---

## Task 7: Frontend — componente base e Dashboard (`index.html`)

**Files:**
- Create: `aguada-web/frontend/index.html`

O frontend usa Tailwind CDN + Alpine.js + Chart.js. Não há build step.

- [ ] **Step 7.1: Criar `frontend/index.html`**

```html
<!DOCTYPE html>
<html lang="pt-BR" x-data="dashboardApp()" :class="{ dark: darkMode }">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Aguada — Dashboard</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script defer src="https://unpkg.com/alpinejs@3/dist/cdn.min.js"></script>
  <script>
    tailwind.config = { darkMode: 'class' }
  </script>
  <style>
    [x-cloak] { display: none; }
    .card-hover { transition: transform 0.15s ease, box-shadow 0.15s ease; }
    .card-hover:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(0,0,0,0.12); }
  </style>
</head>
<body class="bg-gray-100 dark:bg-gray-900 min-h-screen" x-cloak>

  <!-- Header / Nav -->
  <header class="bg-white dark:bg-gray-800 shadow-sm sticky top-0 z-10">
    <div class="max-w-7xl mx-auto px-4 py-3 flex items-center justify-between">
      <div class="flex items-center gap-6">
        <span class="font-bold text-xl text-blue-700 dark:text-blue-400">💧 Aguada</span>
        <nav class="flex gap-1 text-sm font-medium">
          <a href="index.html" class="px-3 py-1.5 rounded-md bg-blue-100 text-blue-700 dark:bg-blue-900 dark:text-blue-300">Dashboard</a>
          <a href="scada.html" class="px-3 py-1.5 rounded-md text-gray-600 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-700">SCADA</a>
          <a href="history.html" class="px-3 py-1.5 rounded-md text-gray-600 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-700">Histórico</a>
          <a href="consumption.html" class="px-3 py-1.5 rounded-md text-gray-600 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-700">Consumo</a>
          <a href="report.html" class="px-3 py-1.5 rounded-md text-gray-600 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-700">Relatório</a>
        </nav>
      </div>
      <div class="flex items-center gap-3 text-sm text-gray-500 dark:text-gray-400">
        <span x-text="lastUpdate ? 'Atualizado: ' + lastUpdate : 'Aguardando dados...'"></span>
        <span :class="wsConnected ? 'text-green-500' : 'text-red-500'" x-text="wsConnected ? '● Online' : '● Offline'"></span>
        <button @click="darkMode = !darkMode; localStorage.setItem('darkMode', darkMode)"
                class="p-1.5 rounded-md hover:bg-gray-100 dark:hover:bg-gray-700">
          <span x-text="darkMode ? '☀️' : '🌙'"></span>
        </button>
      </div>
    </div>
  </header>

  <main class="max-w-7xl mx-auto px-4 py-6">

    <!-- KPI Cards -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6">
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-5 card-hover">
        <p class="text-gray-500 dark:text-gray-400 text-sm">Reservatórios</p>
        <p class="text-3xl font-bold text-gray-900 dark:text-white mt-1" x-text="reservoirs.length"></p>
        <p class="text-5xl mt-2">💧</p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-5 card-hover">
        <p class="text-gray-500 dark:text-gray-400 text-sm">Nodes Online</p>
        <p class="text-3xl font-bold text-gray-900 dark:text-white mt-1">
          <span x-text="reservoirs.filter(r=>r.online).length"></span>/<span x-text="reservoirs.length"></span>
        </p>
        <p class="text-5xl mt-2">📡</p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-5 card-hover">
        <p class="text-gray-500 dark:text-gray-400 text-sm">Volume Total</p>
        <p class="text-3xl font-bold text-gray-900 dark:text-white mt-1">
          <span x-text="totalVolume"></span> m³
        </p>
        <p class="text-5xl mt-2">🏗️</p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-5 card-hover">
        <p class="text-gray-500 dark:text-gray-400 text-sm">Nível Médio</p>
        <p class="text-3xl font-bold mt-1" :class="avgPct >= 80 ? 'text-green-600' : avgPct >= 50 ? 'text-blue-600' : avgPct >= 20 ? 'text-yellow-600' : 'text-red-600'"
           x-text="avgPct + '%'"></p>
        <p class="text-5xl mt-2">📊</p>
      </div>
    </div>

    <!-- Reservoir Cards Grid -->
    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
      <template x-for="r in reservoirs" :key="r.alias">
        <div class="bg-white dark:bg-gray-800 rounded-xl shadow card-hover overflow-hidden">
          <!-- Status header -->
          <div class="px-4 py-3 flex items-center justify-between"
               :style="'background:' + statusColor(r.pct, r.online)">
            <div>
              <p class="font-bold text-white text-lg" x-text="r.alias"></p>
              <p class="text-white/80 text-xs" x-text="r.name"></p>
            </div>
            <div class="text-right">
              <p class="text-white font-bold text-2xl" x-text="(r.pct ?? '--') + '%'"></p>
              <span class="text-xs text-white/80" x-text="r.online ? '● Online' : '○ Offline'"></span>
            </div>
          </div>
          <!-- Progress bar -->
          <div class="mx-4 mt-3">
            <div class="w-full bg-gray-200 dark:bg-gray-700 rounded-full h-3">
              <div class="h-3 rounded-full transition-all duration-500"
                   :style="'width:' + (r.pct ?? 0) + '%;background:' + statusColor(r.pct, r.online)"></div>
            </div>
          </div>
          <!-- Details -->
          <div class="px-4 py-3 text-sm text-gray-600 dark:text-gray-400 space-y-1">
            <div class="flex justify-between">
              <span>Nível</span><span class="font-medium text-gray-900 dark:text-white" x-text="(r.level_cm ?? '--') + ' cm'"></span>
            </div>
            <div class="flex justify-between">
              <span>Volume</span><span class="font-medium text-gray-900 dark:text-white" x-text="r.volume_l ? formatVol(r.volume_l) + ' L' : '--'"></span>
            </div>
            <div class="flex justify-between">
              <span>Capacidade</span><span class="font-medium text-gray-900 dark:text-white" x-text="r.volume_max_l ? formatVol(r.volume_max_l) + ' L' : '--'"></span>
            </div>
            <div class="flex justify-between">
              <span>RSSI</span><span class="font-medium" :class="rssiColor(r.rssi)" x-text="r.rssi ? r.rssi + ' dBm' : '--'"></span>
            </div>
            <div class="flex justify-between">
              <span>Última leitura</span><span class="text-xs text-gray-400" x-text="r.ts ? timeAgo(r.ts) : '--'"></span>
            </div>
          </div>
        </div>
      </template>
    </div>

    <!-- Empty state -->
    <div x-show="reservoirs.length === 0" class="text-center py-20 text-gray-400">
      <p class="text-6xl mb-4">📡</p>
      <p class="text-xl">Aguardando dados do gateway...</p>
      <p class="text-sm mt-2">Verifique se o gateway está conectado via USB serial.</p>
    </div>

  </main>

  <script>
  function dashboardApp() {
    return {
      darkMode: localStorage.getItem('darkMode') === 'true',
      reservoirs: [],
      lastUpdate: null,
      wsConnected: false,
      _ws: null,
      _retryDelay: 1000,

      get totalVolume() {
        const total = this.reservoirs.reduce((s, r) => s + (r.volume_l || 0), 0);
        return (total / 1000).toFixed(1);
      },
      get avgPct() {
        if (!this.reservoirs.length) return 0;
        const sum = this.reservoirs.reduce((s, r) => s + (r.pct || 0), 0);
        return Math.round(sum / this.reservoirs.length);
      },

      init() { this.connect(); },

      connect() {
        const protocol = location.protocol === 'https:' ? 'wss' : 'ws';
        this._ws = new WebSocket(`${protocol}://${location.host}/ws`);

        this._ws.onopen = () => { this.wsConnected = true; this._retryDelay = 1000; };
        this._ws.onclose = () => {
          this.wsConnected = false;
          setTimeout(() => this.connect(), this._retryDelay);
          this._retryDelay = Math.min(this._retryDelay * 2, 30000);
        };
        this._ws.onmessage = (e) => {
          const msg = JSON.parse(e.data);
          if (msg.type === 'snapshot') {
            this.reservoirs = msg.data;
          } else if (msg.type === 'reading') {
            const idx = this.reservoirs.findIndex(r => r.alias === msg.data.alias);
            if (idx >= 0) this.reservoirs[idx] = msg.data;
            else this.reservoirs.push(msg.data);
          }
          this.lastUpdate = new Date().toLocaleTimeString('pt-BR');
        };
      },

      statusColor(pct, online) {
        if (!online || pct == null) return '#94a3b8';
        if (pct >= 80) return '#16a34a';
        if (pct >= 50) return '#2563eb';
        if (pct >= 20) return '#d97706';
        return '#dc2626';
      },
      rssiColor(rssi) {
        if (!rssi) return '';
        if (rssi >= -60) return 'text-green-600';
        if (rssi >= -75) return 'text-yellow-600';
        return 'text-red-500';
      },
      formatVol(v) { return v >= 1000 ? (v/1000).toFixed(1) + 'k' : Math.round(v).toString(); },
      timeAgo(ts) {
        const d = Math.floor(Date.now()/1000 - ts);
        if (d < 60) return d + 's atrás';
        if (d < 3600) return Math.floor(d/60) + 'min atrás';
        return Math.floor(d/3600) + 'h atrás';
      },
    }
  }
  </script>
</body>
</html>
```

- [ ] **Step 7.2: Testar no browser**

Com o servidor rodando (`uvicorn backend.main:app --reload`), abrir `http://localhost:8000` — Nginx não está configurado ainda, usar diretamente via uvicorn servindo o arquivo estático temporariamente, ou abrir `frontend/index.html` diretamente no browser com `file://` (WebSocket não funciona com file://, usar live server ou nginx).

Melhor: subir com Docker no Task 9 e testar completo.

- [ ] **Step 7.3: Commit**

```bash
git add aguada-web/frontend/index.html
git commit -m "feat(aguada-web): index.html — dashboard WebSocket tempo real"
```

---

## Task 8: Frontend — SCADA, Histórico, Consumo, Relatório

**Files:**
- Create: `aguada-web/frontend/scada.html`
- Create: `aguada-web/frontend/history.html`
- Create: `aguada-web/frontend/consumption.html`
- Create: `aguada-web/frontend/report.html`

- [ ] **Step 8.1: Criar `frontend/scada.html`**

A página SCADA usa SVG estático com Alpine.js para atualizar os níveis de água.
Layout real: CIE1/CIE2 (topo esquerda), CBIF1/CBIF2 (topo direita), CB31/CB32 (centro), CON e CAV (baixo).

O SVG consiste em:
- Retângulos de tanque com `clipPath` para o nível de água (altura proporcional ao %)
- `fill` gradiente azul para água
- Labels de alias, %, volume dentro ou abaixo de cada tanque
- Linhas de tubulação entre tanques com animação de fluxo (`stroke-dashoffset`)
- Polling GET `/api/reservoirs` a cada 30s

Estrutura mínima (implementar com os tanques posicionados no SVG):

```html
<!DOCTYPE html>
<html lang="pt-BR" x-data="scadaApp()" :class="{ dark: darkMode }">
<head>
  <meta charset="UTF-8">
  <title>Aguada — SCADA</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script defer src="https://unpkg.com/alpinejs@3/dist/cdn.min.js"></script>
  <script>tailwind.config = { darkMode: 'class' }</script>
  <style>
    @keyframes flow { to { stroke-dashoffset: -20; } }
    .pipe-active { animation: flow 1.5s linear infinite; }
  </style>
</head>
<body class="bg-gray-100 dark:bg-gray-900 min-h-screen" x-cloak>
  <!-- Mesmo header de index.html com aba SCADA ativa -->
  <!-- [incluir header igual ao index.html trocando a aba ativa] -->

  <main class="max-w-7xl mx-auto px-4 py-6">
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 overflow-auto">
      <svg viewBox="0 0 1000 650" class="w-full max-w-4xl mx-auto">
        <defs>
          <!-- Gradiente de água -->
          <linearGradient id="water" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stop-color="#60a5fa"/>
            <stop offset="100%" stop-color="#2563eb"/>
          </linearGradient>
          <!-- ClipPaths dinâmicos para cada tanque (um por reservatório) -->
          <template x-for="r in reservoirs" :key="'clip-'+r.alias">
            <clipPath :id="'clip-'+r.alias">
              <rect :x="tankPos[r.alias]?.x" :y="tankPos[r.alias]?.y + tankPos[r.alias]?.h * (1 - (r.pct||0)/100)"
                    :width="tankPos[r.alias]?.w" :height="tankPos[r.alias]?.h * ((r.pct||0)/100)"/>
            </clipPath>
          </template>
        </defs>

        <!-- Tubulações (linhas) -->
        <g stroke="#94a3b8" stroke-width="4" fill="none" stroke-dasharray="10,5">
          <line x1="150" y1="170" x2="150" y2="290" class="pipe-active"/>
          <line x1="500" y1="170" x2="500" y2="290" class="pipe-active"/>
          <line x1="150" y1="350" x2="300" y2="430" class="pipe-active"/>
          <line x1="500" y1="350" x2="500" y2="430"/>
          <line x1="700" y1="350" x2="500" y2="430"/>
        </g>

        <!-- Tanques: fundo (cinza) + água (clip) -->
        <template x-for="r in reservoirs" :key="'tank-'+r.alias">
          <g :transform="'translate('+tankPos[r.alias]?.x+','+tankPos[r.alias]?.y+')'">
            <!-- Fundo do tanque -->
            <rect :width="tankPos[r.alias]?.w" :height="tankPos[r.alias]?.h"
                  fill="#e2e8f0" rx="4" stroke="#94a3b8" stroke-width="1"/>
            <!-- Água (clip dinâmico) -->
            <rect :width="tankPos[r.alias]?.w" :height="tankPos[r.alias]?.h"
                  fill="url(#water)" :clip-path="'url(#clip-'+r.alias+')'" rx="4"/>
            <!-- Label alias -->
            <text x="50%" y="-10" text-anchor="middle" class="fill-gray-700 dark:fill-gray-200"
                  font-size="13" font-weight="bold" :textContent="r.alias"></text>
            <!-- Percentual dentro do tanque -->
            <text x="50%" :y="tankPos[r.alias]?.h/2+5" text-anchor="middle"
                  fill="white" font-size="14" font-weight="bold"
                  :textContent="(r.pct??'--')+'%'"></text>
          </g>
        </template>
      </svg>
    </div>

    <!-- Painel de detalhes -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4 mt-6">
      <template x-for="r in reservoirs" :key="'detail-'+r.alias">
        <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 text-sm">
          <p class="font-bold text-gray-900 dark:text-white" x-text="r.alias + ' — ' + (r.name||'')"></p>
          <p class="text-gray-500 mt-1">Volume: <span class="font-medium text-gray-800 dark:text-gray-200" x-text="r.volume_l ? Math.round(r.volume_l).toLocaleString('pt-BR') + ' L' : '--'"></span></p>
          <p class="text-gray-500">RSSI: <span x-text="r.rssi ? r.rssi + ' dBm' : '--'"></span></p>
          <span :class="r.online ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'"
                class="mt-2 inline-block px-2 py-0.5 rounded-full text-xs font-medium"
                x-text="r.online ? 'Online' : 'Offline'"></span>
        </div>
      </template>
    </div>
  </main>

  <script>
  function scadaApp() {
    return {
      darkMode: localStorage.getItem('darkMode') === 'true',
      reservoirs: [],
      // Posições dos tanques no SVG (x, y, w, h)
      tankPos: {
        CIE1:  { x: 30,  y: 30,  w: 110, h: 120 },
        CIE2:  { x: 170, y: 30,  w: 110, h: 120 },
        CBIF1: { x: 720, y: 30,  w: 110, h: 120 },
        CBIF2: { x: 860, y: 30,  w: 110, h: 120 },
        CB31:  { x: 100, y: 290, w: 110, h: 100 },
        CB32:  { x: 230, y: 290, w: 110, h: 100 },
        CON:   { x: 380, y: 430, w: 110, h: 140 },
        CAV:   { x: 680, y: 430, w: 110, h: 140 },
      },
      init() { this.load(); setInterval(() => this.load(), 30000); },
      async load() {
        const r = await fetch('/api/reservoirs');
        this.reservoirs = await r.json();
      },
    }
  }
  </script>
</body>
</html>
```

- [ ] **Step 8.2: Criar `frontend/history.html`**

```html
<!DOCTYPE html>
<html lang="pt-BR" x-data="historyApp()" :class="{ dark: darkMode }">
<head>
  <meta charset="UTF-8">
  <title>Aguada — Histórico</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script defer src="https://unpkg.com/alpinejs@3/dist/cdn.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4/dist/chart.umd.min.js"></script>
  <script>tailwind.config = { darkMode: 'class' }</script>
</head>
<body class="bg-gray-100 dark:bg-gray-900 min-h-screen" x-cloak>
  <!-- [mesmo header — aba Histórico ativa] -->
  <main class="max-w-7xl mx-auto px-4 py-6">

    <!-- Filtros -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 mb-6 flex flex-wrap gap-4 items-end">
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Reservatório</label>
        <select x-model="alias" @change="load()"
                class="rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 px-3 py-2 text-sm">
          <template x-for="opt in aliases" :key="opt"><option :value="opt" x-text="opt"></option></template>
        </select>
      </div>
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Período</label>
        <div class="flex gap-2">
          <template x-for="p in ['24h','7d','30d']" :key="p">
            <button @click="period=p; load()"
                    :class="period===p ? 'bg-blue-600 text-white' : 'bg-gray-100 dark:bg-gray-700 text-gray-700 dark:text-gray-300'"
                    class="px-3 py-2 rounded-lg text-sm font-medium" x-text="p"></button>
          </template>
        </div>
      </div>
    </div>

    <!-- Gráfico -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 mb-6">
      <h2 class="font-semibold text-gray-900 dark:text-white mb-3">Volume (L) — <span x-text="alias"></span></h2>
      <div class="relative h-64">
        <canvas id="chart"></canvas>
      </div>
    </div>

    <!-- Tabela -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow overflow-hidden">
      <div class="px-4 py-3 border-b border-gray-200 dark:border-gray-700">
        <h2 class="font-semibold text-gray-900 dark:text-white">Leituras</h2>
      </div>
      <div class="overflow-x-auto">
        <table class="w-full text-sm">
          <thead class="bg-gray-50 dark:bg-gray-700">
            <tr>
              <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Timestamp</th>
              <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Nível cm</th>
              <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Volume L</th>
              <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">%</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-gray-200 dark:divide-gray-700">
            <template x-for="r in readings.slice(0, 100)" :key="r.ts">
              <tr class="hover:bg-gray-50 dark:hover:bg-gray-750">
                <td class="px-4 py-2 text-gray-600 dark:text-gray-400" x-text="new Date(r.ts*1000).toLocaleString('pt-BR')"></td>
                <td class="px-4 py-2" x-text="r.level_cm?.toFixed(1)"></td>
                <td class="px-4 py-2" x-text="r.volume_l?.toLocaleString('pt-BR')"></td>
                <td class="px-4 py-2 font-medium"
                    :class="r.pct>=80?'text-green-600':r.pct>=50?'text-blue-600':r.pct>=20?'text-yellow-600':'text-red-600'"
                    x-text="r.pct?.toFixed(1)+'%'"></td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>
    </div>
  </main>

  <script>
  function historyApp() {
    return {
      darkMode: localStorage.getItem('darkMode') === 'true',
      alias: 'CON', period: '24h',
      aliases: ['CON','CAV','CB31','CB32','CIE1','CIE2','CBIF1','CBIF2'],
      readings: [],
      _chart: null,
      init() { this.load(); },
      async load() {
        const r = await fetch(`/api/history/${this.alias}?period=${this.period}`);
        this.readings = await r.json();
        this.$nextTick(() => this.renderChart());
      },
      renderChart() {
        const ctx = document.getElementById('chart');
        if (this._chart) this._chart.destroy();
        this._chart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: this.readings.map(r => new Date(r.ts*1000).toLocaleString('pt-BR', {month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit'})),
            datasets: [{
              label: 'Volume (L)',
              data: this.readings.map(r => r.volume_l),
              borderColor: '#2563eb', backgroundColor: 'rgba(37,99,235,0.1)',
              tension: 0.4, pointRadius: 0, fill: true,
            }]
          },
          options: {
            responsive: true, maintainAspectRatio: false,
            plugins: { legend: { display: false } },
            scales: { y: { beginAtZero: false } },
          }
        });
      }
    }
  }
  </script>
</body>
</html>
```

- [ ] **Step 8.3: Criar `frontend/consumption.html`**

```html
<!DOCTYPE html>
<html lang="pt-BR" x-data="consumptionApp()" :class="{ dark: darkMode }">
<head>
  <meta charset="UTF-8">
  <title>Aguada — Consumo</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script defer src="https://unpkg.com/alpinejs@3/dist/cdn.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4/dist/chart.umd.min.js"></script>
  <script>tailwind.config = { darkMode: 'class' }</script>
</head>
<body class="bg-gray-100 dark:bg-gray-900 min-h-screen" x-cloak>
  <!-- [header — aba Consumo ativa] -->
  <main class="max-w-7xl mx-auto px-4 py-6">

    <!-- Filtros -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 mb-6 flex flex-wrap gap-4 items-end">
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Reservatório</label>
        <select x-model="alias" class="rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 px-3 py-2 text-sm">
          <template x-for="opt in aliases" :key="opt"><option :value="opt" x-text="opt"></option></template>
        </select>
      </div>
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Data</label>
        <input type="date" x-model="date" class="rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 px-3 py-2 text-sm">
      </div>
      <button @click="load()" class="px-4 py-2 bg-blue-600 text-white rounded-lg text-sm font-medium hover:bg-blue-700">Buscar</button>
    </div>

    <!-- KPI Cards -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4 mb-6" x-show="data">
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4">
        <p class="text-sm text-gray-500">Consumido</p>
        <p class="text-2xl font-bold text-red-600" x-text="data?.summary?.consumed_l?.toLocaleString('pt-BR') + ' L'"></p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4">
        <p class="text-sm text-gray-500">Abastecido</p>
        <p class="text-2xl font-bold text-green-600" x-text="'+' + data?.summary?.supplied_l?.toLocaleString('pt-BR') + ' L'"></p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4">
        <p class="text-sm text-gray-500">Saldo</p>
        <p class="text-2xl font-bold" :class="(data?.summary?.balance_l||0)>=0?'text-green-600':'text-red-600'"
           x-text="(data?.summary?.balance_l||0) >= 0 ? '+' : '' + data?.summary?.balance_l?.toLocaleString('pt-BR') + ' L'"></p>
      </div>
      <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4">
        <p class="text-sm text-gray-500">Eventos</p>
        <p class="text-2xl font-bold text-gray-900 dark:text-white" x-text="data?.events?.length || 0"></p>
      </div>
    </div>

    <!-- Bar chart -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-4 mb-6" x-show="data?.events?.length">
      <h2 class="font-semibold text-gray-900 dark:text-white mb-3">Delta por Hora</h2>
      <div class="relative h-56"><canvas id="barChart"></canvas></div>
    </div>

    <!-- Tabela de eventos -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow overflow-hidden" x-show="data?.events?.length">
      <table class="w-full text-sm">
        <thead class="bg-gray-50 dark:bg-gray-700">
          <tr>
            <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Hora</th>
            <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Vol Início</th>
            <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Vol Fim</th>
            <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Delta</th>
            <th class="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Tipo</th>
          </tr>
        </thead>
        <tbody class="divide-y divide-gray-200 dark:divide-gray-700">
          <template x-for="e in data?.events||[]" :key="e.hour">
            <tr>
              <td class="px-4 py-2" x-text="e.hour"></td>
              <td class="px-4 py-2" x-text="e.vol_start?.toLocaleString('pt-BR')"></td>
              <td class="px-4 py-2" x-text="e.vol_end?.toLocaleString('pt-BR')"></td>
              <td class="px-4 py-2 font-medium"
                  :class="e.delta_l<0?'text-red-600':'text-green-600'"
                  x-text="(e.delta_l>0?'+':'')+e.delta_l?.toLocaleString('pt-BR')"></td>
              <td class="px-4 py-2">
                <span class="px-2 py-0.5 rounded-full text-xs font-medium"
                      :class="e.type==='consumption'?'bg-red-100 text-red-800':e.type==='supply'?'bg-green-100 text-green-800':'bg-gray-100 text-gray-700'"
                      x-text="e.type==='consumption'?'⬇️ Consumo':e.type==='supply'?'⬆️ Abastec.':'➡️ Estável'"></span>
              </td>
            </tr>
          </template>
        </tbody>
      </table>
    </div>
  </main>

  <script>
  function consumptionApp() {
    return {
      darkMode: localStorage.getItem('darkMode') === 'true',
      alias: 'CON',
      date: new Date().toISOString().slice(0,10),
      aliases: ['CON','CAV','CB31','CB32','CIE1','CIE2','CBIF1','CBIF2'],
      data: null,
      _chart: null,
      init() { this.load(); },
      async load() {
        const r = await fetch(`/api/consumption?alias=${this.alias}&date=${this.date}`);
        this.data = await r.json();
        this.$nextTick(() => this.renderChart());
      },
      renderChart() {
        const ctx = document.getElementById('barChart');
        if (!ctx || !this.data?.events?.length) return;
        if (this._chart) this._chart.destroy();
        this._chart = new Chart(ctx, {
          type: 'bar',
          data: {
            labels: this.data.events.map(e => e.hour),
            datasets: [{
              label: 'Delta (L)',
              data: this.data.events.map(e => e.delta_l),
              backgroundColor: this.data.events.map(e =>
                e.delta_l < 0 ? 'rgba(220,38,38,0.7)' : 'rgba(22,163,74,0.7)'
              ),
            }]
          },
          options: { responsive: true, maintainAspectRatio: false,
            plugins: { legend: { display: false } } }
        });
      }
    }
  }
  </script>
</body>
</html>
```

- [ ] **Step 8.4: Criar `frontend/report.html`**

```html
<!DOCTYPE html>
<html lang="pt-BR" x-data="reportApp()" :class="{ dark: darkMode }">
<head>
  <meta charset="UTF-8">
  <title>Aguada — Relatório</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script defer src="https://unpkg.com/alpinejs@3/dist/cdn.min.js"></script>
  <script>tailwind.config = { darkMode: 'class' }</script>
</head>
<body class="bg-gray-100 dark:bg-gray-900 min-h-screen" x-cloak>
  <!-- [header — aba Relatório ativa] -->
  <main class="max-w-5xl mx-auto px-4 py-6">
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-6 mb-6 flex flex-wrap gap-4 items-end">
      <div>
        <label class="block text-sm font-medium text-gray-700 dark:text-gray-300 mb-1">Data</label>
        <input type="date" x-model="date" class="rounded-lg border border-gray-300 dark:border-gray-600 bg-white dark:bg-gray-700 px-3 py-2 text-sm">
      </div>
      <button @click="load()" class="px-4 py-2 bg-blue-600 text-white rounded-lg text-sm font-medium hover:bg-blue-700">Ver Relatório</button>
      <a :href="'/api/report/daily.pdf?date='+date" target="_blank"
         class="px-4 py-2 bg-green-600 text-white rounded-lg text-sm font-medium hover:bg-green-700 inline-block">
        ⬇️ Baixar PDF
      </a>
    </div>

    <!-- Preview do relatório -->
    <div class="bg-white dark:bg-gray-800 rounded-xl shadow p-6" x-show="data">
      <h1 class="text-2xl font-bold text-blue-700 dark:text-blue-400 border-b border-blue-200 pb-3 mb-4">
        💧 CMASM — Aguada — Relatório Diário
      </h1>
      <p class="text-sm text-gray-500 mb-6"><strong>Data:</strong> <span x-text="data?.date"></span></p>

      <h2 class="text-lg font-semibold text-gray-900 dark:text-white mb-3">Resumo dos Reservatórios</h2>
      <div class="overflow-x-auto">
        <table class="w-full text-sm border-collapse">
          <thead>
            <tr class="bg-blue-700 text-white">
              <th class="px-4 py-2 text-left">Alias</th>
              <th class="px-4 py-2 text-left">Reservatório</th>
              <th class="px-4 py-2 text-left">Nível %</th>
              <th class="px-4 py-2 text-left">Volume</th>
              <th class="px-4 py-2 text-left">Leituras</th>
            </tr>
          </thead>
          <tbody>
            <template x-for="r in data?.reservoirs||[]" :key="r.alias">
              <tr class="border-b border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-700">
                <td class="px-4 py-2 font-bold" x-text="r.alias"></td>
                <td class="px-4 py-2 text-gray-600 dark:text-gray-400" x-text="r.name"></td>
                <td class="px-4 py-2 font-bold"
                    :class="r.pct>=80?'text-green-600':r.pct>=50?'text-blue-600':r.pct>=20?'text-yellow-600':'text-red-600'"
                    x-text="(r.pct??'--')+'%'"></td>
                <td class="px-4 py-2" x-text="r.volume_l ? r.volume_l.toLocaleString('pt-BR') + ' L' : '--'"></td>
                <td class="px-4 py-2 text-gray-500" x-text="r.readings_count + ' leituras'"></td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>

      <!-- Alertas -->
      <div x-show="alertas.length > 0" class="mt-6 bg-red-50 dark:bg-red-900/20 border border-red-200 dark:border-red-800 rounded-lg p-4">
        <h3 class="font-semibold text-red-800 dark:text-red-400 mb-2">⚠️ Alertas</h3>
        <ul class="list-disc list-inside text-sm text-red-700 dark:text-red-300">
          <template x-for="a in alertas" :key="a"><li x-text="a"></li></template>
        </ul>
      </div>
    </div>
  </main>

  <script>
  function reportApp() {
    return {
      darkMode: localStorage.getItem('darkMode') === 'true',
      date: new Date().toISOString().slice(0,10),
      data: null,
      get alertas() {
        return (this.data?.reservoirs||[])
          .filter(r => r.pct < 20)
          .map(r => `${r.alias} (${r.name}) abaixo de 20%: ${r.pct?.toFixed(1)}%`);
      },
      async load() {
        const r = await fetch(`/api/report/daily?date=${this.date}`);
        this.data = await r.json();
      }
    }
  }
  </script>
</body>
</html>
```

- [ ] **Step 8.5: Commit**

```bash
git add aguada-web/frontend/
git commit -m "feat(aguada-web): frontend — SCADA, histórico, consumo, relatório"
```

---

## Task 9: Docker e configuração final

**Files:**
- Create: `aguada-web/Dockerfile`
- Create: `aguada-web/docker-compose.yml`
- Create: `aguada-web/nginx.conf`
- Create: `aguada-web/.env.example` (já criado no Task 1)

- [ ] **Step 9.1: Criar `Dockerfile`**

```dockerfile
FROM python:3.12-slim

# WeasyPrint precisa de libs de sistema e fontes
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpango-1.0-0 libpangoft2-1.0-0 libpangocairo-1.0-0 \
    libcairo2 libgdk-pixbuf2.0-0 libffi-dev \
    libfontconfig1 fonts-liberation \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY backend/ ./backend/

CMD ["uvicorn", "backend.main:app", "--host", "0.0.0.0", "--port", "8000"]
```

- [ ] **Step 9.2: Criar `nginx.conf`**

```nginx
server {
    listen 80;

    # Frontend estático
    location / {
        root /usr/share/nginx/html;
        index index.html;
        try_files $uri $uri/ /index.html;
    }

    # Proxy para FastAPI
    location /api/ {
        proxy_pass http://app:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # WebSocket
    location /ws {
        proxy_pass http://app:8000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400;
    }
}
```

- [ ] **Step 9.3: Criar `docker-compose.yml`**

```yaml
services:
  app:
    build: .
    restart: unless-stopped
    devices:
      - ${SERIAL_PORT:-/dev/ttyUSB0}:${SERIAL_PORT:-/dev/ttyUSB0}
    volumes:
      - ./data:/data
    env_file: .env
    environment:
      - DATA_DIR=/data

  nginx:
    image: nginx:alpine
    restart: unless-stopped
    ports:
      - "80:80"
    volumes:
      - ./frontend:/usr/share/nginx/html:ro
      - ./nginx.conf:/etc/nginx/conf.d/default.conf:ro
    depends_on:
      - app
```

- [ ] **Step 9.4: Criar `.env` a partir do `.env.example`**

```bash
cp .env.example .env
# editar SERIAL_PORT conforme necessário (ttyUSB0 ou ttyACM0)
```

- [ ] **Step 9.5: Build e subir**

```bash
docker compose build
docker compose up -d
docker compose logs -f app
```

Esperado nos logs:
- `Serial indisponível (...) — modo simulação` (se serial não conectado)
- `Uvicorn running on http://0.0.0.0:8000`
- Leituras simuladas a cada 30s

- [ ] **Step 9.6: Testar endpoints**

```bash
# Estado atual
curl http://localhost/api/reservoirs | python3 -m json.tool

# Histórico 24h do CON
curl "http://localhost/api/history/CON?period=24h" | python3 -m json.tool

# Consumo de hoje
curl "http://localhost/api/consumption?alias=CON&date=$(date +%F)" | python3 -m json.tool

# Abrir browser
xdg-open http://localhost  # ou acessar via IP do servidor
```

- [ ] **Step 9.7: Commit**

```bash
git add aguada-web/Dockerfile aguada-web/docker-compose.yml aguada-web/nginx.conf
git commit -m "feat(aguada-web): Docker Compose + Nginx + Dockerfile"
```

---

## Task 10: MQTT opcional (coexistência com Home Assistant)

**Files:**
- Modify: `aguada-web/backend/bridge.py` — adicionar publicação MQTT opcional

- [ ] **Step 10.1: Adicionar publicação MQTT ao `bridge.py`**

No método `_save_and_notify` de `Bridge`, após o upsert, adicionar:

```python
# Ao final de __init__:
self._mqtt_client = None
self._init_mqtt()

def _init_mqtt(self):
    host = os.getenv("MQTT_HOST", "")
    if not host:
        return
    import paho.mqtt.client as mqtt
    self._mqtt_client = mqtt.Client()
    user = os.getenv("MQTT_USER", "")
    pwd = os.getenv("MQTT_PASS", "")
    if user:
        self._mqtt_client.username_pw_set(user, pwd)
    port = int(os.getenv("MQTT_PORT", "1883"))
    self._mqtt_client.connect_async(host, port)
    self._mqtt_client.loop_start()
    logger.info("MQTT configurado: %s:%d", host, port)

def _publish_mqtt(self, record: dict):
    if not self._mqtt_client:
        return
    import json
    alias = record["alias"]
    node_id = record["node_id"]
    sensor_id = record["sensor_id"]
    payload = json.dumps({
        "alias": alias,
        "distance_cm": record.get("distance_cm"),
        "level_cm": record.get("level_cm"),
        "pct": record.get("pct"),
        "volume_l": record.get("volume_l"),
        "rssi": record.get("rssi"),
        "vbat": record.get("vbat"),
        "seq": record.get("seq"),
        "ts": record.get("ts"),
    })
    topic = f"aguada/{node_id}/{sensor_id}/state"
    try:
        self._mqtt_client.publish(topic, payload, retain=True)
    except Exception as e:
        logger.warning("MQTT publish error: %s", e)
```

Chamar `self._publish_mqtt(record)` dentro de `_save_and_notify`.

- [ ] **Step 10.2: Verificar que o servidor ainda sobe sem MQTT configurado**

```bash
docker compose restart app
docker compose logs app | grep -E "MQTT|simulação|Uvicorn"
```

Esperado: sem linha MQTT (host vazio = desabilitado), simulação ativa.

- [ ] **Step 10.3: Commit**

```bash
git add aguada-web/backend/bridge.py
git commit -m "feat(aguada-web): bridge MQTT opcional para coexistência com HA"
```

---

## Task 11: Testes finais e validação

- [ ] **Step 11.1: Rodar suite completa de testes**

```bash
cd aguada-web
source .venv/bin/activate
python -m pytest tests/ -v --tb=short
```

Esperado: todos `PASSED`.

- [ ] **Step 11.2: Verificar relatório PDF gerado**

```bash
# Gerar relatório para hoje via API
curl "http://localhost/api/report/daily.pdf?date=$(date +%F)" -o /tmp/aguada-test.pdf
file /tmp/aguada-test.pdf  # deve dizer "PDF document"
xdg-open /tmp/aguada-test.pdf
```

- [ ] **Step 11.3: Verificar WebSocket com modo simulação**

Abrir `http://localhost` no browser, aguardar 30s — os cards devem atualizar automaticamente com os dados simulados.

- [ ] **Step 11.4: Commit final**

```bash
git add -A aguada-web/
git commit -m "feat(aguada-web): sistema completo — dashboard, SCADA, histórico, consumo, PDF"
```

---

## Notas de Operação

**Para ativar serial real:**
```bash
# Editar .env
SERIAL_PORT=/dev/ttyACM0  # ou ttyUSB0
# Reiniciar
docker compose restart app
```

**Para ativar MQTT (HA coexistência):**
```bash
# Editar .env
MQTT_HOST=192.168.0.177
# Desligar mqtt_bridge.py existente em tools/
# Reiniciar
docker compose restart app
```

**Dados persistidos em:**
- `aguada-web/data/aguada.db` — SQLite com histórico
- `aguada-web/data/reports/` — PDFs gerados automaticamente às 06:00

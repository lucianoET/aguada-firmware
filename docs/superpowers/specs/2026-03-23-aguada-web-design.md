# Aguada Web — Sistema de Monitoramento Standalone

**Data:** 2026-03-23
**Status:** Aprovado pelo usuário

---

## Visão Geral

Sistema web standalone em Docker para monitoramento de reservatórios de água do CMASM (Ilha do Engenho). Substitui ou coexiste com o Home Assistant. Recebe dados do gateway ESP32-S3 via USB serial, armazena em SQLite, e serve um dashboard web em tempo real com gráficos, análise de consumo e relatório diário em PDF.

---

## Arquitetura

```
[Gateway ESP32-S3]
    ↓ USB Serial (JSON lines, 115200bps)
[bridge thread — Python]
    ├── lê JSON do serial (ou simula em modo dev)
    ├── calcula level_cm, volume_L, pct
    ├── grava em SQLite (readings + reservoir_state)
    └── notifica WebSocket clients
[FastAPI app]
    ├── GET  /api/reservoirs           — estado atual de todos
    ├── GET  /api/history/{alias}      — histórico (query: period=24h|7d|30d)
    ├── GET  /api/consumption          — eventos de consumo/abastecimento
    ├── GET  /api/report/daily         — dados JSON do relatório diário
    ├── GET  /api/report/daily.pdf     — PDF para download
    └── WS   /ws                       — push tempo real (JSON por reservatório)
[Nginx]
    └── serve frontend estático (HTML + JS)
```

### Containers (Docker Compose)

| Serviço | Imagem | Porta | Observação |
|---------|--------|-------|------------|
| `app` | Python 3.12-slim | 8000 (interno) | FastAPI + bridge serial |
| `nginx` | nginx:alpine | 80 (externo) | Serve frontend + proxy para API |

Volume: `./data/aguada.db` montado em `/data/aguada.db` no container `app`.
Device: `/dev/ttyUSB0` (ou `ttyACM0`) passado via `devices:` no Compose.

---

## Estrutura de Arquivos

```
aguada-web/
├── backend/
│   ├── main.py           # FastAPI app, rotas, WebSocket
│   ├── bridge.py         # thread serial → SQLite + WS notify
│   ├── db.py             # schema SQLite, queries
│   ├── calc.py           # level_cm, volume_L, pct, consumo
│   ├── report.py         # geração HTML/PDF com WeasyPrint
│   └── reservoirs.yaml   # parâmetros dos reservatórios
├── frontend/
│   ├── index.html        # Dashboard (tempo real)
│   ├── scada.html        # Diagrama SCADA SVG
│   ├── history.html      # Gráficos históricos
│   ├── consumption.html  # Balanço hídrico
│   └── report.html       # Visualização/download relatório
├── docker-compose.yml
├── Dockerfile
├── nginx.conf
└── requirements.txt
```

---

## Banco de Dados (SQLite)

### Tabela `readings` — histórico de leituras

```sql
CREATE TABLE readings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    ts          INTEGER NOT NULL,     -- unix timestamp (segundos)
    node_id     TEXT    NOT NULL,     -- ex: "0x7758"
    sensor_id   INTEGER NOT NULL,     -- 1 ou 2
    alias       TEXT    NOT NULL,     -- ex: "CON"
    distance_cm REAL,
    level_cm    REAL,
    volume_l    REAL,
    pct         REAL,                 -- 0.0 a 100.0
    rssi        INTEGER,
    vbat        REAL,                 -- tensão em V
    seq         INTEGER
);
CREATE INDEX idx_readings_ts    ON readings(ts DESC);
CREATE INDEX idx_readings_alias ON readings(alias, ts DESC);
```

### Tabela `reservoir_state` — estado atual (upsert)

```sql
CREATE TABLE reservoir_state (
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
    online       INTEGER DEFAULT 1   -- 0 se última leitura > 10min atrás
);
```

Critério online: `now() - ts < 600` segundos (10 minutos).

---

## Bridge Serial

Arquivo: `backend/bridge.py`

**Fluxo:**
1. Tenta abrir `SERIAL_PORT` (env var, default `/dev/ttyUSB0`) a 115200bps
2. Se falhar (modo dev), entra em simulação: gera leituras sintéticas a cada 30s para todos os reservatórios
3. Para cada linha JSON válida recebida do gateway:
   - Extrai `node_id`, `sensor_id`, `distance_cm`, `rssi`, `vbat`, `seq`
   - Busca parâmetros em `reservoirs.yaml` pelo par `(node_id, sensor_id)`
   - Calcula: `level_cm = clamp(level_max_cm - (distance_cm - sensor_offset_cm), 0, level_max_cm)`
   - Calcula: `pct = level_cm / level_max_cm * 100`
   - Calcula: `volume_l = pct / 100 * volume_max_l`
   - Grava em `readings` (INSERT)
   - Upsert em `reservoir_state`
   - Envia JSON pelo WebSocket para todos os clientes conectados

**Formato JSON do gateway** (conforme spec §8):
```json
{
  "v": 3,
  "node": "0x7758",
  "sensor": 1,
  "dist": 215,
  "rssi": -62,
  "vbat": 33,
  "seq": 412,
  "ts": 1711234567
}
```

**Normalização de `node_id`:** o formato canônico em todo o sistema é lowercase com prefixo `0x` (ex: `"0x7758"`). O `bridge.py` normaliza o campo `node` do gateway para lowercase antes do lookup. O `reservoirs.yaml` deve ter suas chaves atualizadas para lowercase (ex: `"0x7758"` em vez de `"0X7758"`). O banco, a API e o frontend usam sempre lowercase.

**Conversão de `vbat`:** o campo `vbat` do gateway é em décimos de volt (ex: `33` = 3.3V). O `bridge.py` converte para volts antes de gravar: `vbat_v = vbat_raw / 10.0`. O banco armazena sempre em volts (campo `vbat REAL`).

---

## API REST

### `GET /api/reservoirs`
Retorna estado atual de todos os reservatórios.
```json
[
  {
    "alias": "CON",
    "name": "Castelo de Consumo",
    "pct": 72.4,
    "level_cm": 325.8,
    "volume_l": 57920,
    "volume_max_l": 80000,
    "rssi": -61,
    "online": true,
    "ts": 1711234567
  }
]
```

### `GET /api/history/{alias}?period=24h`
Retorna array de `{ts, level_cm, volume_l, pct}` para o período.
Períodos aceitos: `24h`, `7d`, `30d`.
Retorna no máximo 500 pontos. Se o número de leituras no período exceder 500, aplica decimação por média de intervalo: divide o período em 500 intervalos iguais e retorna a média de `volume_l` de cada intervalo (ou o ponto mais próximo se o intervalo estiver vazio).

### `GET /api/consumption?alias=CON&date=2026-03-23`
Parâmetros: `alias` (obrigatório), `date` (obrigatório, formato `YYYY-MM-DD`). Retorna 400 se algum faltar.

Retorna eventos de consumo/abastecimento agrupados por hora.
```json
{
  "date": "2026-03-23",
  "alias": "CON",
  "summary": { "consumed_l": 3200, "supplied_l": 8000, "balance_l": 4800 },
  "events": [
    { "hour": "08:00", "vol_start": 45000, "vol_end": 43800, "delta_l": -1200, "type": "consumption" }
  ]
}
```

**Algoritmo de eventos (em `calc.py`):** para cada hora do dia, pega a primeira e última leitura do reservatório naquela hora. `delta_l = vol_end - vol_start`. Classificação: `delta_l < -50` → `consumption`, `delta_l > 50` → `supply`, caso contrário → `stable`. Limiar de 50L filtra ruído do sensor.

### `GET /api/report/daily?date=2026-03-23`
Retorna dados JSON para renderização do relatório.

### `GET /api/report/daily.pdf?date=2026-03-23`
Retorna PDF gerado com WeasyPrint. Conteúdo: sumário por reservatório, gráfico de nível 24h (SVG inline), tabela de eventos, alertas do dia.

### `WS /ws`
Ao conectar, o servidor envia imediatamente um snapshot com o estado atual de todos os reservatórios:
```json
{ "type": "snapshot", "data": [ <reservoir_state>, ... ] }
```
A cada nova leitura recebida do serial, envia atualização do reservatório afetado:
```json
{ "type": "reading", "data": { <reservoir_state> } }
```
O cliente deve reconectar com backoff exponencial (1s, 2s, 4s, máx 30s) em caso de desconexão.

---

## Frontend

**Stack:** HTML5 + Tailwind CSS (CDN) + Alpine.js + Chart.js — sem build step.

**Navegação:** Header fixo com abas: Dashboard | SCADA | Histórico | Consumo | Relatório.

**Design:** Baseado no projeto de referência (Aguada v2.3):
- Paleta de status por percentual: verde (≥80%), azul (50-79%), amarelo (20-49%), vermelho (<20%), cinza (sem dados/offline)
- Dark mode toggle via Alpine.js + localStorage
- Cards com sombra, hover `translateY(-2px)`
- Badges coloridas por status
- Responsivo: 1→2→3→4 colunas por breakpoint

### Páginas

#### `index.html` — Dashboard
- 4 KPI cards: reservatórios totais, nodes online, volume total (m³), % médio geral
- Grid de cards por reservatório com barra de progresso colorida, volume atual, status, timestamp
- Conexão WebSocket: atualiza cards em tempo real sem reload
- Indicador "última atualização" no header

#### `scada.html` — Diagrama SCADA
- SVG interativo 1000×700: layout real da Ilha do Engenho
- Tanques com nível de água animado (altura proporcional ao %)
- Tubulações com `stroke-dasharray` animado quando há fluxo ativo
- Clique em tanque exibe painel lateral com detalhes
- Polling REST `/api/reservoirs` a cada 30s

#### `history.html` — Histórico
- Seletor de reservatório + período (24h / 7d / 30d)
- Line chart (Chart.js) de volume (L) ao longo do tempo
- Tabela paginada de leituras com colunas: timestamp, distância, nível, volume, %, RSSI

#### `consumption.html` — Consumo/Abastecimento
- Filtros: reservatório, data
- 4 cards resumo: consumido, abastecido, saldo, nº eventos
- Bar chart por hora (verde=abastecimento, vermelho=consumo)
- Tabela de eventos: hora, vol início, vol fim, delta, tipo

#### `report.html` — Relatório Diário
- Seletor de data
- Preview HTML do relatório na página
- Botão "Baixar PDF" (`/api/report/daily.pdf?date=...`)
- Relatório inclui: sumário, gráfico SVG 24h, tabela de eventos, alertas

---

## Relatório PDF

Gerado por `backend/report.py` com WeasyPrint.

**Conteúdo:**
1. Cabeçalho: CMASM — Aguada — data
2. Sumário de reservatórios: tabela com alias, nível médio, volume mín/máx, variação 24h
3. Gráfico de nível 24h por reservatório (SVG embutido, gerado com matplotlib ou SVG manual)
4. Tabela de eventos de consumo/abastecimento
5. Alertas: reservatórios abaixo de 20% em algum momento do dia

**Agendamento:** cron diário às 06:00 usando `APScheduler` (dentro do processo FastAPI), gera PDF em `/data/reports/YYYY-MM-DD.pdf`.

---

## Configuração

### `reservoirs.yaml` (cópia de `tools/reservoirs.yaml`)
Mesmo formato existente — parâmetros por `(node_id, sensor_id)`.

### Variáveis de ambiente (`.env`)
```
SERIAL_PORT=/dev/ttyUSB0
DATA_DIR=/data
TZ=America/Sao_Paulo
```

### `docker-compose.yml` (esquema)

A porta serial é configurada via `.env` e referenciada em um único lugar:

```yaml
services:
  app:
    build: .
    devices:
      - ${SERIAL_PORT:-/dev/ttyUSB0}:${SERIAL_PORT:-/dev/ttyUSB0}
    volumes:
      - ./data:/data
    env_file: .env
  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
    volumes:
      - ./frontend:/usr/share/nginx/html:ro
      - ./nginx.conf:/etc/nginx/conf.d/default.conf:ro
```

Para usar `ttyACM0`, o operador edita apenas o `.env`: `SERIAL_PORT=/dev/ttyACM0`.

---

## Coexistência com Home Assistant

Dois processos **não podem** abrir a mesma porta serial simultaneamente. A solução é o `bridge.py` ser o único leitor serial e publicar os dados em dois destinos:

1. **SQLite** (sempre) — alimenta o Aguada Web
2. **MQTT** (opcional) — alimenta o Home Assistant, ativado por `MQTT_HOST` no `.env`

Com isso o HA deixa de ler o serial diretamente e passa a receber via MQTT publicado pelo `bridge.py`. O processo `mqtt_bridge.py` existente em `tools/` deve ser desligado quando o Aguada Web estiver rodando.

Variáveis de ambiente adicionais para MQTT:
```
MQTT_HOST=192.168.0.177   # vazio = desabilitado
MQTT_PORT=1883
MQTT_USER=
MQTT_PASS=
```

---

## Fora de Escopo

- Autenticação/login (rede local apenas)
- Comandos para o gateway (CMD_CONFIG, OTA) — só leitura
- Múltiplos usuários
- Notificações push/email

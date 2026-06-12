# Node AirQ — Qualidade do Ar (indoor)

Variante do Aguada Node para ambientes internos. Mede **qualidade do ar, conforto
térmico, luminosidade e ruído**, publica por ESP-NOW → gateway → MQTT e serve um
**captive portal WiFi** local (sem broker) organizado **por componente de hardware**.

- **MCU:** ESP32-C3 SuperMini
- **Modo:** `sensor_type = AIR_QUALITY (1)`, `relay_enabled = true`, `num_sensors = 1`
- **Env PlatformIO:** `ani-01-c3`
- **AP captive:** `ANI-{node_id}` (ex.: `ANI-0494`) → `http://192.168.4.1`
- **node_id:** 2 últimos bytes do MAC (ex.: `0x0494`)

> O captive portal mostra o nome **AirQ / ARQ-{node_id}**; o SSID do AP ainda é `ANI-…`.

---

## Componentes de sensor

| Componente | Tipo / Interface | GPIO | Mede | Estado |
|---|---|---|---|---|
| **AHT21** | Digital I2C `0x38` | SDA 6 / SCL 7 | Temperatura, Humidade | ✅ Validado |
| **ENS160** | Digital I2C `0x52` | SDA 6 / SCL 7 | eCO₂, TVOC, AQI | ✅ Validado |
| **TEMT6000** | Analógico (ADC1) | SIG 3 | Luminosidade | ✅ Validado |
| **KY-037/038** | Analógico (ADC1, A0) | A0 1 | Ruído ambiente | 🟠 Ligado, **por validar** (ver `todo.md`) |

> O bus I2C (SDA 6 / SCL 7) é partilhado pelo AHT21 + ENS160. O `relay_aux` **não**
> toca no I2C neste node (o bus pertence ao ANI).

---

## Variáveis lidas e unidades

| Variável | Sensor | Campo `/data` | Unidade | Gama típica | Notas |
|---|---|---|---|---|---|
| Temperatura | AHT21 | `temp_c` | °C | -40 … 85 | compensa o ENS160 |
| Humidade relativa | AHT21 | `hum_pct` | % HR | 0 … 100 | |
| eCO₂ | ENS160 | `eco2` | ppm | 400 … 65000 | estimado por VOCs |
| TVOC | ENS160 | `tvoc` | ppb | 0 … 65000 | |
| AQI (UBA) | ENS160 | `aqi` | índice 1–5 | 1=excelente … 5=muito mau | |
| Luminosidade | TEMT6000 | `lux` | lux (estimado) | 0 … 65535 | **não calibrado** |
| Luminosidade (rel.) | TEMT6000 | `lpct` | % | 0 … 100 | mv / fundo-de-escala |
| Luminosidade (raw) | TEMT6000 | `lraw` | counts ADC | 0 … 4095 | diagnóstico |
| Nível sonoro | KY-037/038 | `db` | "dB" relativo | ~35 … 90 | **não é dB SPL calibrado** |
| Som RMS | KY-037/038 | `srms` | counts ADC | — | diagnóstico |
| Som pico-a-pico | KY-037/038 | `svpp` | counts ADC | — | diagnóstico |

### Campos de estado / diagnóstico (`/data`)

| Campo | Significado |
|---|---|
| `valid` | leitura I2C (AHT/ENS) válida |
| `aht` / `ens` / `light` / `sound` | presença/saúde de cada sensor |
| `warmup` | ENS160 nos ~3 min iniciais de aquecimento |
| `node` | node_id (hex, 4 dígitos) |
| `fw` | versão de firmware |
| `up` | uptime (s) |
| `heap` | heap livre (bytes) |
| `age` | segundos desde a última leitura local |

---

## Caminhos de dados

| Caminho | Cadência | O que transporta |
|---|---|---|
| **Captive page** (`/data`, polling 5 s) | leitura local a **8 s** | **todos** os sensores (ar, T/H, luz, som, diagnóstico) |
| **ESP-NOW → MQTT** | envio a **`interval_send_s` (120 s)** | só **eCO₂+AQI** (sensor_id 1) e **T/H** (env) |

> ⚠️ **Luz e som ainda NÃO são publicados** por ESP-NOW/MQTT — só aparecem no captive
> portal. Integração no Home Assistant está pendente (ver `todo.md`).

A leitura local (8 s) está desacoplada do envio ESP-NOW (120 s): a captive page fica
"ao vivo" sem aumentar o tráfego de rádio.

---

## Captive portal (estrutura por componente)

```
node ESP32 (AirQ · ARQ-0494 · ao vivo há Xs)
├── AHT21   → cards Temperatura, Humidade
├── ENS160  → AQI (5 badges), eCO₂, TVOC, [Valores de referência]
├── TEMT6000→ card Luminosidade
├── KY-037  → card Ruído ambiente
└── footer  → Node, Firmware, Uptime, Heap, Canal
```

Recursos: estado ao vivo (verde/âmbar/vermelho), mini-gráficos (sparklines
bufferizadas no browser), dark mode automático, banner de warmup do ENS160,
pills OK/falha por sensor.

---

## Pinagem (ESP32-C3 SuperMini)

| Função | GPIO | Notas |
|---|---|---|
| I2C SDA (ENS160+AHT21) | 6 | |
| I2C SCL | 7 | |
| LED onboard | 8 | active-LOW / WS2812B |
| Botão BOOT | 9 | HELLO / restart |
| TEMT6000 SIG | 3 | ADC1_CH3, `DEFAULT_LIGHT_PIN` |
| KY-037/038 A0 | 1 | ADC1_CH1, `DEFAULT_SOUND_PIN` |

> Sensores analógicos: usar **ADC1 (GPIO0–4)**; o ADC2/GPIO5 não funciona com WiFi.
> Alimentar o KY-037/038 a **3.3 V** (não 5 V) para os picos do A0 ficarem no ADC.
> Pinagem completa: [`firmware/node/node_pinout.md`](firmware/node/node_pinout.md).

---

## Build & flash

```bash
cd firmware/node
pio run -e ani-01-c3 -t upload --upload-port /dev/ttyACM0
```

Build flags úteis (override de pinos/calibração):

| Flag | Default | Descrição |
|---|---|---|
| `DEFAULT_LIGHT_PIN` | 3 | pino ADC do TEMT6000 |
| `DEFAULT_SOUND_PIN` | 1 | pino ADC do KY-037/038 (A0) |
| `SOUND_DB_OFFSET` | 25 | offset de calibração do nível sonoro relativo |
| `SOUND_DEBUG` | — | monitor raw rápido do A0 no serial (bancada) |

---

## Ficheiros

| Ficheiro | Conteúdo |
|---|---|
| `firmware/node/src/ani_sensor.cpp/.h` | driver AHT21 + ENS160 (I2C) |
| `firmware/node/src/light_sensor.cpp/.h` | driver TEMT6000 (lux) |
| `firmware/node/src/sound_sensor.cpp/.h` | driver KY-037/038 (nível sonoro) |
| `firmware/node/src/main.cpp` | loop, `ani_tick`, captive `/data`, ESP-NOW |
| `firmware/node/include/ani_web.h` | página do captive portal (HTML/CSS/JS) |
| `firmware/node/node_pinout.md` | pinagem detalhada |
| `todo.md` | trabalho pendente deste node |

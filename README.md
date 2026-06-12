# Aguada — Telemetria de Reservatórios (ESP-NOW + Home Assistant)

Sistema de telemetria de nível/volume de reservatórios e qualidade do ar com:

- **nodes ESP32-C3** — ultrassónico (HC-SR04), qualidade do ar (ANI-01: ENS160+AHT21) ou relay mesh
- **gateway ESP32-S3 ou ESP32 clássico** — ESP-NOW → USB serial → bridge.py
- **`tools/bridge.py`** — cálculo + MQTT + MQTT Discovery
- **Home Assistant** — dashboard, templates, automações

## Arquitectura

```text
Node(s) ESP32-C3  ──ESP-NOW──►  Gateway ESP32  ──serial──►  bridge.py  ──MQTT──►  Home Assistant
    ANI-01 AP  ──WiFi──►  browser  (captive portal, sem broker)
```

## Tipos de node

| Tipo | Sensor | DEFAULT_SENSOR_TYPE | Env PlatformIO |
| --- | --- | --- | --- |
| Ultrassónico | HC-SR04 (nível de água) | 0 | `esp32-c3-supermini` |
| Qualidade do ar | ENS160 + AHT21 (ANI-01) | 1 | `ani-01-c3` |
| Relay mesh | — (só retransmite) | 0, num_sensors=0 | qualquer |

> Pinagem detalhada: [`firmware/node/node_pinout.md`](firmware/node/node_pinout.md)

## Build e flash

### Node ultrassónico

```bash
cd firmware/node
pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0
```

### Node ANI-01 (qualidade do ar)

```bash
cd firmware/node
pio run -e ani-01-c3 -t upload --upload-port /dev/ttyACM0
```

Após o flash o node cria uma rede WiFi **`ANI-XXXX`** (captive portal).
Ligue-se a essa rede — o browser abre automaticamente `http://192.168.4.1` com leituras em tempo real (sem broker, sem dependências).

### Gateway

```bash
cd firmware/gateway && pio run -e gateway-c3          # ESP32-C3
cd firmware/gateway && pio run -e gateway-s3          # ESP32-S3
cd firmware/gateway && pio run -e gateway-esp32-usb   # ESP32 clássico
cd firmware/gateway-ethernet && pio run -e gateway-esp32-enc28j60  # ESP32 + ENC28J60
```

Upload (exemplos):

```bash
cd firmware/gateway && pio run -e gateway-esp32-usb -t upload --upload-port /dev/ttyACM1
cd firmware/gateway-ethernet && pio run -e gateway-esp32-enc28j60 -t upload --upload-port /dev/ttyACM0
```

### Gateway Ethernet (ESP32 + ENC28J60)

- Projecto: `firmware/gateway-ethernet`
- Fluxo: ESP-NOW (canal 1) → JSON v3 → MQTT via Ethernet
- Tópicos publicados: `aguada/gateway/status`, `aguada/gateway/raw`, `aguada/{node_id}/{sensor_id}/raw`
- Pinagem ENC28J60 (ESP32 DevKit): SCK=`GPIO18`, MISO=`GPIO19`, MOSI=`GPIO23`, CS=`GPIO5`

## Node ANI-01 — Qualidade do Ar

O ANI-01 é um node de interior com ENS160 + AHT21 (placa combinada) e, opcionalmente,
um sensor de luz **TEMT6000** e um sensor de som **KY-037/038**.
Publica dados via ESP-NOW → gateway → MQTT e serve também um captive portal WiFi directo.

O captive portal está organizado **por componente de hardware** (node ESP32 → sensores
AHT21, ENS160, TEMT6000 e KY-037), com estado ao vivo, mini-gráficos e diagnóstico do node.

### MQTT (via gateway)

| Tópico | Payload |
| --- | --- |
| `aguada/{node_id}/1/state` | `{"eco2":450,"aqi":2,"rssi":-65,...}` |
| `aguada/{node_id}/env/state` | `{"temp_c":23.5,"hum_pct":62}` |
| `aguada/{node_id}/status` | `online` / `offline` |

### Captive portal (sem broker)

Ligar ao WiFi **`ANI-{node_id}`** (ex.: `ANI-0494`) — o browser abre `http://192.168.4.1` automaticamente.
Página alternativa para uso com broker MQTT: [`tools/airq.html`](tools/airq.html).

### Ligação do módulo ENS160+AHT21

| Pino do módulo | Liga a (ESP32-C3 SuperMini) | Notas |
| --- | --- | --- |
| VIN | 3V3 | alimentação |
| GND | GND | massa |
| SDA | GPIO 6 | I2C dados |
| SCL | GPIO 7 | I2C clock |
| ADD | GND | endereço ENS160 = 0x52 |
| **CS** | **3V3** | **obrigatório — activa modo I2C** |
| INT | — | não necessário |

> ⚠️ Se CS estiver solto ou ligado a GND o ENS160 entra em modo SPI e não responde no I2C.

### Ligação do sensor de luz TEMT6000 (opcional)

Sensor analógico de luminosidade. A saída `SIG` liga a um pino **ADC1** (GPIO0–4);
o GPIO5/ADC2 não funciona com WiFi ligado.

| Pino do módulo | Liga a (ESP32-C3 SuperMini) | Notas |
| --- | --- | --- |
| VCC | 3V3 | alimentação |
| GND | GND | massa |
| SIG | GPIO 3 | ADC1_CH3 (configurável via `DEFAULT_LIGHT_PIN`) |

> A luminosidade aparece no captive portal (card **TEMT6000 · Luminosidade**, em lux estimado).
> O lux é uma estimativa não calibrada — fiável para tendência e nível relativo.
> Ainda **não** é publicada via ESP-NOW/MQTT (só captive page).

### Ligação do sensor de som KY-037 / KY-038 (opcional)

Microfone electret com saída analógica (A0). **Alimentar a 3.3 V** (não 5 V) para os
picos do A0 ficarem dentro do ADC. Usar um pino **ADC1** diferente do da luz.

| Pino do módulo | Liga a (ESP32-C3 SuperMini) | Notas |
| --- | --- | --- |
| + / VCC | 3V3 | ⚠ 3.3 V — bias do A0 ~1.65 V |
| G / GND | GND | massa |
| A0 | GPIO 1 | ADC1_CH1 (configurável via `DEFAULT_SOUND_PIN`) |
| D0 | — | não usado |

> Mostra um **nível sonoro relativo "tipo dB"** (card **KY-037 · Ruído ambiente**),
> com classificação de conforto (Silencioso → Muito ruidoso). **Não é dB SPL calibrado** —
> é um indicador relativo de ruído ambiente. Calibra o offset (`SOUND_DB_OFFSET`) após ligar.
> Só captive page (não publicado via MQTT).

## Bridge

```bash
pip install -r tools/requirements.txt
python tools/bridge.py --port /dev/ttyACM0 --mqtt localhost
```

Autostart recomendado (Linux):

```bash
./tools/install_autostart_user_service.sh
```

O serviço `aguada-bridge-autoswitch.service` sobe o bridge automaticamente quando um `/dev/ttyACM*` ou `ttyUSB*` aparece.

### Observabilidade do gateway USB

| Tópico MQTT | Conteúdo |
| --- | --- |
| `aguada/gateway/status` | `online` / `offline` (retained) |
| `aguada/gateway/health` | JSON com uptime, heap, drops, CRC (retained) |
| `aguada/gateway/ack` | ACKs de comandos (não retained) |

## Home Assistant

Para evitar entidades duplicadas ou sem dados use MQTT Discovery (publicado pelo `bridge.py`).

No `configuration.yaml` do HA:

```yaml
mqtt:
sensor: !include aguada/statistics_sensors.yaml
template: !include aguada/template_sensors.yaml
automation: !include automations.yaml
```

Ficheiros de referência: `homeassistant/configuration_snippet.yaml`, `homeassistant/configuration_full.yaml`.

> Os ficheiros `homeassistant/mqtt_sensors.yaml` e `homeassistant/mqtt_gateway_sensors.yaml` são **legados** — não usar com discovery activo.

## Referências

| Documento | Conteúdo |
| --- | --- |
| [`Node AirQ.md`](Node%20AirQ.md) | Resumo do node AirQ: sensores, variáveis, unidades, componentes |
| [`todo.md`](todo.md) | Trabalho pendente do node AirQ |
| [`AGUADA_SYSTEM_DOC.md`](AGUADA_SYSTEM_DOC.md) | Especificação completa do protocolo e sistema |
| [`ROADMAP.md`](ROADMAP.md) | Estado das fases e próximas acções |
| [`firmware/node/node_pinout.md`](firmware/node/node_pinout.md) | Pinagem detalhada de todos os modos e hardware |
| [`tools/airq.html`](tools/airq.html) | Página web de qualidade do ar (versão MQTT/broker) |

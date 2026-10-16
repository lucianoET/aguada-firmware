# Gateway ESP-NOW → Ethernet (WT32-ETH01) — design

Data: 2026-10-08 · Status: aprovado

## Objetivo

Substituir o gateway de produção (ESP32-C3 `8C:D0:B2:A8:AB:D6` na USB do notebook,
env `gateway-c3-usb-wifi-ch11`) por uma ESP32-ETH01 V1.4 (WT32-ETH01, PHY LAN8720A)
ligada por cabo ao roteador. Ganhos: libera a USB do notebook, o roteador deixa de
precisar ficar no canal 11, o rádio fica dedicado ao ESP-NOW.

Secundário: AP WiFi ligado por botão, com página de status e configuração de rede.

## Decisões

| Tema | Decisão |
|---|---|
| Toolchain | PlatformIO, mesmo `firmware/gateway/` e mesmo core (pioarduino 55.03.311). ESPHome descartado: não permite `wifi:` + `ethernet:` juntos. |
| Base | `main.cpp` de produção ganha transporte Ethernet sob `#ifdef GATEWAY_ETH`. JSON sai das mesmas funções — sem segunda implementação para divergir. `main_wifi.cpp` não serve de base: publica `aguada/0x…/state`, que colide com o state calculado pelo bridge. |
| AP + página + config | arquivo novo `src/ap_portal.cpp` (+ `.h`), compilado só no env ETH. |
| Rollout | substituição direta após validação em bancada (sem período em paralelo). |
| Canal ESP-NOW | continua em build flag (`ESPNOW_CHANNEL=11`). Nodes também são fixos no build. |

## Hardware

| Função | GPIO | Build flag |
|---|---|---|
| ETH MDC / MDIO | 23 / 18 | `ETH_MDC_PIN` / `ETH_MDIO_PIN` |
| PHY power (habilita oscilador 50 MHz) | 16 | `ETH_POWER_PIN` |
| Clock RMII | 0 (entrada) | — (`ETH_CLOCK_GPIO0_IN`) |
| PHY addr | 1 | `ETH_PHY_ADDR` |
| Botão AP | 4 → GND, `INPUT_PULLUP` | `AP_BUTTON_PIN` |
| LED status (externo, opcional) | 2 → 330 Ω → LED → GND | `LED_PIN` (`-1` desativa) |
| Gravação | TX0=1, RX0=3, IO0→GND no boot | — |

A placa não tem USB: gravação por conversor USB-TTL 3,3 V. Não tem LED de usuário
(confirmar na placa). GPIO 12 evitado (strapping de tensão da flash).

## Fluxo de dados

```
Node ──ESP-NOW ch11──► WT32-ETH01 ──Ethernet/MQTT──► broker 192.168.0.8
                         │   publica  aguada/gateway/rx   (linhas JSON, iguais à serial)
                         │   assina   aguada/gateway/cmd  (linhas {"cmd":...}, iguais à serial)
                         └─ Serial 115200 continua emitindo o mesmo JSON (debug via USB-TTL)

bridge.py (SERIAL_PORT=none) ◄── aguada/gateway/rx ──► matemática, HA discovery, states
aguada-web                   ◄── aguada/gateway/rx  (sem mudança)
```

- `json_out()`: no build ETH publica **toda** linha em `aguada/gateway/rx` (não só as
  `mirror=true`) — é o único link, e o bridge precisa de `GATEWAY_READY`/`CMD_ACK`.
- Mensagem em `aguada/gateway/cmd` → `process_cmd(payload)` existente. Formato de
  comando não muda.
- `GATEWAY_STATUS_INTERVAL_MS=60000` no env: heartbeat que mantém o bridge vendo o gateway.
- Relógio: `configTime()` (NTP) quando o link ETH sobe; `unix_now()` usa `time()` se
  sincronizado, senão o offset do `SETTIME` como hoje.
- Sem LWT no firmware: `aguada/gateway/status` continua sendo do bridge, que marca
  offline por `--gateway-timeout`.
- MQTT reconecta com o mesmo backoff do mirror (15 s), sem bloquear o loop do ESP-NOW.
- ESP-NOW: `WiFi.mode(WIFI_STA)` sem conexão, canal fixo — rádio livre. Nodes mandam em
  broadcast, então ligar o AP (modo `AP_STA`, mesmo canal) não muda a recepção.

## Mudança no bridge.py

`--port none` (`SERIAL_PORT=none` no `bridge.env`) → modo só-MQTT:

1. `run()` não abre serial; o loop só roda `tracker.check_timeouts()` e o timeout do gateway.
2. `_serial_write(line)` publica `line` em `aguada/gateway/cmd` quando não há serial.
3. Gateway online/offline vem de `_mark_gateway_seen()` (já chamado no `dispatch` via MQTT)
   + `--gateway-timeout` (usar ~180 s, 3× o intervalo de status).

Teste: caso novo em `tools/test_bridge_dispatch.py` cobrindo (2) e o loop sem serial.

## AP e página (`ap_portal.cpp`)

**Interação**

1. Técnico aperta o botão (debounce 50 ms) → AP `aguada-gw-XXXX` (2 últimos bytes do MAC),
   WPA2, canal 11. LED acende fixo.
2. Conecta o celular; DNS curinga faz o celular abrir a página sozinho (senão `http://192.168.4.1`).
3. AP desliga após 10 min sem requisição HTTP (cada requisição renova). Novo aperto com AP
   ligado só renova o prazo. LED apaga.

Página única (`/`), padrão do portal do `gps_tracker`: `GET /api/state` a cada 2 s e
`POST /api/config`. Tema claro/escuro pelo sistema.

**Status**

- Cabeçalho: link ETH (sem cabo / sem IP / IP), MQTT (conectado / tentando), relógio,
  uptime, fw, contadores. Sem contagem do AP: o polling da página aberta renova o prazo.
- Tabela de nodes: id, último tipo, `distance_cm`, RSSI, idade. Idade > 5 min em destaque.
- Vazio: "nenhum pacote ainda". Erro de fetch: faixa "gateway sem resposta".
- Tabela alimentada por um cache em RAM (até `NODE_CACHE_MAX`=20 nodes), atualizado no
  mesmo ponto onde o pacote vira JSON.

**Configuração** (seção recolhível da mesma página)

- Campos: DHCP ou IP fixo (IP, gateway, máscara, DNS), broker MQTT, porta, usuário,
  senha, senha do AP (mín. 8 caracteres, exigência do WPA2).
- Senha em branco = mantém a atual. IP inválido → erro inline, nada salvo.
- Salvar → NVS (`Preferences`, namespace `gwcfg`) → página "Salvo, reiniciando…" → reboot.
- Só responde a clientes do AP (`client().localIP() == WiFi.softAPIP()`); o aperto físico
  do botão é a autorização. IP fixo errado tem saída: o botão sempre abre o AP.

**Valores de fábrica**: `secrets.ini` (`mqtt_host`, `mqtt_user`, `mqtt_pass`, novo
`ap_pass`) como defaults de build; NVS sobrescreve. Primeiro boot funciona sem configurar.

## LED (se presente)

| Estado | LED |
|---|---|
| AP ligado | aceso fixo |
| Pacote ESP-NOW recebido | piscada curta (sobre o estado base) |
| Sem link ETH | 2 piscadas a cada 2 s |
| Normal | apagado |

## Build

Env novo `gateway-wt32-eth` em `firmware/gateway/platformio.ini`: `board = esp32dev`,
`-DGATEWAY_ETH -DESPNOW_CHANNEL=11 -DGATEWAY_STATUS_INTERVAL_MS=60000`, credenciais via
`${secrets.*}`, `lib_deps` ArduinoJson + PubSubClient. `WebServer`, `ETH`, `Preferences`
vêm do core.

## Validação

**Bancada** (sem tocar produção):

1. `pio run -e gateway-wt32-eth -t upload` via USB-TTL; boot log mostra link ETH + IP.
2. `mosquitto_sub -h 192.168.0.8 -u aguada -P … -t 'aguada/gateway/#' -v`: linhas de node
   real + `GATEWAY_STATUS` a cada 60 s. (Durante a bancada o C3 de produção continua
   publicando; o dedupe por seq do bridge e do aguada-web absorve.)
3. `mosquitto_pub -t aguada/gateway/cmd -m '{"cmd":"STATUS"}'` → `GATEWAY_STATUS` em rx.
4. Cabo puxado e devolvido → reconecta sozinho, ESP-NOW não para (serial continua).
   IP fixo pela página → reinicia e sobe com o IP novo.
5. Botão → AP aparece, página carrega, tabela popula, config salva e reinicia; AP some
   após 10 min parado. Pacotes continuam chegando com o AP ligando/desligando (troca
   de modo STA ↔ AP_STA não pode derrubar o ESP-NOW).
6. `.venv/bin/python tools/test_bridge_dispatch.py`.

**Troca** (produção — avisar antes):

1. `systemctl --user stop aguada-bridge`; desplugar o C3.
2. `bridge.env`: `SERIAL_PORT=none`, adicionar `--gateway-timeout 180` no `ExecStart`.
3. `systemctl --user daemon-reload && systemctl --user start aguada-bridge`.
4. Conferir HA e aguada-web recebendo.

Volta: replugar o C3, restaurar `bridge.env`, reiniciar o serviço.

## Fora do escopo

Canal ESP-NOW configurável em runtime, OTA, status pela LAN (já existe via MQTT/HA),
autenticação além do botão físico, reset de fábrica por toque longo.

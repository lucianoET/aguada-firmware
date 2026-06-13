# Teste básico RFID → Captive Portal

Data: 2026-06-12
Status: aprovado

## Objetivo

ESP32-C3 SuperMini lê o UID de tags 125 kHz através do leitor **ID-12** (Innovations,
breakout SparkFun) pela UART e exibe o **último código lido** numa página captive portal.
Também loga cada leitura na serial USB para depuração.

É um **sketch standalone de teste** — isolado do firmware do nó (`firmware/node/`), sem
ESP-NOW, MQTT, NVS, LEDs ou buzzer. Serve para validar leitura do RFID + exibição.

## Hardware / ligação

| ID-12   | ESP32-C3 SuperMini                         |
| ------- | ------------------------------------------ |
| VCC     | 5V                                         |
| GND     | GND                                        |
| TX (D0) | divisor 1k / 2k2 → GPIO20 (RX)             |

- Divisor reduz ~5V → ~3,4V, seguro para o ESP32-C3.
- `Serial1` em **9600 8N1**, RX = GPIO20, TX desabilitado (-1).
- Pino **CP não usado** neste teste (leitura contínua por UART basta).
- Debug pela USB nativa (`Serial`, 115200) — GPIO20 livre porque o log vai pela USB-CDC.

## Protocolo do ID-12 (frame ASCII, 16 bytes)

```
STX(0x02) + 10 chars hex (UID, 5 bytes) + 2 chars hex (checksum XOR) + CR(0x0D) + LF(0x0A) + ETX(0x03)
```

Parser:
1. Ignora bytes até receber STX.
2. Acumula os caracteres seguintes até ETX.
3. Espera exatamente 12 chars hex (10 UID + 2 checksum) entre STX e ETX (CR/LF ignorados).
4. Valida: XOR dos 5 bytes do UID == byte de checksum. Frame inválido é descartado.
5. UID válido → guarda string de 10 chars + timestamp `millis()`, loga na serial.

## Firmware

```
firmware/rfid_test/
├── platformio.ini   # env esp32-c3-supermini, USB-CDC on boot, upload em /dev/ttyACM0
├── src/main.cpp     # setup/loop, parser ID-12, handlers HTTP
└── include/page.h   # HTML/JS em PROGMEM (PAGE_HTML)
```

### setup()
- `Serial.begin(115200)` (USB debug).
- `Serial1.begin(9600, SERIAL_8N1, 20, -1)`.
- WiFi modo `WIFI_AP`, SSID `RFID-TEST`, aberto (sem senha), IP 192.168.4.1.
- `DNSServer` na porta 53 → redireciona tudo para 192.168.4.1 (força captive).
- `WebServer` na porta 80 com rotas abaixo.

### loop()
- `dns.processNextRequest()`
- `server.handleClient()`
- Drena `Serial1`, alimenta o parser; ao completar frame válido atualiza estado global
  (`g_last_uid`, `g_last_ms`) e imprime `RFID UID: XXXXXXXXXX` na serial USB.

### Rotas HTTP
- `GET /`     → HTML PROGMEM (fundo escuro, UID grande centralizado, JS poll 1s).
- `GET /last` → JSON `{"uid":"0F0184D2A1","age_ms":1234}` (`uid` vazio se nada lido).
- `*` (404)   → redirect 302 para `http://192.168.4.1/`.

## Critério de sucesso

1. Aproximar cartão/adesivo → serial USB mostra `RFID UID: <código>`.
2. Conectar ao WiFi `RFID-TEST` no celular → abre página captive automaticamente.
3. Página mostra o último UID em destaque; trocar de tag atualiza o valor em ~1s.

## Fora de escopo (futuro)

LEDs verde/vermelho, buzzer, pino CP por interrupção, ESP-NOW, MQTT, persistência NVS,
histórico/contador de leituras, integração como `SENSOR_TYPE_RFID` no firmware do nó.

# Nó RFID — leitor ID-12 → Captive Portal

Nó de teste do sistema **Aguada** que lê o UID de tags 125 kHz com um leitor
**ID-12** e mostra o último código numa página captive portal servida pelo
próprio ESP32-C3. Cada leitura também é impressa na serial USB.

É um **sketch standalone de teste**, isolado do firmware do nó principal
(`firmware/node/`): sem ESP-NOW, MQTT, NVS, LEDs ou buzzer. Serve para validar a
leitura do RFID e a exibição antes de integrar ao framework.

> Spec de design: [`docs/superpowers/specs/2026-06-12-rfid-captive-test-design.md`](../../docs/superpowers/specs/2026-06-12-rfid-captive-test-design.md)

---

## Componente: leitor ID-12 (Innovations / SparkFun)

Leitor RFID **125 kHz** para tags EM4100, saída serial TTL assíncrona.

| Parâmetro     | Valor               |
| ------------- | ------------------- |
| Frequência    | 125 kHz             |
| Alimentação   | 5 V                 |
| Interface     | UART TTL, **9600 8N1** |
| Formato saída | frame ASCII (ver abaixo) |
| Tags          | EM4100 / compatíveis (cartões e adesivos) |

### Pinagem (breakout)

```
VCC   GND   TX   TXR   AN1   AN2   CP   GND
```

- **VCC / GND** — alimentação 5 V.
- **TX (D0)** — saída serial ASCII (a usada neste firmware).
- **CP (Card Present)** — muda de estado quando há tag no campo; pode ir a um GPIO
  para gerar interrupção (não usado neste teste).
- **TXR, AN1, AN2** — saídas alternativas (Magstripe / Wiegand / antena); não usadas.

### Frame de saída do ID-12

Cada leitura emite 16 bytes:

```
STX(0x02) │ 10 chars hex (UID, 5 bytes) │ 2 chars hex (checksum XOR) │ CR │ LF │ ETX(0x03)
```

O **checksum** é o XOR dos 5 bytes do UID. O parser valida o checksum e descarta
frames corrompidos (ruído na ligação aparece como `checksum invalido` na serial).

---

## Ligação ao ESP32-C3 SuperMini

A saída TX do ID-12 é **5 V** — acima do limite do ESP32-C3 (3,3 V). Use um
divisor resistivo 1k / 2k2 (≈3,4 V) entre TX e o GPIO de RX.

| ID-12   | ESP32-C3 SuperMini             |
| ------- | ------------------------------ |
| VCC     | 5V                             |
| GND     | GND                            |
| TX (D0) | 1k / 2k2 → **GPIO20** (RX)     |

```
ID-12 TX ──[ 1k ]──┬── GPIO20 (RX, Serial1)
                   │
                [ 2k2 ]
                   │
                  GND
```

`Serial1` é configurada como **9600 8N1, RX=GPIO20, TX desabilitado**. O log de
depuração usa a USB nativa (`Serial`, 115200), então o GPIO20 fica livre.

---

## Firmware

| Arquivo                | Função                                                        |
| ---------------------- | ------------------------------------------------------------- |
| [`src/main.cpp`](src/main.cpp)   | parser do frame ID-12, WiFi AP + DNS + WebServer, log serial |
| [`include/page.h`](include/page.h) | página captive em PROGMEM (`PAGE_HTML`)                     |
| [`platformio.ini`](platformio.ini) | env `esp32-c3-supermini`, upload em `/dev/ttyACM0`         |

### Comportamento

1. **setup** — inicia `Serial` (USB) e `Serial1` (RFID); sobe AP aberto `RFID-TEST`
   em `192.168.4.1`; DNS responde tudo para esse IP (força o captive); WebServer na porta 80.
2. **loop** — atende DNS + HTTP e drena o `Serial1`. Ao completar um frame válido,
   guarda o UID + timestamp e imprime `RFID UID: XXXXXXXXXX` na serial.

### Rotas HTTP

| Rota        | Resposta                                                  |
| ----------- | --------------------------------------------------------- |
| `GET /`     | página HTML (UID grande, atualiza a cada 1 s via `fetch`) |
| `GET /last` | `{"uid":"0F0184D2A1","age_ms":1234}` (`uid` vazio se nada lido) |
| `*` (404)   | redirect `302` → `http://192.168.4.1/`                    |

---

## Build, gravação e teste

```bash
cd firmware/rfid_test
pio run                       # compila
pio run -t upload             # grava (porta /dev/ttyACM0)
pio device monitor            # serial 115200
```

Validação:

1. Aproxime um cartão/adesivo → a serial mostra `RFID UID: <código>`.
2. Conecte ao WiFi **`RFID-TEST`** (aberto) no celular → a página captive abre sozinha.
3. A página mostra o último UID; trocar de tag atualiza o valor em ~1 s.

---

## Próximos passos (fora do escopo deste teste)

- LED verde / vermelho e buzzer para feedback de leitura.
- Pino **CP** por interrupção (acordar só quando há tag) → deep sleep.
- Lista branca de UIDs + persistência em NVS.
- Integrar como `SENSOR_TYPE_RFID` no firmware do nó, publicando o evento via
  ESP-NOW para o gateway:

  ```json
  { "type": "rfid", "uid": "0F0184D2A1", "reader": "portaria", "ts": 12345678 }
  ```

  O gateway encaminha para MQTT / Home Assistant, como os demais nós Aguada.

# GPS Tracker — Pinagem

## ⚡ Placa atual: ESP32-C3 SuperMini (antena externa)

Enumera como `/dev/ttyACM0` (USB nativo, sem chip serial). Env PlatformIO: `esp32-c3-supermini`.

### GPS (UART1)

| Pino módulo GPS | Liga a (C3 SuperMini) | Notas |
| --- | --- | --- |
| VCC | **5V** | breakout NEO-6M (GY-GPS6MV2) tem LDO AMS1117 — precisa 5 V; com 3V3 o módulo browna e cala. ATGM336H: 3V3 direto ok |
| GND | GND | massa comum |
| TX | **GPIO20** (RX) | dados NMEA GPS → ESP32 |
| RX | **GPIO21** (TX) | opcional |

LED onboard: **GPIO8** (active-high nesta placa; `LED_ACTIVE_LOW=0`).

### Orçamento de pinos v2 (periféricos planejados — HW-02..07, ainda não montar)

| GPIO | Função planejada | Notas |
| --- | --- | --- |
| 0 | ADC bateria | ADC1_CH0 + divisor (Fase 5) |
| 1 | TFT RST | ST7735 1.8" SPI |
| 2 | TFT DC | strapping — precisa ficar alto no boot (DC idle ok) |
| 3 | TFT MOSI (SDA) | |
| 4 | TFT SCK | |
| 5 | TFT CS | |
| 6 | I2C SDA | **MPU6050** (0x68) + **HTU21D** (0x40) no mesmo barramento |
| 7 | I2C SCL | convenção do repo (AirQ usa 6/7) |
| 8 | LED onboard | TFT BL → 3V3 direto (sem dimmer) |
| 9 | botão (boot) | strapping — só entrada c/ pull-up |
| 10 | DS18B20 (HW-07, indefinido) | 1-Wire, pull-up 4k7 |
| 20 | GPS RX (← TX módulo) | ocupado |
| 21 | GPS TX | ocupado |

> ⚠️ **C3 fica 100% ocupado** com esse conjunto — e **HW-04 (saída NMEA 0183 p/ radar) NÃO cabe** (sem UART/pino sobrando). Conjunto completo + radar = voltar ao ESP32 DevKit clássico (escolha original, seção abaixo) quando houver placa funcional.

---

## Placa alternativa: ESP32 DevKit clássico (WROOM-32, 30 pinos)
Módulos GPS suportados: **ATGM336H** (AT6558, NMEA, GPS+BeiDou) e **NEO-6M** (u-blox, NMEA).
Ambos falam NMEA a **9600 baud, 1 Hz** (default) — firmware é NMEA-agnóstico, sem comandos UBX/CASIC obrigatórios.

---

## GPS (UART2)

| Pino módulo GPS | Liga a (ESP32 DevKit) | Notas |
| --- | --- | --- |
| VCC | **3V3** (ATGM336H) / **VIN 5V** (NEO-6M breakout) | NEO-6M breakout tem regulador próprio; ATGM336H é 3.3 V |
| GND | GND | massa comum |
| TX | **GPIO16** (RX2) | dados NMEA GPS → ESP32 |
| RX | **GPIO17** (TX2) | ESP32 → GPS (config opcional; pode ficar ligado) |
| PPS (ATGM336H) | — | não usado no v1 |

> Lógica é 3.3 V nos dois módulos — ligação direta, sem divisor.
> Antena do NEO-6M: conector u.FL/IPEX da antena cerâmica — vista livre do céu na bancada (perto de janela).

## Debug / flash

| Função | Pino | Notas |
| --- | --- | --- |
| USB serial (logs, flash) | UART0 — GPIO1/GPIO3 | via cabo USB da DevKit, 115200 |
| LED onboard | GPIO2 | status básico (fix/no-fix) — é strapping pin, só saída |

## Reservados — integração futura (HW-01, não montar agora)

| Função futura | Pinos reservados | Notas |
| --- | --- | --- |
| OLED I2C | **GPIO21** (SDA), **GPIO22** (SCL) | I2C padrão ESP32 |
| Botões | **GPIO32**, **GPIO33** | entrada c/ pull-up interno |
| Buzzer | **GPIO25** | DAC/PWM |
| LEDs status extra | **GPIO26**, **GPIO27** | |
| Saída NMEA 0183 (radar marítimo) | **GPIO4** (TX UART1 remapeado) | precisa driver RS-422/RS-232 externo — nunca ligar radar direto no ESP32 |
| Leitura bateria (Fase 5) | **GPIO34** (ADC1_CH6) | input-only; divisor resistivo definido na Fase 5 |

## Pinos a evitar

- **GPIO6–11** — flash interna, nunca usar
- **GPIO0, GPIO2, GPIO5, GPIO12, GPIO15** — strapping (boot); GPIO2 ok como LED de saída
- **GPIO34–39** — só entrada (ok para ADC/sinal)
- **GPIO1, GPIO3** — UART0/USB, reservados para log e flash

---

*Fase 1: montar só a seção GPS + USB. Resto é reserva documentada.*

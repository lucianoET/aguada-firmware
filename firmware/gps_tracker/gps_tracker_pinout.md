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

### Fase 01.1 — Barramento I2C (fiação implementada e validada em firmware)

4 módulos em paralelo no mesmo barramento — 4 fios compartilhados (3V3, GND, SDA, SCL). Fiação abaixo é a mesma usada pelos planos 01-04 desta fase (`i2c_bus`, `accel_sensor`, `mag_sensor`, `env_sensor`, `display`) — nenhuma mudança de pino em relação ao que foi montado.

| Barramento | Pino C3 SuperMini | Liga a (todos os módulos) |
| --- | --- | --- |
| SDA | **GPIO3** | SDA do OLED, MPU6050, HMC5883, HTU21 |
| SCL | **GPIO4** | SCL do OLED, MPU6050, HMC5883, HTU21 |
| VCC | **3V3** | VCC de todos (NÃO usar 5V — lógica I2C é 3.3 V) |
| GND | **GND** | GND de todos |

| Módulo | Endereço | Breakout típico | Notas |
| --- | --- | --- | --- |
| OLED SSD1306 128×64 | **0x3C** | 4 pinos I2C | alguns vêm 0x3D — scan no boot detecta |
| MPU6050 accel+gyro | **0x68** | GY-521 | deixar **AD0 solto/GND** (AD0 alto = 0x69); pinos INT/XDA/XCL não ligar |
| HMC5883L bússola | **0x1E** | GY-271 | ⚠️ muitos GY-271 vêm com **QMC5883L (0x0D)** — firmware detecta os dois; pino DRDY não ligar |
| HTU21D temp/umid | **0x40** | GY-21 | — |

> Pull-ups: os breakouts já têm pull-up onboard (4k7/10k). Com 4 módulos em paralelo o equivalente fica ~1–2 kΩ — ok a 3.3 V/400 kHz. Se o barramento falhar com todos ligados, remover resistores de pull-up de um ou dois módulos.
> Fios curtos (<20 cm) na bancada. GPS continua como na seção acima (GPIO20/21, VCC 5V no NEO-6M).
> OLED I2C **substitui** o TFT ST7735 SPI do orçamento v2 abaixo — GPIO1–5 ficam livres.

**Hardware efetivamente montado (preencher em bancada — ver `BENCH.md`, seção "Fase 01.1", registro de calibração de campo):**

| Item | Valor |
| --- | --- |
| Chip de bússola detectado (`[MAG] chip=...` no boot / `mag_chip=` no `[HEALTH]`) | _(preencher: HMC5883L ou QMC5883L)_ |
| Velocidade de barramento em uso (`DEFAULT_I2C_CLOCK_HZ`) | 400 kHz (default) — _(preencher se o fallback de 100 kHz foi necessário)_ |

### Orçamento de pinos v2 (periféricos planejados — HW-02..07, ainda não montar)


| GPIO | Função planejada | Notas |
| --- | --- | --- |
| 0 | ADC bateria | ADC1_CH0 + divisor (Fase 5) |
| 1 | TFT RST (não usado) | ST7735 1.8" SPI — **livre**: OLED I2C (Fase 01.1) substituiu o TFT SPI, GPIO1 disponível |
| 2 | TFT DC (não usado) | strapping — **livre** pelo mesmo motivo acima |
| 3 | I2C SDA | **MPU6050** (0x68) + **HTU21D** (0x40) no mesmo barramento |
| 4 | I2C SCL | ver nota de GPIO6/7 abaixo |
| 5 | TFT CS (não usado) | **livre** pelo mesmo motivo acima |
| 6 | ❌ **não usar** | preso em LOW nesta placa — trava o boot |
| 7 | ❌ **não usar** | preso em LOW nesta placa — trava o boot |
| 8 | LED onboard | TFT BL → 3V3 direto (sem dimmer) |
| 9 | botão (boot) | strapping — só entrada c/ pull-up |
| 10 | DS18B20 (HW-07, indefinido) | 1-Wire, pull-up 4k7 |
| 20 | GPS RX (← TX módulo) | ocupado |
| 21 | GPS TX | ocupado |

> ⚠️ **GPIO6/7 estão mortos nesta placa** (bancada 2026-08-14). A convenção do repo é SDA=6/SCL=7 (igual ao node AirQ) e o default em `gps_config.h` continua assim, mas o env `esp32-c3-supermini` sobrescreve para **3/4** via `build_flags`. Sintoma se voltar para 6/7: o boot para em `[I2C] scan sda=6 scl=7` e nunca chega ao `[HEALTH]` — `Wire.endTransmission()` bloqueia para sempre em 0x08 com o barramento preso em LOW, e `Wire.setTimeOut()` não segura (testado, o hang é abaixo da camada Wire). Numa placa nova, testar 6/7 antes de assumir 3/4.

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

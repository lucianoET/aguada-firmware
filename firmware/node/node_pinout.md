# Node Pinout — Ligações de Sensores

> Hardware actual: **ESP32-C3 SuperMini**

Cobre os três targets suportados e os quatro modos de operação:
`ULTRASONIC` (1 ou 2 sensores) · `AIR_QUALITY` (ANI-01) · `RELAY`

---

## ESP32-C3 SuperMini ✦ (actual)

### Diagrama de pinos

```
              ┌────[USB-C]────┐
         GND ─┤ GND     5V   ├─ 5V
         GND ─┤ GND     3V3  ├─ 3.3 V
       GPIO4 ─┤ G4      RST  ├─ RST
       GPIO5 ─┤ G5      G10  ├─ GPIO10
  SDA ►GPIO6 ─┤ G6       G9  ├─ GPIO9  ◄ BOOT / botão relay
  SCL ►GPIO7 ─┤ G7       G8  ├─ GPIO8  ◄ LED onboard / WS2812B
       GPIO0 ─┤ G0       G3  ├─ GPIO3
       GPIO1 ─┤ G1       G2  ├─ GPIO2
              └───────────────┘
```

> Pinos GPIO 20/21 existem mas não estão expostos no conector principal da maioria dos SuperMini.

---

### Modo ULTRASONIC — 1 sensor (reservatório)

| Sinal | GPIO | Pino HC-SR04 |
|-------|------|--------------|
| TRIG1 | **1** | TRIG |
| ECHO1 | **0** | ECHO |
| LED   | **8** | — (LED onboard, active-LOW) |
| GND   | GND  | GND |
| VCC   | 3V3  | VCC (ou 5V se o sensor exigir) |

```
ESP32-C3          HC-SR04
  G1 ──────────── TRIG
  G0 ──────────── ECHO
 3V3 ──────────── VCC
 GND ──────────── GND
```

---

### Modo ULTRASONIC — 2 sensores

| Sinal | GPIO | Sensor |
|-------|------|--------|
| TRIG1 | **1** | S1 — TRIG |
| ECHO1 | **0** | S1 — ECHO |
| TRIG2 | **3** | S2 — TRIG |
| ECHO2 | **2** | S2 — ECHO |
| LED   | **8** | — |

```
ESP32-C3          HC-SR04 nº1       HC-SR04 nº2
  G1 ─────────── TRIG
  G0 ─────────── ECHO
  G3 ──────────────────────────── TRIG
  G2 ──────────────────────────── ECHO
 3V3 ─────────── VCC ──────────── VCC
 GND ─────────── GND ──────────── GND
```

---

### Modo ANI-01 — qualidade do ar (ENS160 + AHT21)

| Sinal | GPIO | Destino |
|-------|------|---------|
| I2C SDA | **6** | ENS160 SDA · AHT21 SDA |
| I2C SCL | **7** | ENS160 SCL · AHT21 SCL |
| LED onboard | **8** | indicação de estado (active-LOW, onboard) |
| Botão BOOT | **9** | HELLO / restart (pull-up interno, onboard) |
| GND | GND | ENS160 GND · AHT21 GND |
| VCC | 3V3 | ENS160 VCC · AHT21 VCC |

**Endereços I2C:**

| Sensor | Endereço | Pino ADDR |
|--------|----------|-----------|
| ENS160 | 0x52 | ADDR → GND |
| AHT21  | 0x38 | fixo |

```
ESP32-C3          ENS160 / AHT21 (bus I2C partilhado)
  G6 ──────────── SDA
  G7 ──────────── SCL
 3V3 ──────────── VCC (ambos)
 GND ──────────── GND (ambos)
```

**Padrões do LED onboard (GPIO 8):**

| Padrão             | Significado                                  |
| ------------------ | -------------------------------------------- |
| 2 blinks × 100 ms  | Boot OK — sensores detectados                |
| 5 blinks × 100 ms  | Erro de init — sensor não encontrado no I2C  |
| 1 blink × 30 ms    | Leitura enviada com sucesso                  |
| 3 blinks × 200 ms  | Falha de leitura (sensor não respondeu)      |

**Botão BOOT (GPIO 9):**

| Pressão         | Acção                    |
| --------------- | ------------------------ |
| Curta (>40 ms)  | Envia PKT_HELLO imediato |
| Longa (≥5 s)    | Reinicia o node          |

**Sensor de luz TEMT6000 (opcional, modo ANI-01):**

Sensor analógico de luminosidade. Ligar a saída `SIG` a um pino **ADC1** (GPIO0–4).
O GPIO5 (ADC2) não funciona com WiFi ligado.

| Sinal | GPIO | Destino |
|-------|------|---------|
| SIG (OUT) | **3** (ADC1_CH3) | saída analógica |
| VCC | 3V3 | alimentação |
| GND | GND | massa |

```
ESP32-C3            TEMT6000
  3V3 ───────────── VCC
  GND ───────────── GND
   G3 ───────────── SIG
```

> Evitar GPIO2 (strapping). GPIO4 também é ADC1 mas é o pino recomendado para vbat.
> Pino configurável via build flag `DEFAULT_LIGHT_PIN`.

**Sensor de som KY-037 / KY-038 (opcional, modo ANI-01):**

Microfone electret com saída analógica (A0). Ligar a um pino **ADC1** diferente do da luz.
**Alimentar a 3.3 V** (não 5 V) para os picos do A0 ficarem dentro do ADC.

| Sinal | GPIO | Destino |
|-------|------|---------|
| A0 (analógico) | **1** (ADC1_CH1) | saída de áudio |
| VCC | 3V3 | ⚠ 3.3 V |
| GND | GND | massa |
| D0 | — | não usado |

```
ESP32-C3            KY-037 / KY-038
  3V3 ───────────── + / VCC
  GND ───────────── G / GND
   G1 ───────────── A0
```

> Nível sonoro **relativo** (não dB SPL calibrado). Pino via `DEFAULT_SOUND_PIN`,
> calibração via `SOUND_DB_OFFSET`.

---

### Modo RELAY

| Sinal | GPIO | Detalhe |
|-------|------|---------|
| Botão HELLO | **9** | BOOT button, active-LOW |
| I2C SDA | **6** | Opcional: SHT3x (0x44) ou HD21D (0x40) para T/H |
| I2C SCL | **7** | idem |
| LED | **8** | Pulso 30 ms a cada 2 s (se sem conflito) |

O relay faz scan I2C em 0x40 e 0x44. Se encontrar SHT3x ou HD21D, envia T/H como `PKT_HEARTBEAT sensor_id=0xFE`.

---

### vbat — monitorização de tensão (opcional, todos os modos)

| Sinal | GPIO | Detalhe |
|-------|------|---------|
| ADC | **4** (recomendado) | Divisor de tensão 1:2 |

```
Bateria/fonte ── R1(100k) ─┬─ GPIO4
                           │
                         R2(100k)
                           │
                          GND
```

Configurar em NVS: `vbat_enabled=true`, `vbat_pin=4`, `vbat_div=2`.

---

## ESP32-C3 DevKit M1

Mesmo target (`CONFIG_IDF_TARGET_ESP32C3`) → **pinos idênticos ao SuperMini**.  
O layout físico do conector é diferente mas os GPIOs são os mesmos.

---

## ESP32 DevKit (ESP32 clássico)

### Modo ULTRASONIC — 1 sensor

| Sinal | GPIO | Sensor |
|-------|------|--------|
| TRIG1 | **5**  | HC-SR04 TRIG |
| ECHO1 | **18** | HC-SR04 ECHO |
| LED   | **2**  | active-HIGH |

### Modo ULTRASONIC — 2 sensores (pinos recomendados)

> Os pinos padrão (ECHO2=GPIO21) colidem com I2C SDA. Usar os pinos dos envs `ch11`:

| Sinal | GPIO (ch11) | GPIO (padrão — evitar se usar I2C) |
|-------|-------------|-------------------------------------|
| TRIG1 | **25** | 5  |
| ECHO1 | **26** | 18 |
| TRIG2 | **32** | 19 |
| ECHO2 | **33** | ⚠ 21 = SDA |

### Modo ANI-01

| Sinal | GPIO | Destino |
|-------|------|---------|
| I2C SDA | **21** | ENS160 SDA · AHT21 SDA |
| I2C SCL | **22** | ENS160 SCL · AHT21 SCL |
| LED | **2** | monochrome (WS2812B não testado no DevKit) |

### Modo RELAY

| Sinal | GPIO | Detalhe |
|-------|------|---------|
| Botão | **2** | active-HIGH ⚠ mesmo pino que LED — `relay_led_conflict()` desativa LED |
| I2C SDA | **21** | Opcional: SHT3x/HD21D |
| I2C SCL | **22** | idem |

### vbat (DevKit)

Recomendado: GPIO **34** (input-only, sem conflito). `vbat_div=2`.

---

## Personalizar pinos via build flags

Qualquer pino pode ser sobrescrito adicionando flags em `platformio.ini`:

| Flag | Padrão C3 | Padrão DevKit | Descrição |
|------|-----------|---------------|-----------|
| `DEFAULT_TRIG1` | 1 | 5 | HC-SR04 sensor 1 TRIG |
| `DEFAULT_ECHO1` | 0 | 18 | HC-SR04 sensor 1 ECHO |
| `DEFAULT_TRIG2` | 3 | 19 | HC-SR04 sensor 2 TRIG |
| `DEFAULT_ECHO2` | 2 | 21 | HC-SR04 sensor 2 ECHO |
| `DEFAULT_I2C_SDA_PIN` | 6 | 21 | I2C SDA |
| `DEFAULT_I2C_SCL_PIN` | 7 | 22 | I2C SCL |
| `DEFAULT_LED_PIN` | 8 | 2 | LED / WS2812B DIN |
| `DEFAULT_LIGHT_PIN` | 3 | — | TEMT6000 SIG (ADC1, modo ANI-01) |
| `DEFAULT_SOUND_PIN` | 1 | — | KY-037/038 A0 (ADC1, modo ANI-01) |
| `DEFAULT_SENSOR_TYPE` | 0 | 0 | 0=ultrassônico 1=ANI-01 |
| `DEFAULT_NUM_SENSORS` | 1 | 1 | 0=relay 1=1sensor 2=2sensores |
| `DEFAULT_RELAY_ENABLED` | 0 | 0 | 1=relay+sensor simultâneos |

Exemplo — env ANI-01 com pino SDA customizado:
```ini
build_flags = -DDEFAULT_SENSOR_TYPE=1 -DDEFAULT_I2C_SDA_PIN=10
```

# Yeti — boneco de mesa (perfis `yeti_s3` e `yeti_c3` do communicator)

Yeti v1.2 impresso com rosto num ST7789 1.3" 240×240, microfone, falante, touch no topo da
cabeça e sensores de clima, movimento e luz. **É um communicator:** usa os mesmos
`audio_io`, `voice_net`, `voice_dsp` e `alarm`. Fala por PTT, toca os anúncios dos outros
nodes e reage aos alarmes do Aguada. A moldura do display é o `frame_st7789_1p3.stl`,
gerada por `frame_st7789_1p3.py` em `cad/yeti/`. Na mesma pasta: `yeti_edit.py`
(corpo com furos dos mics e marcas de toque nos braços → `yeti_corpo_v2.stl`),
`yeti_base_energia.py` (base de bateria) e as skins `yeti_skin_cmasm.py` (chapéu) e
`yeti_skin_bibico.py` (bibico).

**CAD:** [cad/yeti/](cad/yeti/README.md) · **Manuais:** montagem [S3](docs/MONTAGEM_YETI_S3.md) / [C3](docs/MONTAGEM_YETI_C3.md) · [usuário](docs/MANUAL_USUARIO_YETI.md) ·
diagramas [S3](docs/yeti_s3_ligacoes.svg) / [C3](docs/yeti_c3_ligacoes.svg) (gerados por
`docs/wiring_src/yeti.py`).

## Pinagem — ESP32-S3 SuperMini

Todos os 15 GPIO do SuperMini estão em uso (G1–G13, G43, G44); o G48 é o LED RGB da placa.

| GPIO | Função | Ligação |
|------|--------|---------|
| **G1** | ADC bateria | divisor 100k/100k do B+ (depois da chave) → G1, 100 nF no G1 |
| **G2** | Backlight PWM | display **BLK** |
| **G3** | Touch nativo (TOUCH3) | fita de cobre dentro do braço **esquerdo** |
| **G4** | I2S BCLK | MAX98357A **BCLK** + 2× INMP441 **SCK** |
| **G5** | I2S WS/LRC | MAX98357A **LRC** + 2× INMP441 **WS** |
| **G6** | I2S DOUT | MAX98357A **DIN** |
| **G7** | I2S DIN | 2× INMP441 **SD** (mesmo fio) |
| **G8 / G9** | I2C SDA / SCL | HTU21D 0x40 · MPU6050 0x68 · BH1750 0x23 (3V3) |
| **G10** | DC | display **DC** |
| **G11** | SPI MOSI | display **SDA** |
| **G12** | SPI SCK | display **SCL** |
| **G13** | Touch nativo (TOUCH13) | fita de cobre dentro do braço **direito** |
| **G43** | WS2812 DIN | 2 LEDs das bochechas em cadeia, 330 Ω em série |
| **G44** | Touch da cabeça | TTP223 **I/O** (saída ativa alta) |
| — | RST do display | **RES → 3V3 por 10k + 100 nF ao GND** (reset por RC + reset por software) |

Mic e falante dividem BCLK/WS (I2S full-duplex, igual ao C3). Os dois mics usam o mesmo SD:
o **esquerdo** com **L/R → GND** (canal L), o **direito** com **L/R → 3V3** (canal R).
Todos os pinos podem ser sobrescritos por `-D` (`include/yeti_config.h` e `COMM_PIN_I2S_*`
no `platformio.ini`).

Observações:
- **G3** é strapping (fonte do JTAG), mas só pesa com o eFuse queimado; a fita de cobre é
  alta impedância e não força nível no boot.
- **G43** é o TX da UART0: o ROM solta o log de boot ali, então os LEDs podem piscar
  aleatório no boot até o firmware assumir. O console é o USB nativo (CDC), não a UART0.
- **G44** (RX da UART0) é entrada no boot, seguro para a saída do TTP223.
- Touch do braço: fita de ~12 × 15 mm colada por dentro, atrás da covinha do braço
  (parede de 1,5–2 mm). Fio curto, longe do flex do display.

### Alimentação

| Módulo | VCC | Obs. |
|--------|-----|------|
| ST7789 | **3V3** | módulo de 7 pinos, sem CS (SPI modo 3) |
| MAX98357A | **5V** | SD_MODE → 3V3 (ou solto); GAIN solto = 9 dB; falante 4–8 Ω |
| INMP441 ×2 | 3V3 | esquerdo **L/R → GND**, direito **L/R → 3V3** |
| TTP223 | 3V3 | jumper A aberto (saída alta no toque); fechou A → `-DYETI_TOUCH_ACTIVE=LOW` |
| HTU21D, MPU6050, BH1750 | 3V3 | um barramento I2C só; os módulos já trazem pull-up |
| WS2812 ×2 | 5V | 100 µF no 5V perto dos LEDs; dado em 3V3 funciona com fio curto |

**Energia** (base sob a peana, `yeti_base_energia.stl`): jack DC e conector magnético (5 V) →
**IN+/IN−** do TP4056 → bateria; **OUT+** → chave → pino **5V** do SuperMini e **VIN** do
MAX98357A. LiPo 1S vai direto no 5V (o LDO da placa segura até ~3,5 V). Com 2×AA/3×AAA,
um MT3608 ajustado em 5 V entre a chave e o 5V. Os fios sobem pelos furos da peana e pelas
pernas ocas do Yeti.

**Posição dos sensores:** o HTU21D fica longe do ESP e do backlight, numa abertura na base,
porque perto deles lê de 2 a 4 °C a mais. O BH1750 precisa de um furo de ~3 mm na face. O
MPU6050 vai preso firme ao corpo.

## Interação

| Gesto | Ação |
|-------|------|
| 1 toque na cabeça | carinho: olhos ^ ^, sorriso e piado |
| 2 toques | tela com temperatura, umidade e luz (4 s) |
| 3 toques | volume 25 → 50 → 75 → 100 % |
| segurar ≥ 0,5 s | **PTT**: gongo e voz para todos os communicators; soltar encerra |
| inclinar o boneco | os olhos seguem a inclinação |
| chacoalhar | olhos de espiral (tonto) por 3 s |
| batida no corpo | acorda se estiver dormindo |
| toque no braço **direito** | **sim**: toast, bochechas verdes, bipe |
| toque no braço **esquerdo** | **não**: toast, bochechas vermelhas, bipe |
| segurar um braço ≥ 0,8 s | **gatilho** E/D (toast, bochechas âmbar) |
| os dois braços juntos | **abraço**: olhos felizes, piado, bochechas rosa |
| som alto de um lado (2 mics) | os olhos viram para o lado do som por 1,5 s |

Esquerdo/direito = de quem olha o Yeti de frente. Cada gesto dos braços sai na serial como
`YETI_EVT braco=sim|nao|gatilho_E|gatilho_D|abraco`. Com 2 toques na cabeça, o rodapé mostra a
bateria em %; abaixo de 3,45 V aparece "bateria fraca" (a cada 5 min).

**Bochechas (WS2812):** cor pelo humor (rosado calmo, verde no ar, azul ouvindo, vermelho
piscando no alarme, apagadas dormindo), com brilho seguindo o backlight e teto em
`YETI_LED_MAX`.

**Calibração dos braços:** no boot o log mostra `braços: base E=… D=…`. Se o toque não
pegar, baixe `YETI_ARM_ON` (padrão 1,04 = +4 %); se disparar sozinho, suba. Não toque nos
braços durante o boot (é quando a linha de base é medida).

## Humores (por prioridade)

`ALARM` (vermelho piscando, sirene no falante) → `TALK` (no ar; a boca segue o mic) →
`LISTEN` (anúncio de outro node) → `DIZZY` → `HAPPY` → `SLEEPY` (mais de 30 s abaixo de
3 lx; backlight no mínimo) → `HOT` (≥ 30 °C) / `COLD` (≤ 18 °C) → `IDLE` (olha em volta e pisca).

O backlight acompanha a luz ambiente, de 20 a 255 em escala log. Fica no máximo no alarme e
no PTT.

## Build

```bash
cd firmware/communicator
~/.platformio/penv/bin/pio run -e yeti_s3 -t upload
```

## Bancada

- [ ] Display: aparece "iniciando..." e depois os olhos. Imagem deslocada ou espelhada →
      `-DYETI_TFT_ROTATION=0..3`. Cores trocadas → `invert` / `rgb_order` em `face.cpp`.
- [ ] O serial mostra `[i2c] scan: 0x23 0x40 0x68` e `HTU21D ok, IMU ok, BH1750 ok`.
- [ ] Inclinar para a direita leva o olhar para a direita. Se inverter, trocar o sinal em
      `mood_tick()` (`-v.ay` / `v.ax`), conforme a orientação em que o MPU foi montado.
- [ ] 1, 2 e 3 toques e o hold → PTT. Com outro communicator ligado, a voz chega nele.
- [ ] Mão sobre o BH1750 por 30 s → dorme; um toque acorda.

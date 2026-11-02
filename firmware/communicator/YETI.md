# Yeti — boneco de mesa (perfil `yeti_s3` do communicator)

Yeti v1.2 impresso com rosto num ST7789 1.3" 240×240, microfone, falante, touch no topo da
cabeça e sensores de clima, movimento e luz. **É um communicator:** usa os mesmos
`audio_io`, `voice_net`, `voice_dsp` e `alarm`. Fala por PTT, toca os anúncios dos outros
nodes e reage aos alarmes do Aguada. A moldura do display é o `frame_st7789_1p3.stl`,
gerada por `frame_st7789_1p3.py` na pasta dos STLs do Yeti.

## Pinagem — ESP32-S3 SuperMini

| GPIO | Função | Ligação |
|------|--------|---------|
| **G4** | I2S BCLK | MAX98357A **BCLK** + INMP441 **SCK** |
| **G5** | I2S WS/LRC | MAX98357A **LRC** + INMP441 **WS** |
| **G6** | I2S DOUT | MAX98357A **DIN** |
| **G7** | I2S DIN | INMP441 **SD** |
| **G8 / G9** | I2C SDA / SCL | HTU21D 0x40 · MPU6050 0x68 · BH1750 0x23 (3V3) |
| **G12** | SPI SCK | display **SCL** |
| **G11** | SPI MOSI | display **SDA** |
| **G10** | DC | display **DC** |
| **G13** | RST | display **RES** |
| **G2** | Backlight PWM | display **BLK** |
| **G1** | Touch | TTP223 **I/O** (saída ativa alta) |
| G3 | não usar | strapping |
| G43 / G44 | livres | TX/RX da UART0 |

Mic e falante dividem BCLK/WS (I2S full-duplex, igual ao C3). Todos os pinos podem ser
sobrescritos por `-D` (`include/yeti_config.h` e `COMM_PIN_I2S_*` no `platformio.ini`).

### Alimentação

| Módulo | VCC | Obs. |
|--------|-----|------|
| ST7789 | **3V3** | módulo de 7 pinos, sem CS (SPI modo 3) |
| MAX98357A | **5V** | SD_MODE → 3V3 (ou solto); GAIN solto = 9 dB; falante 4–8 Ω |
| INMP441 | 3V3 | **L/R → GND** |
| TTP223 | 3V3 | jumper A aberto (saída alta no toque); fechou A → `-DYETI_TOUCH_ACTIVE=LOW` |
| HTU21D, MPU6050, BH1750 | 3V3 | um barramento I2C só; os módulos já trazem pull-up |

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

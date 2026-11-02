# Node Communicator — voz, anúncios e alarme

Node de comunicação do Aguada: microfone para voz (anúncios e discursos), alto-falante ou
saída de linha para os amplificadores, OLED, botões de alarme e sirene. Um firmware para
todas as montagens; cada periférico é opcional e detectado no boot.

Base reaproveitada: `DEV_luctronics/luctronics_firmware/fonoclama/slave_wifi` (I2S de saída,
OLED, MQTT/HA, captive portal) e o esboço `xFonoclama/esp32_warning_system.tsx`.

## Requisitos (confirmados em 2026-11-02)

1. Hardware modular, um firmware: mic INMP441, alto-falante MAX98357A (ou linha PCM5102A),
   OLED SSD1306, botões, sirene/relé — tudo opcional.
2. Perfis: `communicator_c3` (C3 SuperMini: mic + falante/linha + OLED + botões + sirene) e
   `communicator_audio` (ESP32-S3 + codec ES8388: entrada **e** saída de linha).
3. Voz por WiFi (UDP multicast); alarme/status/comando por ESP-NOW (protocolo v3).
4. Anúncio PTT: segura o botão → voz para todos os communicators (ou uma zona).
5. Discurso local: mic → saída de linha → amplificador do Fonoclama (70 V), latência mínima.
6. Entrada de linha (mesa, celular) retransmitida como anúncio.
7. Botões de alarme: sirene local + alarme ao Aguada/HA + sirene dos outros nodes.
8. Sirene 12 V por relé/MOSFET + tons de alerta no falante.
9. Integração HA via MQTT Discovery (botões, sirene, "no ar", volume, texto/TTS).

## Fases

| Fase | Escopo | Estado |
|------|--------|--------|
| **1** | C3: I2S full-duplex (mic + falante/linha), PTT por UDP multicast com gongo, AGC/gate, botões, sirene, OLED, alarme por ESP-NOW + multicast | código escrito, compila — **falta bancada** |
| 2 | WiFi/MQTT por captive portal (do slave_wifi), HA Discovery, texto no OLED, bridge.py entende `0xE0` | — |
| 3 | `communicator_audio`: S3 + ES8388, entrada de linha, discurso local mic → linha | — |
| 4 | Opus/ADPCM se o multicast pesar na rede; prioridade/zonas pelo HA | — |

## Pinagem — `communicator_c3` (ESP32-C3 SuperMini)

| GPIO | Função | Ligação |
|------|--------|---------|
| **G4** | I2S BCLK | MAX98357A/PCM5102A **BCLK** + INMP441 **SCK** |
| **G5** | I2S WS/LRC | MAX98357A/PCM5102A **LRC** + INMP441 **WS** |
| **G3** | I2S DOUT | MAX98357A **DIN** ou PCM5102A **DIN** |
| **G10** | I2S DIN | INMP441 **SD** (pull-down interno: sem mic lê zeros → detectado) |
| **G6 / G7** | I2C SDA / SCL | OLED SSD1306 0x3C (3V3) |
| **G1** | Botão **PTT** | botão ao GND (pull-up interno) |
| **G0** | Botão **ALARME** | botão ao GND; segurar 1 s dispara, 3 s com alarme ativo cancela |
| **G9** | Botão BOOT da placa | toque = volume (25→50→75→100 %) |
| **G20** | Sirene | gate de MOSFET logic-level (IRLZ44N/AO3400) ou módulo relé 3,3 V; 10k gate→GND |
| G8 | LED da placa | aceso = no ar |
| G2, G21 | livres | G2 é strapping (alto no boot); G21 pulsa no boot (TX da ROM) |

Mic e saída dividem o mesmo I2S (mesmos BCLK/WS, um fio de dado para cada lado): o C3 só tem
um controlador I2S, e o full-duplex dá 16 kHz, slots de 32 bit (BCLK 1,024 MHz = 64·fs, o
que o INMP441 exige).

- **INMP441:** VDD 3V3, GND, **L/R → GND** (canal esquerdo). Fios curtos (< 20 cm).
- **MAX98357A (falante):** VIN 5 V, **SD_MODE → 3V3**, GAIN solto (9 dB), falante 4–8 Ω ≤ 3 W.
- **PCM5102A (linha → amplificador):** VIN 5 V (ou 3V3), **SCK → GND** (PLL interno),
  FMT/FLT/DEMP → GND, **XMT → 3V3** (sem isso fica mudo). Saída 2,1 Vrms no jack → AUX do
  amplificador. Mesmo firmware: o DAC entra no lugar do MAX98357A.
- **Sirene 12 V:** MOSFET low-side no negativo da sirene + diodo 1N4007 em antiparalelo se
  for indutiva. Desliga sozinha depois de `COMM_SIREN_MAX_S` (180 s), o alarme continua no OLED.

## Voz — UDP multicast

`239.255.42.99:5004`, um pacote a cada 20 ms: cabeçalho de 10 bytes + 320 amostras µ-law
(G.711) a 16 kHz = 128 kbit/s por locutor. WiFi com power-save desligado.

```c
struct { char magic[2] = "AV"; uint8_t ver = 1; uint8_t zone;   // 0 = todas
         uint16_t node_id; uint16_t seq; uint8_t flags; uint8_t prio; }  // + 320 B µ-law
// flags: bit0 = fim do PTT
```

- Transmissor: gongo de 2 tons (~0,5 s) antes da voz; passa-alta 100 Hz → noise gate → AGC
  (até +40 dB, alvo −6 dBFS) → limitador. Falante local mudo durante o PTT (sem microfonia).
- Receptor: trava no primeiro locutor; prioridade maior toma o canal; solta 600 ms depois do
  último pacote. Buffer de 80 ms antes de tocar; falta de pacote = silêncio, rebufferiza.
- **Multicast no WiFi** vai na taxa básica do AP: 128 kbit/s ≈ 10–15 % do ar a 1 Mbit/s.
  Num AP com IGMP snooping, liberar o grupo 239.255.42.99.

## Alarme

Dois caminhos, deduplicados por (node_id, seq):

- **ESP-NOW (Aguada v3):** `PKT_HEARTBEAT`, `sensor_id = SENSOR_ID_ALARM (0xE0)`,
  `distance_cm` = código (`0` limpa, `1` geral/pânico, `2` incêndio, `3` teste),
  `reserved` = zona. O gateway repassa sem reflash (como os canais 0xF0–0xF6 do AirQ).
  Repetido 3× a 200 ms (broadcast, sem ACK).
- **Multicast** (`magic "AC"`) para communicators só em WiFi.

**Canal:** com WiFi conectado o ESP-NOW usa o canal do AP. Para falar com o gateway Aguada
o AP precisa estar no mesmo canal (`ESPNOW_CHANNEL`, 11 no CMASM). Sem WiFi configurado o
node fica no `ESPNOW_CHANNEL` e só o alarme funciona.

## Build

```bash
cd firmware/communicator
cp secrets.ini.example secrets.ini   # SSID/senha do WiFi
~/.platformio/penv/bin/pio run -e communicator_c3 -t upload
```

Build flags: `COMM_NAME`, `COMM_ZONE` (0 = todas), `COMM_PRIO` (0–3), `COMM_PTT_CHIME`,
pinos `COMM_PIN_*` em `include/comm_config.h`.

## Bancada (fase 1)

- [ ] Só falante: boot toca o bip de teste; BOOT muda o volume.
- [ ] Mic: serial mostra `mic OK`; sem mic → `mic ausente` e o PTT avisa no OLED.
- [ ] 2 nodes: PTT num → gongo + voz no outro; latência < 200 ms.
- [ ] PCM5102A no lugar do MAX98357A → linha no amplificador, sem chiado.
- [ ] Alarme: G0 1 s → sirene local + sirene no outro node + `0xE0` no serial do gateway.

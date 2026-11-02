# Manual de montagem — Nó reservatório Aguada 2U (2 reservatórios lado a lado)

ESP32-C3 SuperMini + **dois** sensores ultrassônicos (um por reservatório) + fonte 5 V DC.
Firmware: env **`node_reservatorio_2U`** (ESP-NOW canal 11). Mesmo nó do [1U](MONTAGEM_NODE_RESERVATORIO_1U.md) com um 2º ECHO;
os dois sensores dividem o TRIG. O nó envia `sensor_id` 1 e 2 com o mesmo `node_id`. Tempo de bancada: ~1 h.

![Ligações](node_reservatorio_2U_ligacoes.svg)

> Diagrama gerado por `wiring_src/node_reservatorio.py ... 2U`. A placa está vista por cima
> (componentes para cima, USB-C no topo). **Confira a serigrafia** da sua SuperMini antes de soldar.

---

## 1. Material

| Qtd | Item | Observação |
|----:|------|------------|
| 1 | ESP32-C3 SuperMini | |
| 2 | Sensor ultrassônico | **JSN-SR04T / AJ-SR04M** (à prova d'água, mede a partir de ~25 cm) para campo; HC-SR04 só em bancada |
| 1 | Fonte 5 V DC, ≥ 1 A | |
| 2 | R1, R6 1 kΩ | divisores do ECHO1 e ECHO2 |
| 2 | R2, R7 2 kΩ | ou 2× 1 kΩ em série cada |
| 1 | R3 200 kΩ | monitor da fonte (ou 2× 100 kΩ em série) |
| 1 | R4 100 kΩ | monitor da fonte |
| 1 | C1 470 µF / 10 V eletrolítico | 5V–GND, junto ao C3 |
| 1 | C2 100 nF cerâmico | 5V–GND, junto ao C3 |
| 1 | C3 100 nF cerâmico | G1–GND (estabiliza a leitura) |
| 2 | C4, C5 100 µF / 10 V | 5V–GND, junto a cada sensor |
| 1 | *(opcional)* sensor de temperatura: **HTU21D/SHT3x** (I2C) **ou** DS18B20 à prova d'água + R5 4,7 kΩ | compensação da velocidade do som + temperatura no painel; o nó detecta sozinho no boot |
| 1 | Placa perfurada, bornes, caixa IP65, prensa-cabo | |

---

## 2. Pinagem

| C3 | Sinal | Vai para | Cor sugerida |
|----|-------|----------|--------------|
| **5V** | entrada 5 V | Fonte +, VCC dos 2 sensores, topo do R3, C1/C2 | vermelho |
| **GND** | terra | Fonte −, GND dos 2 sensores, R2, R4, R7, C1–C5, DS18B20 preto | preto |
| **3V3** | saída 3,3 V | DS18B20 VDD + R5 (só isso) | laranja |
| **G3** | TRIG | TRIG do sensor 1 **e** do sensor 2 (um fio que se divide) | azul |
| **G4** | ECHO1 | nó R1/R2 do sensor 1 (**nunca** o ECHO direto) | verde |
| **G20** | ECHO2 | nó R6/R7 do sensor 2 (**nunca** o ECHO direto) | verde-claro |
| **G1** | monitor 5 V | nó R3/R4 + C3 | marrom |
| **G10** | 1-Wire | DS18B20 DQ (amarelo) + R5 para 3V3 | ciano |
| **G6 / G7** | I2C SDA / SCL | HTU21D/SHT3x (opcional; VCC no 3V3) — o OLED futuro divide o mesmo barramento | roxo / cinza |
| G8 | — | **não usar** (pino de boot; LED da placa) | |
| G0, G2, G5–G7, G9, G21 | — | **reservados** (tabela abaixo) | |

**Pinos reservados para expansão** (ainda sem firmware — não usar para outra coisa):

| C3 | Reserva | Cuidado |
|----|---------|---------|
| **G0 / G2 / G21** | Microfone **INMP441** (I2S): G0 → SCK, G2 → WS, G21 ← SD; VDD 3V3, GND, L/R → GND | detectar água entrando pelo ruído. G2 é pino de boot: só o WS do mic nele. Fios curtos (< 20 cm) |
| **G6 / G7** | OLED SSD1306 (0x3C) na mesma I2C do sensor de temperatura | pull-up 4k7 se nenhum módulo tiver |
| **G5** | Saída: relé | pino mais seguro para saída. 2º relé: expansor I2C (PCF8574) na G6/G7 |
| **G9** | Entrada: botão PB1 (é o botão BOOT da placa) | |

Relé: módulo acionado por 3,3 V (transistor/optoacoplador), bobina no 5 V.

**TRIG compartilhado:** os dois sensores disparam juntos, e cada leitura usa só o seu ECHO;
o firmware lê um sensor de cada vez, com 60 ms de folga entre eles. Funciona porque os sensores
ficam em reservatórios separados — **não** use em dois sensores apontando para a mesma água.

**Por que o divisor no ECHO:** o sensor alimentado em 5 V devolve um pulso de 5 V, e o C3 só
aguenta 3,6 V nas entradas. Sem o divisor o pino queima — já aconteceu no nó CON (G0).

**Divisores:**

```
sensor 1 ECHO ─ R1 1k ─┬── G4          5V ── R3 200k ──┬──┬── G1
                       │                               │  │
                     R2 2k                          R4 100k  C3 100nF
                       │                               │  │
                      GND                             GND GND
     5 V × 2/3 = 3,3 V em G4               5 V ÷ 3 = 1,67 V em G1

sensor 2 ECHO ─ R6 1k ─┬── G20         TRIG: G3 ──┬── sensor 1 TRIG
                     R7 2k                        └── sensor 2 TRIG
                      GND
```

---

## 3. Montagem

1. **Fonte:** meça a saída sem carga: **5,0 a 5,3 V**.
2. **Placa perfurada:** solde barra de pinos fêmea para o C3 (ele fica removível) e bornes para
   fonte, os 2 sensores e a sonda.
3. **Barramentos:** trilha de 5V e trilha de GND. Solde C1 + C2 encostados nos pinos 5V/GND do C3
   (observe a polaridade do C1) e C4/C5 junto ao borne de cada sensor.
4. **Divisores do ECHO:** sensor 1 — R1 entre o borne ECHO1 e o G4, R2 entre G4 e GND.
   Sensor 2 — R6 entre o borne ECHO2 e o G20, R7 entre G20 e GND.
5. **Monitor da fonte:** R3 entre 5V e G1; R4 e C3 entre G1 e GND.
6. **TRIG:** do G3 até os bornes TRIG dos dois sensores (um fio que se divide).
7. **Temperatura (opcional, um dos dois):**
   - **HTU21D/SHT3x (I2C):** VCC no 3V3, GND, SDA no G6, SCL no G7. Tem prioridade se os dois existirem.
   - **DS18B20:** vermelho no 3V3, preto no GND, amarelo no G10; R5 4k7 entre G10 e 3V3.
   - Sem nenhum, o nó usa 25 °C fixos e não manda temperatura ao servidor.
8. **Conferência com o C3 fora do soquete e multímetro:**
   - 5V–GND: **sem curto**.
   - G4–GND e G20–GND: ~2 kΩ cada. G1–GND: ~67 kΩ (100k ∥ 200k).
   - Nenhum fio dos sensores encostando no 3V3.
9. **Primeira energização ainda sem o C3** (só a fonte e os sensores):
   - soquete 5V: **5,0 V** · G1: **~1,67 V** · G4 e G20: **0 V** em repouso.
   - Se G4, G20 ou G1 passarem de **3,3 V**, pare e revise os divisores.
10. Desligue a fonte e encaixe o C3.

> **Buck de 3,3 V:** com a fonte de 5 V ele é dispensável, porque o C3 já regula pelo pino 5V.
> Se usar mesmo assim: saída do buck no **3V3** do C3, sensor continua no **5V**, e **desligue a
> fonte antes de plugar o USB**.

---

## 4. Gravação

Com o C3 no USB do computador (fonte desligada). Apaga a flash inteira para o nó não usar
pinos antigos salvos no NVS:

```bash
cd ~/Dev/aguada-firmware/firmware/node
~/.platformio/penv/bin/pio run -e node_reservatorio_2U -t erase
~/.platformio/penv/bin/pio run -e node_reservatorio_2U -t upload
~/.platformio/penv/bin/pio device monitor -b 115200
```

Anote o MAC impresso no boot: os **2 últimos bytes** são o `node_id` (ex.: `…:AB:D6` → `0XABD6`).

---

## 5. Teste de bancada

Cada sensor apontado para uma parede diferente (ex.: ~1 m e ~2 m, para distinguir), gateway ligado no servidor.

**No monitor serial**, em até 1 min:

| Linha | Significa |
|-------|-----------|
| `Mode: SENSOR (num_sensors=2)` | modo 2 reservatórios |
| `Sensor 1: TRIG=3 ECHO=4` / `Sensor 2: TRIG=3 ECHO=20` | pinos certos (outros valores = NVS não foi apagado) |
| `gateway XX:XX:… → unicast` | gateway respondeu ao HELLO; envio com confirmação |
| `S1 dist: 100 cm` / `S2 dist: 200 cm` | as duas leituras chegando (variam ±1–2 cm) |
| `WDT 60s` | watchdog ativo |
| `temperatura I2C …: HTU21D/HD21D 0x40` ou `sonda EXT detectada` | sensor de temperatura achado (sem sensor: `nenhum` / `sem sonda`) |

**LED da placa:** 2 piscadas no boot e 1 piscada curta a cada envio.

**Na ponta do servidor:** o nó aparece em http://192.168.0.26:8090 (aguada-web do lucmac). O pacote leva `vbat` ≈ 50 (5,0 V) e o RSSI.

**Cadastro dos dois reservatórios** (mesmo `node_id`, `sensor_id` 1 e 2) em `tools/reservoirs.yaml` e depois rodar `tools/sync_reservoirs.sh`:

```yaml
  "0XABD6":
    - sensor_id: 1
      alias: NOVO
      name: Nome do reservatório
      level_max_cm: 200        # altura útil da água
      volume_max_L: 20000
      sensor_offset_cm: 30     # do sensor até o nível máximo (ver item 6)
    - sensor_id: 2
      alias: NOVO2
      name: Nome do 2º reservatório
      level_max_cm: 200
      volume_max_L: 20000
      sensor_offset_cm: 30
```

---

## 6. Instalação no reservatório

- **Altura:** cada sensor fica **≥ 25 cm acima do nível máximo** (zona cega do JSN-SR04T). Essa
  distância é o `sensor_offset_cm`: meça com trena, do sensor até a marca de transbordo.
- **Posição:** perpendicular à água, longe de paredes, escadas, boia e da entrada d'água (a queda
  d'água gera eco falso).
- **Eletrônica fora da caixa d'água:** só os transdutores entram. Uma caixa IP65 entre os dois
  reservatórios (cabo do JSN-SR04T até ~2,5 m; não emende o cabo do transdutor). O C3 vai com
  prensa-cabo, com laço de gotejamento no cabo, de preferência na sombra.
- **Antena:** a ponta da placa oposta ao USB voltada para o gateway, sem metal na frente. Antes
  de fechar a caixa, confira o RSSI: melhor que **−85 dBm**.
- **Temperatura (opcional):** um sensor só, no ar de um dos reservatórios (vale para os dois).
  Onde a sonda não cabe (ex.: CIE), um HTU21D dentro da caixa do nó já ajuda, protegido do sol.
  Ao ligar a sonda, a escala muda ~2%, então recalibre o `sensor_offset_cm`.

---

## 7. Diagnóstico

| Sintoma | Causa provável | Ação |
|---------|----------------|------|
| `S1 dist: ERR` sempre | ECHO1 sem sinal: divisor aberto, sensor sem 5V ou TRIG/ECHO trocados | medir 5V no sensor; conferir G3=TRIG e G4=ECHO1 |
| `S2 dist: ERR` sempre | idem no sensor 2 | conferir o ramo do TRIG até o sensor 2 e G20=ECHO2 |
| S1 e S2 dão a mesma leitura | ECHOs trocados ou em curto, ou sensores na mesma água | medir G4–G20 (não pode ser 0 Ω) |
| Leitura fixa ~20–25 cm | objeto ou parede na frente, ou dentro da zona cega | reposicionar; respeitar 25 cm |
| Leitura pula muito | eco na parede ou na entrada d'água | afastar ou mirar o sensor |
| `num_sensors=1` ou pinos diferentes | NVS antigo | gravar de novo com `-t erase` |
| Nunca aparece `→ unicast` | gateway com firmware antigo ou longe | regravar o gateway (branch `feat/node-unicast-wdt-tempcomp`) ou usar `-DDEFAULT_GATEWAY_MAC` |
| `unicast sem ACK` frequente | sinal fraco | reposicionar a antena ou usar um relay |
| Tensão da fonte < 4,5 V (alerta de fonte baixa) | fonte fraca ou cabo longo | trocar a fonte ou encurtar o cabo |
| Reinicia sozinho a cada ~6 h | sem nenhuma entrega confirmada (gateway fora do ar) | verificar o gateway e o servidor |

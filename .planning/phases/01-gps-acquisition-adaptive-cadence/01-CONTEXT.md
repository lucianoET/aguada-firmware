# Phase 1: GPS Acquisition & Adaptive Cadence - Context

**Gathered:** 2026-08-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Firmware do tracker (projeto novo `firmware/gps_tracker/`) lê o módulo GPS via UART, faz gating de qualidade dos fixes e aplica cadência adaptativa por velocidade — entregando um stream de fixes confiáveis e bem cadenciados (observável em serial USB na bancada). Flash logging é Fase 2; sync é Fase 3; energia é Fase 5. Walking Skeleton: projeto PlatformIO novo que compila, flasha e mostra fix real na bancada.

</domain>

<decisions>
## Implementation Decisions

### Hardware alvo
- **D-01:** Placa: **ESP32 DevKit clássico (WROOM-32, 30 pinos)** — mais pinos/UARTs/ADC que o C3, folga para os periféricos futuros do HW-01 e power híbrido da Fase 5.
- **D-02:** Módulos GPS: usuário tem **ATGM336H** (AT6558, GPS+BeiDou, protocolo CASIC) e **NEO-6M** (u-blox). Firmware deve ser **NMEA-agnóstico** — funcionar com ambos sem comandos proprietários (UBX/CASIC) obrigatórios. Power-save proprietário fica fora do v1.
- **D-03:** Taxa/baud: **default 9600 baud, 1 Hz** — sem reconfiguração do módulo. 1 fix/s basta para cadência de 5–10 s.
- **D-04:** Pinagem definida e documentada em `firmware/gps_tracker/gps_tracker_pinout.md`: GPS em **UART2 (GPIO16=RX2, GPIO17=TX2)**, LED onboard GPIO2 para status fix/no-fix, USB UART0 para logs 115200. Pinos reservados p/ HW-01 (I2C 21/22, botões 32/33, buzzer 25, NMEA-out GPIO4/UART1, bateria GPIO34) — só documentados, não montados nesta fase.

### Estrutura do projeto
- **D-05:** Firmware vive em **`firmware/gps_tracker/`** — projeto PlatformIO novo, irmão de `firmware/node/` e `firmware/gateway/`, compartilhando `firmware/shared/protocol.h` quando necessário (Fase v2 ESP-NOW). Não é env dentro de firmware/node.

### Claude's Discretion
- Parâmetros de cadência adaptativa (limiar de velocidade parado/movimento, janela N segundos, intervalo exato 5–10 s, heartbeat parado) — decidir no research/planning com base em PITFALLS.md e prática de campo; expor como constantes configuráveis.
- Critérios de gating de fix (HDOP máximo, sats mínimos, warmup pós-cold-start, descarte de (0,0)) — decidir no planning; seguir PITFALLS.md (fix-quality filtering precede cadência).
- Formato do log serial de bancada e organização de módulos dentro de `src/`.
- Biblioteca de parsing: research recomenda TinyGPSPlus 1.0.3a — confirmar compatibilidade com ATGM336H (NMEA padrão, deve funcionar).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Hardware / pinagem
- `firmware/gps_tracker/gps_tracker_pinout.md` — pinagem oficial do tracker (GPS UART2, reservas HW-01, pinos proibidos). Criado nesta discussão; usuário monta a bancada por ele.
- `firmware/node/node_pinout.md` — convenção de documentação de pinagem do projeto (estilo a seguir).

### Research da milestone
- `.planning/research/SUMMARY.md` — síntese; ordem de build e riscos
- `.planning/research/STACK.md` — TinyGPSPlus/LittleFS/versões; o que NÃO usar
- `.planning/research/PITFALLS.md` — gating de fix antes da cadência; TTFF/VBAT; (0,0) null-island
- `.planning/research/ARCHITECTURE.md` — fronteiras de componentes; tracker como sibling project

### Sistema existente
- `AGUADA_SYSTEM_DOC.md` — spec do sistema Aguada (relevante a partir da Fase 3/4; protocolo v3 só em v2)
- `firmware/node/platformio.ini` — convenções de envs/build_flags do repo (modelo para o platformio.ini novo)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `firmware/node/src/rgb_led.*` — padrão de LED de status; tracker usa LED onboard GPIO2, pode adaptar o padrão (não dependência direta — projetos separados)
- `firmware/node/src/sensor_filter.*` — padrão de filtro (outlier reject + média móvel) como referência conceitual para gating de fix
- `firmware/node/platformio.ini` — modelo de estrutura de envs (debug/release, build_flags com defaults)

### Established Patterns
- Nodes enviam valores crus; matemática no servidor — tracker segue o mesmo princípio (fix cru + gating, nada de cálculo de trajeto no firmware)
- Constantes configuráveis via `#define DEFAULT_*` em build_flags (padrão do repo)
- node_id = 2 últimos bytes do MAC (herdar convenção quando relevante)

### Integration Points
- Nesta fase: nenhum — projeto standalone com saída serial. Integração com logger (F2), WiFi/MQTT (F3), protocolo v3 (v2) vêm depois.

</code_context>

<specifics>
## Specific Ideas

- Usuário vai montar a bancada seguindo `gps_tracker_pinout.md` — o firmware da Fase 1 deve funcionar exatamente com essa pinagem.
- Suporte real aos DOIS módulos em mãos (ATGM336H e NEO-6M) — trocar módulo não pode exigir recompilar com flags diferentes; NMEA 9600/1Hz é o denominador comum.

</specifics>

<deferred>
## Deferred Ideas

- OLED/botões/buzzer/LEDs extras, saída NMEA 0183 p/ radar marítimo, IMU/inclinação — v2 (HW-02..05); pinos já reservados no pinout.
- Power-save proprietário do GPS (UBX CFG-RXM / CASIC) — junto com deep sleep (PWR-03, v2).
- PPS do ATGM336H (sincronização de tempo precisa) — não usado no v1.

</deferred>

---

*Phase: 1-GPS Acquisition & Adaptive Cadence*
*Context gathered: 2026-08-01*

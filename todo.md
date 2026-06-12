# TODO — Node AirQ

Trabalho pendente do node de qualidade do ar (ESP32-C3 + AHT21 + ENS160 + TEMT6000 + KY-037).
Ver visão geral em [`Node AirQ.md`](Node%20AirQ.md).

## 🔴 Bloqueador atual — KY-037/038 (som)

- [ ] **Corrigir alimentação/ligação do KY-037/038.** Diagnóstico (leitura raw longa do A0):
      bias DC preso em **~150 mV** (esperado ~1.65 V = Vcc/2) e `avgΔ` plano ~2.5 sem reação
      a palmas → o módulo **não está a ser alimentado** ou o A0 não chega ao GPIO1.
  - [ ] Medir VCC↔GND do módulo (deve dar ~3.3 V); confirmar LED de power aceso.
  - [ ] Confirmar fio do **GPIO1 no A0** (não D0) e GND comum.
  - [ ] (Alternativa) testar no **GPIO0** para descartar o GPIO1.
- [ ] Após corrigir: validar resposta (com `SOUND_DEBUG`, `mean`→~2000 e `avgΔ` dispara com som).
- [ ] **Calibrar `SOUND_DB_OFFSET`** para o silêncio ficar ~35–40.
- [ ] Gravar a **versão final sem `SOUND_DEBUG`** (remover/baixar log de telemetria de som).

## 🟠 Integração Home Assistant (luz + som)

- [ ] Publicar **luz (lux)** e **som (dB rel.)** via ESP-NOW → MQTT → HA.
  - [ ] Definir `sensor_id` novos no protocolo v3 (ex.: luz, som) sem quebrar os 16 bytes.
  - [ ] Encoder no node + parse no `tools/bridge.py` + MQTT Discovery.
  - [ ] Entidades no Home Assistant.

## 🟡 Calibração / qualidade

- [ ] **TEMT6000:** afinar a constante de lux contra um luxímetro de referência
      (atualmente estimativa não calibrada; ~200 mV ≈ 41 lux no teste).
- [ ] Confirmar que a amostragem extra (luz + som ~66 ms/tick) não afeta mesh/relay.

## 🟢 UI / captive page

- [ ] Com 4 componentes a página passou de "1 ecrã" — tornar os containers de sensor
      **recolhíveis** (ou compactar) para voltar a caber num ecrã de telemóvel.
- [ ] (Opcional) renomear SSID do AP `ANI-…` → `ARQ-…` para coerência com o nome AirQ.

## ⚪ Futuro

- [ ] Deep sleep: incompatível com amostragem contínua de luz/som e com relay — avaliar.
- [ ] Persistir baseline do ENS160 em NVS (reduz warmup entre reboots).

---

### Estado dos sensores (resumo)

| Sensor | Estado |
|---|---|
| AHT21 (T/H) | ✅ validado |
| ENS160 (eCO₂/TVOC/AQI) | ✅ validado |
| TEMT6000 (luz) | ✅ validado |
| KY-037/038 (som) | 🔴 ligado, não lê (alimentação) |

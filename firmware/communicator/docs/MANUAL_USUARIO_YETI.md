# Manual do usuário — Yeti Aguada

O Yeti é um boneco de mesa que também é um **communicator** do Aguada. Com ele você:

- **fala para todos** os aparelhos de anúncio ao mesmo tempo (segurando a cabeça dele);
- **ouve** os anúncios dos outros postos;
- **fica sabendo do alarme**: ele fica vermelho e toca a sirene;
- vê **temperatura, umidade e luz** da sala.

Ele também reage: pisca, olha em volta, gosta de carinho, fica tonto se chacoalhado e dorme
no escuro. Montagem: [MONTAGEM_YETI_S3.md](MONTAGEM_YETI_S3.md).

---

## 1. Conheça o Yeti

| Parte | Para quê |
|-------|----------|
| **Topo da cabeça** | toque: carinho, clima, volume · segure: falar (PTT) |
| **Braço direito** | toque = **sim** · segure = gatilho D |
| **Braço esquerdo** | toque = **não** · segure = gatilho E |
| **Os dois braços** | abraço |
| **Rosto (tela)** | olhos e boca mostram o que está acontecendo |
| **Bochechas** | acendem com a cor do humor |
| **Furos nas laterais** | microfones — fale de frente para o Yeti |
| **Chave na base** | liga e desliga |
| **Jack / conector na base** | carrega a bateria |

Esquerdo e direito = de quem olha o Yeti de frente.

---

## 2. Gestos

| Gesto | O que acontece |
|-------|----------------|
| **1 toque** na cabeça | olhos felizes ^ ^, sorriso e um piado |
| **2 toques** | tela com temperatura, umidade e luz por 4 s; no rodapé, `bateria NN%` |
| **3 toques** | muda o volume: 25 % → 50 % → 75 % → 100 % → 25 % |
| **Segurar** a cabeça (½ s) | **fala para todos** (veja a seção 4) |
| Tocar o braço **direito** | `sim`, bochechas verdes, bipe |
| Tocar o braço **esquerdo** | `nao`, bochechas vermelhas, bipe |
| **Segurar** um braço (1 s) | `gatilho E` / `gatilho D`, bochechas âmbar |
| Tocar os **dois braços** juntos | abraço: olhos felizes, piado, bochechas rosa |
| Inclinar o Yeti | os olhos acompanham |
| Chacoalhar | olhos de espiral (tonto) por 3 s |
| Bater de leve no corpo | acorda, se estiver dormindo |
| Barulho de um lado | os olhos viram para o lado do som |

Os gestos dos braços (sim, não, gatilho, abraço) também saem na serial como `YETI_EVT`, para
automações.

---

## 3. O rosto

| Rosto | Cor | O que está acontecendo |
|-------|-----|------------------------|
| Olhos piscando, olhando em volta | branco-azulado | tudo calmo |
| Olhos ^ ^ e sorriso | branco-azulado | carinho ou abraço |
| Olhos e boca abrindo com a voz | **verde** | **você está no ar** (falando para todos) |
| Boca mexendo, ondas dos lados | azul | outro posto está falando |
| Olhos bravos, piscando | **vermelho** | **ALARME** — siga o procedimento de emergência |
| Espirais | — | foi chacoalhado |
| Olhos semicerrados, "z z z" | cinza, tela fraca | dormindo (sala escura há 30 s) |
| Tremendo | azul | sala fria (≤ 18 °C) |
| Gota de suor | laranja | sala quente (≥ 30 °C) |

O ponto no canto superior direito da tela: **verde** = na rede de voz; **cinza** = sem WiFi.

Mensagens que aparecem por 2 s no rodapé:

| Mensagem | Significa |
|----------|-----------|
| `volume 50%` | volume mudou |
| `bateria 72%` | carga da bateria |
| `bateria fraca` | carregue o Yeti (repete a cada 5 min) |
| `sem WiFi` | sem rede: não dá para falar agora (o alarme continua chegando) |
| `sem microfone` | microfone com defeito: avise a manutenção |

---

## 4. Fazer um anúncio

1. Veja se o rosto está calmo (se estiver **azul com a boca mexendo**, alguém está falando).
2. **Segure o topo da cabeça.** O rosto fica **verde** e toca o gongo.
3. **Espere o gongo** (dois toques, meio segundo). O que for dito durante o gongo **não sai**.
4. **Fale** de frente para o Yeti, a 20–40 cm, em tom normal.
5. **Solte** ao terminar.

Enquanto você fala, o alto-falante do Yeti fica mudo (evita apito). As dicas do
[manual do communicator](MANUAL_USUARIO_COMMUNICATOR.md#3-fazer-um-anúncio) valem aqui.

---

## 5. Alarme

O Yeti **não dispara** alarme — ele só **avisa**. Quando um posto dispara:

- o rosto fica **vermelho piscando**, as bochechas piscam e a tela vai ao brilho máximo;
- a **sirene toca no alto-falante** do Yeti (desliga sozinha depois de 3 min; o rosto continua
  vermelho até o alarme ser cancelado no posto que disparou).

Enquanto a sirene toca, os anúncios não saem neste Yeti.

---

## 6. Ligar, carregar e desligar

- **Ligar:** chave da base. Toca um arpejo e, em até 15 s (tempo de entrar no WiFi), os olhos
  aparecem.
- **Carregar:** ligue o jack ou o conector magnético. O LED do carregador fica vermelho
  carregando e azul/verde carregado. Pode usar o Yeti carregando.
- **Bateria:** 2 toques na cabeça mostram a carga. Abaixo de ~5 % (3,45 V) aparece `bateria fraca`: carregue logo.
- **Desligar:** chave da base. Não perde configuração.
- **Não toque nos braços ao ligar:** é quando ele aprende como é o braço sem toque.

---

## 7. Problemas comuns

| O que acontece | O que fazer |
|----------------|-------------|
| Tela apagada e sem som ao ligar | carregar a bateria; conferir a chave |
| `sem WiFi` ao segurar a cabeça | aguardar 1 min (reconecta sozinho); se continuar, avisar a manutenção |
| Braço responde sozinho ou não responde | desligar e ligar **sem tocar nos braços** |
| Fica dormindo com a sala acesa | o sensor de luz (furo no rosto) está tapado |
| Temperatura diferente do termômetro da sala | normal até ~1 °C; mais que isso, avisar a manutenção |
| Ninguém ouve meu anúncio | conferir se o rosto ficou **verde**; falar só depois do gongo |
| Rosto vermelho sem emergência | alarme de teste ou disparo em outro posto: confirme com a central |
| `bateria fraca` | carregar |

Manutenção: Aguada / CMASM — o código de 4 letras/números no serial (`YETI_BOOT node=…`)
identifica cada aparelho.

# Manual do usuário — Communicator Aguada

O Communicator é o aparelho de **anúncios e alarme** da área. Com ele você:

- **fala para todos** os aparelhos ao mesmo tempo (e para os amplificadores ligados a eles);
- **ouve** os anúncios dos outros postos;
- **dispara o alarme**: sirene em todos os pontos e aviso na central.

Montagem e instalação: [MONTAGEM_COMMUNICATOR_C3.md](MONTAGEM_COMMUNICATOR_C3.md).

---

## 1. Conheça o aparelho

| Parte | Para quê |
|-------|----------|
| **Botão PTT** | segure para falar, solte para encerrar |
| **Botão ALARME** (vermelho) | segure **1 s** para disparar; segure **3 s** para cancelar |
| **Botão BOOT** (pequeno, na placa) | toque para mudar o volume |
| **Tela** | mostra o que está acontecendo |
| **Microfone** | furo na frente da caixa — fale na direção dele |
| **Alto-falante** | toca os anúncios, o gongo e a sirene |
| **LED da placa** | aceso = você está no ar |

---

## 2. A tela

```
┌────────────────────────────┐
│ PORTARIA Z1          -58dBm│  ← nome, zona, sinal do WiFi
├────────────────────────────┤
│                            │
│          PRONTO            │  ← estado (tabela abaixo)
│    segure PTT p/ falar     │
│                            │
│ vol 75%  mic ok            │  ← volume, microfone, SIR = sirene ligada
└────────────────────────────┘
```

| Estado na tela | O que está acontecendo | O que fazer |
|----------------|------------------------|-------------|
| **PRONTO** | tudo em silêncio | pode falar ou aguardar |
| **NO AR** (tela clara) | você está transmitindo | fale; solte o PTT ao terminar |
| **OUVINDO** · anuncio de XXXX | outro posto está falando | ouça; aguarde terminar para responder |
| **ALARME** piscando · local | você disparou o alarme | siga o procedimento de emergência |
| **ALARME** piscando · de XXXX | o alarme veio de outro posto | siga o procedimento de emergência |
| **INCENDIO** piscando | alarme de incêndio | siga o plano de abandono |

Mensagens que aparecem por 2 s no rodapé:

| Mensagem | Significa |
|----------|-----------|
| `VOLUME 50%` | volume mudou |
| `SEM MICROFONE` | este aparelho não tem mic: só ouve |
| `SEM WIFI` | sem rede: não dá para falar agora (o alarme continua funcionando) |
| `ALARME CANCELADO` | você cancelou o alarme |

`sem WiFi` no topo à direita = aparelho fora da rede. Avise a manutenção.

---

## 3. Fazer um anúncio

1. **Espere** a tela mostrar **PRONTO** (se estiver em OUVINDO, alguém está falando).
2. **Segure o PTT.** A tela fica clara com **NO AR** e o LED acende.
3. **Espere o gongo** (dois toques, meio segundo). O que for dito durante o gongo **não sai**.
4. **Fale** a 10–20 cm do microfone, em tom normal — o aparelho ajusta o volume sozinho.
   Não precisa gritar; gritar distorce.
5. **Solte o PTT** ao terminar.

**Dicas para um bom anúncio:**

- Diga quem fala e para quem: *"Atenção, portaria para equipe de manutenção…"*.
- Frases curtas, com pausa entre elas. Repita a informação importante.
- Para um **discurso** ou aviso longo, mantenha o PTT segurado o tempo todo; soltar encerra.
- Enquanto você fala, o seu próprio alto-falante fica mudo (isso evita o apito de microfonia).
- Se ouvir **apito** nos alto-falantes da área, afaste-se das caixas de som ou vire o microfone.

**Quem passa na frente:** se dois postos falam ao mesmo tempo, os outros aparelhos ouvem quem
começou primeiro. A **sala de controle** tem prioridade e interrompe qualquer anúncio.

---

## 4. Ouvir anúncios

Não precisa fazer nada: o gongo toca e a voz sai no alto-falante (e nos amplificadores ligados).
A tela mostra **OUVINDO** e o código do posto que fala.

**Volume:** toque no botão **BOOT** da placa: 25 % → 50 % → 75 % → 100 % → 25 %.
Ao desligar e ligar, o volume volta ao padrão (75 %).

---

## 5. Alarme

### Disparar

**Segure o botão ALARME por 1 segundo.** (Um toque rápido não dispara — é de propósito.)

- A tela pisca **ALARME**.
- A sirene toca aqui e **em todos os pontos de sirene** da área.
- O aviso vai também para o gateway do Aguada. *(O registro na central/Home Assistant entra na
  próxima versão do sistema — até lá, avise a central por telefone.)*

### Cancelar

**Segure o botão ALARME por 3 segundos** até aparecer `ALARME CANCELADO`.

> Para cancelar **em todos os aparelhos**, cancele no **mesmo aparelho que disparou**. Cancelar
> em outro posto só silencia aquele posto.

### Durante o alarme

- A sirene **desliga sozinha depois de 3 minutos**; a tela continua mostrando o alarme até
  alguém cancelar.
- Enquanto a sirene toca, os anúncios **não saem** no alto-falante daquele aparelho. Para dar
  orientações por voz, cancele o alarme primeiro (no aparelho que disparou).
- O alarme também funciona **sem WiFi**: ele vai pelo rádio do Aguada.

### Use o alarme só para

Emergência real (incêndio, acidente, invasão) ou **teste programado e avisado**. Disparo falso
mobiliza todas as equipes.

---

## 6. Ligar e desligar

- **Ligar:** basta energizar. Em até 15 s (tempo de entrar no WiFi) toca um **bip** e a tela
  mostra **PRONTO**.
- **Desligar:** pode cortar a energia a qualquer momento; não perde configuração.
- Na falta de energia o aparelho para. Ao voltar, volta sozinho.

---

## 7. Problemas comuns

| O que acontece | O que fazer |
|----------------|-------------|
| Tela apagada e sem bip ao ligar | conferir a tomada/fonte; avisar a manutenção |
| `SEM WIFI` ao apertar o PTT | aguardar 1 min (reconecta sozinho); se continuar, avisar a manutenção |
| `SEM MICROFONE` | este aparelho só ouve; use um posto de anúncio |
| Ninguém ouve meu anúncio | conferir se a tela mostrou **NO AR**; falar só depois do gongo |
| Minha voz sai cortada | aproximar-se do aparelho do WiFi ou avisar a manutenção (sinal fraco no topo da tela) |
| Voz muito baixa | falar mais perto do microfone (10–20 cm); subir o volume no BOOT de quem ouve |
| Apito alto na área | soltar o PTT; afastar-se das caixas de som |
| Alarme não cancela nos outros postos | cancelar no aparelho que disparou (código na tela: "de XXXX") |
| Sirene parou mas a tela segue em ALARME | normal depois de 3 min; cancelar quando a situação estiver resolvida |

Manutenção: Aguada / CMASM — o código de 4 letras/números (ex.: `A1F3`) identifica cada aparelho.

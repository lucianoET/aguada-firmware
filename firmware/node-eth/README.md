# node-eth — Node CAV cabeado (Ethernet)

## Resumo

Arduino Nano + módulo ENC28J60 (Ethernet) + sensor ultrassônico HC-SR04, dedicado ao node **0xEE02** (**CAV**, Castelo de Incêndio). Publica JSON diretamente no broker MQTT em `aguada/raw/0xEE02/1`. Todo o cálculo de nível/percentual/volume acontece no `bridge.py`, usando `tools/reservoirs.yaml` — nunca no node.

## Pinout

| Sinal          | Pino Nano | Observação                          |
|----------------|-----------|--------------------------------------|
| HC-SR04 TRIG   | D6        | saída digital                        |
| HC-SR04 ECHO   | D7        | entrada digital (não D5)             |
| ENC28J60 CS    | D10       | chip select (padrão UIPEthernet)     |
| ENC28J60 SI/MOSI | D11     | SPI hardware                         |
| ENC28J60 SO/MISO | D12     | SPI hardware                         |
| ENC28J60 SCK   | D13       | SPI hardware                         |
| 5V             | 5V        | alimenta o HC-SR04                   |
| GND            | GND       | comum a todos os módulos             |

O HC-SR04 precisa de 5 V; o ENC28J60 é um módulo de 3.3 V — não ligue o ENC28J60 direto no barramento 5V.

## Alimentação (importante)

O ENC28J60 consome cerca de 120–180 mA, e o pino 3V3 embutido do Nano **não** aguenta essa carga. O módulo precisa de um regulador 3.3 V próprio (a maioria das placas ENC28J60 já vem com um onboard) ou ser alimentado por um AMS1117-3.3 (ou equivalente) externo, com o GND compartilhado com o Nano. Sintoma de errar isso: a placa liga, conecta ao MQTT e cai logo em seguida, ou o node reseta em loop.

## Rede

- IP estático: `192.168.0.202`
- Gateway/DNS: `192.168.0.1`
- Máscara: `255.255.255.0`
- MAC: `AA:BB:CC:DD:EE:02`
- Broker MQTT: `192.168.0.101:1883`
- Client ID MQTT: `aguada-0xEE02`
- Tópico de publicação: `aguada/raw/0xEE02/1`

Não há DHCP nem configuração em runtime. Para alterar qualquer um desses valores, edite `src/main.cpp` e regrave o firmware.

## Build e gravação

`pio` não está no PATH nesta máquina — use o binário do venv:

```bash
# build
/home/luc/Dev/aguada-firmware-main/.venv/bin/pio run -e node-cav-eth

# gravar (Nano usa bootloader antigo, 57600 baud)
/home/luc/Dev/aguada-firmware-main/.venv/bin/pio run -e node-cav-eth -t upload --upload-port /dev/ttyUSB0

# monitor serial
pio device monitor -b 9600
```

Saída serial só aparece com `DEBUG` setado para `1` em `src/main.cpp` (por padrão é `0`, para economizar flash).

## Comportamento

- Medição a cada 30 s, mediana de 5 amostras (ciclo precisa de ao menos 3 leituras válidas na faixa 2–450 cm).
- Transmite quando a leitura muda ≥ 2 cm em relação ao último envio, ou após 120 s sem enviar (o que vier primeiro).
- Após 3 ciclos de medição falhos seguidos, publica um pacote de erro com `distance_cm=65535` e `flags=4`.
- Watchdog de 8 s protege contra travamentos.
- Retry de conexão MQTT a cada 5 s.
- Após 10 falhas de conexão MQTT seguidas, reinicializa a stack Ethernet; após 20, força um reset via watchdog.

## Troubleshooting

- Verificar se o broker está vendo o node: `mosquitto_sub -h 192.168.0.101 -t 'aguada/raw/#' -v`
- Verificar se o bridge republicou os dados: `mosquitto_sub -h 192.168.0.101 -t 'aguada/0XEE02/#' -v`
- `ping 192.168.0.202` para separar problema de rede de problema de broker.
- Se o ping funciona mas o MQTT não conecta, suspeite da alimentação 3.3 V do ENC28J60 ou de uma ACL do broker.
- Se nada aparece em nenhum tópico, sete `DEBUG` para `1` em `src/main.cpp`, regrave e acompanhe pelo monitor serial em 9600 baud.

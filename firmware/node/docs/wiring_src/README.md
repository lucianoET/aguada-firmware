# Gerador dos diagramas de ligação do AirQ

Os diagramas em `../airq_wiring_core.svg`, `../airq_wiring_ac.svg` e a página
`../airq_ligacoes.html` são **gerados** — não editar à mão. As coordenadas dos pinos
vêm das funções `c3()`, `ens()` e `inmp()` em `gen.py`, por isso os fios batem sempre
com os pinos.

```bash
cd firmware/node/docs/wiring_src
python3 export_svg.py          # regera ../airq_wiring_*.svg e ../airq_ligacoes.html
```

- `gen.py` — placas (ESP32-C3 SuperMini, ENS160+AHT21, INMP441) e os 3 diagramas (caixa central, portas do AC, alimentação por bateria)
- `page.py` — página completa (foto anotada, legenda, checklist, cuidados)
- `export_svg.py` — SVG independentes, com cores fixas (abrem no GitHub)
- `crop.jpg` — foto da bancada de 14/10/2026 usada na página

Mudou um pino no firmware (`main.cpp`)? Mude também aqui e regere.

# Gera firmware/node/docs/node_reservatorio_ligacoes.svg (env node-c3-ch11)
import sys
W, H = 960, 800
C = dict(v5="#d6332a", gnd="#1c1f23", v33="#e0661a", trig="#1f6fd1", echo="#1a9a52",
         mon="#9a5b2e", dq="#0d8c8c", off="#a3ada8", ink="#14201b", muted="#5a6b64",
         board="#1d3b5c", panel="#f3f6f4")
o = []
def t(x, y, s, size=13, fill=C["ink"], anchor="start", weight=400, mono=False):
    fam = "DejaVu Sans Mono,Consolas,monospace" if mono else "Arial,Helvetica,sans-serif"
    o.append(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')
def line(pts, col, w=3):
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    o.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>')
def dot(x, y, col=C["ink"]): o.append(f'<circle cx="{x}" cy="{y}" r="4.5" fill="{col}"/>')
def rect(x, y, w, h, fill, stroke="none", rx=6, sw=1.5):
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')
def res(x, y1, y2, label, col):           # resistor vertical entre y1 e y2
    m = (y1 + y2) / 2
    line([(x, y1), (x, m - 18)], col); line([(x, m + 18), (x, y2)], col)
    rect(x - 8, m - 18, 16, 36, "#fff", C["ink"], rx=2)
    t(x + 16, m + 5, label, 13, weight=700)
def gnd(x, y):
    line([(x - 12, y), (x + 12, y)], C["gnd"], 2.5); line([(x - 7, y + 5), (x + 7, y + 5)], C["gnd"], 2.5)
    line([(x - 3, y + 10), (x + 3, y + 10)], C["gnd"], 2.5)
def tag(x, y, w, col, title, sub):
    rect(x, y - 20, w, 40, "#fff", col, rx=6, sw=2)
    t(x + 10, y - 3, title, 13, weight=700)
    t(x + 10, y + 13, sub, 11, C["muted"])

o.append(f'<rect width="{W}" height="{H}" fill="#fbfcfb"/>')
t(24, 34, "Nó reservatório Aguada — ligações (env node-c3-ch11)", 21, weight=700)
t(24, 56, "ESP32-C3 SuperMini visto por cima (componentes para cima, USB-C no topo). Confira a serigrafia da sua placa.", 12, C["muted"])

# ── placa ─────────────────────────────────────────────────────────────────────
bx, by, bw, bh = 400, 100, 170, 404
rect(bx + 55, by - 22, 60, 30, "#9aa3a8", rx=5); t(bx + 85, by - 2, "USB-C", 11, "#fff", "middle", 700)
rect(bx, by, bw, bh, C["board"], rx=10)
t(bx + bw / 2, by + 214, "ESP32-C3", 15, "#fff", "middle", 700)
t(bx + bw / 2, by + 234, "SuperMini", 13, "#cfe0ee", "middle")
rect(bx + 50, by + 336, 70, 46, "#2c557f", rx=4); t(bx + 85, by + 364, "antena", 11, "#cfe0ee", "middle")
left  = ["5V", "GND", "3V3", "G4", "G3", "G2", "G1", "G0"]
right = ["G5", "G6", "G7", "G8", "G9", "G10", "G20", "G21"]
ys = [by + 40 + i * 46 for i in range(8)]
use_l = {"5V": "v5", "GND": "gnd", "3V3": "v33", "G4": "echo", "G3": "trig", "G1": "mon"}
use_r = {"G10": "dq"}
note_r = {"G8": "LED", "G9": "BOOT"}
for name, y in zip(left, ys):
    col = C[use_l[name]] if name in use_l else C["off"]
    o.append(f'<circle cx="{bx}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + 14, y + 5, name, 13, "#fff", weight=700, mono=True)
for name, y in zip(right, ys):
    o.append(f'<circle cx="{bx+bw}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + bw - 14, y + 5, name, 13, "#fff", "end", 700, True)

# ── fios da esquerda → etiquetas ──────────────────────────────────────────────
tags_l = {
    "5V":  ("5V  ← Fonte + (5 V DC)", "também: sensor VCC · R3 200k · C1 470µF"),
    "GND": ("GND ← Fonte −", "também: sensor GND · R2 · R4 · DS18B20 GND"),
    "3V3": ("3V3 → DS18B20 VDD", "e pull-up R5 4k7 (saída, não alimentar aqui)"),
    "G4":  ("ECHO ← nó do divisor R1/R2", "NUNCA ligar o ECHO de 5 V direto"),
    "G3":  ("TRIG → sensor TRIG", "direto (3,3 V basta para o sensor)"),
    "G1":  ("MON 5V ← nó do divisor R3/R4", "leitura da fonte (÷3)"),
}
for name, y in zip(left, ys):
    if name in tags_l:
        col = C[use_l[name]]
        line([(bx - 7, y), (360, y)], col)
        tag(24, y, 336, col, *tags_l[name])
    elif name in ("G2", "G0"):
        t(bx - 14, y + 5, "livre" if name == "G0" else "não usar (boot)", 11, C["off"], "end")
# ── direita ───────────────────────────────────────────────────────────────────
for name, y in zip(right, ys):
    if name in use_r:
        line([(bx + bw + 7, y), (610, y)], C["dq"])
        tag(610, y, 326, C["dq"], "G10 ↔ DS18B20 DQ (amarelo)", "opcional · pull-up R5 4k7 para 3V3")
    elif name in note_r:
        t(bx + bw + 14, y + 5, f"{note_r[name]} — não usar", 11, C["off"])

# ── detalhes ──────────────────────────────────────────────────────────────────
top = 560
def panel(x, w, title):
    rect(x, top, w, 220, C["panel"], "#d5ddd8", rx=10)
    t(x + 14, top + 24, title, 14, weight=700)
# 1) divisor do ECHO
panel(24, 290, "1 · Divisor do ECHO (5 V → 3,3 V)")
x = 110
t(x, top + 52, "sensor ECHO", 12, C["echo"], "middle", 700)
line([(x, top + 60), (x, top + 70)], C["echo"])
res(x, top + 70, top + 130, "R1 1k", C["echo"])
dot(x, top + 130, C["echo"]); line([(x, top + 130), (250, top + 130)], C["echo"]); t(258, top + 135, "G4", 13, weight=700, mono=True)
res(x, top + 130, top + 190, "R2 2k", C["gnd"])
gnd(x, top + 190)
t(170, top + 165, "2k = 2× 1k em série", 11, C["muted"])
t(170, top + 180, "5 V × 2/3 = 3,3 V", 11, C["muted"])
# 2) monitor da fonte
panel(330, 290, "2 · Monitor da fonte 5 V (G1)")
x = 416
t(x, top + 52, "5V", 12, C["v5"], "middle", 700)
line([(x, top + 60), (x, top + 70)], C["v5"])
res(x, top + 70, top + 130, "R3 200k", C["v5"])
dot(x, top + 130, C["mon"]); line([(x, top + 130), (556, top + 130)], C["mon"]); t(564, top + 135, "G1", 13, weight=700, mono=True)
res(x, top + 130, top + 190, "R4 100k", C["gnd"])
gnd(x, top + 190)
line([(500, top + 130), (500, top + 160)], C["mon"], 2)
line([(490, top + 160), (510, top + 160)], C["ink"], 2.5); line([(490, top + 167), (510, top + 167)], C["ink"], 2.5)
line([(500, top + 167), (500, top + 190)], C["gnd"], 2); gnd(500, top + 190)
t(518, top + 168, "C3 100nF", 11, C["muted"])
# 3) DS18B20
panel(636, 300, "3 · Sonda DS18B20 (opcional)")
x = 700
t(x, top + 52, "3V3", 12, C["v33"], "middle", 700)
line([(x, top + 60), (x, top + 70)], C["v33"])
res(x, top + 70, top + 130, "R5 4k7", C["v33"])
dot(x, top + 130, C["dq"]); line([(x, top + 130), (900, top + 130)], C["dq"]); t(860, top + 122, "G10", 13, weight=700, mono=True)
line([(x, top + 130), (x, top + 165)], C["dq"])
rect(x - 34, top + 165, 68, 30, "#2a2f33", rx=4); t(x, top + 185, "DS18B20", 11, "#fff", "middle", 700)
t(784, top + 160, "vermelho → 3V3", 11, C["v5"]); t(784, top + 176, "preto → GND", 11, C["gnd"])
t(784, top + 192, "amarelo → DQ / G10", 11, C["dq"])
t(784, top + 208, "sonda no ar da caixa", 11, C["muted"])

o.insert(0, f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">'
            '<title>Nó reservatório Aguada — ligações</title>')
o.append("</svg>")
open(sys.argv[1], "w").write("\n".join(o))

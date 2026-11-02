# Gera o diagrama de ligações do communicator_c3 (mesmo estilo do nó reservatório):
#   python3 communicator.py ../communicator_c3_ligacoes.svg
# Mudou um pino em include/comm_config.h? Mude aqui também e regere.
import sys
W, H = 960, 1060
C = dict(v5="#d6332a", gnd="#1c1f23", v33="#e0661a", bclk="#1f6fd1", ws="#7b4fc9",
         dout="#1a9a52", din="#0d8c8c", i2c="#8a5cb8", btn="#c27c0e", sir="#b3261e",
         off="#a3ada8", ink="#14201b", muted="#5a6b64", board="#1d3b5c", panel="#f3f6f4")
o = []
def t(x, y, s, size=13, fill=C["ink"], anchor="start", weight=400, mono=False):
    fam = "DejaVu Sans Mono,Consolas,monospace" if mono else "Arial,Helvetica,sans-serif"
    s = str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    o.append(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')
def line(pts, col, w=3):
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    o.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>')
def dot(x, y, col=C["ink"]): o.append(f'<circle cx="{x}" cy="{y}" r="4.5" fill="{col}"/>')
def rect(x, y, w, h, fill, stroke="none", rx=6, sw=1.5):
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')
def res(x, y1, y2, label, col):           # resistor vertical entre y1 e y2
    m = (y1 + y2) / 2
    line([(x, y1), (x, m - 16)], col); line([(x, m + 16), (x, y2)], col)
    rect(x - 7, m - 16, 14, 32, "#fff", C["ink"], rx=2)
    t(x + 13, m + 5, label, 12, weight=700)
def gnd(x, y):
    line([(x - 12, y), (x + 12, y)], C["gnd"], 2.5); line([(x - 7, y + 5), (x + 7, y + 5)], C["gnd"], 2.5)
    line([(x - 3, y + 10), (x + 3, y + 10)], C["gnd"], 2.5)
def tag(x, y, w, col, title, sub):
    rect(x, y - 20, w, 40, "#fff", col, rx=6, sw=2)
    t(x + 10, y - 3, title, 13, weight=700)
    t(x + 10, y + 13, sub, 11, C["muted"])
def module(x, y, w, name, rows, dark="#2a2f33"):
    # Módulo com uma linha por pino: [pino] ── destino (cor do sinal).
    rect(x, y, w, 26, dark, rx=5); t(x + w / 2, y + 18, name, 12, "#fff", "middle", 700)
    for i, (pin, dest, col) in enumerate(rows):
        yy = y + 46 + i * 20
        t(x + 8, yy, pin, 11, weight=700, mono=True)
        line([(x + 74, yy - 4), (x + 104, yy - 4)], col, 3)
        t(x + 110, yy, dest, 11, col if col != C["off"] else C["muted"], weight=700)
    return y + 46 + len(rows) * 20
def panel(x, y, w, h, title):
    rect(x, y, w, h, C["panel"], "#d5ddd8", rx=10)
    t(x + 14, y + 24, title, 14, weight=700)

o.append(f'<rect width="{W}" height="{H}" fill="#fbfcfb"/>')
t(24, 34, "Communicator C3 Aguada — ligações (env communicator_c3)", 21, weight=700)
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
for name, y in zip(left, ys):
    o.append(f'<circle cx="{bx}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + 14, y + 5, name, 13, "#fff", weight=700, mono=True)
for name, y in zip(right, ys):
    o.append(f'<circle cx="{bx+bw}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + bw - 14, y + 5, name, 13, "#fff", "end", 700, True)

tags_l = {
    "5V":  ("v5",   "5V  ← Fonte + (5 V DC, ≥ 2 A)", "também: VIN do MAX98357A / PCM5102A"),
    "GND": ("gnd",  "GND ← Fonte − (comum a tudo)", "mic · amp · OLED · botões · GND dos 12 V"),
    "3V3": ("v33",  "3V3 → INMP441 VDD · OLED VCC", "saída 3,3 V (+ SD_MODE / XMT do amp)"),
    "G4":  ("bclk", "BCLK → amp BCLK + mic SCK", "I2S: um fio que se divide"),
    "G3":  ("dout", "DOUT → amp DIN", "MAX98357A ou PCM5102A"),
    "G1":  ("btn",  "PTT ← botão ao GND", "segurar para falar"),
    "G0":  ("btn",  "ALARME ← botão ao GND", "1 s dispara · 3 s cancela"),
}
for name, y in zip(left, ys):
    if name in tags_l:
        k, a, b = tags_l[name]
        line([(bx - 7, y), (360, y)], C[k]); tag(24, y, 336, C[k], a, b)
    else:
        t(bx - 14, y + 5, "livre (pino de boot)", 11, C["off"], "end")

tags_r = {
    "G5":  ("ws",   "WS → amp LRC + mic WS", "I2S: um fio que se divide"),
    "G6":  ("i2c",  "SDA ↔ OLED SDA", "SSD1306 0x3C"),
    "G7":  ("i2c",  "SCL → OLED SCL", ""),
    "G10": ("din",  "DIN ← mic SD", "INMP441 (sem mic: detectado)"),
    "G20": ("sir",  "SIRENE → gate do MOSFET", "100 Ω série · 10k ao GND"),
}
note_r = {"G8": "LED da placa (no ar)", "G9": "BOOT = botão VOLUME", "G21": "livre (pulsa no boot)"}
for name, y in zip(right, ys):
    if name in tags_r:
        k, a, b = tags_r[name]
        line([(bx + bw + 7, y), (610, y)], C[k]); tag(610, y, 326, C[k], a, b)
    else:
        t(bx + bw + 14, y + 5, note_r[name], 11, C["off"])

# ── painéis ───────────────────────────────────────────────────────────────────
top = 540
panel(24, top, 296, 250, "1 · Microfone INMP441")
module(44, top + 40, 256, "INMP441", [
    ("VDD", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("L/R", "GND (canal esq.)", C["gnd"]),
    ("SCK", "G4  BCLK", C["bclk"]), ("WS", "G5  WS", C["ws"]), ("SD", "G10 DIN", C["din"]),
])
t(44, top + 238, "furo do mic para fora · fios < 20 cm", 11, C["muted"])

panel(340, top, 596, 250, "2 · Saída de áudio — escolha um módulo (mesmo firmware)")
module(360, top + 40, 270, "MAX98357A → alto-falante", [
    ("VIN", "5V", C["v5"]), ("GND", "GND", C["gnd"]), ("SD", "3V3 (liga o amp)", C["v33"]),
    ("GAIN", "solto (9 dB)", C["off"]), ("BCLK", "G4", C["bclk"]), ("LRC", "G5", C["ws"]),
    ("DIN", "G3", C["dout"]),
])
t(360, top + 238, "falante 4–8 Ω, ≤ 3 W nos bornes + / −", 11, C["muted"])
t(645, top + 130, "ou", 15, C["muted"], "middle", 700)
module(660, top + 40, 260, "PCM5102A → amplificador", [
    ("VIN", "5V", C["v5"]), ("GND", "GND", C["gnd"]), ("SCK", "GND (PLL interno)", C["gnd"]),
    ("BCK", "G4", C["bclk"]), ("LCK", "G5", C["ws"]), ("DIN", "G3", C["dout"]),
    ("XMT", "3V3 (sem: mudo)", C["v33"]),
])
t(660, top + 238, "FMT · FLT · DEMP → GND · jack → AUX", 11, C["muted"])

top2 = top + 270
panel(24, top2, 296, 230, "3 · Botões")
for i, (pin, lab, sub) in enumerate([("G1", "PTT", "botão grande, NA"), ("G0", "ALARME", "vermelho / cogumelo")]):
    y = top2 + 66 + i * 70
    t(44, y + 5, pin, 13, weight=700, mono=True)
    line([(76, y), (140, y)], C["btn"])
    line([(140, y), (160, y - 14)], C["ink"], 2.5); dot(140, y, C["ink"]); dot(166, y, C["ink"])
    line([(166, y), (196, y)], C["gnd"]); gnd(196, y + 2)
    t(222, y, lab, 13, weight=700); t(222, y + 16, sub, 11, C["muted"])
t(44, top2 + 210, "VOLUME = botão BOOT da placa (G9)", 11, C["muted"])

panel(340, top2, 316, 230, "4 · Sirene 12 V (MOSFET low-side)")
x = 520
t(x, top2 + 50, "+12 V", 12, C["v5"], "middle", 700)
line([(x, top2 + 56), (x, top2 + 72)], C["v5"])
rect(x - 34, top2 + 72, 68, 30, "#2a2f33", rx=4); t(x, top2 + 92, "SIRENE", 11, "#fff", "middle", 700)
line([(x, top2 + 102), (x, top2 + 140)], C["sir"])
t(x + 46, top2 + 92, "1N4007 em paralelo", 11, C["muted"]); t(x + 46, top2 + 106, "(cátodo no +12 V)", 11, C["muted"])
rect(x - 22, top2 + 140, 44, 44, "#fff", C["ink"], rx=4); t(x, top2 + 160, "MOS", 11, weight=700, anchor="middle")
t(x, top2 + 175, "FET", 11, weight=700, anchor="middle")
line([(x, top2 + 184), (x, top2 + 200)], C["gnd"]); gnd(x, top2 + 200)
t(x + 30, top2 + 158, "IRLZ44N / AO3400", 11, C["muted"]); t(x + 30, top2 + 172, "(logic-level)", 11, C["muted"])
t(360, top2 + 166, "G20", 13, weight=700, mono=True)
line([(392, top2 + 162), (420, top2 + 162)], C["sir"])
rect(420, top2 + 155, 30, 14, "#fff", C["ink"], rx=2); t(435, top2 + 150, "100 Ω", 10, weight=700, anchor="middle")
line([(450, top2 + 162), (x - 22, top2 + 162)], C["sir"])
dot(470, top2 + 162, C["sir"]); res(470, top2 + 162, top2 + 210, "", C["gnd"]); t(380, top2 + 196, "10k → GND", 11, C["muted"])
gnd(470, top2 + 210)

panel(676, top2, 260, 230, "5 · OLED SSD1306 128×64")
module(696, top2 + 40, 220, "OLED I2C 0x3C", [
    ("VCC", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("SDA", "G6", C["i2c"]), ("SCL", "G7", C["i2c"]),
])
t(696, top2 + 200, "opcional: sem OLED o node", 11, C["muted"]); t(696, top2 + 214, "funciona igual", 11, C["muted"])

o.insert(0, f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">'
            '<title>Communicator C3 Aguada — ligações</title>')
o.append("</svg>")
open(sys.argv[1], "w").write("\n".join(o))

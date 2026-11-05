# Gera o diagrama de ligações do Yeti (mesmo estilo do communicator_c3):
#   python3 yeti.py ../yeti_s3_ligacoes.svg s3
#   python3 yeti.py ../yeti_c3_ligacoes.svg c3
# Mudou um pino em include/yeti_config.h ou nos envs yeti_s3 / yeti_c3? Mude aqui e regere.
import sys
V = sys.argv[2] if len(sys.argv) > 2 else "s3"
# Pinos citados nos painéis, por placa.
P = dict(s3=dict(sck="G12", mosi="G11", dc="G10", bl="G2", bclk="G4", ws="G5", dout="G6",
                 din="G7", sda="G8", scl="G9", led="G43", vbat="G1", chip="ESP32-S3"),
         c3=dict(sck="G21", mosi="G20", dc="G0", bl="G8", bclk="G4", ws="G5", dout="G3",
                 din="G10", sda="G6", scl="G7", led="G2", vbat="G1", chip="ESP32-C3"))[V]
W, H = 960, 1440
C = dict(v5="#d6332a", gnd="#1c1f23", v33="#e0661a", bclk="#1f6fd1", ws="#7b4fc9",
         dout="#1a9a52", din="#0d8c8c", i2c="#8a5cb8", spi="#2b7bb9", dc="#5c7f99",
         bl="#c99a0e", touch="#c27c0e", led="#d1407a", adc="#6b8e23",
         off="#a3ada8", ink="#14201b", muted="#5a6b64", board="#1d3b5c", panel="#f3f6f4")
o = []
def t(x, y, s, size=13, fill=C["ink"], anchor="start", weight=400, mono=False):
    fam = "DejaVu Sans Mono,Consolas,monospace" if mono else "Arial,Helvetica,sans-serif"
    s = str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    o.append(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{weight}" fill="{fill}" text-anchor="{anchor}">{s}</text>')
def line(pts, col, w=3):
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    o.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/>')
def rect(x, y, w, h, fill, stroke="none", rx=6, sw=1.5):
    o.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')
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
def block(x, y, w, h, title, sub=""):
    rect(x, y, w, h, "#2a2f33", rx=5)
    t(x + w / 2, y + (h / 2 if not sub else h / 2 - 3), title, 12, "#fff", "middle", 700)
    if sub: t(x + w / 2, y + h / 2 + 13, sub, 10, "#cfd6d2", "middle")

o.append(f'<rect width="{W}" height="{H}" fill="#fbfcfb"/>')
t(24, 34, f"Yeti Aguada — ligações (env yeti_{V})", 21, weight=700)
t(24, 56, P["chip"] + " SuperMini visto por cima (componentes para cima, USB-C no topo). Confira a serigrafia da sua placa.", 12, C["muted"])

# ── placa ─────────────────────────────────────────────────────────────────────
bx, by, bw, bh = 400, 100, 170, 450
rect(bx + 55, by - 22, 60, 30, "#9aa3a8", rx=5); t(bx + 85, by - 2, "USB-C", 11, "#fff", "middle", 700)
rect(bx, by, bw, bh, C["board"], rx=10)
t(bx + bw / 2, by + 214, P["chip"], 15, "#fff", "middle", 700)
t(bx + bw / 2, by + 234, "SuperMini", 13, "#cfe0ee", "middle")
if V == "s3": t(bx + bw / 2, by + 262, "G48 = LED RGB", 10, "#cfe0ee", "middle")
rect(bx + 50, by + 380, 70, 46, "#2c557f", rx=4); t(bx + 85, by + 408, "antena", 11, "#cfe0ee", "middle")
if V == "s3":
    left  = ["G43", "G44", "G1", "G2", "G3", "G4", "G5", "G6", "G7"]
    right = ["5V", "GND", "3V3", "G13", "G12", "G11", "G10", "G9", "G8"]
else:
    left  = ["5V", "GND", "3V3", "G4", "G3", "G2", "G1", "G0"]
    right = ["G5", "G6", "G7", "G8", "G9", "G10", "G20", "G21"]
ys = [by + 40 + i * 46 for i in range(9)]
for name, y in zip(left, ys):
    o.append(f'<circle cx="{bx}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + 14, y + 5, name, 13, "#fff", weight=700, mono=True)
for name, y in zip(right, ys):
    o.append(f'<circle cx="{bx+bw}" cy="{y}" r="7" fill="#e8c46a" stroke="#8a6d1f" stroke-width="1.5"/>')
    t(bx + bw - 14, y + 5, name, 13, "#fff", "end", 700, True)

TAG_PWR = {
    "5V":  ("v5",    "5V ← chave ← TP4056 OUT+", "também: VIN do amp e 5V dos LEDs"),
    "GND": ("gnd",   "GND (comum a tudo)", "bateria − · todos os módulos"),
}
tags_l = {
    "G43": ("led",   "LEDs → WS2812 DIN (330 Ω)", "2 bochechas em cadeia · pisca no boot"),
    "G44": ("touch", "CABEÇA ← TTP223 I/O", "saída ativa alta"),
    "G1":  ("adc",   "VBAT ← divisor 100k/100k", "100 nF no G1"),
    "G2":  ("bl",    "BLK → backlight do display", "PWM"),
    "G3":  ("touch", "BRAÇO E ↔ fita de cobre", "touch nativo (TOUCH3)"),
    "G4":  ("bclk",  "BCLK → amp BCLK + 2 mics SCK", "I2S: um fio que se divide"),
    "G5":  ("ws",    "WS → amp LRC + 2 mics WS", "I2S: um fio que se divide"),
    "G6":  ("dout",  "DOUT → amp DIN", "MAX98357A"),
    "G7":  ("din",   "DIN ← SD dos 2 mics", "mesmo fio (canais L e R)"),
}
if V == "c3":
    tags_l = dict(TAG_PWR, **{
        "3V3": ("v33",   "3V3 → display · mics · sensores", "MPR121 · RC do RES · pull-up do G2"),
        "G4":  ("bclk",  "BCLK → amp BCLK + 2 mics SCK", "I2S: um fio que se divide"),
        "G3":  ("dout",  "DOUT → amp DIN", "MAX98357A"),
        "G2":  ("led",   "LEDs → WS2812 DIN (330 Ω)", "10k ao 3V3 (pino de boot)"),
        "G1":  ("adc",   "VBAT ← divisor 100k/100k", "100 nF no G1"),
        "G0":  ("dc",    "DC → display DC", ""),
    })
for name, y in zip(left, ys):
    k, a, b = tags_l[name]
    line([(bx - 7, y), (360, y)], C[k]); tag(24, y, 336, C[k], a, b)

tags_r = {
    "5V":  ("v5",    "5V ← chave ← TP4056 OUT+", "também: VIN do amp e 5V dos LEDs"),
    "GND": ("gnd",   "GND (comum a tudo)", "bateria − · todos os módulos"),
    "3V3": ("v33",   "3V3 → display · mics · sensores", "TTP223 · RC do RES do display"),
    "G13": ("touch", "BRAÇO D ↔ fita de cobre", "touch nativo (TOUCH13)"),
    "G12": ("spi",   "SCK → display SCL", "FSPI nativo · 40 MHz"),
    "G11": ("spi",   "MOSI → display SDA", ""),
    "G10": ("dc",    "DC → display DC", ""),
    "G9":  ("i2c",   "SCL → HTU21D · MPU6050 · BH1750", ""),
    "G8":  ("i2c",   "SDA ↔ HTU21D · MPU6050 · BH1750", "0x40 · 0x68 · 0x23"),
}
if V == "c3":
    tags_r = {
        "G5":  ("ws",    "WS → amp LRC + 2 mics WS", "I2S: um fio que se divide"),
        "G6":  ("i2c",   "SDA ↔ sensores + MPR121", "0x40 · 0x68 · 0x23 · 0x5A"),
        "G7":  ("i2c",   "SCL → sensores + MPR121", ""),
        "G8":  ("bl",    "BLK → backlight do display", "PWM · o LED da placa acompanha"),
        "G9":  ("off",   "livre: botão BOOT da placa", "não ligar nada (pino de boot)"),
        "G10": ("din",   "DIN ← SD dos 2 mics", "mesmo fio (canais L e R)"),
        "G20": ("spi",   "MOSI → display SDA", "40 MHz"),
        "G21": ("spi",   "SCK → display SCL", "pisca no boot (TX do ROM)"),
    }
for name, y in zip(right, ys):
    k, a, b = tags_r[name]
    line([(bx + bw + 7, y), (610, y)], C[k]); tag(610, y, 326, C[k], a, b)

# ── painéis: linha 1 ──────────────────────────────────────────────────────────
top = 590
panel(24, top, 296, 250, "1 · Display ST7789 1.3\" 240×240")
module(44, top + 40, 256, "módulo de 7 pinos (sem CS)", [
    ("GND", "GND", C["gnd"]), ("VCC", "3V3", C["v33"]), ("SCL", f"{P['sck']}  SCK", C["spi"]),
    ("SDA", f"{P['mosi']}  MOSI", C["spi"]), ("RES", "RC: 10k a 3V3 + 100 nF", C["v33"]),
    ("DC", P["dc"], C["dc"]), ("BLK", f"{P['bl']}  PWM", C["bl"]),
])
t(44, top + 238, "flex de lado · girar no firmware", 11, C["muted"])

panel(340, top, 296, 250, "2 · Microfones INMP441 ×2")
module(360, top + 40, 256, "esquerdo e direito", [
    ("VDD", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("SCK", f"{P['bclk']}  BCLK", C["bclk"]),
    ("WS", f"{P['ws']}  WS", C["ws"]), ("SD", f"{P['din']}  (mesmo fio)", C["din"]),
    ("L/R esq", "GND  → canal L", C["gnd"]), ("L/R dir", "3V3  → canal R", C["v33"]),
])
t(360, top + 238, "furos nas laterais do corpo · fios < 20 cm", 11, C["muted"])

panel(656, top, 280, 250, "3 · Falante MAX98357A")
module(676, top + 40, 244, "MAX98357A → falante", [
    ("VIN", "5V", C["v5"]), ("GND", "GND", C["gnd"]), ("SD", "3V3 (ou solto)", C["v33"]),
    ("GAIN", "solto (9 dB)", C["off"]), ("BCLK", P["bclk"], C["bclk"]), ("LRC", P["ws"], C["ws"]),
    ("DIN", P["dout"], C["dout"]),
])
t(676, top + 238, "falante 4–8 Ω nos bornes + / −", 11, C["muted"])

# ── painéis: linha 2 ──────────────────────────────────────────────────────────
top2 = top + 270
panel(24, top2, 296, 230, "4 · Sensores I2C (opcionais)")
module(44, top2 + 40, 256, "HTU21D · MPU6050 · BH1750", [
    ("VCC", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("SDA", P["sda"], C["i2c"]),
    ("SCL", P["scl"], C["i2c"]), ("ADDR", "BH1750: solto (0x23)", C["off"]),
    ("AD0", "MPU: solto (0x68)", C["off"]),
])
t(44, top2 + 212, "em paralelo no mesmo barramento", 11, C["muted"])

panel(340, top2, 296, 230, "5 · Toque: cabeça e braços")
if V == "s3":
    module(360, top2 + 40, 256, "TTP223 (topo da cabeça)", [
        ("VCC", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("I/O", "G44", C["touch"]),
    ])
    rect(360, top2 + 138, 256, 26, "#2a2f33", rx=5)
    t(488, top2 + 156, "fitas de cobre ~12×15 mm", 12, "#fff", "middle", 700)
    for i, (pin, lab) in enumerate([("G3", "braço ESQUERDO"), ("G13", "braço DIREITO")]):
        yy = top2 + 184 + i * 20
        t(368, yy, pin, 11, weight=700, mono=True)
        line([(434, yy - 4), (464, yy - 4)], C["touch"], 3)
        t(470, yy, lab, 11, C["touch"], weight=700)
else:
    module(360, top2 + 40, 256, "MPR121 (I2C 0x5A)", [
        ("VCC", "3V3", C["v33"]), ("GND", "GND", C["gnd"]), ("SDA", P["sda"], C["i2c"]),
        ("SCL", P["scl"], C["i2c"]), ("E0", "fita: topo da cabeça", C["touch"]),
        ("E1", "fita: braço ESQUERDO", C["touch"]), ("E2", "fita: braço DIREITO", C["touch"]),
    ])
t(360, top2 + 222, "lado = de quem olha o Yeti de frente", 11, C["muted"])

panel(656, top2, 280, 230, "6 · Bochechas WS2812 ×2")
module(676, top2 + 40, 244, "LED 1 → LED 2 (cadeia)", [
    ("5V", "5V  (100 µF perto)", C["v5"]), ("GND", "GND", C["gnd"]),
    ("DIN 1", f"330 Ω ← {P['led']}", C["led"]), ("DOUT 1", "DIN do LED 2", C["led"]),
])
t(676, top2 + 182, "dado em 3V3 funciona com fio curto", 11, C["muted"])
t(676, top2 + 198, "brilho máx. YETI_LED_MAX", 11, C["muted"])

# ── painel: energia ───────────────────────────────────────────────────────────
top3 = top2 + 250
panel(24, top3, 912, 230, "7 · Energia (base yeti_base_energia.stl)")
y = top3 + 70
block(44, y - 22, 150, 44, "jack DC / magnético", "5 V")
line([(194, y), (244, y)], C["v5"])
block(244, y - 22, 140, 44, "TP4056", "IN+ / IN−")
line([(314, y + 22), (314, y + 70)], C["v5"])
block(244, y + 70, 140, 40, "LiPo 1S", "B+ / B−")
line([(384, y), (444, y)], C["v5"]); t(414, y - 8, "OUT+", 10, C["muted"], "middle", 700)
block(444, y - 22, 90, 44, "chave", "liga/desl.")
line([(534, y), (600, y)], C["v5"])
block(600, y - 22, 150, 44, "pino 5V do " + V.upper(), "+ VIN amp · 5V LEDs")
# divisor da bateria
line([(567, y), (567, y + 34)], C["adc"])
rect(560, y + 34, 14, 26, "#fff", C["ink"], rx=2); t(580, y + 52, "100k", 11, weight=700)
line([(567, y + 60), (567, y + 76), (650, y + 76)], C["adc"])
rect(560, y + 76, 14, 26, "#fff", C["ink"], rx=2); t(580, y + 96, "100k → GND", 11, weight=700)
t(656, y + 80, f"→ {P['vbat']} (VBAT) + 100 nF ao GND", 11, C["adc"], weight=700)
t(770, y - 4, "Com 2×AA / 3×AAA:", 11, C["muted"], weight=700)
t(770, y + 12, "MT3608 em 5 V entre", 11, C["muted"])
t(770, y + 28, "a chave e o 5V.", 11, C["muted"])
t(44, top3 + 216, "GND de tudo em comum. Fios sobem pelos furos da peana e pelas pernas ocas do Yeti.", 11, C["muted"])

o.insert(0, f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">'
            f'<title>Yeti Aguada {V.upper()} — ligações</title>')
o.append("</svg>")
open(sys.argv[1], "w").write("\n".join(o))

"""Gera a página de ligações do AirQ (SVG inline calculado) — saída: airq_wiring.html"""
import base64, html

# ── primitivas SVG ────────────────────────────────────────────────────────────
def esc(s): return html.escape(str(s))

def wire(pts, cls, title=None):
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    t = f"<title>{esc(title)}</title>" if title else ""
    return (f'<g class="w {cls}">{t}<path class="halo" d="{d}"/>'
            f'<path class="core" d="{d}"/></g>')

def dot(x, y): return f'<circle class="node" cx="{x}" cy="{y}" r="4.5"/>'

def pad(x, y, ring="pad"):
    return f'<circle class="{ring}" cx="{x}" cy="{y}" r="8.5"/><circle class="hole" cx="{x}" cy="{y}" r="3.6"/>'

def text(x, y, s, cls="lbl", anchor="start"):
    return f'<text class="{cls}" x="{x}" y="{y}" text-anchor="{anchor}">{esc(s)}</text>'

def flag(x, y, net, side="up"):
    """Etiqueta de rede (3V3 / GND) ligada por um toco curto."""
    cls = "f3v3" if net == "3V3" else "fgnd"
    if side == "up":
        y2 = y - 22
        return (wire([(x, y), (x, y2)], "w3v3" if net == "3V3" else "wgnd") +
                f'<g class="flag {cls}"><rect x="{x-20}" y="{y2-17}" width="40" height="17" rx="3"/>'
                + text(x, y2 - 4.5, net, "flagt", "middle") + "</g>")
    if side == "down":
        y2 = y + 22
        return (wire([(x, y), (x, y2)], "w3v3" if net == "3V3" else "wgnd") +
                f'<g class="flag {cls}"><rect x="{x-20}" y="{y2}" width="40" height="17" rx="3"/>'
                + text(x, y2 + 12.5, net, "flagt", "middle") + "</g>")
    if side == "left":
        x2 = x - 22
        return (wire([(x, y), (x2, y)], "w3v3" if net == "3V3" else "wgnd") +
                f'<g class="flag {cls}"><rect x="{x2-40}" y="{y-8.5}" width="40" height="17" rx="3"/>'
                + text(x2 - 20, y + 4, net, "flagt", "middle") + "</g>")
    x2 = x + 22
    return (wire([(x, y), (x2, y)], "w3v3" if net == "3V3" else "wgnd") +
            f'<g class="flag {cls}"><rect x="{x2}" y="{y-8.5}" width="40" height="17" rx="3"/>'
            + text(x2 + 20, y + 4, net, "flagt", "middle") + "</g>")

def resistor_v(x, y1, y2, label, left=False):
    """Resistor vertical entre y1 e y2 (corpo no meio)."""
    m = (y1 + y2) / 2
    return (wire([(x, y1), (x, m - 16)], "wpas") + wire([(x, m + 16), (x, y2)], "wpas") +
            f'<rect class="res" x="{x-7}" y="{m-16}" width="14" height="32" rx="3"/>' +
            (text(x - 12, m + 4, label, "small", "end") if left else text(x + 12, m + 4, label, "small")))

def resistor_h(x1, x2, y, label):
    m = (x1 + x2) / 2
    return (wire([(x1, y), (m - 16, y)], "wpas") + wire([(m + 16, y), (x2, y)], "wpas") +
            f'<rect class="res" x="{m-16}" y="{y-7}" width="32" height="14" rx="3"/>' +
            text(m, y - 12, label, "small", "middle"))

# ── ESP32-C3 SuperMini (vista do lado da serigrafia, como na foto) ────────────
C3_LEFT  = ["5V", "G", "3.3", "4", "3", "2", "1", "0"]
C3_RIGHT = ["5", "6", "7", "8", "9", "10", "20", "21"]

def c3(x0, y0, used=(), label_top=False):
    """Desenha a placa; devolve (svg, pinos{nome:(x,y)}). x0,y0 = canto sup. esq."""
    w, h, step = 180, 330, 36
    xl, xr = x0 + 20, x0 + w - 20
    pins, s = {}, []
    s.append(f'<rect class="usb" x="{x0+60}" y="{y0-14}" width="60" height="22" rx="6"/>')
    s.append(f'<rect class="c3pcb" x="{x0}" y="{y0}" width="{w}" height="{h}" rx="10"/>')
    s.append(f'<rect class="silk" x="{x0+60}" y="{y0+110}" width="60" height="92" rx="3"/>')
    for i, t in enumerate(["TENSTAR", "ESP32-C3", "Super Mini"]):
        s.append(text(x0 + 90, y0 + 136 + i * 26, t, "silkt", "middle"))
    for i, (a, b) in enumerate(zip(C3_LEFT, C3_RIGHT)):
        y = y0 + 40 + i * step
        for name, x, anchor, dx in ((a, xl, "start", 15), (b, xr, "end", -15)):
            key = name if name in ("5V", "G", "3.3") else f"GPIO{name}"
            pins[key] = (x, y)
            s.append(pad(x, y, "pad used" if key in used else "pad"))
            s.append(text(x + dx, y + 4.5, name, "pint", anchor))
    s.append(text(x0 + w / 2, (y0 - 26) if label_top else (y0 + h + 22), "ESP32-C3 SuperMini", "part", "middle"))
    return "".join(s), pins

# ── Placa ENS160 + AHT21 (ordem dos pinos como na foto) ───────────────────────
ENS_PINS = ["INT", "CS", "ADD", "SDA", "SCL", "GND", "3V3", "VIN"]

def ens(x0, y0):
    w, h, step = 280, 150, 32
    s, pins = [], {}
    s.append(f'<rect class="enspcb" x="{x0}" y="{y0}" width="{w}" height="{h}" rx="8"/>')
    s.append(f'<circle class="mhole" cx="{x0+w-22}" cy="{y0+h-22}" r="9"/>')
    # chips
    s.append(f'<rect class="chipw" x="{x0+40}" y="{y0+70}" width="34" height="30" rx="4"/>')
    s.append(text(x0 + 57, y0 + 118, "AHT21", "chipt", "middle"))
    s.append(f'<rect class="chips" x="{x0+150}" y="{y0+74}" width="26" height="26" rx="3"/>')
    s.append(text(x0 + 163, y0 + 118, "ENS160", "chipt", "middle"))
    s.append(f'<path class="heat" d="M{x0+82},{y0+86} q16,-10 32,0 q16,10 32,0"/>')
    s.append(text(x0 + 113, y0 + 140, "calor do ENS160 → AHT21 lê alto", "chipt2", "middle"))
    for i, n in enumerate(ENS_PINS):
        x, y = x0 + 26 + i * step, y0 + 22
        pins[n] = (x, y)
        s.append(pad(x, y, "pad used" if n not in ("INT", "3V3") else "pad"))
        s.append(text(x, y + 26, n, "pint", "middle"))
    s.append(text(x0 + w / 2, y0 + h + 22, "ENS160 + AHT21", "part", "middle"))
    return "".join(s), pins

# ── INMP441 (redondo, 2 colunas como na foto) ─────────────────────────────────
def inmp(cx, cy):
    s, pins = [], {}
    s.append(f'<circle class="mring" cx="{cx}" cy="{cy}" r="82"/>')
    s.append(f'<circle class="mpcb" cx="{cx}" cy="{cy}" r="74"/>')
    for i, (l, r) in enumerate([("SCK", "SD"), ("WS", "VDD"), ("L/R", "GND")]):
        y = cy - 34 + i * 34
        for name, x, anchor, dx in ((l, cx - 30, "end", -14), (r, cx + 30, "start", 14)):
            pins[name] = (x, y)
            s.append(pad(x, y, "pad used"))
            s.append(text(x + dx, y + 4.5, name, "pinw", anchor))
    s.append(text(cx, cy + 108, "INMP441", "part", "middle"))
    return "".join(s), pins

# ══ DIAGRAMA 1 — caixa central (o que já tem na bancada) ═════════════════════
def diagram_core():
    W, H = 960, 640
    out = []
    c3svg, P = c3(70, 150, used={"3.3", "G", "GPIO5", "GPIO6", "GPIO7", "GPIO20", "GPIO21"})
    enssvg, E = ens(470, 150)
    misvg, M = inmp(690, 500)
    R3, RG = 34, 58          # barramentos 3V3 / GND (y)
    # barramentos
    out.append(wire([(20, R3), (W - 20, R3)], "w3v3 rail", "barramento 3V3"))
    out.append(wire([(40, RG), (W - 20, RG)], "wgnd rail", "barramento GND"))
    out.append(text(W - 24, R3 - 9, "3V3", "railt w3t", "end"))
    out.append(text(W - 90, RG + 20, "GND", "railt wgt", "end"))
    # C3 → barramentos
    x, y = P["3.3"]; out.append(wire([(x, y), (32, y), (32, R3)], "w3v3", "3.3 V do C3"))
    x, y = P["G"];   out.append(wire([(x, y), (46, y), (46, RG)], "wgnd", "GND do C3"))
    out.append(dot(32, R3) + dot(46, RG))
    # ENS: alimentação
    x, y = E["VIN"]; out.append(wire([(x, R3), (x, y)], "w3v3", "3V3 → VIN da placa (regulador onboard)")); out.append(dot(x, R3))
    x, y = E["GND"]; out.append(wire([(x, RG), (x, y)], "wgnd", "GND → ENS160/AHT21")); out.append(dot(x, RG))
    # ENS: CS→3V3, ADD→GND como etiquetas (ficam curtos, não cruzam o I2C)
    # I2C por cima
    gx, gy = P["GPIO6"]; sx, sy = E["SDA"]
    out.append(wire([(gx, gy), (300, gy), (300, 104), (sx, 104), (sx, sy)], "wsda", "SDA: GPIO6 → SDA"))
    gx, gy = P["GPIO7"]; sx, sy = E["SCL"]
    out.append(wire([(gx, gy), (318, gy), (318, 118), (sx, 118), (sx, sy)], "wscl", "SCL: GPIO7 → SCL"))
    # INMP441 sinais
    gx, gy = P["GPIO20"]; mx, my = M["SCK"]
    out.append(wire([(gx, gy), (520, gy), (520, my), (mx, my)], "wsck", "SCK: GPIO20 → SCK"))
    gx, gy = P["GPIO21"]; mx, my = M["WS"]
    out.append(wire([(gx, gy), (500, gy), (500, my), (mx, my)], "wws", "WS: GPIO21 → WS"))
    gx, gy = P["GPIO5"]; mx, my = M["SD"]
    out.append(wire([(gx, gy), (282, gy), (282, 352), (560, 352), (560, 400),
                     (mx + 40, 400), (mx + 40, my), (mx, my)], "wsd", "SD: GPIO5 → SD"))
    # INMP alimentação para os barramentos pela direita
    mx, my = M["VDD"]; out.append(wire([(mx, my), (900, my), (900, R3)], "w3v3", "3V3 → INMP441")); out.append(dot(900, R3))
    mx, my = M["GND"]; out.append(wire([(mx, my), (920, my), (920, RG)], "wgnd", "GND → INMP441")); out.append(dot(920, RG))
    mx, my = M["L/R"]; out.append(wire([(mx, my), (mx - 30, my), (mx - 30, my + 46)], "wgnd", "L/R → GND"))
    out.append(f'<g class="flag fgnd"><rect x="{mx-50}" y="{my+46}" width="40" height="17" rx="3"/>'
               + text(mx - 30, my + 58.5, "GND", "flagt", "middle") + "</g>")
    body = "".join(out) + c3svg + enssvg + misvg
    # CS / ADD — jumpers na própria placa (na bancada: CS→VIN azul, ADD→GND preto)
    def mini(x, y, net):
        c = "f3v3" if net == "3V3" else "fgnd"
        return (f'<path class="jump {c}" d="M{x},{y} v-18"/>'
                f'<g class="flag {c}"><rect x="{x-15}" y="{y-35}" width="30" height="17" rx="3"/>'
                + text(x, y - 22.5, net, "flagt", "middle") + "</g>")
    body += mini(*E["CS"], "3V3") + mini(*E["ADD"], "GND")
    body += text(E["INT"][0], E["INT"][1] - 16, "NC", "nc", "middle")
    body += text(E["3V3"][0], E["3V3"][1] - 16, "NC", "nc", "middle")
    return f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Ligações da caixa central">{body}</svg>'

def capacitor_v(x, y1, y2, label):
    m = (y1 + y2) / 2
    return (wire([(x, y1), (x, m - 5)], "wpas") + wire([(x, m + 5), (x, y2)], "wpas") +
            f'<path class="cap" d="M{x-12},{m-5} h24 M{x-12},{m+5} h24"/>' + text(x + 16, m + 4, label, "small"))

def connector(x0, y0, name):
    """Conector de painel (M8 / RJ11) de 3 vias: VDD em cima, DQ ao lado, GND em baixo."""
    return (f'<rect class="conn" x="{x0}" y="{y0}" width="50" height="56" rx="6"/>'
            + text(x0 + 25, y0 + 33, name, "conntext", "middle"))

def probe(x1, x2, y, label, flip=False):
    """Cabo + cápsula inox do DS18B20 entre x1 e x2 (cápsula na ponta x2)."""
    cap_x = x2 - 70 if not flip else x2
    s = wire([(x1, y), (x2 if flip else x2 - 70, y)], "wcable")
    s += f'<rect class="probe" x="{cap_x}" y="{y-9}" width="70" height="18" rx="9"/>'
    s += (text(x2, y - 18, label, "small", "start") if flip else text(x2, y - 18, label, "small", "end"))
    return s

# ══ DIAGRAMA 2 — portas opcionais do analisador de AC ════════════════════════
def diagram_ac():
    W, H = 960, 600
    out, top = [], []
    c3svg, P = c3(380, 110, used={"3.3", "G", "GPIO1", "GPIO3", "GPIO4", "GPIO6", "GPIO7"}, label_top=True)
    # alimentação do C3 como etiquetas de rede
    top.append(flag(*P["3.3"], "3V3", "left") + flag(*P["G"], "GND", "left"))

    # ── Sonda EXT (GPIO4) ────────────────────────────────────────────────
    gx, gy = P["GPIO4"]
    out.append(wire([(gx, gy), (250, gy)], "wdq", "DQ sonda EXT → GPIO4"))
    out.append(dot(310, gy) + resistor_v(310, gy, 196, "4k7", left=True) + flag(310, 196, "3V3", "up"))
    top.append(connector(200, 230, "EXT"))
    top.append(flag(225, 230, "3V3", "up") + flag(225, 286, "GND", "down"))
    top.append(probe(200, 30, gy, "sonda EXT · condensadora", flip=True))
    # ── TEMT6000 (GPIO3) ─────────────────────────────────────────────────
    gx, gy = P["GPIO3"]
    out.append(wire([(gx, gy), (334, gy)], "wlux", "SIG TEMT6000 → GPIO3"))
    top.append('<rect class="temtpcb" x="258" y="276" width="82" height="60" rx="6"/>'
               '<circle class="lsens" cx="286" cy="300" r="7"/>')
    top.append(pad(334, gy, "pad used") + text(322, gy + 4, "SIG", "pinw", "end"))
    top.append(flag(275, 336, "3V3", "down") + flag(318, 336, "GND", "down"))
    top.append(text(299, 329, "TEMT6000", "chipt", "middle"))
    # ── TC SCT-013 (GPIO1) ───────────────────────────────────────────────
    gx, gy = P["GPIO1"]
    out.append(wire([(gx, gy), (372, gy), (372, 430), (250, 430)], "wct", "ponta (tip) do jack → GPIO1"))
    top.append('<rect class="jack" x="180" y="405" width="70" height="70" rx="6"/>')
    top.append(text(215, 398, "jack P2 c/ chave", "small", "middle"))
    top.append(text(244, 434, "T", "pinw", "end") + text(244, 458, "S", "pinw", "end") + text(215, 468, "SW", "pinw", "middle"))
    out.append(wire([(250, 454), (410, 454)], "wpas", "anel (sleeve) → bias"))
    out.append(dot(300, 454) + dot(340, 454))
    out.append(resistor_v(300, 454, 536, "6k8", left=True) + flag(300, 536, "GND", "down"))
    out.append(capacitor_v(340, 454, 536, "10µF") + flag(340, 536, "GND", "down"))
    out.append(text(375, 478, "10k", "small", "middle"))
    out.append(f'<rect class="res" x="359" y="447" width="32" height="14" rx="3"/>')
    top.append(flag(410, 454, "3V3", "down"))
    top.append(flag(215, 475, "GND", "down"))
    top.append(text(300, 588, "bias ≈ 1,34 V", "small", "middle"))
    # garra no condutor de fase
    top.append('<path class="phase" d="M30,440 H150"/>'
               '<ellipse class="clamp" cx="88" cy="440" rx="26" ry="34"/>')
    out.append(wire([(112, 452), (150, 452), (150, 440), (180, 440)], "wcable"))
    top.append(text(88, 496, "SCT-013-030", "small", "middle"))
    top.append(text(88, 512, "1 condutor de fase", "small", "middle"))

    # ── SCD41 (I2C 6/7) ──────────────────────────────────────────────────
    sx0, sy0 = 640, 140
    top.append(f'<rect class="scdpcb" x="{sx0}" y="{sy0}" width="150" height="124" rx="8"/>'
               f'<rect class="chips" x="{sx0+80}" y="{sy0+42}" width="40" height="40" rx="4"/>')
    top.append(text(sx0 + 75, sy0 - 12, "SCD41 · CO₂ NDIR", "part sm", "middle"))
    for n, y in (("VDD", 158), ("SDA", 186), ("SCL", 222), ("GND", 250)):
        top.append(pad(sx0 + 16, y, "pad used") + text(sx0 + 30, y + 4.5, n, "pinw"))
    gx, gy = P["GPIO6"]; out.append(wire([(gx, gy), (sx0 + 16, 186)], "wsda", "SDA: GPIO6 → SCD41"))
    gx, gy = P["GPIO7"]; out.append(wire([(gx, gy), (sx0 + 16, 222)], "wscl", "SCL: GPIO7 → SCD41"))
    top.append(flag(sx0 + 16, 158, "3V3", "left") + flag(sx0 + 16, 250, "GND", "left"))
    top.append(text(sx0 + 75, sy0 - 34, "mesmo I2C do ENS160/AHT21", "small", "middle"))
    # ── Sonda SUP — MESMO barramento 1-Wire da EXT (GPIO4) ─────────────────
    # DQ do conector SUP volta por cima da placa até o nó do DQ da EXT; 1 pull-up só.
    gy4 = P["GPIO4"][1]
    out.append(wire([(700, 498), (600, 498), (600, 60), (264, 60), (264, gy4)], "wdq",
                    "DQ sondas SUP e RET → mesmo fio da EXT (GPIO4)"))
    out.append(wire([(700, 338), (600, 338)], "wdq", "DQ sonda SUP → barramento"))
    out.append(dot(264, gy4) + dot(600, 338))
    top.append(text(440, 50, "EXT + SUP + RET no mesmo DQ · 1 pull-up 4k7 · papel pelo ID ROM", "small", "middle"))
    top.append(connector(700, 310, "SUP"))
    top.append(flag(725, 310, "3V3", "up") + flag(725, 366, "GND", "down"))
    top.append(probe(750, 930, 338, "sonda SUP · insuflamento"))
    top.append(connector(700, 470, "RET"))
    top.append(flag(725, 470, "3V3", "up") + flag(725, 526, "GND", "down"))
    top.append(probe(750, 930, 498, "sonda RET · retorno"))

    body = "".join(out) + c3svg + "".join(top)
    return f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Ligações das portas opcionais do analisador de AC">{body}</svg>'

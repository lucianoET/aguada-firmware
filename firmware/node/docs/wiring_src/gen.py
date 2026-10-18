"""Gera a página de ligações do AirQ (SVG inline calculado) — saída: airq_wiring.html"""
import base64, html

# ── primitivas SVG ────────────────────────────────────────────────────────────
def esc(s): return html.escape(str(s))

def wire(pts, cls, title=None, halo=True):
    """halo=False: sem contorno (trechos do mesmo fio que se emendam não abrem falha)."""
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    t = f"<title>{esc(title)}</title>" if title else ""
    h = f'<path class="halo" d="{d}"/>' if halo else ""
    return f'<g class="w {cls}">{t}{h}<path class="core" d="{d}"/></g>'

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


# ── primitivas de alimentação ────────────────────────────────────────────────
def switch_h(x1, x2, y, cls, title=None):
    """Chave liga/desliga (SPST) horizontal entre x1 e x2, desenhada aberta."""
    a, b = x1 + 14, x2 - 14
    return (wire([(x1, y), (a, y)], cls, title, False) + wire([(b, y), (x2, y)], cls, title, False) +
            f'<path class="swl" d="M{a},{y} L{b - 2},{y - 22}"/>' +
            f'<circle class="swd" cx="{a}" cy="{y}" r="5"/><circle class="swd" cx="{b}" cy="{y}" r="5"/>')

def diode_h(x1, x2, y, cls, title=None):
    """Diodo horizontal, anodo à esquerda (conduz →)."""
    m = (x1 + x2) / 2
    return (wire([(x1, y), (m - 12, y)], cls, title, False) + wire([(m + 11, y), (x2, y)], cls, title, False) +
            f'<polygon class="dio" points="{m-12},{y-11} {m-12},{y+11} {m+10},{y}"/>' +
            f'<path class="diol" d="M{m+11},{y-12} L{m+11},{y+12}"/>')

def cell_v(x, ytop, ybot, label):
    """Célula Li-ion vertical (+ em cima)."""
    return (f'<rect class="cellcap" x="{x-9}" y="{ytop-9}" width="18" height="10" rx="2"/>'
            f'<rect class="cell" x="{x-26}" y="{ytop}" width="52" height="{ybot-ytop}" rx="7"/>' +
            text(x, ytop + 24, "+", "cellt", "middle") + text(x, ybot - 12, "−", "cellt", "middle") +
            text(x, (ytop + ybot) / 2 + 4, "3,7 V", "cellt", "middle") +
            text(x, ytop - 54, label, "small", "middle"))

# ══ DIAGRAMA 3 — alimentação por bateria ══════════════════════════════════════
def diagram_power():
    W, H = 960, 560
    out, top = [], []
    c3svg, P = c3(600, 110, used={"5V", "G", "3.3", "GPIO0"}, label_top=True)

    # ── bateria Li-ion → TP4056 (B+ / B-) ────────────────────────────────
    top.append(cell_v(85, 150, 290, "Li-ion 18650"))
    out.append(wire([(85, 141), (85, 112), (150, 112), (150, 150), (196, 150)], "wbat", "bateria + → B+", False))
    out.append(wire([(85, 290), (85, 312), (150, 312), (150, 270), (196, 270)], "wgnd", "bateria − → B−", False))
    # módulo TP4056 com proteção (DW01 + 8205A)
    top.append('<rect class="usb" x="230" y="100" width="60" height="22" rx="6"/>')
    top.append(text(260, 92, "USB-C · carga 5 V", "small", "middle"))
    top.append('<rect class="enspcb" x="180" y="118" width="160" height="182" rx="8"/>')
    top.append('<rect class="chips" x="238" y="182" width="44" height="40" rx="4"/>')
    top.append(text(260, 240, "TP4056", "chipt", "middle"))
    top.append(text(260, 255, "+ DW01 (proteção)", "chipt", "middle"))
    for name, x, y, anchor, dx in (("B+", 196, 150, "start", 14), ("B−", 196, 270, "start", 14),
                                   ("OUT+", 324, 150, "end", -14), ("OUT−", 324, 270, "end", -14)):
        top.append(pad(x, y, "pad used") + text(x + dx, y + 4.5, name, "pinw", anchor))
    top.append(text(260, 324, "módulo carregador TP4056", "part sm", "middle"))
    top.append(text(260, 342, "use a versão COM proteção (6 pinos)", "small", "middle"))
    out.append(wire([(324, 270), (366, 270)], "wgnd", None, False))
    top.append(flag(366, 270, "GND", "down"))

    # ── SW1 liga/desliga → nó VBAT → D1 → 5V do C3 ───────────────────────
    out.append(wire([(324, 150), (380, 150)], "wbat", "OUT+ → SW1", False))
    out.append(switch_h(380, 444, 150, "wbat", "SW1 liga/desliga"))
    top.append(text(412, 116, "SW1 · liga/desliga", "small", "middle"))
    out.append(wire([(444, 150), (500, 150)], "wbat", "VBAT após SW1", False))
    out.append(dot(470, 150))
    out.append(diode_h(500, 560, 150, "wbat", "D1 Schottky → pino 5V"))
    top.append(text(530, 124, "D1 · SS34", "small", "middle"))
    x5, y5 = P["5V"]
    out.append(wire([(560, 150), (x5, y5)], "wbat", "→ 5V do C3 (LDO onboard → 3,3 V)", False))

    # ── divisor VBAT/2 → GPIO0 (ADC1) ─────────────────────────────────────
    gx, gy = P["GPIO0"]
    out.append(resistor_v(470, 150, gy, "100k", left=True))
    out.append(dot(470, gy))
    out.append(wire([(470, gy), (gx, gy)], "wpas", "VBAT/2 → GPIO0 (ADC1)", False))
    out.append(resistor_v(470, gy, 470, "100k", left=True))
    top.append(flag(470, 470, "GND", "down"))
    top.append(text(486, gy - 10, "VBAT/2", "small", "start"))

    # ── GND do C3 ────────────────────────────────────────────────────────
    top.append(flag(*P["G"], "GND", "left"))

    # ── 3.3 do C3 → SW2 → 3V3 dos sensores ───────────────────────────────
    x3, y3 = P["3.3"]
    out.append(wire([(x3, y3), (586, y3), (586, 500), (700, 500)], "w3v3", "3,3 V do C3 → SW2"))
    out.append(switch_h(700, 764, 500, "w3v3", "SW2 corta o 3V3 dos sensores"))
    top.append(text(732, 536, "SW2 · sensores (tracejado = 3V3 comutado)", "small", "middle"))
    out.append(wire([(764, 500), (870, 500), (870, 452)], "w3v3 sw3", "3V3 comutado → sensores", False))
    # caixa com os consumidores do 3V3 comutado
    top.append('<rect class="conn" x="796" y="196" width="150" height="256" rx="8"/>')
    top.append('<g class="flag f3v3"><rect x="810" y="208" width="122" height="20" rx="3"/>'
               + text(871, 222.5, "3V3 SENSORES", "flagt", "middle") + "</g>")
    for i, s in enumerate(["ENS160+AHT21 VIN", "INMP441 VDD", "SCD41 VDD", "sondas + 4k7",
                           "TEMT6000", "bias do TC (10k)"]):
        top.append(text(808, 254 + i * 30, "· " + s, "pinw", "start"))
    top.append(text(600, 56, "D1 impede o 5 V do USB do C3 de voltar à bateria", "small", "start"))

    body = "".join(out) + c3svg + "".join(top)
    return f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Alimentação por bateria: Li-ion, TP4056, chaves e divisor de tensão">{body}</svg>'


# ── etiqueta de rede genérica (VBAT, 3V3S, SDA, SCL…) ─────────────────────────
NET = {"3V3S": ("f3v3s", "w3v3 sw3"), "VBAT": ("fvbat", "wbat"), "GND": ("fgnd", "wgnd"),
       "SDA": ("fsda", "wsda"), "SCL": ("fscl", "wscl"), "3V3": ("f3v3", "w3v3")}

def netflag(x, y, net, side="up", stub=22):
    """Etiqueta de rede: o mesmo nome em dois sítios = mesmo fio."""
    cls, wcls = NET[net]
    w = max(40, 9 * len(net) + 14)
    if side == "up":
        y2 = y - stub; r = (x - w / 2, y2 - 17); t = (x, y2 - 4.5); s = [(x, y), (x, y2)]
    elif side == "down":
        y2 = y + stub; r = (x - w / 2, y2); t = (x, y2 + 12.5); s = [(x, y), (x, y2)]
    elif side == "left":
        x2 = x - stub; r = (x2 - w, y - 8.5); t = (x2 - w / 2, y + 4); s = [(x, y), (x2, y)]
    else:
        x2 = x + stub; r = (x2, y - 8.5); t = (x2 + w / 2, y + 4); s = [(x, y), (x2, y)]
    return (wire(s, wcls, None, False) +
            f'<g class="flag {cls}"><rect x="{r[0]}" y="{r[1]}" width="{w}" height="17" rx="3"/>'
            + text(t[0], t[1], net, "flagt", "middle") + "</g>")

# ══ DIAGRAMA 4 — sistema completo (bateria + C3 + todos os sensores) ═══════════
def diagram_full():
    W, H = 1200, 1060
    out, top = [], []
    used = {"5V", "G", "3.3", "GPIO0", "GPIO1", "GPIO3", "GPIO4", "GPIO5", "GPIO6", "GPIO7", "GPIO20", "GPIO21"}
    c3svg, P = c3(560, 250, used=used, label_top=True)

    # ── alimentação: Li-ion → TP4056 → SW1 → (VBAT) → D1 → 5V ────────────
    top.append(cell_v(70, 270, 410, "Li-ion 18650"))
    out.append(wire([(70, 261), (70, 236), (125, 236), (125, 290), (166, 290)], "wbat", "bateria + → B+", False))
    out.append(wire([(70, 410), (70, 432), (125, 432), (125, 370), (166, 370)], "wgnd", "bateria − → B−", False))
    top.append('<rect class="usb" x="195" y="240" width="60" height="22" rx="6"/>')
    top.append(text(225, 230, "USB-C · carga", "small", "middle"))
    top.append('<rect class="enspcb" x="150" y="256" width="150" height="152" rx="8"/>')
    top.append('<rect class="chips" x="203" y="306" width="44" height="34" rx="4"/>')
    top.append(text(225, 356, "TP4056", "chipt", "middle") + text(225, 370, "+ DW01", "chipt", "middle"))
    for name, x, y, anchor, dx in (("B+", 166, 290, "start", 14), ("B−", 166, 370, "start", 14),
                                   ("OUT+", 284, 290, "end", -14), ("OUT−", 284, 370, "end", -14)):
        top.append(pad(x, y, "pad used") + text(x + dx, y + 4.5, name, "pinw", anchor))
    top.append(text(225, 452, "carregador TP4056 c/ proteção", "small", "middle"))
    out.append(wire([(284, 370), (310, 370)], "wgnd", None, False))
    top.append(netflag(310, 370, "GND", "down"))
    out.append(wire([(284, 290), (300, 290)], "wbat", None, False))
    out.append(switch_h(300, 356, 290, "wbat", "SW1 liga/desliga"))
    top.append(text(328, 318, "SW1", "small", "middle"))
    out.append(wire([(356, 290), (420, 290)], "wbat", None, False))
    out.append(dot(380, 290))
    top.append(netflag(380, 290, "VBAT", "up"))
    out.append(diode_h(420, 476, 290, "wbat", "D1 Schottky"))
    top.append(text(448, 318, "D1 SS34", "small", "middle"))
    out.append(wire([(476, 290), P["5V"]], "wbat", "→ 5V do C3", False))
    top.append(netflag(*P["G"], "GND", "left"))

    # ── 3.3 do C3 → SW2 → rede 3V3S (todos os sensores) ──────────────────
    x3, y3 = P["3.3"]
    out.append(wire([(x3, y3), (546, y3)], "w3v3", None, False))
    out.append(switch_h(490, 546, y3, "w3v3", "SW2 corta o 3V3 dos sensores"))
    out.append(wire([(490, y3), (470, y3)], "w3v3 sw3", None, False))
    top.append(netflag(470, y3, "3V3S", "left"))
    top.append(text(518, y3 + 24, "SW2", "small", "middle"))

    # ── GPIO4: barramento 1-Wire das 3 sondas ────────────────────────────
    gx, gy = P["GPIO4"]
    out.append(wire([(gx, gy), (480, gy), (480, 690), (100, 690), (100, 733)], "wdq", "DQ sondas → GPIO4"))
    for cx in (220, 340):
        out.append(wire([(cx, 690), (cx, 733)], "wdq") + dot(cx, 690))
    out.append(dot(420, 690) + resistor_v(420, 610, 690, "4k7", left=True))
    top.append(netflag(420, 610, "3V3S", "up"))
    for cx, name, what in ((100, "EXT", "condensadora"), (220, "SUP", "insuflamento"), (340, "RET", "retorno")):
        out.append(wire([(cx, 750), (cx, 812)], "wcable"))
        top.append(f'<rect class="conn" x="{cx-48}" y="{716}" width="96" height="34" rx="6"/>')
        top.append(pad(cx - 30, 733, "pad used") + pad(cx, 733, "pad used") + pad(cx + 30, 733, "pad used"))
        top.append(netflag(cx - 30, 741.5, "3V3S", "down") + netflag(cx + 30, 741.5, "GND", "down"))
        top.append(f'<rect class="probe" x="{cx-9}" y="812" width="18" height="56" rx="9"/>')
        top.append(text(cx, 888, f"sonda {name}", "small", "middle") + text(cx, 903, what, "small", "middle"))

    # ── GPIO3: TEMT6000 ──────────────────────────────────────────────────
    gx, gy = P["GPIO3"]
    out.append(wire([(gx, gy), (500, gy), (500, 930), (462, 930)], "wlux", "SIG TEMT6000 → GPIO3"))
    top.append('<rect class="temtpcb" x="378" y="900" width="92" height="60" rx="6"/>'
               '<circle class="lsens" cx="402" cy="922" r="7"/>')
    top.append(pad(462, 930, "pad used") + text(450, 934, "SIG", "pinw", "end"))
    top.append(text(420, 954, "TEMT6000", "chipt", "middle"))
    top.append(netflag(392, 960, "3V3S", "down") + netflag(444, 960, "GND", "down"))

    # ── GPIO1: TC SCT-013 no jack P2 + bias ──────────────────────────────
    gx, gy = P["GPIO1"]
    out.append(wire([(gx, gy), (520, gy), (520, 898), (572, 898)], "wct", "ponta (tip) do jack → GPIO1"))
    top.append('<rect class="jack" x="560" y="880" width="70" height="64" rx="6"/>')
    top.append(pad(572, 898, "pad used") + pad(618, 926, "pad used") + pad(595, 944, "pad used"))
    top.append(text(586, 902, "T", "pinw", "start") + text(604, 930, "S", "pinw", "end"))
    top.append(text(600, 1004, "jack P2 · garra SCT-013", "small", "middle"))
    top.append(netflag(595, 952.5, "GND", "down"))
    out.append(wire([(618, 926), (730, 926)], "wpas", "anel (sleeve) → bias"))
    out.append(dot(680, 926))
    out.append(resistor_v(680, 846, 926, "10k", left=True))
    top.append(netflag(680, 846, "3V3S", "up"))
    out.append(resistor_v(680, 926, 1000, "6k8", left=True))
    top.append(netflag(680, 1000, "GND", "down"))
    out.append(capacitor_v(730, 926, 1000, "10µF"))
    top.append(netflag(730, 1000, "GND", "down"))

    # ── GPIO0: divisor VBAT/2 ────────────────────────────────────────────
    gx, gy = P["GPIO0"]
    out.append(wire([(gx, gy), (540, gy), (540, 700), (780, 700)], "wpas", "VBAT/2 → GPIO0 (ADC1)"))
    out.append(dot(780, 700))
    out.append(resistor_v(780, 640, 700, "100k") + resistor_v(780, 700, 770, "100k"))
    top.append(netflag(780, 640, "VBAT", "up") + netflag(780, 770, "GND", "down"))
    top.append(text(700, 690, "VBAT/2", "small", "middle"))

    # ── I2C: ENS160+AHT21 (por cima) ─────────────────────────────────────
    enssvg, E = ens(800, 120)
    gx, gy = P["GPIO6"]; sx, sy = E["SDA"]
    out.append(wire([(gx, gy), (770, gy), (770, 66), (sx, 66), (sx, sy)], "wsda", "SDA: GPIO6 → ENS160/AHT21"))
    gx, gy = P["GPIO7"]; sx, sy = E["SCL"]
    out.append(wire([(gx, gy), (785, gy), (785, 82), (sx, 82), (sx, sy)], "wscl", "SCL: GPIO7 → ENS160/AHT21"))
    top.append(netflag(*E["VIN"], "3V3S", "up") + netflag(*E["GND"], "GND", "up"))
    def mini(x, y, net, w):
        c = {"3V3S": "f3v3s", "GND": "fgnd"}[net]
        return (f'<path class="jump {"f3v3" if net == "3V3S" else "fgnd"}" d="M{x},{y} v-18"/>'
                f'<g class="flag {c}"><rect x="{x-w/2}" y="{y-35}" width="{w}" height="17" rx="3"/>'
                + text(x, y - 22.5, net, "flagt", "middle") + "</g>")
    ensx = mini(*E["CS"], "3V3S", 34) + mini(*E["ADD"], "GND", 28)

    # ── SCD41 (mesmo I2C, por etiquetas) ─────────────────────────────────
    sx0, sy0 = 880, 300
    top.append(f'<rect class="scdpcb" x="{sx0}" y="{sy0}" width="150" height="124" rx="8"/>'
               f'<rect class="chips" x="{sx0+80}" y="{sy0+30}" width="40" height="40" rx="4"/>')
    top.append(text(sx0 + 100, sy0 + 104, "SCD41 · CO₂", "pinw", "middle"))
    for n, y, net in (("VDD", 318, "3V3S"), ("SDA", 346, "SDA"), ("SCL", 374, "SCL"), ("GND", 402, "GND")):
        top.append(pad(sx0 + 16, y, "pad used") + text(sx0 + 30, y + 4.5, n, "pinw"))
        top.append(netflag(sx0 + 16, y, net, "left"))
    top.append(text(850, 61, "SDA", "netl nsda", "middle") + text(880, 77, "SCL", "netl nscl", "middle"))

    # ── INMP441 (I2S) ────────────────────────────────────────────────────
    misvg, M = inmp(940, 560)
    gx, gy = P["GPIO5"]; mx, my = M["SD"]
    out.append(wire([(gx, gy), (745, gy), (745, 40), (1160, 40), (1160, my), (mx, my)], "wsd", "SD: GPIO5 → INMP441 SD"))
    gx, gy = P["GPIO20"]; mx, my = M["SCK"]
    out.append(wire([(gx, gy), (840, gy), (840, my), (mx, my)], "wsck", "SCK: GPIO20 → INMP441 SCK"))
    gx, gy = P["GPIO21"]; mx, my = M["WS"]
    out.append(wire([(gx, gy), (825, gy), (825, my), (mx, my)], "wws", "WS: GPIO21 → INMP441 WS"))
    # etiquetas no `out`: a placa do mic (desenhada depois) cobre o traço e os nomes ficam legíveis
    out.append(netflag(*M["VDD"], "3V3S", "right", 64) + netflag(*M["GND"], "GND", "right", 64)
               + netflag(*M["L/R"], "GND", "left", 64))

    top.append(dot(420, 690) + dot(680, 926) + dot(780, 700))
    top.append(text(20, 1050, "Etiquetas com o mesmo nome estão ligadas · 3V3S = 3,3 V comutado pelo SW2 · "
                    "fios que se cruzam sem ponto não estão ligados", "small", "start"))
    body = "".join(out) + c3svg + enssvg + misvg + "".join(top) + ensx
    return f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Sistema completo: bateria, carregador, chaves, ESP32-C3 e todos os sensores">{body}</svg>'

"""Moldura do Yeti para display 1.3" ST7789 240x240 (substitui frame.stl do OLED 0.96").

Contorno externo igual ao frame.stl original (37,4 x 28,5 x 4 mm, cantos R7, borda
frontal arredondada) para encaixar na mesma abertura da cabeça. Muda só a janela.
z = 0 é o verso (lado do display), z = 4 é a frente.

    python3 frame_st7789_1p3.py            -> frame_st7789_1p3.stl
"""
import struct
import sys

import numpy as np

# --- parâmetros (mm) ---------------------------------------------------------
OUT_W, OUT_H, OUT_R = 37.4, 28.5, 7.0   # contorno externo (medido do original)
THICK = 4.0
EDGE_FILLET = 0.5                       # arredondado da borda frontal externa
WIN = 24.2                              # janela no verso: área ativa 23,4 + folga
WIN_R = 1.0
CHAMFER_FROM = 3.1                      # janela reta até aqui, depois chanfro 45°
WIN_OFFSET_X, WIN_OFFSET_Y = 0.0, 0.0   # deslocar a janela se o display não centrar
K = 16                                  # pontos por canto


def rrect(hw, hh, r, cx=0.0, cy=0.0):
    """Retângulo arredondado com 4*K pontos, anti-horário, começando no canto +x+y."""
    pts = []
    for (sx, sy, a0) in ((1, 1, 0), (-1, 1, 90), (-1, -1, 180), (1, -1, 270)):
        ccx, ccy = sx * (hw - r) + cx, sy * (hh - r) + cy
        for a in np.radians(np.linspace(a0, a0 + 90, K)):
            pts.append((ccx + r * np.cos(a), ccy + r * np.sin(a)))
    return np.array(pts)


def loops():
    outer, inner = [], []
    # externo: reto até THICK - fillet, depois quarto de círculo
    outer.append((0.0, rrect(OUT_W / 2, OUT_H / 2, OUT_R)))
    for a in np.radians(np.linspace(0, 90, 7)):
        z = THICK - EDGE_FILLET + EDGE_FILLET * np.sin(a)
        d = EDGE_FILLET * (1 - np.cos(a))
        outer.append((z, rrect(OUT_W / 2 - d, OUT_H / 2 - d, OUT_R - d)))
    # janela: reta até CHAMFER_FROM, chanfro 45° até a frente
    c = THICK - CHAMFER_FROM
    for z, g in ((0.0, 0.0), (CHAMFER_FROM, 0.0), (THICK, c)):
        inner.append((z, rrect(WIN / 2 + g, WIN / 2 + g, WIN_R + g, WIN_OFFSET_X, WIN_OFFSET_Y)))
    return outer, inner


def mesh():
    outer, inner = loops()
    tris = []
    n = 4 * K

    def p3(xy, z):
        return (xy[0], xy[1], z)

    def wall(levels, flip):
        for (z0, a), (z1, b) in zip(levels, levels[1:]):
            for i in range(n):
                j = (i + 1) % n
                q = [p3(a[i], z0), p3(a[j], z0), p3(b[j], z1), p3(b[i], z1)]
                if flip:
                    q = q[::-1]
                tris.extend([(q[0], q[1], q[2]), (q[0], q[2], q[3])])

    def ring(o, inn, z, up):
        for i in range(n):
            j = (i + 1) % n
            q = [p3(o[i], z), p3(o[j], z), p3(inn[j], z), p3(inn[i], z)]
            if not up:
                q = q[::-1]
            tris.extend([(q[0], q[1], q[2]), (q[0], q[2], q[3])])

    wall(outer, flip=False)   # normais para fora
    wall(inner, flip=True)    # normais para dentro da janela
    ring(outer[0][1], inner[0][1], 0.0, up=False)
    ring(outer[-1][1], inner[-1][1], THICK, up=True)
    return np.array(tris)


def write_stl(path, t):
    t = t - [t[..., 0].min(), t[..., 1].min(), 0]
    nrm = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    with open(path, "wb") as f:
        f.write(b"Yeti frame ST7789 1.3in".ljust(80, b"\0"))
        f.write(struct.pack("<I", len(t)))
        for nn, tri in zip(nrm, t):
            f.write(struct.pack("<12fH", *nn, *tri.ravel(), 0))


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "frame_st7789_1p3.stl"
    write_stl(out, mesh())
    print(out)

"""Base de energia do Yeti: copo Ø94 que fica sob a peana original (obj_2, Ø90).

A peana encaixa no rebaixo do topo (RECESS) e fecha o copo; para trocar pilhas,
levanta-se Yeti + peana (os cabos sobem pelos furos da peana e pelas pernas ocas).

Layout (vista de cima, centrado na origem):
  * faixa central y +-BAY_W/2: baia universal 62 x 36 (LiPo 103450 / 2xAA / 3xAAA),
    com rasgos para fita de velcro
  * segmento frontal (y-): TP4056 em cantoneiras
  * segmento traseiro (y+): regulador 37 x 17 (MP1584 buck ou MT3608 boost p/ pilhas)
  * laterais x: corredor de cabos; parede direita = jack DC + conector magnético,
    esquerda = chave liga/desliga
  * separadores com passa-cabos

    ~/.venvs/cad/bin/python yeti_base_energia.py   -> yeti_base_energia.stl + .step
"""
import os

from build123d import (Align, Axis, Box, Cylinder, Location, Plane, Pos, RectangleRounded,
                       SlotOverall, chamfer, export_step, export_stl, extrude, fillet)

OD = 94.0
STAND_D = 90.0                  # peana original
CLR = 0.4                       # folga da peana no rebaixo
RECESS = 3.0                    # profundidade do rebaixo onde a peana assenta
ID = 86.0                       # interno abaixo do rebaixo (degrau de apoio da peana)
FLOOR = 2.0
INNER_H = 18.0                  # altura útil (suporte 2xAA ~15-16 mm)
H = FLOOR + INNER_H + RECESS

BAY_L, BAY_W = 62.0, 36.0
SEP_T, RIB_T = 1.6, 1.2
WALL_TOP = FLOOR + INNER_H - 1.0      # topo dos separadores (folga p/ peana e cabos)
NOTCH_X = (-24.0, 0.0, 24.0)
NOTCH_W, NOTCH_D = 5.0, 5.0

TP4056 = dict(w=26.0, d=17.0, y=-(BAY_W / 2 + SEP_T + 0.8 + 17.0 / 2))
REG = dict(w=37.0, d=17.0, y=BAY_W / 2 + SEP_T + 0.6 + 17.0 / 2)
CLIP_L, CLIP_T, CLIP_H = 4.0, 1.2, 3.0
BOARD_CLR = 0.4

JACK = dict(y=-9.0, z=FLOOR + 8.0, d=8.0)         # DC 5,5x2,1 de painel (parede x+)
MAG = dict(y=9.0, z=FLOOR + 9.0, w=10.0, h=4.5)   # pogo magnético 2 pinos (parede x+)
SWITCH = dict(y=0.0, z=FLOOR + 8.0, w=9.0, h=4.2)  # chave deslizante (parede x-)
STRAP_W = 15.0


def _wall_x(x, y, z):
    """Location numa parede normal a X (local X -> Y global, local Y -> Z)."""
    return Location(Plane(origin=(x, y, z), x_dir=(0, 1, 0), z_dir=(1, 0, 0)))


def _clips(cx, cy, w, d):
    """4 cantoneiras em L em volta de uma placa w x d."""
    hw, hd = w / 2 + BOARD_CLR, d / 2 + BOARD_CLR
    out = None
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, y0 = cx + sx * (hw + CLIP_T / 2), cy + sy * (hd + CLIP_T / 2)
            a = Pos(x0, y0 - sy * (CLIP_L / 2 - CLIP_T / 2), FLOOR) * Box(
                CLIP_T, CLIP_L, CLIP_H, align=(Align.CENTER, Align.CENTER, Align.MIN))
            b = Pos(x0 - sx * (CLIP_L / 2 - CLIP_T / 2), y0, FLOOR) * Box(
                CLIP_L, CLIP_T, CLIP_H, align=(Align.CENTER, Align.CENTER, Align.MIN))
            out = a + b if out is None else out + a + b
    return out


def base():
    up = (Align.CENTER, Align.CENTER, Align.MIN)
    p = Cylinder(OD / 2, H, align=up)
    p = fillet(p.edges().sort_by(Axis.Z)[-1], 1.0)
    p = chamfer(p.edges().sort_by(Axis.Z)[0], 0.6)
    p -= Pos(0, 0, FLOOR) * Cylinder(ID / 2, H, align=up)
    p -= Pos(0, 0, H - RECESS) * Cylinder(STAND_D / 2 + CLR, H, align=up)

    # separadores (cordas inteiras, fundidos na parede) com passa-cabos
    disc = Pos(0, 0, FLOOR) * Cylinder(ID / 2 + 0.5, WALL_TOP - FLOOR, align=up)
    for sy in (-1, 1):
        y = sy * (BAY_W / 2 + SEP_T / 2)
        p += disc & (Pos(0, y, FLOOR) * Box(OD, SEP_T, WALL_TOP - FLOOR, align=up))
        for x in NOTCH_X:
            p -= Pos(x, y, WALL_TOP) * Box(NOTCH_W, 3 * SEP_T, 2 * NOTCH_D)
    # batentes da baia (corredor de cabos entre eles e a parede)
    for sx in (-1, 1):
        p += Pos(sx * (BAY_L / 2 + RIB_T / 2), 0, FLOOR) * Box(
            RIB_T, BAY_W, WALL_TOP - FLOOR - 4.0, align=up)
    # fita da bateria: rasgos no piso junto aos separadores + rebaixo por baixo
    for sy in (-1, 1):
        p -= Pos(0, sy * (BAY_W / 2 - 1.5), 0) * Box(STRAP_W + 1, 2.5, 4 * FLOOR)
    p -= Box(STRAP_W + 1, BAY_W, 2.0)
    # cantoneiras das placas
    p += _clips(0, TP4056["y"], TP4056["w"], TP4056["d"])
    p += _clips(0, REG["y"], REG["w"], REG["d"])
    # furos de painel
    p -= _wall_x(OD / 2, JACK["y"], JACK["z"]) * Cylinder(JACK["d"] / 2, 12)
    p -= _wall_x(OD / 2, MAG["y"], MAG["z"]) * extrude(
        SlotOverall(MAG["w"], MAG["h"]), 6, both=True)
    p -= _wall_x(-OD / 2, SWITCH["y"], SWITCH["z"]) * extrude(
        RectangleRounded(SWITCH["w"], SWITCH["h"], 0.8), 6, both=True)
    return p


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    part = base()
    export_stl(part, os.path.join(here, "yeti_base_energia.stl"), tolerance=0.02,
               angular_tolerance=0.1)
    export_step(part, os.path.join(here, "yeti_base_energia.step"))
    s = part.bounding_box().size
    print(f"yeti_base_energia {s.X:.1f} x {s.Y:.1f} x {s.Z:.1f} mm  "
          f"vol {part.volume / 1000:.1f} cm³  valid={part.is_valid}")

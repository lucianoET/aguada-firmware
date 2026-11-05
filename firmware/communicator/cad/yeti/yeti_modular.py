"""Yeti modular — chassi empilhável em build123d, footprint ~ do Yeti v1.2 (76 x 58 mm).

Padrão (vale para todo módulo):
  * footprint externo W x D, cantos R, parede WALL, piso FLOOR
  * encaixe: saia (skirt) sob o piso entra no módulo de baixo (folga CLR)
  * tirantes M2 nos 4 cantos: base tem furo-guia (autoatarraxa / inserto), módulos e
    tampa têm furo passante -> parafuso/barra M2 longo prende a pilha toda
  * canaleta vertical de cabos na parede esquerda (CHASE_*), passa por todos os pisos
  * ranhuras-guia nas paredes frente/trás a cada DIV_PITCH para divisores (nichos)

Peças (z = 0 no fundo, centradas em XY):
  base_energia  baia universal (LiPo 103450 / 2xAA / 3xAAA), separador, TP4056 +
                regulador em pé em guias, USB-C, jack DC, conector magnético, chave
  modulo_nicho  bandeja genérica com ranhuras para divisores
  divisor       placa que entra nas ranhuras, com passa-cabo
  tampa         fecha a pilha / apoio da cabeça, furos escareados p/ cabeça M2

    ~/.venvs/cad/bin/python yeti_modular.py      -> modular/*.stl + *.step
"""
import os

from build123d import (Align, Box, Cylinder, Location, Plane, Pos, RectangleRounded,
                       SlotOverall, export_step, export_stl, extrude)

# --- padrão ------------------------------------------------------------------
W, D, R = 76.0, 58.0, 8.0
WALL, FLOOR = 2.0, 2.0
CLR = 0.2                               # folga de encaixe
SKIRT_H, SKIRT_T = 1.5, 1.2
IW, ID, IR = W - 2 * WALL, D - 2 * WALL, R - WALL
BOSS_D, BOSS_IN = 5.0, 1.5              # coluna dos cantos, centro a 1,5 mm das paredes
PILOT_D, THRU_D = 1.7, 2.4              # M2: furo-guia na base / passante nos módulos
HEAD_D, HEAD_H = 4.2, 1.6               # rebaixo da cabeça M2 na tampa
BOSS_X, BOSS_Y = IW / 2 - BOSS_IN, ID / 2 - BOSS_IN
CHASE_W, CHASE_Y0, CHASE_Y1 = 4.5, -15.0, 5.0
RIB_T = 1.2
RIB_X = -IW / 2 + CHASE_W               # face da nervura que fecha a canaleta
DIV_T, DIV_GROOVE, DIV_DEPTH, DIV_PITCH = 1.6, 1.9, 0.8, 8.0

# --- base de energia ---------------------------------------------------------
BASE_H = 26.0
SEP_Y, SEP_T = 11.0, 1.6                # baia: y -ID/2 .. SEP_Y (38 mm), eletrônica atrás
BAY_X = RIB_X + RIB_T                   # batentes da baia em +-BAY_X (60,6 mm úteis)
STRIP_C = 20.0                          # linha das placas em pé (componentes p/ -y)
CARD_SLOT, CARD_DEPTH = 1.8, 1.0
CARD_POSTS = (-27.0, -7.0, 3.0, 17.0)   # TP4056 x -34..-8, regulador x -2..20
USBC = dict(y=STRIP_C - 2.4, z=9.5, w=10.0, h=4.5)    # TP4056 na parede esquerda
JACK = dict(y=STRIP_C, z=9.0, d=8.0)                   # DC 5,5x2,1 painel, parede direita
MAG = dict(y=STRIP_C, z=19.5, w=10.0, h=4.5)          # pogo magnético 2 pinos
SWITCH = dict(y=-8.0, z=12.0, w=9.0, h=4.2)            # chave deslizante, parede direita
STRAP_W = 14.0                          # fita de velcro em volta da bateria

MOD_H = 20.0
LID_H = 3.0


def _wall_x(x, y, z):
    """Location numa parede normal a X (local X -> Y global, local Y -> Z)."""
    return Location(Plane(origin=(x, y, z), x_dir=(0, 1, 0), z_dir=(1, 0, 0)))


def _corners():
    return [(sx * BOSS_X, sy * BOSS_Y) for sx in (-1, 1) for sy in (-1, 1)]


def shell(h):
    s = extrude(RectangleRounded(W, D, R), h)
    s -= Pos(0, 0, FLOOR) * extrude(RectangleRounded(IW, ID, IR), h)
    for x, y in _corners():
        s += Pos(x, y, 0) * Cylinder(BOSS_D / 2, h - SKIRT_H - 0.1,
                                     align=(Align.CENTER, Align.CENTER, Align.MIN))
    # nervura da canaleta vertical
    s += Pos(RIB_X + RIB_T / 2, (CHASE_Y0 + CHASE_Y1) / 2, FLOOR) * Box(
        RIB_T, CHASE_Y1 - CHASE_Y0, h - FLOOR - SKIRT_H - 0.2,
        align=(Align.CENTER, Align.CENTER, Align.MIN))
    return s


def skirt():
    o = (IW - 2 * CLR, ID - 2 * CLR, IR - CLR)
    i = (o[0] - 2 * SKIRT_T, o[1] - 2 * SKIRT_T, o[2] - SKIRT_T)
    s = extrude(RectangleRounded(*o), SKIRT_H) - extrude(RectangleRounded(*i), SKIRT_H)
    s -= Pos(-IW / 2, (CHASE_Y0 + CHASE_Y1) / 2, 0) * Box(2 * CHASE_W, CHASE_Y1 - CHASE_Y0,
                                                          3 * SKIRT_H)
    return Pos(0, 0, -SKIRT_H) * s


def chase_slot():
    return Pos(-IW / 2 + CHASE_W / 2, (CHASE_Y0 + CHASE_Y1) / 2, 0) * Box(
        CHASE_W, CHASE_Y1 - CHASE_Y0, 4 * FLOOR)


def div_grooves(h):
    g = None
    n = int((IW / 2 - 10) // DIV_PITCH)
    for k in range(-n, n + 1):
        for sy in (-1, 1):
            b = Pos(k * DIV_PITCH, sy * (ID / 2 + DIV_DEPTH / 2), FLOOR) * Box(
                DIV_GROOVE, DIV_DEPTH * 2, h, align=(Align.CENTER, Align.CENTER, Align.MIN))
            g = b if g is None else g + b
    return g


def base_energia():
    h = BASE_H
    p = shell(h)
    for x, y in _corners():
        p -= Pos(x, y, FLOOR) * Cylinder(PILOT_D / 2, h,
                                         align=(Align.CENTER, Align.CENTER, Align.MIN))
    top = h - SKIRT_H - 0.2
    # separador baia | eletrônica, com passa-cabos
    p += Pos(0, SEP_Y + SEP_T / 2, FLOOR) * Box(IW, SEP_T, top - FLOOR,
                                                align=(Align.CENTER, Align.CENTER, Align.MIN))
    for x in (-IW / 2 + CHASE_W / 2, -20.0, -6.0, 22.0):
        p -= Pos(x, SEP_Y + SEP_T / 2, top) * Box(5.0, 3 * SEP_T, 10.0)
    # batente direito da baia (o esquerdo é a nervura da canaleta)
    p += Pos(BAY_X + RIB_T / 2, (-ID / 2 + SEP_Y) / 2, FLOOR) * Box(
        RIB_T, SEP_Y + ID / 2, top - FLOOR, align=(Align.CENTER, Align.CENTER, Align.MIN))
    p -= Pos(BAY_X + RIB_T / 2, -2.0, top) * Box(3 * RIB_T, 6.0, 10.0)
    # guias das placas em pé: ranhura no piso + colunas com fenda
    p -= Pos(0, STRIP_C, FLOOR) * Box(2 * BOSS_X - BOSS_D - 1, CARD_SLOT, 2 * CARD_DEPTH)
    for x in CARD_POSTS:
        for sy in (-1, 1):
            p += Pos(x, STRIP_C + sy * (CARD_SLOT / 2 + 1.0), FLOOR) * Box(
                2.0, 2.0, 10.0, align=(Align.CENTER, Align.CENTER, Align.MIN))
    # furos de painel
    p -= _wall_x(-W / 2, USBC["y"], USBC["z"]) * extrude(
        RectangleRounded(USBC["w"], USBC["h"], 1.5), 3 * WALL, both=True)
    p -= _wall_x(W / 2, JACK["y"], JACK["z"]) * Cylinder(JACK["d"] / 2, 3 * WALL)
    p -= _wall_x(W / 2, MAG["y"], MAG["z"]) * extrude(
        SlotOverall(MAG["w"], MAG["h"]), 3 * WALL, both=True)
    p -= _wall_x(W / 2, SWITCH["y"], SWITCH["z"]) * extrude(
        RectangleRounded(SWITCH["w"], SWITCH["h"], 0.8), 3 * WALL, both=True)
    # fita da bateria: 2 rasgos no piso + rebaixo por baixo
    for y in (-ID / 2 + 2.0, SEP_Y - 2.0):
        p -= Pos(0, y, 0) * Box(STRAP_W + 1, 2.5, 4 * FLOOR)
    p -= Pos(0, (-ID / 2 + SEP_Y) / 2, 0) * Box(STRAP_W + 1, SEP_Y + ID / 2, 2.0)
    return p


def _through_bosses(p, h):
    for x, y in _corners():
        p -= Pos(x, y, -SKIRT_H - 1) * Cylinder(THRU_D / 2, h + 2 * SKIRT_H + 2,
                                                align=(Align.CENTER, Align.CENTER, Align.MIN))
    return p


def modulo_nicho(h=MOD_H):
    p = shell(h) + skirt() - chase_slot() - div_grooves(h)
    return _through_bosses(p, h)


def divisor(h=MOD_H):
    L = ID + 2 * DIV_DEPTH - 0.3
    H = h - FLOOR - SKIRT_H - 0.3
    d = Box(DIV_T, L, H, align=(Align.CENTER, Align.CENTER, Align.MIN))
    d -= Box(4 * DIV_T, 8.0, 6.0)          # passa-cabo embaixo (3 mm úteis)
    return d


def tampa():
    p = extrude(RectangleRounded(W, D, R), LID_H - SKIRT_H) + skirt()
    p = Pos(0, 0, SKIRT_H) * p
    for x, y in _corners():
        p -= Pos(x, y, 0) * Cylinder(THRU_D / 2, 20)
        p -= Pos(x, y, LID_H - HEAD_H) * Cylinder(HEAD_D / 2, HEAD_H + 1,
                                                  align=(Align.CENTER, Align.CENTER, Align.MIN))
    p -= Pos(0, 0, SKIRT_H) * chase_slot()
    return p


PARTS = {
    "base_energia": base_energia,
    "modulo_nicho": modulo_nicho,
    "divisor": divisor,
    "tampa": tampa,
}

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "modular")
    os.makedirs(out, exist_ok=True)
    for name, fn in PARTS.items():
        part = fn()
        export_stl(part, os.path.join(out, name + ".stl"), tolerance=0.02,
                   angular_tolerance=0.1)
        export_step(part, os.path.join(out, name + ".step"))
        s = part.bounding_box().size
        print(f"{name:14s} {s.X:6.1f} x {s.Y:5.1f} x {s.Z:5.1f} mm  "
              f"vol {part.volume / 1000:5.1f} cm³  valid={part.is_valid}")

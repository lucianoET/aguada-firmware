"""Moldura do Yeti para display 1.3" ST7789 240x240 — versão build123d (B-rep).

Mesma geometria de frame_st7789_1p3.py (malha numpy), mas sólido real: exporta
STEP (editável em FreeCAD/Fusion) e STL. z = 0 é o verso (lado do display),
z = THICK é a frente. Centrada na origem.

    ~/.venvs/cad/bin/python frame_st7789_1p3_b3d.py   -> .stl + .step
"""
import sys

from build123d import (Axis, BuildPart, BuildSketch, Locations, Mode, RectangleRounded,
                       chamfer, export_step, export_stl, extrude, fillet)

# --- parâmetros (mm) ---------------------------------------------------------
OUT_W, OUT_H, OUT_R = 37.4, 28.5, 7.0   # contorno externo (medido do original)
THICK = 4.0
EDGE_FILLET = 0.5                       # arredondado da borda frontal externa
WIN = 24.2                              # janela no verso: área ativa 23,4 + folga
WIN_R = 1.0
CHAMFER_FROM = 3.1                      # janela reta até aqui, depois chanfro 45°
WIN_OFFSET_X, WIN_OFFSET_Y = 0.0, 0.0   # deslocar a janela se o display não centrar


def build():
    with BuildPart() as p:
        with BuildSketch():
            RectangleRounded(OUT_W, OUT_H, OUT_R)
        extrude(amount=THICK)
        with BuildSketch():
            with Locations((WIN_OFFSET_X, WIN_OFFSET_Y)):
                RectangleRounded(WIN, WIN, WIN_R)
        extrude(amount=THICK, mode=Mode.SUBTRACT)
        front = p.faces().sort_by(Axis.Z)[-1]
        fillet(front.outer_wire().edges(), EDGE_FILLET)
        front = p.faces().sort_by(Axis.Z)[-1]
        chamfer(front.inner_wires()[0].edges(), THICK - CHAMFER_FROM)
    return p.part


if __name__ == "__main__":
    base = sys.argv[1] if len(sys.argv) > 1 else "frame_st7789_1p3_b3d"
    part = build()
    export_stl(part, base + ".stl", tolerance=0.01, angular_tolerance=0.1)
    export_step(part, base + ".step")
    bb = part.bounding_box()
    print(f"{base}.stl/.step  {bb.size.X:.2f} x {bb.size.Y:.2f} x {bb.size.Z:.2f} mm  "
          f"vol {part.volume:.1f} mm³  valid={part.is_valid}")

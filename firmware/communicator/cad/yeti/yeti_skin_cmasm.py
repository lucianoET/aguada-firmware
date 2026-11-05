"""Skin CMASM: chapéu que encaixa na cabeça do Yeti (corpo original não muda).

O miolo do chapéu é a própria cabeça subtraída (com folga), então assenta sem
ajuste; os chifres atravessam a aba por recortes que permitem descer o chapéu
na vertical. Estrela na frente da copa e "CMASM" em relevo na aba.

Saída no sistema do chapéu: Z para cima, Y para a frente (rosto), aba em Z = 0.
Imprimir de cabeça para cima com suporte sob a aba/miolo, ou de copa para baixo.

    ~/.venvs/cad/bin/python yeti_skin_cmasm.py   -> yeti_skin_cmasm_chapeu.stl
"""
import math
import os
import tempfile

import manifold3d as mf
import numpy as np
import trimesh
from build123d import (Align, Ellipse, FontStyle, Pos, Rot, Sphere, Text, export_stl,
                       extrude, fillet, make_face, Polyline, scale, Axis)

BODY = "yeti_corpo_v2.stl"
OUT = "yeti_skin_cmasm_chapeu.stl"

# encaixe na cabeça (coordenadas do corpo: y para baixo, z para a frente)
HEAD_X, HEAD_Z = 130.0, 18.0    # centro da cabeça em x / profundidade
BRIM_Y = 97.0                   # altura da aba no corpo (topo da cabeça ~ y 92, tufos ~ 88)
FIT_CLR = 0.3                   # folga chapéu-cabeça

# chapéu
CROWN_A, CROWN_B = 21.0, 19.0   # semi-eixos da copa (largura, profundidade)
CROWN_SIDE, CROWN_DOME = 8.0, 6.0
BAND_H, BAND_T = 4.5, 0.8       # faixa da copa
BRIM_A, BRIM_B, BRIM_T = 31.0, 26.0, 1.8
BRIM_FWD = 4.0                  # aba deslocada para a frente (pala)
HORN_SWEEP = 20.0               # recorte dos chifres na aba (desce o chapéu na vertical)

STAR_R, STAR_Z, STAR_H = 5.0, 7.0, 1.2
TEXT, TEXT_SIZE, TEXT_H = "CMASM", 4.5, 0.6


def star(r_out, r_in):
    pts = []
    for i in range(10):
        r = r_out if i % 2 == 0 else r_in
        a = math.pi / 2 + i * math.pi / 5
        pts.append((r * math.cos(a), r * math.sin(a)))
    return make_face(Polyline(*pts, close=True))


def hat_solid():
    up = (Align.CENTER, Align.CENTER, Align.MIN)
    crown = extrude(Ellipse(CROWN_A, CROWN_B), CROWN_SIDE)
    crown += Pos(0, 0, CROWN_SIDE) * scale(Sphere(1.0), (CROWN_A, CROWN_B, CROWN_DOME))
    crown += extrude(Ellipse(CROWN_A + BAND_T, CROWN_B + BAND_T), BAND_H)
    brim = Pos(0, BRIM_FWD, 0) * extrude(Ellipse(BRIM_A, BRIM_B), BRIM_T)
    brim = fillet(brim.edges().sort_by(Axis.Z)[-1], 0.6)
    hat = crown + brim
    # estrela na frente da copa (plano XZ, saindo para +Y)
    s = extrude(star(STAR_R, STAR_R * 0.42), STAR_H + 2)
    hat += Pos(0, CROWN_B + BAND_T - 2, STAR_Z) * Rot(-90, 0, 0) * s
    # texto na aba, legível de frente (topo das letras aponta para a copa)
    y_txt = (CROWN_B + BAND_T + BRIM_FWD + BRIM_B) / 2
    t = extrude(Text(TEXT, TEXT_SIZE, font_style=FontStyle.BOLD), TEXT_H)
    hat += Pos(0, y_txt, BRIM_T) * Rot(0, 0, 180) * t
    return hat


def to_manifold_mesh(tm):
    tm.merge_vertices()
    return mf.Manifold(mf.Mesh(vert_properties=tm.vertices.astype(np.float32),
                               tri_verts=tm.faces.astype(np.uint32)))


def body_in_hat_frame(path, base_y=BRIM_Y, head_z=HEAD_Z):
    b = trimesh.load(path)
    v = b.vertices
    # (x, y, z) corpo -> (X, Y, Z) chapéu: X = x - cx, Y = z - cz, Z = base_y - y  (det +1)
    b.vertices = np.c_[v[:, 0] - HEAD_X, v[:, 2] - head_z, base_y - v[:, 1]]
    return to_manifold_mesh(b)


def with_clearance(m, c):
    out = m
    for d in ((c, 0, 0), (-c, 0, 0), (0, c, 0), (0, -c, 0), (0, 0, c)):
        out += m.translate(list(d))
    return out


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "hat.stl")
        export_stl(hat_solid(), p, tolerance=0.02, angular_tolerance=0.1)
        hat = to_manifold_mesh(trimesh.load(p))
    body = body_in_hat_frame(os.path.join(here, BODY))
    hat -= with_clearance(body, FIT_CLR)
    # chifres: tudo do corpo fora da copa e acima da base da aba, varrido para baixo
    horns = body ^ mf.Manifold.cube([200, 200, 60]).translate([-100, -100, -2])
    horns -= mf.Manifold.cube([2 * (CROWN_A + BAND_T), 200, 80]).translate(
        [-(CROWN_A + BAND_T), -100, -10])
    horns = with_clearance(horns, FIT_CLR)
    sweep = horns
    for k in range(1, int(HORN_SWEEP / 0.5) + 1):
        sweep += horns.translate([0, 0, -0.5 * k])
    hat -= sweep
    parts = hat.decompose()
    hat = max(parts, key=lambda m: m.volume())     # descarta lascas soltas
    m = hat.to_mesh()
    trimesh.Trimesh(m.vert_properties[:, :3], m.tri_verts).export(os.path.join(here, OUT))
    bb = hat.bounding_box()
    print(f"{OUT}: {bb[3] - bb[0]:.1f} x {bb[4] - bb[1]:.1f} x {bb[5] - bb[2]:.1f} mm  "
          f"vol {hat.volume() / 1000:.1f} cm³  peças descartadas {len(parts) - 1}")

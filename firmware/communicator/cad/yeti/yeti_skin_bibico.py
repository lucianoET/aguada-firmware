"""Skin CMASM: bibico (gorro de quartel) para o Yeti, alternativa ao chapéu.

Mesmo princípio do chapéu: o miolo é a cabeça subtraída com folga, então o
bibico assenta no topo, de frente para trás, cobrindo os tufos e entre os
chifres. Vinco no topo, barra (cortina) em volta, estrela no lado direito e
"CMASM" no lado esquerdo.

Saída: Z para cima, Y para a frente (rosto), barra em Z = 0.

    ~/.venvs/cad/bin/python yeti_skin_bibico.py   -> yeti_skin_cmasm_bibico.stl
"""
import os
import tempfile

import manifold3d as mf
import trimesh
from build123d import (Box, FontStyle, Location, Plane, Pos, RectangleRounded, Text,
                       export_stl, extrude, loft)

from yeti_skin_cmasm import (BODY, FIT_CLR, body_in_hat_frame, star, to_manifold_mesh,
                             with_clearance)

OUT = "yeti_skin_cmasm_bibico.stl"
BASE_Y = 95.0                   # altura da base da barra no corpo (topo da cabeça ~ 92)
HEAD_Z = 19.0                   # centro do bibico na profundidade da cabeça
SKIRT = 6.0                     # quanto a barra desce abaixo de Z = 0 para abraçar a cabeça

# seções (largura X, comprimento Y, raio, altura Z): base larga -> cumeeira fina
SECTIONS = ((28.0, 40.0, 9.0, 0.0), (21.0, 42.0, 7.0, 8.4), (2.4, 45.0, 1.1, 14.0))
BAND_W, BAND_L, BAND_R, BAND_TOP = 29.4, 41.4, 9.0, 5.0
CREASE_W, CREASE_D = 1.0, 2.0

STAR_R, STAR_Y, STAR_Z = 3.0, 6.0, 2.2
TEXT, TEXT_SIZE, TEXT_Z = "CMASM", 4.0, 2.2
RELIEF = 0.6


def _side(x, y, z, sign):
    """Plano na face lateral da barra (normal +-X), base do texto horizontal."""
    return Location(Plane(origin=(x, y, z), x_dir=(0, sign, 0), z_dir=(sign, 0, 0)))


def cap_solid():
    secs = [Pos(0, 0, z) * RectangleRounded(w, l, r) for w, l, r, z in SECTIONS]
    cap = loft(secs)
    cap += Pos(0, 0, -SKIRT) * extrude(RectangleRounded(BAND_W, BAND_L, BAND_R),
                                       SKIRT + BAND_TOP)
    cap -= Pos(0, 0, SECTIONS[-1][3]) * Box(CREASE_W, 60, 2 * CREASE_D)
    # estrela no lado direito (+X), texto no esquerdo (-X), embutidos 1,5 mm na barra
    emb = 1.5
    s = extrude(star(STAR_R, STAR_R * 0.42), emb + RELIEF)
    cap += _side(BAND_W / 2 - emb, STAR_Y, STAR_Z, 1) * s
    t = extrude(Text(TEXT, TEXT_SIZE, font_style=FontStyle.BOLD), emb + RELIEF)
    cap += _side(-BAND_W / 2 + emb, 0, TEXT_Z, -1) * t
    return cap


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "cap.stl")
        export_stl(cap_solid(), p, tolerance=0.02, angular_tolerance=0.1)
        cap = to_manifold_mesh(trimesh.load(p))
    body = body_in_hat_frame(os.path.join(here, BODY), BASE_Y, HEAD_Z)
    cap -= with_clearance(body, FIT_CLR)
    parts = cap.decompose()
    cap = max(parts, key=lambda m: m.volume())
    m = cap.to_mesh()
    trimesh.Trimesh(m.vert_properties[:, :3], m.tri_verts).export(os.path.join(here, OUT))
    bb = cap.bounding_box()
    print(f"{OUT}: {bb[3] - bb[0]:.1f} x {bb[4] - bb[1]:.1f} x {bb[5] - bb[2]:.1f} mm  "
          f"vol {cap.volume() / 1000:.1f} cm³  lascas descartadas {len(parts) - 1}")

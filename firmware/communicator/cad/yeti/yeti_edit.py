"""Edições no corpo original do Yeti (obj_7_corp+face) por boolean de malha (manifold3d).

Sistema do STL: z = 0 costas abertas, z ~ 40 frente (janela do display);
y ~ 88 topo (chifres), y ~ 162 pés; x 94..168 (braços nos extremos).

Passo 1: furos dos mics INMP441 nas laterais da cabeça + rebaixo plano interno
para a placa do mic encostar (deixa WALL_KEEP de parede no furo).

    ~/.venvs/cad/bin/python yeti_edit.py      -> yeti_corpo_v2.stl
"""
import os
import sys

import manifold3d as mf
import numpy as np
import trimesh

SRC = "obj_7_corp+face.stl_1.stl"
OUT = "yeti_corpo_v2.stl"

# --- mics (laterais da cabeça) ------------------------------------------------
MIC_Y, MIC_Z = 114.0, 18.0      # altura (abaixo do chifre) e profundidade do furo
MIC_HOLE_D = 1.5                # porta acústica
MIC_SPOT_D = 15.0               # rebaixo para a placa INMP441 (~14 mm)
WALL_KEEP = 1.4                 # parede que sobra no fundo do rebaixo

# --- touch nos braços ----------------------------------------------------------
# O braço já é oco (parede 1,4-2,6 mm) e aberto para o interior: a fita de cobre
# cola por dentro e o fio passa direto. Aqui só marca por fora onde tocar.
ARM_Y, ARM_Z = 140.0, 16.0      # centro da zona de toque (parte mais saliente)
ARM_MARK_D = 9.0                # covinha de referência para o dedo
ARM_MARK_DEPTH = 0.5            # 0 desliga; some suave nas bordas (superfície curva)


def to_manifold(m):
    return mf.Manifold(mf.Mesh(vert_properties=m.vertices.astype(np.float32),
                               tri_verts=m.faces.astype(np.uint32)))


def ray_hits(tris, o, d):
    o, d = np.asarray(o, float), np.asarray(d, float)
    e1, e2 = tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    p = np.cross(d, e2)
    det = (e1 * p).sum(1)
    ok = np.abs(det) > 1e-12
    inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
    s = o - tris[:, 0]
    u = (s * p).sum(1) * inv
    q = np.cross(s, e1)
    v = (q * d).sum(1) * inv
    t = (e2 * q).sum(1) * inv
    return np.sort(t[ok & (u >= 0) & (v >= 0) & (u + v <= 1)])


def cyl_x(x0, x1, y, z, d):
    """Cilindro ao longo de X entre x0 e x1."""
    c = mf.Manifold.cylinder(abs(x1 - x0), d / 2, d / 2, 48)
    return c.rotate([0, 90, 0]).translate([min(x0, x1), y, z])


def mic_cutters(tris):
    # superfície externa no disco do rebaixo: o plano fica WALL_KEEP para dentro do
    # ponto mais fundo da curva, assim o rebaixo nunca fura a parede
    r = MIC_SPOT_D / 2
    lo_max, ro_min = -1e9, 1e9
    for dy in np.linspace(-r, r, 9):
        for dz in np.linspace(-r, r, 9):
            if dy * dy + dz * dz > r * r:
                continue
            t = ray_hits(tris, (0, MIC_Y + dy, MIC_Z + dz), (1, 0, 0))
            if len(t) < 4:
                sys.exit(f"raio dos mics não achou as 2 paredes em dy={dy} dz={dz}: {t}")
            lo_max, ro_min = max(lo_max, t[0]), min(ro_min, t[-1])
    xl, xr = lo_max + WALL_KEEP, ro_min - WALL_KEEP
    print(f"mics: rebaixo esq x={xl:.1f}, dir x={xr:.1f}")
    c = cyl_x(lo_max - 4, xl + 1, MIC_Y, MIC_Z, MIC_HOLE_D)
    c += cyl_x(xl, xl + 12, MIC_Y, MIC_Z, MIC_SPOT_D)
    c += cyl_x(ro_min + 4, xr - 1, MIC_Y, MIC_Z, MIC_HOLE_D)
    c += cyl_x(xr, xr - 12, MIC_Y, MIC_Z, MIC_SPOT_D)
    return c


def arm_cutters(tris):
    t = ray_hits(tris, (0, ARM_Y, ARM_Z), (1, 0, 0))
    if len(t) < 4:
        sys.exit(f"raio dos braços não achou as paredes: {t}")
    (lo, li), (ri, ro) = t[:2], t[-2:]
    print(f"braços: parede esq {li - lo:.1f} mm, dir {ro - ri:.1f} mm "
          f"(sobra {li - lo - ARM_MARK_DEPTH:.1f} / {ro - ri - ARM_MARK_DEPTH:.1f} na covinha)")
    c = cyl_x(lo - 5, lo + ARM_MARK_DEPTH, ARM_Y, ARM_Z, ARM_MARK_D)
    c += cyl_x(ro + 5, ro - ARM_MARK_DEPTH, ARM_Y, ARM_Z, ARM_MARK_D)
    return c


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    src = trimesh.load(os.path.join(here, SRC))
    src.merge_vertices()
    body = to_manifold(src) - mic_cutters(src.triangles)
    if ARM_MARK_DEPTH > 0:
        body -= arm_cutters(src.triangles)
    out = body.to_mesh()
    res = trimesh.Trimesh(out.vert_properties[:, :3], out.tri_verts)
    res.export(os.path.join(here, OUT))
    print(f"{OUT}: vol {body.volume() / 1000:.1f} cm³ (orig {to_manifold(src).volume() / 1000:.1f}), "
          f"status {body.status()}")

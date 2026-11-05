# CAD do Yeti

Scripts que modificam o Yeti v1.2 impresso para o perfil `yeti_s3` ([../../YETI.md](../../YETI.md)).
Os STLs originais do Yeti v1.2 (`obj_*.stl`) **não ficam no repositório** (modelo de
terceiros, repo público): copie-os para esta pasta antes de rodar os scripts que dependem
deles. O `.gitignore` daqui ignora esses arquivos e as saídas derivadas.

| Script | Entrada | Saída | No git |
|--------|---------|-------|--------|
| `frame_st7789_1p3.py` | — | `frame_st7789_1p3.stl` (moldura do display 1.3") | ✅ STL |
| `frame_st7789_1p3_b3d.py` | — | mesma moldura em build123d (`.stl` + `.step`) | — |
| `yeti_base_energia.py` | — | `yeti_base_energia.stl/.step` (copo Ø94 sob a peana) | ✅ STL |
| `yeti_edit.py` | `obj_7_corp+face.stl_1.stl` | `yeti_corpo_v2.stl` (furos dos mics, toque nos braços) | — |
| `yeti_skin_cmasm.py` | `yeti_corpo_v2.stl` | `yeti_skin_cmasm_chapeu.stl` | — |
| `yeti_skin_bibico.py` | `yeti_corpo_v2.stl` | `yeti_skin_cmasm_bibico.stl` | — |
| `yeti_modular.py` | — | `modular/*` (rascunho de chassi empilhável, fora do caminho principal) | — |

## Ambiente

`frame_st7789_1p3.py` só precisa de numpy. Os demais usam o venv de CAD:

```bash
python3 -m venv ~/.venvs/cad
~/.venvs/cad/bin/pip install build123d manifold3d trimesh numpy
cd firmware/communicator/cad/yeti
~/.venvs/cad/bin/python yeti_edit.py
```

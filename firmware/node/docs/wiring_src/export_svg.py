import sys, re
sys.path.insert(0, '.')
import gen, page
css = page.CSS
svg_css = css[css.index('/* ── SVG ── */'):]
tokens = """svg{--bg:#eef1ef;--panel:#f8faf9;--ink:#14201b;--muted:#5a6b64;--gnd:#1c1f23;--halo:#f8faf9;
--w3v3:#d6332a;--wsda:#1f6fd1;--wscl:#d99a00;--wsd:#1a9a52;--wsck:#7a4bd1;--wws:#e0661a;
--wdq:#0d8c8c;--wct:#9a5b2e;--wlux:#c2457e;--wpas:#3c4a45;--wcable:#8c969c;--wbat:#d9480f;
--display:"IBM Plex Sans Condensed","Arial Narrow",Arial,sans-serif;--mono:"IBM Plex Mono","DejaVu Sans Mono",Consolas,monospace}"""
def standalone(svg, title, note):
    m = re.match(r'<svg viewBox="0 0 (\d+) (\d+)"[^>]*>', svg)
    W, H = int(m.group(1)), int(m.group(2)); Ht = H + 70
    inner = svg[m.end():-len('</svg>')]
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {Ht}" width="{W}" height="{Ht}">'
            f'<title>{title}</title><style>{tokens}{svg_css}'
            '.hd{fill:#14201b;font:600 22px var(--display)}.nt{fill:#5a6b64;font-size:12px}</style>'
            f'<rect width="{W}" height="{Ht}" fill="#f8faf9"/>'
            f'<text class="hd" x="20" y="34">{title}</text><text class="nt" x="20" y="54">{note}</text>'
            f'<g transform="translate(0,62)">{inner}</g></svg>')
n = 'Aguada node · env ani-01-c3 · ESP32-C3 SuperMini visto pelo lado da serigrafia · gerado de firmware/node/docs'
open('../airq_wiring_core.svg','w').write(standalone(gen.diagram_core(), 'AirQ — caixa central (ENS160+AHT21, INMP441)', n))
open('../airq_wiring_power.svg','w').write(standalone(gen.diagram_power(), 'AirQ — alimentação por bateria (opcional)', n))
open('../airq_wiring_ac.svg','w').write(standalone(gen.diagram_ac(), 'AirQ — portas do analisador de AC (opcionais)', n))

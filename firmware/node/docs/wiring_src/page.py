import base64, sys
sys.path.insert(0, '.')
import gen

photo = base64.b64encode(open('crop.jpg', 'rb').read()).decode()

CSS = r"""
/* Layout: folha de bancada — coluna única larga, diagramas a toda a largura com scroll próprio no telemóvel */
:root{
  --bg:#eef1ef; --panel:#f8faf9; --ink:#14201b; --muted:#5a6b64; --line:#c9d3cf;
  --accent:#0f7a5c; --gnd:#1c1f23; --halo:#f8faf9;
  --w3v3:#d6332a; --wsda:#1f6fd1; --wscl:#d99a00; --wsd:#1a9a52; --wsck:#7a4bd1; --wws:#e0661a;
  --wdq:#0d8c8c; --wct:#9a5b2e; --wlux:#c2457e; --wpas:#3c4a45; --wcable:#8c969c; --wbat:#d9480f;
  --display:"IBM Plex Sans Condensed","Arial Narrow",sans-serif;
  --body:"IBM Plex Sans",system-ui,sans-serif;
  --mono:"IBM Plex Mono",ui-monospace,monospace;
}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){
  --bg:#0e1412; --panel:#151d1a; --ink:#e3ebe7; --muted:#93a39c; --line:#2a3833;
  --accent:#3fc596; --gnd:#a9b4bd; --halo:#151d1a;
  --w3v3:#ff5a4f; --wsda:#5aa2ff; --wscl:#f2c03a; --wsd:#3ccf7d; --wsck:#a98bff; --wws:#ff8a45;
  --wdq:#2fc4c4; --wct:#d08a55; --wlux:#ef6fa8; --wpas:#b6c3bd; --wcable:#7d878d; --wbat:#ff8a3d; color-scheme:dark}}
:root[data-theme="dark"]{
  --bg:#0e1412; --panel:#151d1a; --ink:#e3ebe7; --muted:#93a39c; --line:#2a3833;
  --accent:#3fc596; --gnd:#a9b4bd; --halo:#151d1a;
  --w3v3:#ff5a4f; --wsda:#5aa2ff; --wscl:#f2c03a; --wsd:#3ccf7d; --wsck:#a98bff; --wws:#ff8a45;
  --wdq:#2fc4c4; --wct:#d08a55; --wlux:#ef6fa8; --wpas:#b6c3bd; --wcable:#7d878d; --wbat:#ff8a3d; color-scheme:dark}
body{background:var(--bg);color:var(--ink);font-family:var(--body);font-size:15px;line-height:1.55;
  padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));padding-block:28px 48px}
main{max-width:1040px;margin:0 auto;display:flex;flex-direction:column;gap:34px}
header{display:flex;flex-direction:column;gap:6px}
.eyebrow{font-family:var(--mono);font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--accent)}
h1{font-family:var(--display);font-weight:600;font-size:clamp(30px,5vw,44px);line-height:1.05;margin:0;text-wrap:balance}
h2{font-family:var(--display);font-weight:600;font-size:24px;margin:0;text-wrap:balance}
.lede{max-width:68ch;color:var(--muted);margin:0}
section{display:flex;flex-direction:column;gap:14px}
.sec-hd{display:flex;align-items:baseline;gap:12px;flex-wrap:wrap}
.tag{font-family:var(--mono);font-size:11.5px;padding:2px 8px;border:1px solid var(--line);border-radius:999px;color:var(--muted)}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:12px}
.diagram{overflow-x:auto;padding:12px}
.diagram svg{display:block;min-width:760px;width:100%;height:auto}
p{margin:0;max-width:72ch}
.legend{display:flex;flex-wrap:wrap;gap:8px 18px;font-family:var(--mono);font-size:12.5px;color:var(--muted)}
.legend span{display:inline-flex;align-items:center;gap:7px}
.legend i{width:22px;height:4px;border-radius:2px;display:inline-block}
.photo{display:grid;grid-template-columns:minmax(0,1.1fr) minmax(0,1fr);gap:20px;align-items:start}
@media (max-width:760px){.photo{grid-template-columns:1fr}}
.ph{position:relative;overflow:hidden;border-radius:12px;border:1px solid var(--line)}
.ph img{display:block;width:100%;height:auto}
.ph svg{position:absolute;inset:0;width:100%;height:100%}
.co circle{fill:#0f7a5c;stroke:#fff;stroke-width:3}
.co text{fill:#fff;font:700 22px var(--mono)}
.co .ring{fill:none;stroke:#3fe0a8;stroke-width:4}
ol.notes{margin:0;padding-left:22px;display:flex;flex-direction:column;gap:10px;max-width:60ch}
ol.notes b{font-weight:600}
.ok{color:var(--accent);font-weight:600}
.warn{color:var(--w3v3);font-weight:600}
.tbl{overflow-x:auto}
table{border-collapse:collapse;width:100%;min-width:620px;font-size:14px}
th{font-family:var(--mono);font-size:11.5px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);text-align:left;padding:10px 12px;border-bottom:1px solid var(--line)}
td{padding:9px 12px;border-bottom:1px solid var(--line);vertical-align:top}
tr:last-child td{border-bottom:none}
tr.grp td{font-family:var(--display);font-weight:600;font-size:15px;background:color-mix(in srgb,var(--accent) 8%,transparent);color:var(--ink)}
td.pin{font-family:var(--mono);font-variant-numeric:tabular-nums;white-space:nowrap}
td .sw{display:inline-block;width:14px;height:14px;border-radius:3px;vertical-align:-2px;margin-right:7px}
tr.done td:not(:first-child){opacity:.45}
input[type=checkbox]{width:18px;height:18px;accent-color:var(--accent)}
input[type=checkbox]:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.foot{font-size:13px;color:var(--muted)}
/* ── SVG ── */
svg text{font-family:var(--mono)}
.w path{fill:none;stroke-linejoin:round;stroke-linecap:round}
.w .halo{stroke:var(--halo);stroke-width:8}
.w .core{stroke-width:3.4}
.w.rail .core{stroke-width:6}.w.rail .halo{stroke-width:10}
.w3v3 .core{stroke:var(--w3v3)} .wgnd .core{stroke:var(--gnd)} .wsda .core{stroke:var(--wsda)}
.wscl .core{stroke:var(--wscl)} .wsd .core{stroke:var(--wsd)} .wsck .core{stroke:var(--wsck)}
.wws .core{stroke:var(--wws)} .wdq .core{stroke:var(--wdq)} .wct .core{stroke:var(--wct)}
.wlux .core{stroke:var(--wlux)} .wpas .core{stroke:var(--wpas);stroke-width:2.4}
.wcable .core{stroke:var(--wcable);stroke-width:7}
.node{fill:var(--ink)}
.railt{font-size:13px;font-weight:700}.w3t{fill:var(--w3v3)}.wgt{fill:var(--gnd)}
.lbl,.small{fill:var(--muted);font-size:12px}
.part{fill:var(--ink);font:600 17px var(--display)}.part.sm{font-size:15px}
.nc{fill:#c9d4e6;font-size:10px}
.usb{fill:#a3a9ae}.c3pcb{fill:#111315}.silk{fill:#f1f1ee}.silkt{fill:#111;font-size:9px;font-weight:700}
.pad{fill:#d1a536;stroke:#8d6b14;stroke-width:1.5}.pad.used{stroke:#3fe0a8;stroke-width:3}
.hole{fill:#2a2a2a}
.pint{fill:#eef2f6;font-size:13px;font-weight:600}.pinw{fill:#eef2f6;font-size:11.5px;font-weight:600}
.enspcb{fill:#1e4fa6}.mhole{fill:var(--bg);stroke:#c9a33e;stroke-width:3}
.chipw{fill:#f4f4f2}.chips{fill:#b9c0c7}.chipt{fill:#dfe8ff;font-size:10.5px}.chipt2{fill:#ffd27a;font-size:10px;font-style:italic}
.heat{fill:none;stroke:#ffb347;stroke-width:2;stroke-dasharray:4 3}
.mring{fill:#b58a2e}.mpcb{fill:#15171a}
.flag rect{stroke:none}.f3v3 rect{fill:var(--w3v3)}.fgnd rect{fill:var(--gnd)}
.flagt{fill:var(--panel);font-size:10.5px;font-weight:700}
.jump{fill:none;stroke-width:3}.jump.f3v3{stroke:var(--w3v3)}.jump.fgnd{stroke:var(--gnd)}
.res{fill:#e7d9b5;stroke:var(--wpas);stroke-width:1.5}
.cap{stroke:var(--wpas);stroke-width:3;fill:none}
.conn{fill:#2b3236;stroke:#7b858c;stroke-width:2}.conntext{fill:#eef2f6;font-size:11px;font-weight:700}
.probe{fill:#c7ced3;stroke:#7d878d;stroke-width:1.5}
.temtpcb{fill:#1e4fa6}.lsens{fill:#2b2b2b;stroke:#cfd6dd}
.jack{fill:#2b3236;stroke:#7b858c;stroke-width:2}
.phase{stroke:#6b6b6b;stroke-width:9;stroke-linecap:round;fill:none}
.clamp{fill:none;stroke:#2b2b2b;stroke-width:12}
.scdpcb{fill:#121a22}
.wbat .core{stroke:var(--wbat)} .sw3 .core{stroke-dasharray:10 6}
.swl{stroke:var(--ink);stroke-width:3.4;fill:none;stroke-linecap:round}
.swd{fill:var(--panel);stroke:var(--ink);stroke-width:2.5}
.dio{fill:var(--ink)} .diol{stroke:var(--ink);stroke-width:3.4}
.cell{fill:#2f7d4f;stroke:#1d5233;stroke-width:2}.cellcap{fill:#9aa3a8}
.cellt{fill:#eaf5ee;font-size:12px;font-weight:700}
.f3v3s rect{fill:var(--w3v3);stroke:var(--panel);stroke-width:1.5;stroke-dasharray:3 2}
.fvbat rect{fill:var(--wbat)} .fsda rect{fill:var(--wsda)} .fscl rect{fill:var(--wscl)}
.netl{font-size:11.5px;font-weight:700}.nsda{fill:var(--wsda)}.nscl{fill:var(--wscl)}
"""

LEG = [("3V3","--w3v3"),("GND","--gnd"),("SDA · GPIO6","--wsda"),("SCL · GPIO7","--wscl"),
       ("SD · GPIO5","--wsd"),("SCK · GPIO20","--wsck"),("WS · GPIO21","--wws")]
LEG2 = [("DQ sondas · GPIO4","--wdq"),("TC · GPIO1","--wct"),("luz · GPIO3","--wlux"),("passivos","--wpas")]
LEG4 = [("VBAT","--wbat"),("3V3S (comutado)","--w3v3"),("GND","--gnd"),("SDA · 6","--wsda"),("SCL · 7","--wscl"),("SD · 5","--wsd"),("SCK · 20","--wsck"),("WS · 21","--wws"),("DQ · 4","--wdq"),("TC · 1","--wct"),("luz · 3","--wlux"),("passivos","--wpas")]
LEG3 = [("bateria / VBAT","--wbat"),("3V3 (tracejado = comutado por SW2)","--w3v3"),("GND","--gnd"),("divisor → GPIO0","--wpas")]
def legend(items): return '<div class="legend">' + "".join(f'<span><i style="background:var({c})"></i>{t}</span>' for t,c in items) + '</div>'

ROWS = [
 ("Caixa central — já na bancada", None),
 ("Alimentação 3,3 V", "3.3", "ENS VIN · INMP441 VDD · SCD41 VDD · sondas · TEMT6000 · divisor do TC", "--w3v3"),
 ("Terra", "G", "ENS GND · INMP441 GND e L/R · SCD41 · sondas · TEMT6000 · jack", "--gnd"),
 ("I2C SDA", "6", "ENS160/AHT21 SDA · SCD41 SDA", "--wsda"),
 ("I2C SCL", "7", "ENS160/AHT21 SCL · SCD41 SCL", "--wscl"),
 ("ENS160 CS", "—", "3V3 (na placa: jumper CS→VIN). Sem isto o ENS160 entra em SPI", "--w3v3"),
 ("ENS160 ADD", "—", "GND → endereço 0x52, o que o firmware usa", "--gnd"),
 ("Microfone SD", "5", "INMP441 SD", "--wsd"),
 ("Microfone SCK", "20", "INMP441 SCK", "--wsck"),
 ("Microfone WS", "21", "INMP441 WS", "--wws"),
 ("Microfone L/R", "—", "GND (canal esquerdo)", "--gnd"),
 ("Portas do analisador de AC — opcionais", None),
 ("Sonda EXT (DQ)", "4", "DS18B20 da condensadora + o ÚNICO 4k7 do barramento para 3V3", "--wdq"),
 ("Sonda SUP (DQ)", "4", "mesmo fio da EXT (barramento 1-Wire); sem 2º pull-up", "--wdq"),
 ("Sonda RET (DQ)", "4", "idem — ar de retorno da evaporadora", "--wdq"),
 ("Corrente (TC)", "1", "ponta do jack; anel no bias 10k/6k8 + 10 µF; chave do jack → GND", "--wct"),
 ("Luz", "3", "TEMT6000 SIG", "--wlux"),
 ("Alimentação por bateria — opcional", None),
 ("Bateria → TP4056", "—", "Li-ion + → B+, − → B− (módulo TP4056 com proteção DW01)", "--wbat"),
 ("TP4056 OUT+ → SW1", "—", "chave liga/desliga no positivo", "--wbat"),
 ("SW1 → D1 → 5V", "5V", "Schottky SS34 (anodo na chave, catodo no 5V do C3)", "--wbat"),
 ("Divisor VBAT", "0", "100k de VBAT (após SW1) ao GPIO0, 100k do GPIO0 ao GND", "--wpas"),
 ("TP4056 OUT−", "G", "GND comum (bateria, C3, sensores)", "--gnd"),
 ("3.3 → SW2", "3.3", "SW2 corta o 3V3 de TODOS os sensores; o C3 continua ligado", "--w3v3"),
]
def table():
    out, i = [], 0
    for r in ROWS:
        if r[1] is None:
            out.append(f'<tr class="grp"><td colspan="4">{r[0]}</td></tr>'); continue
        i += 1
        out.append(f'<tr><td><input type="checkbox" id="chk{i}" aria-label="ligado: {r[0]}"></td>'
                   f'<td><span class="sw" style="background:var({r[3]})"></span>{r[0]}</td>'
                   f'<td class="pin">{r[1]}</td><td>{r[2]}</td></tr>')
    return ('<div class="tbl"><table><thead><tr><th>ok</th><th>Sinal</th><th>ESP32-C3</th><th>Liga em</th></tr></thead><tbody>'
            + "".join(out) + "</tbody></table></div>")

OVER = '''<svg viewBox="0 0 690 660" aria-hidden="true"><g class="co">
<circle class="ring" cx="160" cy="135" r="110"/><circle cx="62" cy="40" r="20"/><text x="55" y="48">1</text>
<circle class="ring" cx="188" cy="368" r="74"/><circle cx="110" cy="300" r="20"/><text x="103" y="308">2</text>
<circle class="ring" cx="470" cy="403" r="26"/><circle cx="530" cy="446" r="20"/><text x="523" y="454">3</text>
<circle class="ring" cx="445" cy="310" r="140" style="stroke-dasharray:10 8"/><circle cx="600" cy="200" r="20"/><text x="593" y="208">4</text>
</g></svg>'''

html = f'''<title>AirQ Ligações</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600;700&family=IBM+Plex+Sans+Condensed:wght@600&family=IBM+Plex+Sans:wght@400;600&display=swap" rel="stylesheet">
<style>{CSS}</style>
<main>
<header>
  <span class="eyebrow">Luctronics · Aguada node · env ani-01-c3</span>
  <h1>AirQ — ligações da bancada</h1>
  <p class="lede">Caixa central (ESP32-C3 SuperMini, ENS160+AHT21, INMP441) e as portas opcionais do analisador de ar-condicionado. Placas desenhadas com a mesma disposição de pinos das suas: o SuperMini visto pelo lado da serigrafia, como na foto.</p>
</header>

<section>
  <div class="sec-hd"><h2>Sua bancada</h2><span class="tag">foto de 14/10</span></div>
  <div class="photo">
    <div class="ph"><img alt="Bancada: placa ENS160+AHT21, microfone INMP441 e ESP32-C3 SuperMini" src="data:image/jpeg;base64,{photo}">{OVER}</div>
    <ol class="notes">
      <li><b>ENS160 + AHT21.</b> Alimentada pelo <b>VIN</b> (fio vermelho), com jumper <b>CS→VIN</b> (azul) e <b>ADD→GND</b> (preto). <span class="ok">Correto.</span> SDA e SCL ainda sem fio.</li>
      <li><b>INMP441.</b> Pinos SCK · WS · L/R de um lado, SD · VDD · GND do outro. <b>L/R vai ao GND</b>; se ficar solto o canal fica indefinido e o firmware lê zeros.</li>
      <li><b>GPIO 20 e 21 expostos</b> neste SuperMini (TENSTAR). <span class="ok">A pinagem do microfone serve sem mudança.</span></li>
      <li><b>ESP32-C3 SuperMini.</b> Lado esquerdo 5V · G · 3.3 · 4 · 3 · 2 · 1 · 0; lado direito 5 · 6 · 7 · 8 · 9 · 10 · 20 · 21. Evite 2, 8 e 9 (pinos de boot).</li>
    </ol>
  </div>
</section>

<section>
  <div class="sec-hd"><h2>1 · Caixa central</h2><span class="tag">ENS160+AHT21 · INMP441</span></div>
  <div class="panel diagram">{gen.diagram_core()}</div>
  {legend(LEG)}
  <p>As etiquetas <b>3V3</b> e <b>GND</b> ligam ao barramento do mesmo nome. Os pinos com aro verde são os que levam fio. Fios que se cruzam não estão ligados; ligação é sempre marcada com ponto.</p>
</section>

<section>
  <div class="sec-hd"><h2>2 · Portas do analisador de AC</h2><span class="tag">opcionais · detectadas sozinhas</span></div>
  <div class="panel diagram">{gen.diagram_ac()}</div>
  {legend(LEG2)}
  <p>As três sondas partilham o <b>mesmo fio de dados</b> (GPIO4, um só pull-up de 4k7). O firmware reconhece cada uma pelo ID gravado no chip: a 1ª que aparece vira EXT, a 2ª SUP, a 3ª RET, e isso fica salvo. Para conferir ou corrigir, use <b>Identificar sondas</b> no portal (192.168.4.1): aqueça uma sonda com a mão, a que sobe fica em destaque, atribua o papel e cole a etiqueta com o ID de 4 caracteres.</p>
  <p>Com a sonda <b>RET</b> o ΔT da evaporadora é <b>retorno − insuflamento</b> (normal 8–12 °C); sem ela, cai para o AHT21 da caixa, que lê alto. O jack do TC precisa de <b>contacto de comutação</b>: sem plugue, a chave põe o GPIO1 no GND e o card de corrente some do portal.</p>
</section>

<section>
  <div class="sec-hd"><h2>3 · Alimentação por bateria</h2><span class="tag">opcional · Li-ion + TP4056</span></div>
  <div class="panel diagram">{gen.diagram_power()}</div>
  {legend(LEG3)}
  <p><b>SW1</b> liga/desliga o aparelho todo. A carga é pelo <b>USB-C do TP4056</b> (com SW1 desligado também carrega). <b>D1</b> evita que o 5 V do USB do C3 entre na bateria quando você grava o firmware com SW1 ligado. O divisor 100k/100k depois de SW1 leva VBAT/2 ao <b>GPIO0</b> (4,2 V → 2,1 V, dentro da faixa linear do ADC) e não drena a bateria com o aparelho desligado; no firmware basta habilitar <code>vbat</code> (pino 0, divisor 2 — já são os padrões).</p>
  <p><b>SW2</b> corta o 3V3 dos sensores e deixa o C3 ligado. Desligue SW2 só com o C3 desligado, ou reinicie depois de religar: o ENS160 e o AHT21 só são detectados no arranque.</p>
</section>

<section>
  <div class="sec-hd"><h2>4 · Sistema completo</h2><span class="tag">bateria + C3 + todos os sensores</span></div>
  <div class="panel diagram">{gen.diagram_full()}</div>
  {legend(LEG4)}
  <p>Tudo junto numa folha só. Para não virar um emaranhado, a alimentação usa <b>etiquetas de rede</b>: toda etiqueta <b>3V3S</b> (3,3 V que passa pela chave SW2) está ligada às outras 3V3S, toda <b>VBAT</b> às outras VBAT, e o mesmo para GND. O SCD41 entra no mesmo I2C do ENS160 pelas etiquetas SDA/SCL. Os sinais (I2C, I2S, 1-Wire, ADC) estão desenhados fio a fio. Conector de cada sonda, da esquerda para a direita: VDD · DQ · GND.</p>
</section>

<section>
  <div class="sec-hd"><h2>Checklist de ligação</h2><span class="tag">marque ao soldar</span></div>
  <div class="panel">{table()}</div>
</section>

<section>
  <h2>Cuidados</h2>
  <ol class="notes">
    <li><b>Tudo em 3,3 V.</b> Nada de sensor no pino 5V do SuperMini: o INMP441 e o SCD41 não aguentam, e o ADC do C3 só lê até ~2,5 V.</li>
    <li><b>VIN ou 3V3 na placa azul.</b> Com 3,3 V no VIN o regulador da placa entrega ~3,1–3,2 V, que basta. Ligar direto no pino 3V3 também serve; não ligue os dois.</li>
    <li><b>AHT21 lê alto.</b> Na bancada (17/10) leu ~33 °C com as sondas em ~28 °C: o C3 e o ENS160 aquecem a placa. Afaste-a do ESP32, com ventilação; para o ΔT use a sonda RET.</li>
    <li><b>Pull-ups do I2C.</b> A placa azul e o módulo SCD41 já trazem os seus. Dois de 10 kΩ em paralelo dão ~5 kΩ, o que está bem a 50 kHz.</li>
    <li><b>Sondas em cabo.</b> DS18B20 em 3 fios, nunca em modo parasita. Conector M8 ou RJ11 (P2 faz curto ao entrar). Barramento em estrela: somando os cabos, até ~5 m com 4k7; acima disso, 2k2. TVS + 100 Ω em série no DQ de cada conector se o cabo for longo.</li>
    <li><b>Microfone em dB(A).</b> O nível sai ponderado A (padrão de ruído ambiente) a 32 kHz. Ventilador e TV aparecem: na bancada deu 67–71 dB(A). Para conferir, compare com um app de sonómetro ao lado e tape o furo do INMP441 com o dedo — o nível tem de cair.</li>
    <li><b>Autonomia na bateria.</b> O portal Wi-Fi fica sempre ligado (~80–120 mA no C3) e o ENS160 aquece (~30 mA): uma 18650 de 2500 mAh dura ~1 dia. Para semanas é preciso sono profundo com o portal só sob demanda.</li>
    <li><b>Queda de tensão.</b> Com D1 (~0,3 V) e o regulador do C3, abaixo de ~3,6 V na bateria o 3,3 V começa a cair. O DW01 do TP4056 corta em ~2,5 V; na prática, recarregue quando o VBAT passar de ~3,5 V.</li>
    <li><b>TC num só condutor.</b> A garra abraça só a fase; o cabo inteiro (fase + neutro) dá zero. Até ~25 A com o SCT-013-030; acima disso o portal mostra "TC SATURADO".</li>
  </ol>
</section>
<p class="foot">Fonte: firmware/node (Node AirQ.md, node_pinout.md) e foto da bancada. Diagramas gerados a partir das coordenadas dos pinos.</p>
</main>
<script>
document.querySelectorAll('input[type=checkbox]').forEach(function(c){{
  var k='airq-chk-'+c.id;
  try{{ if(localStorage.getItem(k)==='1'){{c.checked=true;}} }}catch(e){{}}
  var row=c.closest('tr'); row.classList.toggle('done',c.checked);
  c.addEventListener('change',function(){{ row.classList.toggle('done',c.checked);
    try{{ localStorage.setItem(k,c.checked?'1':'0'); }}catch(e){{}} }});
}});
</script>
'''
open('../airq_ligacoes.html', 'w').write(html)
print(len(html))

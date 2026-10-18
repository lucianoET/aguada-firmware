#pragma once

// HTML da página de estado AirQ (ANI-01) servida pelo AP captive.
// Armazenado em flash (PROGMEM). Acedido via http://192.168.4.1/
// Phone-first, organizado por componentes (node ESP32 > sensores AHT21/ENS160).
// Single-file, sem dependências externas (o AP não tem internet).

static const char ANI_WEB_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>AirQ — Analisador</title>
<style>
:root{
  --bg:#f1f5f9;--s:#ffffff;--b:#e2e8f0;--t:#0f172a;--mu:#64748b;--mu2:#94a3b8;
  --ac:#4f46e5;--sh:0 1px 2px rgba(0,0,0,.07);
}
@media (prefers-color-scheme:dark){
  :root{
    --bg:#0b1120;--s:#111827;--b:#1f2937;--t:#f1f5f9;--mu:#94a3b8;--mu2:#64748b;
    --ac:#818cf8;--sh:0 1px 2px rgba(0,0,0,.4);
  }
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--t);font-family:system-ui,-apple-system,sans-serif;
  padding:10px 10px env(safe-area-inset-bottom);max-width:520px;margin:0 auto;-webkit-text-size-adjust:100%}

/* ── Node (ESP32) component ─────────────────────────────────────── */
.node{background:var(--s);border:1px solid var(--b);border-radius:14px;box-shadow:var(--sh);overflow:hidden}
.node-hd{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:10px 13px;border-bottom:1px solid var(--b)}
#dot{width:9px;height:9px;border-radius:50%;background:var(--mu2);flex-shrink:0;transition:background .4s,box-shadow .4s}
#dot.ok{background:#16a34a;box-shadow:0 0 0 3px rgba(22,163,74,.22);animation:pulse 2s infinite}
#dot.warn{background:#d97706;box-shadow:0 0 0 3px rgba(217,119,6,.22)}
#dot.err{background:#dc2626;box-shadow:0 0 0 3px rgba(220,38,38,.22)}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(22,163,74,.4)}70%{box-shadow:0 0 0 5px rgba(22,163,74,0)}100%{box-shadow:0 0 0 0 rgba(22,163,74,0)}}
.ttl{font-size:.95rem;font-weight:800}
.ttl small{font-weight:600;color:var(--mu);font-size:.78rem}
#node{font-size:.64rem;font-weight:700;color:var(--ac);background:rgba(79,70,229,.12);
  padding:2px 8px;border-radius:999px;font-variant-numeric:tabular-nums}
@media (prefers-color-scheme:dark){#node{background:rgba(129,140,248,.16)}}
#sl{font-size:.7rem;color:var(--mu);font-weight:600;margin-left:auto;text-align:right}
.node-bd{padding:9px;display:flex;flex-direction:column;gap:9px}
.node-ft{border-top:1px solid var(--b);padding:8px 12px;display:flex;flex-wrap:wrap;gap:5px 16px;align-items:center}

/* ── Sensor sub-component ───────────────────────────────────────── */
.sensor{background:var(--bg);border:1px solid var(--b);border-radius:11px;padding:8px}
.sensor-hd{display:flex;align-items:center;gap:7px;margin:1px 3px 7px}
.sensor-hd b{font-size:.64rem;text-transform:uppercase;letter-spacing:.05em;color:var(--t);font-weight:800}
.sensor-hd span{font-size:.62rem;color:var(--mu);font-weight:600}
.sensor-hd .pill{margin-left:auto}
.btn{padding:4px 10px;border:1px solid var(--b);border-radius:6px;background:var(--s);color:var(--t);font:inherit;font-size:.72rem;cursor:pointer;margin-left:auto}
.prow{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:6px 8px;border:1px solid var(--b);border-radius:8px;margin-top:6px;font-size:.78rem}
.prow.hot{border-color:#dc2626;background:rgba(220,38,38,.08)}
.pid{font:700 1rem ui-monospace,Menlo,monospace;letter-spacing:.06em}
.prow .pt{margin-left:auto;font-variant-numeric:tabular-nums}
.prow .pd{min-width:52px;text-align:right;font-variant-numeric:tabular-nums;color:var(--mu)}
.prow.hot .pd{color:#dc2626;font-weight:700}
.prow select{font:inherit}
.cards{display:flex;flex-direction:column;gap:7px}

/* ── Metric cards ───────────────────────────────────────────────── */
.card{background:var(--s);border:1px solid var(--b);border-radius:9px;padding:8px 12px;box-shadow:var(--sh)}
.card.hl{border-left:3px solid var(--ac)}
.cl{font-size:.6rem;text-transform:uppercase;letter-spacing:.05em;color:var(--mu);font-weight:600}
.bodyr{display:flex;align-items:center;gap:12px;margin-top:1px}
.nums{display:flex;align-items:baseline;gap:5px;flex:0 0 auto;min-width:92px}
.cv{font-size:1.6rem;font-weight:800;line-height:1.02;font-variant-numeric:tabular-nums}
.cu{font-size:.72rem;color:var(--mu)}
.cs{font-size:.72rem;font-weight:700;margin-left:2px}
.spark{flex:1;min-width:0;height:28px;display:block;overflow:visible}
.spark polyline{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round;vector-effect:non-scaling-stroke}

/* ── AQI 5-segment index ────────────────────────────────────────── */
.aqihd{display:flex;align-items:baseline;justify-content:space-between;gap:8px}
.aqicl{font-size:.8rem;font-weight:700}
.aqiseg{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:7px}
.aqiseg span{text-align:center;padding:7px 0;border-radius:8px;font-weight:800;font-size:.9rem;
  border:1.5px solid transparent;transition:transform .25s,box-shadow .25s,background .25s}
.aqiseg span.l1{color:#16a34a;background:rgba(22,163,74,.13)}
.aqiseg span.l2{color:#65a30d;background:rgba(101,163,13,.13)}
.aqiseg span.l3{color:#d97706;background:rgba(217,119,6,.13)}
.aqiseg span.l4{color:#ea580c;background:rgba(234,88,12,.13)}
.aqiseg span.l5{color:#dc2626;background:rgba(220,38,38,.13)}
.aqiseg span.on{color:#fff;transform:translateY(-1px);box-shadow:0 2px 6px rgba(0,0,0,.18)}
.aqiseg span.on.l1{background:#16a34a}.aqiseg span.on.l2{background:#65a30d}
.aqiseg span.on.l3{background:#d97706}.aqiseg span.on.l4{background:#ea580c}.aqiseg span.on.l5{background:#dc2626}

/* ── Warmup + diagnostics + refs ────────────────────────────────── */
#warm{display:none;margin-bottom:7px;padding:7px 11px;border-radius:8px;font-size:.72rem;line-height:1.35;
  background:rgba(217,119,6,.12);border:1px solid rgba(217,119,6,.35);color:#b45309}
@media (prefers-color-scheme:dark){#warm{color:#fbbf24}}
#warm.on{display:block}
.di{display:flex;flex-direction:column;line-height:1.15}
.dk{font-size:.56rem;text-transform:uppercase;letter-spacing:.05em;color:var(--mu);font-weight:600}
.dv{font-size:.8rem;font-weight:700;font-variant-numeric:tabular-nums}
.pill{display:inline-block;font-size:.64rem;font-weight:700;padding:1px 7px;border-radius:999px}
.pill.ok{background:rgba(22,163,74,.15);color:#16a34a}
.pill.bad{background:rgba(220,38,38,.15);color:#dc2626}
.refs{margin-top:7px}
.refs summary{cursor:pointer;list-style:none;background:var(--s);border:1px solid var(--b);border-radius:8px;
  padding:7px 11px;text-align:center;font-size:.7rem;font-weight:700;color:var(--mu);
  text-transform:uppercase;letter-spacing:.04em;box-shadow:var(--sh)}
.refs summary::-webkit-details-marker{display:none}
.refs summary:before{content:"▸ "}.refs[open] summary:before{content:"▾ "}
.rb{background:var(--s);border:1px solid var(--b);border-radius:9px;padding:9px 11px;box-shadow:var(--sh);margin-top:7px}
.rt{font-size:.62rem;text-transform:uppercase;letter-spacing:.05em;color:var(--mu);font-weight:700;margin-bottom:6px}
table{width:100%;border-collapse:collapse;font-size:.72rem}
th{text-align:left;color:var(--mu);font-weight:600;padding:2px 5px 4px;border-bottom:1px solid var(--b)}
td{padding:3px 5px;border-bottom:1px solid var(--b);vertical-align:middle}
tr:last-child td{border-bottom:none}
.d{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px;vertical-align:middle}
.g{color:#16a34a}.f{color:#65a30d}.m{color:#d97706}.p{color:#ea580c}.r{color:#dc2626}

/* ── Modos (abas) ───────────────────────────────────────────────── */
:root{--ac:#0e7490;--run:#b45309;--ok:#15803d;--wa:#b45309;--bad:#b91c1c;
  --s1:#2a78d6;--s2:#eb6834;--s3:#1baf7a;--s4:#7c3aed}
@media (prefers-color-scheme:dark){:root{--ac:#38bdf8;--run:#f59e0b;--ok:#22c55e;--wa:#f59e0b;--bad:#f87171;
  --s1:#3987e5;--s2:#d95926;--s3:#199e70;--s4:#a78bfa}}
.tabs{position:sticky;top:0;z-index:5;display:grid;grid-template-columns:repeat(4,1fr);gap:4px;
  padding:6px;background:var(--s);border-bottom:1px solid var(--b)}
.tabs button{font:inherit;font-size:.78rem;font-weight:700;padding:9px 2px;border-radius:9px;border:1px solid transparent;
  background:transparent;color:var(--mu);cursor:pointer}
.tabs button[aria-selected="true"]{background:var(--ac);color:#fff}
@media (prefers-color-scheme:dark){.tabs button[aria-selected="true"]{color:#06202a}}
.tabs button:focus-visible,.pbtn:focus-visible,.gbtn:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--ac);outline-offset:2px}
.tabs button .rec{display:none;width:7px;height:7px;border-radius:50%;background:var(--run);margin-left:5px;vertical-align:1px}
.tabs button.recording .rec{display:inline-block;animation:blink 1.2s infinite}
@keyframes blink{50%{opacity:.25}}
@media (prefers-reduced-motion:reduce){.tabs button.recording .rec,#dot.ok{animation:none}}
.view{display:none}.view.on{display:flex;flex-direction:column;gap:9px}
.box{background:var(--s);border:1px solid var(--b);border-radius:12px;padding:12px 13px}
.box h2{font-size:1rem;font-weight:800;margin-bottom:3px}
.box h3{font-size:.86rem;font-weight:800;margin:10px 0 4px}
.lead{font-size:.8rem;color:var(--mu);line-height:1.45}
.frm{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.frm label{display:flex;flex-direction:column;gap:3px;font-size:.74rem;font-weight:600;color:var(--mu)}
.frm label.wide{grid-column:1/-1}
.frm input,.frm select,.cal input{font:inherit;font-size:.92rem;color:var(--t);background:var(--bg);
  border:1px solid var(--b);border-radius:8px;padding:8px 9px;width:100%}
.pbtn{font:inherit;font-weight:800;font-size:.9rem;padding:11px 14px;border-radius:10px;border:0;
  background:var(--ac);color:#fff;cursor:pointer;width:100%;margin-top:10px}
@media (prefers-color-scheme:dark){.pbtn{color:#06202a}}
.pbtn.stop{background:var(--run)}
.pbtn:disabled{opacity:.45;cursor:default}
.gbtn{font:inherit;font-weight:700;font-size:.8rem;padding:8px 11px;border-radius:9px;border:1px solid var(--b);
  background:var(--s);color:var(--t);cursor:pointer}
.row{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}
.chk{list-style:none;margin-top:8px;display:flex;flex-direction:column;gap:4px;font-size:.82rem}
.chk li{display:flex;gap:7px;align-items:baseline}
.chk .i{width:1.1em;text-align:center;font-weight:800}
.i.ok{color:var(--ok)}.i.wa{color:var(--wa)}.i.bad{color:var(--bad)}.i.na{color:var(--mu2)}
.clock{display:flex;align-items:baseline;gap:8px;margin-top:2px}
.clock b{font-size:2.3rem;font-weight:800;font-variant-numeric:tabular-nums;letter-spacing:-.01em}
.clock span{color:var(--mu);font-size:.85rem;font-variant-numeric:tabular-nums}
.bar{height:6px;border-radius:3px;background:var(--b);overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;background:var(--run);width:0;transition:width .6s}
.now{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:10px}
.now div{display:flex;flex-direction:column;font-size:.68rem;color:var(--mu);font-weight:600}
.now b{font-size:1.15rem;color:var(--t);font-variant-numeric:tabular-nums}
.chart{position:relative;margin-top:6px}
.chart svg{display:block;width:100%;height:auto;overflow:visible;touch-action:pan-y}
.chart .gl{stroke:var(--b);stroke-width:1}
.chart .ax{fill:var(--mu);font-size:10px;font-variant-numeric:tabular-nums}
.chart .ln{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.chart .xh{stroke:var(--mu2);stroke-width:1;stroke-dasharray:3 3}
.leg{display:flex;flex-wrap:wrap;gap:4px 12px;font-size:.74rem;color:var(--mu);margin-top:4px}
.leg i{display:inline-block;width:14px;height:3px;border-radius:2px;vertical-align:3px;margin-right:5px}
.tip{position:absolute;top:0;pointer-events:none;background:var(--t);color:var(--s);font-size:.72rem;
  border-radius:7px;padding:5px 7px;white-space:nowrap;display:none;font-variant-numeric:tabular-nums;line-height:1.35}
.ctitle{font-size:.76rem;font-weight:700;color:var(--mu);margin-top:12px}
.verdict{display:flex;align-items:center;gap:12px;border-radius:12px;padding:14px;margin-top:4px;color:#fff}
.verdict .vi{font-size:1.6rem;font-weight:900;line-height:1;width:44px;height:44px;border-radius:50%;flex-shrink:0;
  display:flex;align-items:center;justify-content:center;background:rgba(255,255,255,.22)}
.verdict b{font-size:1.45rem;font-weight:900;display:block;line-height:1.1}
.verdict span{font-size:.8rem;opacity:.95}
.verdict.ok{background:#15803d}.verdict.wa{background:#b45309}.verdict.bad{background:#b91c1c}.verdict.na{background:#64748b}
.crit{margin-top:10px;display:flex;flex-direction:column}
.crit .c{display:grid;grid-template-columns:1.2em 1fr auto;gap:2px 8px;padding:9px 0;border-top:1px solid var(--b);align-items:baseline}
.crit .c:first-child{border-top:0}
.crit .c b{font-size:.86rem}
.crit .c .v{font-weight:800;font-variant-numeric:tabular-nums;font-size:.9rem;text-align:right}
.crit .c p{grid-column:2/-1;font-size:.76rem;color:var(--mu);line-height:1.4}
.note{font-size:.72rem;color:var(--mu);margin-top:10px;line-height:1.45}
.cal{display:flex;flex-direction:column}
.cal .cr{display:grid;grid-template-columns:1fr auto;gap:2px 10px;padding:10px 0;border-top:1px solid var(--b)}
.cal .cr:first-child{border-top:0}
.cal .cr b{font-size:.88rem}
.cal .cr .rd{font-size:.78rem;color:var(--mu);font-variant-numeric:tabular-nums;text-align:right}
.cal .cr .rd strong{color:var(--t)}
.cal .cr p{grid-column:1/-1;font-size:.74rem;color:var(--mu);line-height:1.4}
.cal .ap{grid-column:1/-1;display:flex;gap:6px;margin-top:5px}
.cal .ap input{flex:1;min-width:0}
.ptab{width:100%;border-collapse:collapse;font-size:.8rem;margin-top:8px;font-variant-numeric:tabular-nums}
.ptab th{font-size:.7rem;color:var(--mu);font-weight:600;text-align:right;padding:3px 4px;border-bottom:1px solid var(--b)}
.ptab th:first-child,.ptab td:first-child{text-align:left}
.ptab td{text-align:right;padding:6px 4px;border-bottom:1px solid var(--b)}
.msg{font-size:.78rem;font-weight:700;margin-top:8px;min-height:1em}
.msg.ok{color:var(--ok)}.msg.bad{color:var(--bad)}
.monlink{font-size:.76rem;color:var(--ac);margin-top:8px;display:inline-block}

.node{overflow:visible}
</style>
</head>
<body>

<div class="node">
  <!-- header do node -->
  <div class="node-hd">
    <div id="dot"></div>
    <div class="ttl">AirQ <small>· Qualidade do Ar</small></div>
    <span id="node">ARQ-????</span>
    <span id="sl">a carregar…</span>
  </div>
  <nav class="tabs" role="tablist" aria-label="Modo de uso">
    <button id="t-mon" role="tab" aria-selected="true" type="button" onclick="tab('mon')">Monitor</button>
    <button id="t-ac" role="tab" aria-selected="false" type="button" onclick="tab('ac')">ArCond<span class="rec"></span></button>
    <button id="t-env" role="tab" aria-selected="false" type="button" onclick="tab('env')">Ambiente<span class="rec"></span></button>
    <button id="t-cal" role="tab" aria-selected="false" type="button" onclick="tab('cal')">Calibração</button>
  </nav>

  <!-- body do node: um container por sensor -->
  <div class="node-bd">
  <div class="view on" id="v-mon" role="tabpanel">

    <!-- sensor SCD41 (CO2 real, opcional) -->
    <div class="sensor" id="scdbox" style="display:none">
      <div class="sensor-hd"><b>SCD41</b><span>· CO₂ real (NDIR)</span><span class="pill" id="d_scd">—</span></div>
      <div class="cards">
        <div class="card hl">
          <span class="cl">CO₂ · limite RE 09/2003: 1000 ppm</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="co2">—</span><span class="cu">ppm</span><span class="cs" id="co2s"></span></div>
            <svg class="spark" id="co2k" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
      </div>
    </div>

    <!-- sensor AHT21 -->
    <div class="sensor">
      <div class="sensor-hd"><b>AHT21</b><span>· Temperatura e humidade</span><span class="pill" id="d_aht">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Temperatura</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="temp">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="tempk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
        <div class="card">
          <span class="cl">Humidade relativa</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="hum">—</span><span class="cu">%</span></div>
            <svg class="spark" id="humk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
      </div>
    </div>

    <!-- sensor ENS160 -->
    <div class="sensor">
      <div class="sensor-hd"><b>ENS160</b><span>· Qualidade do ar</span><span class="pill" id="d_ens">—</span></div>
      <div id="warm">⏳ ENS160 a aquecer — eCO₂/TVOC estabilizam nos primeiros ~3 min.</div>
      <div class="cards">
        <div class="card">
          <div class="aqihd"><span class="cl">AQI · índice de qualidade do ar</span><span class="aqicl" id="aqis"></span></div>
          <div class="aqiseg" id="aqiseg">
            <span class="l1">1</span><span class="l2">2</span><span class="l3">3</span><span class="l4">4</span><span class="l5">5</span>
          </div>
        </div>
        <div class="card hl">
          <span class="cl">eCO₂ · estimado por VOC (não vale p/ RE 09)</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="eco2">—</span><span class="cu">ppm</span><span class="cs" id="eco2s"></span></div>
            <svg class="spark" id="eco2k" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
        <div class="card">
          <span class="cl">TVOC · compostos orgânicos voláteis</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="tvoc">—</span><span class="cu">ppb</span><span class="cs" id="tvocs"></span></div>
            <svg class="spark" id="tvock" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
        <details class="refs">
        <summary>Valores de referência</summary>
        <div class="rb">
          <p class="rt">eCO₂ (ppm)</p>
          <table>
            <tr><th>Intervalo</th><th>Nível</th><th>Efeitos</th></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>&lt;400</td><td class="g">Excelente</td><td>Ar exterior</td></tr>
            <tr><td><span class="d" style="background:#65a30d"></span>400–600</td><td class="f">Bom</td><td>Interior ventilado</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>600–1000</td><td class="m">Razoável</td><td>Ligeiro cansaço</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>1000–2000</td><td class="p">Mau</td><td>Sonolência, dificuldade de foco</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;2000</td><td class="r">Muito mau</td><td>Dores de cabeça — ventilar</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">AQI ENS160 (1–5)</p>
          <table>
            <tr><th>Nível</th><th>Classificação</th><th>Acção</th></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>1</td><td class="g">Excelente</td><td>Nenhuma</td></tr>
            <tr><td><span class="d" style="background:#65a30d"></span>2</td><td class="f">Bom</td><td>Nenhuma</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>3</td><td class="m">Moderado</td><td>Considerar ventilação</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>4</td><td class="p">Mau</td><td>Ventilar e reduzir fontes</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>5</td><td class="r">Muito mau</td><td>Abrir janelas imediatamente</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">TVOC (ppb)</p>
          <table>
            <tr><th>Intervalo</th><th>Nível</th><th>Fontes típicas</th></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>&lt;150</td><td class="g">Bom</td><td>Sem fontes relevantes</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>150–500</td><td class="m">Razoável</td><td>Perfumes, limpeza leve</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>500–1500</td><td class="p">Mau</td><td>Tintas, solventes, cozinha</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;1500</td><td class="r">Muito mau</td><td>Ventilar imediatamente</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">Humidade relativa (%)</p>
          <table>
            <tr><th>Intervalo</th><th>Nível</th><th>Risco</th></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>&lt;30%</td><td class="p">Seco</td><td>Irritação de mucosas</td></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>40–60%</td><td class="g">Confortável</td><td>Ideal</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>60–70%</td><td class="m">Húmido</td><td>Possíveis bolores</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;70%</td><td class="r">Muito húmido</td><td>Bolores, ácaros</td></tr>
          </table>
        </div>
        </details>
      </div>
    </div>

    <!-- sensor TEMT6000 -->
    <div class="sensor">
      <div class="sensor-hd"><b>TEMT6000</b><span>· Luminosidade</span><span class="pill" id="d_lux">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Luminosidade ambiente</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="lux">—</span><span class="cu">lux</span><span class="cs" id="luxs"></span></div>
            <svg class="spark" id="luxk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
      </div>
    </div>

    <!-- sensor INMP441 -->
    <div class="sensor">
      <div class="sensor-hd"><b>INMP441</b><span>· Ruído ambiente</span><span class="pill" id="d_snd">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Nível sonoro · dB(A) (janela 0,26 s)</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="db">—</span><span class="cu">dB</span><span class="cs" id="dbs"></span></div>
            <svg class="spark" id="dbk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
          <span class="cu">pico <b id="dbpk">—</b> dB</span>
        </div>
      </div>
    </div>

    <!-- analisador de ar-condicionado (só aparece com sondas e/ou TC ligados) -->
    <div class="sensor" id="acbox" style="display:none">
      <div class="sensor-hd"><b>Ar-condicionado</b><span>· sondas DS18B20 + TC</span><span class="pill" id="d_ac">—</span></div>
      <div class="cards">
        <div class="card hl" id="c_dt">
          <span class="cl" id="dtl">ΔT · retorno − insuflamento</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="dt">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="dtk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
          <span class="cs" id="dts" style="margin-left:0"></span>
        </div>
        <div class="card" id="c_sup">
          <span class="cl">Insuflamento (sonda SUP)</span>
          <div class="bodyr"><div class="nums"><span class="cv" id="tsup">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="tsupk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg></div>
        </div>
        <div class="card" id="c_ret">
          <span class="cl">Retorno · ar que entra na evaporadora (sonda RET)</span>
          <div class="bodyr"><div class="nums"><span class="cv" id="tret">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="tretk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg></div>
        </div>
        <div class="card" id="c_ext">
          <span class="cl">Externo · entrada da condensadora (sonda EXT)</span>
          <div class="bodyr"><div class="nums"><span class="cv" id="text">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="textk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg></div>
        </div>
        <div class="card" id="c_ct">
          <span class="cl">Corrente da condensadora (TC)</span>
          <div class="bodyr"><div class="nums"><span class="cv" id="amps">—</span><span class="cu">A</span><span class="cs" id="cmp"></span></div>
            <svg class="spark" id="ampsk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg></div>
        </div>
      </div>
      <a class="monlink" href="#" onclick="tab('cal');return false">Identificar e calibrar as sondas</a>
    </div>

  </div><!-- /v-mon -->

  <!-- ═══ TESTE DE AR-CONDICIONADO ═══ -->
  <div class="view" id="v-ac" role="tabpanel">
    <div class="box" id="ac-setup">
      <h2>Teste de ar-condicionado</h2>
      <p class="lead">Mede a evaporadora com o compressor a trabalhar: ΔT entre retorno e insuflamento, temperatura de insuflamento, estabilização e corrente. Ligue o aparelho em refrigeração, no frio máximo, e espere 5 min antes de iniciar.</p>
      <ul class="chk" id="ac-pre"></ul>
      <div class="frm">
        <label class="wide">Equipamento / local<input id="ac-tag" maxlength="60" placeholder="ex.: Sala 12 · split 12 000 BTU"></label>
        <label>Corrente nominal (A)<input id="ac-inom" type="text" inputmode="decimal" autocomplete="off" placeholder="placa, ex.: 5,2"></label>
        <label>Duração<select id="ac-dur"><option value="600">10 min</option><option value="900" selected>15 min</option><option value="1200">20 min</option><option value="1800">30 min</option></select></label>
      </div>
      <button class="pbtn" id="ac-go" type="button" onclick="testStart('ac')">Iniciar teste</button>
    </div>
    <div class="box" id="ac-run" hidden>
      <h2>Gravando</h2>
      <div class="clock"><b id="ac-el">00:00</b><span id="ac-of">de 15:00</span></div>
      <div class="bar"><i id="ac-bar"></i></div>
      <div class="now">
        <div>Retorno<b id="ac-nr">—</b></div><div>Insufl.<b id="ac-ns">—</b></div>
        <div>ΔT<b id="ac-nd">—</b></div><div>Corrente<b id="ac-na">—</b></div>
      </div>
      <button class="pbtn stop" type="button" onclick="testStop()">Encerrar agora</button>
      <p class="note">Pode fechar esta página: o aparelho continua a gravar. Ao voltar, o teste reaparece.</p>
    </div>
    <div class="box" id="ac-res" hidden>
      <h2 id="ac-rt">Laudo</h2>
      <p class="lead" id="ac-rs"></p>
      <div id="ac-verdict"></div>
      <div class="crit" id="ac-crit"></div>
      <p class="ctitle">Temperaturas (°C)</p>
      <div class="chart" id="ac-ch1"></div>
      <p class="ctitle" id="ac-ch2t">Corrente da condensadora (A)</p>
      <div class="chart" id="ac-ch2"></div>
      <div class="row">
        <button class="gbtn" type="button" onclick="copyReport()">Copiar laudo</button>
        <button class="gbtn" type="button" onclick="downloadCsv()">Baixar CSV</button>
        <button class="gbtn" type="button" id="ac-new" onclick="newTest('ac')">Novo teste</button>
      </div>
      <p class="msg" id="ac-msg"></p>
      <p class="note">Laudo indicativo, feito com temperaturas de ar e corrente. Antes de concluir falta de gás, confirme com manifold (pressões, superaquecimento e sub-resfriamento).</p>
    </div>
  </div>

  <!-- ═══ TESTE DE AMBIENTE ═══ -->
  <div class="view" id="v-env" role="tabpanel">
    <div class="box" id="env-setup">
      <h2>Teste de ambiente</h2>
      <p class="lead">Avalia o conforto e a qualidade do ar do local contra a RE 09/2003 (ANVISA), a ISO/CIE 8995-1 (iluminação) e a NBR 10152 (ruído). Deixe o AirQ na altura de trabalho (~1,1 m), longe de janelas e da saída do ar-condicionado.</p>
      <ul class="chk" id="env-pre"></ul>
      <div class="frm">
        <label class="wide">Local<input id="env-tag" maxlength="60" placeholder="ex.: Sala de reuniões · 2º andar"></label>
        <label>Uso do local<select id="env-use">
          <option value="500">Escritório / informática (500 lx)</option>
          <option value="500r">Sala de reunião (500 lx)</option>
          <option value="300">Sala de aula (300 lx)</option>
          <option value="300o">Oficina / manutenção (300 lx)</option>
          <option value="100">Circulação / depósito (100 lx)</option></select></label>
        <label>Estação<select id="env-season"><option value="v">Verão</option><option value="i">Inverno</option></select></label>
        <label class="wide">Duração<select id="env-dur"><option value="300">5 min</option><option value="600" selected>10 min</option><option value="900">15 min</option><option value="1800">30 min</option><option value="3600">60 min</option></select></label>
      </div>
      <button class="pbtn" id="env-go" type="button" onclick="testStart('env')">Iniciar teste</button>
    </div>
    <div class="box" id="env-run" hidden>
      <h2>Gravando</h2>
      <div class="clock"><b id="env-el">00:00</b><span id="env-of">de 10:00</span></div>
      <div class="bar"><i id="env-bar"></i></div>
      <div class="now">
        <div>Temp.<b id="env-nt">—</b></div><div>UR<b id="env-nh">—</b></div>
        <div>CO₂<b id="env-nc">—</b></div><div>Ruído<b id="env-nd">—</b></div>
      </div>
      <button class="pbtn stop" type="button" onclick="testStop()">Encerrar agora</button>
      <p class="note">Pode fechar esta página: o aparelho continua a gravar. Evite falar perto do microfone durante o teste.</p>
    </div>
    <div class="box" id="env-res" hidden>
      <h2 id="env-rt">Laudo</h2>
      <p class="lead" id="env-rs"></p>
      <div id="env-verdict"></div>
      <div class="crit" id="env-crit"></div>
      <div class="row">
        <button class="gbtn" type="button" onclick="copyReport()">Copiar laudo</button>
        <button class="gbtn" type="button" onclick="downloadCsv()">Baixar CSV</button>
        <button class="gbtn" type="button" onclick="newTest('env')">Novo teste</button>
      </div>
      <p class="msg" id="env-msg"></p>
      <p class="note">Não medido aqui (exigidos pela RE 09/2003): velocidade do ar (0,025–0,25 m/s), renovação de ar (27 m³/h por pessoa) e contaminantes biológicos. O ruído é uma média de janelas curtas de 0,26 s a cada 8 s, não um Leq contínuo de sonômetro.</p>
    </div>
  </div>

  <!-- ═══ CALIBRAÇÃO ═══ -->
  <div class="view" id="v-cal" role="tabpanel">
    <div class="box">
      <h2>Sondas de temperatura</h2>
      <p class="lead">Junte as sondas (e o termômetro de referência, se tiver) no mesmo lugar, por exemplo amarradas juntas ou num copo d'água, e espere 2 min até as leituras pararem de mudar.</p>
      <table class="ptab" id="cal-pt"></table>
      <div class="row">
        <button class="gbtn" type="button" onclick="calProbesEqual()">Igualar sondas</button>
        <button class="gbtn" type="button" onclick="calProbesZero()">Zerar offsets</button>
      </div>
      <div class="cal"><div class="cr"><b>Termômetro de referência</b><span class="rd">aplica a todas as sondas</span>
        <div class="ap"><input id="cal-pref" type="text" inputmode="decimal" autocomplete="off" placeholder="°C, ex.: 25,4"><button class="gbtn" type="button" onclick="calProbesRef()">Aplicar</button></div></div></div>
      <p class="msg" id="cal-pmsg"></p>
    </div>
    <div class="box" id="swapbox">
      <h2>Identificar sondas</h2>
      <div style="display:flex;align-items:center;gap:8px;flex-wrap:wrap">
        <span id="idst" style="font-size:.76rem;color:var(--mu)"></span>
        <button type="button" class="btn" id="idbtn" onclick="identToggle()">iniciar</button>
      </div>
      <div id="idhelp" style="display:none;font-size:.76rem;color:var(--mu);margin:6px 0 2px">
        Segure <b>uma</b> sonda na mão (~20 s). A que subir fica em destaque: escolha o papel,
        toque em <b>atribuir</b> e cole a etiqueta com o ID. Solte, espere voltar e passe à próxima.
        <a href="#" onclick="identReset();return false">zerar referência</a>
      </div>
      <div id="idlist"></div>
    </div>
    <div class="box">
      <h2>Sensores da caixa</h2>
      <p class="lead">Coloque o instrumento de referência ao lado do AirQ, espere a leitura estabilizar e digite o valor dele. Usa-se a média das últimas leituras.</p>
      <div class="cal" id="cal-list"></div>
      <p class="msg" id="cal-msg"></p>
    </div>
    <div class="box">
      <h2>Zerar calibração</h2>
      <p class="lead">Volta todos os sensores e sondas às leituras de fábrica.</p>
      <button class="gbtn" type="button" onclick="calReset()" style="margin-top:8px">Zerar toda a calibração</button>
    </div>
  </div>

  </div>

  <!-- footer do node: diagnóstico -->
  <div class="node-ft">
    <div class="di"><span class="dk">Node</span><span class="dv" id="d_node">—</span></div>
    <div class="di"><span class="dk">Firmware</span><span class="dv" id="d_fw">—</span></div>
    <div class="di"><span class="dk">Uptime</span><span class="dv" id="d_up">—</span></div>
    <div class="di"><span class="dk">Heap livre</span><span class="dv" id="d_heap">—</span></div>
    <div class="di"><span class="dk">Canal</span><span class="dv">ESP-NOW</span></div>
  </div>
</div>

<script>
var AL=['','Excelente','Bom','Moderado','Mau','Muito mau'];
var MAXPTS=40;
var H={eco2:[],tvoc:[],temp:[],hum:[],lux:[],db:[],co2:[],dt:[],tsup:[],text:[],tret:[],amps:[]};
var CMP_ON_A=1.0;   // corrente acima disto = compressor ligado
function show(id,on){$e(id).style.display=on?'':'none';}
// Diagnóstico pelo ΔT (ambiente − insuflamento). on: true/false/null (sem TC).
function dti(dt,tsup,on){
  if(tsup<2)return{l:'Risco de congelamento da serpentina',c:'r'};
  if(on===false)return{l:'Compressor desligado — ΔT não avaliado',c:''};
  if(dt<6)return{l:'Baixo — ver carga de gás / condensadora',c:'p'};
  if(dt<8)return{l:'Abaixo do esperado (8–14 °C)',c:'m'};
  if(dt<=14)return{l:'Normal',c:'g'};
  return{l:'Alto — ver filtro / vazão de ar',c:'p'};}
function setVal(id,v,dec,col){var el=$e(id);el.textContent=v===null?'—':v.toFixed(dec);el.className='cv'+(col?' '+col:'');}
var lastOk=0,lastAge=0;

function $e(id){return document.getElementById(id);}
function e2i(v){if(v<400)return{l:'Excelente',c:'g'};if(v<600)return{l:'Bom',c:'f'};
  if(v<1000)return{l:'Razoável',c:'m'};if(v<2000)return{l:'Mau',c:'p'};return{l:'Muito mau',c:'r'};}
function t2i(v){if(v<150)return{l:'Bom',c:'g'};if(v<500)return{l:'Razoável',c:'m'};
  if(v<1500)return{l:'Mau',c:'p'};return{l:'Muito mau',c:'r'};}
function hc(v){return v<30?'#ea580c':v<40?'#d97706':v<=60?'#16a34a':v<=70?'#d97706':'#dc2626';}
function l2i(v){if(v<10)return{l:'Escuro',c:'#64748b'};if(v<50)return{l:'Penumbra',c:'#6366f1'};
  if(v<300)return{l:'Interior',c:'#16a34a'};if(v<800)return{l:'Bem iluminado',c:'#65a30d'};
  if(v<2000)return{l:'Muito claro',c:'#d97706'};return{l:'Luz solar',c:'#ea580c'};}
function s2i(v){if(v<40)return{l:'Silencioso',c:'#16a34a'};if(v<52)return{l:'Calmo',c:'#65a30d'};
  if(v<64)return{l:'Conversa',c:'#d97706'};if(v<74)return{l:'Ruidoso',c:'#ea580c'};
  return{l:'Muito ruidoso',c:'#dc2626'};}
function sv(id,v,c){var el=$e(id);el.textContent=v;el.className='cv'+(c?' '+c:'');}

function spark(id,arr,col){
  var el=$e(id).firstElementChild;
  if(arr.length<2){el.setAttribute('points','');return;}
  var mn=Math.min.apply(null,arr),mx=Math.max.apply(null,arr),rg=mx-mn||1,n=arr.length,pts=[];
  for(var i=0;i<n;i++){var x=(i/(n-1))*100,y=26-((arr[i]-mn)/rg)*24;pts.push(x.toFixed(1)+','+y.toFixed(1));}
  el.setAttribute('points',pts.join(' '));el.setAttribute('stroke',col);
}
function push(k,v){H[k].push(v);if(H[k].length>MAXPTS)H[k].shift();}
function fmtUp(s){s=s|0;if(s<60)return s+'s';var m=(s/60)|0;if(m<60)return m+'m';var h=(m/60)|0;return h+'h '+(m%60)+'m';}
function setDot(cls,txt){$e('dot').className=cls;$e('sl').textContent=txt;}
function setPill(id,ok){var el=$e(id);el.textContent=ok?'OK':'falha';el.className='pill '+(ok?'ok':'bad');}

function setAqi(a){
  var segs=$e('aqiseg').children;
  for(var i=0;i<segs.length;i++){segs[i].className='l'+(i+1)+((i+1)===a?' on':'');}
  var el=$e('aqis');
  if(a>=1&&a<=5){el.textContent=AL[a];el.style.color=segs[a-1]?getComputedStyle(segs[a-1]).color:'';}
  else{el.textContent='';}
}

function upd(d){
  LD=d;if(curView==='ac'||curView==='env')renderPre();
  lastOk=Date.now();lastAge=d.age||0;
  $e('node').textContent='ARQ-'+(d.node||'????');
  $e('warm').className=d.warmup?'on':'';
  $e('d_node').textContent='0x'+(d.node||'????');
  $e('d_fw').textContent=d.fw||'—';
  $e('d_up').textContent=fmtUp(d.up||0);
  $e('d_heap').textContent=((d.heap||0)/1024).toFixed(0)+' KB';
  setPill('d_aht',!!d.aht);
  setPill('d_ens',!!d.ens);

  // TEMT6000 — independente dos sensores I2C
  setPill('d_lux',!!d.light);
  if(d.light){
    var L=l2i(d.lux|0);
    var le=$e('lux');le.textContent=d.lux;le.className='cv';le.style.color=L.c;
    var ls=$e('luxs');ls.textContent=L.l;ls.className='cs';ls.style.color=L.c;
    push('lux',d.lux|0);spark('luxk',H.lux,L.c);
  }

  // INMP441 — independente dos sensores I2C
  setPill('d_snd',!!d.sound);
  if(d.sound){
    var S=s2i(d.db);
    var de=$e('db');de.textContent=d.db.toFixed(1);de.className='cv';de.style.color=S.c;
    var ds=$e('dbs');ds.textContent=S.l;ds.className='cs';ds.style.color=S.c;
    $e('dbpk').textContent=d.dbpk.toFixed(0);
    push('db',d.db);spark('dbk',H.db,S.c);
  }

  // SCD41 — CO2 real (secção só aparece se o sensor existir)
  show('scdbox',!!d.scd);
  if(d.scd){
    setPill('d_scd',!!d.co2ok);
    if(d.co2ok){var c=e2i(d.co2);sv('co2',d.co2,c.c);$e('co2s').textContent=c.l;$e('co2s').className='cs '+c.c;
      push('co2',d.co2);spark('co2k',H.co2,d.co2<600?'#16a34a':d.co2<1000?'#d97706':'#dc2626');}
  }

  // Analisador de AC — secção só aparece com alguma sonda ou TC
  var anyAc=d.pext||d.psup||d.pret||d.pbus>0||d.ct;
  show('acbox',anyAc);
  if(anyAc){
    show('c_sup',d.psup);show('c_ext',d.pext);show('c_ret',d.pret);show('c_ct',d.ct);
    var tsup=d.tsup_ok?d.t_sup:null, text=d.text_ok?d.t_ext:null, tret=d.tret_ok?d.t_ret:null, amps=d.ct?d.amps:null;
    var on=amps===null?null:amps>=CMP_ON_A;
    setVal('tsup',tsup,1);setVal('text',text,1);setVal('tret',tret,1);setVal('amps',amps,2);
    if(tret!==null){push('tret',tret);spark('tretk',H.tret,'#d97706');}
    if(tsup!==null){push('tsup',tsup);spark('tsupk',H.tsup,'#0ea5e9');}
    if(text!==null){push('text',text);spark('textk',H.text,'#ea580c');}
    if(amps!==null){push('amps',amps);spark('ampsk',H.amps,'#4f46e5');
      $e('cmp').textContent=(on?'compressor ligado':'compressor desligado')+(d.ct_clip?' · TC SATURADO':'');
      $e('cmp').className='cs '+(d.ct_clip?'r':on?'g':'');}
    // ΔT da evaporadora: retorno (sonda RET) − insuflamento; sem RET usa o AHT21 da caixa
    var tref=tret!==null?tret:(d.thv?d.temp_c:null);
    $e('dtl').textContent='ΔT · '+(tret!==null?'retorno':'ambiente (caixa)')+' − insuflamento';
    var dtOk=tsup!==null&&tref!==null;
    show('c_dt',d.psup);
    if(dtOk){
      var dt=tref-tsup,D=dti(dt,tsup,on);
      setVal('dt',dt,1,D.c);$e('dts').textContent=D.l;$e('dts').className='cs '+D.c;
      push('dt',dt);spark('dtk',H.dt,'#4f46e5');
    }else{setVal('dt',null,1);$e('dts').textContent='sem leitura de retorno/insuflamento';}
    setPill('d_ac',(!d.psup||d.tsup_ok)&&(!d.pext||d.text_ok)&&(!d.pret||d.tret_ok));
  }

  if(d.thv){
    sv('temp',parseFloat(d.temp_c).toFixed(1),null);
    sv('hum',Math.round(d.hum_pct),null);
    push('temp',parseFloat(d.temp_c));push('hum',d.hum_pct);
    spark('tempk',H.temp,'#4f46e5');
    spark('humk',H.hum,hc(d.hum_pct));
  }
  if(!d.valid){setDot('warn',d.thv?'ENS160 sem leitura (T/H ok)':'sensor sem leitura');return;}

  setAqi(d.aqi|0);
  var i=e2i(d.eco2);sv('eco2',d.eco2,i.c);$e('eco2s').textContent=i.l;$e('eco2s').className='cs '+i.c;
  var t=t2i(d.tvoc);sv('tvoc',d.tvoc,t.c);$e('tvocs').textContent=t.l;$e('tvocs').className='cs '+t.c;

  push('eco2',d.eco2);push('tvoc',d.tvoc);
  spark('eco2k',H.eco2,d.eco2<600?'#16a34a':d.eco2<1000?'#d97706':'#dc2626');
  spark('tvock',H.tvoc,d.tvoc<150?'#16a34a':d.tvoc<500?'#d97706':'#dc2626');
}

var idTimer=null,psel={};
function identToggle(){
  var on=idTimer===null;
  fetch('/probe_ident?on='+(on?1:0),{method:'POST'}).then(function(){if(on)identStart();else identEnd();}).catch(function(){});
}
function identStart(){show('idhelp',true);$e('idbtn').textContent='parar';if(idTimer===null)idTimer=setInterval(identPoll,1000);identPoll();}
function identEnd(){if(idTimer!==null){clearInterval(idTimer);idTimer=null;}show('idhelp',false);$e('idbtn').textContent='iniciar';$e('idst').textContent='';identPoll();}
function identReset(){fetch('/probe_ident?on=1',{method:'POST'}).then(identPoll).catch(function(){});}
function identAssign(rom,i){
  var r=$e('ps'+i).value;
  if(r===''){$e('idst').textContent='escolha o papel';return;}
  fetch('/probe_assign?rom='+rom+'&role='+r,{method:'POST'}).then(function(res){
    $e('idst').textContent=res.ok?'atribuído ✓':'falhou';delete psel[rom];identPoll();
    return fetch('/data').then(function(x){return x.json();}).then(upd);
  }).catch(function(){});
}
function identPoll(){
  fetch('/probes').then(function(r){return r.json();}).then(function(p){
    if(p.ident&&idTimer===null){identStart();return;}
    if(!p.ident&&idTimer!==null){identEnd();return;}
    if(p.ident){var m=Math.floor(p.left/60),s=p.left%60;$e('idst').textContent='ligado · '+m+':'+(s<10?'0':'')+s;}
    var ae=document.activeElement;
    if(ae&&ae.tagName==='SELECT'&&$e('idlist').contains(ae))return;   // não fechar o menu aberto
    // destaque: maior subida ≥ 1 °C e ≥ 0,5 °C acima da 2ª
    var best=-1,bd=-99,sd=-99;
    p.dev.forEach(function(d,i){if(!d.ok)return;if(d.d>bd){sd=bd;bd=d.d;best=i;}else if(d.d>sd)sd=d.d;});
    var hot=(p.ident&&bd>=1.0&&bd-sd>=0.5)?best:-1;
    var h=p.dev.length?'':'<div style="font-size:.75rem;color:var(--mu);margin-top:6px">nenhuma sonda no barramento</div>';
    p.dev.forEach(function(d,i){
      var cur=psel[d.rom]!==undefined?psel[d.rom]:({EXT:'0',SUP:'1',RET:'2'})[d.role];
      var opt=function(v,n){return '<option value="'+v+'"'+(cur===v?' selected':'')+'>'+n+'</option>';};
      h+='<div class="prow'+(i===hot?' hot':'')+'" title="ROM '+d.rom+'"><span class="pid">'+d.rom.substr(2,4)+'</span>'
        +'<span style="color:var(--mu)">'+(d.role||'sem papel')+(i===hot?' · <b style="color:#dc2626">ESTA</b>':'')+'</span>'
        +'<span class="pt">'+(d.ok?d.t.toFixed(2)+' °C':'—')+'</span>'
        +'<span class="pd">'+(p.ident&&d.ok?(d.d>=0?'+':'')+d.d.toFixed(2):'')+'</span>'
        +'<span style="display:flex;gap:6px;margin-left:auto"><select id="ps'+i+'" onchange="psel[\''+d.rom+'\']=this.value">'
        +(cur===undefined?'<option value="" selected>papel…</option>':'')+opt('0','EXT')+opt('1','SUP')+opt('2','RET')+'</select>'
        +'<button type="button" class="btn" style="margin:0" onclick="identAssign(\''+d.rom+'\','+i+')">atribuir</button></span></div>';
    });
    $e('idlist').innerHTML=h;
  }).catch(function(){});
}

// ═══ Modos: navegação ═══════════════════════════════════════════════
var VIEWS=['mon','ac','env','cal'],curView='mon',LD=null,T=null,dismissed={ac:false,env:false};
function tab(v){
  curView=v;
  VIEWS.forEach(function(x){$e('t-'+x).setAttribute('aria-selected',x===v?'true':'false');$e('v-'+x).className='view'+(x===v?' on':'');});
  try{localStorage.setItem('airq-tab',v);}catch(e){}
  if(v==='ac'||v==='env'){renderPre();testPoll();}
  if(v==='cal'){calPoll();identPoll();}
  window.scrollTo(0,0);
}
function fmt(v,d){return v===null||v===undefined||isNaN(v)?'—':v.toFixed(d).replace('.',',');}
function mmss(s){s=Math.max(0,s|0);var m=(s/60)|0,r=s%60;return(m<10?'0':'')+m+':'+(r<10?'0':'')+r;}
function ico(s){return{ok:'✓',wa:'!',bad:'✕',na:'–'}[s];}
function li(s,t){return '<li><span class="i '+s+'">'+ico(s)+'</span><span>'+t+'</span></li>';}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];});}

// ═══ Pré-verificação (a partir de /data) ════════════════════════════
function renderPre(){
  var d=LD;if(!d)return;
  var on=d.ct?d.amps>=CMP_ON_A:null,h='';
  h+=li(d.tret_ok?'ok':'bad',d.tret_ok?'Sonda RET (retorno): '+fmt(d.t_ret,1)+' °C':'Sonda RET ausente — o ΔT usará o AHT21 da caixa (menos preciso)');
  h+=li(d.tsup_ok?'ok':'bad',d.tsup_ok?'Sonda SUP (insuflamento): '+fmt(d.t_sup,1)+' °C':'Sonda SUP ausente — necessária para o teste');
  h+=li(d.text_ok?'ok':'na',d.text_ok?'Sonda EXT (condensadora): '+fmt(d.t_ext,1)+' °C':'Sonda EXT ausente (opcional)');
  h+=li(d.ct?(on?'ok':'wa'):'na',d.ct?(on?'Compressor ligado · '+fmt(d.amps,2)+' A':'Compressor desligado ('+fmt(d.amps,2)+' A) — ligue antes de iniciar'):'Sem TC: corrente e ciclos não serão avaliados');
  $e('ac-pre').innerHTML=h;
  $e('ac-go').disabled=!d.tsup_ok||(T&&T.state==='run');
  h='';
  h+=li(d.thv?'ok':'bad',d.thv?'Temperatura e umidade (AHT21): '+fmt(d.temp_c,1)+' °C · '+d.hum_pct+' %':'AHT21 sem leitura');
  h+=li(d.co2ok?'ok':'na',d.co2ok?'CO₂ real (SCD41): '+d.co2+' ppm':'Sem SCD41: o CO₂ será estimado pelo ENS160 (eCO₂)');
  h+=li(d.valid?(d.warmup?'wa':'ok'):'bad',d.valid?(d.warmup?'ENS160 aquecendo (~3 min após ligar): espere antes de iniciar':'ENS160: eCO₂ '+d.eco2+' ppm · TVOC '+d.tvoc+' ppb · AQI '+d.aqi):'ENS160 sem leitura');
  h+=li(d.sound?'ok':'na',d.sound?'Ruído (INMP441): '+fmt(d.db,1)+' dB(A)':'Microfone sem sinal');
  h+=li(d.light?'ok':'na',d.light?'Iluminância (TEMT6000): '+d.lux+' lx':'Sensor de luz sem leitura');
  $e('env-pre').innerHTML=h;
  $e('env-go').disabled=!d.thv||(T&&T.state==='run');
}

// ═══ Testes: gravação no firmware, laudo aqui ═══════════════════════
function testStart(k){
  var dur=$e(k+'-dur').value,tag=$e(k+'-tag').value.trim(),opt='';
  if(k==='ac'){var i=num('ac-inom');if(i>0)opt='inom='+i;}
  else opt='use='+$e('env-use').value+';season='+$e('env-season').value;
  dismissed[k]=false;
  fetch('/test/start?kind='+k+'&dur='+dur+'&tag='+encodeURIComponent(tag)+'&opt='+encodeURIComponent(opt),{method:'POST'})
    .then(testPoll).catch(function(){});
}
function testStop(){fetch('/test/stop',{method:'POST'}).then(testPoll).catch(function(){});}
function newTest(k){dismissed[k]=true;renderTest();}
var testTimer=null;
function testPoll(){
  fetch('/test').then(function(r){return r.json();}).then(function(t){T=t;renderTest();}).catch(function(){});
}
function C(name,k){var i=T.cols.indexOf(name);return T.d.map(function(r){return r[i]===null?null:r[i]/k;});}
function nn(v){return v!==null&&v!==undefined&&!isNaN(v);}
function avg(a){var s=0,n=0;a.forEach(function(v){if(nn(v)){s+=v;n++;}});return n?s/n:null;}
function mx(a){var m=null;a.forEach(function(v){if(nn(v)&&(m===null||v>m))m=v;});return m;}
function mn(a){var m=null;a.forEach(function(v){if(nn(v)&&(m===null||v<m))m=v;});return m;}
function leq(a){var s=0,n=0;a.forEach(function(v){if(nn(v)){s+=Math.pow(10,v/10);n++;}});return n?10*Math.log10(s/n):null;}
function optv(k){var m=(T.opt||'').match(new RegExp('(?:^|;)'+k+'=([^;]*)'));return m?m[1]:null;}
function worst(cr){var o={ok:0,wa:1,bad:2},w=-1;cr.forEach(function(c){if(c.s in o&&o[c.s]>w)w=o[c.s];});return w<0?'na':['ok','wa','bad'][w];}

function evalAC(){
  var tr=C('tr',10),ts=C('ts',10),te=C('te',10),ta=C('ta',10),rh=C('rh',10),a=C('a',100),db=C('db',10);
  var hasA=a.some(nn),hasR=tr.some(nn),ref=hasR?tr:ta,cr=[];
  var on=a.map(function(v){return nn(v)?v>=CMP_ON_A:null;});
  var idx=[];for(var i=0;i<T.d.length;i++)if(!hasA||on[i])idx.push(i);
  var w5=idx.slice(-Math.round(300/T.step)),w3=idx.slice(-Math.round(180/T.step));
  var dt5=w5.map(function(i){return nn(ref[i])&&nn(ts[i])?ref[i]-ts[i]:null;}),dt3=w3.map(function(i){return nn(ref[i])&&nn(ts[i])?ref[i]-ts[i]:null;});
  var dtA=avg(dt5),tsA=avg(w5.map(function(i){return ts[i];})),nOn=idx.length*T.step;
  var refTxt=hasR?'retorno (sonda RET)':'ambiente da caixa (AHT21, menos preciso)';
  if(dtA===null)cr.push({k:'ΔT da evaporadora',v:'—',s:'na',p:'Sem leituras de insuflamento'+(hasA?' com o compressor ligado':'')+'.'});
  else{var s=dtA>=8&&dtA<=12?'ok':(dtA>=6&&dtA<=14?'wa':'bad'),p;
    if(s==='ok')p='Dentro de 8–12 °C. Referência: '+refTxt+'; média dos últimos 5 min com compressor ligado.';
    else if(dtA<8)p='Abaixo de 8 °C: pouca troca de calor. Causas comuns: falta de gás, compressor fraco, ar quente infiltrando no retorno.';
    else p='Acima de 12 °C: pouco ar passando na serpentina. Causas comuns: filtro sujo, ventilador em baixa ou serpentina obstruída.';
    cr.push({k:'ΔT da evaporadora',v:fmt(dtA,1)+' °C',s:s,p:p});}
  if(tsA!==null){var s2=tsA>=10&&tsA<=16?'ok':(tsA<7?'bad':'wa');
    cr.push({k:'Ar de insuflamento',v:fmt(tsA,1)+' °C',s:s2,p:s2==='ok'?'Dentro de 10–16 °C.':(tsA<7?'Abaixo de 7 °C: risco de congelar a serpentina (pouca vazão de ar ou pouco gás).':(tsA<10?'Entre 7 e 10 °C: frio demais — observar vazão de ar.':'Acima de 16 °C: o aparelho não está a resfriar o suficiente.'))});}
  var vals=dt3.filter(nn);
  if(vals.length>=Math.round(120/T.step)){var r=mx(vals)-mn(vals),s3=r<=0.6?'ok':(r<=1.5?'wa':'bad');
    cr.push({k:'Estabilização',v:'±'+fmt(r/2,1)+' °C',s:s3==='bad'?'wa':s3,p:s3==='ok'?'O ΔT ficou estável nos últimos 3 min.':'O ΔT ainda varia nos últimos 3 min — prolongue o teste para um resultado firme.'});}
  else cr.push({k:'Estabilização',v:'—',s:'na',p:'Menos de 3 min com compressor ligado: tempo insuficiente para avaliar.'});
  if(hasA){var aOn=avg(w5.map(function(i){return a[i];})),inom=parseFloat(optv('inom'));
    if(inom>0&&aOn!==null){var q=aOn/inom,s4=q>=0.85&&q<=1.15?'ok':((q>1.3||q<0.7)?'bad':'wa');
      cr.push({k:'Corrente vs. nominal',v:fmt(aOn,1)+' A ('+Math.round(q*100)+' %)',s:s4,p:s4==='ok'?'Dentro de ±15 % da corrente de placa ('+fmt(inom,1)+' A).':(q>1?'Acima da nominal: sobrecarga — condensadora suja, excesso de gás ou compressor a desgastar.':'Abaixo da nominal: compressor a trabalhar pouco — possível falta de gás.')});}
    else cr.push({k:'Corrente média',v:aOn===null?'—':fmt(aOn,1)+' A',s:'na',p:'Informe a corrente nominal (placa do aparelho) para avaliar.'});
    var cyc=0;for(var j=1;j<on.length;j++)if(on[j-1]===true&&on[j]===false)cyc++;
    cr.push({k:'Ciclos do compressor',v:cyc+'×',s:cyc===0?'ok':'wa',p:cyc===0?'Compressor ligado o teste todo.':'O compressor desligou durante o teste: termostato atingido, proteção térmica ou pressostato. Repita com o frio no máximo.'});}
  var teA=avg(te);if(teA!==null)cr.push({k:'Ar de entrada da condensadora',v:fmt(teA,1)+' °C',s:teA>35?'wa':'ok',p:teA>35?'Acima de 35 °C: capacidade reduzida. Ver sol direto, recirculação de ar quente ou falta de espaço.':'Condição normal de operação.'});
  var dbL=leq(db);if(dbL!==null)cr.push({k:'Ruído no ambiente',v:fmt(dbL,0)+' dB(A)',s:'na',p:'Informativo (média de energia das janelas gravadas).'});
  var rhA=avg(rh);if(rhA!==null)cr.push({k:'Umidade do ambiente',v:fmt(rhA,0)+' %',s:'na',p:'Informativo. Acima de 65 % o aparelho gasta capacidade a desumidificar.'});
  var v=cr[0].s==='na'?'na':worst(cr);
  return{cr:cr,v:v,info:Math.round(nOn/60)+' min com compressor '+(hasA?'ligado':'(sem TC: assumido ligado)')};
}
function evalENV(){
  var ta=C('ta',10),rh=C('rh',10),co2=C('co2',1),eco2=C('eco2',1),tvoc=C('tvoc',1),db=C('db',10),lux=C('lux',1),cr=[];
  var win=optv('season')!=='i',use=optv('use')||'500',tgt=parseInt(use,10);
  var tLo=win?23:20,tHi=win?26:22,hLo=win?40:35,hHi=65,seas=win?'verão':'inverno';
  var tA=avg(ta);if(tA!==null){var s=tA>=tLo&&tA<=tHi?'ok':(tA>=tLo-1&&tA<=tHi+1?'wa':'bad');
    cr.push({k:'Temperatura',v:fmt(tA,1)+' °C',s:s,p:'RE 09/2003, '+seas+': '+tLo+'–'+tHi+' °C. Variação no teste: '+fmt(mn(ta),1)+' a '+fmt(mx(ta),1)+' °C.'});}
  var hA=avg(rh);if(hA!==null){var s2=hA>=hLo&&hA<=hHi?'ok':(hA>=hLo-5&&hA<=hHi+5?'wa':'bad');
    cr.push({k:'Umidade relativa',v:fmt(hA,0)+' %',s:s2,p:'RE 09/2003, '+seas+': '+hLo+'–'+hHi+' %.'+(hA>hHi?' Umidade alta favorece fungos e ácaros.':hA<hLo?' Ar seco irrita olhos e vias aéreas.':'')});}
  if(co2.some(nn)){var cA=avg(co2),cM=mx(co2),s3=cA>1000?'bad':(cM>1000?'wa':'ok');
    cr.push({k:'CO₂',v:Math.round(cA)+' ppm',s:s3,p:'RE 09/2003: até 1000 ppm. Máximo no teste: '+Math.round(cM)+' ppm.'+(s3!=='ok'?' Aumente a renovação de ar externo.':'')});}
  else{var eA=avg(eco2);
    if(eA===null)cr.push({k:'CO₂',v:'—',s:'na',p:'Sem SCD41 e sem leitura do ENS160.'});
    else{var eM=mx(eco2),s3e=eA>1000?'bad':(eM>1000?'wa':'ok');
      cr.push({k:'CO₂ (estimado · ENS160)',v:'≈'+Math.round(eA)+' ppm',s:s3e,p:'Sem SCD41: eCO₂ do ENS160, estimado a partir de compostos voláteis — indica ventilação, mas não substitui medição NDIR para laudo da RE 09/2003 (1000 ppm). Máximo: '+Math.round(eM)+' ppm.'});}}
  var aq=C('aqi',1);if(aq.some(nn)){var qA=avg(aq),sq=qA<=2.5?'ok':(qA<=3.5?'wa':'bad');
    cr.push({k:'Qualidade do ar (AQI · ENS160)',v:fmt(qA,1)+' de 5',s:sq,p:'Índice UBA do ENS160: 1 excelente, 2 bom, 3 moderado, 4 ruim, 5 insalubre. Máximo no teste: '+mx(aq)+'.'});}
  var dL=leq(db);if(dL!==null){var s4=dL<=50?'ok':(dL<=65?'wa':'bad');
    cr.push({k:'Ruído',v:fmt(dL,0)+' dB(A)',s:s4,p:'NBR 10152: escritórios e salas de reunião ≈ 40–50 dB(A); acima de 65 dB(A) é desconfortável (NR-17). Máximo: '+fmt(mx(db),0)+' dB(A).'});}
  var lA=avg(lux);if(lA!==null){var s5=lA>=tgt?'ok':(lA>=0.7*tgt?'wa':'bad');
    cr.push({k:'Iluminância',v:Math.round(lA)+' lx',s:s5,p:'ISO/CIE 8995-1 para este uso: '+tgt+' lx no plano de trabalho. O TEMT6000 é aproximado: calibre com luxímetro para laudo.'});}
  var vA=avg(tvoc);if(vA!==null)cr.push({k:'TVOC',v:Math.round(vA)+' ppb',s:vA<=500?'ok':'wa',p:'Sem limite legal no Brasil; referência informativa (até ~500 ppb). Picos vêm de produtos de limpeza, tintas e perfumes.'});
  return{cr:cr,v:worst(cr),info:''};
}
function verdictHtml(v,kind){
  var t={ok:['Aprovado','Todos os critérios avaliados estão dentro da faixa.'],wa:['Atenção','Há itens fora da faixa recomendada — ver abaixo.'],
    bad:['Reprovado','Há itens claramente fora da faixa — ver abaixo.'],na:['Sem laudo','Dados insuficientes para avaliar.']}[v];
  return '<div class="verdict '+v+'"><span class="vi">'+ico(v)+'</span><div><b>'+t[0]+'</b><span>'+t[1]+'</span></div></div>';
}
function critHtml(cr){return cr.map(function(c){return '<div class="c"><span class="i '+c.s+'">'+ico(c.s)+'</span><b>'+c.k+'</b><span class="v">'+c.v+'</span><p>'+c.p+'</p></div>';}).join('');}

function renderTest(){
  if(!T)return;
  ['ac','env'].forEach(function(k){$e('t-'+k).className=(T.state==='run'&&T.kind===k)?'recording':'';});
  ['ac','env'].forEach(function(k){
    var mine=T.kind===k,run=mine&&T.state==='run',done=mine&&T.state==='done'&&!dismissed[k],other=T.state==='run'&&!mine;
    $e(k+'-setup').hidden=run||done;
    $e(k+'-run').hidden=!run;
    var hasData=mine&&T.n>=3&&(run||done);
    $e(k+'-res').hidden=!hasData;
    $e(k+'-go').textContent=other?'Outro teste está a gravar':'Iniciar teste';
    if(run){$e(k+'-el').textContent=mmss(T.el);$e(k+'-of').textContent='de '+mmss(T.dur);$e(k+'-bar').style.width=Math.min(100,100*T.el/T.dur)+'%';}
    if(!hasData)return;
    var R=k==='ac'?evalAC():evalENV();
    $e(k+'-rt').textContent=(run?'Laudo parcial':'Laudo')+(T.tag?' · '+T.tag:'');
    $e(k+'-rs').textContent=mmss(T.el)+' gravados · '+T.n+' amostras'+(R.info?' · '+R.info:'');
    $e(k+'-verdict').innerHTML=verdictHtml(R.v,k);
    $e(k+'-crit').innerHTML=critHtml(R.cr);
    if(k==='ac')drawAC();
  });
  if(T.state==='run'&&T.n){var L=T.d[T.n-1],g=function(n,s){var i=T.cols.indexOf(n);return L[i]===null?null:L[i]/s;};
    if(T.kind==='ac'){var r=g('tr',10),s=g('ts',10),a=g('a',100);if(r===null)r=g('ta',10);
      $e('ac-nr').textContent=fmt(r,1);$e('ac-ns').textContent=fmt(s,1);$e('ac-nd').textContent=nn(r)&&nn(s)?fmt(r-s,1):'—';$e('ac-na').textContent=fmt(a,1);}
    else{var c=g('co2',1);$e('env-nt').textContent=fmt(g('ta',10),1);$e('env-nh').textContent=fmt(g('rh',10),0);
      if(c===null){var e=g('eco2',1);$e('env-nc').textContent=e===null?'—':'≈'+e;}else $e('env-nc').textContent=c;$e('env-nd').textContent=fmt(g('db',10),0);}}
  renderPre();
}
function drawAC(){
  var tr=C('tr',10),ts=C('ts',10),te=C('te',10),a=C('a',100),S=[];
  if(tr.some(nn))S.push({n:'Retorno',c:'var(--s2)',v:tr});
  if(ts.some(nn))S.push({n:'Insuflamento',c:'var(--s1)',v:ts});
  if(te.some(nn))S.push({n:'Condensadora',c:'var(--s3)',v:te});
  lineChart($e('ac-ch1'),S,'°C',1);
  var hasA=a.some(nn);$e('ac-ch2t').hidden=!hasA;$e('ac-ch2').hidden=!hasA;
  if(hasA)lineChart($e('ac-ch2'),[{n:'Corrente',c:'var(--s4)',v:a}],'A',2);
}
// Gráfico de linhas, um eixo, toque/hover mostra os valores do instante.
function lineChart(el,S,unit,dec){
  var W=320,Hh=140,L=34,R=8,Tp=8,B=20,n=T.d.length,all=[];
  S.forEach(function(s){s.v.forEach(function(v){if(nn(v))all.push(v);});});
  if(!all.length||n<2){el.innerHTML='';return;}
  var lo=Math.min.apply(null,all),hi=Math.max.apply(null,all),pad=(hi-lo)*0.1||1,pos=lo>=0;lo-=pad;hi+=pad;if(pos&&lo<0)lo=0;
  var X=function(i){return L+(W-L-R)*i/(n-1);},Y=function(v){return Tp+(Hh-Tp-B)*(1-(v-lo)/(hi-lo));};
  var g='';for(var k=0;k<=3;k++){var v=lo+(hi-lo)*k/3,y=Y(v);g+='<line class="gl" x1="'+L+'" x2="'+(W-R)+'" y1="'+y+'" y2="'+y+'"/><text class="ax" x="'+(L-4)+'" y="'+(y+3)+'" text-anchor="end">'+fmt(v,dec===2?1:0)+'</text>';}
  g+='<text class="ax" x="'+L+'" y="'+(Hh-5)+'">0</text><text class="ax" x="'+(W-R)+'" y="'+(Hh-5)+'" text-anchor="end">'+mmss((n-1)*T.step)+'</text>';
  S.forEach(function(s){var d='',pen=false;s.v.forEach(function(v,i){if(!nn(v)){pen=false;return;}d+=(pen?'L':'M')+X(i).toFixed(1)+','+Y(v).toFixed(1);pen=true;});
    g+='<path class="ln" d="'+d+'" stroke="'+s.c+'"/>';});
  g+='<line class="xh" visibility="hidden" y1="'+Tp+'" y2="'+(Hh-B)+'" x1="0" x2="0"/>';
  el.innerHTML='<svg viewBox="0 0 '+W+' '+Hh+'" role="img" aria-label="Gráfico: '+S.map(function(s){return s.n;}).join(', ')+' em '+unit+'">'+g+'</svg><div class="tip"></div>'
    +(S.length>1?'<div class="leg">'+S.map(function(s){return '<span><i style="background:'+s.c+'"></i>'+s.n+'</span>';}).join('')+'</div>':'');
  var svg=el.firstChild,tip=el.querySelector('.tip'),xh=svg.querySelector('.xh');
  function mv(e){var b=svg.getBoundingClientRect(),px=(e.clientX-b.left)/b.width*W,i=Math.round((px-L)/(W-L-R)*(n-1));
    if(i<0||i>=n){tip.style.display='none';return;}
    xh.setAttribute('x1',X(i));xh.setAttribute('x2',X(i));xh.setAttribute('visibility','visible');
    tip.innerHTML=mmss(i*T.step)+S.map(function(s){return '<br>'+s.n+': '+fmt(s.v[i],dec)+' '+unit;}).join('');
    tip.style.display='block';var tx=X(i)/W*b.width;tip.style.left=Math.min(Math.max(0,tx-60),b.width-130)+'px';}
  svg.addEventListener('pointermove',mv);svg.addEventListener('pointerdown',mv);
  svg.addEventListener('pointerleave',function(){tip.style.display='none';xh.setAttribute('visibility','hidden');});
}
function reportText(){
  if(!T||!T.n)return'';var k=T.kind,R=k==='ac'?evalAC():evalENV();
  var lines=['AirQ '+($e('node').textContent)+' — '+(k==='ac'?'Teste de ar-condicionado':'Teste de ambiente'),
    'Local: '+(T.tag||'—'),'Data: '+new Date().toLocaleString('pt-BR'),
    'Gravação: '+mmss(T.el)+' ('+T.n+' amostras a cada '+T.step+' s)',
    'Resultado: '+{ok:'APROVADO',wa:'ATENÇÃO',bad:'REPROVADO',na:'SEM LAUDO'}[R.v],''];
  R.cr.forEach(function(c){lines.push('['+{ok:'OK',wa:'!',bad:'X',na:'-'}[c.s]+'] '+c.k+': '+c.v+' — '+c.p);});
  return lines.join('\n');
}
function copyReport(){
  var t=reportText(),m=$e(T.kind+'-msg');
  function done(ok){m.textContent=ok?'Laudo copiado.':'Não foi possível copiar automaticamente: selecione o texto abaixo.';m.className='msg '+(ok?'ok':'bad');
    if(!ok){var ta=document.createElement('textarea');ta.value=t;ta.rows=10;ta.style.width='100%';m.appendChild(ta);ta.select();}}
  if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t).then(function(){done(true);},function(){done(false);});return;}
  var ta=document.createElement('textarea');ta.value=t;document.body.appendChild(ta);ta.select();
  var ok=false;try{ok=document.execCommand('copy');}catch(e){}document.body.removeChild(ta);done(ok);
}
function downloadCsv(){
  var sc={ta:10,rh:10,tr:10,ts:10,te:10,a:100,db:10},rows=['t_s,'+T.cols.join(',')];
  T.d.forEach(function(r,i){rows.push(i*T.step+','+r.map(function(v,j){return v===null?'':(v/(sc[T.cols[j]]||1));}).join(','));});
  var a=document.createElement('a');a.href=URL.createObjectURL(new Blob([rows.join('\n')],{type:'text/csv'}));
  a.download='airq_'+T.kind+'_'+(T.tag||'teste').replace(/[^\w-]+/g,'_')+'.csv';document.body.appendChild(a);a.click();a.remove();
}

// ═══ Calibração ═════════════════════════════════════════════════════
var CAL=null,CH={},calTimer=null;
function hist(k,v){if(!nn(v))return;var a=CH[k]||(CH[k]=[]);if(a.length&&a[a.length-1]===v)return;a.push(v);if(a.length>4)a.shift();}
function havg(k){var a=CH[k];return a&&a.length?a.reduce(function(s,v){return s+v;},0)/a.length:null;}
function calPoll(){
  if(calTimer===null)calTimer=setInterval(function(){if(curView==='cal')calPoll();},2000);
  fetch('/cal').then(function(r){return r.json();}).then(function(c){CAL=c;
    if(c.t.ok)hist('t',c.t.raw);if(c.rh.ok)hist('rh',c.rh.raw);if(c.db.ok)hist('db',c.db.raw);if(c.ct.ok)hist('ct',c.ct.raw);if(c.lux.ok)hist('lux',c.lux.raw);
    c.pr.forEach(function(p){if(p.ok)hist('p'+p.role,p.raw);});calRender();}).catch(function(){});
}
var CALDEF=[{k:'t',ex:'25,4',n:'Temperatura (AHT21)',u:'°C',d:2,off:1,p:'A placa aquece com o ESP32: o offset costuma ser negativo.'},
  {k:'rh',ex:'55',n:'Umidade (AHT21)',u:'%',d:1,off:1,p:'Use um termo-higrômetro de referência.'},
  {k:'db',ex:'45',n:'Ruído (INMP441)',u:'dB(A)',d:1,off:1,p:'Compare com um sonômetro ou app de sonômetro, lado a lado, com som constante.'},
  {k:'ct',ex:'5,2',n:'Corrente (TC)',u:'A',d:2,off:0,min:0.5,p:'Meça a mesma fase com alicate amperímetro, com o compressor ligado.'},
  {k:'lux',ex:'350',n:'Luz (TEMT6000)',u:'lx',d:0,off:0,min:20,p:'Luxímetro na mesma posição e direção do sensor.'}];
function num(id){var v=($e(id).value||'').trim().replace(',','.');return v===''?NaN:parseFloat(v);}
var calBuilt=false;
function calBuild(){
  $e('cal-list').innerHTML=CALDEF.map(function(c){
    return '<div class="cr"><b>'+c.n+'</b><span class="rd" id="cr-'+c.k+'">—</span><p>'+c.p+'</p>'
      +'<div class="ap"><input id="ci-'+c.k+'" type="text" inputmode="decimal" autocomplete="off" placeholder="referência ('+c.u+'), ex.: '+c.ex+'">'
      +'<button class="gbtn" type="button" onclick="calApply(\''+c.k+'\')">Aplicar</button></div>'
      +(c.k==='t'?'<div class="ap"><button class="gbtn" type="button" id="cal-tavg" onclick="calTempFromProbes()" style="width:100%">Usar a média das sondas como referência</button></div>':'')
      +'</div>';}).join('');
  calBuilt=true;
}
function calRender(){
  if(!CAL)return;
  if(!calBuilt)calBuild();
  var h='<tr><th>Sonda</th><th>Papel</th><th>Lida</th><th>Offset</th><th>Corrigida</th></tr>',any=false;
  CAL.pr.forEach(function(p){if(!p.rom)return;any=true;
    h+='<tr><td><b>'+p.rom.substr(2,4)+'</b></td><td>'+p.role+'</td><td>'+(p.ok?fmt(p.raw,2):'—')+'</td><td>'+(p.off>=0?'+':'')+fmt(p.off,2)+'</td><td>'+(p.ok?fmt(p.raw+p.off,2):'—')+'</td></tr>';});
  $e('cal-pt').innerHTML=any?h:'<tr><td>Nenhuma sonda com papel atribuído.</td></tr>';
  CALDEF.forEach(function(c){var o=CAL[c.k],cv=c.off?o.raw+o.off:o.raw*o.gain;
    $e('cr-'+c.k).innerHTML=(o.ok?'lida <strong>'+fmt(o.raw,c.d)+'</strong> → '+fmt(cv,c.d)+' '+c.u:'sem leitura')+'<br>'+(c.off?'offset '+(o.off>=0?'+':'')+fmt(o.off,c.d):'ganho ×'+fmt(o.gain,3));});
  var pm=probeMean();$e('cal-tavg').disabled=pm===null;
  $e('cal-tavg').textContent=pm===null?'Usar a média das sondas (sem sondas com leitura)':'Usar a média das sondas como referência ('+fmt(pm,2)+' °C)';
}
// média das sondas JÁ corrigidas pelos seus offsets
function probeMean(){var P=probesOk();if(!P.length)return null;
  return P.reduce(function(s,p){return s+havg('p'+p.role)+p.off;},0)/P.length;}
function calTempFromProbes(){
  var pm=probeMean(),raw=havg('t');
  if(pm===null){calMsg('cal-msg',false,'Nenhuma sonda com leitura.');return;}
  if(raw===null){calMsg('cal-msg',false,'AHT21 sem leitura.');return;}
  var T=probesOk().map(function(p){return havg('p'+p.role)+p.off;}),sp=Math.max.apply(null,T)-Math.min.apply(null,T);
  if(sp>1){calMsg('cal-msg',false,'As sondas discordam '+fmt(sp,1)+' °C entre si: junte-as ao lado da caixa (AC desligado) e use "Igualar sondas" antes.');return;}
  var v=pm-raw;
  calSet('t',v).then(function(ok){calMsg('cal-msg',ok,ok?'Temperatura (AHT21): offset '+fmt(v,2)+' °C gravado — referência = média de '+probesOk().length+' sonda(s), '+fmt(pm,2)+' °C.':'Offset fora da faixa (±10 °C): confira se as sondas estão junto da caixa.');calPoll();});
}
function calSet(k,v){return fetch('/cal?k='+k+'&v='+v.toFixed(4),{method:'POST'}).then(function(r){return r.ok;});}
function calMsg(id,ok,t){var m=$e(id);m.textContent=t;m.className='msg '+(ok?'ok':'bad');}
function calApply(k){
  var c=CALDEF.filter(function(x){return x.k===k;})[0],ref=num('ci-'+k),raw=havg(k);
  if(!nn(ref)){calMsg('cal-msg',false,'Digite o valor do instrumento de referência.');return;}
  if(raw===null){calMsg('cal-msg',false,c.n+': sem leitura do sensor.');return;}
  if(c.min&&raw<c.min){calMsg('cal-msg',false,c.n+': leitura baixa demais para calibrar (mín. '+c.min+' '+c.u+').');return;}
  var v=c.off?ref-raw:ref/raw;
  calSet(k,v).then(function(ok){calMsg('cal-msg',ok,ok?c.n+': '+(c.off?'offset '+fmt(v,2):'ganho ×'+fmt(v,3))+' gravado (média de '+CH[k].length+' leituras).':c.n+': valor fora da faixa permitida — confira a referência.');
    $e('ci-'+k).value='';calPoll();});
}
function probesOk(){return CAL?CAL.pr.filter(function(p){return p.rom&&p.ok&&havg('p'+p.role)!==null;}):[];}
function calProbesSet(fn,txt){var P=probesOk();if(!P.length){calMsg('cal-pmsg',false,'Nenhuma sonda com leitura.');return;}
  Promise.all(P.map(function(p){return calSet(p.role,fn(p));})).then(function(r){var ok=r.every(Boolean);calMsg('cal-pmsg',ok,ok?txt:'Algum offset ficou fora da faixa (±5 °C).');calPoll();});}
function calProbesEqual(){var P=probesOk();if(P.length<2){calMsg('cal-pmsg',false,'São precisas pelo menos 2 sondas com leitura.');return;}
  var m=P.reduce(function(s,p){return s+havg('p'+p.role);},0)/P.length;
  calProbesSet(function(p){return m-havg('p'+p.role);},'Sondas igualadas à média: '+fmt(m,2)+' °C.');}
function calProbesRef(){var ref=num('cal-pref');if(!nn(ref)){calMsg('cal-pmsg',false,'Digite a temperatura do termômetro de referência.');return;}
  calProbesSet(function(p){return ref-havg('p'+p.role);},'Sondas ajustadas a '+fmt(ref,2)+' °C.');$e('cal-pref').value='';}
function calProbesZero(){calProbesSet(function(){return 0;},'Offsets das sondas zerados.');}
function calReset(){if(!confirm('Zerar toda a calibração (sensores e sondas)?'))return;
  fetch('/cal?reset=1',{method:'POST'}).then(function(r){calMsg('cal-msg',r.ok,r.ok?'Calibração zerada.':'Falhou.');CH={};calPoll();});}

function tick(){
  if(lastOk===0)return;
  var dt=(Date.now()-lastOk)/1000;
  if(dt>20)setDot('err','sem resposta há '+(dt|0)+'s');
  else setDot('ok','ao vivo · há '+((lastAge+dt)|0)+'s');
}
function load(){
  fetch('/data').then(function(r){return r.json();}).then(upd)
    .catch(function(){if(lastOk===0)setDot('err','sem resposta');});
  setTimeout(load,5000);
}
setInterval(tick,1000);
load();
testPoll();setInterval(testPoll,8000);
try{var tv=localStorage.getItem('airq-tab');if(tv&&VIEWS.indexOf(tv)>=0)tab(tv);}catch(e){}
</script>
</body>
</html>
)rawhtml";

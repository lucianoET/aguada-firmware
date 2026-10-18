#pragma once

// Portal web do AirQ (ANI-01), servido pelo AP captive em http://192.168.4.1/
// Abas: Painel (dashboard), Sensores, ArCond (teste de AC), Ambiente (teste RE 09) e Calibração.
// Exporta relatórios no navegador: PDF/impressão, .html autônomo, CSV (;) e JSON.
// Arquivo único em flash (PROGMEM), sem dependências externas (o AP não tem internet).
// Ícones: sprite <symbol> inline; logo também em docs/airq_logo.svg.

static const char ANI_WEB_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#103440">
<title>AirQ — Analisador de ar</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='8' fill='%230b2a33'/%3E%3Ccircle cx='15' cy='14.5' r='9.2' fill='none' stroke='%23fff' stroke-width='2.4'/%3E%3Cpath d='M9.5 11.5h4.5M9.5 15h8.5a2.1 2.1 0 1 0-2.1-2.1M9.5 18.5h5.5' fill='none' stroke='%2345c6cc' stroke-width='1.9' stroke-linecap='round'/%3E%3Cpath d='M19.8 21.2c2.3 3 5 4.1 8.4 3.6' fill='none' stroke='%23fff' stroke-width='2.4' stroke-linecap='round'/%3E%3C/svg%3E">
<style>
:root{
  --bg:#e8edf0;--s:#fbfcfd;--s2:#f1f4f6;--b:#d3dce1;--t:#132630;--mu:#566a75;--mu2:#8a9ba4;
  --ac:#0a6e78;--act:rgba(10,110,120,.11);--hd:#103440;--hdt:#e3f1f3;--air:#45c6cc;
  --ok:#1a8a4c;--wa:#b8730a;--bad:#c0392b;--na:#c3ced4;--run:#b8730a;
  --c1:#2a78d6;--c2:#e0662f;--c3:#1a9e6f;--c4:#7c3aed;
  --sh:0 1px 2px rgba(16,52,64,.07);--r:14px;
}
@media (prefers-color-scheme:dark){
  :root{
    --bg:#08141a;--s:#0f2129;--s2:#0b1a21;--b:#1d3640;--t:#e3edf0;--mu:#93a9b2;--mu2:#5f7782;
    --ac:#45c6cc;--act:rgba(69,198,204,.13);--hd:#0b2a33;--hdt:#dff1f3;--air:#45c6cc;
    --ok:#36c072;--wa:#e6a23c;--bad:#f0705f;--na:#27424c;--run:#e6a23c;
    --c1:#4a95ee;--c2:#ec7a45;--c3:#2bbf8a;--c4:#a58bfa;--sh:0 1px 2px rgba(0,0,0,.35);
  }
}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{background:var(--bg);color:var(--t);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  padding:0 0 env(safe-area-inset-bottom);max-width:560px;margin:0 auto}
.ic{width:1em;height:1em;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;flex-shrink:0}

/* ── Cabeçalho (instrumento) ─────────────────────────────────────── */
.top{display:flex;align-items:center;gap:10px;padding:12px 14px calc(12px);background:var(--hd);color:var(--hdt)}
.logo{width:34px;height:34px;flex-shrink:0}
.brand{display:flex;flex-direction:column;line-height:1.1}
.brand b{font-size:1.15rem;font-weight:800;letter-spacing:-.01em}
.brand span{font-size:.72rem;opacity:.72}
#node{font-size:.68rem;font-weight:700;padding:3px 8px;border-radius:999px;background:rgba(255,255,255,.1);font-variant-numeric:tabular-nums}
.live{margin-left:auto;display:flex;align-items:center;gap:6px;font-size:.72rem;text-align:right}
#dot{width:9px;height:9px;border-radius:50%;background:#8aa3ad;flex-shrink:0;transition:background .4s}
#dot.ok{background:#3fd27f;animation:pulse 2s infinite}
#dot.warn{background:#f0b44c}
#dot.err{background:#ff7a66}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(63,210,127,.5)}70%{box-shadow:0 0 0 6px rgba(63,210,127,0)}100%{box-shadow:0 0 0 0 rgba(63,210,127,0)}}

/* ── Abas ────────────────────────────────────────────────────────── */
.tabs{position:sticky;top:0;z-index:5;display:grid;grid-template-columns:repeat(5,1fr);gap:2px;
  padding:5px 6px;background:var(--s);border-bottom:1px solid var(--b);box-shadow:var(--sh)}
.tabs button{position:relative;font:inherit;font-size:.68rem;font-weight:600;padding:6px 2px 5px;border-radius:10px;border:0;
  background:transparent;color:var(--mu);cursor:pointer;display:flex;flex-direction:column;align-items:center;gap:2px}
.tabs button .ic{width:20px;height:20px}
.tabs button[aria-selected="true"]{background:var(--act);color:var(--ac);font-weight:800}
.tabs button .rec{display:none;position:absolute;top:5px;right:calc(50% - 17px);width:8px;height:8px;border-radius:50%;background:var(--run);border:2px solid var(--s)}
.tabs button.recording .rec{display:block;animation:blink 1.2s infinite}
@keyframes blink{50%{opacity:.3}}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}

/* ── Vistas e blocos ─────────────────────────────────────────────── */
.node-bd{padding:10px}
.view{display:none}.view.on{display:flex;flex-direction:column;gap:10px}
.box{background:var(--s);border:1px solid var(--b);border-radius:var(--r);padding:14px;box-shadow:var(--sh)}
.box h2{display:flex;align-items:center;gap:8px;font-size:1.02rem;font-weight:800;margin-bottom:4px}
.box h2 .ic,.sensor-hd .ic{width:20px;height:20px;color:var(--ac)}
.box h3{font-size:.86rem;font-weight:800;margin:12px 0 6px}
.lead{font-size:.82rem;color:var(--mu);line-height:1.45}
.note{font-size:.74rem;color:var(--mu);margin-top:10px;line-height:1.45}
.row{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}
.gbtn{font:inherit;font-weight:700;font-size:.8rem;padding:8px 11px;border-radius:10px;border:1px solid var(--b);
  background:var(--s);color:var(--t);cursor:pointer;display:inline-flex;align-items:center;gap:6px}
.gbtn .ic{width:16px;height:16px;color:var(--ac)}
.gbtn:disabled{opacity:.5;cursor:default}
.pbtn{font:inherit;font-weight:800;font-size:.92rem;padding:12px 14px;border-radius:11px;border:0;
  background:var(--ac);color:#fff;cursor:pointer;width:100%;margin-top:12px;display:flex;align-items:center;justify-content:center;gap:8px}
@media (prefers-color-scheme:dark){.pbtn{color:#06222a}}
.pbtn .ic{width:18px;height:18px}
.pbtn.stop{background:var(--run)}
.pbtn:disabled{opacity:.45;cursor:default}
.btn{padding:5px 10px;border:1px solid var(--b);border-radius:8px;background:var(--s);color:var(--t);font:inherit;font-size:.74rem;font-weight:600;cursor:pointer;margin-left:auto}
.lnk{font:inherit;color:var(--ac);background:none;border:0;padding:0;cursor:pointer;text-decoration:underline;text-underline-offset:2px}

/* ── Painel ──────────────────────────────────────────────────────── */
.hero{display:grid;grid-template-columns:auto 1fr;gap:16px;align-items:center}
.rose{position:relative;width:156px;height:156px}
.rose svg{width:100%;height:100%;display:block}
.sg{fill:none;stroke-width:30}
.sg.ok{stroke:var(--ok)}.sg.wa{stroke:var(--wa)}.sg.bad{stroke:var(--bad)}.sg.na{stroke:var(--na)}
.sgi{fill:none;stroke:#fff;stroke-width:2.1;stroke-linecap:round;stroke-linejoin:round}
.sgi.na{stroke:var(--mu)}
.rc{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;pointer-events:none}
.rc b{font-size:1.3rem;font-weight:800;line-height:1.1}
.rc b.ok{color:var(--ok)}.rc b.wa{color:var(--wa)}.rc b.bad{color:var(--bad)}
.rc span{font-size:.68rem;color:var(--mu);margin-top:2px}
.hx h1{font-size:1.08rem;font-weight:800;margin-bottom:4px}
.hlist{list-style:none;margin-top:8px;display:flex;flex-direction:column;gap:5px;font-size:.8rem}
.hlist li{display:flex;gap:7px;align-items:baseline}
.hlist li:before{content:"";width:8px;height:8px;border-radius:50%;flex-shrink:0;transform:translateY(-1px)}
.hlist li.wa:before{background:var(--wa)}.hlist li.bad:before{background:var(--bad)}
.ctx{font-size:.74rem;color:var(--mu);margin-top:10px}
@media (max-width:400px){.hero{grid-template-columns:1fr;justify-items:center;text-align:center}.hlist{align-items:center}}
.tiles{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.tile{font:inherit;color:inherit;text-align:left;background:var(--s);border:1px solid var(--b);border-radius:12px;
  padding:10px 12px 9px;cursor:pointer;display:flex;flex-direction:column;gap:1px;min-width:0;box-shadow:var(--sh)}
.tile.ok{box-shadow:inset 3px 0 0 var(--ok),var(--sh)}.tile.wa{box-shadow:inset 3px 0 0 var(--wa),var(--sh)}.tile.bad{box-shadow:inset 3px 0 0 var(--bad),var(--sh)}
.th{display:flex;align-items:center;gap:6px;font-size:.76rem;color:var(--mu);font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.th .ic{width:17px;height:17px;color:var(--ac)}
.tv{font-size:1.75rem;font-weight:750;font-variant-numeric:tabular-nums;line-height:1.08;letter-spacing:-.02em;margin-top:3px}
.tv small{font-size:.74rem;font-weight:600;color:var(--mu);margin-left:3px;letter-spacing:0}
.ts{font-size:.74rem;font-weight:700;color:var(--mu2)}
.ts.ok{color:var(--ok)}.ts.wa{color:var(--wa)}.ts.bad{color:var(--bad)}
.tr{font-size:.66rem;color:var(--mu2)}
.tile .spark{flex:none;width:100%;height:26px;margin:6px 0 2px}
.acstrip{font:inherit;color:inherit;text-align:left;width:100%;cursor:pointer;display:grid;grid-template-columns:auto 1fr;gap:6px 14px;align-items:center}
.acstrip .th{grid-column:1/-1}
.acdt{display:block}.acdt b{display:block;font-size:2.1rem;font-weight:800;line-height:1;font-variant-numeric:tabular-nums;letter-spacing:-.02em}
.acdt span{font-size:.72rem;font-weight:700}
.acv{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
.acv>span{display:flex;flex-direction:column;font-size:.66rem;color:var(--mu);font-weight:600}
.acv b{font-size:1rem;color:var(--t);font-variant-numeric:tabular-nums}
.sstat{display:flex;gap:18px;margin-top:10px;font-size:.74rem;color:var(--mu)}
.sstat b{display:block;font-size:1.05rem;color:var(--t);font-variant-numeric:tabular-nums}

/* ── Exportação ─────────────────────────────────────────────────── */
.exp{border-top:1px solid var(--b);margin-top:14px;padding-top:4px}
.who{display:flex;flex-direction:column;gap:3px;font-size:.74rem;font-weight:600;color:var(--mu)}
.who input,.frm input,.frm select,.cal input{font:inherit;font-size:.92rem;color:var(--t);background:var(--s2);
  border:1px solid var(--b);border-radius:9px;padding:9px 10px;width:100%}
#print{display:none}

/* ── Sensores ───────────────────────────────────────────────────── */
.sensor{background:var(--s);border:1px solid var(--b);border-radius:var(--r);padding:10px;box-shadow:var(--sh)}
.sensor-hd{display:flex;align-items:center;gap:7px;margin:2px 3px 9px}
.sensor-hd b{font-size:.9rem;font-weight:800}
.sensor-hd span{font-size:.7rem;color:var(--mu2);font-weight:600}
.sensor-hd .pill{margin-left:auto}
.cards{display:flex;flex-direction:column;gap:7px}
.card{background:var(--s2);border-radius:10px;padding:9px 12px}
.card.hl{box-shadow:inset 3px 0 0 var(--ac)}
.cl{font-size:.74rem;color:var(--mu);font-weight:600}
.bodyr{display:flex;align-items:center;gap:12px;margin-top:2px}
.nums{display:flex;align-items:baseline;gap:5px;flex:0 0 auto;min-width:92px}
.cv{font-size:1.6rem;font-weight:800;line-height:1.05;font-variant-numeric:tabular-nums;letter-spacing:-.01em}
.cu{font-size:.74rem;color:var(--mu)}
.cs{font-size:.74rem;font-weight:700;margin-left:2px}
.spark{flex:1;min-width:0;height:28px;display:block;overflow:visible}
.spark polyline{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round;vector-effect:non-scaling-stroke}
.aqihd{display:flex;align-items:baseline;justify-content:space-between;gap:8px}
.aqicl{font-size:.8rem;font-weight:700}
.aqiseg{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:7px}
.aqiseg span{text-align:center;padding:7px 0;border-radius:8px;font-weight:800;font-size:.9rem;transition:transform .25s,background .25s}
.aqiseg span.l1{color:#16a34a;background:rgba(22,163,74,.13)}
.aqiseg span.l2{color:#65a30d;background:rgba(101,163,13,.13)}
.aqiseg span.l3{color:#d97706;background:rgba(217,119,6,.13)}
.aqiseg span.l4{color:#ea580c;background:rgba(234,88,12,.13)}
.aqiseg span.l5{color:#dc2626;background:rgba(220,38,38,.13)}
.aqiseg span.on{color:#fff;transform:translateY(-1px)}
.aqiseg span.on.l1{background:#16a34a}.aqiseg span.on.l2{background:#65a30d}
.aqiseg span.on.l3{background:#d97706}.aqiseg span.on.l4{background:#ea580c}.aqiseg span.on.l5{background:#dc2626}
#warm{display:none;margin:0 2px 8px;padding:8px 11px;border-radius:9px;font-size:.74rem;line-height:1.35;
  background:rgba(184,115,10,.12);color:var(--wa)}
#warm.on{display:block}
.pill{display:inline-block;font-size:.66rem;font-weight:700;padding:2px 8px;border-radius:999px}
.pill.ok{background:rgba(26,138,76,.14);color:var(--ok)}
.pill.bad{background:rgba(192,57,43,.14);color:var(--bad)}
.refs{margin-top:2px}
.refs summary{cursor:pointer;list-style:none;border-radius:9px;padding:8px 11px;font-size:.78rem;font-weight:700;color:var(--ac)}
.refs summary::-webkit-details-marker{display:none}
.refs summary:before{content:"▸ "}.refs[open] summary:before{content:"▾ "}
.rb{background:var(--s2);border-radius:10px;padding:9px 11px;margin-top:7px}
.rt{font-size:.74rem;color:var(--mu);font-weight:700;margin-bottom:6px}
table{width:100%;border-collapse:collapse;font-size:.74rem}
th{text-align:left;color:var(--mu);font-weight:600;padding:2px 5px 4px;border-bottom:1px solid var(--b)}
td{padding:4px 5px;border-bottom:1px solid var(--b);vertical-align:middle}
tr:last-child td{border-bottom:none}
.d{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px;vertical-align:middle}
.g{color:#16a34a}.f{color:#65a30d}.m{color:#d97706}.p{color:#ea580c}.r{color:#dc2626}
@media (prefers-color-scheme:dark){.g{color:#36c072}.f{color:#8bc34a}.m{color:#e6a23c}.p{color:#f08a4b}.r{color:#f0705f}}
.prow{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:7px 9px;border:1px solid var(--b);border-radius:9px;margin-top:6px;font-size:.8rem}
.prow.hot{border-color:var(--bad);background:rgba(192,57,43,.08)}
.pid{font:700 1rem ui-monospace,Menlo,monospace;letter-spacing:.06em}
.prow .pt{margin-left:auto;font-variant-numeric:tabular-nums}
.prow .pd{min-width:52px;text-align:right;font-variant-numeric:tabular-nums;color:var(--mu)}
.prow.hot .pd{color:var(--bad);font-weight:700}
.prow select{font:inherit}
.monlink{font-size:.78rem;margin:8px 3px 2px;display:inline-block}

/* ── Testes ─────────────────────────────────────────────────────── */
.frm{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
.frm label{display:flex;flex-direction:column;gap:3px;font-size:.74rem;font-weight:600;color:var(--mu)}
.frm label.wide{grid-column:1/-1}
.chk{list-style:none;margin-top:10px;display:flex;flex-direction:column;gap:5px;font-size:.82rem}
.chk li{display:flex;gap:7px;align-items:baseline}
.chk .i{width:1.1em;text-align:center;font-weight:800}
.i.ok{color:var(--ok)}.i.wa{color:var(--wa)}.i.bad{color:var(--bad)}.i.na{color:var(--mu2)}
.clock{display:flex;align-items:baseline;gap:8px;margin-top:2px}
.clock b{font-size:2.4rem;font-weight:800;font-variant-numeric:tabular-nums;letter-spacing:-.02em}
.clock span{color:var(--mu);font-size:.85rem;font-variant-numeric:tabular-nums}
.bar{height:6px;border-radius:3px;background:var(--b);overflow:hidden;margin-top:6px}
.bar i{display:block;height:100%;background:var(--run);width:0;transition:width .6s}
.now{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:12px}
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
.ctitle{font-size:.78rem;font-weight:700;color:var(--mu);margin-top:14px}
.verdict{display:flex;align-items:center;gap:12px;border-radius:12px;padding:14px;margin-top:6px;color:#fff}
.verdict .vi{font-size:1.6rem;font-weight:900;line-height:1;width:44px;height:44px;border-radius:50%;flex-shrink:0;
  display:flex;align-items:center;justify-content:center;background:rgba(255,255,255,.22)}
.verdict b{font-size:1.45rem;font-weight:900;display:block;line-height:1.1}
.verdict span{font-size:.8rem;opacity:.95}
.verdict.ok{background:#1a8a4c}.verdict.wa{background:#b8730a}.verdict.bad{background:#c0392b}.verdict.na{background:#566a75}
.crit{margin-top:10px;display:flex;flex-direction:column}
.crit .c{display:grid;grid-template-columns:1.2em 1fr auto;gap:2px 8px;padding:9px 0;border-top:1px solid var(--b);align-items:baseline}
.crit .c:first-child{border-top:0}
.crit .c b{font-size:.86rem}
.crit .c .v{font-weight:800;font-variant-numeric:tabular-nums;font-size:.9rem;text-align:right}
.crit .c p{grid-column:2/-1;font-size:.76rem;color:var(--mu);line-height:1.4}

/* ── Calibração ─────────────────────────────────────────────────── */
.cal{display:flex;flex-direction:column}
.cal .cr{display:grid;grid-template-columns:1fr auto;gap:2px 10px;padding:11px 0;border-top:1px solid var(--b)}
.cal .cr:first-child{border-top:0}
.cal .cr b{font-size:.88rem}
.cal .cr .rd{font-size:.78rem;color:var(--mu);font-variant-numeric:tabular-nums;text-align:right}
.cal .cr .rd strong{color:var(--t)}
.cal .cr p{grid-column:1/-1;font-size:.74rem;color:var(--mu);line-height:1.4}
.cal .ap{grid-column:1/-1;display:flex;gap:6px;margin-top:6px}
.cal .ap input{flex:1;min-width:0}
.ptab{width:100%;border-collapse:collapse;font-size:.8rem;margin-top:8px;font-variant-numeric:tabular-nums}
.ptab th{font-size:.7rem;color:var(--mu);font-weight:600;text-align:right;padding:3px 4px;border-bottom:1px solid var(--b)}
.ptab th:first-child,.ptab td:first-child{text-align:left}
.ptab td{text-align:right;padding:6px 4px;border-bottom:1px solid var(--b)}
.msg{font-size:.78rem;font-weight:700;margin-top:8px;min-height:1em}
.msg.ok{color:var(--ok)}.msg.bad{color:var(--bad)}

/* ── Rodapé ─────────────────────────────────────────────────────── */
.node-ft{margin:0 10px 12px;padding:10px 14px;display:flex;flex-wrap:wrap;gap:6px 18px;align-items:center;
  background:var(--s);border:1px solid var(--b);border-radius:var(--r)}
.di{display:flex;flex-direction:column;line-height:1.2}
.dk{font-size:.64rem;color:var(--mu);font-weight:600}
.dv{font-size:.8rem;font-weight:700;font-variant-numeric:tabular-nums}

/* ── Tela larga: painel em 3 colunas, sensores/calibração em 2 ──── */
@media (min-width:760px){
  body{max-width:980px;padding:12px 16px}
  .top{border-radius:var(--r) var(--r) 0 0}
  .tabs{border-radius:0 0 var(--r) var(--r);grid-template-columns:repeat(5,minmax(0,120px));justify-content:start}
  .node-bd{padding:12px 0}.node-ft{margin:0}
  .tiles{grid-template-columns:repeat(3,1fr)}
  #v-dash.on{display:grid;grid-template-columns:1fr 1fr;align-items:start}
  #v-dash .hero,#v-dash .tiles{grid-column:1/-1}
  #v-mon.on,#v-cal.on{display:grid;grid-template-columns:1fr 1fr;align-items:start}
  #v-ac.on,#v-env.on{max-width:640px}
}
@media print{
  @page{size:A4;margin:12mm}
  body{background:#fff;max-width:none;padding:0}
  body>*:not(#print){display:none!important}
  #print{display:block}
}
</style>
</head>
<body>
<svg width="0" height="0" style="position:absolute" aria-hidden="true">
  <symbol id="i-temp" viewBox="0 0 24 24"><path d="M14 14.8V5a2 2 0 0 0-4 0v9.8a4 4 0 1 0 4 0z"/><path d="M12 9.5v6.5"/></symbol>
  <symbol id="i-hum" viewBox="0 0 24 24"><path d="M12 3s6 6.4 6 11a6 6 0 0 1-12 0c0-4.6 6-11 6-11z"/></symbol>
  <symbol id="i-co2" viewBox="0 0 24 24"><path d="M7 18.5h10a4 4 0 0 0 .5-8 6 6 0 0 0-11.3.9A3.6 3.6 0 0 0 7 18.5z"/></symbol>
  <symbol id="i-voc" viewBox="0 0 24 24"><circle cx="6" cy="17" r="2.6"/><circle cx="18" cy="17" r="2.6"/><circle cx="12" cy="6" r="2.6"/><path d="M7.4 14.8l3.3-6.4M16.6 14.8l-3.3-6.4M8.6 17h6.8"/></symbol>
  <symbol id="i-snd" viewBox="0 0 24 24"><path d="M4 10v4M8 7v10M12 4v16M16 8v8M20 11v2"/></symbol>
  <symbol id="i-lux" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/></symbol>
  <symbol id="i-ac" viewBox="0 0 24 24"><path d="M12 2.5v19M3.8 7.2l16.4 9.6M3.8 16.8l16.4-9.6M9.2 4.2L12 6l2.8-1.8M9.2 19.8L12 18l2.8 1.8"/></symbol>
  <symbol id="i-amp" viewBox="0 0 24 24"><path d="M13 2.5L5 13.5h6.2l-1 8 8.8-11.5h-6.4z"/></symbol>
  <symbol id="i-dash" viewBox="0 0 24 24"><path d="M3.5 17.5a8.5 8.5 0 1 1 17 0"/><path d="M12 15l4-5.2"/><circle cx="12" cy="15.5" r="1.4"/></symbol>
  <symbol id="i-chip" viewBox="0 0 24 24"><rect x="6.5" y="6.5" width="11" height="11" rx="2"/><path d="M9.5 3v3.5M14.5 3v3.5M9.5 17.5V21M14.5 17.5V21M3 9.5h3.5M3 14.5h3.5M17.5 9.5H21M17.5 14.5H21"/></symbol>
  <symbol id="i-env" viewBox="0 0 24 24"><path d="M3.5 10.5L12 4l8.5 6.5V20a1 1 0 0 1-1 1h-15a1 1 0 0 1-1-1z"/><path d="M9.5 21v-6h5v6"/></symbol>
  <symbol id="i-cal" viewBox="0 0 24 24"><path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12"/><circle cx="16" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="18" cy="18" r="2"/></symbol>
  <symbol id="i-dl" viewBox="0 0 24 24"><path d="M12 3.5v11.5M7 10.5l5 5 5-5M4.5 20h15"/></symbol>
  <symbol id="i-print" viewBox="0 0 24 24"><path d="M7 9V3.5h10V9"/><rect x="3" y="9" width="18" height="8" rx="2"/><path d="M7 14h10v7H7z"/></symbol>
  <symbol id="i-copy" viewBox="0 0 24 24"><rect x="8.5" y="8.5" width="12" height="12" rx="2"/><path d="M15.5 8.5V5a1.5 1.5 0 0 0-1.5-1.5H5A1.5 1.5 0 0 0 3.5 5v9A1.5 1.5 0 0 0 5 15.5h3.5"/></symbol>
  <symbol id="i-doc" viewBox="0 0 24 24"><path d="M14 3H6.5a1.5 1.5 0 0 0-1.5 1.5v15A1.5 1.5 0 0 0 6.5 21h11a1.5 1.5 0 0 0 1.5-1.5V8z"/><path d="M14 3v5h5M8.5 13h7M8.5 17h4.5"/></symbol>
  <symbol id="i-play" viewBox="0 0 24 24"><path d="M7.5 4.5l12 7.5-12 7.5z"/></symbol>
  <symbol id="i-stop" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="1.5"/></symbol>
  <symbol id="i-find" viewBox="0 0 24 24"><circle cx="11" cy="11" r="6.5"/><path d="M20 20l-4.3-4.3"/></symbol>
  <symbol id="i-reset" viewBox="0 0 24 24"><path d="M4.5 12a7.5 7.5 0 1 0 2.2-5.3M4.5 4v4.5H9"/></symbol>
  <symbol id="i-probe" viewBox="0 0 24 24"><rect x="9" y="2.5" width="6" height="9" rx="3"/><path d="M12 11.5V21M9 21h6"/></symbol>
</svg>

<div class="node">
  <header class="top">
    <svg class="logo" viewBox="0 0 32 32" aria-hidden="true"><circle cx="15" cy="14.5" r="10.2" fill="none" stroke="currentColor" stroke-width="2.6"/><path d="M9 11h5.5M9 15h9.5a2.3 2.3 0 1 0-2.3-2.3M9 19h6.5" fill="none" stroke="#45c6cc" stroke-width="2.1" stroke-linecap="round"/><path d="M20.5 22c2.6 3.4 5.6 4.6 9.5 4" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round"/></svg>
    <div class="brand"><b>AirQ</b><span>Analisador de ar</span></div>
    <span id="node">ARQ-????</span>
    <span class="live"><i id="dot"></i><span id="sl">carregando…</span></span>
  </header>
  <nav class="tabs" role="tablist" aria-label="Modo de uso">
    <button id="t-dash" role="tab" aria-selected="true" type="button" onclick="tab('dash')"><svg class="ic"><use href="#i-dash"/></svg>Painel</button>
    <button id="t-mon" role="tab" aria-selected="false" type="button" onclick="tab('mon')"><svg class="ic"><use href="#i-chip"/></svg>Sensores</button>
    <button id="t-ac" role="tab" aria-selected="false" type="button" onclick="tab('ac')"><svg class="ic"><use href="#i-ac"/></svg>ArCond<span class="rec"></span></button>
    <button id="t-env" role="tab" aria-selected="false" type="button" onclick="tab('env')"><svg class="ic"><use href="#i-env"/></svg>Ambiente<span class="rec"></span></button>
    <button id="t-cal" role="tab" aria-selected="false" type="button" onclick="tab('cal')"><svg class="ic"><use href="#i-cal"/></svg>Calibração</button>
  </nav>

  <div class="node-bd">
  <!-- ═══ PAINEL ═══ -->
  <div class="view on" id="v-dash" role="tabpanel">
    <section class="box hero">
      <div class="rose"><svg viewBox="0 0 200 200" id="rose" role="img" aria-label="Situação de cada parâmetro"></svg>
        <div class="rc"><b id="hv">—</b><span id="hs">lendo sensores…</span></div></div>
      <div class="hx">
        <h1 id="hh">Ar do ambiente</h1>
        <p class="lead" id="hd">Aguardando a primeira leitura.</p>
        <ul class="hlist" id="hl"></ul>
        <p class="ctx">Referência: <button type="button" class="lnk" id="ctxb" onclick="tab('env')">—</button></p>
      </div>
    </section>
    <div class="tiles" id="tiles"></div>
    <button type="button" class="box acstrip" id="dash-ac" hidden onclick="tab('ac')">
      <span class="th"><svg class="ic"><use href="#i-ac"/></svg>Ar-condicionado</span>
      <span class="acdt"><b id="da-dt">—</b><span id="da-ds" class="ts">ΔT retorno − insuflamento</span></span>
      <span class="acv">
        <span><span>Retorno</span><b id="da-r">—</b></span><span><span>Insufl.</span><b id="da-s">—</b></span>
        <span><span>Externo</span><b id="da-e">—</b></span><span><span>Corrente</span><b id="da-a">—</b></span>
      </span>
    </button>
    <section class="box" id="sessbox">
      <h2><svg class="ic"><use href="#i-doc"/></svg>Sessão de monitoramento</h2>
      <p class="lead">Começa quando esta página abre: o navegador guarda uma leitura a cada 5 s, até 1 h. Fechar ou recarregar a página apaga a sessão.</p>
      <div class="sstat"><span>Início<b id="ss-t0">—</b></span><span>Duração<b id="ss-du">—</b></span><span>Leituras<b id="ss-n">0</b></span></div>
      <div class="exp" data-k="dash"></div>
      <div class="row"><button class="gbtn" type="button" onclick="sessClear()"><svg class="ic"><use href="#i-reset"/></svg>Reiniciar sessão</button></div>
    </section>
  </div>

  <!-- ═══ SENSORES ═══ -->
  <div class="view" id="v-mon" role="tabpanel">

    <div class="sensor" id="scdbox" style="display:none">
      <div class="sensor-hd"><svg class="ic"><use href="#i-co2"/></svg><b>CO₂ real</b><span>SCD41 · NDIR</span><span class="pill" id="d_scd">—</span></div>
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

    <div class="sensor">
      <div class="sensor-hd"><svg class="ic"><use href="#i-temp"/></svg><b>Temperatura e umidade</b><span>AHT21</span><span class="pill" id="d_aht">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Temperatura</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="temp">—</span><span class="cu">°C</span></div>
            <svg class="spark" id="tempk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
        <div class="card">
          <span class="cl">Umidade relativa</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="hum">—</span><span class="cu">%</span></div>
            <svg class="spark" id="humk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
      </div>
    </div>

    <div class="sensor">
      <div class="sensor-hd"><svg class="ic"><use href="#i-voc"/></svg><b>Qualidade do ar</b><span>ENS160</span><span class="pill" id="d_ens">—</span></div>
      <div id="warm">ENS160 aquecendo: eCO₂ e TVOC estabilizam nos primeiros ~3 min.</div>
      <div class="cards">
        <div class="card">
          <div class="aqihd"><span class="cl">AQI · índice de qualidade do ar</span><span class="aqicl" id="aqis"></span></div>
          <div class="aqiseg" id="aqiseg">
            <span class="l1">1</span><span class="l2">2</span><span class="l3">3</span><span class="l4">4</span><span class="l5">5</span>
          </div>
        </div>
        <div class="card hl">
          <span class="cl">eCO₂ · estimado por VOC (não vale para a RE 09)</span>
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
            <tr><td><span class="d" style="background:#16a34a"></span>&lt;400</td><td class="g">Excelente</td><td>Ar externo</td></tr>
            <tr><td><span class="d" style="background:#65a30d"></span>400–600</td><td class="f">Bom</td><td>Ambiente ventilado</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>600–1000</td><td class="m">Razoável</td><td>Ligeiro cansaço</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>1000–2000</td><td class="p">Ruim</td><td>Sonolência, dificuldade de foco</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;2000</td><td class="r">Muito ruim</td><td>Dores de cabeça — ventilar</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">AQI ENS160 (1–5)</p>
          <table>
            <tr><th>Nível</th><th>Classificação</th><th>Ação</th></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>1</td><td class="g">Excelente</td><td>Nenhuma</td></tr>
            <tr><td><span class="d" style="background:#65a30d"></span>2</td><td class="f">Bom</td><td>Nenhuma</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>3</td><td class="m">Moderado</td><td>Considerar ventilação</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>4</td><td class="p">Ruim</td><td>Ventilar e reduzir fontes</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>5</td><td class="r">Muito ruim</td><td>Abrir janelas imediatamente</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">TVOC (ppb)</p>
          <table>
            <tr><th>Intervalo</th><th>Nível</th><th>Fontes típicas</th></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>&lt;150</td><td class="g">Bom</td><td>Sem fontes relevantes</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>150–500</td><td class="m">Razoável</td><td>Perfumes, limpeza leve</td></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>500–1500</td><td class="p">Ruim</td><td>Tintas, solventes, cozinha</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;1500</td><td class="r">Muito ruim</td><td>Ventilar imediatamente</td></tr>
          </table>
        </div>
        <div class="rb">
          <p class="rt">Humidade relativa (%)</p>
          <table>
            <tr><th>Intervalo</th><th>Nível</th><th>Risco</th></tr>
            <tr><td><span class="d" style="background:#ea580c"></span>&lt;30%</td><td class="p">Seco</td><td>Irritação de mucosas</td></tr>
            <tr><td><span class="d" style="background:#16a34a"></span>40–60%</td><td class="g">Confortável</td><td>Ideal</td></tr>
            <tr><td><span class="d" style="background:#d97706"></span>60–70%</td><td class="m">Úmido</td><td>Possível mofo</td></tr>
            <tr><td><span class="d" style="background:#dc2626"></span>&gt;70%</td><td class="r">Muito úmido</td><td>Mofo, ácaros</td></tr>
          </table>
        </div>
        </details>
      </div>
    </div>

    <div class="sensor">
      <div class="sensor-hd"><svg class="ic"><use href="#i-lux"/></svg><b>Luminosidade</b><span>TEMT6000</span><span class="pill" id="d_lux">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Iluminância ambiente</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="lux">—</span><span class="cu">lx</span><span class="cs" id="luxs"></span></div>
            <svg class="spark" id="luxk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
      </div>
    </div>

    <div class="sensor">
      <div class="sensor-hd"><svg class="ic"><use href="#i-snd"/></svg><b>Ruído ambiente</b><span>INMP441</span><span class="pill" id="d_snd">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Nível sonoro · dB(A), janela de 0,26 s</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="db">—</span><span class="cu">dB</span><span class="cs" id="dbs"></span></div>
            <svg class="spark" id="dbk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
          <span class="cu">pico <b id="dbpk">—</b> dB</span>
        </div>
      </div>
    </div>

    <div class="sensor" id="acbox" style="display:none">
      <div class="sensor-hd"><svg class="ic"><use href="#i-ac"/></svg><b>Ar-condicionado</b><span>DS18B20 + TC</span><span class="pill" id="d_ac">—</span></div>
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
      <button type="button" class="lnk monlink" onclick="tab('cal')">Identificar e calibrar as sondas</button>
    </div>

  </div><!-- /v-mon -->

  <!-- ═══ TESTE DE AR-CONDICIONADO ═══ -->
  <div class="view" id="v-ac" role="tabpanel">
    <div class="box" id="ac-setup">
      <h2><svg class="ic"><use href="#i-ac"/></svg>Teste de ar-condicionado</h2>
      <p class="lead">Mede a evaporadora com o compressor trabalhando: ΔT entre retorno e insuflamento, temperatura de insuflamento, estabilização e corrente. Ligue o aparelho em refrigeração, no frio máximo, e espere 5 min antes de iniciar.</p>
      <ul class="chk" id="ac-pre"></ul>
      <div class="frm">
        <label class="wide">Equipamento / local<input id="ac-tag" maxlength="60" placeholder="ex.: Sala 12 · split 12 000 BTU"></label>
        <label>Corrente nominal (A)<input id="ac-inom" type="text" inputmode="decimal" autocomplete="off" placeholder="placa, ex.: 5,2"></label>
        <label>Duração<select id="ac-dur"><option value="600">10 min</option><option value="900" selected>15 min</option><option value="1200">20 min</option><option value="1800">30 min</option></select></label>
      </div>
      <button class="pbtn" id="ac-go" type="button" onclick="testStart('ac')">Iniciar teste</button>
    </div>
    <div class="box" id="ac-run" hidden>
      <h2><svg class="ic"><use href="#i-play"/></svg>Gravando</h2>
      <div class="clock"><b id="ac-el">00:00</b><span id="ac-of">de 15:00</span></div>
      <div class="bar"><i id="ac-bar"></i></div>
      <div class="now">
        <div>Retorno<b id="ac-nr">—</b></div><div>Insufl.<b id="ac-ns">—</b></div>
        <div>ΔT<b id="ac-nd">—</b></div><div>Corrente<b id="ac-na">—</b></div>
      </div>
      <button class="pbtn stop" type="button" onclick="testStop()"><svg class="ic"><use href="#i-stop"/></svg>Encerrar agora</button>
      <p class="note">Pode fechar esta página: o aparelho continua gravando. Ao voltar, o teste reaparece.</p>
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
      <div class="exp" data-k="ac"></div>
      <div class="row"><button class="gbtn" type="button" onclick="newTest('ac')"><svg class="ic"><use href="#i-reset"/></svg>Novo teste</button></div>
      <p class="note">Laudo indicativo, feito com temperaturas de ar e corrente. Antes de concluir falta de gás, confirme com manifold (pressões, superaquecimento e sub-resfriamento).</p>
    </div>
  </div>

  <!-- ═══ TESTE DE AMBIENTE ═══ -->
  <div class="view" id="v-env" role="tabpanel">
    <div class="box" id="env-setup">
      <h2><svg class="ic"><use href="#i-env"/></svg>Teste de ambiente</h2>
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
      <p class="note">Uso e estação também valem como referência do Painel.</p>
      <button class="pbtn" id="env-go" type="button" onclick="testStart('env')">Iniciar teste</button>
    </div>
    <div class="box" id="env-run" hidden>
      <h2><svg class="ic"><use href="#i-play"/></svg>Gravando</h2>
      <div class="clock"><b id="env-el">00:00</b><span id="env-of">de 10:00</span></div>
      <div class="bar"><i id="env-bar"></i></div>
      <div class="now">
        <div>Temp.<b id="env-nt">—</b></div><div>UR<b id="env-nh">—</b></div>
        <div>CO₂<b id="env-nc">—</b></div><div>Ruído<b id="env-nd">—</b></div>
      </div>
      <button class="pbtn stop" type="button" onclick="testStop()"><svg class="ic"><use href="#i-stop"/></svg>Encerrar agora</button>
      <p class="note">Pode fechar esta página: o aparelho continua gravando. Evite falar perto do microfone durante o teste.</p>
    </div>
    <div class="box" id="env-res" hidden>
      <h2 id="env-rt">Laudo</h2>
      <p class="lead" id="env-rs"></p>
      <div id="env-verdict"></div>
      <div class="crit" id="env-crit"></div>
      <div id="env-charts"></div>
      <div class="exp" data-k="env"></div>
      <div class="row"><button class="gbtn" type="button" onclick="newTest('env')"><svg class="ic"><use href="#i-reset"/></svg>Novo teste</button></div>
      <p class="note">Não medido aqui (exigidos pela RE 09/2003): velocidade do ar (0,025–0,25 m/s), renovação de ar (27 m³/h por pessoa) e contaminantes biológicos. O ruído é uma média de janelas curtas de 0,26 s a cada 8 s, não um Leq contínuo de sonômetro.</p>
    </div>
  </div>

  <!-- ═══ CALIBRAÇÃO ═══ -->
  <div class="view" id="v-cal" role="tabpanel">
    <div class="box">
      <h2><svg class="ic"><use href="#i-probe"/></svg>Sondas de temperatura</h2>
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
      <h2><svg class="ic"><use href="#i-find"/></svg>Identificar sondas</h2>
      <div style="display:flex;align-items:center;gap:8px;flex-wrap:wrap">
        <span id="idst" style="font-size:.78rem;color:var(--mu)"></span>
        <button type="button" class="btn" id="idbtn" onclick="identToggle()">iniciar</button>
      </div>
      <div id="idhelp" style="display:none;font-size:.78rem;color:var(--mu);margin:6px 0 2px;line-height:1.45">
        Segure <b>uma</b> sonda na mão (~20 s). A que subir fica em destaque: escolha o papel,
        toque em <b>atribuir</b> e cole a etiqueta com o ID. Solte, espere voltar e passe à próxima.
        <button type="button" class="lnk" onclick="identReset()">zerar referência</button>
      </div>
      <div id="idlist"></div>
    </div>
    <div class="box">
      <h2><svg class="ic"><use href="#i-cal"/></svg>Sensores da caixa</h2>
      <p class="lead">Coloque o instrumento de referência ao lado do AirQ, espere a leitura estabilizar e digite o valor dele. Usa-se a média das últimas leituras.</p>
      <div class="cal" id="cal-list"></div>
      <p class="msg" id="cal-msg"></p>
    </div>
    <div class="box">
      <h2><svg class="ic"><use href="#i-reset"/></svg>Zerar calibração</h2>
      <p class="lead">Volta todos os sensores e sondas às leituras de fábrica.</p>
      <button class="gbtn" type="button" onclick="calReset()" style="margin-top:10px">Zerar toda a calibração</button>
    </div>
  </div>

  </div>

  <footer class="node-ft">
    <div class="di"><span class="dk">Nó</span><span class="dv" id="d_node">—</span></div>
    <div class="di"><span class="dk">Firmware</span><span class="dv" id="d_fw">—</span></div>
    <div class="di"><span class="dk">Ligado há</span><span class="dv" id="d_up">—</span></div>
    <div class="di"><span class="dk">Memória livre</span><span class="dv" id="d_heap">—</span></div>
    <div class="di"><span class="dk">Rede</span><span class="dv">ESP-NOW</span></div>
  </footer>
</div>
<div id="print"></div>

<script>
var AL=['','Excelente','Bom','Moderado','Ruim','Muito ruim'];
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
function setVal(id,v,dec,col){var el=$e(id);el.textContent=v===null?'—':fmt(v,dec);el.className='cv'+(col?' '+col:'');}
var lastOk=0,lastAge=0;

function $e(id){return document.getElementById(id);}
function e2i(v){if(v<400)return{l:'Excelente',c:'g'};if(v<600)return{l:'Bom',c:'f'};
  if(v<1000)return{l:'Razoável',c:'m'};if(v<2000)return{l:'Ruim',c:'p'};return{l:'Muito ruim',c:'r'};}
function t2i(v){if(v<150)return{l:'Bom',c:'g'};if(v<500)return{l:'Razoável',c:'m'};
  if(v<1500)return{l:'Ruim',c:'p'};return{l:'Muito ruim',c:'r'};}
function hc(v){return v<30?'#ea580c':v<40?'#d97706':v<=60?'#16a34a':v<=70?'#d97706':'#dc2626';}
function l2i(v){if(v<10)return{l:'Escuro',c:'#64748b'};if(v<50)return{l:'Penumbra',c:'#6366f1'};
  if(v<300)return{l:'Interior',c:'#16a34a'};if(v<800)return{l:'Bem iluminado',c:'#65a30d'};
  if(v<2000)return{l:'Muito claro',c:'#d97706'};return{l:'Luz solar',c:'#ea580c'};}
function s2i(v){if(v<40)return{l:'Silencioso',c:'#16a34a'};if(v<52)return{l:'Calmo',c:'#65a30d'};
  if(v<64)return{l:'Conversa',c:'#d97706'};if(v<74)return{l:'Ruidoso',c:'#ea580c'};
  return{l:'Muito ruidoso',c:'#dc2626'};}
function sv(id,v,c){var el=$e(id);el.textContent=String(v).replace('.',',');el.className='cv'+(c?' '+c:'');}

function spark(id,arr,col){
  var el=$e(id).firstElementChild;
  if(arr.length<2){el.setAttribute('points','');return;}
  var mn=Math.min.apply(null,arr),mx=Math.max.apply(null,arr),rg=mx-mn,n=arr.length,pts=[];
  for(var i=0;i<n;i++){var x=(i/(n-1))*100,y=rg?26-((arr[i]-mn)/rg)*24:14;pts.push(x.toFixed(1)+','+y.toFixed(1));}
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
  if(a>=1&&a<=5){el.textContent=AL[a];el.style.color=['#16a34a','#65a30d','#d97706','#ea580c','#dc2626'][a-1];}
  else{el.textContent='';}
}

function updMon(d){
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
    var de=$e('db');de.textContent=fmt(d.db,1);de.className='cv';de.style.color=S.c;
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
        +'<span class="pt">'+(d.ok?fmt(d.t,2)+' °C':'—')+'</span>'
        +'<span class="pd">'+(p.ident&&d.ok?(d.d>=0?'+':'')+fmt(d.d,2):'')+'</span>'
        +'<span style="display:flex;gap:6px;margin-left:auto"><select id="ps'+i+'" onchange="psel[\''+d.rom+'\']=this.value">'
        +(cur===undefined?'<option value="" selected>papel…</option>':'')+opt('0','EXT')+opt('1','SUP')+opt('2','RET')+'</select>'
        +'<button type="button" class="btn" style="margin:0" onclick="identAssign(\''+d.rom+'\','+i+')">atribuir</button></span></div>';
    });
    $e('idlist').innerHTML=h;
  }).catch(function(){});
}

// ═══ Modos: navegação ═══════════════════════════════════════════════
var VIEWS=['dash','mon','ac','env','cal'],curView='dash',LD=null,T=null,dismissed={ac:false,env:false};
function tab(v){
  curView=v;
  VIEWS.forEach(function(x){$e('t-'+x).setAttribute('aria-selected',x===v?'true':'false');$e('v-'+x).className='view'+(x===v?' on':'');});
  try{localStorage.setItem('airq-tab',v);}catch(e){}
  if(v==='ac'||v==='env'){renderPre();testPoll();}
  if(v==='cal'){calPoll();identPoll();}
  if(v==='dash')renderDash();
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
    cr.push({k:'Ar de insuflamento',v:fmt(tsA,1)+' °C',s:s2,p:s2==='ok'?'Dentro de 10–16 °C.':(tsA<7?'Abaixo de 7 °C: risco de congelar a serpentina (pouca vazão de ar ou pouco gás).':(tsA<10?'Entre 7 e 10 °C: frio demais — observar vazão de ar.':'Acima de 16 °C: o aparelho não está resfriando o suficiente.'))});}
  var vals=dt3.filter(nn);
  if(vals.length>=Math.round(120/T.step)){var r=mx(vals)-mn(vals),s3=r<=0.6?'ok':(r<=1.5?'wa':'bad');
    cr.push({k:'Estabilização',v:'±'+fmt(r/2,1)+' °C',s:s3==='bad'?'wa':s3,p:s3==='ok'?'O ΔT ficou estável nos últimos 3 min.':'O ΔT ainda varia nos últimos 3 min — prolongue o teste para um resultado firme.'});}
  else cr.push({k:'Estabilização',v:'—',s:'na',p:'Menos de 3 min com compressor ligado: tempo insuficiente para avaliar.'});
  if(hasA){var aOn=avg(w5.map(function(i){return a[i];})),inom=parseFloat(optv('inom'));
    if(inom>0&&aOn!==null){var q=aOn/inom,s4=q>=0.85&&q<=1.15?'ok':((q>1.3||q<0.7)?'bad':'wa');
      cr.push({k:'Corrente vs. nominal',v:fmt(aOn,1)+' A ('+Math.round(q*100)+' %)',s:s4,p:s4==='ok'?'Dentro de ±15 % da corrente de placa ('+fmt(inom,1)+' A).':(q>1?'Acima da nominal: sobrecarga — condensadora suja, excesso de gás ou compressor desgastando.':'Abaixo da nominal: compressor trabalhando pouco — possível falta de gás.')});}
    else cr.push({k:'Corrente média',v:aOn===null?'—':fmt(aOn,1)+' A',s:'na',p:'Informe a corrente nominal (placa do aparelho) para avaliar.'});
    var cyc=0;for(var j=1;j<on.length;j++)if(on[j-1]===true&&on[j]===false)cyc++;
    cr.push({k:'Ciclos do compressor',v:cyc+'×',s:cyc===0?'ok':'wa',p:cyc===0?'Compressor ligado o teste todo.':'O compressor desligou durante o teste: termostato atingido, proteção térmica ou pressostato. Repita com o frio no máximo.'});}
  var teA=avg(te);if(teA!==null)cr.push({k:'Ar de entrada da condensadora',v:fmt(teA,1)+' °C',s:teA>35?'wa':'ok',p:teA>35?'Acima de 35 °C: capacidade reduzida. Ver sol direto, recirculação de ar quente ou falta de espaço.':'Condição normal de operação.'});
  var dbL=leq(db);if(dbL!==null)cr.push({k:'Ruído no ambiente',v:fmt(dbL,0)+' dB(A)',s:'na',p:'Informativo (média de energia das janelas gravadas).'});
  var rhA=avg(rh);if(rhA!==null)cr.push({k:'Umidade do ambiente',v:fmt(rhA,0)+' %',s:'na',p:'Informativo. Acima de 65 % o aparelho gasta capacidade desumidificando.'});
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
  var t=VT[v];
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
    $e(k+'-go').textContent=other?'Outro teste está gravando':'Iniciar teste';
    if(run){$e(k+'-el').textContent=mmss(T.el);$e(k+'-of').textContent='de '+mmss(T.dur);$e(k+'-bar').style.width=Math.min(100,100*T.el/T.dur)+'%';}
    if(!hasData)return;
    var R=k==='ac'?evalAC():evalENV();
    $e(k+'-rt').textContent=(run?'Laudo parcial':'Laudo')+(T.tag?' · '+T.tag:'');
    $e(k+'-rs').textContent=mmss(T.el)+' gravados · '+T.n+' amostras'+(R.info?' · '+R.info:'');
    $e(k+'-verdict').innerHTML=verdictHtml(R.v,k);
    $e(k+'-crit').innerHTML=critHtml(R.cr);
    if(k==='ac')drawAC();else drawEnv();
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
  if(tr.some(nn))S.push({n:'Retorno',c:'var(--c2)',v:tr});
  if(ts.some(nn))S.push({n:'Insuflamento',c:'var(--c1)',v:ts});
  if(te.some(nn))S.push({n:'Condensadora',c:'var(--c3)',v:te});
  lineChart($e('ac-ch1'),S,'°C',1);
  var hasA=a.some(nn);$e('ac-ch2t').hidden=!hasA;$e('ac-ch2').hidden=!hasA;
  if(hasA)lineChart($e('ac-ch2'),[{n:'Corrente',c:'var(--c4)',v:a}],'A',2);
}
// Gráfico de linhas, um eixo, toque/hover mostra os valores do instante.
function lineChart(el,S,unit,dec,step){
  step=step||T.step;var W=320,Hh=140,L=34,R=8,Tp=8,B=20,n=S.length?S[0].v.length:0,all=[];
  S.forEach(function(s){s.v.forEach(function(v){if(nn(v))all.push(v);});});
  if(!all.length||n<2){el.innerHTML='';return;}
  var lo=Math.min.apply(null,all),hi=Math.max.apply(null,all),pad=(hi-lo)*0.1||1,pos=lo>=0;lo-=pad;hi+=pad;if(pos&&lo<0)lo=0;
  var ad=(hi-lo)<4?(dec?1:0):0,X=function(i){return L+(W-L-R)*i/(n-1);},Y=function(v){return Tp+(Hh-Tp-B)*(1-(v-lo)/(hi-lo));};
  var g='';for(var k=0;k<=3;k++){var v=lo+(hi-lo)*k/3,y=Y(v);g+='<line class="gl" x1="'+L+'" x2="'+(W-R)+'" y1="'+y+'" y2="'+y+'"/><text class="ax" x="'+(L-4)+'" y="'+(y+3)+'" text-anchor="end">'+fmt(v,ad)+'</text>';}
  g+='<text class="ax" x="'+L+'" y="'+(Hh-5)+'">0</text><text class="ax" x="'+(W-R)+'" y="'+(Hh-5)+'" text-anchor="end">'+mmss((n-1)*step)+'</text>';
  S.forEach(function(s){var d='',pen=false;s.v.forEach(function(v,i){if(!nn(v)){pen=false;return;}d+=(pen?'L':'M')+X(i).toFixed(1)+','+Y(v).toFixed(1);pen=true;});
    g+='<path class="ln" d="'+d+'" stroke="'+s.c+'"/>';});
  g+='<line class="xh" visibility="hidden" y1="'+Tp+'" y2="'+(Hh-B)+'" x1="0" x2="0"/>';
  el.innerHTML='<svg viewBox="0 0 '+W+' '+Hh+'" role="img" aria-label="Gráfico: '+S.map(function(s){return s.n;}).join(', ')+' em '+unit+'">'+g+'</svg><div class="tip"></div>'
    +(S.length>1?'<div class="leg">'+S.map(function(s){return '<span><i style="background:'+s.c+'"></i>'+s.n+'</span>';}).join('')+'</div>':'');
  var svg=el.firstChild,tip=el.querySelector('.tip'),xh=svg.querySelector('.xh');
  function mv(e){var b=svg.getBoundingClientRect(),px=(e.clientX-b.left)/b.width*W,i=Math.round((px-L)/(W-L-R)*(n-1));
    if(i<0||i>=n){tip.style.display='none';return;}
    xh.setAttribute('x1',X(i));xh.setAttribute('x2',X(i));xh.setAttribute('visibility','visible');
    tip.innerHTML=mmss(i*step)+S.map(function(s){return '<br>'+s.n+': '+fmt(s.v[i],dec)+' '+unit;}).join('');
    tip.style.display='block';var tx=X(i)/W*b.width;tip.style.left=Math.min(Math.max(0,tx-60),b.width-130)+'px';}
  svg.addEventListener('pointermove',mv);svg.addEventListener('pointerdown',mv);
  svg.addEventListener('pointerleave',function(){tip.style.display='none';xh.setAttribute('visibility','hidden');});
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
function calProbesEqual(){var P=probesOk();if(P.length<2){calMsg('cal-pmsg',false,'São necessárias pelo menos 2 sondas com leitura.');return;}
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

// ═══ Painel ═════════════════════════════════════════════════════════
function ic(n){return '<svg class="ic" aria-hidden="true"><use href="#i-'+n+'"/></svg>';}
function band(v,lo,hi,tol){return v>=lo&&v<=hi?'ok':(v>=lo-tol&&v<=hi+tol?'wa':'bad');}
// referência do painel = estação e uso escolhidos na aba Ambiente
function refCtx(){
  var w=$e('env-season').value!=='i',u=$e('env-use'),tgt=parseInt(u.value,10);
  return{w:w,tLo:w?23:20,tHi:w?26:22,hLo:w?40:35,tgt:tgt,txt:(w?'verão':'inverno')+' · '+u.options[u.selectedIndex].text.replace(/ \(.*\)/,'')+' ('+tgt+' lx)'};
}
var PD=[
  {k:'temp',n:'Temperatura',i:'temp',u:'°C',d:1,h:'temp',
    v:function(d){return d.thv?+d.temp_c:null;},
    s:function(v,c){return band(v,c.tLo,c.tHi,1);},
    t:function(v,c,s){return s==='ok'?'confortável':(v<c.tLo?'frio':'quente');},
    r:function(c){return 'faixa '+c.tLo+'–'+c.tHi+' °C';}},
  {k:'hum',n:'Umidade',i:'hum',u:'%',d:0,h:'hum',
    v:function(d){return d.thv?+d.hum_pct:null;},
    s:function(v,c){return band(v,c.hLo,65,5);},
    t:function(v,c,s){return s==='ok'?'adequada':(v<c.hLo?'ar seco':'ar úmido');},
    r:function(c){return 'faixa '+c.hLo+'–65 %';}},
  {k:'co2',n:function(d){return d&&!d.co2ok?'eCO₂ estimado':'CO₂';},i:'co2',u:'ppm',d:0,
    h:function(d){return d&&d.co2ok?'co2':'eco2';},warm:1,
    v:function(d){return d.co2ok?d.co2:(d.valid?d.eco2:null);},
    s:function(v){return v<=800?'ok':(v<=1000?'wa':'bad');},
    t:function(v,c,s){return {ok:'bem ventilado',wa:'perto do limite',bad:'ventilar'}[s];},
    r:function(){return 'limite 1000 ppm';}},
  {k:'voc',n:'TVOC',i:'voc',u:'ppb',d:0,h:'tvoc',warm:1,
    v:function(d){return d.valid?d.tvoc:null;},
    s:function(v){return v<500?'ok':(v<1500?'wa':'bad');},
    t:function(v,c,s){return {ok:'baixo',wa:'moderado',bad:'alto'}[s];},
    r:function(c,d){return d&&d.valid&&d.aqi?'AQI '+d.aqi+' · '+AL[d.aqi]:'AQI —';}},
  {k:'snd',n:'Ruído',i:'snd',u:'dB(A)',d:0,h:'db',
    v:function(d){return d.sound?+d.db:null;},
    s:function(v){return v<=50?'ok':(v<=65?'wa':'bad');},
    t:function(v,c,s){return {ok:'silencioso',wa:'moderado',bad:'ruidoso'}[s];},
    r:function(){return 'conforto até 50 dB(A)';}},
  {k:'lux',n:'Iluminância',i:'lux',u:'lx',d:0,h:'lux',
    v:function(d){return d.light?d.lux:null;},
    s:function(v,c){return v>=c.tgt?'ok':(v>=0.7*c.tgt?'wa':'bad');},
    t:function(v,c,s){return {ok:'adequada',wa:'abaixo do ideal',bad:'insuficiente'}[s];},
    r:function(c){return 'mínimo '+c.tgt+' lx';}}
];
function pv(f,a,b){return typeof f==='function'?f(a,b):f;}
var SCOL={ok:'var(--ok)',wa:'var(--wa)',bad:'var(--bad)',na:'var(--mu2)'};
function buildDash(){
  $e('tiles').innerHTML=PD.map(function(p){
    return '<button type="button" class="tile na" id="tl-'+p.k+'" onclick="tab(\'mon\')"><span class="th">'+ic(p.i)+'<span id="tn-'+p.k+'">'+pv(p.n)+'</span></span>'
      +'<span class="tv" id="tv-'+p.k+'">—</span><span class="ts" id="ts-'+p.k+'">sem leitura</span>'
      +'<svg class="spark" id="tk-'+p.k+'" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>'
      +'<span class="tr" id="tr-'+p.k+'"></span></button>';}).join('');
  $e('rose').innerHTML=roseSvg(PD.map(function(p){return{s:'na',i:p.i};}));
}
function arc(cx,cy,r,a0,a1){var q=Math.PI/180,f=function(a,t){return (t?cy+r*Math.sin(a*q):cx+r*Math.cos(a*q)).toFixed(1);};
  return 'M'+f(a0)+','+f(a0,1)+'A'+r+','+r+' 0 '+(a1-a0>180?1:0)+' 1 '+f(a1)+','+f(a1,1);}
function roseSvg(st){
  var n=st.length,g='';
  st.forEach(function(x,j){var a0=-90+j*360/n+2.5,a1=-90+(j+1)*360/n-2.5,am=(a0+a1)/2*Math.PI/180;
    g+='<path class="sg '+x.s+'" d="'+arc(100,100,82,a0,a1)+'"/>'
      +'<use href="#i-'+x.i+'" class="sgi '+x.s+'" x="'+(100+82*Math.cos(am)-10).toFixed(1)+'" y="'+(100+82*Math.sin(am)-10).toFixed(1)+'" width="20" height="20"/>';});
  return g;
}
function dashEval(d){
  var c=refCtx();
  return PD.map(function(p){
    var v=p.v(d),s=v===null?'na':p.s(v,c),warm=p.warm&&d.warmup&&!(p.k==='co2'&&d.co2ok);
    if(warm&&s!=='na')s='na';
    return{p:p,v:v,s:s,t:v===null?'sem leitura':(warm?'aquecendo':p.t(v,c,s)),r:p.r(c,d),n:pv(p.n,d)};});
}
var STW={ok:'Bom',wa:'Atenção',bad:'Ruim',na:'—'};
function renderDash(){
  var d=LD;$e('ctxb').textContent=refCtx().txt;
  if(!d)return;
  var E=dashEval(d),w=worst(E),ok=0,av=0,hl='';
  E.forEach(function(e){var p=e.p,k=p.k;
    if(e.s!=='na'){av++;if(e.s==='ok')ok++;else hl+='<li class="'+e.s+'">'+e.n+': '+fmt(e.v,p.d)+' '+p.u+' · '+e.t+'</li>';}
    $e('tl-'+k).className='tile '+e.s;$e('tn-'+k).textContent=e.n;
    $e('tv-'+k).innerHTML=e.v===null?'—':fmt(e.v,p.d)+'<small>'+p.u+'</small>';
    $e('ts-'+k).textContent=e.t;$e('ts-'+k).className='ts '+e.s;$e('tr-'+k).textContent=e.r;
    spark('tk-'+k,H[pv(p.h,d)]||[],SCOL[e.s]);});
  $e('rose').innerHTML=roseSvg(E.map(function(e){return{s:e.s,i:e.p.i};}));
  $e('hv').textContent=STW[w];$e('hv').className=w;
  $e('hs').textContent=av?ok+' de '+av+' na faixa':'sem leituras';
  $e('hd').textContent=!av?'Nenhum sensor respondeu ainda.':(w==='ok'?'Todos os parâmetros medidos estão dentro da faixa de referência.':'Fora da faixa de referência:');
  $e('hl').innerHTML=hl;
  // ar-condicionado
  var anyAc=d.pext||d.psup||d.pret||d.ct;$e('dash-ac').hidden=!anyAc;
  if(anyAc){var ts=d.tsup_ok?d.t_sup:null,tr=d.tret_ok?d.t_ret:(d.thv?d.temp_c:null),on=d.ct?d.amps>=CMP_ON_A:null;
    $e('da-r').textContent=fmt(d.tret_ok?d.t_ret:null,1);$e('da-s').textContent=fmt(ts,1);
    $e('da-e').textContent=fmt(d.text_ok?d.t_ext:null,1);$e('da-a').textContent=d.ct?fmt(d.amps,1)+' A':'—';
    if(ts!==null&&tr!==null){var D=dti(tr-ts,ts,on),cs={g:'ok',m:'wa',p:'bad',r:'bad'}[D.c]||'na';
      $e('da-dt').textContent=fmt(tr-ts,1)+' °C';$e('da-ds').textContent='ΔT · '+D.l;$e('da-ds').className='ts '+cs;}
    else{$e('da-dt').textContent='—';$e('da-ds').textContent='ΔT: falta sonda de insuflamento';$e('da-ds').className='ts';}}
  sessInfo();
}

// ═══ Sessão no navegador (uma linha a cada leitura, até 1 h) ════════
var SC=['temp','hum','co2','eco2','tvoc','aqi','db','lux','tret','tsup','text','amps'],SMAX=720,SESS={t0:Date.now(),r:[]};
function sessRec(d){
  var g=function(c,v){return c?+v:null;};
  SESS.r.push([Date.now(),g(d.thv,d.temp_c),g(d.thv,d.hum_pct),g(d.co2ok,d.co2),g(d.valid,d.eco2),g(d.valid,d.tvoc),g(d.valid,d.aqi),
    g(d.sound,d.db),g(d.light,d.lux),g(d.tret_ok,d.t_ret),g(d.tsup_ok,d.t_sup),g(d.text_ok,d.t_ext),g(d.ct,d.amps)]);
  if(SESS.r.length>SMAX)SESS.r.shift();
}
function sc(k){var i=SC.indexOf(k)+1;return SESS.r.map(function(r){return r[i];});}
function hhmm(t){var d=new Date(t);return (d.getHours()<10?'0':'')+d.getHours()+':'+(d.getMinutes()<10?'0':'')+d.getMinutes();}
function sessInfo(){var n=SESS.r.length,t0=n?SESS.r[0][0]:SESS.t0;
  $e('ss-t0').textContent=hhmm(t0);$e('ss-du').textContent=n?mmss((SESS.r[n-1][0]-t0)/1000):'00:00';$e('ss-n').textContent=n;}
function sessClear(){SESS={t0:Date.now(),r:[]};sessInfo();}
function upd(d){updMon(d);sessRec(d);if(curView==='dash')renderDash();}

// ═══ Relatórios ═════════════════════════════════════════════════════
var LOGO='<svg viewBox="0 0 32 32" width="40" height="40"><circle cx="15" cy="14.5" r="10.2" fill="none" stroke="#103440" stroke-width="2.6"/><path d="M9 11h5.5M9 15h9.5a2.3 2.3 0 1 0-2.3-2.3M9 19h6.5" fill="none" stroke="#0a9aa3" stroke-width="2.1" stroke-linecap="round"/><path d="M20.5 22c2.6 3.4 5.6 4.6 9.5 4" fill="none" stroke="#103440" stroke-width="2.6" stroke-linecap="round"/></svg>';
var REP_CSS='.rep{font:13px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;color:#132630;max-width:780px;margin:0 auto;padding:20px;background:#fff}'
+'.rep header{display:flex;align-items:center;gap:12px;border-bottom:2px solid #103440;padding-bottom:10px;margin-bottom:12px}'
+'.rep h1{font-size:19px;margin:0}.rep header p{margin:0;color:#566a75;font-size:12px}.rep header .dt{margin-left:auto;text-align:right}'
+'.rep .meta{display:grid;grid-template-columns:1fr 1fr;gap:0 18px;margin:0 0 10px}.rep .meta div{display:flex;gap:8px;border-bottom:1px solid #e1e7ea;padding:4px 0}.rep .meta span{color:#566a75;min-width:118px}'
+'.rep .vd{padding:10px 14px;border-radius:8px;color:#fff;font-weight:800;font-size:16px;margin:10px 0 12px;-webkit-print-color-adjust:exact;print-color-adjust:exact}'
+'.rep .vd.ok{background:#1a8a4c}.rep .vd.wa{background:#b8730a}.rep .vd.bad{background:#c0392b}.rep .vd.na{background:#566a75}.rep .vd small{display:block;font-weight:400;font-size:12px;opacity:.95}'
+'.rep table{width:100%;border-collapse:collapse;margin:4px 0 12px}.rep th,.rep td{text-align:left;padding:5px 6px;border-bottom:1px solid #e1e7ea;vertical-align:top}'
+'.rep th{font-size:11px;color:#566a75;font-weight:600}.rep td.n{text-align:right;white-space:nowrap;font-variant-numeric:tabular-nums}.rep tr{break-inside:avoid}'
+'.rep .st{font-weight:800;white-space:nowrap}.rep .st.ok{color:#1a8a4c}.rep .st.wa{color:#b8730a}.rep .st.bad{color:#c0392b}.rep .st.na{color:#8a9ba4}'
+'.rep h2{font-size:14px;margin:16px 0 4px;color:#103440}.rep .ch{break-inside:avoid;margin-bottom:6px}.rep .ch p{font-size:12px;font-weight:700;color:#566a75;margin:6px 0 2px}.rep .ch svg{width:100%;height:auto;max-height:190px}'
+'.rep .gl{stroke:#dfe6ea}.rep .ax{fill:#566a75;font-size:10px}.rep .ln{fill:none;stroke-width:2}.rep .xh{display:none}'
+'.rep .lg{font-size:11px;color:#566a75}.rep .lg i{display:inline-block;width:12px;height:3px;margin:0 4px 2px 10px;vertical-align:middle;-webkit-print-color-adjust:exact;print-color-adjust:exact}'
+'.rep .nt{font-size:11px;color:#566a75;margin:4px 0}.rep .sig{display:flex;gap:28px;margin-top:36px;break-inside:avoid}.rep .sig div{flex:1;border-top:1px solid #132630;padding-top:4px;font-size:11px;color:#566a75}'
+'.rep footer{margin-top:18px;font-size:10px;color:#8a9ba4;border-top:1px solid #e1e7ea;padding-top:6px}'
+'@media (max-width:560px){.rep .meta{grid-template-columns:1fr}}';
var CHEX={'var(--c1)':'#2a78d6','var(--c2)':'#e0662f','var(--c3)':'#1a9e6f','var(--c4)':'#7c3aed'};
var STT={ok:'OK',wa:'Atenção',bad:'Fora',na:'—'};
var VT={ok:['Aprovado','Todos os critérios avaliados estão dentro da faixa.'],wa:['Atenção','Há itens fora da faixa recomendada.'],
  bad:['Reprovado','Há itens claramente fora da faixa.'],na:['Sem laudo','Dados insuficientes para avaliar.']};
function who(){try{return localStorage.getItem('airq-who')||'';}catch(e){return '';}}
function chartRep(title,S,unit,dec,step){
  S=S.filter(function(s){return s.v.some(nn);});if(!S.length)return '';
  var el=document.createElement('div');lineChart(el,S,unit,dec,step);
  var svg=el.querySelector('svg');if(!svg)return '';
  var h=svg.outerHTML.replace(/var\(--c\d\)/g,function(m){return CHEX[m];});
  return '<div class="ch"><p>'+esc(title)+'</p>'+h+(S.length>1?'<div class="lg">'+S.map(function(s){return '<i style="background:'+CHEX[s.c]+'"></i>'+s.n;}).join('')+'</div>':'')+'</div>';
}
function metaHtml(rows){return '<div class="meta">'+rows.filter(function(r){return r[1]!==null&&r[1]!=='';}).map(function(r){return '<div><span>'+r[0]+'</span><b>'+esc(r[1])+'</b></div>';}).join('')+'</div>';}
function calHtml(){
  if(!CAL)return '<p class="nt">Não foi possível consultar a calibração do aparelho.</p>';
  var R=[['Temperatura (AHT21)','offset',CAL.t.off,'°C',2],['Umidade (AHT21)','offset',CAL.rh.off,'%',1],['Ruído (INMP441)','offset',CAL.db.off,'dB',1],
    ['Corrente (TC)','ganho',CAL.ct.gain,'×',3],['Luz (TEMT6000)','ganho',CAL.lux.gain,'×',3]];
  CAL.pr.forEach(function(p){if(p.rom)R.push(['Sonda '+p.role+' ('+p.rom.substr(2,4)+')','offset',p.off,'°C',2]);});
  return '<table><tr><th>Sensor</th><th>Correção</th><th class="n">Valor</th></tr>'+R.map(function(r){
    var v=r[1]==='ganho'?'× '+fmt(r[2],r[4]):(r[2]>=0?'+':'')+fmt(r[2],r[4])+' '+r[3];
    return '<tr><td>'+r[0]+'</td><td>'+r[1]+'</td><td class="n">'+v+'</td></tr>';}).join('')+'</table>'
    +'<p class="nt">Correções gravadas no aparelho; os valores deste relatório já saem corrigidos.</p>';
}
function sigHtml(){return '<div class="sig"><div>'+(esc(who())||'Responsável técnico')+'<br>Assinatura</div><div>Data</div></div>';}
function repHead(t,sub){var dt=new Date();
  return '<header>'+LOGO+'<div><h1>'+t+'</h1><p>'+sub+'</p></div><p class="dt">'+dt.toLocaleDateString('pt-BR')+'<br>'+dt.toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'})+'</p></header>';}
function repFoot(){return '<footer>Gerado pelo AirQ '+esc($e('node').textContent)+' · firmware '+esc(LD&&LD.fw||'—')+' · '+new Date().toLocaleString('pt-BR')+'</footer>';}
function repTest(){
  var k=T.kind,R=k==='ac'?evalAC():evalENV(),c,h='',u=$e('env-use');
  var meta=[['Local / equipamento',T.tag||'—'],['Aparelho',$e('node').textContent],['Gravação',mmss(T.el)+' · '+T.n+' amostras a cada '+T.step+' s'+(T.state==='run'?' (em andamento)':'')]];
  if(k==='ac'){var inom=parseFloat(optv('inom'));meta.push(['Corrente nominal',inom>0?fmt(inom,1)+' A':'não informada']);if(R.info)meta.push(['Tempo avaliado',R.info]);}
  else{var uo=[].filter.call(u.options,function(o){return o.value===optv('use');})[0];meta.push(['Estação',optv('season')==='i'?'Inverno':'Verão']);meta.push(['Uso do local',uo?uo.text:'—']);}
  meta.push(['Responsável',who()]);
  h+=metaHtml(meta)+'<div class="vd '+R.v+'">'+VT[R.v][0]+'<small>'+VT[R.v][1]+'</small></div>';
  h+='<h2>Critérios</h2><table><tr><th>Situação</th><th>Item</th><th class="n">Valor</th><th>Observação</th></tr>'
    +R.cr.map(function(x){return '<tr><td class="st '+x.s+'">'+STT[x.s]+'</td><td>'+x.k+'</td><td class="n">'+x.v+'</td><td>'+x.p+'</td></tr>';}).join('')+'</table>';
  h+='<h2>Registros</h2>';
  if(k==='ac'){h+=chartRep('Temperaturas (°C)',[{n:'Retorno',c:'var(--c2)',v:C('tr',10)},{n:'Insuflamento',c:'var(--c1)',v:C('ts',10)},{n:'Condensadora',c:'var(--c3)',v:C('te',10)}],'°C',1)
    +chartRep('Corrente da condensadora (A)',[{n:'Corrente',c:'var(--c4)',v:C('a',100)}],'A',2);}
  else h+=envCharts(chartRep);
  h+='<h2>Calibração do aparelho</h2>'+calHtml();
  h+='<p class="nt">'+(k==='ac'?'Laudo indicativo, feito com temperaturas de ar e corrente. Antes de concluir falta de gás, confirme com manifold (pressões, superaquecimento e sub-resfriamento).'
    :'Não medido (exigidos pela RE 09/2003): velocidade do ar, renovação de ar (27 m³/h por pessoa) e contaminantes biológicos. Ruído: média de janelas curtas, não Leq de sonômetro. eCO₂ do ENS160 é estimativa.')+'</p>';
  c=k==='ac'?'Laudo de ar-condicionado':'Laudo de qualidade do ambiente';
  return{t:c,f:'airq_'+(k==='ac'?'laudo_arcond':'laudo_ambiente')+'_'+fileTag(T.tag),
    body:'<div class="rep">'+repHead(c,esc(T.tag||'Local não informado'))+h+sigHtml()+repFoot()+'</div>'};
}
function envCharts(fn){var c2=C('co2',1),hasC=c2.some(nn);
  return fn('Temperatura (°C)',[{n:'Temperatura',c:'var(--c2)',v:C('ta',10)}],'°C',1)+fn('Umidade relativa (%)',[{n:'Umidade',c:'var(--c1)',v:C('rh',10)}],'%',0)
    +fn(hasC?'CO₂ (ppm)':'eCO₂ estimado (ppm)',[{n:hasC?'CO₂':'eCO₂',c:'var(--c3)',v:hasC?c2:C('eco2',1)}],'ppm',0)
    +fn('Ruído (dB(A))',[{n:'Ruído',c:'var(--c4)',v:C('db',10)}],'dB',0)+fn('Iluminância (lx)',[{n:'Luz',c:'var(--c2)',v:C('lux',1)}],'lx',0);}
function sessStats(){var c=refCtx(),d=LD||{};
  return PD.map(function(p){var hk=p.k==='co2'?(sc('co2').some(nn)?'co2':'eco2'):pv(p.h,d),a=sc(hk),
    av=p.k==='snd'?leq(a):avg(a),s=av===null?'na':p.s(av,c);
    return{n:p.k==='co2'?(hk==='co2'?'CO₂':'eCO₂ estimado'):pv(p.n,d),u:p.u,d:p.d,cur:a.length?a[a.length-1]:null,mn:mn(a),av:av,mx:mx(a),s:s,r:p.r(c,d)};});}
function repDash(){
  var n=SESS.r.length,t0=n?SESS.r[0][0]:SESS.t0,t1=n?SESS.r[n-1][0]:t0,st=sessStats(),w=worst(st),c=refCtx(),h='';
  h+=metaHtml([['Aparelho',$e('node').textContent],['Período',new Date(t0).toLocaleString('pt-BR')+' → '+hhmm(t1)],['Duração',mmss((t1-t0)/1000)+' · '+n+' leituras (~5 s)'],['Referência',c.txt],['Responsável',who()]]);
  var V={ok:['Dentro da faixa','Média de todos os parâmetros medidos dentro da referência.'],wa:['Atenção','Há parâmetros fora da faixa de referência.'],bad:['Fora da faixa','Há parâmetros claramente fora da referência.'],na:['Sem dados','Nenhuma leitura registrada.']}[w];
  h+='<div class="vd '+w+'">'+V[0]+'<small>'+V[1]+'</small></div>';
  h+='<h2>Resumo da sessão</h2><table><tr><th>Parâmetro</th><th class="n">Atual</th><th class="n">Mín.</th><th class="n">Média</th><th class="n">Máx.</th><th>Referência</th><th>Situação</th></tr>'
    +st.map(function(x){return '<tr><td>'+x.n+' ('+x.u+')</td><td class="n">'+fmt(x.cur,x.d)+'</td><td class="n">'+fmt(x.mn,x.d)+'</td><td class="n">'+fmt(x.av,x.d)+'</td><td class="n">'+fmt(x.mx,x.d)+'</td><td>'+x.r+'</td><td class="st '+x.s+'">'+STT[x.s]+'</td></tr>';}).join('')+'</table>';
  var ac=['tret','tsup','text','amps'].some(function(k){return sc(k).some(nn);});
  h+='<h2>Registros</h2>'+chartRep('Temperatura (°C)',[{n:'Temperatura',c:'var(--c2)',v:sc('temp')}],'°C',1,5)+chartRep('Umidade relativa (%)',[{n:'Umidade',c:'var(--c1)',v:sc('hum')}],'%',0,5)
    +chartRep(sc('co2').some(nn)?'CO₂ (ppm)':'eCO₂ estimado (ppm)',[{n:'CO₂',c:'var(--c3)',v:sc('co2').some(nn)?sc('co2'):sc('eco2')}],'ppm',0,5)
    +chartRep('Ruído (dB(A))',[{n:'Ruído',c:'var(--c4)',v:sc('db')}],'dB',0,5)
    +(ac?chartRep('Ar-condicionado (°C)',[{n:'Retorno',c:'var(--c2)',v:sc('tret')},{n:'Insuflamento',c:'var(--c1)',v:sc('tsup')},{n:'Externo',c:'var(--c3)',v:sc('text')}],'°C',1,5):'');
  h+='<h2>Calibração do aparelho</h2>'+calHtml();
  h+='<p class="nt">Monitoramento contínuo, sem valor de laudo: para avaliar o ambiente pela RE 09/2003 use o teste de Ambiente. Ruído: média de energia das leituras. eCO₂ do ENS160 é estimativa.</p>';
  return{t:'Relatório de monitoramento',f:'airq_monitoramento_'+fileStamp(),body:'<div class="rep">'+repHead('Relatório de monitoramento','Qualidade do ar · sessão no navegador')+h+sigHtml()+repFoot()+'</div>'};
}
function fileStamp(){var d=new Date(),p=function(x){return (x<10?'0':'')+x;};return d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'_'+p(d.getHours())+p(d.getMinutes());}
function fileTag(t){return ((t||'teste').normalize?(t||'teste').normalize('NFD').replace(/[̀-ͯ]/g,''):(t||'teste')).replace(/[^\w-]+/g,'_').slice(0,40)+'_'+fileStamp();}
function hasRep(k){return k==='dash'?SESS.r.length>0:(T&&T.n&&T.kind===k);}
function repGet(k,cb,b){
  if(!hasRep(k)){expMsg(b,false,k==='dash'?'A sessão ainda não tem leituras.':'Sem dados de teste.');return;}
  fetch('/cal').then(function(r){return r.json();}).then(function(c){CAL=c;}).catch(function(){}).then(function(){cb(k==='dash'?repDash():repTest());});
}
function repPrint(k,b){repGet(k,function(R){
  var st=$e('repcss');if(!st){st=document.createElement('style');st.id='repcss';st.textContent=REP_CSS;document.head.appendChild(st);}
  $e('print').innerHTML=R.body;var t0=document.title;document.title=R.f;
  window.addEventListener('afterprint',function f(){document.title=t0;window.removeEventListener('afterprint',f);});
  setTimeout(function(){window.print();},50);
  expMsg(b,true,'Escolha "Salvar como PDF" na janela de impressão.');},b);}
function repHtml(k,b){repGet(k,function(R){
  dl(R.f+'.html','text/html','<!DOCTYPE html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+R.t+'</title><style>body{margin:0;background:#eef2f4}'+REP_CSS+'</style></head><body>'+R.body+'</body></html>');
  expMsg(b,true,'Relatório baixado: abre em qualquer navegador, sem internet.');},b);}
function dl(name,type,txt){var a=document.createElement('a');a.href=URL.createObjectURL(new Blob(['﻿'+txt],{type:type+';charset=utf-8'}));
  a.download=name;document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove();},500);}
function expMsg(b,ok,t){var m=b&&b.closest?b.closest('.exp').querySelector('.msg'):null;if(!m)return;m.textContent=t;m.className='msg '+(ok?'ok':'bad');}
var TSC={ta:10,rh:10,tr:10,ts:10,te:10,a:100,db:10};
function downloadCsv(k,b){
  if(!hasRep(k)){expMsg(b,false,'Sem dados para exportar.');return;}
  var rows;
  if(k==='dash'){rows=['data_hora;temp_c;ur_pct;co2_ppm;eco2_ppm;tvoc_ppb;aqi;ruido_dba;lux;t_ret_c;t_sup_c;t_ext_c;corrente_a'];
    SESS.r.forEach(function(r){var d=new Date(r[0]);rows.push(d.toLocaleDateString('pt-BR')+' '+d.toLocaleTimeString('pt-BR')+';'+r.slice(1).map(function(v){return v===null?'':String(+v.toFixed(2)).replace('.',',');}).join(';'));});
    dl('airq_sessao_'+fileStamp()+'.csv','text/csv',rows.join('\r\n'));}
  else{rows=['t_s;'+T.cols.join(';')];
    T.d.forEach(function(r,i){rows.push(i*T.step+';'+r.map(function(v,j){return v===null?'':String(v/(TSC[T.cols[j]]||1)).replace('.',',');}).join(';'));});
    dl('airq_'+k+'_'+fileTag(T.tag)+'.csv','text/csv',rows.join('\r\n'));}
  expMsg(b,true,'CSV baixado (separador ";", decimal com vírgula: abre direto no Excel).');
}
function downloadJson(k,b){
  if(!hasRep(k)){expMsg(b,false,'Sem dados para exportar.');return;}
  fetch('/cal').then(function(r){return r.json();}).then(function(c){CAL=c;}).catch(function(){}).then(function(){
    var o={aparelho:$e('node').textContent,firmware:LD&&LD.fw,gerado:new Date().toISOString(),responsavel:who()||null,calibracao:CAL};
    if(k==='dash'){o.tipo='sessao';o.referencia=refCtx().txt;o.intervalo_s=5;o.colunas=['t_ms'].concat(SC);o.dados=SESS.r;
      o.resumo=sessStats().map(function(x){return{parametro:x.n,unidade:x.u,min:x.mn,media:x.av,max:x.mx,situacao:x.s};});}
    else{var R=k==='ac'?evalAC():evalENV();o.tipo=k==='ac'?'laudo_arcond':'laudo_ambiente';o.local=T.tag;o.opcoes=T.opt;o.estado=T.state;
      o.duracao_s=T.el;o.intervalo_s=T.step;o.colunas=T.cols;o.escala=T.cols.map(function(c){return TSC[c]||1;});o.dados=T.d;
      o.resultado={veredito:R.v,criterios:R.cr.map(function(x){return{item:x.k,valor:x.v,situacao:x.s,observacao:x.p};})};}
    dl((k==='dash'?'airq_sessao_'+fileStamp():'airq_'+k+'_'+fileTag(T.tag))+'.json','application/json',JSON.stringify(o,null,1));
    expMsg(b,true,'JSON baixado.');});
}
function reportText(k){
  if(k!=='dash'){if(!T||!T.n)return '';var R=T.kind==='ac'?evalAC():evalENV();
    var L=['AirQ '+$e('node').textContent+' — '+(T.kind==='ac'?'Teste de ar-condicionado':'Teste de ambiente'),'Local: '+(T.tag||'—'),'Data: '+new Date().toLocaleString('pt-BR'),
      'Gravação: '+mmss(T.el)+' ('+T.n+' amostras a cada '+T.step+' s)','Resultado: '+VT[R.v][0].toUpperCase()];
    if(who())L.push('Responsável: '+who());L.push('');
    R.cr.forEach(function(c){L.push('['+{ok:'OK',wa:'!',bad:'X',na:'-'}[c.s]+'] '+c.k+': '+c.v+' — '+c.p);});return L.join('\n');}
  var st=sessStats(),n=SESS.r.length,L2=['AirQ '+$e('node').textContent+' — Monitoramento','Data: '+new Date().toLocaleString('pt-BR'),
    'Sessão: '+(n?mmss((SESS.r[n-1][0]-SESS.r[0][0])/1000):'00:00')+' · '+n+' leituras','Referência: '+refCtx().txt,''];
  st.forEach(function(x){L2.push('['+{ok:'OK',wa:'!',bad:'X',na:'-'}[x.s]+'] '+x.n+': atual '+fmt(x.cur,x.d)+' '+x.u+' · média '+fmt(x.av,x.d)+' (mín. '+fmt(x.mn,x.d)+', máx. '+fmt(x.mx,x.d)+') — '+x.r);});
  return L2.join('\n');
}
function copyReport(k,b){
  if(!hasRep(k)){expMsg(b,false,'Sem dados para copiar.');return;}
  var t=reportText(k);
  function done(ok){expMsg(b,ok,ok?'Texto copiado.':'Não foi possível copiar automaticamente: selecione o texto abaixo.');
    if(!ok){var ta=document.createElement('textarea');ta.value=t;ta.rows=10;ta.style.width='100%';b.closest('.exp').querySelector('.msg').appendChild(ta);ta.select();}}
  if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t).then(function(){done(true);},function(){done(false);});return;}
  var ta=document.createElement('textarea');ta.value=t;document.body.appendChild(ta);ta.select();
  var ok=false;try{ok=document.execCommand('copy');}catch(e){}document.body.removeChild(ta);done(ok);
}
function expInit(){
  [].forEach.call(document.querySelectorAll('.exp'),function(el){var k=el.getAttribute('data-k'),B=function(i,t,f){return '<button class="gbtn" type="button" onclick="'+f+'(\''+k+'\',this)">'+ic(i)+t+'</button>';};
    el.innerHTML='<h3>Exportar</h3><label class="who">Responsável técnico<input class="whoin" maxlength="60" autocomplete="name" placeholder="nome e registro (opcional)"></label>'
      +'<div class="row">'+B('print','PDF / imprimir','repPrint')+B('doc','Relatório .html','repHtml')+B('dl','CSV','downloadCsv')+B('dl','JSON','downloadJson')+B('copy','Copiar texto','copyReport')+'</div>'
      +'<p class="msg"></p><p class="note">Na janela de login do Wi-Fi o celular bloqueia downloads e impressão: abra <b>http://192.168.4.1</b> no navegador.</p>';});
  var W=document.querySelectorAll('.whoin');
  [].forEach.call(W,function(i){i.value=who();i.addEventListener('input',function(){try{localStorage.setItem('airq-who',i.value);}catch(e){}
    [].forEach.call(W,function(j){if(j!==i)j.value=i.value;});});});
}
function drawEnv(){var box=$e('env-charts');box.innerHTML='';envCharts(function(t,S,u,d){S=S.filter(function(s){return s.v.some(nn);});if(!S.length)return '';
  var w=document.createElement('div');w.innerHTML='<p class="ctitle">'+t+'</p><div class="chart"></div>';box.appendChild(w);lineChart(w.lastChild,S,u,d);return '';});}
// preferências: uso/estação/durações persistem neste navegador
['env-use','env-season','ac-dur','env-dur'].forEach(function(id){var el=$e(id);
  try{var v=localStorage.getItem('airq-'+id);if(v!==null&&[].some.call(el.options,function(o){return o.value===v;}))el.value=v;}catch(e){}
  el.addEventListener('change',function(){try{localStorage.setItem('airq-'+id,el.value);}catch(e){}if(curView==='dash')renderDash();});});
buildDash();expInit();renderDash();

try{var tv=localStorage.getItem('airq-tab');if(tv&&VIEWS.indexOf(tv)>=0)tab(tv);}catch(e){}
</script>
</body>
</html>
)rawhtml";

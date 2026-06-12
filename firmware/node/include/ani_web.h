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
<title>AirQ — Qualidade do Ar</title>
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

  <!-- body do node: um container por sensor -->
  <div class="node-bd">

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
          <span class="cl">eCO₂ · CO₂ equivalente</span>
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

    <!-- sensor KY-037/038 -->
    <div class="sensor">
      <div class="sensor-hd"><b>KY-037</b><span>· Ruído ambiente</span><span class="pill" id="d_snd">—</span></div>
      <div class="cards">
        <div class="card">
          <span class="cl">Nível sonoro (relativo)</span>
          <div class="bodyr">
            <div class="nums"><span class="cv" id="db">—</span><span class="cu">dB~</span><span class="cs" id="dbs"></span></div>
            <svg class="spark" id="dbk" viewBox="0 0 100 28" preserveAspectRatio="none"><polyline points=""/></svg>
          </div>
        </div>
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
var H={eco2:[],tvoc:[],temp:[],hum:[],lux:[],db:[]};
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

  // KY-037/038 — independente dos sensores I2C
  setPill('d_snd',!!d.sound);
  if(d.sound){
    var S=s2i(d.db|0);
    var de=$e('db');de.textContent=d.db;de.className='cv';de.style.color=S.c;
    var ds=$e('dbs');ds.textContent=S.l;ds.className='cs';ds.style.color=S.c;
    push('db',d.db|0);spark('dbk',H.db,S.c);
  }

  if(!d.valid){setDot('warn','sensor sem leitura');return;}

  sv('temp',parseFloat(d.temp_c).toFixed(1),null);
  sv('hum',Math.round(d.hum_pct),null);
  setAqi(d.aqi|0);
  var i=e2i(d.eco2);sv('eco2',d.eco2,i.c);$e('eco2s').textContent=i.l;$e('eco2s').className='cs '+i.c;
  var t=t2i(d.tvoc);sv('tvoc',d.tvoc,t.c);$e('tvocs').textContent=t.l;$e('tvocs').className='cs '+t.c;

  push('temp',parseFloat(d.temp_c));push('hum',d.hum_pct);push('eco2',d.eco2);push('tvoc',d.tvoc);
  spark('tempk',H.temp,'#4f46e5');
  spark('humk',H.hum,hc(d.hum_pct));
  spark('eco2k',H.eco2,d.eco2<600?'#16a34a':d.eco2<1000?'#d97706':'#dc2626');
  spark('tvock',H.tvoc,d.tvoc<150?'#16a34a':d.tvoc<500?'#d97706':'#dc2626');
}

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
</script>
</body>
</html>
)rawhtml";

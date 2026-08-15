#pragma once

// HTML da página de estado do GPS Tracker servida pelo AP captive.
// Armazenado em flash (PROGMEM). Acedido via http://192.168.4.1/
// Phone-first, mesma linguagem visual do node AirQ (node/include/ani_web.h):
// componentes empilhados, dark mode automático, sem dependência externa —
// o AP não tem internet, então qualquer <script src> ou <link href> remoto
// deixaria a página pendurada até dar timeout.

#include <pgmspace.h>   // PROGMEM — não depender de <Arduino.h> vir antes deste header

static const char GPS_WEB_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="pt">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>GPS Tracker</title>
<style>
:root{
  --bg:#f1f5f9;--s:#ffffff;--b:#e2e8f0;--t:#0f172a;--mu:#64748b;--mu2:#94a3b8;
  --ac:#0ea5e9;--sh:0 1px 2px rgba(0,0,0,.07);
}
@media (prefers-color-scheme:dark){
  :root{
    --bg:#0b1120;--s:#111827;--b:#1f2937;--t:#f1f5f9;--mu:#94a3b8;--mu2:#64748b;
    --ac:#38bdf8;--sh:0 1px 2px rgba(0,0,0,.4);
  }
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--t);font-family:system-ui,-apple-system,sans-serif;
  padding:10px 10px env(safe-area-inset-bottom);max-width:520px;margin:0 auto;-webkit-text-size-adjust:100%}
.card{background:var(--s);border:1px solid var(--b);border-radius:14px;box-shadow:var(--sh);
  overflow:hidden;margin-bottom:10px}
.hd{display:flex;align-items:center;gap:8px;flex-wrap:wrap;padding:10px 13px;border-bottom:1px solid var(--b)}
#dot{width:9px;height:9px;border-radius:50%;background:var(--mu2);flex-shrink:0;transition:background .4s,box-shadow .4s}
#dot.ok{background:#16a34a;box-shadow:0 0 0 3px rgba(22,163,74,.22);animation:pulse 2s infinite}
#dot.warn{background:#d97706;box-shadow:0 0 0 3px rgba(217,119,6,.22)}
#dot.err{background:#dc2626;box-shadow:0 0 0 3px rgba(220,38,38,.22)}
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(22,163,74,.4)}70%{box-shadow:0 0 0 5px rgba(22,163,74,0)}100%{box-shadow:0 0 0 0 rgba(22,163,74,0)}}
.ttl{font-size:.95rem;font-weight:800}
.ttl small{font-weight:600;color:var(--mu);font-size:.78rem;display:block}
#nid{font-size:.64rem;font-weight:700;color:var(--ac);background:rgba(14,165,233,.12);
  padding:2px 8px;border-radius:999px;font-variant-numeric:tabular-nums;margin-left:auto}
.bd{padding:11px 13px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:9px}
.grid.g3{grid-template-columns:1fr 1fr 1fr}
.m{background:var(--bg);border:1px solid var(--b);border-radius:10px;padding:8px 10px;min-width:0}
.ml{font-size:.63rem;font-weight:700;color:var(--mu);text-transform:uppercase;letter-spacing:.04em}
.mv{font-size:1.32rem;font-weight:800;font-variant-numeric:tabular-nums;line-height:1.18;
  white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.mv small{font-size:.66rem;font-weight:600;color:var(--mu);margin-left:2px}
.mh{font-size:.63rem;color:var(--mu);margin-top:1px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.coord{font-size:1.02rem;font-weight:800;font-variant-numeric:tabular-nums;letter-spacing:-.01em}
.pill{font-size:.6rem;font-weight:700;padding:2px 7px;border-radius:999px;white-space:nowrap}
.pill.ok{background:rgba(22,163,74,.15);color:#16a34a}
.pill.bad{background:rgba(220,38,38,.15);color:#dc2626}
.pill.idle{background:rgba(100,116,139,.15);color:var(--mu)}
.row{display:flex;align-items:center;justify-content:space-between;gap:8px;
  padding:7px 0;border-bottom:1px solid var(--b);font-size:.82rem}
.row:last-child{border-bottom:0}
.row .k{color:var(--mu);font-weight:600}
.row .v{font-weight:700;font-variant-numeric:tabular-nums;text-align:right;
  overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
a.map{display:block;text-align:center;background:var(--ac);color:#fff;font-weight:700;
  font-size:.8rem;padding:9px;border-radius:10px;text-decoration:none;margin-top:9px}
a.map[aria-disabled=true]{background:var(--mu2);pointer-events:none;opacity:.6}
form{display:grid;gap:8px}
fieldset{border:0;display:grid;gap:8px}
legend{font-size:.63rem;font-weight:700;color:var(--mu);text-transform:uppercase;
  letter-spacing:.04em;padding-bottom:5px}
label{display:grid;gap:3px;font-size:.7rem;font-weight:600;color:var(--mu)}
input{background:var(--bg);border:1px solid var(--b);border-radius:9px;padding:9px 10px;
  font-size:.92rem;color:var(--t);font-family:inherit;width:100%}
input:focus{outline:2px solid var(--ac);outline-offset:-1px;border-color:transparent}
.two{display:grid;grid-template-columns:2fr 1fr;gap:8px}
.ssidrow{display:grid;grid-template-columns:1fr auto;gap:6px;align-items:end}
button.gh{background:var(--bg);color:var(--ac);border:1px solid var(--b);
  font-size:.72rem;padding:9px 11px;white-space:nowrap;width:auto}
#nets{display:none;border:1px solid var(--b);border-radius:10px;overflow:hidden;margin-top:2px}
#nets.on{display:block}
.net{display:flex;align-items:center;gap:8px;width:100%;background:var(--s);border:0;
  border-bottom:1px solid var(--b);padding:9px 11px;font-family:inherit;font-size:.82rem;
  color:var(--t);text-align:left;cursor:pointer;border-radius:0}
.net:last-child{border-bottom:0}
.net:active{background:var(--bg)}
.net.sel{background:rgba(14,165,233,.12)}
.net .nm{flex:1;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.net .lk{font-size:.7rem;color:var(--mu2)}
.bars{display:flex;align-items:flex-end;gap:1px;height:12px;flex-shrink:0}
.bars i{width:3px;background:var(--b);border-radius:1px}
.bars i:nth-child(1){height:25%}
.bars i:nth-child(2){height:50%}
.bars i:nth-child(3){height:75%}
.bars i:nth-child(4){height:100%}
.bars i.on{background:#16a34a}
.bars.w i.on{background:#d97706}
.bars.b i.on{background:#dc2626}
#nmsg{font-size:.7rem;color:var(--mu);text-align:center;padding:9px}
button{background:var(--ac);color:#fff;border:0;border-radius:10px;padding:11px;
  font-size:.85rem;font-weight:700;font-family:inherit;cursor:pointer;width:100%;
  transition:transform .08s,filter .15s}
button:active{transform:scale(.985);filter:brightness(.92)}
button[disabled]{background:var(--mu2);cursor:default}
#msg{font-size:.75rem;font-weight:600;text-align:center;min-height:1em}
#msg.ok{color:#16a34a}
#msg.bad{color:#dc2626}
.ft{text-align:center;font-size:.63rem;color:var(--mu2);padding:2px 0 10px}
</style>
</head>
<body>

<div class="card">
  <div class="hd">
    <span id="dot"></span>
    <span class="ttl">GPS Tracker<small id="sl">a ligar…</small></span>
    <span id="nid">—</span>
  </div>
  <div class="bd">
    <div class="grid">
      <div class="m"><div class="ml">Latitude</div><div class="coord" id="lat">—</div></div>
      <div class="m"><div class="ml">Longitude</div><div class="coord" id="lon">—</div></div>
    </div>
    <a class="map" id="mapl" href="#" target="_blank" rel="noopener" aria-disabled="true">Abrir no mapa</a>
  </div>
</div>

<div class="card">
  <div class="hd"><span class="ttl">Movimento</span><span class="pill idle" id="mv">—</span></div>
  <div class="bd"><div class="grid g3">
    <div class="m"><div class="ml">Velocidade</div><div class="mv"><span id="spd">—</span><small>km/h</small></div></div>
    <div class="m"><div class="ml">Rumo</div><div class="mv" id="hdg">—</div><div class="mh" id="hsrc">—</div></div>
    <div class="m"><div class="ml">Altitude</div><div class="mv"><span id="alt">—</span><small>m</small></div></div>
  </div></div>
</div>

<div class="card">
  <div class="hd"><span class="ttl">Sinal</span></div>
  <div class="bd"><div class="grid g3">
    <div class="m"><div class="ml">Satélites</div><div class="mv" id="sat">—</div><div class="mh" id="satv">—</div></div>
    <div class="m"><div class="ml">HDOP</div><div class="mv" id="hdop">—</div><div class="mh" id="hq">—</div></div>
    <div class="m"><div class="ml">Precisão</div><div class="mv"><span id="acc">—</span><small>m</small></div></div>
  </div></div>
</div>

<div class="card">
  <div class="hd"><span class="ttl">Ambiente</span></div>
  <div class="bd"><div class="grid">
    <div class="m"><div class="ml">Temperatura</div><div class="mv"><span id="tmp">—</span><small>°C</small></div></div>
    <div class="m"><div class="ml">Humidade</div><div class="mv"><span id="hum">—</span><small>%</small></div></div>
  </div></div>
</div>

<div class="card">
  <div class="hd"><span class="ttl">Ligação</span></div>
  <div class="bd">
    <div class="row"><span class="k">WiFi de casa</span><span class="v"><span class="pill idle" id="wst">—</span></span></div>
    <div class="row"><span class="k">Endereço IP</span><span class="v" id="wip">—</span></div>
    <div class="row"><span class="k">Broker MQTT</span><span class="v"><span class="pill idle" id="mst">—</span></span></div>
    <div class="row"><span class="k">Envios</span><span class="v" id="pub">—</span></div>
    <div class="row"><span class="k">Ligado há</span><span class="v" id="up">—</span></div>
  </div>
</div>

<div class="card">
  <div class="hd"><span class="ttl">Configuração<small>guardada na placa, sobrevive a reinício</small></span></div>
  <div class="bd">
    <form id="cfg">
      <fieldset>
        <legend>WiFi de casa</legend>
        <div class="ssidrow">
          <label>Rede (SSID)<input name="wifi_ssid" id="f_ssid" maxlength="32" autocapitalize="off" autocomplete="off" spellcheck="false" required></label>
          <button type="button" class="gh" id="scan">Procurar</button>
        </div>
        <div id="nets"><div id="nmsg">a procurar redes…</div></div>
        <label>Palavra-passe<input name="wifi_pass" id="f_wpass" type="password" maxlength="63" autocomplete="off" placeholder="deixar vazio = rede aberta"></label>
      </fieldset>
      <fieldset>
        <legend>Broker MQTT</legend>
        <div class="two">
          <label>Endereço<input name="mqtt_host" id="f_host" maxlength="63" autocapitalize="off" autocomplete="off" spellcheck="false" placeholder="192.168.1.10"></label>
          <label>Porta<input name="mqtt_port" id="f_port" type="number" min="1" max="65535" value="1883"></label>
        </div>
        <label>Utilizador<input name="mqtt_user" id="f_user" maxlength="32" autocapitalize="off" autocomplete="off" spellcheck="false" placeholder="opcional"></label>
        <label>Palavra-passe<input name="mqtt_pass" id="f_mpass" type="password" maxlength="63" autocomplete="off" placeholder="opcional"></label>
      </fieldset>
      <button type="submit" id="sv">Guardar e ligar</button>
      <div id="msg"></div>
    </form>
  </div>
</div>

<div class="ft">Luctronics · ESP32-C3 SuperMini</div>

<script>
function $e(id){return document.getElementById(id);}
function txt(id,v){$e(id).textContent=v;}
function pill(id,cls,label){var el=$e(id);el.className='pill '+cls;el.textContent=label;}
function fmtUp(s){s=s|0;if(s<60)return s+'s';var m=(s/60)|0;if(m<60)return m+'m';
  var h=(m/60)|0;return h+'h '+(m%60)+'m';}
function card(d){var a=['N','NE','E','SE','S','SO','O','NO'];return a[Math.round(d/45)%8];}
function hq(h){if(h<=1)return'excelente';if(h<=2)return'bom';if(h<=5)return'aceitável';return'fraco';}

var missed=0;
var filled=false;   // formulario ja foi preenchido a partir do estado?

function upd(d){
  missed=0;
  txt('nid',d.id||'—');

  if(d.fix){
    $e('dot').className='ok';txt('sl','fixo válido');
    txt('lat',d.lat.toFixed(6));txt('lon',d.lon.toFixed(6));
    var m=$e('mapl');
    m.href='https://www.openstreetmap.org/?mlat='+d.lat+'&mlon='+d.lon+'#map=17/'+d.lat+'/'+d.lon;
    m.setAttribute('aria-disabled','false');
  }else{
    $e('dot').className=d.rx?'warn':'err';
    txt('sl',d.rx?'a procurar satélites…':'sem dados do módulo GPS');
    txt('lat','—');txt('lon','—');
    $e('mapl').setAttribute('aria-disabled','true');
  }

  pill('mv',d.moving?'ok':'idle',d.moving?'em movimento':'parado');
  txt('spd',d.fix?d.speed.toFixed(1):'—');
  txt('hdg',d.fix||d.hdg_mag?Math.round(d.hdg)+'°':'—');
  txt('hsrc',d.hdg_mag?card(d.hdg)+' · bússola':(d.fix?card(d.hdg)+' · GPS':'—'));
  txt('alt',d.fix?Math.round(d.alt):'—');

  txt('sat',d.sats);txt('satv',d.sats_view+' à vista');
  txt('hdop',d.hdop>=99?'—':d.hdop.toFixed(1));
  txt('hq',d.hdop>=99?'sem fixo':hq(d.hdop));
  txt('acc',d.fix?Math.round(d.hdop*5):'—');

  txt('tmp',d.env?d.temp.toFixed(1):'—');
  txt('hum',d.env?Math.round(d.hum):'—');

  pill('wst',d.wifi?'ok':'bad',d.wifi?'ligado':'fora de alcance');
  txt('wip',d.ip||'—');
  pill('mst',d.mqtt?'ok':'bad',d.mqtt?'ligado':'sem ligação');
  txt('pub',d.pub);
  txt('up',fmtUp(d.up));

  // Preenche UMA vez, e so campos ainda vazios. Antes isto corria enquanto
  // 'touched' fosse falso, o que deixava uma janela de ~2s (ate ao primeiro
  // poll) em que os campos estavam vazios: quem digitasse nesse intervalo
  // submetia os OUTROS campos em branco e apagava o que estava gravado --
  // foi assim que o mqtt_user se perdeu em bancada. Encher so o que esta
  // vazio nunca sobrepoe o que o utilizador escreveu.
  if(!filled){
    filled=true;
    if(!$e('f_ssid').value)$e('f_ssid').value=d.cfg_ssid||'';
    if(!$e('f_host').value)$e('f_host').value=d.cfg_host||'';
    if(!$e('f_user').value)$e('f_user').value=d.cfg_user||'';
    $e('f_port').value=d.cfg_port||1883;
    if(d.cfg_wpass)$e('f_wpass').placeholder='••••••• (guardada)';
    if(d.cfg_mpass)$e('f_mpass').placeholder='••••••• (guardada)';
  }
}

function tick(){
  fetch('/api/state',{cache:'no-store'}).then(function(r){return r.json();}).then(upd)
    .catch(function(){
      if(++missed>=3){$e('dot').className='err';txt('sl','sem ligação à placa');}
    });
}

['f_ssid','f_wpass','f_host','f_port','f_user','f_mpass'].forEach(function(id){
  $e(id).addEventListener('input',function(){$e('f_ssid').dataset.touched='1';});
});

// ── Scan de redes ────────────────────────────────────────────────────────
// O scan tira o rádio do canal do AP por alguns segundos, então esta ligação
// pode falhar a meio. Por isso: polling com tentativas, e um erro de rede não
// é tratado como falha — só a desistência final é.
var scanTries=0;

function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;')
  .replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

function bars(rssi){
  var lvl=rssi>=-55?4:rssi>=-67?3:rssi>=-78?2:1;
  var cls=lvl>=3?'':(lvl==2?'w':'b');
  var h='<span class="bars '+cls+'">';
  for(var i=1;i<=4;i++)h+='<i class="'+(i<=lvl?'on':'')+'"></i>';
  return h+'</span>';
}

function showNets(nets){
  var box=$e('nets');
  if(!nets.length){$e('nmsg').textContent='nenhuma rede encontrada';return;}
  nets.sort(function(a,b){return b.rssi-a.rssi;});
  var cur=$e('f_ssid').value;
  box.innerHTML=nets.map(function(n){
    var e=esc(n.ssid);
    return '<button type="button" class="net'+(n.ssid===cur?' sel':'')+'" data-s="'+e+'">'+
      bars(n.rssi)+'<span class="nm">'+e+'</span>'+
      '<span class="lk">'+(n.open?'aberta':'🔒')+' ch'+n.ch+'</span></button>';
  }).join('');
  Array.prototype.forEach.call(box.querySelectorAll('.net'),function(b){
    b.addEventListener('click',function(){
      $e('f_ssid').value=b.dataset.s;
      $e('f_ssid').dataset.touched='1';
      Array.prototype.forEach.call(box.querySelectorAll('.net'),function(o){o.classList.remove('sel');});
      b.classList.add('sel');
      $e('f_wpass').focus();
    });
  });
}

function pollScan(){
  fetch('/api/scan',{cache:'no-store'}).then(function(r){return r.json();}).then(function(j){
    if(j.status==='ok'){$e('scan').disabled=false;$e('scan').textContent='Procurar';showNets(j.nets);return;}
    if(++scanTries>15){$e('nmsg').textContent='scan demorou demais — tentar de novo';
      $e('scan').disabled=false;$e('scan').textContent='Procurar';return;}
    setTimeout(pollScan,1000);
  }).catch(function(){
    // Falha esperada enquanto o rádio está fora do canal do AP: insistir.
    if(++scanTries>15){$e('nmsg').textContent='sem resposta da placa';
      $e('scan').disabled=false;$e('scan').textContent='Procurar';return;}
    setTimeout(pollScan,1500);
  });
}

$e('scan').addEventListener('click',function(){
  scanTries=0;
  $e('scan').disabled=true;$e('scan').textContent='…';
  $e('nets').className='on';
  $e('nets').innerHTML='<div id="nmsg">a procurar redes… a ligação pode piscar</div>';
  pollScan();
});

$e('cfg').addEventListener('submit',function(ev){
  ev.preventDefault();
  var b=$e('sv'),m=$e('msg');
  b.disabled=true;b.textContent='A guardar…';m.className='';m.textContent='';
  fetch('/api/config',{method:'POST',body:new URLSearchParams(new FormData($e('cfg')))})
    .then(function(r){return r.json();})
    .then(function(j){
      m.className=j.ok?'ok':'bad';
      m.textContent=j.ok?'Guardado. A ligar à rede…':(j.err||'Falha ao guardar.');
      if(j.ok)$e('f_ssid').dataset.touched='';
    })
    .catch(function(){m.className='bad';m.textContent='Sem resposta da placa.';})
    .finally(function(){b.disabled=false;b.textContent='Guardar e ligar';});
});

tick();setInterval(tick,2000);
</script>
</body>
</html>
)rawhtml";

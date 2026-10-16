// AP de manutenção do gateway Ethernet — ver ap_portal.h.
// Padrão de página/rotas herdado de firmware/gps_tracker/src/portal.cpp.
#ifdef GATEWAY_ETH

#include "ap_portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_log.h>

#ifndef AP_BUTTON_PIN
#define AP_BUTTON_PIN 4                       // → GND, pull-up interno
#endif
#ifndef AP_IDLE_TIMEOUT_MS
#define AP_IDLE_TIMEOUT_MS (10UL * 60UL * 1000UL)
#endif
#ifndef MQTT_BROKER
#define MQTT_BROKER ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif
#ifndef AP_PASS_DEFAULT
#define AP_PASS_DEFAULT ""
#endif
static_assert(sizeof(AP_PASS_DEFAULT) - 1 >= 8,
              "secrets.ini: ap_pass precisa de 8+ caracteres (WPA2)");

#define NVS_NS        "gwcfg"
#define NODE_ROWS_MAX 24

static const char *TAG = "portal";

static const char PAGE[] = R"HTML(<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Gateway Aguada</title>
<style>
:root{--bg:#f1f5f9;--s:#fff;--b:#e2e8f0;--t:#0f172a;--mu:#64748b;--mu2:#94a3b8;--ac:#0ea5e9;--sh:0 1px 2px rgba(0,0,0,.07)}
@media (prefers-color-scheme:dark){:root{--bg:#0b1120;--s:#111827;--b:#1f2937;--t:#f1f5f9;--mu:#94a3b8;--mu2:#64748b;--ac:#38bdf8;--sh:0 1px 2px rgba(0,0,0,.4)}}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--t);font-family:system-ui,-apple-system,sans-serif;padding:10px 10px env(safe-area-inset-bottom);max-width:560px;margin:0 auto;-webkit-text-size-adjust:100%}
.card{background:var(--s);border:1px solid var(--b);border-radius:14px;box-shadow:var(--sh);overflow:hidden;margin-bottom:10px}
.hd{display:flex;align-items:center;gap:8px;padding:10px 13px;border-bottom:1px solid var(--b)}
summary.hd{cursor:pointer;list-style:none;border-bottom:0}
details[open] summary.hd{border-bottom:1px solid var(--b)}
summary.hd::after{content:"▾";margin-left:auto;color:var(--mu)}
details[open] summary.hd::after{content:"▴"}
#dot{width:9px;height:9px;border-radius:50%;background:var(--mu2);flex-shrink:0}
#dot.ok{background:#16a34a;box-shadow:0 0 0 3px rgba(22,163,74,.22)}
#dot.warn{background:#d97706;box-shadow:0 0 0 3px rgba(217,119,6,.22)}
#dot.err{background:#dc2626;box-shadow:0 0 0 3px rgba(220,38,38,.22)}
.ttl{font-size:.95rem;font-weight:800;min-width:0}
.ttl small{font-weight:600;color:var(--mu);font-size:.74rem;display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.bd{padding:11px 13px}
.pill{font-size:.62rem;font-weight:700;padding:2px 7px;border-radius:999px;white-space:nowrap}
.pill.ok{background:rgba(22,163,74,.15);color:#16a34a}
.pill.bad{background:rgba(220,38,38,.15);color:#dc2626}
.pill.idle{background:rgba(100,116,139,.15);color:var(--mu)}
.pill.warn{background:rgba(217,119,6,.15);color:#d97706}
.row{display:flex;align-items:center;justify-content:space-between;gap:8px;padding:7px 0;border-bottom:1px solid var(--b);font-size:.82rem}
.row:last-child{border-bottom:0}
.row .k{color:var(--mu);font-weight:600;flex-shrink:0}
.row .v{font-weight:700;font-variant-numeric:tabular-nums;text-align:right;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tw{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:.8rem;font-variant-numeric:tabular-nums}
th{font-size:.62rem;font-weight:700;color:var(--mu);text-transform:uppercase;letter-spacing:.04em;text-align:right;padding:0 6px 6px}
th:first-child,td:first-child{text-align:left;padding-left:0}
td{text-align:right;padding:7px 6px;border-top:1px solid var(--b);font-weight:700;white-space:nowrap}
td small{display:block;font-size:.6rem;font-weight:600;color:var(--mu)}
tr.old td{color:#d97706}
td.empty{text-align:center;color:var(--mu);font-weight:600;padding:16px 0;white-space:normal}
.err{color:#dc2626}
form{display:grid;gap:10px}
fieldset{border:0;display:grid;gap:8px}
legend{font-size:.63rem;font-weight:700;color:var(--mu);text-transform:uppercase;letter-spacing:.04em;padding-bottom:5px}
label{display:grid;gap:3px;font-size:.7rem;font-weight:600;color:var(--mu)}
input{background:var(--bg);border:1px solid var(--b);border-radius:9px;padding:9px 10px;font-size:.92rem;color:var(--t);font-family:inherit;width:100%;min-width:0}
input:focus{outline:2px solid var(--ac);outline-offset:-1px;border-color:transparent}
input:disabled{opacity:.45}
input:invalid:not(:placeholder-shown):not(:focus){border-color:#dc2626}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.two{display:grid;grid-template-columns:2fr 1fr;gap:8px}
.seg{display:grid;grid-template-columns:1fr 1fr;border:1px solid var(--b);border-radius:10px;overflow:hidden}
.seg label{display:block;text-align:center;padding:9px;font-size:.8rem;font-weight:700;color:var(--mu);cursor:pointer}
.seg input{position:absolute;opacity:0;width:0}
.seg label:has(input:checked){background:var(--ac);color:#fff}
button{background:var(--ac);color:#fff;border:0;border-radius:10px;padding:11px;font-size:.85rem;font-weight:700;font-family:inherit;cursor:pointer;width:100%;transition:transform .08s,filter .15s}
button:active{transform:scale(.985);filter:brightness(.92)}
button[disabled]{background:var(--mu2);cursor:default}
#msg{font-size:.75rem;font-weight:600;text-align:center;min-height:1em}
#msg.ok{color:#16a34a}#msg.bad{color:#dc2626}
#off{background:rgba(220,38,38,.12);color:#dc2626;font-size:.75rem;font-weight:700;text-align:center;padding:9px;border-radius:10px;margin-bottom:10px}
.ft{text-align:center;font-size:.63rem;color:var(--mu2);padding:2px 0 10px}
</style></head><body>
<div id="off" hidden>gateway sem resposta — o AP pode ter desligado</div>
<div class="card"><div class="hd"><span id="dot"></span>
<div class="ttl">Gateway Aguada<small id="sub">carregando…</small></div></div>
<div class="bd">
<div class="row"><span class="k">Ethernet</span><span class="v" id="eth">—</span></div>
<div class="row"><span class="k">MQTT</span><span class="v" id="mq">—</span></div>
<div class="row"><span class="k">Relógio</span><span class="v" id="clk">—</span></div>
<div class="row"><span class="k">Ligado há</span><span class="v" id="up">—</span></div>
<div class="row"><span class="k">Pacotes</span><span class="v" id="pk">—</span></div>
</div></div>
<div class="card"><div class="hd"><div class="ttl">Nodes<small id="nsub">último contato de cada node</small></div></div>
<div class="bd tw"><table><thead><tr><th>Node</th><th>Sensor</th><th>Dist.</th><th>RSSI</th><th>Idade</th></tr></thead>
<tbody id="nodes"><tr><td class="empty" colspan="5">carregando…</td></tr></tbody></table></div></div>
<details class="card"><summary class="hd"><div class="ttl">Configuração<small>rede, broker e senha do AP</small></div></summary>
<div class="bd"><form id="cfg">
<fieldset><legend>Rede Ethernet</legend>
<div class="seg"><label><input type="radio" name="dhcp" value="1" checked>DHCP</label><label><input type="radio" name="dhcp" value="0">IP fixo</label></div>
<div class="grid">
<label>IP<input name="ip" inputmode="decimal" pattern="((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)" title="IPv4, ex.: 192.168.0.50" autocomplete="off" placeholder="192.168.0.50"></label>
<label>Gateway<input name="gw" inputmode="decimal" pattern="((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)" title="IPv4, ex.: 192.168.0.50" autocomplete="off" placeholder="192.168.0.1"></label>
<label>Máscara<input name="mask" inputmode="decimal" pattern="((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)" title="IPv4, ex.: 192.168.0.50" autocomplete="off" placeholder="255.255.255.0"></label>
<label>DNS<input name="dns" inputmode="decimal" pattern="((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)" title="IPv4, ex.: 192.168.0.50" autocomplete="off" placeholder="vazio = gateway"></label>
</div></fieldset>
<fieldset><legend>Broker MQTT</legend>
<div class="two"><label>Endereço<input name="mqtt_host" required maxlength="63" autocapitalize="off" autocomplete="off" spellcheck="false"></label>
<label>Porta<input name="mqtt_port" type="number" min="1" max="65535" required></label></div>
<label>Usuário<input name="mqtt_user" maxlength="32" autocapitalize="off" autocomplete="off" spellcheck="false" placeholder="vazio = sem login"></label>
<label>Senha<input name="mqtt_pass" type="password" maxlength="63" autocomplete="new-password" placeholder="vazio = mantém a atual"></label>
</fieldset>
<fieldset><legend>AP de manutenção</legend>
<label>Nova senha do AP<input name="ap_pass" type="password" minlength="8" maxlength="63" autocomplete="new-password" placeholder="vazio = mantém a atual"></label>
</fieldset>
<button id="sv">Salvar e reiniciar</button><div id="msg"></div>
</form></div></details>
<div class="ft">O AP desliga 10 min depois que a página fecha. Botão do gateway liga de novo.</div>
<script>
var $=function(i){return document.getElementById(i)},F=$('cfg').elements,filled=false,fails=0,timer;
function esc(s){return String(s).replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function pill(c,t){return '<span class="pill '+c+'">'+esc(t)+'</span>'}
function dur(s){return s<60?s+' s':s<3600?Math.floor(s/60)+' min':s<86400?Math.floor(s/3600)+' h '+Math.floor(s%3600/60)+' min':Math.floor(s/86400)+' d '+Math.floor(s%86400/3600)+' h'}
function statik(){var st=F.dhcp.value==='0';['ip','gw','mask','dns'].forEach(function(n){F[n].disabled=!st;F[n].required=st&&n!=='dns'})}
function fill(c){F.dhcp.value=c.dhcp?'1':'0';F.ip.value=c.ip;F.gw.value=c.gw;F.mask.value=c.mask;F.dns.value=c.dns;
F.mqtt_host.value=c.host;F.mqtt_port.value=c.port;F.mqtt_user.value=c.user;
F.mqtt_pass.placeholder=c.mpass?'••• salva — vazio mantém':'nenhuma — vazio mantém';statik()}
function upd(j){fails=0;$('off').hidden=true;
$('sub').textContent=j.ssid+' · fw '+j.fw;
var ip=j.eth_link&&j.eth_ip;$('dot').className=ip&&j.mqtt?'ok':ip?'warn':'err';
$('eth').innerHTML=!j.eth_link?pill('bad','sem cabo'):!j.eth_ip?pill('warn','sem IP'):esc(j.eth_ip)+' '+pill('ok',j.eth_mbps+' Mbps');
$('mq').innerHTML=esc(j.broker)+' '+(j.mqtt?pill('ok','conectado'):pill('bad','tentando'));
$('clk').innerHTML=j.time?pill('ok','sincronizado'):pill('idle','sem hora');
$('up').textContent=dur(j.up);
$('pk').innerHTML=j.rx+' ok'+(j.crc?' · <span class="err">'+j.crc+' CRC</span>':'')+(j.drops?' · <span class="err">'+j.drops+' perdidos</span>':'');
var n=j.nodes.sort(function(a,b){return a.id<b.id?-1:a.id>b.id?1:a.s-b.s});
$('nsub').textContent=n.length?n.length+(n.length>1?' linhas':' linha')+' · destaque = sem contato há 5+ min':'último contato de cada node';
$('nodes').innerHTML=n.length?n.map(function(r){
var d=r.d===null?'—':r.d<0?'<span class="err">erro</span>':r.d+' cm';
return '<tr'+(r.age>=300?' class="old"':'')+'><td>'+esc(r.id)+'<small>'+esc(r.t)+'</small></td><td>'+(r.s||'—')+'</td><td>'+d+'</td><td>'+r.rssi+'</td><td>'+dur(r.age)+'</td></tr>'}).join('')
:'<tr><td class="empty" colspan="5">nenhum pacote ainda — nodes transmitem a cada ~2 min</td></tr>';
if(!filled){fill(j.cfg);filled=true}}
function tick(){fetch('/api/state',{cache:'no-store'}).then(function(r){return r.json()}).then(upd)
.catch(function(){if(++fails>=2){$('off').hidden=false;$('dot').className='err'}})}
F.dhcp[0].onchange=F.dhcp[1].onchange=statik;
$('cfg').onsubmit=function(e){e.preventDefault();var b=$('sv'),m=$('msg');
function fail(t){m.className='bad';m.textContent=t;b.disabled=false;b.textContent='Salvar e reiniciar'}
b.disabled=true;b.textContent='Salvando…';m.className='';m.textContent='';
fetch('/api/config',{method:'POST',body:new URLSearchParams(new FormData(this))}).then(function(r){return r.json()})
.then(function(j){if(!j.ok)return fail(j.err);clearInterval(timer);b.textContent='Salvo';m.className='ok';
m.textContent='Salvo. Gateway reiniciando — o AP desliga; aperte o botão para voltar.'})
.catch(function(){fail('sem resposta do gateway')})};
tick();timer=setInterval(tick,2000);
</script></body></html>)HTML";

// ── Estado ────────────────────────────────────────────────────────────────────

typedef struct {
    uint16_t node_id;
    uint8_t  sensor_id;
    uint8_t  type;
    int16_t  distance;      // -1 = erro do sensor, INT16_MIN = nunca veio SENSOR
    int8_t   rssi;
    uint32_t last_ms;
} node_row_t;

static WebServer   s_server(80);
static DNSServer   s_dns;
static const IPAddress kApIp(192, 168, 4, 1);

static const gw_cfg_t   *s_cfg = nullptr;
static portal_state_cb_t s_state_cb = nullptr;
static char       s_ssid[24] = "";
static bool       s_ap_on = false;
static uint32_t   s_last_req_ms = 0;
static uint32_t   s_reboot_at = 0;
static bool       s_btn_raw = true, s_btn_stable = true;
static uint32_t   s_btn_change_ms = 0;
static node_row_t s_rows[NODE_ROWS_MAX];
static int        s_rows_len = 0;

// ── Config (NVS) ──────────────────────────────────────────────────────────────

namespace portal {

void load_cfg(gw_cfg_t &c) {
    c.dhcp = true;
    c.ip = c.gw = c.mask = c.dns = IPAddress((uint32_t)0);
    strlcpy(c.mqtt_host, MQTT_BROKER, sizeof(c.mqtt_host));
    c.mqtt_port = MQTT_PORT;
    strlcpy(c.mqtt_user, MQTT_USER, sizeof(c.mqtt_user));
    strlcpy(c.mqtt_pass, MQTT_PASS, sizeof(c.mqtt_pass));
    strlcpy(c.ap_pass, AP_PASS_DEFAULT, sizeof(c.ap_pass));

    Preferences p;
    if (!p.begin(NVS_NS, true)) return;   // namespace ainda não existe: primeiro boot
    c.dhcp      = p.getBool("dhcp", c.dhcp);
    c.ip        = IPAddress(p.getUInt("ip", 0));
    c.gw        = IPAddress(p.getUInt("gw", 0));
    c.mask      = IPAddress(p.getUInt("mask", 0));
    c.dns       = IPAddress(p.getUInt("dns", 0));
    c.mqtt_port = p.getUShort("mport", c.mqtt_port);
    strlcpy(c.mqtt_host, p.getString("mhost", c.mqtt_host).c_str(), sizeof(c.mqtt_host));
    strlcpy(c.mqtt_user, p.getString("muser", c.mqtt_user).c_str(), sizeof(c.mqtt_user));
    strlcpy(c.mqtt_pass, p.getString("mpass", c.mqtt_pass).c_str(), sizeof(c.mqtt_pass));
    strlcpy(c.ap_pass,   p.getString("appass", c.ap_pass).c_str(), sizeof(c.ap_pass));
    p.end();
}

}  // namespace portal

static bool save_cfg(const gw_cfg_t &c) {
    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    p.putBool("dhcp", c.dhcp);
    p.putUInt("ip", (uint32_t)c.ip);
    p.putUInt("gw", (uint32_t)c.gw);
    p.putUInt("mask", (uint32_t)c.mask);
    p.putUInt("dns", (uint32_t)c.dns);
    p.putUShort("mport", c.mqtt_port);
    p.putString("mhost", c.mqtt_host);
    p.putString("muser", c.mqtt_user);
    p.putString("mpass", c.mqtt_pass);
    p.putString("appass", c.ap_pass);
    p.end();
    return true;
}

// ── AP ────────────────────────────────────────────────────────────────────────

static void ap_start(void) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
    // Mesmo canal do ESP-NOW: um rádio só, e os nodes mandam broadcast nesse canal.
    if (!WiFi.softAP(s_ssid, s_cfg->ap_pass, ESPNOW_CHANNEL)) {
        ESP_LOGE(TAG, "softAP falhou");
        WiFi.mode(WIFI_STA);
        esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
        return;
    }
    WiFi.setSleep(false);   // em AP_STA o driver liga power-save e o AP some do scan
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", kApIp);   // celular abre a página sozinho (captive portal)
    s_server.begin();
    s_ap_on = true;
    s_last_req_ms = millis();
    ESP_LOGI(TAG, "AP ligado ssid=%s ch=%d", s_ssid, ESPNOW_CHANNEL);
}

static void ap_stop(void) {
    s_server.stop();
    s_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    s_ap_on = false;
    ESP_LOGI(TAG, "AP desligado");
}

// ── HTTP ──────────────────────────────────────────────────────────────────────

// O servidor escuta em todas as interfaces; quem vem pela Ethernet leva 403.
static bool from_ap(void) {
    if (s_server.client().localIP() != WiFi.softAPIP()) {
        s_server.send(403, "text/plain", "so pelo AP de manutencao");
        return false;
    }
    s_last_req_ms = millis();
    return true;
}

static const char *type_name(uint8_t t) {
    switch (t) {
        case PKT_SENSOR:    return "SENSOR";
        case PKT_HEARTBEAT: return "HEARTBEAT";
        case PKT_HELLO:     return "HELLO";
        default:            return "?";
    }
}

static String ip_or_blank(const IPAddress &ip) {
    return (uint32_t)ip == 0 ? String() : ip.toString();
}

static void handle_state(void) {
    if (!from_ap()) return;
    JsonDocument doc;
    if (s_state_cb) s_state_cb(doc);
    doc["ssid"] = s_ssid;
    char broker[80];
    snprintf(broker, sizeof(broker), "%s:%u", s_cfg->mqtt_host, (unsigned)s_cfg->mqtt_port);
    doc["broker"] = broker;

    JsonObject c = doc["cfg"].to<JsonObject>();
    c["dhcp"]  = s_cfg->dhcp;
    c["ip"]    = ip_or_blank(s_cfg->ip);
    c["gw"]    = ip_or_blank(s_cfg->gw);
    c["mask"]  = ip_or_blank(s_cfg->mask);
    c["dns"]   = ip_or_blank(s_cfg->dns);
    c["host"]  = s_cfg->mqtt_host;
    c["port"]  = s_cfg->mqtt_port;
    c["user"]  = s_cfg->mqtt_user;
    c["mpass"] = s_cfg->mqtt_pass[0] != '\0';   // senha nunca volta pro navegador

    JsonArray nodes = doc["nodes"].to<JsonArray>();
    uint32_t now = millis();
    for (int i = 0; i < s_rows_len; i++) {
        const node_row_t &r = s_rows[i];
        JsonObject o = nodes.add<JsonObject>();
        char id[8];
        snprintf(id, sizeof(id), "0x%04X", r.node_id);
        o["id"]   = id;
        o["s"]    = r.sensor_id;
        o["t"]    = type_name(r.type);
        if (r.distance == INT16_MIN) o["d"] = nullptr;
        else                         o["d"] = r.distance;
        o["rssi"] = r.rssi;
        o["age"]  = (now - r.last_ms) / 1000UL;
    }

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void send_err(const char *msg) {
    JsonDocument doc;
    doc["ok"]  = false;
    doc["err"] = msg;
    String out;
    serializeJson(doc, out);
    s_server.send(400, "application/json; charset=utf-8", out);
}

// Campo de texto: rejeita em vez de truncar (credencial cortada = falha inexplicável).
// keep_if_empty: campo de senha vazio mantém o valor atual.
static bool take(const char *name, char *dst, size_t size, bool keep_if_empty) {
    String v = s_server.arg(name);
    if (v.length() == 0 && keep_if_empty) return true;
    if (v.length() >= size) return false;
    strlcpy(dst, v.c_str(), size);
    return true;
}

static void handle_config(void) {
    if (!from_ap()) return;
    gw_cfg_t c = *s_cfg;

    c.dhcp = s_server.arg("dhcp") != "0";
    if (!c.dhcp) {
        if (!c.ip.fromString(s_server.arg("ip")) || !c.gw.fromString(s_server.arg("gw")) ||
            !c.mask.fromString(s_server.arg("mask")))
            return send_err("IP, gateway e máscara precisam ser IPv4 válidos");
        if (s_server.arg("dns").length() == 0) c.dns = c.gw;
        else if (!c.dns.fromString(s_server.arg("dns"))) return send_err("DNS inválido");
    }

    if (s_server.arg("mqtt_host").length() == 0) return send_err("informe o endereço do broker");
    if (!take("mqtt_host", c.mqtt_host, sizeof(c.mqtt_host), false)) return send_err("endereço do broker longo demais");
    long port = s_server.arg("mqtt_port").toInt();
    if (port < 1 || port > 65535) return send_err("porta MQTT inválida");
    c.mqtt_port = (uint16_t)port;
    if (!take("mqtt_user", c.mqtt_user, sizeof(c.mqtt_user), false)) return send_err("usuário MQTT longo demais");
    if (!take("mqtt_pass", c.mqtt_pass, sizeof(c.mqtt_pass), true))  return send_err("senha MQTT longa demais");

    String ap = s_server.arg("ap_pass");
    if (ap.length() > 0 && (ap.length() < 8 || ap.length() > 63))
        return send_err("senha do AP: 8 a 63 caracteres");
    if (!take("ap_pass", c.ap_pass, sizeof(c.ap_pass), true)) return send_err("senha do AP longa demais");

    if (!save_cfg(c)) {
        s_server.send(500, "application/json; charset=utf-8", "{\"ok\":false,\"err\":\"NVS recusou a escrita\"}");
        return;
    }
    ESP_LOGI(TAG, "config salva dhcp=%d broker=%s:%u — reiniciando", c.dhcp, c.mqtt_host, c.mqtt_port);
    s_server.send(200, "application/json", "{\"ok\":true}");
    s_reboot_at = (millis() + 1500) | 1;   // dá tempo da resposta sair antes do restart
}

// Qualquer outra URL vai pra raiz: é o que dispara o aviso de captive portal no celular.
static void handle_not_found(void) {
    if (!from_ap()) return;
    s_server.sendHeader("Location", "http://192.168.4.1/", true);
    s_server.send(302, "text/plain", "");
}

// ── Botão ─────────────────────────────────────────────────────────────────────

static void button_tick(void) {
    bool raw = digitalRead(AP_BUTTON_PIN);
    uint32_t now = millis();
    if (raw != s_btn_raw) { s_btn_raw = raw; s_btn_change_ms = now; return; }
    if (raw == s_btn_stable || now - s_btn_change_ms < 50) return;   // debounce
    s_btn_stable = raw;
    if (raw) return;                                                 // só borda de descida
    if (s_ap_on) { s_last_req_ms = now; ESP_LOGI(TAG, "AP renovado"); }
    else         ap_start();
}

// ── API ───────────────────────────────────────────────────────────────────────

namespace portal {

void begin(const gw_cfg_t &cfg, const uint8_t mac[6], portal_state_cb_t state_cb) {
    s_cfg = &cfg;
    s_state_cb = state_cb;
    snprintf(s_ssid, sizeof(s_ssid), "aguada-gw-%02X%02X", mac[4], mac[5]);
    pinMode(AP_BUTTON_PIN, INPUT_PULLUP);

    s_server.on("/", HTTP_GET, []() {
        if (from_ap()) s_server.send(200, "text/html; charset=utf-8", PAGE);
    });
    s_server.on("/api/state", HTTP_GET, handle_state);
    s_server.on("/api/config", HTTP_POST, handle_config);
    s_server.onNotFound(handle_not_found);
}

void tick(void) {
    button_tick();
    if (s_reboot_at && (int32_t)(millis() - s_reboot_at) >= 0) ESP.restart();
    if (!s_ap_on) return;
    // ponytail: a página aberta faz polling e segura o AP ligado; tela do celular
    // apagada para o polling. Se virar problema, teto absoluto desde o aperto.
    if (millis() - s_last_req_ms > AP_IDLE_TIMEOUT_MS) { ap_stop(); return; }
    s_dns.processNextRequest();
    s_server.handleClient();
}

void note_packet(const espnow_packet_t *pkt) {
    uint32_t now = millis();
    bool sensor = pkt->type == PKT_SENSOR;

    // HEARTBEAT/HELLO renovam as linhas do node sem apagar a última distância.
    if (!sensor) {
        bool found = false;
        for (int i = 0; i < s_rows_len; i++) {
            if (s_rows[i].node_id != pkt->node_id) continue;
            s_rows[i].type = pkt->type;
            s_rows[i].rssi = pkt->rssi;
            s_rows[i].last_ms = now;
            found = true;
        }
        if (found) return;
    }

    uint8_t sid = sensor ? pkt->sensor_id : 0;
    int slot = -1, oldest = 0;
    for (int i = 0; i < s_rows_len; i++) {
        if (s_rows[i].node_id == pkt->node_id && s_rows[i].sensor_id == sid) { slot = i; break; }
        if (s_rows[i].last_ms < s_rows[oldest].last_ms) oldest = i;
    }
    if (slot < 0) {
        slot = (s_rows_len < NODE_ROWS_MAX) ? s_rows_len++ : oldest;
        s_rows[slot] = {pkt->node_id, sid, 0, INT16_MIN, 0, 0};
    }
    node_row_t &r = s_rows[slot];
    r.type = pkt->type;
    r.rssi = pkt->rssi;
    r.last_ms = now;
    if (sensor) r.distance = (pkt->flags & FLAG_SENSOR_ERROR) ? -1 : (int16_t)pkt->distance_cm;

    // Node que só mandava HEARTBEAT ganhou sensor: a linha "sensor 0" vira lixo.
    if (sensor) {
        for (int i = 0; i < s_rows_len; i++) {
            if (s_rows[i].node_id == pkt->node_id && s_rows[i].sensor_id == 0) {
                s_rows[i] = s_rows[--s_rows_len];
                break;
            }
        }
    }
}

bool ap_active(void) { return s_ap_on; }

}  // namespace portal

#endif  // GATEWAY_ETH

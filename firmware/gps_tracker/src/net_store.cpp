#include "net_store.h"
#include "net_config.h"

#include <Preferences.h>
#include <string.h>

namespace {

constexpr const char *kNamespace = "gpsnet";

Preferences prefs;

// Preferences::getString() into a fixed buffer needs the size including the
// terminator; this wrapper also guarantees termination when the stored value
// is exactly buffer-sized, which the underlying call does not.
void getStr(const char *key, char *out, size_t size) {
    prefs.getString(key, out, size);
    out[size - 1] = '\0';
}

// Copy with hard truncation at the destination's capacity. Callers pass
// browser-supplied strings, so an over-long field must clamp rather than
// overflow -- portal validates length first and rejects, this is the
// belt-and-braces layer behind it.
void setStr(char *dst, size_t size, const char *src) {
    strncpy(dst, src ? src : "", size - 1);
    dst[size - 1] = '\0';
}

}  // namespace

namespace net_store {

void load(NetCreds &c) {
    memset(&c, 0, sizeof(c));
    c.mqtt_port = DEFAULT_MQTT_PORT;

    if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
        return;   // never written yet -- blanks + default port is the correct first-boot state
    }

    getStr("wifi_ssid", c.wifi_ssid, sizeof(c.wifi_ssid));
    getStr("wifi_pass", c.wifi_pass, sizeof(c.wifi_pass));
    getStr("mqtt_host", c.mqtt_host, sizeof(c.mqtt_host));
    getStr("mqtt_user", c.mqtt_user, sizeof(c.mqtt_user));
    getStr("mqtt_pass", c.mqtt_pass, sizeof(c.mqtt_pass));
    c.mqtt_port = prefs.getUShort("mqtt_port", DEFAULT_MQTT_PORT);
    if (c.mqtt_port == 0) {
        c.mqtt_port = DEFAULT_MQTT_PORT;   // a stored 0 would be an unconnectable port
    }

    prefs.end();
}

bool save(const NetCreds &c) {
    if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
        return false;
    }

    NetCreds w;
    setStr(w.wifi_ssid, sizeof(w.wifi_ssid), c.wifi_ssid);
    setStr(w.wifi_pass, sizeof(w.wifi_pass), c.wifi_pass);
    setStr(w.mqtt_host, sizeof(w.mqtt_host), c.mqtt_host);
    setStr(w.mqtt_user, sizeof(w.mqtt_user), c.mqtt_user);
    setStr(w.mqtt_pass, sizeof(w.mqtt_pass), c.mqtt_pass);

    bool ok = prefs.putString("wifi_ssid", w.wifi_ssid) > 0 || w.wifi_ssid[0] == '\0';
    ok = (prefs.putString("wifi_pass", w.wifi_pass) > 0 || w.wifi_pass[0] == '\0') && ok;
    ok = (prefs.putString("mqtt_host", w.mqtt_host) > 0 || w.mqtt_host[0] == '\0') && ok;
    ok = (prefs.putString("mqtt_user", w.mqtt_user) > 0 || w.mqtt_user[0] == '\0') && ok;
    ok = (prefs.putString("mqtt_pass", w.mqtt_pass) > 0 || w.mqtt_pass[0] == '\0') && ok;
    ok = prefs.putUShort("mqtt_port", c.mqtt_port ? c.mqtt_port : DEFAULT_MQTT_PORT) > 0 && ok;

    prefs.end();
    return ok;
}

bool haveWifi(const NetCreds &c) { return c.wifi_ssid[0] != '\0'; }
bool haveMqtt(const NetCreds &c) { return c.mqtt_host[0] != '\0'; }

}  // namespace net_store

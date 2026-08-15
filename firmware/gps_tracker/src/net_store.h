#pragma once
// firmware/gps_tracker/src/net_store.h
//
// Persistent network credentials, in NVS. Mirrors firmware/node/src/
// nvs_config.{h,cpp}: load() fills a struct with stored values (or blanks),
// save() writes it back, and nothing else in the firmware touches the
// Preferences namespace.
//
// These fields are the ONLY place a credential lives. They are never
// compiled into the image (see net_config.h) and portal never echoes the
// two password fields back to the browser -- it reports whether one is set,
// not what it is.

#include <stdint.h>
#include <stdbool.h>

// Sized to the protocol maxima, +1 for the terminator: SSID is 32 bytes
// (IEEE 802.11), WPA2 passphrase is 63, and a broker host may be a
// hostname rather than a dotted quad.
struct NetCreds {
    char     wifi_ssid[33];
    char     wifi_pass[64];
    char     mqtt_host[64];
    uint16_t mqtt_port;
    char     mqtt_user[33];
    char     mqtt_pass[64];
};

namespace net_store {

// Fills `c` from NVS. Missing keys come back as empty strings and
// mqtt_port as DEFAULT_MQTT_PORT, so a first boot yields a valid struct
// with configured() == false rather than garbage. Always succeeds.
void load(NetCreds &c);

// Writes `c` to NVS. Returns false when the Preferences namespace could not
// be opened read-write, in which case the running config is unchanged on
// the next boot -- portal surfaces that as a failed save rather than
// pretending it stuck.
bool save(const NetCreds &c);

// True when there is enough to attempt a station association: an SSID.
// An open network legitimately has no password, so wifi_pass is not tested.
bool haveWifi(const NetCreds &c);

// True when there is enough to attempt a broker connection: a host.
// Anonymous brokers are legitimate, so mqtt_user is not tested.
bool haveMqtt(const NetCreds &c);

}  // namespace net_store

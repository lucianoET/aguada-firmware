#pragma once
// firmware/gps_tracker/src/portal.h
//
// Captive portal: a SoftAP that is always up plus a wildcard DNS that points
// every lookup at it, so a phone joining the AP pops the status page on its
// own. This is the tracker's field UI -- it must keep working with no home
// network, no broker and no internet anywhere in sight, which is why the AP
// is unconditional and never torn down when the station associates.
//
// Three routes:
//   GET  /            the page (PROGMEM, single file, no external assets --
//                     the AP has no internet, so a CDN reference would hang)
//   GET  /api/state   NetState as JSON, polled by the page
//   POST /api/config  WiFi + broker credentials -> net_store
// Everything else 302s to /, which is what makes the OS captive-portal
// probe fire instead of silently succeeding.
//
// Like display, this module reads no driver: serve() renders whatever
// NetState main.cpp last handed it. handleClient() is non-blocking except
// for the single send of the page body.

#include <stdint.h>
#include <stdbool.h>
#include "net_state.h"
#include "net_store.h"

namespace portal {

// Brings up WIFI_AP_STA, the SoftAP (SSID <prefix>-<mac3>, open) and the
// wildcard DNS, then starts the HTTP server. Sets AP_STA rather than AP so
// telemetry can associate to the home network later without the AP -- and
// the field UI with it -- ever dropping. Returns false when softAP() failed,
// in which case tick() is a no-op and the tracker still logs to serial.
bool begin();

// Services DNS and HTTP. Call from loop() on every iteration; both calls
// return immediately when there is nothing pending. `s` is cached and used
// to answer the next /api/state, so the page always shows the most recent
// frame main.cpp built rather than one assembled inside the HTTP handler.
void tick(const NetState &s);

// True exactly once after /api/config stored new credentials, so main.cpp
// can hand them to telemetry::reload(). Reading it clears the flag.
bool configDirty();

// The credentials as last saved, for main.cpp to forward on configDirty().
const NetCreds &creds();

// AP SSID actually in use, for the boot banner and [HEALTH].
const char *apSsid();

// Canal em que o radio esta neste instante. Em WIFI_AP_STA o C3 tem UM
// radio: uma tentativa de associacao arrasta o AP para o canal da rede
// alvo. Ver este numero mudar sozinho e a prova de que isso aconteceu.
uint8_t apChannel();

// Number of /api/state polls served since boot, for [HEALTH] -- a non-zero
// value is the proof that a phone actually reached the page.
uint32_t polls();

}  // namespace portal

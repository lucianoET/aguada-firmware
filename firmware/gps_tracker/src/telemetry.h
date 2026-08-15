#pragma once
// firmware/gps_tracker/src/telemetry.h
//
// Station-side uplink: joins the configured home WiFi when in range, keeps
// an MQTT session to the broker, and publishes NetState plus the Home
// Assistant discovery payloads that make the tracker appear as a device
// without any YAML on the HA side.
//
// The tracker is mobile, so "not connected" is the NORMAL state, not an
// error. Every entry point here must therefore be non-blocking: tick() may
// not spend more than a few milliseconds, because the GPS UART buffer has
// roughly a 266ms budget before bytes are lost (same constraint display and
// the I2C drivers work under). Association and broker connects are retried
// on DEFAULT_STA_RETRY_MS / DEFAULT_MQTT_RETRY_MS timers rather than in a
// wait loop -- a busy retry against an out-of-range network would starve
// the GPS pipeline for the whole trip.
//
// This module never owns WiFi mode. portal::begin() sets WIFI_AP_STA once
// and the AP stays up regardless of station state, so the captive page keeps
// working in the field with no broker and no home network in sight.

#include <stdint.h>
#include <stdbool.h>
#include "net_state.h"
#include "net_store.h"

namespace telemetry {

// Stores the credentials to use and derives the device identity (client id,
// discovery uid prefix, topic base) from the station MAC. Does not connect:
// the first association attempt happens on the first tick(). Safe to call
// with unconfigured creds -- tick() then does nothing until portal saves
// some and calls reload().
void begin(const NetCreds &c);

// Re-reads credentials after the portal saved new ones and drops any
// current session so the next tick() reconnects with them. Called by
// main.cpp when portal::configDirty() reports a save.
void reload(const NetCreds &c);

// Drives association, broker connect, keepalive and the publish timer.
// Call from loop() on every iteration; it self-throttles. `s` is the frame
// to publish when the publish timer is due -- ignored otherwise, so the
// caller may pass a freshly built state every time without cost.
void tick(const NetState &s);

// True once the station has an IP on the home network.
bool wifiConnected();

// True once there is a live MQTT session. Implies wifiConnected().
bool mqttConnected();

// Station IP as a dotted quad, or "-" when not associated. For the [HEALTH]
// line and the captive page.
const char *staIp();

// Number of successful state publishes since boot, for [HEALTH].
uint32_t published();

// Falhas de associacao consecutivas. Alimenta o backoff e aparece no
// [HEALTH] -- um valor alto e estavel diz "desisti de insistir", que e o
// comportamento correto fora de alcance, nao uma avaria.
uint32_t staFailCount();

}  // namespace telemetry

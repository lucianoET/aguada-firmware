// cadence decides only from f.speed_kmh and f.mono_ms — never Serial, never
// HardwareSerial, never the parser, never a distance/great-circle
// computation. It is reachable only from main.cpp's single
// GateResult::ACCEPT branch.

#include "cadence.h"
#include "gps_config.h"

const char *cadenceStateName(CadenceState s) {
    switch (s) {
        case CadenceState::MOVING:     return "MOVING";
        case CadenceState::STATIONARY: return "STATIONARY";
    }
    return "UNKNOWN";
}

const char *cadenceActionName(CadenceAction a) {
    switch (a) {
        case CadenceAction::EMIT:                return "EMIT";
        case CadenceAction::EMIT_HEARTBEAT:       return "EMIT_HEARTBEAT";
        case CadenceAction::SUPPRESS_STATIONARY:  return "SUPPRESS_STATIONARY";
        case CadenceAction::SUPPRESS_INTERVAL:    return "SUPPRESS_INTERVAL";
    }
    return "UNKNOWN";
}

CadenceAction Cadence::onGatedFix(const Fix &f) {
    lastKnownMonoMs_ = f.mono_ms;

    // --- 1. Classify the sample against the hysteresis band -------------
    const bool movingVote     = f.speed_kmh > DEFAULT_GPS_MOVING_KMH;
    const bool stationaryVote = f.speed_kmh < DEFAULT_GPS_STATIONARY_KMH;
    // A sample inside [DEFAULT_GPS_STATIONARY_KMH, DEFAULT_GPS_MOVING_KMH]
    // votes for neither and resets whichever streak is pending below.

    bool justEnteredMoving = false;

    // --- 2. State machine: debounced hysteresis flip ---------------------
    if (state_ == CadenceState::STATIONARY) {
        movingStreak_ = movingVote ? (movingStreak_ + 1) : 0;
        stationaryStreak_ = 0;   // not the pending streak in this state

        if (movingStreak_ >= DEFAULT_GPS_DEBOUNCE_FIXES) {
            lastTransitionElapsedS_ = (f.mono_ms - stateEnteredMs_) / 1000;
            lastTransitionSpeedKmh_ = f.speed_kmh;
            state_          = CadenceState::MOVING;
            stateEnteredMs_ = f.mono_ms;
            movingStreak_   = 0;
            stateChanged_   = true;
            justEnteredMoving = true;
        }
    } else {   // MOVING
        stationaryStreak_ = stationaryVote ? (stationaryStreak_ + 1) : 0;
        movingStreak_ = 0;   // not the pending streak in this state

        if (stationaryStreak_ >= DEFAULT_GPS_DEBOUNCE_FIXES) {
            lastTransitionElapsedS_ = (f.mono_ms - stateEnteredMs_) / 1000;
            lastTransitionSpeedKmh_ = f.speed_kmh;
            state_             = CadenceState::STATIONARY;
            stateEnteredMs_    = f.mono_ms;
            stationaryStreak_  = 0;
            stateChanged_      = true;
        }
    }

    // --- 3. Pacing decision ----------------------------------------------
    if (state_ == CadenceState::MOVING) {
        const uint32_t intervalMs = (uint32_t)DEFAULT_GPS_CADENCE_MOVING_S * 1000UL;
        const uint32_t elapsedMs  = haveEmitted_ ? (f.mono_ms - lastEmitMs_) : intervalMs;

        if (justEnteredMoving || !haveEmitted_ || elapsedMs >= intervalMs) {
            lastEmitMs_  = f.mono_ms;
            haveEmitted_ = true;
            return CadenceAction::EMIT;
        }
        return CadenceAction::SUPPRESS_INTERVAL;
    }

    // STATIONARY
    if (DEFAULT_GPS_STATIONARY_HEARTBEAT_S != 0) {
        const uint32_t heartbeatMs = (uint32_t)DEFAULT_GPS_STATIONARY_HEARTBEAT_S * 1000UL;
        const uint32_t elapsedMs   = haveEmitted_ ? (f.mono_ms - lastEmitMs_) : heartbeatMs;

        if (!haveEmitted_ || elapsedMs >= heartbeatMs) {
            lastEmitMs_  = f.mono_ms;
            haveEmitted_ = true;
            return CadenceAction::EMIT_HEARTBEAT;
        }
    }
    return CadenceAction::SUPPRESS_STATIONARY;
}

uint32_t Cadence::secondsSinceEmit() const {
    if (!haveEmitted_) return 0;
    return (lastKnownMonoMs_ - lastEmitMs_) / 1000;
}

bool Cadence::stateChanged() {
    bool changed = stateChanged_;
    stateChanged_ = false;
    return changed;
}

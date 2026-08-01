#pragma once
#include "gps_types.h"

// Cadence is a pure speed-driven, hysteresis-debounced pacing decision over
// a stream of gate-accepted Fix snapshots. It never touches the parser, the
// UART, or Serial (see cadence.cpp) — the only call site is main.cpp's
// single GateResult::ACCEPT branch, never anywhere else, so an ungated fix
// can never reach this state machine and flip moving/stationary.
//
// Movement is classified from f.speed_kmh (the receiver's own Doppler-
// derived velocity) only — never from the distance between consecutive
// positions, because stationary position wander on these single-frequency
// receivers reads as continuous slow motion, whereas the receiver's own
// speed field does not.
class Cadence {
public:
    // Initial state is STATIONARY: a tracker that has just booted has not
    // proven it is moving, and starting in MOVING would emit a burst of
    // parked points before the first real classification lands.
    Cadence() = default;

    // Runs the state machine (hysteresis band + debounce streak) first,
    // then the pacing decision (interval gate while MOVING, heartbeat-or-
    // suppress while STATIONARY). Call only from inside GateResult::ACCEPT.
    CadenceAction onGatedFix(const Fix &f);

    CadenceState state() const { return state_; }

    // Seconds elapsed since the last EMIT/EMIT_HEARTBEAT, from mono_ms.
    // Meaningless (0) before the first emit.
    uint32_t secondsSinceEmit() const;

    // One-shot: true only immediately after onGatedFix() flipped state_.
    // The caller consumes this flag (it resets on read) to know a
    // transition just happened, for the [STATE] log line.
    bool stateChanged();

private:
    CadenceState state_ = CadenceState::STATIONARY;

    uint8_t movingStreak_     = 0;   // consecutive moving votes while STATIONARY
    uint8_t stationaryStreak_ = 0;   // consecutive stationary votes while MOVING

    bool     haveEmitted_  = false;
    uint32_t lastEmitMs_   = 0;
    uint32_t lastKnownMonoMs_ = 0;   // f.mono_ms as of the most recent onGatedFix() call

    bool     stateChanged_ = false;
    uint32_t stateEnteredMs_ = 0;   // mono_ms at which the current state was entered
    float    lastTransitionSpeedKmh_ = 0.0f;
    uint32_t lastTransitionElapsedS_ = 0;   // seconds spent in the previous state

public:
    // Diagnostics for the [STATE] log line — valid only in the same call
    // that flips stateChanged_ true; read them before calling onGatedFix()
    // again.
    float    lastTransitionSpeedKmh() const { return lastTransitionSpeedKmh_; }
    uint32_t lastTransitionElapsedS() const { return lastTransitionElapsedS_; }
};

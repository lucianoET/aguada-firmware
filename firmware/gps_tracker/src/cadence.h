#pragma once
#include "gps_types.h"

// Cadence has TWO entry points with deliberately asymmetric authority
// (Fase 01.1, D-05):
//
//   - onGatedFix(f) is called exclusively from main.cpp's single
//     GateResult::ACCEPT branch (handleAccept()), never anywhere else, so an
//     ungated fix can never reach this state machine. It is the ONLY path
//     that may demote MOVING -> STATIONARY, via its hysteresis/debounce
//     streak over f.speed_kmh -- unchanged from Fase 1.
//   - onAccelWake(mono_ms) is called exclusively from main.cpp's
//     accelerometer wake-edge handler in loop() (accel_sensor::wakeEdge()),
//     never anywhere else. It can only PROMOTE STATIONARY -> MOVING, and
//     returns false with no effect when already MOVING.
//
// This asymmetry is deliberate, not an oversight: engine vibration on a
// parked vehicle must never be able to hold the tracker in MOVING
// indefinitely, so the authority to declare "stopped" belongs solely to the
// receiver's own Doppler-derived speed (f.speed_kmh) -- never to the
// accelerometer, and never to the distance between consecutive positions,
// because stationary position wander on these single-frequency receivers
// reads as continuous slow motion, whereas the receiver's own speed field
// does not. Cadence never touches the parser, the UART, or Serial (see
// cadence.cpp).
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

    // Accelerometer wake-edge entry point (D-05/D-06). Returns true only
    // when this call actually promoted STATIONARY -> MOVING; returns false
    // with no state change when already MOVING. Never demotes. Call only
    // from main.cpp's accelerometer wake-edge handler.
    bool onAccelWake(uint32_t mono_ms);

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

    // One-shot: armed by onAccelWake() when it promotes STATIONARY -> MOVING,
    // consumed by onGatedFix()'s pacing decision to force an immediate EMIT
    // on the first fix accepted after the wake (D-05's "fast resume").
    bool accelWakePending_ = false;

public:
    // Diagnostics for the [STATE] log line — valid only in the same call
    // that flips stateChanged_ true; read them before calling onGatedFix()
    // again.
    float    lastTransitionSpeedKmh() const { return lastTransitionSpeedKmh_; }
    uint32_t lastTransitionElapsedS() const { return lastTransitionElapsedS_; }
};

#pragma once
#include "gps_types.h"

// FixGate is a pure accept/reject decision function over an already-built
// Fix snapshot. It never touches the parser, the UART, or Serial (see
// fix_gate.cpp) — every call site must route a Fix through evaluate()
// before doing anything else with it, and may only forward the fix
// downstream inside the GateResult::ACCEPT branch.
class FixGate {
public:
    GateResult evaluate(const Fix &f);

    // Clears the warmup streak and the last-accepted reference position.
    void reset();

private:
    uint8_t  warmupStreak_ = 0;
    bool     haveLastAccepted_ = false;
    double   lastLat_ = 0.0;
    double   lastLon_ = 0.0;
    uint32_t lastMonoMs_ = 0;
};

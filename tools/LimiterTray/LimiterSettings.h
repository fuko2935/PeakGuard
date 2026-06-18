#pragma once

#include <windows.h>

#include "common/LimiterSharedState.h"

namespace CodexLimiter {

struct LimiterSettings {
    bool enabled = true;
    LONG ceilingMilliDb = kDefaultCeilingMilliDb;
};

inline void ApplySettings(LimiterSharedState* state, const LimiterSettings& settings) {
    if (state == nullptr) {
        return;
    }

    InterlockedExchange(const_cast<volatile LONG*>(&state->enabled), settings.enabled ? 1 : 0);
    SetCeilingMilliDb(state, settings.ceilingMilliDb);
}

inline LimiterSettings ReadSettingsFromState(const LimiterSharedState* state) {
    LimiterSettings settings{};
    if (state == nullptr) {
        return settings;
    }

    settings.enabled = state->enabled != 0;
    settings.ceilingMilliDb = ClampMilliDb(state->ceilingMilliDb);
    return settings;
}

} // namespace CodexLimiter

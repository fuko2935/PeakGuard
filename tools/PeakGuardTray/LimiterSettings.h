#pragma once

#include <windows.h>

#include "common/PeakGuardSharedState.h"

namespace PeakGuard {

enum class LimiterSettingsTab {
    Limiter = 0,
    Hotkeys = 1,
    Startup = 2,
    Power = 3,
};

struct LimiterSettings {
    bool enabled = true;
    LONG ceilingMilliDb = kDefaultCeilingMilliDb;
    bool boostEnabled = false;
    LONG boostMilliDb = kDefaultBoostMilliDb;
    bool overlayEnabled = true;
    bool startWithWindows = false;
    LimiterSettingsTab selectedTab = LimiterSettingsTab::Limiter;
    UINT lowerCeilingHotkey = 'A';
    UINT raiseCeilingHotkey = 'D';
    UINT toggleLimiterHotkey = 'S';
    UINT lowerBoostHotkey = VK_F6;
    UINT raiseBoostHotkey = VK_F7;
};

inline void ApplySettings(PeakGuardSharedState* state, const LimiterSettings& settings) {
    if (state == nullptr) {
        return;
    }

    InterlockedExchange(const_cast<volatile LONG*>(&state->enabled), settings.enabled ? 1 : 0);
    SetCeilingMilliDb(state, settings.ceilingMilliDb);
    InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), settings.boostEnabled ? 1 : 0);
    SetBoostMilliDb(state, settings.boostMilliDb);
}

inline LimiterSettings ReadSettingsFromState(const PeakGuardSharedState* state) {
    LimiterSettings settings{};
    if (state == nullptr) {
        return settings;
    }

    settings.enabled = state->enabled != 0;
    settings.ceilingMilliDb = ClampMilliDb(state->ceilingMilliDb);
    settings.boostEnabled = state->boostEnabled != 0;
    settings.boostMilliDb = ClampBoostMilliDb(state->boostMilliDb);
    return settings;
}

} // namespace PeakGuard

#pragma once

#include <windows.h>

#include <cstddef>
#include <string>

namespace PeakGuard {

constexpr int kHotkeyLowerCeilingId = 2001;
constexpr int kHotkeyRaiseCeilingId = 2002;
constexpr int kHotkeyToggleLimiterId = 2003;
constexpr int kHotkeyLowerBoostId = 2004;
constexpr int kHotkeyRaiseBoostId = 2005;

struct HotkeyBinding {
    int id;
    UINT modifiers;
    UINT key;
};

inline UINT LimiterHotkeyModifiers() {
    return MOD_CONTROL | MOD_ALT;
}

inline UINT LimiterHotkeyKey(int hotkeyId) {
    switch (hotkeyId) {
    case kHotkeyLowerCeilingId:
        return 'A';
    case kHotkeyRaiseCeilingId:
        return 'D';
    case kHotkeyToggleLimiterId:
        return 'S';
    case kHotkeyLowerBoostId:
        return VK_F6;
    case kHotkeyRaiseBoostId:
        return VK_F7;
    default:
        return 0;
    }
}

inline HotkeyBinding DefaultHotkeyBinding(int hotkeyId) {
    return HotkeyBinding{ hotkeyId, LimiterHotkeyModifiers(), LimiterHotkeyKey(hotkeyId) };
}

inline bool HasDuplicateLimiterHotkey(
    const HotkeyBinding* bindings,
    size_t count,
    int currentHotkeyId,
    UINT modifiers,
    UINT key) {
    if (bindings == nullptr) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (bindings[i].id != currentHotkeyId &&
            bindings[i].modifiers == modifiers &&
            bindings[i].key == key) {
            return true;
        }
    }
    return false;
}

inline bool IsSupportedLimiterHotkey(UINT modifiers, UINT key) {
    return modifiers == LimiterHotkeyModifiers() &&
        ((key >= 'A' && key <= 'Z') || (key >= VK_F1 && key <= VK_F24));
}

inline bool IsHotkeyCaptureModifierKey(UINT key) {
    return key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL ||
        key == VK_MENU || key == VK_LMENU || key == VK_RMENU ||
        key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT;
}

inline std::wstring LimiterHotkeyDisplay(UINT modifiers, UINT key) {
    if (!IsSupportedLimiterHotkey(modifiers, key)) {
        return L"Unassigned";
    }
    if (key >= 'A' && key <= 'Z') {
        wchar_t buffer[] = L"Ctrl+Alt+?";
        buffer[9] = static_cast<wchar_t>(key);
        return buffer;
    }
    if (key >= VK_F1 && key <= VK_F24) {
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"Ctrl+Alt+F%u", key - VK_F1 + 1);
        return buffer;
    }
    return L"Unassigned";
}

inline std::wstring LimiterHotkeyDisplay(int hotkeyId) {
    return LimiterHotkeyDisplay(LimiterHotkeyModifiers(), LimiterHotkeyKey(hotkeyId));
}

inline LONG LimiterHotkeyStepMilliDb() {
    return 1000;
}

} // namespace PeakGuard

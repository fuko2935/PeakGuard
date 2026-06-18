# Symmetric Black UI Redesign Implementation Plan

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild Codex Limiter's tray UI, overlay, hotkeys, startup behavior, and single-instance handling around the approved symmetric black Windows 11-style design.

**Architecture:** Keep the audio engine untouched except for reading existing shared state. Move deterministic policy into small headers tested by `LimiterAudioTests`; keep Win32 window creation, painting, timers, startup registration, and shell interaction in `tools/LimiterTray/main.cpp` unless a helper can be header-only and deterministic.

**Tech Stack:** C++17, Win32, CMake/MSVC, `RegisterHotKey`, user-level Windows startup registry, existing `LimiterSharedState`, existing native `LimiterAudioTests`.

---

## File Structure

- Create/extend `tools/LimiterTray/HotkeyPolicy.h`: hotkey action IDs, default key combinations, display text, validation helpers, and settings serialization names.
- Create `tools/LimiterTray/StartupPolicy.h`: pure helpers for startup registry value name and command normalization; Win32 registry calls stay in `main.cpp`.
- Modify `tools/LimiterTray/LimiterSettings.h`: add overlay, startup, selected tab, and hotkey setting fields; keep shared-state apply/read behavior compatible.
- Modify `tools/LimiterTray/main.cpp`: single-instance mutex/message, resizable symmetric UI, settings tabs, hotkey capture/register, startup toggle, bottom-center overlay, and timer gating.
- Modify `tools/LimiterAudioTests/main.cpp`: add expectations for hotkey defaults, settings defaults, startup policy helpers, and power/UI timing invariants.
- Keep `.superpowers/` mockup files out of git; they are brainstorming artifacts only.

### Task 1: Extend Hotkey Policy Under Test

**Files:**
- Create/modify: `tools/LimiterTray/HotkeyPolicy.h`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Write failing tests for all approved default hotkeys**

Add expectations in `tools/LimiterAudioTests/main.cpp` after existing hotkey tests:

```cpp
Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyLowerBoostId) == VK_F6,
    "Ctrl+Alt+F6 lowers soundbooster");
Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyRaiseBoostId) == VK_F7,
    "Ctrl+Alt+F7 raises soundbooster");
Expect(CodexLimiter::LimiterHotkeyDisplay(CodexLimiter::kHotkeyLowerCeilingId) == std::wstring(L"Ctrl+Alt+A"),
    "lower ceiling hotkey display is stable");
Expect(CodexLimiter::LimiterHotkeyDisplay(CodexLimiter::kHotkeyRaiseBoostId) == std::wstring(L"Ctrl+Alt+F7"),
    "raise boost hotkey display is stable");
Expect(CodexLimiter::IsSupportedLimiterHotkey(CodexLimiter::LimiterHotkeyModifiers(), 'A'),
    "Ctrl+Alt letter hotkeys are accepted");
Expect(!CodexLimiter::IsSupportedLimiterHotkey(MOD_ALT, 'A'),
    "hotkey validation rejects missing Ctrl modifier");
```

- [ ] **Step 2: Run test build to verify RED**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target LimiterAudioTests
```

Expected: compile failure for missing boost hotkey IDs/display helpers.

- [ ] **Step 3: Implement minimal hotkey policy**

Update `tools/LimiterTray/HotkeyPolicy.h` with:

```cpp
struct HotkeyBinding {
    int id;
    UINT modifiers;
    UINT key;
};

constexpr int kHotkeyLowerBoostId = 2004;
constexpr int kHotkeyRaiseBoostId = 2005;

inline HotkeyBinding DefaultHotkeyBinding(int hotkeyId) {
    return HotkeyBinding{ hotkeyId, LimiterHotkeyModifiers(), LimiterHotkeyKey(hotkeyId) };
}

inline bool IsSupportedLimiterHotkey(UINT modifiers, UINT key) {
    return modifiers == LimiterHotkeyModifiers() &&
        ((key >= 'A' && key <= 'Z') || (key >= VK_F1 && key <= VK_F24));
}

inline std::wstring LimiterHotkeyDisplay(int hotkeyId) {
    const UINT key = LimiterHotkeyKey(hotkeyId);
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
```

Extend `LimiterHotkeyKey` to return `VK_F6` and `VK_F7` for the boost IDs.

- [ ] **Step 4: Run tests to verify GREEN**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

Expected: build succeeds and prints `LimiterAudioTests passed`.

### Task 2: Persist New UI Settings

**Files:**
- Modify: `tools/LimiterTray/LimiterSettings.h`
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Write failing settings expectations**

Add to `LimiterAudioTests`:

```cpp
CodexLimiter::LimiterSettings defaultSettings{};
Expect(defaultSettings.overlayEnabled, "overlay defaults enabled");
Expect(!defaultSettings.startWithWindows, "start with Windows defaults off");
Expect(defaultSettings.selectedTab == CodexLimiter::LimiterSettingsTab::Limiter,
    "settings default to limiter tab");
```

Expected RED: missing fields/types.

- [ ] **Step 2: Add settings fields**

Update `LimiterSettings.h`:

```cpp
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
};
```

Keep `ApplySettings` and `ReadSettingsFromState` focused on shared audio state; the extra UI settings are persisted/read in `main.cpp`.

- [ ] **Step 3: Update settings file parsing/writing**

In `LoadSettings`, parse:

```cpp
} else if (key == "overlayEnabled" && TryParseLong(value, &parsed)) {
    settings.overlayEnabled = parsed != 0;
} else if (key == "startWithWindows" && TryParseLong(value, &parsed)) {
    settings.startWithWindows = parsed != 0;
} else if (key == "selectedTab" && TryParseLong(value, &parsed)) {
    settings.selectedTab = static_cast<CodexLimiter::LimiterSettingsTab>(
        std::max<LONG>(0, std::min<LONG>(3, parsed)));
}
```

In `SaveSettings`, write:

```cpp
file << "overlayEnabled=" << (settings.overlayEnabled ? 1 : 0) << "\n";
file << "startWithWindows=" << (settings.startWithWindows ? 1 : 0) << "\n";
file << "selectedTab=" << static_cast<int>(settings.selectedTab) << "\n";
```

- [ ] **Step 4: Run tests**

Run `LimiterAudioTests` build and executable. Expected: pass.

### Task 3: Single Instance Guard

**Files:**
- Modify: `tools/LimiterTray/main.cpp`

- [ ] **Step 1: Add single-instance constants and message**

Near globals:

```cpp
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\CodexLimiterTrayInstance";
constexpr wchar_t kMainWindowClassName[] = L"CodexLimiterTrayWindow";
constexpr UINT kShowExistingMessage = WM_APP + 20;
HANDLE g_singleInstanceMutex = nullptr;
```

- [ ] **Step 2: Check for existing instance before audio startup**

At the start of `wWinMain`, before COM/audio setup:

```cpp
g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
if (g_singleInstanceMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing = FindWindowW(kMainWindowClassName, nullptr);
    if (existing != nullptr) {
        PostMessageW(existing, kShowExistingMessage, 0, 0);
    }
    if (g_singleInstanceMutex != nullptr) {
        CloseHandle(g_singleInstanceMutex);
    }
    return 0;
}
```

- [ ] **Step 3: Handle show-existing message**

In `MainWindowProc`:

```cpp
case kShowExistingMessage:
    ShowMainWindow();
    return 0;
```

Release the mutex in shutdown after message loop:

```cpp
if (g_singleInstanceMutex != nullptr) {
    ReleaseMutex(g_singleInstanceMutex);
    CloseHandle(g_singleInstanceMutex);
    g_singleInstanceMutex = nullptr;
}
```

- [ ] **Step 4: Build `LimiterTray` Debug**

Run Debug tray build. Expected: compile succeeds.

Manual verification step: launch two instances; the second should show the existing window and exit.

### Task 4: Startup Policy And Toggle

**Files:**
- Create: `tools/LimiterTray/StartupPolicy.h`
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Write failing pure helper tests**

Add include and expectations:

```cpp
#include "tools/LimiterTray/StartupPolicy.h"

Expect(CodexLimiter::StartupRegistryValueName() == std::wstring(L"CodexLimiter"),
    "startup registry value name is stable");
Expect(CodexLimiter::QuoteStartupCommand(L"C:\\Apps\\LimiterTray.exe") ==
        std::wstring(L"\"C:\\Apps\\LimiterTray.exe\""),
    "startup command quotes executable path");
```

- [ ] **Step 2: Add `StartupPolicy.h`**

```cpp
#pragma once

#include <string>

namespace CodexLimiter {

inline std::wstring StartupRegistryValueName() {
    return L"CodexLimiter";
}

inline std::wstring QuoteStartupCommand(const std::wstring& executablePath) {
    return L"\"" + executablePath + L"\"";
}

} // namespace CodexLimiter
```

- [ ] **Step 3: Add user-level registry helpers in `main.cpp`**

Use `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`.

Implement:

```cpp
bool SetStartWithWindows(bool enabled);
bool IsStartWithWindowsEnabled();
```

Use `RegSetValueExW` when enabled, `RegDeleteValueW` when disabled, and no elevation.

- [ ] **Step 4: Wire startup setting UI**

The Startup tab will call `SetStartWithWindows`, update `g_settings.startWithWindows`, save settings, and refresh controls.

- [ ] **Step 5: Run tests and Debug tray build**

Expected: helper tests pass; tray compiles.

Manual verification step: toggle startup, inspect the current-user Run key, then toggle startup off.

### Task 5: Resizable Symmetric Main UI

**Files:**
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterTray/AudioPowerPolicy.h`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Add layout policy expectations**

In `LimiterAudioTests`, update UI size expectations:

```cpp
Expect(CodexLimiter::UiWindowMinWidthPx() >= 420,
    "main window minimum width prevents text clipping");
Expect(CodexLimiter::UiWindowMinHeightPx() >= 420,
    "main window minimum height prevents status clipping");
```

- [ ] **Step 2: Add layout constants**

In `AudioPowerPolicy.h`:

```cpp
constexpr int UiWindowMinWidthPx() { return 460; }
constexpr int UiWindowMinHeightPx() { return 440; }
```

Keep `UiUpdateIntervalMs()` unchanged.

- [ ] **Step 3: Make main window resizable**

Change window style from:

```cpp
WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
```

to:

```cpp
WS_OVERLAPPEDWINDOW
```

Handle `WM_GETMINMAXINFO`:

```cpp
MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
info->ptMinTrackSize.x = CodexLimiter::UiWindowMinWidthPx();
info->ptMinTrackSize.y = CodexLimiter::UiWindowMinHeightPx();
return 0;
```

- [ ] **Step 4: Replace fixed child layout with `LayoutControls`**

Create:

```cpp
void LayoutControls(HWND window) {
    RECT rect{};
    GetClientRect(window, &rect);
    const int width = rect.right - rect.left;
    const int centerX = width / 2;
    const int contentWidth = std::max(360, std::min(520, width - 40));
    const int left = centerX - contentWidth / 2;
    // MoveWindow each child around the center axis.
}
```

Call it from `CreateControls`, `WM_SIZE`, and after tab changes.

- [ ] **Step 5: Update static/control text for symmetry**

Use centered static controls where possible:

```cpp
SS_CENTER
```

Shorten status with a helper:

```cpp
std::wstring BuildShortStatus(CodexLimiter::AudioEngineState engineState, const std::wstring& deviceName);
```

Return examples: `Active - CABLE loopback - hotkeys OK`, `Idle - CABLE loopback`, `Error - hotkeys unavailable`.

- [ ] **Step 6: Build Debug tray**

Expected: compile succeeds. Visual validation happens manually after full implementation.

### Task 6: Settings Tabs And Hotkey Editing UI

**Files:**
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterTray/HotkeyPolicy.h`

- [ ] **Step 1: Add UI mode globals**

```cpp
CodexLimiter::LimiterSettings g_settings;
CodexLimiter::LimiterSettingsTab g_selectedTab = CodexLimiter::LimiterSettingsTab::Limiter;
int g_capturingHotkeyId = 0;
std::wstring g_settingsMessage;
```

- [ ] **Step 2: Add tab buttons**

Create child buttons for `Limiter`, `Keys`, `Startup`, `Power`. Use symmetric equal widths in `LayoutControls`.

- [ ] **Step 3: Show/hide controls per tab**

Implement:

```cpp
void ApplyTabVisibility() {
    const bool limiterTab = g_selectedTab == CodexLimiter::LimiterSettingsTab::Limiter;
    ShowWindow(g_slider, limiterTab ? SW_SHOW : SW_HIDE);
    // Same for boost slider, tiles, hotkey buttons, startup button, power labels.
}
```

- [ ] **Step 4: Add hotkey capture controls**

For each action, add one button with display text. When clicked:

```cpp
g_capturingHotkeyId = actionId;
SetWindowTextW(button, L"Press shortcut...");
SetFocus(g_window);
```

In `MainWindowProc`, intercept `WM_KEYDOWN` when `g_capturingHotkeyId != 0`; build modifiers from `GetKeyState(VK_CONTROL)` and `GetKeyState(VK_MENU)`, validate with `IsSupportedLimiterHotkey`, save, re-register, and update labels.

- [ ] **Step 5: Add reset defaults**

Add a reset button that restores all default hotkeys and re-registers them.

- [ ] **Step 6: Build Debug tray**

Expected: compile succeeds.

### Task 7: Bottom-Center Overlay Redesign

**Files:**
- Modify: `tools/LimiterTray/main.cpp`

- [ ] **Step 1: Add overlay content state**

```cpp
std::wstring g_overlayTitle = L"Limiter";
std::wstring g_overlayValue;
bool g_overlayEnabled = true;
```

- [ ] **Step 2: Move overlay to bottom center**

Update `PositionOverlay`:

```cpp
const int x = workArea.left + ((workArea.right - workArea.left) - kOverlayWidth) / 2;
const int y = workArea.bottom - kOverlayHeight - 80;
SetWindowPos(g_overlay, HWND_TOPMOST, x, y, kOverlayWidth, kOverlayHeight, SWP_NOACTIVATE);
```

- [ ] **Step 3: Use black symmetric drawing**

In `OverlayProc`, center title/value with a three-column header feel:

```cpp
DrawTextW(dc, g_overlayTitle.c_str(), -1, &leftRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
DrawTextW(dc, L"-", -1, &centerRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
DrawTextW(dc, g_overlayValue.c_str(), -1, &rightRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
```

Use black/antracite background and amber fill.

- [ ] **Step 4: Show contextual overlay**

Add:

```cpp
void ShowLimiterOverlay(const std::wstring& title, const std::wstring& value);
```

Call with:

- `Limiter`, `FormatDb(g_state->ceilingMilliDb)` for limiter changes.
- `Soundbooster`, `FormatBoostDb(g_state->boostMilliDb)` for boost changes.
- `Limiter`, `On`/`Off` for toggle.

- [ ] **Step 5: Respect overlay setting**

Return early from `ShowLimiterOverlay` if `!g_settings.overlayEnabled`.

### Task 8: Soundbooster Hotkey Actions

**Files:**
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Ensure tests cover boost hotkeys**

Covered by Task 1; add any missing uniqueness expectation:

```cpp
Expect(CodexLimiter::kHotkeyLowerBoostId != CodexLimiter::kHotkeyLowerCeilingId,
    "boost hotkey ids do not overlap limiter ids");
```

- [ ] **Step 2: Register boost hotkeys**

Extend `RegisterLimiterHotkeys`:

```cpp
RegisterOneHotkey(CodexLimiter::kHotkeyLowerBoostId, L"Ctrl+Alt+F6");
RegisterOneHotkey(CodexLimiter::kHotkeyRaiseBoostId, L"Ctrl+Alt+F7");
```

- [ ] **Step 3: Handle boost hotkeys**

Extend `HandleLimiterHotkey`:

```cpp
case CodexLimiter::kHotkeyLowerBoostId:
    AdjustBoostByMilliDb(-CodexLimiter::LimiterHotkeyStepMilliDb());
    ShowLimiterOverlay(L"Soundbooster", FormatBoostDb(g_state->boostMilliDb));
    return true;
case CodexLimiter::kHotkeyRaiseBoostId:
    AdjustBoostByMilliDb(CodexLimiter::LimiterHotkeyStepMilliDb());
    ShowLimiterOverlay(L"Soundbooster", FormatBoostDb(g_state->boostMilliDb));
    return true;
```

- [ ] **Step 4: Unregister boost hotkeys**

Add matching `UnregisterHotKey` calls.

- [ ] **Step 5: Run tests and Debug tray build**

Expected: pass/compile.

### Task 9: Power-Efficient Timer Gating

**Files:**
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterAudioTests/main.cpp` if helper extracted

- [ ] **Step 1: Add visible-state helper**

In `main.cpp`:

```cpp
bool ShouldRunUiTimer() {
    return (g_window != nullptr && IsWindowVisible(g_window)) ||
        (g_overlay != nullptr && IsWindowVisible(g_overlay));
}
```

- [ ] **Step 2: Gate timer start/stop**

Update `StartUiTimer`:

```cpp
if (g_window != nullptr && ShouldRunUiTimer()) {
    SetTimer(g_window, kUiTimer, CodexLimiter::UiUpdateIntervalMs(), nullptr);
}
```

Update `StopUiTimer` and `HideLimiterOverlay` to kill timers when no visible UI remains.

- [ ] **Step 3: Avoid idle animations**

Confirm no new animation timers are introduced. Overlay hide timer only runs after overlay show.

- [ ] **Step 4: Build Debug tray**

Expected: compile succeeds.

### Task 10: Final Verification And Manual Handoff

**Files:**
- No source edits expected.

- [ ] **Step 1: Run native tests**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

Expected: `LimiterAudioTests passed`.

- [ ] **Step 2: Build Release tray**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release --target LimiterTray
```

Expected: Release `LimiterTray.exe` builds.

- [ ] **Step 3: Manual desktop verification**

Run the Release app and verify:

- The main window is resizable and does not clip status text.
- The UI follows the approved black symmetric layout.
- `Ctrl+Alt+A/D/S` control limiter.
- `Ctrl+Alt+F6/F7` control Soundbooster.
- Overlay appears bottom-center and hides.
- Hotkey editing captures and persists replacements.
- Start with Windows toggles the current-user Run entry and can remove it.
- Launching a second instance shows the existing window and exits.
- UI timer does not run while window and overlay are hidden.

- [ ] **Step 4: Git hygiene**

Do not stage `.superpowers/`, `build/`, binaries, screenshots, or logs. Stage only source, test, and docs changes.

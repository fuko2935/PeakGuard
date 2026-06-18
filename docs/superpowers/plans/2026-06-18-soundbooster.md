# Soundbooster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an independent tray-controlled soundbooster with `0..+24 dB` gain that can be used together with the existing limiter.

**Architecture:** Store booster state in the existing shared memory contract, persist it through existing settings, expose it through new Win32 controls in the tray window, and apply booster gain before limiter processing in the current WASAPI loopback DSP path. The VB-CABLE routing model and endpoint behavior stay unchanged.

**Tech Stack:** C++17, Win32, WASAPI, CMake, MSVC-compatible native tests.

---

## File Structure

- Modify `common/LimiterSharedState.h`: add booster ABI fields, clamp/conversion helpers, initialization, and interlocked setter.
- Modify `tools/LimiterTray/LimiterSettings.h`: add booster fields to persisted settings translation.
- Modify `tools/LimiterTray/main.cpp`: load/save booster settings and add tray-window checkbox, label, slider, keyboard, and repaint handling.
- Modify `tools/LimiterTray/LoopbackAudioEngine.h`: rename the private DSP method from limiter-only naming to audio-processing naming.
- Modify `tools/LimiterTray/LoopbackAudioEngine.cpp`: apply booster gain before limiter and keep meter counters updated.
- Modify `tools/LimiterTray/AudioPowerPolicy.h`: increase the UI window height constant if it currently returns too little space.
- Modify `tools/LimiterStateProbe/main.cpp`: print and mutate booster fields for diagnostics because the shared state contract changes.
- Modify `tools/LimiterAudioTests/main.cpp`: add deterministic tests for booster shared-state and settings helpers.

## Task 1: Add Failing Booster Helper Tests

**Files:**
- Modify: `tools/LimiterAudioTests/main.cpp`
- Test: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Add booster expectations after `InitializeStateFields(&state);` and before the existing `ApplySettings` call**

Insert this code in `tools/LimiterAudioTests/main.cpp` inside `main`, immediately after:

```cpp
CodexLimiter::InitializeStateFields(&state);
```

Code to insert:

```cpp
Expect(state.boostEnabled == 0, "shared state defaults booster off");
Expect(state.boostMilliDb == CodexLimiter::kDefaultBoostMilliDb,
    "shared state defaults boost to 0 dB");
Expect(state.boostLinearScaled == CodexLimiter::BoostMilliDbToLinearScaled(CodexLimiter::kDefaultBoostMilliDb),
    "shared state initializes linear boost");
Expect(CodexLimiter::ClampBoostMilliDb(-1000) == CodexLimiter::kMinBoostMilliDb,
    "boost clamp rejects negative values");
Expect(CodexLimiter::ClampBoostMilliDb(0) == 0,
    "boost clamp accepts 0 dB");
Expect(CodexLimiter::ClampBoostMilliDb(24000) == 24000,
    "boost clamp accepts 24 dB");
Expect(CodexLimiter::ClampBoostMilliDb(25000) == CodexLimiter::kMaxBoostMilliDb,
    "boost clamp rejects above-range values");
Expect(CodexLimiter::BoostMilliDbToLinearScaled(0) == CodexLimiter::kLinearScale,
    "0 dB boost maps to unity gain");
Expect(CodexLimiter::BoostMilliDbToLinearScaled(24000) == 15848931925LL,
    "24 dB boost maps to rounded 10^(24/20) scaled gain");
```

- [ ] **Step 2: Extend the settings fixture before `ApplySettings`**

Replace:

```cpp
settings.ceilingMilliDb = -120000;
```

with:

```cpp
settings.ceilingMilliDb = -120000;
settings.boostEnabled = true;
settings.boostMilliDb = 25000;
```

- [ ] **Step 3: Add settings expectations after the existing linear ceiling expectation**

Insert after:

```cpp
Expect(state.ceilingLinearScaled == CodexLimiter::MilliDbToLinearScaled(CodexLimiter::kMinCeilingMilliDb),
    "settings restore updates linear ceiling");
```

Code to insert:

```cpp
Expect(state.boostEnabled == 1, "settings restore enabled booster state");
Expect(state.boostMilliDb == CodexLimiter::kMaxBoostMilliDb,
    "settings restore clamps too-high boost");
Expect(state.boostLinearScaled == CodexLimiter::BoostMilliDbToLinearScaled(CodexLimiter::kMaxBoostMilliDb),
    "settings restore updates linear boost");

CodexLimiter::LimiterSettings readSettings = CodexLimiter::ReadSettingsFromState(&state);
Expect(readSettings.boostEnabled, "settings read returns booster enabled state");
Expect(readSettings.boostMilliDb == CodexLimiter::kMaxBoostMilliDb,
    "settings read clamps boost value");
```

- [ ] **Step 4: Run tests to verify they fail for missing booster API**

Run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
```

Expected: compile fails with errors mentioning missing `boostEnabled`, `boostMilliDb`, `kDefaultBoostMilliDb`, `ClampBoostMilliDb`, or `BoostMilliDbToLinearScaled`.

- [ ] **Step 5: Commit the failing tests**

```powershell
git add tools/LimiterAudioTests/main.cpp
git commit -m "test: cover soundbooster shared settings"
```

## Task 2: Implement Shared State And Settings Helpers

**Files:**
- Modify: `common/LimiterSharedState.h`
- Modify: `tools/LimiterTray/LimiterSettings.h`
- Test: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Update constants and shared ABI in `common/LimiterSharedState.h`**

Replace:

```cpp
constexpr LONG kStateVersion = 2;
```

with:

```cpp
constexpr LONG kStateVersion = 3;
```

Add after the ceiling constants:

```cpp
constexpr LONG kDefaultBoostMilliDb = 0;
constexpr LONG kMinBoostMilliDb = 0;
constexpr LONG kMaxBoostMilliDb = 24000;
constexpr LONGLONG kMaxBoostLinearScaled = 15848931925LL;
```

Replace the tail of `LimiterSharedState`:

```cpp
volatile LONG sampleRate;
volatile LONG _reserved[4];
```

with:

```cpp
volatile LONG sampleRate;
volatile LONG boostEnabled;
volatile LONG boostMilliDb;
volatile LONGLONG boostLinearScaled;
volatile LONG _reserved[2];
```

- [ ] **Step 2: Add boost clamp and conversion helpers in `common/LimiterSharedState.h`**

Insert after `MilliDbToLinearScaled`:

```cpp
inline LONG ClampBoostMilliDb(LONG value) {
    if (value < kMinBoostMilliDb) {
        return kMinBoostMilliDb;
    }
    if (value > kMaxBoostMilliDb) {
        return kMaxBoostMilliDb;
    }
    return value;
}

inline LONGLONG BoostMilliDbToLinearScaled(LONG milliDb) {
    const float db = static_cast<float>(ClampBoostMilliDb(milliDb)) / 1000.0f;
    const double linear = std::pow(10.0, static_cast<double>(db) / 20.0);
    const double scaled = linear * static_cast<double>(kLinearScale);
    if (scaled < static_cast<double>(kLinearScale)) {
        return kLinearScale;
    }
    if (scaled > static_cast<double>(kMaxBoostLinearScaled)) {
        return kMaxBoostLinearScaled;
    }
    return static_cast<LONGLONG>(scaled + 0.5);
}

inline float BoostScaledLinearToFloat(LONGLONG value) {
    if (value < kLinearScale) {
        value = kLinearScale;
    }
    if (value > kMaxBoostLinearScaled) {
        value = kMaxBoostLinearScaled;
    }
    return static_cast<float>(static_cast<double>(value) / static_cast<double>(kLinearScale));
}
```

- [ ] **Step 3: Initialize booster fields in `InitializeStateFields`**

Insert after the `sampleRate` initialization:

```cpp
InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), 0);
InterlockedExchange(const_cast<volatile LONG*>(&state->boostMilliDb), kDefaultBoostMilliDb);
InterlockedExchange64(const_cast<volatile LONGLONG*>(&state->boostLinearScaled),
    BoostMilliDbToLinearScaled(kDefaultBoostMilliDb));
```

Keep the `_reserved` loop unchanged after the new fields.

- [ ] **Step 4: Add `SetBoostMilliDb` in `common/LimiterSharedState.h`**

Insert after `SetCeilingMilliDb`:

```cpp
inline void SetBoostMilliDb(LimiterSharedState* state, LONG milliDb) {
    if (state == nullptr) {
        return;
    }

    const LONG clamped = ClampBoostMilliDb(milliDb);
    InterlockedExchange(const_cast<volatile LONG*>(&state->boostMilliDb), clamped);
    InterlockedExchange64(const_cast<volatile LONGLONG*>(&state->boostLinearScaled),
        BoostMilliDbToLinearScaled(clamped));
}
```

- [ ] **Step 5: Extend `LimiterSettings` in `tools/LimiterTray/LimiterSettings.h`**

Replace the struct:

```cpp
struct LimiterSettings {
    bool enabled = true;
    LONG ceilingMilliDb = kDefaultCeilingMilliDb;
};
```

with:

```cpp
struct LimiterSettings {
    bool enabled = true;
    LONG ceilingMilliDb = kDefaultCeilingMilliDb;
    bool boostEnabled = false;
    LONG boostMilliDb = kDefaultBoostMilliDb;
};
```

- [ ] **Step 6: Apply and read booster settings**

In `ApplySettings`, insert after `SetCeilingMilliDb(state, settings.ceilingMilliDb);`:

```cpp
InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), settings.boostEnabled ? 1 : 0);
SetBoostMilliDb(state, settings.boostMilliDb);
```

In `ReadSettingsFromState`, insert before `return settings;`:

```cpp
settings.boostEnabled = state->boostEnabled != 0;
settings.boostMilliDb = ClampBoostMilliDb(state->boostMilliDb);
```

- [ ] **Step 7: Run helper tests**

Run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

Expected: build succeeds and executable prints `LimiterAudioTests passed`.

- [ ] **Step 8: Commit shared state and settings helpers**

```powershell
git add common/LimiterSharedState.h tools/LimiterTray/LimiterSettings.h
git commit -m "feat: add soundbooster shared settings"
```

## Task 3: Update LimiterStateProbe For Booster Fields

**Files:**
- Modify: `tools/LimiterStateProbe/main.cpp`
- Test: build `LimiterStateProbe`

- [ ] **Step 1: Print booster fields**

In `PrintState`, add inside the `if (state->version >= 2)` block, after `sampleRate`:

```cpp
if (state->version >= 3) {
    std::wcout << L"boostEnabled=" << state->boostEnabled << L"\n";
    std::wcout << L"boostMilliDb=" << state->boostMilliDb << L"\n";
    std::wcout << L"boostLinearScaled=" << state->boostLinearScaled << L"\n";
}
```

- [ ] **Step 2: Add booster command-line arguments**

In the argument loop, add after the `--disable` branch:

```cpp
} else if (arg == L"--boost-enable") {
    InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), 1);
} else if (arg == L"--boost-disable") {
    InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), 0);
} else if (arg == L"--boost-millidb") {
    if (i + 1 >= argc) {
        std::wcerr << L"--boost-millidb requires a value.\n";
        return 2;
    }
    LONG value = 0;
    if (!TryParseLong(argv[++i], &value)) {
        std::wcerr << L"Invalid boost value.\n";
        return 2;
    }
    CodexLimiter::SetBoostMilliDb(state, value);
```

The order must remain deterministic: `--enable`, `--disable`, booster arguments, then `--ceiling-millidb`.

- [ ] **Step 3: Build the diagnostic probe**

Run:

```powershell
cmake --build build --config Debug --target LimiterStateProbe
```

Expected: target builds successfully.

- [ ] **Step 4: Commit probe support**

```powershell
git add tools/LimiterStateProbe/main.cpp
git commit -m "feat: expose soundbooster in state probe"
```

## Task 4: Persist Booster Settings In The Tray App

**Files:**
- Modify: `tools/LimiterTray/main.cpp`
- Test: manual code review plus final app build

- [ ] **Step 1: Parse booster settings in `LoadSettings`**

In `LoadSettings`, add cases after the existing `ceilingMilliDb` branch:

```cpp
} else if (key == "boostEnabled" && TryParseLong(value, &parsed)) {
    settings.boostEnabled = parsed != 0;
} else if (key == "boostMilliDb" && TryParseLong(value, &parsed)) {
    settings.boostMilliDb = CodexLimiter::ClampBoostMilliDb(parsed);
```

The final branch sequence must be:

```cpp
if (key == "enabled" && TryParseLong(value, &parsed)) {
    settings.enabled = parsed != 0;
} else if (key == "ceilingMilliDb" && TryParseLong(value, &parsed)) {
    settings.ceilingMilliDb = CodexLimiter::ClampMilliDb(parsed);
} else if (key == "boostEnabled" && TryParseLong(value, &parsed)) {
    settings.boostEnabled = parsed != 0;
} else if (key == "boostMilliDb" && TryParseLong(value, &parsed)) {
    settings.boostMilliDb = CodexLimiter::ClampBoostMilliDb(parsed);
}
```

- [ ] **Step 2: Save booster settings in `SaveSettings`**

Add after:

```cpp
file << "ceilingMilliDb=" << settings.ceilingMilliDb << "\n";
```

Code to insert:

```cpp
file << "boostEnabled=" << (settings.boostEnabled ? 1 : 0) << "\n";
file << "boostMilliDb=" << settings.boostMilliDb << "\n";
```

- [ ] **Step 3: Build tray app to catch persistence compile errors**

Run:

```powershell
cmake --build build --config Debug --target LimiterTray
```

Expected: target builds successfully.

- [ ] **Step 4: Commit settings persistence**

```powershell
git add tools/LimiterTray/main.cpp
git commit -m "feat: persist soundbooster settings"
```

## Task 5: Add Soundbooster Tray Controls

**Files:**
- Modify: `tools/LimiterTray/AudioPowerPolicy.h`
- Modify: `tools/LimiterTray/main.cpp`
- Test: `tools/LimiterAudioTests/main.cpp`

- [ ] **Step 1: Increase UI height policy**

In `tools/LimiterTray/AudioPowerPolicy.h`, update `UiWindowHeightPx()` so it returns at least `320`.

Use this exact body:

```cpp
inline int UiWindowHeightPx() {
    return 320;
}
```

- [ ] **Step 2: Add booster control IDs and globals**

In `tools/LimiterTray/main.cpp`, add IDs after `kIdStatusText`:

```cpp
constexpr int kIdBoostEnable = 1007;
constexpr int kIdBoostSlider = 1008;
constexpr int kIdBoostText = 1009;
```

Add globals after `g_slider`:

```cpp
HWND g_boostEnable = nullptr;
HWND g_boostSlider = nullptr;
HWND g_boostText = nullptr;
```

- [ ] **Step 3: Add boost formatting and slider conversion helpers**

Insert after `FormatDb`:

```cpp
std::wstring FormatBoostDb(LONG milliDb) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"+%.0f dB", static_cast<double>(CodexLimiter::ClampBoostMilliDb(milliDb)) / 1000.0);
    return buffer;
}
```

Insert after `SliderPosToMilliDb`:

```cpp
int BoostMilliDbToSliderPos(LONG milliDb) {
    return static_cast<int>(CodexLimiter::ClampBoostMilliDb(milliDb) / 1000);
}

LONG BoostSliderPosToMilliDb(int position) {
    return CodexLimiter::ClampBoostMilliDb(position * 1000);
}
```

- [ ] **Step 4: Add boost adjustment helper**

Insert after `AdjustCeilingByMilliDb`:

```cpp
void AdjustBoostByMilliDb(LONG deltaMilliDb) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    LONG oldVal, newVal;
    do {
        oldVal = InterlockedCompareExchange(
            const_cast<volatile LONG*>(&g_state->boostMilliDb), 0, 0);
        newVal = CodexLimiter::ClampBoostMilliDb(oldVal + deltaMilliDb);
    } while (InterlockedCompareExchange(
        const_cast<volatile LONG*>(&g_state->boostMilliDb), newVal, oldVal) != oldVal);
    InterlockedExchange64(const_cast<volatile LONGLONG*>(&g_state->boostLinearScaled),
        CodexLimiter::BoostMilliDbToLinearScaled(newVal));
    SaveSettings();
    UpdateControls();
}
```

- [ ] **Step 5: Update keyboard routing**

In `HandleWindowKey`, replace the left/right block with focus-aware handling:

```cpp
const HWND focus = GetFocus();
if (message->wParam == VK_LEFT) {
    if (focus == g_boostSlider) {
        AdjustBoostByMilliDb(-1000);
    } else {
        AdjustCeilingByMilliDb(-1000);
    }
    return true;
}
if (message->wParam == VK_RIGHT) {
    if (focus == g_boostSlider) {
        AdjustBoostByMilliDb(1000);
    } else {
        AdjustCeilingByMilliDb(1000);
    }
    return true;
}
```

- [ ] **Step 6: Add boost slider mouse setter**

Insert after `SetCeilingFromSliderX`:

```cpp
int BoostSliderXToPosition(HWND window, int x) {
    RECT rect{};
    GetClientRect(window, &rect);
    const int left = 12;
    const int right = std::max(left + 1, static_cast<int>(rect.right) - 12);
    x = std::max(left, std::min(right, x));
    const double normalized = static_cast<double>(x - left) / static_cast<double>(right - left);
    return static_cast<int>(normalized * 24.0 + 0.5);
}

void SetBoostFromSliderX(HWND window, int x) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    CodexLimiter::SetBoostMilliDb(g_state, BoostSliderPosToMilliDb(BoostSliderXToPosition(window, x)));
    SaveSettings();
    UpdateControls();
}
```

- [ ] **Step 7: Generalize slider paint for ceiling and boost**

In `SliderProc`, before the `WM_PAINT` body computes `ceiling`, add:

```cpp
const bool isBoostSlider = window == g_boostSlider;
```

Replace:

```cpp
LONG ceiling = g_state != nullptr ? g_state->ceilingMilliDb : CodexLimiter::kDefaultCeilingMilliDb;
const int position = MilliDbToSliderPos(ceiling);
const int knobX = left + static_cast<int>((static_cast<double>(position) / 75.0) * (right - left));
```

with:

```cpp
const int maxPosition = isBoostSlider ? 24 : 75;
const int position = isBoostSlider
    ? BoostMilliDbToSliderPos(g_state != nullptr ? g_state->boostMilliDb : CodexLimiter::kDefaultBoostMilliDb)
    : MilliDbToSliderPos(g_state != nullptr ? g_state->ceilingMilliDb : CodexLimiter::kDefaultCeilingMilliDb);
const int knobX = left + static_cast<int>((static_cast<double>(position) / static_cast<double>(maxPosition)) * (right - left));
```

Replace:

```cpp
for (int i = 0; i <= 75; i += 5) {
    const int x = left + static_cast<int>((static_cast<double>(i) / 75.0) * (right - left));
```

with:

```cpp
for (int i = 0; i <= maxPosition; i += (isBoostSlider ? 4 : 5)) {
    const int x = left + static_cast<int>((static_cast<double>(i) / static_cast<double>(maxPosition)) * (right - left));
```

- [ ] **Step 8: Route boost slider mouse and keyboard events**

At the top of each mouse case in `SliderProc`, use the boost setter when `window == g_boostSlider`.

For `WM_LBUTTONDOWN`, replace the setter call with:

```cpp
if (window == g_boostSlider) {
    SetBoostFromSliderX(window, GET_X_LPARAM(lParam));
} else {
    SetCeilingFromSliderX(window, GET_X_LPARAM(lParam));
}
```

Use the same conditional replacement in `WM_MOUSEMOVE` and `WM_LBUTTONUP`.

In `WM_KEYDOWN`, replace the left/right calls with:

```cpp
if (wParam == VK_LEFT) {
    if (window == g_boostSlider) {
        AdjustBoostByMilliDb(-1000);
    } else {
        AdjustCeilingByMilliDb(-1000);
    }
    return 0;
}
if (wParam == VK_RIGHT) {
    if (window == g_boostSlider) {
        AdjustBoostByMilliDb(1000);
    } else {
        AdjustCeilingByMilliDb(1000);
    }
    return 0;
}
```

- [ ] **Step 9: Create booster controls**

In `CreateControls`, replace the controls after `g_slider` creation with this layout:

```cpp
g_enable = CreateWindowExW(0, L"BUTTON", L"Limiter enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
    16, 128, 160, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEnable)), g_instance, nullptr);

g_boostEnable = CreateWindowExW(0, L"BUTTON", L"Soundbooster enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
    16, 160, 190, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostEnable)), g_instance, nullptr);
g_boostText = CreateWindowExW(0, L"STATIC", L"Boost: +0 dB", WS_CHILD | WS_VISIBLE,
    16, 192, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostText)), g_instance, nullptr);
g_boostSlider = CreateWindowExW(0, L"CodexLimiterSlider", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
    16, 216, 310, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostSlider)), g_instance, nullptr);

g_statusText = CreateWindowExW(0, L"STATIC", L"Status: starting", WS_CHILD | WS_VISIBLE,
    16, 260, 320, 40, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusText)), g_instance, nullptr);
```

- [ ] **Step 10: Synchronize controls in `UpdateControls`**

After the existing ceiling text line:

```cpp
const std::wstring ceilingText = L"Ceiling: " + FormatDb(ceiling);
```

add:

```cpp
const std::wstring boostText = L"Boost: " + FormatBoostDb(g_state->boostMilliDb);
```

After `SetWindowTextW(g_ceilingText, ceilingText.c_str());`, add:

```cpp
SetWindowTextW(g_boostText, boostText.c_str());
```

After the limiter checkbox sync:

```cpp
SendMessageW(g_enable, BM_SETCHECK, g_state->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
```

add:

```cpp
SendMessageW(g_boostEnable, BM_SETCHECK, g_state->boostEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
EnableWindow(g_boostSlider, g_state->boostEnabled != 0);
```

After `InvalidateRect(g_slider, nullptr, FALSE);`, add:

```cpp
InvalidateRect(g_boostSlider, nullptr, FALSE);
```

- [ ] **Step 11: Handle booster checkbox commands**

In `WM_COMMAND`, add after the limiter checkbox branch:

```cpp
if (LOWORD(wParam) == kIdBoostEnable && g_state != nullptr) {
    const bool checked = SendMessageW(g_boostEnable, BM_GETCHECK, 0, 0) == BST_CHECKED;
    InterlockedExchange(const_cast<volatile LONG*>(&g_state->boostEnabled), checked ? 1 : 0);
    SaveSettings();
    UpdateControls();
    return 0;
}
```

- [ ] **Step 12: Build tests and tray**

Run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
cmake --build build --config Debug --target LimiterTray
```

Expected: tests pass and Debug tray target builds.

- [ ] **Step 13: Commit UI controls**

```powershell
git add tools/LimiterTray/AudioPowerPolicy.h tools/LimiterTray/main.cpp tools/LimiterAudioTests/main.cpp
git commit -m "feat: add soundbooster tray controls"
```

## Task 6: Apply Booster Gain Before Limiter

**Files:**
- Modify: `tools/LimiterTray/LoopbackAudioEngine.h`
- Modify: `tools/LimiterTray/LoopbackAudioEngine.cpp`
- Test: build targets

- [ ] **Step 1: Rename the private DSP method**

In `tools/LimiterTray/LoopbackAudioEngine.h`, replace:

```cpp
void ApplyLimiter(float* frames, UINT32 frameCount, UINT32 channels);
```

with:

```cpp
void ProcessAudio(float* frames, UINT32 frameCount, UINT32 channels);
```

In `tools/LimiterTray/LoopbackAudioEngine.cpp`, replace the call in `ProcessCaptureBuffer`:

```cpp
ApplyLimiter(reinterpret_cast<float*>(data), framesAvailable, channels);
```

with:

```cpp
ProcessAudio(reinterpret_cast<float*>(data), framesAvailable, channels);
```

Rename the function definition:

```cpp
void LoopbackAudioEngine::ApplyLimiter(float* frames, UINT32 frameCount, UINT32 channels) {
```

to:

```cpp
void LoopbackAudioEngine::ProcessAudio(float* frames, UINT32 frameCount, UINT32 channels) {
```

- [ ] **Step 2: Replace the processing method body**

Replace the full renamed `ProcessAudio` body with:

```cpp
void LoopbackAudioEngine::ProcessAudio(float* frames, UINT32 frameCount, UINT32 channels) {
    const bool limiterEnabled = (sharedState_ == nullptr) || (sharedState_->enabled != 0);
    const bool boostEnabled = (sharedState_ != nullptr) && (sharedState_->boostEnabled != 0);
    const float boostGain = boostEnabled
        ? BoostScaledLinearToFloat(sharedState_->boostLinearScaled)
        : 1.0f;

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;

    const float ceiling = (sharedState_ != nullptr)
        ? ScaledLinearToFloat(sharedState_->ceilingLinearScaled)
        : ScaledLinearToFloat(kDefaultLinearScaled);

    if (!limiterEnabled) {
        limiterGain_ = 1.0f;
    }

    constexpr float kReleasePerFrame = 0.00008f;
    float gain = limiterGain_;

    for (UINT32 frame = 0; frame < frameCount; ++frame) {
        const UINT32 offset = frame * channels;
        float boostedPeak = 0.0f;

        for (UINT32 ch = 0; ch < channels; ++ch) {
            const float original = fabsf(frames[offset + ch]);
            if (original > inputPeak) {
                inputPeak = original;
            }

            frames[offset + ch] *= boostGain;
            const float boosted = fabsf(frames[offset + ch]);
            if (boosted > boostedPeak) {
                boostedPeak = boosted;
            }
        }

        if (limiterEnabled) {
            const float targetGain = (boostedPeak > ceiling) ? (ceiling / boostedPeak) : 1.0f;
            if (targetGain < gain) {
                gain = targetGain;
            } else if (gain < 1.0f) {
                gain += kReleasePerFrame;
                if (gain > 1.0f) {
                    gain = 1.0f;
                }
            }
        } else {
            gain = 1.0f;
        }

        for (UINT32 ch = 0; ch < channels; ++ch) {
            frames[offset + ch] *= gain;
            const float processed = fabsf(frames[offset + ch]);
            if (processed > outputPeak) {
                outputPeak = processed;
            }
        }
    }

    limiterGain_ = limiterEnabled ? gain : 1.0f;

    if (sharedState_ != nullptr) {
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->inputPeakMilliDb),
            LinearToMilliDb(inputPeak));
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->outputPeakMilliDb),
            LinearToMilliDb(outputPeak));
        InterlockedIncrement(const_cast<volatile LONG*>(&sharedState_->processCounter));
    }
}
```

- [ ] **Step 3: Build tests and tray**

Run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
cmake --build build --config Debug --target LimiterTray
```

Expected: tests pass and Debug tray target builds.

- [ ] **Step 4: Commit audio processing**

```powershell
git add tools/LimiterTray/LoopbackAudioEngine.h tools/LimiterTray/LoopbackAudioEngine.cpp
git commit -m "feat: apply soundbooster before limiter"
```

## Task 7: Final Verification

**Files:**
- No source edits are expected during this task. If verification fails, make the smallest targeted source fix and rerun the failed command.
- Test: full required checks for helper, tray, and release build.

- [ ] **Step 1: Check working tree**

Run:

```powershell
git status --short
```

Expected: no unstaged source changes except intentional final fixes.

- [ ] **Step 2: Run native helper tests**

Run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
cmake --build build --config Debug --target LimiterStateProbe
```

Expected: builds succeed and `LimiterAudioTests.exe` prints `LimiterAudioTests passed`.

- [ ] **Step 3: Build Release tray app**

Run:

```powershell
cmake --build build --config Release --target LimiterTray
```

Expected: Release `LimiterTray` target builds successfully.

- [ ] **Step 4: Manual behavior note**

Do not run install or uninstall scripts for verification. If manual app testing is performed from the built executable, document whether these scenarios were checked:

```text
Limiter -20 dBFS + Soundbooster +10 dB: output remains limited near -20 dBFS.
Soundbooster enabled + limiter disabled: boosted audio is not limiter-protected.
Older settings file without boost keys: booster defaults off and +0 dB.
```

- [ ] **Step 5: Commit final fixes if needed**

If verification required edits, commit them:

```powershell
git add common/LimiterSharedState.h tools/LimiterTray tools/LimiterAudioTests/main.cpp tools/LimiterStateProbe/main.cpp
git commit -m "fix: finish soundbooster verification"
```

If no edits were required, do not create an empty commit.

## Self-Review

- Spec coverage: shared state, settings, UI, independent limiter/booster behavior, pre-limiter gain, tray menu scope, tests, and verification are each covered by a task.
- Red-flag scan: no deferred implementation wording is intentionally left in this plan.
- Type consistency: shared state uses `LONG` for flags/millidecibels and `LONGLONG` for boost scaled gain because `+24 dB` exceeds the existing `LONG` scaled range.

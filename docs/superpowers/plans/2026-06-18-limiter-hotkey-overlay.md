# Limiter Hotkey Overlay Implementation Plan

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add configurable global limiter and soundbooster controls with a transient Windows-volume-style overlay.

**Architecture:** Put deterministic hotkey and startup policy in tray helper headers so native tests can cover the contracts. Keep Win32 registration, message handling, settings persistence, startup registry writes, and overlay drawing in `tools/LimiterTray/main.cpp`, following the existing tray UI globals and window procedures.

**Tech Stack:** C++17, Win32 `RegisterHotKey`, layered/topmost windows, existing CMake targets and `LimiterAudioTests`.

---

### Task 1: Hotkey Policy Header

**Files:**
- Create: `tools/LimiterTray/HotkeyPolicy.h`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] Add a failing `LimiterAudioTests` expectation that `Ctrl+Alt+A`, `Ctrl+Alt+D`, `Ctrl+Alt+S`, `Ctrl+Alt+F6`, and `Ctrl+Alt+F7` are the declared default hotkeys.
- [ ] Run `cmake --build build --config Debug --target LimiterAudioTests` and `.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe`; the compile should fail until `HotkeyPolicy.h` exists.
- [ ] Add `HotkeyPolicy.h` with hotkey IDs, modifier policy, virtual keys, duplicate detection, display names, and 1 dB step.
- [ ] Re-run `LimiterAudioTests` and confirm the new expectations pass.

### Task 2: Register And Handle Hotkeys

**Files:**
- Modify: `tools/LimiterTray/main.cpp`

- [ ] Include `HotkeyPolicy.h`.
- [ ] Add `RegisterLimiterHotkeys`, `UnregisterLimiterHotkeys`, and `HandleLimiterHotkey`.
- [ ] Route `WM_HOTKEY` in `MainWindowProc`.
- [ ] On limiter hotkeys, adjust ceiling or toggle `g_state->enabled`. On boost hotkeys, adjust boost gain. Save settings and refresh controls after each action.
- [ ] Build `LimiterTray` Debug to catch Win32 compile errors.

### Task 3: Transient Overlay

**Files:**
- Modify: `tools/LimiterTray/main.cpp`

- [ ] Add a `CodexLimiterOverlay` window class and `OverlayProc`.
- [ ] Create a layered, topmost, no-activate overlay window at startup.
- [ ] Paint label, current limiter state, ceiling text, and a level bar from `g_state`.
- [ ] Show overlay on each handled hotkey and hide it after a timer.
- [ ] Release overlay GDI resources on shutdown.

### Task 4: Settings, Startup, And Device Recovery

**Files:**
- Create: `tools/LimiterTray/StartupPolicy.h`
- Modify: `tools/LimiterTray/main.cpp`
- Modify: `tools/LimiterAudioTests/main.cpp`

- [ ] Persist selected tab, overlay enabled state, startup state, and custom hotkey keys in the existing settings file.
- [ ] Add settings tabs for limiter, hotkeys, startup, and power information.
- [ ] Reject duplicate custom hotkey assignments before saving or registering them.
- [ ] Toggle the HKCU `CodexLimiter` Run entry from the Startup tab.
- [ ] Treat output removal or disable notifications as a retarget signal so the engine can fall back to another active physical output.

### Task 5: Verification

**Files:**
- No source edits expected.

- [ ] Run `cmake --build build --config Debug --target LimiterAudioTests`.
- [ ] Run `.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe`.
- [ ] Run `cmake --build build --config Release --target LimiterTray`.
- [ ] Report that manual hotkey, overlay, startup registry, and device unplug behavior need a running desktop session to validate visually.

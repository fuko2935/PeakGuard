# Limiter Tray Guide

Purpose: Windows tray UI and WASAPI loopback limiter engine.

Technology: C++17, Win32, WASAPI, COM, WRL, AVRT, resource scripts.

Parent context: See `../../AGENTS.md` and `../AGENTS.md`.

## Package Rules

- **MUST** preserve the VB-CABLE loopback model unless the task explicitly changes product architecture.
- **MUST** keep UI state and shared state synchronized through `PeakGuardSharedState`.
- **MUST** avoid blocking the audio thread with UI work, file I/O, or long device enumeration.
- **MUST** use interlocked writes for shared-state fields.
- **MUST** update `tools/PeakGuardAudioTests/main.cpp` when changing header-only helpers.
- **MUST** keep Win32 resources in sync with `resource.h`, `PeakGuardTray.rc`, and `app.ico`.
- **SHOULD** isolate pure policy decisions in small headers so tests can cover them without running audio hardware.

## Setup & Run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target PeakGuardTray
cmake --build build --config Release --target PeakGuardTray
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

## Patterns & Conventions

- DO: Follow UI message handling in `main.cpp` for tray icon, custom controls, timers, and window messages.
- DO: Follow audio lifecycle boundaries in `LoopbackAudioEngine.h` and `LoopbackAudioEngine.cpp`.
- DO: Keep endpoint-name policy in `AudioEndpointSelection.h`; tests already exercise these helpers.
- DO: Keep stream flags, wait interval, and UI timing policy in `AudioPowerPolicy.h`.
- DO: Keep hotkey and startup registry policy in `HotkeyPolicy.h` and `StartupPolicy.h`; tests should cover these contracts.
- DO: Keep persisted settings translation in `LimiterSettings.h` and file I/O in `main.cpp`.
- MUST NOT: Use microphone capture for the limiter path; `PeakGuardAudioTests/main.cpp` expects render loopback flags.
- MUST NOT: Write settings or call shell APIs from `LoopbackAudioEngine::AudioThreadProc`.

## Audio / Backend Patterns

- Audio clients are initialized in `LoopbackAudioEngine::InitializeAudioClients`.
- Device-change recovery is coordinated through `OnDefaultDeviceChanged`, `deviceChangedEvent_`, and the audio thread loop.
- DSP lives in `LoopbackAudioEngine::ProcessAudio`.
- Shared meter and engine state updates use `InterlockedExchange` and `InterlockedIncrement`.
- The audio thread enables AVRT priority and SSE FTZ/DAZ handling before processing.

## UI Patterns

- Main UI globals and Win32 procedures live in `main.cpp`.
- Custom meter and slider controls use `MeterProc` and `SliderProc`.
- Tray behavior uses `Shell_NotifyIconW`, `kTrayMessage`, and `ShowContextMenu`.
- Settings are stored under `%LOCALAPPDATA%\\PeakGuard\\settings.ini`.

## Touch Points / Key Files

- App entry and UI: `main.cpp`.
- Audio engine API: `LoopbackAudioEngine.h`.
- Audio routing and limiter: `LoopbackAudioEngine.cpp`.
- Endpoint selection helpers: `AudioEndpointSelection.h`.
- Audio timing and stream policy: `AudioPowerPolicy.h`.
- Hotkey policy: `HotkeyPolicy.h`.
- Settings helpers: `LimiterSettings.h`.
- Startup policy: `StartupPolicy.h`.
- Resources: `PeakGuardTray.rc`, `resource.h`, `app.ico`.
- Build target: `CMakeLists.txt`.

## JIT Index Hints

```powershell
rg -n "ProcessAudio|InitializeAudioClients|ProcessCaptureBuffer|AudioThreadProc" .
rg -n "Shell_NotifyIcon|WM_|WNDCLASSW|CreateWindowExW|SliderProc|MeterProc" .
rg -n "IsVirtualAudioEndpointName|CaptureStreamFlags|ApplySettings|LimiterHotkey|StartupRegistry" .
rg -n "Interlocked|sharedState_|g_state" .
```

## Common Gotchas

- The app may change the default render endpoint to the virtual cable and restore a remembered physical endpoint on shutdown.
- Silence detection changes `AudioEngineState` after consecutive silent buffers.
- `captureFormat_` is allocated by COM and must be released with `CoTaskMemFree`.
- The installer expects the Release binary at `build/PeakGuardTray/Release/PeakGuardTray.exe` when it builds the tray project directly.

## Pre-PR Checks

```powershell
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
cmake --build build --config Release --target PeakGuardTray
```

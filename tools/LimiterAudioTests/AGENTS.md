# Limiter Audio Tests Guide

Purpose: native regression tests for limiter helpers and audio policy decisions.

Technology: C++17 console executable built by CMake.

Parent context: See `../../AGENTS.md` and `../AGENTS.md`.

## Package Rules

- **MUST** keep tests deterministic and independent of live audio hardware.
- **MUST** add expectations when changing pure helper behavior in `../LimiterTray/AudioEndpointSelection.h`, `../LimiterTray/AudioPowerPolicy.h`, `../LimiterTray/HotkeyPolicy.h`, `../LimiterTray/LimiterSettings.h`, `../LimiterTray/StartupPolicy.h`, or `../../common/LimiterSharedState.h`.
- **MUST** return nonzero on failure and print useful failure names.
- **SHOULD** keep the lightweight `Expect` style unless a real test framework is introduced across the repo.

## Setup & Run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

## Patterns & Conventions

- DO: Add focused `Expect(condition, "clear behavior name")` checks in `main.cpp`.
- DO: Use local fixtures like `MakeWaveFormat` when testing format helpers.
- DO: Include tray helper headers directly when they contain pure logic.
- MUST NOT: Require VB-CABLE, active endpoints, timers, process state, or user interaction in this test executable.

## Touch Points / Key Files

- Test executable: `main.cpp`.
- Build target: `CMakeLists.txt`.
- Endpoint helper under test: `../LimiterTray/AudioEndpointSelection.h`.
- Power policy under test: `../LimiterTray/AudioPowerPolicy.h`.
- Hotkey policy under test: `../LimiterTray/HotkeyPolicy.h`.
- Settings helper under test: `../LimiterTray/LimiterSettings.h`.
- Startup policy under test: `../LimiterTray/StartupPolicy.h`.
- Shared state helper under test: `../../common/LimiterSharedState.h`.

## JIT Index Hints

```powershell
rg -n "Expect\\(" .
rg -n "MakeWaveFormat|LimiterSettings|InitializeStateFields|LimiterHotkey|StartupRegistry" .
rg -n "IsVirtualAudioEndpointName|IsFloatPcmFormat|CaptureStreamFlags" .
```

## Common Gotchas

- The tests currently validate helper behavior, not live WASAPI streaming.
- New tests should be clear enough that the failure name explains the broken contract.

## Pre-PR Checks

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

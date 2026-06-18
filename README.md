# Codex Limiter

Codex Limiter is a Windows tray app that routes system audio through VB-CABLE, applies a simple limiter in a WASAPI loopback path, and renders the processed audio to a physical output device.

The old feasibility branch is retired. Active development lives on `main`.

## Layout

- `common/`: shared limiter state headers.
- `tools/LimiterTray/`: tray UI and loopback audio engine.
- `tools/LimiterAudioTests/`: native regression tests.
- `tools/LimiterStateProbe/`: shared-state inspection helper.
- `tools/EndpointProbe/`: endpoint diagnostic helper.
- `scripts/`: install and uninstall helpers.

## Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Release --target LimiterTray
```

If `cmake` is not on `PATH`, use the Visual Studio bundled CMake executable.

## Test

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
```

## Install

```powershell
.\scripts\Install-CodexLimiter.ps1 -Configuration Release
```

## Uninstall

```powershell
.\scripts\Uninstall-CodexLimiter.ps1
```

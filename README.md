# PeakGuard

PeakGuard is a lightweight Windows tray app that routes system audio through VB-CABLE, applies a WASAPI loopback limiter and soundbooster, and renders the processed audio to a physical output device.

![PeakGuard Screenshot](assets/screenshot.png)

## Installation (For Users)

Open PowerShell from the repository root and run:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release
```

PeakGuard installs to `%LOCALAPPDATA%\PeakGuard`, creates a Start Menu shortcut, and launches the tray app.

Note: PeakGuard uses VB-CABLE to route audio. The installer script will automatically download the official VB-CABLE driver package and prompt you to install it if you do not have it.

To also start PeakGuard with Windows:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release -EnableStartup
```

## Uninstall

```powershell
.\scripts\Uninstall-PeakGuard.ps1
```

## Developer Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Release --target PeakGuardTray
```

## Tests

```powershell
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

## Repository Layout

- `common/`: shared state ABI and helpers.
- `tools/PeakGuardTray/`: tray UI and WASAPI loopback audio engine.
- `tools/PeakGuardAudioTests/`: native deterministic regression tests.
- `tools/PeakGuardStateProbe/`: shared-state inspection helper.
- `tools/EndpointProbe/`: endpoint diagnostic helper.
- `scripts/`: install and uninstall helpers.
- `docs/superpowers/`: historical specs and implementation plans.

## Release Hygiene

Build outputs, binaries, archives, logs, ETL traces, local worktrees, and snapshots are ignored and should not be committed.

<p align="center">
  <img src="assets/peakguard-logo.svg" alt="PeakGuard logo" width="160">
</p>

<h1 align="center">PeakGuard</h1>

<p align="center">
  A lightweight Windows tray app for peak limiting, safe boosting, and VB-CABLE based system audio routing.
</p>

<p align="center">
  <a href="https://github.com/fuko2935/PeakGuard/actions/workflows/windows-ci.yml"><img alt="Windows CI" src="https://github.com/fuko2935/PeakGuard/actions/workflows/windows-ci.yml/badge.svg"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows-0078D4">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-17-00599C">
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-MIT-green"></a>
</p>

PeakGuard routes Windows system audio through VB-CABLE, captures it with WASAPI loopback, applies a limiter and soundbooster, then renders the processed signal to a physical output device.

## Features

- Tray-first Windows app with low-friction background operation.
- WASAPI loopback processing path for system-wide audio.
- Limiter and soundbooster controls designed to prevent runaway peaks.
- VB-CABLE routing support with installer-assisted driver setup.
- Shared-state probe and native regression tests for deterministic helper behavior.

## Install

Open PowerShell from the repository root:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release
```

The installer copies PeakGuard to `%LOCALAPPDATA%\PeakGuard`, creates a Start Menu shortcut, and launches the tray app.

PeakGuard depends on VB-CABLE. If VB-CABLE is missing, the installer downloads the official VB-CABLE package and prompts for the driver installation.

To start PeakGuard with Windows:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release -EnableStartup
```

## Uninstall

```powershell
.\scripts\Uninstall-PeakGuard.ps1
```

## Build

Requirements:

- Windows 10 or later
- Visual Studio 2022 Build Tools with MSVC
- CMake 3.20 or later

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Release --target PeakGuardTray
```

## Test

```powershell
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

## Repository Layout

| Path | Purpose |
| --- | --- |
| `common/` | Shared state ABI and helpers |
| `tools/PeakGuardTray/` | Tray UI and WASAPI loopback audio engine |
| `tools/PeakGuardAudioTests/` | Native deterministic regression tests |
| `tools/PeakGuardStateProbe/` | Shared-state inspection helper |
| `tools/EndpointProbe/` | Audio endpoint diagnostic helper |
| `scripts/` | Install and uninstall helpers |
| `docs/superpowers/` | Historical specs and implementation plans |

## Release Hygiene

Build outputs, binaries, archives, logs, ETL traces, local worktrees, and snapshots are ignored and should not be committed.

## License

PeakGuard is released under the [MIT License](LICENSE).

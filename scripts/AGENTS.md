# Scripts Guide

Purpose: user-facing PowerShell install and uninstall helpers.

Technology: PowerShell, CMake invocation, Windows shell shortcuts, HKCU Run key.

Parent context: See `../AGENTS.md`.

## Package Rules

- **MUST** keep scripts runnable from the repository root.
- **MUST** preserve `$ErrorActionPreference = 'Stop'` for fail-fast behavior.
- **MUST** treat process stops, startup entries, shortcut creation, and local app data deletion as user-machine mutations.
- **MUST** update `README.md` when install or uninstall commands change.
- **MUST NOT** add machine-specific absolute paths except guarded discovery paths such as Visual Studio `vswhere`.
- **SHOULD** keep script parameters explicit and validated.

## Setup & Run

```powershell
# Build/install Release app, create shortcut, launch tray
.\scripts\Install-PeakGuard.ps1 -Configuration Release

# After rebuilding PeakGuardTray, rerun install to refresh the Start Menu/startup copy
.\scripts\Install-PeakGuard.ps1 -Configuration Release

# Also enable HKCU startup
.\scripts\Install-PeakGuard.ps1 -Configuration Release -EnableStartup

# Stop tray, remove startup entry, shortcut, and installed files
.\scripts\Uninstall-PeakGuard.ps1
```

## Patterns & Conventions

- DO: Follow CMake discovery and fallback style in `Install-PeakGuard.ps1`.
- DO: Use `Join-Path`, `Resolve-Path`, and `-LiteralPath` for filesystem operations.
- DO: Keep HKCU startup changes scoped to `PeakGuard`.
- DO: Keep install output under `%LOCALAPPDATA%\\PeakGuard`.
- MUST NOT: Delete repository files or recurse outside the intended install/start-menu directories.

## Touch Points / Key Files

- Installer: `Install-PeakGuard.ps1`.
- Uninstaller: `Uninstall-PeakGuard.ps1`.
- User-facing commands: `../README.md`.
- Tray executable target: `../tools/PeakGuardTray/CMakeLists.txt`.

## JIT Index Hints

```powershell
rg -n "PeakGuard|PeakGuardTray|Run|Start-Process|Stop-Process" .
rg -n "Remove-Item|Copy-Item|New-Item|New-ItemProperty" .
rg -n "cmake|vswhere|Configuration|EnableStartup" .
```

## Common Gotchas

- `Install-PeakGuard.ps1` stops existing `PeakGuardTray` processes before launching the installed copy.
- `Uninstall-PeakGuard.ps1` recursively removes `%LOCALAPPDATA%\\PeakGuard`.
- The Start Menu shortcut and HKCU startup entry run `%LOCALAPPDATA%\\PeakGuard\\PeakGuardTray.exe`, not the repo build output.
- Rerun `.\scripts\Install-PeakGuard.ps1 -Configuration Release` after Release tray changes so Start Menu/startup uses the updated binary.

## Pre-PR Checks

```powershell
cmake --build build --config Release --target PeakGuardTray
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Install-PeakGuard.ps1 -Configuration Release
```

Only run the install check when machine-level side effects are acceptable for the task.

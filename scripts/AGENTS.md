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
.\scripts\Install-CodexLimiter.ps1 -Configuration Release

# After rebuilding LimiterTray, rerun install to refresh the Start Menu/startup copy
.\scripts\Install-CodexLimiter.ps1 -Configuration Release

# Also enable HKCU startup
.\scripts\Install-CodexLimiter.ps1 -Configuration Release -EnableStartup

# Stop tray, remove startup entry, shortcut, and installed files
.\scripts\Uninstall-CodexLimiter.ps1
```

## Patterns & Conventions

- DO: Follow CMake discovery and fallback style in `Install-CodexLimiter.ps1`.
- DO: Use `Join-Path`, `Resolve-Path`, and `-LiteralPath` for filesystem operations.
- DO: Keep HKCU startup changes scoped to `CodexLimiter`.
- DO: Keep install output under `%LOCALAPPDATA%\\CodexLimiter`.
- MUST NOT: Delete repository files or recurse outside the intended install/start-menu directories.

## Touch Points / Key Files

- Installer: `Install-CodexLimiter.ps1`.
- Uninstaller: `Uninstall-CodexLimiter.ps1`.
- User-facing commands: `../README.md`.
- Tray executable target: `../tools/LimiterTray/CMakeLists.txt`.

## JIT Index Hints

```powershell
rg -n "CodexLimiter|LimiterTray|Run|Start-Process|Stop-Process" .
rg -n "Remove-Item|Copy-Item|New-Item|New-ItemProperty" .
rg -n "cmake|vswhere|Configuration|EnableStartup" .
```

## Common Gotchas

- `Install-CodexLimiter.ps1` stops existing `LimiterTray` processes before launching the installed copy.
- `Uninstall-CodexLimiter.ps1` recursively removes `%LOCALAPPDATA%\\CodexLimiter`.
- The Start Menu shortcut and HKCU startup entry run `%LOCALAPPDATA%\\CodexLimiter\\LimiterTray.exe`, not the repo build output.
- Rerun `.\scripts\Install-CodexLimiter.ps1 -Configuration Release` after Release tray changes so Start Menu/startup uses the updated binary.

## Pre-PR Checks

```powershell
cmake --build build --config Release --target LimiterTray
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Install-CodexLimiter.ps1 -Configuration Release
```

Only run the install check when machine-level side effects are acceptable for the task.

# PeakGuard Agent Guide

## Project Snapshot

PeakGuard is a simple single-project Windows C++ repo built with CMake and MSVC-compatible C++17.

- Product: Windows tray app that routes system audio through VB-CABLE, applies a WASAPI loopback limiter, and renders to a physical output device.
- Build system: CMake 3.20+ from the repository root.
- Primary source layout: `common/`, `tools/`, `scripts/`, and `docs/`.
- Tests: native executable in `tools/PeakGuardAudioTests/`.
- Subdirectories may contain their own `AGENTS.md`; follow the nearest file first.

## Root Commands

Run from the repository root unless noted.

```powershell
# Configure
cmake -S . -B build -A x64

# Build all Debug targets
cmake --build build --config Debug

# Build the Release tray app
cmake --build build --config Release --target PeakGuardTray

# Refresh the installed Start Menu/startup copy after Release changes
.\scripts\Install-PeakGuard.ps1 -Configuration Release

# Build and run native tests
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe

# Install/uninstall the app
.\scripts\Install-PeakGuard.ps1 -Configuration Release
.\scripts\Uninstall-PeakGuard.ps1
```

No dedicated lint or typecheck command is configured. Treat CMake configure, MSVC compile, and `PeakGuardAudioTests` as the required checks.

## Universal Rules

- **MUST** follow existing C++17, Win32, WASAPI, and CMake patterns before adding abstractions.
- **MUST** keep shared limiter state changes compatible with `common/PeakGuardSharedState.h`.
- **MUST** update or add `tools/PeakGuardAudioTests/` coverage when changing endpoint selection, limiter settings, audio policy constants, hotkey/startup policy helpers, or shared-state helpers.
- **MUST** run the relevant CMake build and test commands before claiming work is complete.
- **MUST** run `.\scripts\Install-PeakGuard.ps1 -Configuration Release` after Release tray changes when the user expects Start Menu or Windows startup to launch the updated app.
- **MUST NOT** commit build outputs, binaries, logs, ETL traces, snapshots, or local worktrees.
- **MUST NOT** run install/uninstall scripts for verification without considering their process, startup, shortcut, and local app data side effects.
- **SHOULD** keep changes focused and avoid unrelated refactors in the audio path.

## Security & Secrets

- **MUST NOT** commit secrets, credentials, tokens, private keys, or machine-specific audio endpoint IDs.
- `.gitignore` excludes `build/`, `out/`, binaries, logs, PDBs, ETL files, `snapshots/`, `.worktrees/`, and accidental `nul` files.
- Treat PowerShell scripts in `scripts/` as user-machine mutating operations.
- Avoid logging PII or stable local device identifiers in committed docs or tests.

## Project Structure

- Shared state: `common/` -> see `common/AGENTS.md`.
- Native tools: `tools/` -> see `tools/AGENTS.md`.
- Tray app and audio engine: `tools/PeakGuardTray/` -> see `tools/PeakGuardTray/AGENTS.md`.
- Native regression tests: `tools/PeakGuardAudioTests/` -> see `tools/PeakGuardAudioTests/AGENTS.md`.
- Install helpers: `scripts/` -> see `scripts/AGENTS.md`.
- Historical plans/specs: `docs/superpowers/`.

## JIT Index

```powershell
# Find source files
rg --files common tools scripts docs

# Find limiter state fields and helpers
rg -n "PeakGuardSharedState|SetCeilingMilliDb|AudioEngineState" common tools

# Find audio routing and WASAPI usage
rg -n "IAudioClient|IMMDevice|AUDCLNT|Loopback|VB-CABLE|Virtual" tools

# Find Win32 tray/UI handlers
rg -n "WNDCLASSW|WindowProc|Shell_NotifyIcon|WM_" tools/PeakGuardTray

# Find tests and expectations
rg -n "Expect\\(" tools/PeakGuardAudioTests

# Find CMake targets
rg -n "add_executable|target_link_libraries|add_subdirectory" CMakeLists.txt tools
```

## Testing Expectations

- Unit-style/native regression tests live in `tools/PeakGuardAudioTests/main.cpp`.
- Add test expectations for pure helper behavior in `AudioEndpointSelection.h`, `AudioPowerPolicy.h`, `HotkeyPolicy.h`, `LimiterSettings.h`, `StartupPolicy.h`, and `common/PeakGuardSharedState.h`.
- Manual or hardware-dependent audio behavior should be documented in the final handoff when it cannot be covered by `PeakGuardAudioTests`.
- For targeted verification, build only the relevant CMake target before running the executable.

## Definition of Done

- Root CMake configure succeeds when build files changed.
- Relevant Debug target builds.
- `PeakGuardAudioTests.exe` passes when helper behavior changed.
- `PeakGuardTray` Release builds when tray, resource, or audio engine code changed.
- Install/uninstall behavior is documented or manually verified when scripts changed.
- Docs are updated when commands, layout, or behavior changes.

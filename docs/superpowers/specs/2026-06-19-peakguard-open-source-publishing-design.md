# PeakGuard Open Source Publishing Design

## Goal

Rebrand the project from Codex Limiter to PeakGuard and prepare the repository for public GitHub publishing. The rename must cover source code, CMake targets, scripts, docs, Win32 identifiers, shared memory, registry names, Start Menu shortcuts, and installed app data paths so new PeakGuard builds do not conflict with older Codex Limiter builds.

## Scope

This work includes repository changes only:

- Rename project-facing files, folders, CMake targets, namespaces, executable names, and user-visible strings to PeakGuard.
- Rename OS-level identifiers to PeakGuard: shared mapping, single-instance mutex, Win32 window classes, settings path, startup registry value, process names, Start Menu folder, and installed executable.
- Update installer and uninstaller script names and behavior for PeakGuard.
- Add VB-CABLE detection and optional official-driver download flow to the installer without committing any VB-CABLE binaries.
- Add an MIT `LICENSE`.
- Rewrite `README.md` for normal users and open-source consumers.
- Update `AGENTS.md` files so future agents use PeakGuard paths, targets, commands, and helper names.
- Verify `.gitignore` still excludes build outputs, binaries, archives, local worktrees, logs, and snapshots.

This work does not include:

- Renaming the active root folder `C:\Users\mansi\new\sound` from inside this session.
- Creating a GitHub repository, adding a remote, pushing to GitHub, or uploading a release.
- Running installer/uninstaller scripts or installing VB-CABLE, because those mutate processes, startup entries, shortcuts, driver state, and `%LOCALAPPDATA%`.
- Committing proprietary VB-CABLE binaries or downloaded archives.

## Naming Model

Use these final names consistently:

- Product: `PeakGuard`
- C++ namespace: `PeakGuard`
- Root CMake project: `PeakGuard`
- Tray target and executable: `PeakGuardTray`
- Audio tests target and executable: `PeakGuardAudioTests`
- State probe target and executable: `PeakGuardStateProbe`
- Shared-state type/header: `PeakGuardSharedState`
- Installer: `scripts/Install-PeakGuard.ps1`
- Uninstaller: `scripts/Uninstall-PeakGuard.ps1`
- Installed directory: `%LOCALAPPDATA%\PeakGuard`
- Start Menu folder: `PeakGuard`
- Startup registry value: `PeakGuard`
- Shared memory mapping: `Local\PeakGuardState`
- Single-instance mutex: `Local\PeakGuardTrayInstance`
- Window classes: `PeakGuardTrayWindow`, `PeakGuardMeter`, `PeakGuardSlider`, `PeakGuardOverlay`

Historical planning documents under `docs/superpowers/` may mention old names as historical context, but active commands and agent guidance must use PeakGuard names. If historical docs are edited during this work, they should avoid presenting old names as current instructions.

## Repository Layout

Rename active tool folders:

- `tools/LimiterTray` -> `tools/PeakGuardTray`
- `tools/LimiterAudioTests` -> `tools/PeakGuardAudioTests`
- `tools/LimiterStateProbe` -> `tools/PeakGuardStateProbe`

Keep `tools/EndpointProbe` as-is unless it contains product-name strings that need updating.

Update root `CMakeLists.txt` to include the new subdirectories. Each renamed target keeps the same structure and source files unless a filename itself contains the old product name.

## Source-Level Changes

The rename is mechanical but must preserve behavior:

- Replace namespace `CodexLimiter` with `PeakGuard`.
- Rename `LimiterSharedState.h` to `PeakGuardSharedState.h`.
- Rename `LimiterSharedState` type references to `PeakGuardSharedState`.
- Update includes from old tool paths to new `tools/PeakGuardTray` and `common/PeakGuardSharedState.h` paths.
- Update Win32 class names and custom control class names to PeakGuard names.
- Update tray title, overlay title, shortcut descriptions, installer output text, README text, and AGENTS guidance.
- Update settings file path construction to `%LOCALAPPDATA%\PeakGuard\settings.ini`.
- Update startup registry helpers so the value name is `PeakGuard`.

The old Codex Limiter shared mapping should not be reused. PeakGuard should open `Local\PeakGuardState` only.

## Installer Design

`Install-PeakGuard.ps1` remains the single entry point for normal users.

Installer responsibilities:

- Detect whether VB-CABLE is already installed using Windows audio device enumeration from PowerShell.
- If VB-CABLE is missing, download the official driver zip from VB-Audio, extract it under `%TEMP%`, and start the x64 setup elevated.
- Never store or commit the downloaded zip or extracted driver files in the repository.
- Clean temporary zip/extract folders after a successful setup attempt.
- Configure/build `PeakGuardTray` from the root CMake build tree if the Release executable is missing.
- Stop an existing `PeakGuardTray` process gracefully with `--quit`, falling back to process stop if needed.
- Copy `PeakGuardTray.exe` to `%LOCALAPPDATA%\PeakGuard`.
- Create a Start Menu shortcut pointing at the installed executable with `--show`.
- Optionally create the HKCU Run entry when `-EnableStartup` is passed.
- Launch the installed copy with `--show`.

Failure handling:

- If CMake is not on `PATH`, keep the existing Visual Studio bundled CMake discovery fallback.
- If VB-CABLE download or setup launch fails, print the manual install URL and exit nonzero.
- If the Release executable cannot be found or built, fail fast with a command that references `PeakGuardTray`.

`Uninstall-PeakGuard.ps1` should stop `PeakGuardTray`, remove the `PeakGuard` HKCU Run entry, remove the Start Menu shortcut/folder, and remove `%LOCALAPPDATA%\PeakGuard`.

## README And License

`README.md` should become user-first:

- Title: `# PeakGuard`
- Short description: lightweight WASAPI loopback limiter and soundbooster for Windows.
- Installation section for normal users using `.\scripts\Install-PeakGuard.ps1 -Configuration Release`.
- Clear VB-CABLE note: the installer downloads and prompts for VB-CABLE installation when missing.
- Developer build/test commands using new target names.
- Uninstall command using `Uninstall-PeakGuard.ps1`.
- Screenshot placeholder: `![PeakGuard Screenshot](assets/screenshot.png)`.
- Mention that built binaries, archives, and build folders are not committed.

Add an MIT `LICENSE` at the repository root.

## Verification

Required automated checks after implementation:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
cmake --build build --config Release --target PeakGuardTray
git diff --check
git status --short
```

Use Visual Studio bundled CMake if `cmake` is not on `PATH`.

Manual checks to report, not run automatically:

- Run `.\scripts\Install-PeakGuard.ps1 -Configuration Release` from a clean state.
- Verify VB-CABLE detection/download prompt behavior.
- Verify the installed app launches from `%LOCALAPPDATA%\PeakGuard`.
- Verify Start Menu shortcut and optional startup entry point to `PeakGuardTray.exe`.
- Verify UI, tray, overlay, settings, and process names show PeakGuard.
- Uninstall old Codex Limiter separately if the user wants to remove legacy installed state.

## Risks

- A broad rename can miss string literals in Win32 identifiers, registry values, or shared memory names. Mitigation: run focused `rg` searches for old names after the rename.
- Existing build directories may contain stale old target outputs. Mitigation: configure/build from root and rely on ignored `build/`; report if a clean build directory is needed.
- VB-CABLE installer requires elevation and user interaction. Mitigation: do not run it automatically during verification; document the manual validation path.
- Historical docs can contain old names. Mitigation: separate active instructions from historical context and ensure AGENTS/README/current commands are PeakGuard-only.


# VB-CABLE Limiter Power and Performance Optimization Design

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

## Goal

Reduce Codex Limiter idle and active CPU overhead without changing the VB-CABLE routing model, limiter behavior, or output-device switching semantics.

## Scope

This spec covers low-risk power and performance improvements only:

- UI timer and tray icon update throttling.
- Hidden-window idle behavior.
- Limiter-disabled audio-path bypass.
- Denormal floating-point protection.
- GDI resource reuse for paint-heavy controls.
- Lightweight measurement and regression checks.

This spec does not change:

- VB-CABLE installation or routing.
- Windows default output recovery behavior.
- Physical output target selection.
- Limiter DSP shape, ceiling semantics, attack/release behavior, or latency.
- Historical feasibility work.

## Current Architecture

The app routes system audio through VB-CABLE:

```text
Windows apps -> CABLE Input -> CABLE Output capture -> Codex Limiter -> physical output
```

The audio thread uses WASAPI event callbacks, runs under MMCSS audio scheduling, captures from the virtual cable, optionally applies limiter gain, and writes to a physical render endpoint. The UI thread owns the tray icon, meter drawing, slider, checkbox, and shared-state display.

## Problem Statement

The current app is stable enough to keep routing intact, but it still performs avoidable work:

- The UI timer runs even when the window is hidden.
- The tray tooltip is updated every timer tick even when the text has not changed.
- The limiter-disabled path still enters the limiter DSP loop.
- Paint handlers allocate and destroy GDI objects on every paint.
- Floating-point denormals are not explicitly flushed on the audio thread.

These are worth fixing before more aggressive silence-mode changes because they reduce overhead without risking missed audio, pops, clicks, or route churn.

## Design

### 1. UI Timer Lifecycle

The UI timer should run only when it has useful work:

- Start the timer when the main window is created and visible.
- Stop the timer when the main window is hidden.
- Restart the timer when the user opens the window from the tray.
- Keep meter updates at `100 ms` while visible so the output meter feels responsive.
- Do not poll the UI at `100 ms` while the window is hidden.

Tray menu actions and audio state changes must still be safe while the timer is stopped. User commands should call `UpdateControls()` directly after changing state.

### 2. Tray Icon Dirty-Check

The tray tooltip depends on a small amount of state, currently whether settings are unavailable, limiter enabled, or limiter disabled. The app should cache the last tooltip text and call `Shell_NotifyIconW(NIM_MODIFY)` only when the text changes.

This removes Explorer IPC from every timer tick.

### 3. Limiter Disabled Bypass

When `LimiterSharedState.enabled == 0`, the audio path must still copy captured audio to the render buffer, but it should skip limiter gain computation.

Behavior when disabled:

- Do not scan input peak for limiter gain decisions.
- Do not multiply samples by `1.0f`.
- Do not run release logic.
- Optionally update a low-cost process counter so the UI can still know audio is flowing.
- Leave captured samples unchanged before copying to the render buffer.

The implementation must not change audio format handling or render-buffer fill behavior.

### 4. Denormal Protection

The audio thread should enable flush-to-zero and denormals-are-zero for SSE floating-point processing before entering the capture/render loop.

Expected behavior:

- Very small float values should not trigger denormal slow paths.
- The setting is thread-local and should be applied inside `AudioThreadProc()`.
- This should not change normal audible output.

### 5. GDI Resource Reuse

Meter and slider paint code should reuse fixed-color GDI resources instead of allocating them for each paint.

The app already creates some module-level brushes. Extend that pattern for repeated pens and brushes used by:

- Meter background.
- Meter fill.
- Ceiling marker.
- Slider rail.
- Slider active fill.
- Slider tick marks.
- Slider knob.

All created GDI objects must be destroyed during `WM_DESTROY`.

### 6. Measurement

Optimization work must be verified with repeatable local commands:

- Build `LimiterTray` in Release.
- Run `LimiterAudioTests`.
- Start the Release tray app.
- Measure `Get-Process LimiterTray` CPU delta over 5 seconds while idle.
- Confirm `audioEngineState=Running` when VB-CABLE is present.
- Confirm `processCounter` continues increasing during test tone playback.

The measurements are not hard pass/fail benchmarks because Windows scheduling and external audio activity vary. They are sanity checks to catch regressions and obviously wasteful behavior.

## Non-Goals

- Do not stop WASAPI capture/render clients during silence in this milestone.
- Do not lower or toggle MMCSS/AVRT priority during silence.
- Do not replace `InterlockedExchange` shared-state writes with volatile writes.
- Do not add SIMD limiter processing.
- Do not remove `_reserved` fields from shared state.
- Do not redesign the limiter DSP.

## Rationale

Stopping audio clients during silence could save more power, but it risks missing the first audio buffer or producing clicks during restart. That needs a separate design with explicit startup latency, pre-roll, and silence transition tests.

The first optimization pass should remove obvious UI and bypass overhead while preserving the stable audio route.

## Success Criteria

- `LimiterAudioTests` passes.
- `LimiterTray` Release build succeeds.
- The tray window meter updates smoothly enough when visible.
- Hiding the window stops periodic UI polling.
- The tray tooltip is not modified unless the tooltip text changes.
- Disabling the limiter bypasses limiter DSP while preserving audio passthrough.
- Audio thread enables denormal flush protection.
- No new routing behavior is introduced.
- No measurable regression in idle CPU compared with the current Release build.

## Test Plan

Use these commands from the worktree root:

```powershell
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
& $cmake --build build\LimiterAudioTests --config Debug
& build\LimiterAudioTests\Debug\LimiterAudioTests.exe
& $cmake --build build\LimiterTray --config Release
```

Runtime sanity check:

```powershell
Get-Process LimiterTray -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath (Resolve-Path build\LimiterTray\Release\LimiterTray.exe) -ArgumentList '--show'
Start-Sleep -Seconds 2
& build\LimiterStateProbe\Debug\LimiterStateProbe.exe
$p = Get-Process LimiterTray -ErrorAction Stop | Select-Object -First 1
$cpu1 = $p.CPU
Start-Sleep -Seconds 5
$cpu2 = (Get-Process -Id $p.Id).CPU
[Math]::Round($cpu2 - $cpu1, 4)
```

Expected runtime state with VB-CABLE installed:

```text
audioEngineState=Running
sampleRate=48000
```

## Risks

- Hiding the UI timer must not prevent visible controls from refreshing when the window is shown again.
- Dirty-checking tray updates must not leave stale tray text after enabling or disabling the limiter.
- Disabled bypass must not skip render-buffer writes.
- GDI resource caching must not leak handles or destroy objects while controls still use them.
- Denormal mode should be set on the audio thread, not only the UI thread.

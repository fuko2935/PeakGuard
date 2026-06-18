# Symmetric Black UI Redesign Design

## Goal

Redesign every visible Codex Limiter surface around a symmetric black Windows 11-style control experience while preserving power efficiency as the primary engineering constraint.

## Approved Visual Direction

The approved direction is a centered, symmetric, black/antracite interface with a warm amber accent. Avoid blue-heavy surfaces. Avoid left-weighted layouts. The main limiter value, sliders, status, and overlay content should align around the visual center.

The UI should feel close to a Windows 11 sound flyout, but implemented with lightweight Win32 drawing rather than heavyweight UI frameworks.

## Main Window

- The main window becomes resizable.
- The default window size is large enough that no text is clipped.
- Controls should reflow from the current client rectangle instead of using fixed bottom coordinates.
- The status line becomes short, centered, and ellipsized when needed.
- The status text should prefer short phrases such as `Active`, `Idle`, `Device changed`, `Error`, `CABLE loopback`, and `hotkeys OK`.
- The primary limiter readout is centered and large.
- Limiter and Soundbooster controls use matching symmetric tiles.
- The existing meter, limiter ceiling slider, boost slider, and enable toggles remain available.

## Settings Surface

Add a small settings surface inside the tray window rather than a separate process.

Settings should include:

- Shortcut display and editing for limiter controls.
- Shortcut display and editing for Soundbooster controls.
- Start with Windows toggle.
- Overlay on/off toggle.
- Power/UI refresh mode display.

The implementation can use simple tab-like buttons or a segmented control. It does not need a full custom settings framework.

## Hotkeys

Limiter hotkeys:

- `Ctrl+Alt+A`: lower limiter ceiling by 1 dB.
- `Ctrl+Alt+D`: raise limiter ceiling by 1 dB.
- `Ctrl+Alt+S`: toggle limiter enabled state.

Soundbooster hotkeys:

- `Ctrl+Alt+F6`: lower boost by 1 dB.
- `Ctrl+Alt+F7`: raise boost by 1 dB.

If a hotkey registration fails, the app keeps running and reports the unavailable shortcut in short status/settings text.

Shortcut editing should be simple: choose an action, press a replacement key combination, validate it, save it, unregister the old hotkey, and register the new one. Persist custom shortcuts in settings. Provide a reset-to-defaults action.

## Overlay

The overlay moves to the bottom center of the primary/work-area monitor. It remains topmost, no-activate, and transient.

Overlay behavior:

- Limiter changes show `Limiter` and the current ceiling, for example `-26 dBFS`.
- Soundbooster changes show `Soundbooster` and the current boost, for example `+6 dB`.
- Toggle actions show the relevant on/off state.
- The overlay uses the same black/antracite surface and amber accent as the main UI.
- The overlay hides automatically after a short delay.

## Startup

Add a `Start with Windows` setting. Prefer the current executable path under the current user startup mechanism. Do not require elevation.

The startup setting must be explicit and reversible from the UI. Installer behavior stays separate from this redesign.

## Single Instance

Only one tray app instance should run at a time.

When a second instance starts:

- It should signal the running instance to show/focus the existing window.
- Then the second instance should exit.
- If signaling fails, it should exit without starting a second audio engine.

## Power Efficiency

Power efficiency remains the first priority.

Rules:

- Do not add idle animations.
- Do not run UI refresh timers while the main window and overlay are both hidden.
- Use existing timer cadence only while visible UI needs updates.
- Overlay timers should only run while the overlay is shown.
- Avoid doing file I/O, shell registration, or device enumeration on the audio thread.
- Keep all audio path changes compatible with the current WASAPI loopback model.

## Persistence

Persist new settings in the existing `%LOCALAPPDATA%\CodexLimiter\settings.ini` file.

Persist:

- Limiter enabled state.
- Limiter ceiling.
- Soundbooster enabled state.
- Soundbooster boost.
- Overlay enabled.
- Start with Windows preference.
- Hotkey defaults and custom hotkey values.

## Testing

Add or update `LimiterAudioTests` coverage for deterministic helpers:

- Hotkey policy defaults and uniqueness.
- Settings apply/read behavior for new persisted fields if moved into helper code.
- Startup setting command/path helper if extracted from Win32 calls.
- Any shared-state helper changes.

Compile verification:

- Build `LimiterAudioTests` Debug and run it.
- Build `LimiterTray` Release.

Manual verification:

- Visual layout, resize behavior, overlay position, startup registration, and single-instance behavior require a Windows desktop session.

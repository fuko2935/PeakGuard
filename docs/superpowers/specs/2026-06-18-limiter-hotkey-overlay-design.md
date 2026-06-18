# Limiter Hotkey Overlay Design

> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.

## Goal

Add single-hand global keyboard control for the limiter and soundbooster, with a transient Windows-volume-style overlay whenever the shortcuts are used.

## Shortcuts

- `Ctrl+Alt+A` lowers the limiter ceiling by 1 dB.
- `Ctrl+Alt+D` raises the limiter ceiling by 1 dB.
- `Ctrl+Alt+S` toggles limiter enabled state.
- `Ctrl+Alt+F6` lowers soundbooster gain by 1 dB.
- `Ctrl+Alt+F7` raises soundbooster gain by 1 dB.

The app will attempt to register all five hotkeys with `RegisterHotKey`. If one registration fails because another app owns it, the tray UI remains usable and the status text reports the failure. Hotkeys can be changed from the tray settings UI, but duplicate assignments are rejected before saving because Windows cannot register two actions with the same modifier/key pair.

## Overlay

The tray app creates a borderless, topmost, tool-window overlay. It does not take focus. A hotkey press updates limiter or soundbooster state, shows the overlay, and restarts a short hide timer. The overlay can be disabled from the Startup tab.

## State And Persistence

Hotkeys mutate the existing `LimiterSharedState` with the same interlocked writes used by the tray controls. Shortcut assignments, selected tab, overlay enabled state, limiter settings, and soundbooster settings are saved through the existing settings file path so changes survive restart.

## Startup

The Startup tab can toggle the HKCU `CodexLimiter` Run entry for the current executable. The installer and Start Menu shortcut still launch the installed copy under `%LOCALAPPDATA%\CodexLimiter`.

## Testing

Pure hotkey and startup policy constants live in small headers and are covered by `LimiterAudioTests`. UI registration, startup registry writes, and overlay drawing are verified by compiling the `LimiterTray` target and require manual validation in a desktop session.

# Windows APO Limiter Design

## Goal

Build a small Windows system-wide audio limiter for personal use. The app should behave like the JamesDSP limiter workflow the user liked on Linux: all output audio is limited, the sound should stay clean, and the app should consume as little CPU and power as practical.

The first implementation target is APO-first. If APO support proves unreliable on the user's devices, the APO work should be removable and the project should fall back to a virtual-audio-device design.

Target devices for the first validation pass:

- The laptop's built-in speakers.
- Havit H655BT Bluetooth headphones in normal stereo/music output mode.

## Non-Goals

- No equalizer.
- No VST hosting.
- No compressor UI.
- No per-app routing.
- No manual output-device selector in the main UI.
- No Electron UI.
- No generic promise that every Windows audio device will support the APO path.

## Chosen Approach

Use a custom Windows Audio Processing Object (APO) as the primary audio processing path.

The APO path is preferred because it avoids a permanent virtual routing layer, should be closer to JamesDSP-style system integration, and has better potential for low latency and low power use. It is also riskier than virtual-device routing because APO behavior depends on the Windows audio effect chain, device driver behavior, enhancement settings, Bluetooth endpoint mode, and Windows updates.

Because of that risk, implementation must start with an APO feasibility milestone before building the full limiter product.

Primary implementation stack:

- C++ for APO and real-time DSP integration.
- Win32 or another lightweight native UI layer for the tray app.
- Windows SDK/WDK APIs for endpoint monitoring, APO registration, installation, and diagnostics.
- Microsoft SysVAD/SwapAPO samples and Equalizer APO behavior as reference material, not as copied product scope.

## Architecture

### Tray App

The tray app owns user-facing control and device monitoring. It does not perform real-time audio processing.

Responsibilities:

- Run in the system tray.
- Open a small control window when clicked.
- Store the limiter enabled state.
- Store the ceiling value in dB.
- Watch output-device changes.
- Detect when the active output device changes.
- Trigger APO install, repair, test, or uninstall actions.
- Show clear status when APO is active, testing, unsupported, or needs repair.

The tray app should be native and lightweight. Electron is explicitly out of scope.

### APO Limiter Module

The APO module runs in the Windows audio effect chain for each supported playback endpoint.

Responsibilities:

- Process output samples in real time.
- Pass audio through unchanged when the limiter is disabled.
- Apply the limiter when enabled.
- Read the latest enabled state and ceiling value from a safe shared settings mechanism.
- Avoid heap allocation, logging, blocking calls, UI calls, disk I/O, and locks on the real-time audio path.
- Fail safe: if settings cannot be read or are invalid, use pass-through or a conservative safe default rather than muting audio unexpectedly.

### Installer and Diagnostics

The installer/diagnostic layer is required, not optional. APO failures must be visible instead of silently pretending the limiter is active.

Responsibilities:

- Install the APO for supported playback endpoints.
- Remove the APO cleanly.
- Detect whether Windows audio enhancements/effects are enabled enough for the APO to run.
- Run a clear feasibility test before the full limiter work proceeds.
- Provide a repair flow if a device becomes unsupported after a driver or Windows update.

The first diagnostic test should be intentionally obvious, such as applying a temporary `-20 dB` gain test through the APO. If the user cannot hear a clear drop, or if any diagnostic validation proves no APO processing occurred, the device must be treated as unsupported until repaired.

## Device Handling

The user does not want manual device selection.

Expected behavior:

- The user changes output devices through Windows as usual.
- The tray app observes playback endpoint changes.
- The app ignores manual device selection in its own UI.
- When a new active output endpoint is detected, the app checks whether the APO is installed and working for that endpoint.
- If the endpoint is supported, the limiter works automatically.
- If the endpoint is not supported, the app shows a concise warning and offers repair.

Bluetooth handling:

- Havit H655BT should be tested in stereo/music playback mode.
- Hands-free/headset mode is not a primary target because it can lower audio quality and change endpoint behavior.
- The app should report a clear unsupported or degraded-mode status if the current Bluetooth endpoint cannot run the APO correctly.

## Limiter Behavior

The limiter should be a clean brickwall-style limiter, not a clipping effect.

User controls:

- `Limiter`: on/off.
- `Ceiling`: dB value.

Suggested ceiling range:

- Minimum: `-30 dB`.
- Maximum: `0 dB`.
- Initial default: `-6 dB`.

DSP behavior:

- Use a short lookahead buffer, targeting about `5-10 ms`.
- Use fast attack to prevent obvious threshold overshoot.
- Use controlled release to reduce pumping and audible distortion.
- Smooth enabled/disabled and ceiling changes to avoid clicks.
- Protect against NaN, infinity, denormal, silence, and sudden transient cases.
- Keep the processing simple enough to stay comfortably below `1%` CPU on normal hardware.

Bypass behavior:

- Limiter off means pass-through inside the APO.
- The APO stays installed so toggling is instant and device state remains stable.

## UI Design

The UI should be intentionally small.

Tray:

- Left click opens the small control window.
- Double click or an `Open` menu action may open the same window as a normal window.
- Context menu should include open, enable/disable, diagnostic/repair if needed, and quit.

Small window:

- Limiter on/off control.
- Ceiling dB slider or numeric control.
- Short status line, such as:
  - `APO active`
  - `Testing device`
  - `Limiter inactive on this device`
  - `Repair required`

The main UI should not expose device selection, attack, release, lookahead, compressor ratio, VST loading, EQ bands, profiles, or routing.

## Feasibility Milestone

The first implementation milestone is not the full product. It is a narrow APO proof.

Pass criteria:

- Minimal APO can be installed and removed cleanly.
- Built-in laptop speakers can run an obvious test effect.
- Havit H655BT can run an obvious test effect in stereo/music mode, or the app clearly reports that it cannot.
- The tray/diagnostic code can detect active endpoint changes.
- The system can recover after disabling or uninstalling the APO.

If the built-in speakers fail APO feasibility, stop the APO product path and move to the virtual-audio-device fallback design.

If only Havit H655BT fails, continue only if the user accepts that the limiter will not work on that Bluetooth endpoint. Otherwise move to the virtual-audio-device fallback design.

## Fallback Plan

If APO is not reliable enough on the user's devices, the project should pivot to a virtual-audio-device design:

```text
Windows apps -> virtual playback device -> limiter app -> real output device
```

That fallback has more routing overhead than APO, but it is more predictable across output devices. The limiter DSP core should be written so it can be reused by either the APO path or the virtual-device path.

## Safety and Recovery

The project must include a clear recovery path because APO work can affect audio output.

Required behavior:

- Provide a clean uninstall path for the APO.
- Provide a disable path that leaves audio pass-through.
- Avoid muting the system unexpectedly if settings are invalid.
- Keep diagnostics explicit when the APO is installed but not processing.
- Do not hide APO/device compatibility failures behind a normal-looking `enabled` state.

## Testing Strategy

### APO Feasibility Tests

- Install minimal APO.
- Apply temporary `-20 dB` gain test.
- Verify on laptop speakers.
- Verify on Havit H655BT stereo/music endpoint.
- Switch between speakers and headphones and verify the app detects the active endpoint.
- Uninstall APO and confirm normal audio returns.

### DSP Unit Tests

- Bypass outputs match input within expected float tolerance.
- Ceiling is not exceeded for sine waves, impulses, stepped tones, and mixed test signals.
- Enable/disable transition does not click due to abrupt gain jumps.
- Ceiling changes are smoothed.
- NaN, infinity, denormal, silence, and all-zero buffers do not break processing.

### Runtime and Power Checks

- Verify no heap allocation on the audio callback path.
- Verify no locks, logging, disk I/O, or UI calls on the audio callback path.
- Measure CPU while idle, bypassed, and limiting active audio.
- Confirm the tray app is near-zero CPU when the window is closed.

### Manual Audio Tests

- YouTube or browser audio.
- System notification sounds.
- Game or low-latency app if available.
- Built-in speakers.
- Havit H655BT connect, disconnect, reconnect.
- Windows output-device switching.
- Limiter on/off toggle.
- Ceiling changes while audio is playing.

### Review Checklist

Before claiming the implementation is complete:

- Review real-time audio code for allocations, locks, blocking calls, and logging.
- Review installer/uninstaller for reversible changes.
- Review device-change handling for loops, stale endpoint IDs, and unsupported states.
- Run the feasibility tests and DSP tests.
- Run manual switching tests on the user's actual devices.

## Implementation Boundary

Do not build broad audio tooling. The product is a single-purpose limiter with a minimal UI.

Implementation should preserve these boundaries:

- DSP core is independent and reusable.
- APO host/integration is separate from DSP.
- Tray UI is separate from real-time audio code.
- Diagnostics are separate enough to be run before the full limiter path is trusted.

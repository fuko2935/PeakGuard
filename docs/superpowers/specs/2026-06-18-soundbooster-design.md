# Soundbooster Design

## Goal

Add a user-controlled soundbooster to Codex Limiter so quiet content can be made louder while the existing limiter can still prevent excessive peaks. The feature stays in the current tray app and uses the existing VB-CABLE loopback audio path.

## User-Facing Behavior

- Add a separate `Soundbooster enabled` checkbox.
- Add a second slider labeled `Boost: +N dB`.
- Boost range is `0 dB` to `+24 dB`, in 1 dB steps.
- Default behavior for existing users is unchanged: soundbooster is off and boost is `0 dB`.
- Limiter and soundbooster are independent controls:
  - Limiter on, soundbooster off: current limiter behavior.
  - Limiter off, soundbooster on: audio is boosted without limiter protection.
  - Limiter on, soundbooster on: audio is boosted first, then limited by the ceiling.
  - Both off: audio passes through unchanged except for existing metering.

Example: the user can set limiter ceiling to `-20 dBFS` and soundbooster to `+10 dB`. The engine raises samples by `+10 dB`, then the limiter keeps output peaks near the `-20 dBFS` ceiling when the boosted signal would exceed it.

## Recommended Approach

Implement soundbooster as pre-limiter gain inside the existing audio processing path:

```text
captured float samples -> enabled booster gain -> enabled limiter -> render output
```

This keeps the architecture small and preserves the current WASAPI loopback model. It avoids changing Windows endpoint volume, default device behavior, or VB-CABLE routing. High boost values can make the limiter work harder and produce more compressed audio, but that is expected behavior for a manual booster.

## Shared State

Extend `LimiterSharedState` with soundbooster fields:

- `boostEnabled`: `1` when soundbooster is active, `0` otherwise.
- `boostMilliDb`: clamped gain in millidecibels, from `0` to `24000`.
- `boostLinearScaled`: linear gain scaled by `kLinearScale`.

Increase `kStateVersion` because the shared memory ABI changes. Initialize new fields in `InitializeStateFields` and preserve interlocked writes. Keep fields as simple `LONG` values only; do not store process-local pointers, handles, or STL objects in shared state.

Add helper constants and functions near the existing limiter helpers:

- `kDefaultBoostMilliDb = 0`
- `kMinBoostMilliDb = 0`
- `kMaxBoostMilliDb = 24000`
- `ClampBoostMilliDb`
- `BoostMilliDbToLinearScaled`
- `SetBoostMilliDb`

## Settings

Extend `LimiterSettings` with:

- `bool boostEnabled = false`
- `LONG boostMilliDb = kDefaultBoostMilliDb`

Persist settings in the existing `%LOCALAPPDATA%\CodexLimiter\settings.ini` file:

```ini
enabled=1
ceilingMilliDb=-20000
boostEnabled=1
boostMilliDb=10000
```

Missing `boostEnabled` or `boostMilliDb` keys fall back to defaults, so older settings files remain valid.

## Audio Processing

Update the DSP step currently owned by `LoopbackAudioEngine::ApplyLimiter`.

The processing order is:

1. Read `boostEnabled`, `boostLinearScaled`, `enabled`, and `ceilingLinearScaled` from shared state.
2. Track the input peak before boost so existing input-meter semantics remain stable.
3. If soundbooster is enabled, multiply each sample by the boost gain.
4. If limiter is enabled, apply the existing limiter to the boosted sample stream.
5. Track output peak after all enabled processing, so the visible output meter reflects what is rendered.

If limiter is disabled and soundbooster is enabled, the engine applies boost without extra hard clipping. This keeps the controls honest: limiting is controlled by the limiter checkbox.

The engine must not write settings, update UI, enumerate devices, or do file I/O from the audio thread.

## UI

Keep the existing small Win32 tray window. Add the soundbooster controls below the limiter controls:

- `Output` text and meter remain at the top.
- Existing `Ceiling` label and limiter slider remain.
- Existing `Limiter enabled` checkbox remains.
- New `Soundbooster enabled` checkbox appears below limiter controls.
- New `Boost: +N dB` label appears above the boost slider.
- New boost slider appears below the boost label.
- Status text moves lower and the window height increases enough to avoid overlap.

The boost slider can reuse the existing custom slider drawing pattern, but should have separate conversion helpers so ceiling and boost math stay clear.

Keyboard behavior should remain simple:

- Existing left/right behavior continues to adjust limiter ceiling when the limiter slider has focus.
- The boost slider adjusts boost by 1 dB with left/right when it has focus.

## Tray Menu

Keep the tray menu focused:

- `Open`
- Existing limiter enable/disable action
- `Quit`

Do not add a soundbooster context-menu item in the first implementation. The primary soundbooster control is the main tray window, keeping the context menu small.

## Tests

Update `tools/LimiterAudioTests/main.cpp` for deterministic helper behavior:

- default shared state initializes booster off and boost `0 dB`
- boost clamp accepts `0` and `24000`
- boost clamp rejects negative and above-range values
- boost linear conversion maps `0 dB` to `1.0x` scaled and `+24 dB` to the rounded scaled value for `10^(24/20)`
- `ApplySettings` writes boost enabled and boost value
- `ReadSettingsFromState` clamps boost value when reading

No live audio hardware or VB-CABLE should be required for tests.

## Verification

For implementation, run:

```powershell
cmake --build build --config Debug --target LimiterAudioTests
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
cmake --build build --config Release --target LimiterTray
```

Run CMake configure only if build files change.

## Out Of Scope

- Automatic gain control or normalization.
- Changing Windows system volume or endpoint volume.
- New installer behavior.
- New diagnostics tools.

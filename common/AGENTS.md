# Shared State Guide

Purpose: `common/` contains shared headers used across the tray app, probes, and tests.

Technology: C++17 headers with Win32 APIs.

Parent context: See `../AGENTS.md`.

## Package Rules

- **MUST** treat `PeakGuardSharedState` in `PeakGuardSharedState.h` as a cross-process ABI.
- **MUST** update `kStateVersion` when changing the shared struct layout or field meaning.
- **MUST** preserve atomic/interlocked writes for shared fields that can be read by another process.
- **MUST** update `tools/PeakGuardStateProbe/main.cpp` if printed state fields change.
- **MUST** update `tools/PeakGuardAudioTests/main.cpp` when changing clamp, conversion, initialization, or settings semantics.
- **MUST NOT** store pointers, handles, STL objects, or process-local addresses in shared mapped state.
- **SHOULD** keep helpers inline and small unless a second source file becomes necessary.

## Setup & Run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
cmake --build build --config Debug --target PeakGuardStateProbe
```

## Patterns & Conventions

- DO: Keep constants and conversion helpers beside the state contract in `common/PeakGuardSharedState.h`.
- DO: Use `InterlockedExchange` style writes as shown in `InitializeStateFields` and `SetCeilingMilliDb`.
- DO: Keep mapped-state access compatible with `tools/PeakGuardTray/LoopbackAudioEngine.cpp` and `tools/PeakGuardStateProbe/main.cpp`.
- DO: Use `SharedLimiterMapping` when a tool needs to open the shared limiter state.
- MUST NOT: Change field order, defaults, or scaling without updating versioning, tests, and probes.

## Touch Points / Key Files

- Shared ABI and helpers: `PeakGuardSharedState.h`.
- Runtime writer: `../tools/PeakGuardTray/LoopbackAudioEngine.cpp`.
- UI/settings writer: `../tools/PeakGuardTray/main.cpp`.
- State reader/mutator: `../tools/PeakGuardStateProbe/main.cpp`.
- Regression coverage: `../tools/PeakGuardAudioTests/main.cpp`.

## JIT Index Hints

```powershell
rg -n "PeakGuardSharedState|kStateVersion|SharedLimiterMapping" common tools
rg -n "Interlocked|ceilingMilliDb|ceilingLinearScaled|processCounter" common tools
rg -n "InitializeStateFields|SetCeilingMilliDb|MilliDbToLinearScaled" common tools/PeakGuardAudioTests
```

## Common Gotchas

- The shared mapping name is `Local\\PeakGuardState`; changing it disconnects probes from the tray app.
- `ceilingMilliDb` and `ceilingLinearScaled` must stay in sync.
- `LinearToMilliDb` returns `-120000` for silence or invalid values; UI and tests expect this sentinel.

## Pre-PR Checks

```powershell
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
cmake --build build --config Debug --target PeakGuardStateProbe
```

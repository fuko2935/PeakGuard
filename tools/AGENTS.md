# Native Tools Guide

Purpose: `tools/` contains the tray app, diagnostic probes, and native regression tests.

Technology: C++17, CMake, Win32, WASAPI, WRL COM helpers.

Parent context: See `../AGENTS.md`.

## Package Rules

- **MUST** keep each executable target in its own subdirectory with a local `CMakeLists.txt`.
- **MUST** add new tool directories to the root `CMakeLists.txt`.
- **MUST** use `UNICODE` and `_UNICODE` compile definitions for Windows tools unless there is a concrete reason not to.
- **MUST** use `Microsoft::WRL::ComPtr` or clear RAII wrappers for COM lifetimes.
- **MUST** keep hardware-mutating diagnostics opt-in through command-line arguments.
- **SHOULD** prefer focused helper headers when behavior is shared between app and tests.

## Setup & Run

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Debug --target EndpointProbe
cmake --build build --config Debug --target LimiterStateProbe
cmake --build build --config Debug --target LimiterAudioTests
cmake --build build --config Release --target LimiterTray
```

## Patterns & Conventions

- DO: Follow per-target CMake style in `tools/EndpointProbe/CMakeLists.txt` and `tools/LimiterTray/CMakeLists.txt`.
- DO: Use COM initialization and RAII patterns from `tools/EndpointProbe/main.cpp` for console diagnostics.
- DO: Keep shared behavior testable through headers like `tools/LimiterTray/AudioEndpointSelection.h`.
- DO: Keep tray app behavior in `tools/LimiterTray/`; probes should inspect or diagnose, not own product logic.
- MUST NOT: Add generated Visual Studio project files or build artifacts under `tools/`.

## Touch Points / Key Files

- Root target list: `../CMakeLists.txt`.
- Tray app: `LimiterTray/`.
- Native tests: `LimiterAudioTests/`.
- Endpoint diagnostics: `EndpointProbe/main.cpp`.
- Shared-state diagnostics: `LimiterStateProbe/main.cpp`.

## JIT Index Hints

```powershell
rg -n "add_executable|target_compile_definitions|target_link_libraries" tools
rg -n "CoInitializeEx|ComPtr|IMMDevice|IAudioClient" tools
rg -n "wmain|wWinMain|ParseArgs|PrintUsage" tools
rg -n "CodexLimiter::" tools
```

## Common Gotchas

- `EndpointProbe --render-tone-seconds N` plays audio on the default endpoint.
- `LimiterStateProbe --enable`, `--disable`, and `--ceiling-millidb` mutate the live shared state.
- `LimiterTray` can change the default render endpoint while routing through virtual audio.

## Pre-PR Checks

```powershell
cmake --build build --config Debug
.\build\tools\LimiterAudioTests\Debug\LimiterAudioTests.exe
cmake --build build --config Release --target LimiterTray
```

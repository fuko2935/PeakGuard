# PeakGuard Open Source Publishing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename Codex Limiter to PeakGuard across the active repo, add user-facing open-source publishing assets, and prepare a safe installer path that can bootstrap VB-CABLE without committing driver binaries.

**Architecture:** Treat this as a behavior-preserving rename plus publishing polish. Rename active source folders, CMake targets, scripts, namespace/type names, OS-level identifiers, docs, and tests together so each build artifact has one PeakGuard identity. Keep installer execution and GitHub publishing as manual verification/release steps because they mutate local machine state or require account credentials.

**Tech Stack:** C++17, Win32, WASAPI, CMake, MSVC Build Tools, PowerShell, native `PeakGuardAudioTests`.

---

## File Structure

- Rename `common/LimiterSharedState.h` to `common/PeakGuardSharedState.h`; it remains the shared-state ABI and helper location.
- Rename `tools/LimiterTray/` to `tools/PeakGuardTray/`; it remains the Win32 tray UI and audio engine target.
- Rename `tools/LimiterAudioTests/` to `tools/PeakGuardAudioTests/`; it remains the deterministic native test target.
- Rename `tools/LimiterStateProbe/` to `tools/PeakGuardStateProbe/`; it remains the shared-state diagnostic tool.
- Rename `scripts/Install-CodexLimiter.ps1` to `scripts/Install-PeakGuard.ps1`.
- Rename `scripts/Uninstall-CodexLimiter.ps1` to `scripts/Uninstall-PeakGuard.ps1`.
- Create `LICENSE`.
- Modify `README.md`, root `CMakeLists.txt`, `AGENTS.md`, `common/AGENTS.md`, `tools/AGENTS.md`, `tools/PeakGuardTray/AGENTS.md`, `tools/PeakGuardAudioTests/AGENTS.md`, and `scripts/AGENTS.md`.

## Task 1: Rename Files, Folders, And CMake Targets

**Files:**
- Rename: `common/LimiterSharedState.h` -> `common/PeakGuardSharedState.h`
- Rename: `tools/LimiterTray/` -> `tools/PeakGuardTray/`
- Rename: `tools/LimiterAudioTests/` -> `tools/PeakGuardAudioTests/`
- Rename: `tools/LimiterStateProbe/` -> `tools/PeakGuardStateProbe/`
- Rename: `scripts/Install-CodexLimiter.ps1` -> `scripts/Install-PeakGuard.ps1`
- Rename: `scripts/Uninstall-CodexLimiter.ps1` -> `scripts/Uninstall-PeakGuard.ps1`
- Modify: `CMakeLists.txt`
- Modify: `tools/PeakGuardTray/CMakeLists.txt`
- Modify: `tools/PeakGuardAudioTests/CMakeLists.txt`
- Modify: `tools/PeakGuardStateProbe/CMakeLists.txt`

- [ ] **Step 1: Rename active files and folders**

Run from repository root:

```powershell
Rename-Item -LiteralPath common\LimiterSharedState.h -NewName PeakGuardSharedState.h
Rename-Item -LiteralPath tools\LimiterTray -NewName PeakGuardTray
Rename-Item -LiteralPath tools\LimiterAudioTests -NewName PeakGuardAudioTests
Rename-Item -LiteralPath tools\LimiterStateProbe -NewName PeakGuardStateProbe
Rename-Item -LiteralPath scripts\Install-CodexLimiter.ps1 -NewName Install-PeakGuard.ps1
Rename-Item -LiteralPath scripts\Uninstall-CodexLimiter.ps1 -NewName Uninstall-PeakGuard.ps1
```

Expected: `git status --short` shows deletes/adds or renames for the old/new paths.

- [ ] **Step 2: Update root CMake project and subdirectories**

Replace `CMakeLists.txt` content with:

```cmake
cmake_minimum_required(VERSION 3.20)
project(PeakGuard LANGUAGES CXX)

add_subdirectory(tools/EndpointProbe)
add_subdirectory(tools/PeakGuardAudioTests)
add_subdirectory(tools/PeakGuardStateProbe)
add_subdirectory(tools/PeakGuardTray)
```

- [ ] **Step 3: Update tray target CMake**

In `tools/PeakGuardTray/CMakeLists.txt`, replace the project and target names:

```cmake
cmake_minimum_required(VERSION 3.20)
project(PeakGuardTray LANGUAGES CXX)

add_executable(PeakGuardTray WIN32 main.cpp LoopbackAudioEngine.cpp PeakGuardTray.rc)
target_compile_features(PeakGuardTray PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardTray PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardTray PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardTray PRIVATE
    comctl32 ole32 oleaut32 uuid propsys advapi32 dwmapi
    mmdevapi avrt)
```

- [ ] **Step 4: Rename tray resource script**

Run:

```powershell
Rename-Item -LiteralPath tools\PeakGuardTray\LimiterTray.rc -NewName PeakGuardTray.rc
```

Expected: `tools/PeakGuardTray/CMakeLists.txt` references `PeakGuardTray.rc`.

- [ ] **Step 5: Update audio test target CMake**

Replace `tools/PeakGuardAudioTests/CMakeLists.txt` content with:

```cmake
cmake_minimum_required(VERSION 3.20)
project(PeakGuardAudioTests LANGUAGES CXX)

add_executable(PeakGuardAudioTests main.cpp)
target_compile_features(PeakGuardAudioTests PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardAudioTests PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardAudioTests PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardAudioTests PRIVATE ole32)
```

- [ ] **Step 6: Update state probe target CMake**

Replace `tools/PeakGuardStateProbe/CMakeLists.txt` content with:

```cmake
cmake_minimum_required(VERSION 3.20)
project(PeakGuardStateProbe LANGUAGES CXX)

add_executable(PeakGuardStateProbe main.cpp)
target_compile_features(PeakGuardStateProbe PRIVATE cxx_std_17)
target_compile_definitions(PeakGuardStateProbe PRIVATE UNICODE _UNICODE NOMINMAX)
target_include_directories(PeakGuardStateProbe PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../..")
target_link_libraries(PeakGuardStateProbe PRIVATE ole32)
```

- [ ] **Step 7: Commit structural rename**

```powershell
git add CMakeLists.txt common tools scripts
git commit -m "Rename project targets to PeakGuard"
```

Expected: commit succeeds. It may not build yet because source identifiers are renamed in later tasks.

## Task 2: Rename C++ Namespace, Shared State, Includes, And OS Identifiers

**Files:**
- Modify: `common/PeakGuardSharedState.h`
- Modify: `tools/PeakGuardTray/*.h`
- Modify: `tools/PeakGuardTray/*.cpp`
- Modify: `tools/PeakGuardAudioTests/main.cpp`
- Modify: `tools/PeakGuardStateProbe/main.cpp`
- Modify: `tools/EndpointProbe/main.cpp` if product strings are present

- [ ] **Step 1: Apply mechanical source replacements**

Run from repository root:

```powershell
$files = rg --files common tools -g '*.h' -g '*.cpp' -g '*.rc'
foreach ($file in $files) {
    $text = Get-Content -Raw -LiteralPath $file
    $text = $text.Replace('CodexLimiter', 'PeakGuard')
    $text = $text.Replace('LimiterSharedState', 'PeakGuardSharedState')
    $text = $text.Replace('LimiterTray', 'PeakGuardTray')
    $text = $text.Replace('LimiterAudioTests', 'PeakGuardAudioTests')
    $text = $text.Replace('LimiterStateProbe', 'PeakGuardStateProbe')
    $text = $text.Replace('Codex Limiter', 'PeakGuard')
    $text = $text.Replace('CodexLimiterTrayWindow', 'PeakGuardTrayWindow')
    $text = $text.Replace('CodexLimiterMeter', 'PeakGuardMeter')
    $text = $text.Replace('CodexLimiterSlider', 'PeakGuardSlider')
    $text = $text.Replace('CodexLimiterOverlay', 'PeakGuardOverlay')
    $text = $text.Replace('CodexLimiterTrayInstance', 'PeakGuardTrayInstance')
    Set-Content -LiteralPath $file -Value $text -NoNewline
}
```

Expected: all active source files now use `PeakGuard` identifiers.

- [ ] **Step 2: Fix include paths**

Run:

```powershell
$files = rg --files common tools -g '*.h' -g '*.cpp'
foreach ($file in $files) {
    $text = Get-Content -Raw -LiteralPath $file
    $text = $text.Replace('common/LimiterSharedState.h', 'common/PeakGuardSharedState.h')
    $text = $text.Replace('tools/LimiterTray/', 'tools/PeakGuardTray/')
    Set-Content -LiteralPath $file -Value $text -NoNewline
}
```

Expected examples:

```cpp
#include "common/PeakGuardSharedState.h"
#include "tools/PeakGuardTray/AudioEndpointSelection.h"
```

- [ ] **Step 3: Verify shared mapping and startup policy names**

In `common/PeakGuardSharedState.h`, verify the mapping open call is:

```cpp
return OpenNamed(L"Local\\PeakGuardState");
```

In `tools/PeakGuardTray/StartupPolicy.h`, verify:

```cpp
inline std::wstring StartupRegistryValueName() {
    return L"PeakGuard";
}
```

- [ ] **Step 4: Verify Win32 class names in tray main**

In `tools/PeakGuardTray/main.cpp`, verify these constants/usages exist:

```cpp
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\PeakGuardTrayInstance";
constexpr wchar_t kMainWindowClassName[] = L"PeakGuardTrayWindow";
```

Verify custom window class registrations use:

```cpp
meterClass.lpszClassName = L"PeakGuardMeter";
sliderClass.lpszClassName = L"PeakGuardSlider";
overlayClass.lpszClassName = L"PeakGuardOverlay";
```

Verify control creation uses the same class names by running:

```powershell
rg -n 'L"PeakGuardMeter"|L"PeakGuardSlider"|L"PeakGuardOverlay"' tools\PeakGuardTray\main.cpp
```

Expected: matches appear in both window class registration and `CreateWindowExW` calls for each custom class.

- [ ] **Step 5: Run source old-name scan**

Run:

```powershell
rg -n "CodexLimiter|Codex Limiter|CodexLimiterTray|CodexLimiterMeter|CodexLimiterSlider|CodexLimiterOverlay|CodexLimiterTrayInstance|LimiterSharedState|LimiterTray|LimiterAudioTests|LimiterStateProbe" common tools CMakeLists.txt
```

Expected: no matches in active source, active CMake, or active AGENTS files. Historical docs are handled in a later documentation task.

- [ ] **Step 6: Build audio tests and fix compile errors**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build -A x64
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target PeakGuardAudioTests
```

Expected: `PeakGuardAudioTests.vcxproj -> C:\Users\mansi\new\sound\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe`.

- [ ] **Step 7: Run renamed audio tests**

Run:

```powershell
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

Expected:

```text
PeakGuardAudioTests passed
```

If the executable still prints `LimiterAudioTests passed`, update `tools/PeakGuardAudioTests/main.cpp` to print:

```cpp
std::cout << "PeakGuardAudioTests passed\n";
```

- [ ] **Step 8: Commit source rename**

```powershell
git add common tools CMakeLists.txt
git commit -m "Rename source identifiers to PeakGuard"
```

## Task 3: Update Installer And Uninstaller For PeakGuard

**Files:**
- Modify: `scripts/Install-PeakGuard.ps1`
- Modify: `scripts/Uninstall-PeakGuard.ps1`

- [ ] **Step 1: Rename installer variables and paths**

In `scripts/Install-PeakGuard.ps1`, ensure the top-level paths and names are:

```powershell
$rootBuildDir = Join-Path $repoRoot 'build'
$directBuildDir = Join-Path $repoRoot 'build/PeakGuardTray'
$rootBuildExe = Join-Path $rootBuildDir "tools/PeakGuardTray/$Configuration/PeakGuardTray.exe"
$directBuildExe = Join-Path $directBuildDir "$Configuration/PeakGuardTray.exe"
$installDir = Join-Path $env:LOCALAPPDATA 'PeakGuard'
$exeDest = Join-Path $installDir 'PeakGuardTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\PeakGuard'
$shortcutPath = Join-Path $startMenuDir 'PeakGuard.lnk'
$runValueName = 'PeakGuard'
```

- [ ] **Step 2: Rename process helper**

Replace the process-stop helper with:

```powershell
function Stop-PeakGuardTray {
    $processes = @(Get-Process PeakGuardTray -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return
    }

    if (Test-Path -LiteralPath $exeDest) {
        Start-Process -FilePath $exeDest -ArgumentList '--quit' -WindowStyle Hidden
    }

    Start-Sleep -Milliseconds 2500
    $remaining = @(Get-Process PeakGuardTray -ErrorAction SilentlyContinue)
    if ($remaining.Count -gt 0) {
        $remaining | Stop-Process -Force
        $remaining | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    }
}
```

- [ ] **Step 3: Add VB-CABLE detection and installer function**

Insert after `Resolve-CMake`:

```powershell
function Install-VBCableIfNeeded {
    $vbCableExists = @(Get-CimInstance Win32_SoundDevice -ErrorAction SilentlyContinue |
        Where-Object { $_.ProductName -match 'VB-Audio Virtual Cable|VB-CABLE|CABLE Input|CABLE Output' })
    if ($vbCableExists.Count -gt 0) {
        Write-Host 'Requirement met: VB-CABLE is already installed.'
        return
    }

    Write-Host 'VB-CABLE not found. Downloading from official servers.'
    $zipUrl = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
    $tempZip = Join-Path $env:TEMP 'VBCABLE_Driver.zip'
    $tempFolder = Join-Path $env:TEMP 'VBCABLE_Extract'

    try {
        Invoke-WebRequest -Uri $zipUrl -OutFile $tempZip -UserAgent 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'
        if (Test-Path -LiteralPath $tempFolder) {
            Remove-Item -LiteralPath $tempFolder -Recurse -Force
        }
        Expand-Archive -LiteralPath $tempZip -DestinationPath $tempFolder -Force

        $setupExe = Join-Path $tempFolder 'VBCABLE_Setup_x64.exe'
        if (-not (Test-Path -LiteralPath $setupExe)) {
            throw "VB-CABLE setup executable not found: $setupExe"
        }

        Write-Host "Launching VB-CABLE setup: $setupExe"
        Start-Process -FilePath $setupExe -Verb RunAs -Wait | Out-Null

        Remove-Item -LiteralPath $tempZip -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $tempFolder -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host 'VB-CABLE setup finished.'
    } catch {
        Write-Host "Failed to auto-install VB-CABLE: $_"
        Write-Host 'Please install manually from: https://vb-audio.com/Cable/'
        exit 1
    }
}
```

- [ ] **Step 4: Call VB-CABLE detection before build**

Add before the build-if-needed block:

```powershell
Install-VBCableIfNeeded
```

- [ ] **Step 5: Update build and launch messages**

Ensure installer command strings use `PeakGuardTray`:

```powershell
throw "$Description not found: $Path. Build first with: cmake --build build --config $Configuration --target PeakGuardTray"
Write-Host 'Building PeakGuardTray.'
& $cmakeExe --build $rootBuildDir --config $Configuration --target PeakGuardTray
$exeSource = Resolve-RequiredPath $exeSource 'PeakGuardTray.exe'
Stop-PeakGuardTray
$shortcut.Description = 'PeakGuard'
Start-Process -FilePath $exeDest -ArgumentList '--show'
Write-Host 'PeakGuard started.'
```

- [ ] **Step 6: Update uninstaller names and paths**

In `scripts/Uninstall-PeakGuard.ps1`, use:

```powershell
$installDir = Join-Path $env:LOCALAPPDATA 'PeakGuard'
$exeDest = Join-Path $installDir 'PeakGuardTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\PeakGuard'
$shortcutPath = Join-Path $startMenuDir 'PeakGuard.lnk'
$runValueName = 'PeakGuard'
```

Rename `Stop-LimiterTray` to `Stop-PeakGuardTray` and use `Get-Process PeakGuardTray`.

- [ ] **Step 7: Script old-name scan**

Run:

```powershell
rg -n "CodexLimiter|Codex Limiter|LimiterTray|CodexLimiterState|Codex Limiter|Install-CodexLimiter|Uninstall-CodexLimiter" scripts
```

Expected: no matches.

- [ ] **Step 8: Commit scripts**

```powershell
git add scripts
git commit -m "Update installer scripts for PeakGuard"
```

## Task 4: Update Agent Guidance And README

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `common/AGENTS.md`
- Modify: `tools/AGENTS.md`
- Modify: `tools/PeakGuardTray/AGENTS.md`
- Modify: `tools/PeakGuardAudioTests/AGENTS.md`
- Modify: `scripts/AGENTS.md`

- [ ] **Step 1: Replace README with user-focused PeakGuard docs**

Replace `README.md` content with:

```markdown
# PeakGuard

PeakGuard is a lightweight Windows tray app that routes system audio through VB-CABLE, applies a WASAPI loopback limiter and soundbooster, and renders the processed audio to a physical output device.

![PeakGuard Screenshot](assets/screenshot.png)

## Installation (For Users)

Open PowerShell from the repository root and run:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release
```

PeakGuard installs to `%LOCALAPPDATA%\PeakGuard`, creates a Start Menu shortcut, and launches the tray app.

Note: PeakGuard uses VB-CABLE to route audio. The installer script will automatically download the official VB-CABLE driver package and prompt you to install it if you do not have it.

To also start PeakGuard with Windows:

```powershell
.\scripts\Install-PeakGuard.ps1 -Configuration Release -EnableStartup
```

## Uninstall

```powershell
.\scripts\Uninstall-PeakGuard.ps1
```

## Developer Build

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
cmake --build build --config Release --target PeakGuardTray
```

## Tests

```powershell
cmake --build build --config Debug --target PeakGuardAudioTests
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

## Repository Layout

- `common/`: shared state ABI and helpers.
- `tools/PeakGuardTray/`: tray UI and WASAPI loopback audio engine.
- `tools/PeakGuardAudioTests/`: native deterministic regression tests.
- `tools/PeakGuardStateProbe/`: shared-state inspection helper.
- `tools/EndpointProbe/`: endpoint diagnostic helper.
- `scripts/`: install and uninstall helpers.
- `docs/superpowers/`: historical specs and implementation plans.

## Release Hygiene

Build outputs, binaries, archives, logs, ETL traces, local worktrees, and snapshots are ignored and should not be committed.
```

- [ ] **Step 2: Update AGENTS command names**

Run mechanical replacements on active guidance:

```powershell
$files = @(
    'AGENTS.md',
    'common/AGENTS.md',
    'tools/AGENTS.md',
    'tools/PeakGuardTray/AGENTS.md',
    'tools/PeakGuardAudioTests/AGENTS.md',
    'scripts/AGENTS.md'
)
foreach ($file in $files) {
    $text = Get-Content -Raw -LiteralPath $file
    $text = $text.Replace('Codex Limiter', 'PeakGuard')
    $text = $text.Replace('CodexLimiter', 'PeakGuard')
    $text = $text.Replace('LimiterSharedState', 'PeakGuardSharedState')
    $text = $text.Replace('LimiterTray', 'PeakGuardTray')
    $text = $text.Replace('LimiterAudioTests', 'PeakGuardAudioTests')
    $text = $text.Replace('LimiterStateProbe', 'PeakGuardStateProbe')
    $text = $text.Replace('Install-CodexLimiter.ps1', 'Install-PeakGuard.ps1')
    $text = $text.Replace('Uninstall-CodexLimiter.ps1', 'Uninstall-PeakGuard.ps1')
    $text = $text.Replace('tools/LimiterTray', 'tools/PeakGuardTray')
    $text = $text.Replace('tools/LimiterAudioTests', 'tools/PeakGuardAudioTests')
    $text = $text.Replace('tools/LimiterStateProbe', 'tools/PeakGuardStateProbe')
    $text = $text.Replace('%LOCALAPPDATA%\\CodexLimiter', '%LOCALAPPDATA%\\PeakGuard')
    Set-Content -LiteralPath $file -Value $text -NoNewline
}
```

- [ ] **Step 3: Verify active docs old-name scan**

Run:

```powershell
rg -n "CodexLimiter|Codex Limiter|Install-CodexLimiter|Uninstall-CodexLimiter|LimiterTray|LimiterAudioTests|LimiterStateProbe|LimiterSharedState" README.md AGENTS.md common/AGENTS.md tools/AGENTS.md tools/PeakGuardTray/AGENTS.md tools/PeakGuardAudioTests/AGENTS.md scripts/AGENTS.md
```

Expected: no matches.

- [ ] **Step 4: Commit active docs**

```powershell
git add README.md AGENTS.md common/AGENTS.md tools/AGENTS.md tools/PeakGuardTray/AGENTS.md tools/PeakGuardAudioTests/AGENTS.md scripts/AGENTS.md
git commit -m "Update docs and guidance for PeakGuard"
```

## Task 5: Add MIT License And Verify Ignore Rules

**Files:**
- Create: `LICENSE`
- Modify: `.gitignore` only if archive exclusions are missing

- [ ] **Step 1: Create MIT license**

Create `LICENSE` with:

```text
MIT License

Copyright (c) 2026 PeakGuard contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

- [ ] **Step 2: Ensure archives are ignored**

If `.gitignore` does not contain archive exclusions, add:

```gitignore
*.zip
*.7z
*.tar
*.tar.gz
PeakGuard-Release-*/
```

Keep existing build and binary exclusions:

```gitignore
build/
out/
bin/
obj/
*.exe
*.dll
*.pdb
*.log
snapshots/
.worktrees/
```

- [ ] **Step 3: Commit license and ignore update**

```powershell
git add LICENSE .gitignore
git commit -m "Add license and release ignore rules"
```

## Task 6: Historical Docs Triage

**Files:**
- Modify: historical files under `docs/superpowers/` only when they present old names as current commands

- [ ] **Step 1: Scan historical docs for old current commands**

Run:

```powershell
rg -n "cmake --build build --config .*Limiter|Install-CodexLimiter|Uninstall-CodexLimiter|%LOCALAPPDATA%\\CodexLimiter|Local\\\\CodexLimiterState" docs/superpowers
```

Expected: matches may exist in historical plans. Decide per match whether it is clearly historical or likely to mislead current workers.

- [ ] **Step 2: Add historical notice to old plans that retain old names**

For each old plan that remains intentionally historical and still contains old names, add this line near the top after the title:

```markdown
> Historical note: this document predates the PeakGuard rename. Current active commands and paths are documented in root `AGENTS.md` and `README.md`.
```

- [ ] **Step 3: Commit historical doc notes**

```powershell
git add docs/superpowers
git commit -m "Mark pre-rename planning docs as historical"
```

## Task 7: Full Build And Old-Name Verification

**Files:**
- Modify source/docs only if verification finds missed active old names

- [ ] **Step 1: Configure root build**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build -A x64
```

Expected: configure succeeds and lists PeakGuard targets without CMake errors.

- [ ] **Step 2: Build Debug tests**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target PeakGuardAudioTests
```

Expected: `PeakGuardAudioTests` builds successfully.

- [ ] **Step 3: Run Debug tests**

Run:

```powershell
.\build\tools\PeakGuardAudioTests\Debug\PeakGuardAudioTests.exe
```

Expected:

```text
PeakGuardAudioTests passed
```

- [ ] **Step 4: Build Release tray**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Release --target PeakGuardTray
```

Expected: `PeakGuardTray.exe` builds under `build\tools\PeakGuardTray\Release\`.

- [ ] **Step 5: Build state probe**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target PeakGuardStateProbe
```

Expected: `PeakGuardStateProbe.exe` builds under `build\tools\PeakGuardStateProbe\Debug\`.

- [ ] **Step 6: Scan active repo for old names**

Run:

```powershell
rg -n "CodexLimiter|Codex Limiter|codex-limiter|CodexLimiterState|CodexLimiterTrayInstance|CodexLimiterTrayWindow|CodexLimiterMeter|CodexLimiterSlider|CodexLimiterOverlay|LimiterTray|LimiterAudioTests|LimiterStateProbe|LimiterSharedState|Install-CodexLimiter|Uninstall-CodexLimiter" CMakeLists.txt README.md AGENTS.md common tools scripts
```

Expected: no matches.

- [ ] **Step 7: Check whitespace and untracked files**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors. `git status --short` should show only intended source/doc/license/script changes.

- [ ] **Step 8: Commit verification fixes**

If this task required fixes:

```powershell
git add CMakeLists.txt README.md AGENTS.md common tools scripts docs LICENSE .gitignore
git commit -m "Fix PeakGuard rename verification issues"
```

If no fixes were required, do not create an empty commit.

## Task 8: Prepare Local Release Archive Without Publishing

**Files:**
- Do not commit generated archives or release folders

- [ ] **Step 1: Create local release staging folder**

Run:

```powershell
$releaseDir = Join-Path $PWD 'PeakGuard-Release-v1.0.0'
if (Test-Path -LiteralPath $releaseDir) {
    Remove-Item -LiteralPath $releaseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseDir | Out-Null
Copy-Item -LiteralPath 'build\tools\PeakGuardTray\Release\PeakGuardTray.exe' -Destination $releaseDir
Copy-Item -LiteralPath 'scripts\Install-PeakGuard.ps1' -Destination $releaseDir
Copy-Item -LiteralPath 'scripts\Uninstall-PeakGuard.ps1' -Destination $releaseDir
Copy-Item -LiteralPath 'README.md' -Destination $releaseDir
Copy-Item -LiteralPath 'LICENSE' -Destination $releaseDir
```

Expected: `PeakGuard-Release-v1.0.0` contains exactly the exe, install script, uninstall script, README, and LICENSE.

- [ ] **Step 2: Zip local release folder**

Run:

```powershell
Compress-Archive -LiteralPath 'PeakGuard-Release-v1.0.0' -DestinationPath 'PeakGuard-Release-v1.0.0.zip' -Force
```

Expected: `PeakGuard-Release-v1.0.0.zip` exists and is ignored by git.

- [ ] **Step 3: Verify release artifacts are ignored**

Run:

```powershell
git status --short
```

Expected: release folder and zip do not appear.

- [ ] **Step 4: Manual publishing handoff**

Report these manual commands and actions instead of running them. Ask the user for the exact GitHub repository URL first, then substitute it through `$RepositoryUrl`:

```powershell
$RepositoryUrl = Read-Host 'GitHub repository URL'
git remote add origin $RepositoryUrl
git branch -M main
git push -u origin main
```

Manual GitHub release fields:

```text
Tag: v1.0.0
Title: PeakGuard v1.0.0 - Initial Release
Asset: PeakGuard-Release-v1.0.0.zip
```

Do not run these commands unless the user supplies the repository URL and explicitly asks to push.

## Final Review Checklist

- [ ] `rg` over active repo finds no old Codex Limiter identifiers.
- [ ] `PeakGuardAudioTests` builds and passes.
- [ ] `PeakGuardTray` Release builds.
- [ ] `PeakGuardStateProbe` Debug builds.
- [ ] Installer and uninstaller are renamed and reference PeakGuard paths/processes only.
- [ ] README is user-focused and documents VB-CABLE installer behavior.
- [ ] `LICENSE` exists.
- [ ] `.gitignore` excludes release folders and archives.
- [ ] Install/uninstall/VB-CABLE/GitHub publish steps are documented as manual side-effecting checks.

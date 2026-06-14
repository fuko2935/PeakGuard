# APO Feasibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove that a custom APO can be built, installed, audibly processed, detected, and cleanly removed on the user's built-in speakers and Havit H655BT before building the full limiter.

**Architecture:** This milestone is a reversible feasibility spike, not the final limiter. It vendors Microsoft's SysVAD/SwapAPO sample as the APO base, patches it to apply an obvious `-20 dB` diagnostic gain, adds local endpoint/snapshot/recovery tooling, and gates all device tests with backup and restore scripts.

**Tech Stack:** C++, PowerShell, Windows SDK, Windows Driver Kit, Visual Studio/MSBuild, Windows MMDevice API, Microsoft SysVAD/SwapAPO sample.

---

## Primary References

- Microsoft APO architecture: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- Microsoft APO implementation guide: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- Microsoft sample audio drivers: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/sample-audio-drivers
- Microsoft SysVAD sample: https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- Microsoft `IAudioProcessingObject`: https://learn.microsoft.com/en-us/windows/win32/api/audioenginebaseapo/nn-audioenginebaseapo-iaudioprocessingobject
- FxProperties notes used only for endpoint-scoped feasibility binding: https://github.com/dechamps/APO

## Scope

This plan implements only the APO feasibility milestone from the approved design spec. It does not build the final limiter, tray UI, lookahead DSP, settings storage, or virtual-device fallback.

Stop criteria:

- If built-in speakers fail the diagnostic APO test after one clean repair attempt, stop the APO path and write a fallback plan for virtual-device routing.
- If built-in speakers pass but Havit H655BT fails in stereo/music mode, ask the user whether to continue with APO for speakers only or pivot to the virtual-device fallback.

## File Structure

- `.gitignore`: Keeps build outputs, downloaded samples, snapshots, and local logs out of git.
- `docs/references/apo-primary-sources.md`: Local notes and links to the official APO sources used by the plan.
- `docs/superpowers/plans/2026-06-14-apo-feasibility.md`: This implementation plan.
- `docs/feasibility/apo-feasibility-report.md`: Filled during execution with exact device test results.
- `scripts/Check-ApoDevEnvironment.ps1`: Verifies Visual Studio, MSBuild, WDK, Windows SDK, and admin status.
- `scripts/Vendor-SysvadSwapApo.ps1`: Sparse-checks out Microsoft Windows driver samples and records the exact commit.
- `scripts/Patch-SwapApoDiagnosticGain.ps1`: Applies the reversible `-20 dB` diagnostic gain patch to the vendored SwapAPO source.
- `scripts/Get-AudioEndpointSnapshot.ps1`: Exports endpoint registry state before any APO binding.
- `scripts/Set-DiagnosticApoEndpoint.ps1`: Applies or restores endpoint-scoped diagnostic APO FxProperties for a single render endpoint.
- `scripts/Restart-WindowsAudio.ps1`: Restarts Windows audio services after registry changes.
- `tools/EndpointProbe/CMakeLists.txt`: Build file for the endpoint probe.
- `tools/EndpointProbe/main.cpp`: Lists render endpoints, prints the default endpoint, and watches device-change notifications.

## Safety Rules For Execution

- Do not modify endpoint registry without first running `scripts/Get-AudioEndpointSnapshot.ps1`.
- Do not run `scripts/Set-DiagnosticApoEndpoint.ps1 -Mode Install` unless the user is present and audio can be tested immediately.
- Always test restore on the same endpoint before moving to the next endpoint.
- Treat a normal-looking registry write as insufficient. The pass condition is audible `-20 dB` processing or a clearly documented failure.
- Keep every endpoint-specific change scoped to one endpoint GUID at a time.

### Task 1: Project Hygiene And Source Notes

**Files:**
- Create: `.gitignore`
- Create: `docs/references/apo-primary-sources.md`
- Modify: none
- Test: `git status --short`

- [ ] **Step 1: Write `.gitignore`**

Create `.gitignore` with exactly:

```gitignore
# Build outputs
build/
out/
bin/
obj/
*.user
*.suo
*.vcxproj.user
*.log
*.etl
*.pdb
*.ilk
*.dll
*.exe
*.lib
*.exp

# Vendored external samples are fetched by script
external/

# Local machine state
snapshots/
docs/feasibility/*.local.md
```

- [ ] **Step 2: Write source notes**

Create `docs/references/apo-primary-sources.md` with exactly:

```markdown
# APO Primary Sources

This project uses official Microsoft APO documentation first, and uses community notes only to understand endpoint-scoped `FxProperties` behavior seen in tools such as Equalizer APO.

## Official Microsoft Sources

- APO architecture: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture
- APO implementation guide: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/implementing-audio-processing-objects
- Sample audio drivers: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/sample-audio-drivers
- SysVAD sample: https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad
- `IAudioProcessingObject`: https://learn.microsoft.com/en-us/windows/win32/api/audioenginebaseapo/nn-audioenginebaseapo-iaudioprocessingobject

## Constraints Captured From Microsoft Docs

- APOs are COM-based, real-time, in-process objects.
- Real-time APO processing methods must not block.
- APO processing code must not rely on paged memory.
- APOs must avoid significant latency in the audio chain.
- Microsoft recommends the SysVAD SwapAPO sample as the implementation starting point.

## Endpoint Binding Notes

Official componentized APO installation is driver-owned. This feasibility milestone also tests endpoint-scoped `FxProperties` binding because the target is the user's existing laptop and Bluetooth endpoints.

The endpoint-scoped binding is guarded by registry export, one-endpoint-at-a-time changes, audio-service restart, audible validation, and restore testing.
```

- [ ] **Step 3: Run status check**

Run:

```powershell
git status --short
```

Expected:

```text
?? .gitignore
?? docs/references/
```

- [ ] **Step 4: Commit**

Run:

```powershell
git add .gitignore docs/references/apo-primary-sources.md
git commit -m "chore: add APO source notes and hygiene"
```

Expected: commit succeeds.

### Task 2: Development Environment Preflight

**Files:**
- Create: `scripts/Check-ApoDevEnvironment.ps1`
- Test: `scripts/Check-ApoDevEnvironment.ps1`

- [ ] **Step 1: Write the preflight script**

Create `scripts/Check-ApoDevEnvironment.ps1` with exactly:

```powershell
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

function Write-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $status = if ($Passed) { 'PASS' } else { 'FAIL' }
    Write-Host "[$status] $Name - $Detail"
    if (-not $Passed) {
        $script:failed = $true
    }
}

function Value-OrDefault {
    param(
        [object]$Value,
        [string]$Default
    )

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return $Default
    }
    return [string]$Value
}

$script:failed = $false

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
Write-Check -Name 'Administrator shell' -Passed $isAdmin -Detail 'Required only for install/restore steps; build steps can run without admin.'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
Write-Check -Name 'vswhere.exe' -Passed (Test-Path $vswhere) -Detail $vswhere

$vsInstall = $null
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
Write-Check -Name 'Visual C++ tools' -Passed (-not [string]::IsNullOrWhiteSpace($vsInstall)) -Detail (Value-OrDefault $vsInstall 'not found')

$msbuild = $null
if ($vsInstall) {
    $candidate = Join-Path $vsInstall 'MSBuild\Current\Bin\MSBuild.exe'
    if (Test-Path $candidate) {
        $msbuild = $candidate
    }
}
Write-Check -Name 'MSBuild.exe' -Passed ($null -ne $msbuild) -Detail (Value-OrDefault $msbuild 'not found')

$kitsRoot = 'C:\Program Files (x86)\Windows Kits\10'
$apoHeader = Get-ChildItem -Path (Join-Path $kitsRoot 'Include') -Filter 'audioenginebaseapo.h' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
Write-Check -Name 'Windows SDK APO header' -Passed ($null -ne $apoHeader) -Detail (Value-OrDefault $apoHeader.FullName 'audioenginebaseapo.h not found')

$stampInf = Get-ChildItem -Path (Join-Path $kitsRoot 'bin') -Filter 'stampinf.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
Write-Check -Name 'WDK stampinf.exe' -Passed ($null -ne $stampInf) -Detail (Value-OrDefault $stampInf.FullName 'stampinf.exe not found')

$git = Get-Command git -ErrorAction SilentlyContinue
Write-Check -Name 'git' -Passed ($null -ne $git) -Detail (Value-OrDefault $git.Source 'not found')

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
Write-Check -Name 'cmake' -Passed ($null -ne $cmake) -Detail (Value-OrDefault $cmake.Source 'not found')

if ($script:failed) {
    Write-Error 'APO development environment preflight failed. Install Visual Studio C++ workload, Windows SDK, WDK, Git, and CMake before continuing.'
}

Write-Host 'APO development environment preflight completed.'
```

- [ ] **Step 2: Run the preflight**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Check-ApoDevEnvironment.ps1
```

Expected:

```text
[PASS] Visual C++ tools - ...
[PASS] MSBuild.exe - ...
[PASS] Windows SDK APO header - ...
[PASS] WDK stampinf.exe - ...
[PASS] git - ...
[PASS] cmake - ...
APO development environment preflight completed.
```

`Administrator shell` may show `FAIL` during this task. Continue only for non-install steps. Before Task 8, rerun this script from an elevated PowerShell and require `Administrator shell` to pass.

- [ ] **Step 3: Commit**

Run:

```powershell
git add scripts/Check-ApoDevEnvironment.ps1
git commit -m "chore: add APO development preflight"
```

Expected: commit succeeds.

### Task 3: Endpoint Snapshot And Restore Tooling

**Files:**
- Create: `scripts/Get-AudioEndpointSnapshot.ps1`
- Create: `scripts/Set-DiagnosticApoEndpoint.ps1`
- Create: `scripts/Restart-WindowsAudio.ps1`
- Test: `powershell -ExecutionPolicy Bypass -File .\scripts\Get-AudioEndpointSnapshot.ps1`

- [ ] **Step 1: Write endpoint snapshot script**

Create `scripts/Get-AudioEndpointSnapshot.ps1` with exactly:

```powershell
[CmdletBinding()]
param(
    [string]$OutputDirectory = 'snapshots'
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$renderPath = 'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'
$capturePath = 'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture'

$renderFile = Join-Path $OutputDirectory "render-$timestamp.reg"
$captureFile = Join-Path $OutputDirectory "capture-$timestamp.reg"

& reg.exe export $renderPath $renderFile /y | Out-Host
& reg.exe export $capturePath $captureFile /y | Out-Host

$summary = [ordered]@{
    CreatedAt = (Get-Date).ToString('o')
    RenderRegistryExport = (Resolve-Path $renderFile).Path
    CaptureRegistryExport = (Resolve-Path $captureFile).Path
}

$jsonFile = Join-Path $OutputDirectory "snapshot-$timestamp.json"
$summary | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 $jsonFile

Write-Host "Snapshot written: $jsonFile"
```

- [ ] **Step 2: Write endpoint APO install/restore script**

Create `scripts/Set-DiagnosticApoEndpoint.ps1` with exactly:

```powershell
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Install', 'Restore')]
    [string]$Mode,

    [Parameter(Mandatory)]
    [string]$EndpointIdOrGuid,

    [string]$ApoClsid,

    [string]$BackupDirectory = 'snapshots'
)

$ErrorActionPreference = 'Stop'

function Require-Admin {
    $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator
    )
    if (-not $isAdmin) {
        throw 'Run this script from an elevated PowerShell session.'
    }
}

function Normalize-EndpointGuid {
    param([string]$Value)
    if ($Value -match '\{[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}$') {
        return $Matches[0].ToUpperInvariant()
    }
    throw "Could not find endpoint GUID in '$Value'. Expected a GUID such as {01234567-89AB-CDEF-0123-456789ABCDEF}."
}

function Read-PropertyState {
    param(
        [string]$Path,
        [string]$Name
    )
    $item = Get-ItemProperty -LiteralPath $Path -ErrorAction Stop
    $property = $item.PSObject.Properties | Where-Object { $_.Name -eq $Name } | Select-Object -First 1
    if ($null -eq $property) {
        return [ordered]@{ Exists = $false; Value = $null; Type = $null }
    }
    $kind = (Get-Item -LiteralPath $Path).GetValueKind($Name).ToString()
    return [ordered]@{ Exists = $true; Value = $property.Value; Type = $kind }
}

function Restore-PropertyState {
    param(
        [string]$Path,
        [string]$Name,
        [object]$State
    )
    if ($State.Exists) {
        New-ItemProperty -LiteralPath $Path -Name $Name -Value $State.Value -PropertyType $State.Type -Force | Out-Null
    } else {
        Remove-ItemProperty -LiteralPath $Path -Name $Name -ErrorAction SilentlyContinue
    }
}

Require-Admin
New-Item -ItemType Directory -Force -Path $BackupDirectory | Out-Null

$endpointGuid = Normalize-EndpointGuid $EndpointIdOrGuid
$fxPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render\$endpointGuid\FxProperties"
if (-not (Test-Path $fxPath)) {
    throw "FxProperties path not found: $fxPath"
}

$streamEffect = '{D04E05A6-594B-4fb6-A80D-01AF5EED7D1D},5'
$modeEffect = '{D04E05A6-594B-4fb6-A80D-01AF5EED7D1D},6'
$endpointEffect = '{D04E05A6-594B-4fb6-A80D-01AF5EED7D1D},7'
$disableSysFx = '{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5'
$backupFile = Join-Path $BackupDirectory ("fxproperties-" + $endpointGuid.Trim('{}') + ".json")

if ($Mode -eq 'Install') {
    if ([string]::IsNullOrWhiteSpace($ApoClsid)) {
        throw '-ApoClsid is required for Install mode.'
    }
    if ($ApoClsid -notmatch '^\{[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}$') {
        throw '-ApoClsid must include braces, for example {EC1CC9CE-FAED-4822-828A-82A81A6F018F}.'
    }

    $backup = [ordered]@{
        EndpointGuid = $endpointGuid
        CreatedAt = (Get-Date).ToString('o')
        FxPath = $fxPath
        Properties = [ordered]@{
            StreamEffect = Read-PropertyState -Path $fxPath -Name $streamEffect
            ModeEffect = Read-PropertyState -Path $fxPath -Name $modeEffect
            EndpointEffect = Read-PropertyState -Path $fxPath -Name $endpointEffect
            DisableSysFx = Read-PropertyState -Path $fxPath -Name $disableSysFx
        }
    }
    $backup | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $backupFile

    if ($PSCmdlet.ShouldProcess($endpointGuid, "Install diagnostic APO $ApoClsid")) {
        New-ItemProperty -LiteralPath $fxPath -Name $streamEffect -Value $ApoClsid -PropertyType String -Force | Out-Null
        New-ItemProperty -LiteralPath $fxPath -Name $disableSysFx -Value 0 -PropertyType DWord -Force | Out-Null
        Write-Host "Installed diagnostic StreamEffect CLSID $ApoClsid on endpoint $endpointGuid"
        Write-Host "Backup written: $backupFile"
    }
    return
}

if (-not (Test-Path $backupFile)) {
    throw "Backup not found: $backupFile"
}

$restore = Get-Content -Raw $backupFile | ConvertFrom-Json
if ($PSCmdlet.ShouldProcess($endpointGuid, 'Restore original endpoint FxProperties')) {
    Restore-PropertyState -Path $fxPath -Name $streamEffect -State $restore.Properties.StreamEffect
    Restore-PropertyState -Path $fxPath -Name $modeEffect -State $restore.Properties.ModeEffect
    Restore-PropertyState -Path $fxPath -Name $endpointEffect -State $restore.Properties.EndpointEffect
    Restore-PropertyState -Path $fxPath -Name $disableSysFx -State $restore.Properties.DisableSysFx
    Write-Host "Restored endpoint $endpointGuid from $backupFile"
}
```

- [ ] **Step 3: Write Windows audio restart script**

Create `scripts/Restart-WindowsAudio.ps1` with exactly:

```powershell
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin) {
    throw 'Run this script from an elevated PowerShell session.'
}

Write-Host 'Restarting Windows Audio service. Current audio playback will stop briefly.'
Restart-Service -Name Audiosrv -Force
Write-Host 'Windows Audio service restarted.'
```

- [ ] **Step 4: Run snapshot test**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Get-AudioEndpointSnapshot.ps1
```

Expected:

```text
The operation completed successfully.
The operation completed successfully.
Snapshot written: snapshots\snapshot-...
```

- [ ] **Step 5: Commit**

Run:

```powershell
git add scripts/Get-AudioEndpointSnapshot.ps1 scripts/Set-DiagnosticApoEndpoint.ps1 scripts/Restart-WindowsAudio.ps1
git commit -m "chore: add APO endpoint backup and restore scripts"
```

Expected: commit succeeds.

### Task 4: Endpoint Probe CLI

**Files:**
- Create: `tools/EndpointProbe/CMakeLists.txt`
- Create: `tools/EndpointProbe/main.cpp`
- Test: `cmake -S tools/EndpointProbe -B build/EndpointProbe -A x64`

- [ ] **Step 1: Write EndpointProbe CMake file**

Create `tools/EndpointProbe/CMakeLists.txt` with exactly:

```cmake
cmake_minimum_required(VERSION 3.20)
project(EndpointProbe LANGUAGES CXX)

add_executable(EndpointProbe main.cpp)
target_compile_features(EndpointProbe PRIVATE cxx_std_17)
target_compile_definitions(EndpointProbe PRIVATE UNICODE _UNICODE)
target_link_libraries(EndpointProbe PRIVATE ole32 uuid propsys)
```

- [ ] **Step 2: Write EndpointProbe source**

Create `tools/EndpointProbe/main.cpp` with exactly:

```cpp
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace {

void ThrowIfFailed(HRESULT hr, const wchar_t* operation) {
    if (FAILED(hr)) {
        std::wcerr << L"FAILED: " << operation << L" hr=0x" << std::hex << hr << std::dec << L"\n";
        ExitProcess(static_cast<UINT>(hr));
    }
}

std::wstring GetDeviceString(IMMDevice* device, const PROPERTYKEY& key) {
    ComPtr<IPropertyStore> store;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr)) {
        return L"<property-store-error>";
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(key, &value);
    if (FAILED(hr)) {
        PropVariantClear(&value);
        return L"<property-error>";
    }

    std::wstring result = L"<empty>";
    if (value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        result = value.pwszVal;
    }
    PropVariantClear(&value);
    return result;
}

std::wstring GetDeviceId(IMMDevice* device) {
    LPWSTR id = nullptr;
    ThrowIfFailed(device->GetId(&id), L"IMMDevice::GetId");
    std::wstring result = id;
    CoTaskMemFree(id);
    return result;
}

const wchar_t* StateName(DWORD state) {
    switch (state) {
    case DEVICE_STATE_ACTIVE:
        return L"Active";
    case DEVICE_STATE_DISABLED:
        return L"Disabled";
    case DEVICE_STATE_NOTPRESENT:
        return L"NotPresent";
    case DEVICE_STATE_UNPLUGGED:
        return L"Unplugged";
    default:
        return L"Unknown";
    }
}

void PrintDefault(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDevice> defaultDevice;
    HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    if (FAILED(hr)) {
        std::wcout << L"Default render endpoint: <none> hr=0x" << std::hex << hr << std::dec << L"\n";
        return;
    }

    std::wcout << L"Default render endpoint:\n";
    std::wcout << L"  Name: " << GetDeviceString(defaultDevice.Get(), PKEY_Device_FriendlyName) << L"\n";
    std::wcout << L"  ID:   " << GetDeviceId(defaultDevice.Get()) << L"\n";
}

void PrintEndpoints(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDeviceCollection> collection;
    ThrowIfFailed(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_NOTPRESENT | DEVICE_STATE_UNPLUGGED, &collection),
        L"IMMDeviceEnumerator::EnumAudioEndpoints");

    UINT count = 0;
    ThrowIfFailed(collection->GetCount(&count), L"IMMDeviceCollection::GetCount");
    std::wcout << L"Render endpoint count: " << count << L"\n";

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        ThrowIfFailed(collection->Item(i, &device), L"IMMDeviceCollection::Item");

        DWORD state = 0;
        ThrowIfFailed(device->GetState(&state), L"IMMDevice::GetState");

        std::wcout << L"[" << i << L"] " << GetDeviceString(device.Get(), PKEY_Device_FriendlyName) << L"\n";
        std::wcout << L"    State: " << StateName(state) << L"\n";
        std::wcout << L"    ID:    " << GetDeviceId(device.Get()) << L"\n";
    }
}

class NotificationClient final : public IMMNotificationClient {
public:
    explicit NotificationClient(IMMDeviceEnumerator* enumerator) : refCount_(1), enumerator_(enumerator) {}

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR deviceId) override {
        if (flow == eRender && role == eConsole) {
            std::wcout << L"\nDefault render endpoint changed: " << (deviceId ? deviceId : L"<null>") << L"\n";
            PrintDefault(enumerator_.Get());
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override {
        std::wcout << L"\nDevice added: " << (deviceId ? deviceId : L"<null>") << L"\n";
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override {
        std::wcout << L"\nDevice removed: " << (deviceId ? deviceId : L"<null>") << L"\n";
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override {
        std::wcout << L"\nDevice state changed: " << (deviceId ? deviceId : L"<null>") << L" -> " << StateName(newState) << L"\n";
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        return S_OK;
    }

private:
    ~NotificationClient() = default;
    volatile LONG refCount_;
    ComPtr<IMMDeviceEnumerator> enumerator_;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const bool watch = argc > 1 && std::wstring(argv[1]) == L"--watch";

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED), L"CoInitializeEx");

    ComPtr<IMMDeviceEnumerator> enumerator;
    ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)),
        L"CoCreateInstance(MMDeviceEnumerator)");

    PrintDefault(enumerator.Get());
    PrintEndpoints(enumerator.Get());

    if (watch) {
        NotificationClient* client = new NotificationClient(enumerator.Get());
        ThrowIfFailed(enumerator->RegisterEndpointNotificationCallback(client), L"RegisterEndpointNotificationCallback");
        std::wcout << L"\nWatching endpoint changes. Press Ctrl+C to exit.\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    CoUninitialize();
    return 0;
}
```

- [ ] **Step 3: Configure and build EndpointProbe**

Run:

```powershell
cmake -S .\tools\EndpointProbe -B .\build\EndpointProbe -A x64
cmake --build .\build\EndpointProbe --config Debug
```

Expected:

```text
Build succeeded.
```

- [ ] **Step 4: Run EndpointProbe**

Run:

```powershell
.\build\EndpointProbe\Debug\EndpointProbe.exe
```

Expected:

```text
Default render endpoint:
  Name: ...
  ID:   {0.0.0.00000000}.{...}
Render endpoint count: ...
```

- [ ] **Step 5: Test endpoint change watch**

Run:

```powershell
.\build\EndpointProbe\Debug\EndpointProbe.exe --watch
```

Change Windows output between built-in speakers and Havit H655BT.

Expected:

```text
Default render endpoint changed: ...
```

Stop with `Ctrl+C`.

- [ ] **Step 6: Commit**

Run:

```powershell
git add tools/EndpointProbe/CMakeLists.txt tools/EndpointProbe/main.cpp
git commit -m "feat: add render endpoint probe"
```

Expected: commit succeeds.

### Task 5: Vendor Microsoft SysVAD SwapAPO Sample

**Files:**
- Create: `scripts/Vendor-SysvadSwapApo.ps1`
- Test: `powershell -ExecutionPolicy Bypass -File .\scripts\Vendor-SysvadSwapApo.ps1`

- [ ] **Step 1: Write vendor script**

Create `scripts/Vendor-SysvadSwapApo.ps1` with exactly:

```powershell
[CmdletBinding()]
param(
    [string]$RepositoryUrl = 'https://github.com/microsoft/Windows-driver-samples.git',
    [string]$Destination = 'external/windows-driver-samples'
)

$ErrorActionPreference = 'Stop'

if (Test-Path $Destination) {
    Write-Host "Destination already exists: $Destination"
} else {
    git clone --filter=blob:none --sparse $RepositoryUrl $Destination
}

Push-Location $Destination
try {
    git sparse-checkout set audio/sysvad
    git fetch origin main
    git checkout main
    $commit = git rev-parse HEAD
} finally {
    Pop-Location
}

New-Item -ItemType Directory -Force -Path 'docs/references' | Out-Null
Set-Content -Encoding UTF8 -Path 'docs/references/windows-driver-samples-commit.txt' -Value $commit

Write-Host "Windows driver samples commit: $commit"
Write-Host "SysVAD sample path: $Destination/audio/sysvad"
```

- [ ] **Step 2: Run vendor script**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Vendor-SysvadSwapApo.ps1
```

Expected:

```text
Windows driver samples commit: ...
SysVAD sample path: external/windows-driver-samples/audio/sysvad
```

- [ ] **Step 3: Verify key sample files exist**

Run:

```powershell
Test-Path .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\swapaposfx.cpp
Test-Path .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\swapapomfx.cpp
Test-Path .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\SwapAPO.vcxproj
```

Expected:

```text
True
True
True
```

- [ ] **Step 4: Commit tracked vendor metadata only**

Run:

```powershell
git add scripts/Vendor-SysvadSwapApo.ps1 docs/references/windows-driver-samples-commit.txt
git commit -m "chore: add SysVAD vendor script"
```

Expected: commit succeeds. `external/` remains ignored by git.

### Task 6: Build Unmodified SwapAPO

**Files:**
- Modify: none tracked
- Test: MSBuild target for `SwapAPO.vcxproj`

- [ ] **Step 1: Locate MSBuild**

Run:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin\MSBuild.exe'
$msbuild
```

Expected: prints a path ending in `MSBuild.exe`.

- [ ] **Step 2: Build unmodified SwapAPO**

Run:

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsInstall 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\SwapAPO.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

Expected:

```text
Build succeeded.
```

- [ ] **Step 3: Locate DLL**

Run:

```powershell
Get-ChildItem .\external\windows-driver-samples\audio\sysvad -Recurse -Filter SwapAPO.dll | Select-Object -ExpandProperty FullName
```

Expected: at least one `SwapAPO.dll` path.

- [ ] **Step 4: Commit build-note file**

Create `docs/feasibility/apo-feasibility-report.md` with exactly:

```markdown
# APO Feasibility Report

## Build

- Unmodified Microsoft SwapAPO build: PASS
- Diagnostic SwapAPO build: not run

## Endpoint Probe

- Built-in speakers endpoint ID: not recorded
- Havit H655BT endpoint ID: not recorded

## Diagnostic APO Tests

- Built-in speakers `-20 dB` audible test: not run
- Havit H655BT stereo/music `-20 dB` audible test: not run

## Restore Tests

- Built-in speakers restore: not run
- Havit H655BT restore: not run

## Decision

- APO path decision: not reached
```

Run:

```powershell
git add docs/feasibility/apo-feasibility-report.md
git commit -m "docs: start APO feasibility report"
```

Expected: commit succeeds.

### Task 7: Patch SwapAPO With Diagnostic Gain

**Files:**
- Create: `scripts/Patch-SwapApoDiagnosticGain.ps1`
- Test: rebuild SwapAPO

- [ ] **Step 1: Write diagnostic patch script**

Create `scripts/Patch-SwapApoDiagnosticGain.ps1` with exactly:

```powershell
[CmdletBinding()]
param(
    [string]$SwapApoDirectory = 'external/windows-driver-samples/audio/sysvad/APO/SwapAPO'
)

$ErrorActionPreference = 'Stop'

$files = @(
    (Join-Path $SwapApoDirectory 'swapaposfx.cpp'),
    (Join-Path $SwapApoDirectory 'swapapomfx.cpp')
)

$helper = @'

static void ApplyDiagnosticGain(FLOAT32* frames, UINT32 frameCount, UINT32 samplesPerFrame)
{
    constexpr FLOAT32 kDiagnosticGain = 0.1f; // -20 dB
    const UINT32 sampleCount = frameCount * samplesPerFrame;
    for (UINT32 i = 0; i < sampleCount; ++i)
    {
        frames[i] *= kDiagnosticGain;
    }
}
'@

foreach ($file in $files) {
    if (-not (Test-Path $file)) {
        throw "Missing file: $file"
    }

    $text = Get-Content -Raw $file

    if ($text -notmatch 'ApplyDiagnosticGain') {
        $text = $text -replace '#pragma AVRT_CODE_BEGIN', ($helper + "`r`n#pragma AVRT_CODE_BEGIN")
    }

    if ($text -notmatch 'ApplyDiagnosticGain\(pf32InputFrames,') {
        $needle = '// copy the memory only if there is an output connection, and input/output pointers are unequal'
        $insert = @'
        ApplyDiagnosticGain(pf32InputFrames,
            ppInputConnections[0]->u32ValidFrameCount,
            GetSamplesPerFrame());

'@
        if ($text -notmatch [regex]::Escape($needle)) {
            throw "Could not find copy marker in $file"
        }
        $text = $text.Replace($needle, $insert + '        ' + $needle)
    }

    Set-Content -Encoding UTF8 -Path $file -Value $text
    Write-Host "Patched $file"
}
```

- [ ] **Step 2: Apply diagnostic patch**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Patch-SwapApoDiagnosticGain.ps1
```

Expected:

```text
Patched external/windows-driver-samples/audio/sysvad/APO/SwapAPO\swapaposfx.cpp
Patched external/windows-driver-samples/audio/sysvad/APO/SwapAPO\swapapomfx.cpp
```

- [ ] **Step 3: Rebuild diagnostic SwapAPO**

Run:

```powershell
& $msbuild .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\SwapAPO.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

Expected:

```text
Build succeeded.
```

- [ ] **Step 4: Update feasibility report build status**

Edit `docs/feasibility/apo-feasibility-report.md` so the build section is exactly:

```markdown
## Build

- Unmodified Microsoft SwapAPO build: PASS
- Diagnostic SwapAPO build: PASS
```

- [ ] **Step 5: Commit**

Run:

```powershell
git add scripts/Patch-SwapApoDiagnosticGain.ps1 docs/feasibility/apo-feasibility-report.md
git commit -m "chore: add diagnostic SwapAPO patch flow"
```

Expected: commit succeeds.

### Task 8: Built-In Speaker Diagnostic APO Test

**Files:**
- Modify: `docs/feasibility/apo-feasibility-report.md`
- Test: audible `-20 dB` change on built-in speakers, then restore

- [ ] **Step 1: Rerun preflight elevated**

Open an elevated PowerShell in the repo root and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Check-ApoDevEnvironment.ps1
```

Expected includes:

```text
[PASS] Administrator shell - Required only for install/restore steps; build steps can run without admin.
```

- [ ] **Step 2: Capture registry snapshot**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Get-AudioEndpointSnapshot.ps1
```

Expected: snapshot files are written under `snapshots/`.

- [ ] **Step 3: Record built-in speaker endpoint ID**

Run:

```powershell
.\build\EndpointProbe\Debug\EndpointProbe.exe
```

Copy the default render endpoint ID for the built-in speakers into `docs/feasibility/apo-feasibility-report.md`:

```markdown
## Endpoint Probe

- Built-in speakers endpoint ID: paste the exact built-in speaker ID printed by EndpointProbe
- Havit H655BT endpoint ID: not recorded
```

Before continuing, set a shell variable from the exact ID printed by `EndpointProbe`:

```powershell
$builtInEndpoint = Read-Host 'Paste the built-in speaker endpoint ID printed by EndpointProbe'
```

- [ ] **Step 4: Register diagnostic SwapAPO COM DLL**

Find the diagnostic `SwapAPO.dll` path:

```powershell
$swapApoDll = Get-ChildItem .\external\windows-driver-samples\audio\sysvad -Recurse -Filter SwapAPO.dll | Select-Object -First 1 -ExpandProperty FullName
$swapApoDll
```

Register it:

```powershell
& regsvr32.exe /s $swapApoDll
if ($LASTEXITCODE -ne 0) { throw "regsvr32 failed with exit code $LASTEXITCODE" }
```

Expected: no output and no exception.

- [ ] **Step 5: Install diagnostic APO on built-in speaker endpoint**

Extract the SFX CLSID from the sample registration file:

```powershell
$rgs = Get-Content -Raw .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\SwapAPOSFX.rgs
if ($rgs -notmatch '\{[0-9A-Fa-f-]{36}\}') { throw 'Could not find SFX CLSID in SwapAPOSFX.rgs' }
$sfxClsid = $Matches[0].ToUpperInvariant()
$sfxClsid
```

Install on the built-in speaker endpoint:

```powershell
if ([string]::IsNullOrWhiteSpace($builtInEndpoint)) {
    $builtInEndpoint = Read-Host 'Paste the built-in speaker endpoint ID printed by EndpointProbe'
}
powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Install -EndpointIdOrGuid $builtInEndpoint -ApoClsid $sfxClsid
powershell -ExecutionPolicy Bypass -File .\scripts\Restart-WindowsAudio.ps1
```

Expected:

```text
Installed diagnostic StreamEffect CLSID ...
Windows Audio service restarted.
```

- [ ] **Step 6: Run audible built-in speaker test**

Play stable audio through the built-in speakers, such as a browser video or local audio file.

Expected: audio is clearly quieter by about `-20 dB`. If no clear drop is heard, mark the speaker diagnostic as `FAIL`.

Update `docs/feasibility/apo-feasibility-report.md`:

```markdown
## Diagnostic APO Tests

- Built-in speakers `-20 dB` audible test: PASS
- Havit H655BT stereo/music `-20 dB` audible test: not run
```

Use `FAIL` instead of `PASS` if the change is not obvious.

- [ ] **Step 7: Restore built-in speaker endpoint**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Restore -EndpointIdOrGuid $builtInEndpoint
powershell -ExecutionPolicy Bypass -File .\scripts\Restart-WindowsAudio.ps1
```

Expected:

```text
Restored endpoint ...
Windows Audio service restarted.
```

Play the same audio again.

Expected: normal volume returns.

Update restore section:

```markdown
## Restore Tests

- Built-in speakers restore: PASS
- Havit H655BT restore: not run
```

- [ ] **Step 8: Commit built-in speaker result**

Run:

```powershell
git add docs/feasibility/apo-feasibility-report.md
git commit -m "test: record built-in speaker APO feasibility"
```

Expected: commit succeeds.

### Task 9: Havit H655BT Diagnostic APO Test

**Files:**
- Modify: `docs/feasibility/apo-feasibility-report.md`
- Test: audible `-20 dB` change on Havit H655BT stereo/music endpoint, then restore

- [ ] **Step 1: Put Havit H655BT in stereo/music mode**

Connect Havit H655BT. In Windows sound output, select the normal stereo/music playback endpoint, not hands-free/headset mode.

Run:

```powershell
.\build\EndpointProbe\Debug\EndpointProbe.exe
```

Expected: default render endpoint name matches Havit H655BT or the selected Bluetooth stereo output.

- [ ] **Step 2: Record Havit endpoint ID**

Update `docs/feasibility/apo-feasibility-report.md`:

```markdown
## Endpoint Probe

- Built-in speakers endpoint ID: keep the exact built-in speaker ID recorded in Task 8
- Havit H655BT endpoint ID: paste the exact Havit H655BT ID printed by EndpointProbe
```

Before continuing, set a shell variable from the exact ID printed by `EndpointProbe`:

```powershell
$havitEndpoint = Read-Host 'Paste the Havit H655BT endpoint ID printed by EndpointProbe'
```

- [ ] **Step 3: Capture registry snapshot**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Get-AudioEndpointSnapshot.ps1
```

Expected: a fresh snapshot is written.

- [ ] **Step 4: Install diagnostic APO on Havit endpoint**

Run:

```powershell
$rgs = Get-Content -Raw .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\SwapAPOSFX.rgs
if ($rgs -notmatch '\{[0-9A-Fa-f-]{36}\}') { throw 'Could not find SFX CLSID in SwapAPOSFX.rgs' }
$sfxClsid = $Matches[0].ToUpperInvariant()
if ([string]::IsNullOrWhiteSpace($havitEndpoint)) {
    $havitEndpoint = Read-Host 'Paste the Havit H655BT endpoint ID printed by EndpointProbe'
}
powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Install -EndpointIdOrGuid $havitEndpoint -ApoClsid $sfxClsid
powershell -ExecutionPolicy Bypass -File .\scripts\Restart-WindowsAudio.ps1
```

Expected:

```text
Installed diagnostic StreamEffect CLSID ...
Windows Audio service restarted.
```

- [ ] **Step 5: Run audible Havit test**

Play stable audio through Havit H655BT.

Expected: audio is clearly quieter by about `-20 dB`. If no clear drop is heard, mark the Havit diagnostic as `FAIL`.

Update `docs/feasibility/apo-feasibility-report.md`:

```markdown
## Diagnostic APO Tests

- Built-in speakers `-20 dB` audible test: PASS
- Havit H655BT stereo/music `-20 dB` audible test: PASS
```

Use the actual built-in speaker result already recorded, and use `FAIL` for Havit if the change is not obvious.

- [ ] **Step 6: Restore Havit endpoint**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Restore -EndpointIdOrGuid $havitEndpoint
powershell -ExecutionPolicy Bypass -File .\scripts\Restart-WindowsAudio.ps1
```

Expected: normal Havit volume returns.

Update restore section:

```markdown
## Restore Tests

- Built-in speakers restore: PASS
- Havit H655BT restore: PASS
```

Use `FAIL` for Havit restore only if normal volume does not return after restore and service restart.

- [ ] **Step 7: Commit Havit result**

Run:

```powershell
git add docs/feasibility/apo-feasibility-report.md
git commit -m "test: record Havit APO feasibility"
```

Expected: commit succeeds.

### Task 10: Feasibility Decision And Review

**Files:**
- Modify: `docs/feasibility/apo-feasibility-report.md`
- Test: final restore and clean git status

- [ ] **Step 1: Confirm no diagnostic APO remains installed**

Run restore for both endpoint IDs recorded in the report:

```powershell
$builtInEndpoint = Read-Host 'Paste the built-in speaker endpoint ID from docs/feasibility/apo-feasibility-report.md'
$havitEndpoint = Read-Host 'Paste the Havit H655BT endpoint ID from docs/feasibility/apo-feasibility-report.md, or press Enter if Havit was not tested'
powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Restore -EndpointIdOrGuid $builtInEndpoint
if (-not [string]::IsNullOrWhiteSpace($havitEndpoint)) {
    powershell -ExecutionPolicy Bypass -File .\scripts\Set-DiagnosticApoEndpoint.ps1 -Mode Restore -EndpointIdOrGuid $havitEndpoint
}
powershell -ExecutionPolicy Bypass -File .\scripts\Restart-WindowsAudio.ps1
```

Expected: both endpoints restore without error, and audio returns at normal volume.

- [ ] **Step 2: Write final decision**

If both built-in speakers and Havit pass, set decision section to:

```markdown
## Decision

- APO path decision: PASS
- Next plan: build reusable limiter DSP core, APO settings bridge, and minimal tray UI.
```

If built-in speakers fail, set decision section to:

```markdown
## Decision

- APO path decision: FAIL
- Reason: built-in speakers did not audibly process the diagnostic APO.
- Next plan: pivot to virtual-audio-device routing with reusable limiter DSP core.
```

If built-in speakers pass and Havit fails, set decision section to:

```markdown
## Decision

- APO path decision: PARTIAL
- Reason: built-in speakers processed the diagnostic APO, but Havit H655BT did not in stereo/music mode.
- Next plan: ask the user whether to continue with APO for speakers only or pivot to virtual-audio-device routing.
```

- [ ] **Step 3: Review real-time APO patch**

Run:

```powershell
Select-String -Path .\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\swapaposfx.cpp,.\external\windows-driver-samples\audio\sysvad\APO\SwapAPO\swapapomfx.cpp -Pattern 'ApplyDiagnosticGain|new |malloc|CreateFile|Sleep|WaitFor|std::|fstream|cout|cerr'
```

Expected: matches for `ApplyDiagnosticGain`; no matches for heap allocation, file I/O, waits, or console I/O introduced by this project inside the processing files.

- [ ] **Step 4: Check git status**

Run:

```powershell
git status --short --branch
```

Expected:

```text
## main
```

or only `docs/feasibility/apo-feasibility-report.md` modified before the final commit.

- [ ] **Step 5: Commit final feasibility report**

Run:

```powershell
git add docs/feasibility/apo-feasibility-report.md
git commit -m "docs: finalize APO feasibility decision"
```

Expected: commit succeeds unless the report was already committed with the final decision.

## Completion Criteria

This plan is complete when:

- The environment preflight is documented.
- Endpoint probe builds and detects output-device changes.
- Microsoft SwapAPO builds unmodified.
- Diagnostic `-20 dB` SwapAPO builds.
- Built-in speaker test is recorded as `PASS` or `FAIL`.
- Havit H655BT test is recorded as `PASS`, `FAIL`, or skipped because built-in speakers already failed.
- All endpoint changes are restored.
- `docs/feasibility/apo-feasibility-report.md` contains a final `PASS`, `FAIL`, or `PARTIAL` decision.

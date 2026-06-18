<#
.SYNOPSIS
    Installs PeakGuard (WASAPI loopback mode).
    Builds PeakGuardTray, copies to LocalAppData, creates a Start Menu shortcut, optionally sets HKCU startup, launches.
    Run from repo root. Elevated not required for build, but HKCU Run key works without admin.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$EnableStartup
)

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$rootBuildDir = Join-Path $repoRoot 'build'
$directBuildDir = Join-Path $repoRoot 'build/PeakGuardTray'
$rootBuildExe = Join-Path $rootBuildDir "tools/PeakGuardTray/$Configuration/PeakGuardTray.exe"
$directBuildExe = Join-Path $directBuildDir "$Configuration/PeakGuardTray.exe"
$installDir = Join-Path $env:LOCALAPPDATA 'PeakGuard'
$exeDest = Join-Path $installDir 'PeakGuardTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\PeakGuard'
$shortcutPath = Join-Path $startMenuDir 'PeakGuard.lnk'
$runKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'PeakGuard'

function Resolve-RequiredPath {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path. Build first with: cmake --build build --config $Configuration --target PeakGuardTray"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Resolve-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -eq $cmake) {
        # Try VS bundled cmake
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path $vswhere) {
            $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            $cmakePath = Join-Path $vsInstall 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            if (Test-Path $cmakePath) {
                $cmake = @{ Source = $cmakePath }
            }
        }
    }
    if ($null -eq $cmake) { throw 'cmake not found. Install Visual Studio Build Tools or CMake.' }

    return $cmake.Source
}

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

# 1. Install dependency and build if needed. Prefer the root build tree used by README and AGENTS.md.
Install-VBCableIfNeeded
if (-not (Test-Path -LiteralPath $rootBuildExe) -and -not (Test-Path -LiteralPath $directBuildExe)) {
    Write-Host 'Building PeakGuardTray.'
    $cmakeExe = Resolve-CMake
    & $cmakeExe -S $repoRoot -B $rootBuildDir -A x64
    & $cmakeExe --build $rootBuildDir --config $Configuration --target PeakGuardTray
}

$exeSource = if (Test-Path -LiteralPath $rootBuildExe) {
    $rootBuildExe
} else {
    $directBuildExe
}

$exeSource = Resolve-RequiredPath $exeSource 'PeakGuardTray.exe'

# 2. Copy to install directory
Stop-PeakGuardTray
New-Item -ItemType Directory -Force -Path $installDir | Out-Null
Copy-Item -LiteralPath $exeSource -Destination $exeDest -Force
Write-Host "Installed: $exeDest"

# 3. Create Start Menu shortcut
New-Item -ItemType Directory -Force -Path $startMenuDir | Out-Null
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $exeDest
$shortcut.Arguments = '--show'
$shortcut.WorkingDirectory = $installDir
$shortcut.IconLocation = "$exeDest,0"
$shortcut.Description = 'PeakGuard'
$shortcut.Save()
Write-Host "Start Menu shortcut created: $shortcutPath"

# 4. Set startup (optional)
if ($EnableStartup) {
    New-Item -Path $runKeyPath -Force | Out-Null
    New-ItemProperty -LiteralPath $runKeyPath -Name $runValueName -Value "`"$exeDest`"" -PropertyType String -Force | Out-Null
    Write-Host "Startup enabled: $runValueName"
}

# 5. Launch
Start-Process -FilePath $exeDest -ArgumentList '--show'
Write-Host 'PeakGuard started.'

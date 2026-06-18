<#
.SYNOPSIS
    Installs Codex Limiter (WASAPI loopback mode).
    Builds LimiterTray, copies to LocalAppData, creates a Start Menu shortcut, optionally sets HKCU startup, launches.
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
$directBuildDir = Join-Path $repoRoot 'build/LimiterTray'
$rootBuildExe = Join-Path $rootBuildDir "tools/LimiterTray/$Configuration/LimiterTray.exe"
$directBuildExe = Join-Path $directBuildDir "$Configuration/LimiterTray.exe"
$installDir = Join-Path $env:LOCALAPPDATA 'CodexLimiter'
$exeDest = Join-Path $installDir 'LimiterTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Codex Limiter'
$shortcutPath = Join-Path $startMenuDir 'Codex Limiter.lnk'
$runKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'CodexLimiter'

function Resolve-RequiredPath {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path. Build first with: cmake --build build --config $Configuration --target LimiterTray"
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

function Stop-LimiterTray {
    $processes = @(Get-Process LimiterTray -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return
    }

    if (Test-Path -LiteralPath $exeDest) {
        Start-Process -FilePath $exeDest -ArgumentList '--quit' -WindowStyle Hidden
    }

    Start-Sleep -Milliseconds 2500
    $remaining = @(Get-Process LimiterTray -ErrorAction SilentlyContinue)
    if ($remaining.Count -gt 0) {
        $remaining | Stop-Process -Force
        $remaining | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    }
}

# 1. Build if needed. Prefer the root build tree used by README and AGENTS.md.
if (-not (Test-Path -LiteralPath $rootBuildExe) -and -not (Test-Path -LiteralPath $directBuildExe)) {
    Write-Host "Building LimiterTray..."
    $cmakeExe = Resolve-CMake
    & $cmakeExe -S $repoRoot -B $rootBuildDir -A x64
    & $cmakeExe --build $rootBuildDir --config $Configuration --target LimiterTray
}

$exeSource = if (Test-Path -LiteralPath $rootBuildExe) {
    $rootBuildExe
} else {
    $directBuildExe
}

$exeSource = Resolve-RequiredPath $exeSource 'LimiterTray.exe'

# 2. Copy to install directory
Stop-LimiterTray
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
$shortcut.Description = 'Codex Limiter'
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
Write-Host "Codex Limiter started."

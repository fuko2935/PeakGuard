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
$exeSource = Join-Path $repoRoot "build/LimiterTray/$Configuration/LimiterTray.exe"
$installDir = Join-Path $env:LOCALAPPDATA 'CodexLimiter'
$exeDest = Join-Path $installDir 'LimiterTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\Codex Limiter'
$shortcutPath = Join-Path $startMenuDir 'Codex Limiter.lnk'
$runKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'CodexLimiter'

function Resolve-RequiredPath {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path. Build first with: cmake --build build/LimiterTray --config $Configuration"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

# 1. Build if needed
if (-not (Test-Path -LiteralPath $exeSource)) {
    Write-Host "Building LimiterTray..."
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

    $cmakeExe = if ($cmake.Source) { $cmake.Source } else { $cmake.Source }
    & $cmakeExe -S (Join-Path $repoRoot 'tools/LimiterTray') -B (Join-Path $repoRoot 'build/LimiterTray') -A x64
    & $cmakeExe --build (Join-Path $repoRoot 'build/LimiterTray') --config $Configuration
}

Resolve-RequiredPath $exeSource 'LimiterTray.exe'

# 2. Copy to install directory
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

# 5. Stop old instance and launch
Get-Process LimiterTray -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath $exeDest -ArgumentList '--show'
Write-Host "Codex Limiter started."

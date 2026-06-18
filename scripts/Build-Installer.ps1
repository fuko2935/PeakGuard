<#
.SYNOPSIS
    Builds the PeakGuard NSIS installer.
#>
[CmdletBinding()]
param(
    [string]$Version = '1.0.0',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$OutDir = 'out'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$outPath = Join-Path $repoRoot $OutDir
$installerScript = Join-Path $repoRoot 'installer\PeakGuard.nsi'
$sourceExe = Join-Path $repoRoot "build\tools\PeakGuardTray\$Configuration\PeakGuardTray.exe"
$sourceIcon = Join-Path $repoRoot 'tools\PeakGuardTray\app.ico'
$outputExe = Join-Path $outPath "PeakGuardSetup-$Version.exe"

function Resolve-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) { return $cmake.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $cmakePath = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $cmakePath) { return $cmakePath }
    }

    throw 'cmake not found. Install Visual Studio Build Tools or CMake.'
}

function Resolve-MakeNSIS {
    $makensis = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($makensis) { return $makensis.Source }

    $commonPaths = @(
        (Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe'),
        (Join-Path $env:ProgramFiles 'NSIS\makensis.exe')
    )
    foreach ($path in $commonPaths) {
        if (Test-Path -LiteralPath $path) { return $path }
    }

    throw 'makensis.exe not found. Install NSIS, then rerun this script.'
}

New-Item -ItemType Directory -Force -Path $outPath | Out-Null

$cmake = Resolve-CMake
& $cmake -S $repoRoot -B (Join-Path $repoRoot 'build') -A x64
& $cmake --build (Join-Path $repoRoot 'build') --config $Configuration --target PeakGuardTray

if (-not (Test-Path -LiteralPath $sourceExe)) {
    throw "PeakGuardTray.exe not found after build: $sourceExe"
}

$makensis = Resolve-MakeNSIS
& $makensis `
    "/DAPP_VERSION=$Version" `
    "/DSOURCE_EXE=$sourceExe" `
    "/DSOURCE_ICON=$sourceIcon" `
    "/DOUT_FILE=$outputExe" `
    $installerScript

if (-not (Test-Path -LiteralPath $outputExe)) {
    throw "Installer was not created: $outputExe"
}

Write-Host "Installer created: $outputExe"

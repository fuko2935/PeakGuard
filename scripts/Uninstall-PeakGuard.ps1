<#
.SYNOPSIS
    Uninstalls PeakGuard.
    Stops tray, removes HKCU startup, removes installed files.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$installDir = Join-Path $env:LOCALAPPDATA 'PeakGuard'
$exeDest = Join-Path $installDir 'PeakGuardTray.exe'
$startMenuDir = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\PeakGuard'
$shortcutPath = Join-Path $startMenuDir 'PeakGuard.lnk'
$runKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'PeakGuard'

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

# 1. Stop processes
Write-Host 'Stopping PeakGuardTray...'
Stop-PeakGuardTray

# 2. Remove startup entry
if (Test-Path -LiteralPath $runKeyPath) {
    $existing = Get-ItemProperty -LiteralPath $runKeyPath -Name $runValueName -ErrorAction SilentlyContinue
    if ($null -ne $existing) {
        Remove-ItemProperty -LiteralPath $runKeyPath -Name $runValueName
        Write-Host "Removed startup entry: $runValueName"
    }
}

# 3. Remove Start Menu shortcut
if (Test-Path -LiteralPath $shortcutPath) {
    Remove-Item -LiteralPath $shortcutPath -Force
    Write-Host "Removed Start Menu shortcut: $shortcutPath"
}
if (Test-Path -LiteralPath $startMenuDir) {
    $remaining = Get-ChildItem -LiteralPath $startMenuDir -Force -ErrorAction SilentlyContinue
    if ($null -eq $remaining) {
        Remove-Item -LiteralPath $startMenuDir -Force
        Write-Host "Removed Start Menu folder: $startMenuDir"
    }
}

# 4. Remove installed files
if (Test-Path -LiteralPath $installDir) {
    Remove-Item -LiteralPath $installDir -Recurse -Force
    Write-Host "Removed: $installDir"
}

Write-Host 'PeakGuard uninstalled.'

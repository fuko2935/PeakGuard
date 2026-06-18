Unicode true
ManifestSupportedOS all
RequestExecutionLevel user

!define APP_NAME "PeakGuard"
!define APP_EXE "PeakGuardTray.exe"
!define COMPANY_NAME "PeakGuard"
!define STARTUP_NAME "PeakGuard"

!ifndef APP_VERSION
!define APP_VERSION "1.0.0"
!endif

!ifndef SOURCE_EXE
!define SOURCE_EXE "..\build\tools\PeakGuardTray\Release\PeakGuardTray.exe"
!endif

!ifndef SOURCE_ICON
!define SOURCE_ICON "..\tools\PeakGuardTray\app.ico"
!endif

!ifndef OUT_FILE
!define OUT_FILE "..\out\PeakGuardSetup-${APP_VERSION}.exe"
!endif

Name "${APP_NAME}"
OutFile "${OUT_FILE}"
InstallDir "$LOCALAPPDATA\${APP_NAME}"
InstallDirRegKey HKCU "Software\${APP_NAME}" "InstallDir"
Icon "${SOURCE_ICON}"
UninstallIcon "${SOURCE_ICON}"

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "CompanyName" "${COMPANY_NAME}"
VIAddVersionKey "FileDescription" "${APP_NAME} Installer"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "MIT License"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "${SOURCE_ICON}"
!define MUI_UNICON "${SOURCE_ICON}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_PARAMETERS "--show"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function StopPeakGuard
  nsExec::ExecToLog '"$INSTDIR\${APP_EXE}" --quit'
  Sleep 1200
  nsExec::ExecToLog 'taskkill /IM ${APP_EXE} /F'
FunctionEnd

Function DetectVBCable
  nsExec::ExecToStack 'powershell -NoProfile -ExecutionPolicy Bypass -Command "$$d = Get-CimInstance Win32_SoundDevice -ErrorAction SilentlyContinue | Where-Object { $$_.ProductName -match ''VB-Audio Virtual Cable|VB-CABLE|CABLE Input|CABLE Output'' }; if ($$d) { exit 0 } else { exit 1 }"'
  Pop $0
FunctionEnd

Section "${APP_NAME}" SecApp
  SectionIn RO

  Call StopPeakGuard

  SetOutPath "$INSTDIR"
  File /oname=${APP_EXE} "${SOURCE_EXE}"

  WriteRegStr HKCU "Software\${APP_NAME}" "InstallDir" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}" "--show" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "${COMPANY_NAME}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "NoRepair" 1
SectionEnd

Section "Start with Windows" SecStartup
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${STARTUP_NAME}" '"$INSTDIR\${APP_EXE}"'
SectionEnd

Section "Install VB-CABLE driver if missing" SecVBCable
  Call DetectVBCable
  ${If} $0 == 0
    DetailPrint "VB-CABLE is already installed."
  ${Else}
    DetailPrint "VB-CABLE not found. Downloading official driver package."
    nsExec::ExecToStack 'powershell -NoProfile -ExecutionPolicy Bypass -Command "$$ErrorActionPreference = ''Stop''; $$zip = Join-Path $$env:TEMP ''VBCABLE_Driver.zip''; $$dir = Join-Path $$env:TEMP ''VBCABLE_Extract''; Invoke-WebRequest -Uri ''https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'' -OutFile $$zip -UserAgent ''Mozilla/5.0 (Windows NT 10.0; Win64; x64)''; if (Test-Path -LiteralPath $$dir) { Remove-Item -LiteralPath $$dir -Recurse -Force }; Expand-Archive -LiteralPath $$zip -DestinationPath $$dir -Force; $$setup = Join-Path $$dir ''VBCABLE_Setup_x64.exe''; if (-not (Test-Path -LiteralPath $$setup)) { throw ''VB-CABLE setup executable not found.'' }; Start-Process -FilePath $$setup -Verb RunAs -Wait; exit 0"'
    Pop $1
    Pop $2
    ${If} $1 != 0
      MessageBox MB_ICONEXCLAMATION "Failed to install VB-CABLE automatically. Please install it manually from https://vb-audio.com/Cable/"
    ${EndIf}
  ${EndIf}
SectionEnd

Section "Uninstall"
  nsExec::ExecToLog '"$INSTDIR\${APP_EXE}" --quit'
  Sleep 1200
  nsExec::ExecToLog 'taskkill /IM ${APP_EXE} /F'

  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${STARTUP_NAME}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
  DeleteRegKey HKCU "Software\${APP_NAME}"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"

  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
SectionEnd

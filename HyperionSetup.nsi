; Hyperion Browser NSIS Installer Script
; Version: 0.1.0 Alpha
; Made by CloudDev

!include "MUI2.nsh"

; General Settings
Name "Hyperion Browser"
OutFile "HyperionSetup.exe"
InstallDir "$PROGRAMFILES64\Hyperion Browser"
InstallDirRegKey HKLM "Software\Hyperion Browser" "InstallDir"
RequestExecutionLevel admin

; Interface Settings
!define MUI_ABORTWARNING

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; Installer Section
Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Main executable
    File "release-v0.1.0-alpha\hyperion.exe"
    
    ; Renderer (optional)
    File "release-v0.1.0-alpha\hyperion_renderer.exe"
    
    ; Test HTML
    File "release-v0.1.0-alpha\test.html"
    
    ; Create Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\Hyperion Browser"
    CreateShortcut "$SMPROGRAMS\Hyperion Browser\Hyperion Browser.lnk" "$INSTDIR\hyperion.exe"
    CreateShortcut "$SMPROGRAMS\Hyperion Browser\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    
    ; Create Desktop shortcut
    CreateShortcut "$DESKTOP\Hyperion Browser.lnk" "$INSTDIR\hyperion.exe"
    
    ; Write uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Write registry keys
    WriteRegStr HKLM "Software\Hyperion Browser" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "DisplayName" "Hyperion Browser"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "DisplayVersion" "0.1.0 Alpha"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "Publisher" "Hyperion"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion" "NoRepair" 1
SectionEnd

; Uninstaller Section
Section "Uninstall"
    ; Remove files
    Delete "$INSTDIR\hyperion.exe"
    Delete "$INSTDIR\hyperion_renderer.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    
    ; Remove shortcuts
    Delete "$SMPROGRAMS\Hyperion Browser\Hyperion Browser.lnk"
    Delete "$SMPROGRAMS\Hyperion Browser\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Hyperion Browser"
    Delete "$DESKTOP\Hyperion Browser.lnk"
    
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Hyperion Browser"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Hyperion"
SectionEnd
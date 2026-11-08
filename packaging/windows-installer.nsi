Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

Name "PetDesk ${VERSION}"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\PetDesk"
InstallDirRegKey HKCU "Software\PetDesk" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "PetDesk"
VIAddVersionKey "FileDescription" "PetDesk installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "PetDesk contributors"
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\PetDesk.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "SimpChinese"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "此版本需要 64 位 Windows。"
        Abort
    ${EndIf}
FunctionEnd

Section "Install"
    SetShellVarContext current
    SetOutPath "$INSTDIR"
    File /r "${STAGE_DIR}/*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    CreateDirectory "$SMPROGRAMS\PetDesk"
    CreateShortcut "$SMPROGRAMS\PetDesk\PetDesk.lnk" "$INSTDIR\PetDesk.exe"
    WriteRegStr HKCU "Software\PetDesk" "InstallDir" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "DisplayName" "PetDesk"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "DisplayIcon" "$INSTDIR\PetDesk.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk" "NoRepair" 1
SectionEnd

Section "Uninstall"
    SetShellVarContext current
    !include "${UNINSTALL_INCLUDE}"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    Delete "$SMPROGRAMS\PetDesk\PetDesk.lnk"
    RMDir "$SMPROGRAMS\PetDesk"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\PetDesk"
    DeleteRegKey HKCU "Software\PetDesk"
SectionEnd

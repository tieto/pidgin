; Installer script for win32 Gaim
; Herman Bloggs <hermanator12002@yahoo.com>

; NOTE: this .NSI script is designed for NSIS v2.0b0+

Name "Gaim ${GAIM_VERSION} (Win32)"
OutFile "gaim-${GAIM_VERSION}.exe"
Icon .\pixmaps\gaim-install.ico
UninstallIcon .\pixmaps\gaim-install.ico

; Some default compiler settings (uncomment and change at will):
; SetCompress auto ; (can be off or force)
; SetDatablockOptimize on ; (can be off)
; CRCCheck on ; (can be off)
; AutoCloseWindow false ; (can be true for the window go away automatically at end)
; ShowInstDetails hide ; (can be show to have them shown, or nevershow to disable)
; SetDateSave off ; (can be on to have files restored to their orginal date)

InstallDir "$PROGRAMFILES\Gaim"
InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" ""
DirShow show ; (make this hide to not let the user change it)
DirText "Select the directory to install Gaim in:"

Section "" ; (default section)
  ; Check if previous intallation exists
  ReadRegStr $R0 HKEY_LOCAL_MACHINE "SOFTWARE\gaim" ""
  StrCmp $R0 "" cont_install
    ReadRegStr $R1 HKEY_LOCAL_MACHINE "SOFTWARE\gaim" "Version"
    StrCmp $R1 "" no_version
      ; Gaim found, so exit Intallation
      MessageBox MB_OK "Gaim (v$R1) already exists on this machine. Uninstall first then try again." IDOK
      Quit
      no_version: 
      MessageBox MB_OK "Gaim already exists on this machine. Uninstall first then try again." IDOK
      Quit
  cont_install:
  SetOutPath "$INSTDIR"
  ; Gaim files
  File /r .\win32-install-dir\*.*

  ; Gaim Registry Settings
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "" "$INSTDIR"
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "Version" "${GAIM_VERSION}"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "DisplayName" "Gaim (remove only)"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "UninstallString" '"$INSTDIR\gaim-uninst.exe"'

  ; Set Start Menu icons
  SetShellVarContext "all"
  SetOutPath "$SMPROGRAMS\Gaim"
  CreateShortCut "$SMPROGRAMS\Gaim\Gaim.lnk" \
                 "$INSTDIR\gaim.exe"
  CreateShortCut "$SMPROGRAMS\Gaim\Unistall.lnk" \
                 "$INSTDIR\gaim-uninst.exe"
  ; Set Desktop icon
  CreateShortCut "$DESKTOP\Gaim.lnk" \
                 "$INSTDIR\gaim.exe"

  ; write out uninstaller
  WriteUninstaller "$INSTDIR\gaim-uninst.exe"
SectionEnd ; end of default section

; begin uninstall settings/section
UninstallText "This will uninstall Gaim from your system"

Section Uninstall
  ; Delete Gaim Dir
  RMDir /r "$INSTDIR"

  ; Delete Start Menu group & Desktop icon
  SetShellVarContext "all"
  RMDir /r "$SMPROGRAMS\Gaim"
  Delete "$DESKTOP\Gaim.lnk"

  ; Delete Gaim Registry Settings
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Gaim"

SectionEnd ; end of uninstall section

; eof


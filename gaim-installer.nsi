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
  ; Install Aspell
  SetOutPath "$INSTDIR"
  File ..\win32-dev\aspell-15\bin\aspell-0.50.2.exe
  ; Don't do this silently (i.e /S switch).. because some people wish to specify
  ; the location of the Aspell install directory. (
  ExecWait '"$INSTDIR\aspell-0.50.2.exe"' $R0
  ; cleanup aspell installer file
  Delete "$INSTDIR\aspell-0.50.2.exe"
  ; Check if aspell installer completed ok
  StrCmp $R0 "0" have_aspell
    ; Aspell exited uncleanly so we will exit uncleanly too.
    RMDir /r "$INSTDIR"
    Quit
  have_aspell:
  SetOutPath "$INSTDIR"
  ; Gaim files
  File /r .\win32-install-dir\*.*

  ; Gaim Registry Settings
  ; Read in Aspell install path
  ReadRegStr $R0 HKEY_LOCAL_MACHINE "Software\Aspell" ""
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "" "$INSTDIR"
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "Version" "${GAIM_VERSION}"
  ; Keep track of aspell install path, for when we uninstall
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "AspellPath" $R0
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "DisplayName" "Gaim (remove only)"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "UninstallString" '"$INSTDIR\gaim-uninst.exe"'
  ; Set App path to include aspell dir (so Gaim can find aspell dlls)
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe" "" "$INSTDIR\gaim.exe"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe" "Path" $R0
  ; Increase refrence count for aspell dlls
  Push "$R0\aspell-15.dll"
  Call AddSharedDLL
  Push "$R0\aspell-common-0-50-2.dll"
  Call AddSharedDLL
  Push "$R0\pspell-15.dll"
  Call AddSharedDLL

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

  ; Read in Aspell install path
  ReadRegStr $R0 HKEY_LOCAL_MACHINE "Software\Gaim" "AspellPath"

  ; Delete Gaim Registry Settings
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Gaim"
  DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe"

  ; Decrease refrence count for Aspell dlls
  Push "$R0\aspell-15.dll"
  Call un.RemoveSharedDLL
  Push "$R0\aspell-common-0-50-2.dll"
  Call un.RemoveSharedDLL
  Push "$R0\pspell-15.dll"
  Call un.RemoveSharedDLL

  ; Delete aspell dir if its empty
  RMDir "$R0"
SectionEnd ; end of uninstall section

;;;
;;; FUNCTIONS
;;;

; AddSharedDLL
;
; Increments a shared DLLs reference count.
; Use by passing one item on the stack (the full path of the DLL).
;
; Usage: 
;   Push $SYSDIR\myDll.dll
;   Call AddSharedDLL
;

Function AddSharedDLL
  Exch $R1
  Push $R0
  ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
  IntOp $R0 $R0 + 1
  WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
  Pop $R0
  Pop $R1
FunctionEnd

; un.RemoveSharedDLL
;
; Decrements a shared DLLs reference count, and removes if necessary.
; Use by passing one item on the stack (the full path of the DLL).
; Note: for use in the main installer (not the uninstaller), rename the
; function to RemoveSharedDLL.
; 
; Usage:
;   Push $SYSDIR\myDll.dll
;   Call un.RemoveShareDLL
;

Function un.RemoveSharedDLL
  Exch $R1
  Push $R0
  ReadRegDword $R0 HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
  StrCmp $R0 "" remove
    IntOp $R0 $R0 - 1
    IntCmp $R0 0 rk rk uk
    rk:
      DeleteRegValue HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1
    goto Remove
    uk:
      WriteRegDWORD HKLM Software\Microsoft\Windows\CurrentVersion\SharedDLLs $R1 $R0
    Goto noremove
  remove:
    Delete /REBOOTOK $R1
  noremove:
  Pop $R0
  Pop $R1
FunctionEnd

; eof


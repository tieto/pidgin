; Installer script for win32 Gaim
; Herman Bloggs <hermanator12002@yahoo.com>

; NOTE: this .NSI script is designed for NSIS v2.0b3+

!define MUI_PRODUCT "Gaim" ;Define your own software name here
!define MUI_VERSION ${GAIM_VERSION} ;Define your own software version here

!include "MUI.nsh"

;--------------------------------
;Configuration

  ;General
  OutFile "gaim-${GAIM_VERSION}.exe"

  ;Folder selection page
  InstallDir "$PROGRAMFILES\Gaim"
  
  ;Remember install folder
  InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" ""

  !define MUI_ICON .\pixmaps\gaim-install.ico
  !define MUI_UNICON .\pixmaps\gaim-install.ico


;--------------------------------
;Modern UI Configuration

  !define MUI_WELCOMEPAGE
  !define MUI_LICENSEPAGE
  !define MUI_DIRECTORYPAGE
  !define MUI_FINISHPAGE
  
  !define MUI_ABORTWARNING

  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"
  
;--------------------------------
;Data
  
  LicenseData "./COPYING"

;--------------------------------
;Installer Sections

Section "" ; (default section)
  ; Check if previous intallation exists
  ReadRegStr $R0 HKEY_LOCAL_MACHINE "SOFTWARE\gaim" ""
  StrCmp $R0 "" cont_install
    ; Previous version found
    ReadRegStr $R1 HKEY_LOCAL_MACHINE "SOFTWARE\gaim" "Version"
    ; Version key started with 0.60a3. Prior versions can't be 
    ; automaticlly uninstalled.
    StrCmp $R1 "" uninstall_first_no_ver
      ; Version found - Read in uninstall string.
      ReadRegStr $R2 HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "UninstallString"
      StrCmp $R2 "" uninstall_first_no_ver
        ; Have uninstall string.. go ahead and uninstall.
	ExecWait '$R2 /S _?=$R0'
	IfErrors "" cont_install
        ; Errors occured
        MessageBox MB_OK "Errors encountered while trying to uninstall previous version of Gaim, continuing installation.." IDOK
	Goto cont_install

      uninstall_first_no_ver:
        MessageBox MB_OK "Gaim already exists on this machine. Uninstall first then try again." IDOK
        Quit

  cont_install:
  ; Check to see if GTK+ Runtime is installed.
  ReadRegStr $R0 HKEY_LOCAL_MACHINE "SOFTWARE\GTK\2.0" "Version"
  StrCmp $R0 "" no_gtk have_gtk
  no_gtk:
    ; Instruct user to install GTK+ runtime first.
    MessageBox MB_OK "Could not find GTK+ runtime environment. Visit http://www.dropline.net/gtk/ to download and install GTK+ v2.2.1.1" IDOK
    Quit
  have_gtk:
    ; Check GTK+ version
    StrLen $R3 $R0 ; Expecting 5 char length version string
    IntCmp $R3 5 check_version bad_version check_version ; if greater or equal to 5... good
    check_version:
    StrCpy $R4 $R0 1   ;Major
    StrCpy $R1 $R0 1 2 ;Minor
    StrCpy $R2 $R0 1 4 ;Micro
    IntCmp $R4 2 to_minor bad_version
    to_minor:
      IntCmp $R1 2 to_micro bad_version
    to_micro: ; If greator or equal to one.. good
      IntCmp $R2 1 good_version bad_version good_version
    bad_version:
      MessageBox MB_OK "Found GTK+ verison $R0. Make sure that you have version 2.2.1 or higher installed, before installing Gaim." IDOK
      Quit
  
  ; Continue
  good_version:
  SetOutPath "$INSTDIR"
  ; Gaim files
  File /r .\win32-install-dir\*.*

  ; Gaim Registry Settings
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "" "$INSTDIR"
  WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Gaim" "Version" "${GAIM_VERSION}"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "DisplayName" "Gaim (remove only)"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gaim" "UninstallString" '"$INSTDIR\gaim-uninst.exe"'
  ; Set App path to include GTK+ lib dir
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe" "" "$INSTDIR\gaim.exe"
  ; Concat GTK+ path and lib dir
  ReadRegStr $R5 HKEY_LOCAL_MACHINE "SOFTWARE\GTK\2.0" "Path"
  StrCpy $R5 "$R5\lib"
  WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe" "Path" $R5

  ; Set Start Menu icons
  SetShellVarContext "all"
  CreateDirectory "$SMPROGRAMS\Gaim"
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

;--------------------------------
;Uninstaller Section

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
  DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe"

  ;Display the Finish header
  !insertmacro MUI_UNFINISHHEADER

SectionEnd ; end of uninstall section

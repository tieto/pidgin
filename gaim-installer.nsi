; Installer script for win32 Gaim
; Herman Bloggs <hermanator12002@yahoo.com>

; NOTE: this .NSI script is designed for NSIS v2.0b4+

;--------------------------------
;Configuration

;General
!ifdef WITH_GTK
OutFile "gaim-${GAIM_VERSION}.exe"
!else
!ifdef DEBUG
OutFile "gaim-${GAIM_VERSION}-debug.exe"
!else
OutFile "gaim-${GAIM_VERSION}-no-gtk.exe"
!endif
!endif
SetCompressor bzip2

DirShow show
ShowInstDetails show
ShowUninstDetails show
SetDateSave on

; $INSTDIR is set in .onInit function..

!include "MUI.nsh"
!include Sections.nsh

;--------------------------------
;Defines

!define MUI_PRODUCT			"Gaim"
!define MUI_VERSION			${GAIM_VERSION}

!define MUI_ICON			.\pixmaps\gaim-install.ico
!define MUI_UNICON			.\pixmaps\gaim-install.ico
!define MUI_SPECIALBITMAP		.\src\win32\nsis\gaim-intro.bmp
!define MUI_HEADERBITMAP		.\src\win32\nsis\gaim-header.bmp

!define GAIM_NSIS_INCLUDE_PATH		".\src\win32\nsis"

!define GAIM_REG_KEY			"SOFTWARE\gaim"
!define GAIM_UNINSTALL_KEY		"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Gaim"
!define HKLM_APP_PATHS_KEY		"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\gaim.exe"
!define GAIM_STARTUP_RUN_KEY		"SOFTWARE\Microsoft\Windows\CurrentVersion\Run"
!define GAIM_UNINST_EXE			"gaim-uninst.exe"

!define GTK_VERSION			"2.2.1"
!define GTK_REG_KEY			"SOFTWARE\GTK\2.0"
!define GTK_DEFAULT_INSTALL_PATH	"$PROGRAMFILES\Common Files\GTK\2.0"
!define GTK_INSTALL_VERIFIER		"lib\libgtk-win32-2.0-0.dll"
!define GTK_RUNTIME_INSTALLER		"..\gtk_installer\gtk-runtime*.exe"
!define GTK_THEME_DIR			"..\gtk_installer\gtk_themes"
!define GTK_DEFAULT_THEME_GTKRC_DIR	"share\themes\Default\gtk-2.0"
!define GTK_DEFAULT_THEME_ENGINE_DIR	"lib\gtk-2.0\2.2.0\engines"

;--------------------------------
;Modern UI Configuration

  !define MUI_CUSTOMPAGECOMMANDS

  !define MUI_WELCOMEPAGE
  !define MUI_LICENSEPAGE
  !define MUI_COMPONENTSPAGE
	!define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_DIRECTORYPAGE
  !define MUI_FINISHPAGE
  
  !define MUI_ABORTWARNING

  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE

;--------------------------------
;Pages
  
  !insertmacro MUI_PAGECOMMAND_WELCOME
  !insertmacro MUI_PAGECOMMAND_LICENSE
  !insertmacro MUI_PAGECOMMAND_COMPONENTS
  Page custom ShowGtkInstallDirChooser GtkInstallDirVerify
  !insertmacro MUI_PAGECOMMAND_DIRECTORY
  !insertmacro MUI_PAGECOMMAND_INSTFILES
  !insertmacro MUI_PAGECOMMAND_FINISH

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Language Strings
!ifndef WITH_GTK
  LangString GTK_INSTALLER_NEEDED		${LANG_ENGLISH} \
		"The GTK+ runtime environment is either missing or needs to be upgraded.$\r \
		Please install v${GTK_VERSION} or higher of the GTK+ runtime"
!endif
  ; Componants Page
  LangString GAIM_SECTION_TITLE			${LANG_ENGLISH} \
		"Gaim Instant Messenger (required)"
  LangString GTK_SECTION_TITLE			${LANG_ENGLISH} \
		"GTK+ Rutime Environment (required)"
  LangString GTK_THEMES_SECTION_TITLE		${LANG_ENGLISH} \
		"GTK+ Themes"
  LangString GTK_NOTHEME_SECTION_TITLE		${LANG_ENGLISH} \
		"No Theme"
  LangString GTK_WIMP_SECTION_TITLE		${LANG_ENGLISH} \
		"Wimp Theme"
  LangString GTK_BLUECURVE_SECTION_TITLE	${LANG_ENGLISH} \
		"Bluecurve Theme"
  LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_ENGLISH} \
		"Light House Blue  Theme"
  LangString GAIM_SECTION_DESCRIPTION		${LANG_ENGLISH} \
		"Core Gaim files and dlls"
  LangString GTK_SECTION_DESCRIPTION		${LANG_ENGLISH} \
		"A multi-platform GUI toolkit, used by Gaim"
  LangString GTK_THEMES_SECTION_DESCRIPTION	${LANG_ENGLISH} \
		"GTK+ Themes can change the look and feel of GTK+ applications."
  LangString GTK_NO_THEME_DESC			${LANG_ENGLISH} \
        	"Don't install a GTK+ theme"
  LangString GTK_WIMP_THEME_DESC		${LANG_ENGLISH} \
		"GTK-Wimp (Windows impersonator) is a GTK theme that blends well into the Windows desktop environment."
  LangString GTK_BLUECURVE_THEME_DESC		${LANG_ENGLISH} \
        	"The Bluecurve theme."
  LangString GTK_LIGHTHOUSEBLUE_THEME_DESC	${LANG_ENGLISH} \
        	"The Lighthouseblue theme."

  ; Extra GTK+ Dir Selector Page
  LangString GTK_PAGE_TITLE			${LANG_ENGLISH} \
		"Choose Install Location"
  LangString GTK_PAGE_SUBTITLE			${LANG_ENGLISH} \
		"Choose the folder in which to install GTK+"
  LangString GTK_PAGE_INSTALL_MSG1		${LANG_ENGLISH} \
		"Setup will install GTK+ in the following folder"
  LangString GTK_PAGE_INSTALL_MSG2		${LANG_ENGLISH} \
		"To install in a different folder, click Browse and select \
		another folder. Click Next to continue."
  LangString GTK_PAGE_UPGRADE_MSG1		${LANG_ENGLISH} \
		"Setup will upgrade GTK+ found in the following folder"
  LangString GTK_UPGRADE_PROMPT			${LANG_ENGLISH} \
		"An old version of the GTK+ runtime was found. Do you wish to upgrade? $\r \
		Note: Gaim may not work unless you do."

  ; Gaim Section Prompts and Texts
  LangString GAIM_UNINSTALL_DESC		${LANG_ENGLISH} \
		"Gaim (remove only)"
  LangString GAIM_PROMPT_WIPEOUT		${LANG_ENGLISH} \
		"You're old Gaim directory is about to be deleted. Would you like to continue?$\r$\r \
		Note: Any non-standard plugins that you may have installed will be deleted. $\r \
		Gaim user settings will not be affected."
  LangString GAIM_PROMPT_DIR_EXISTS		${LANG_ENGLISH} \
		"The installation directory you specified already exists. Any contents $\r \
		it may have will be deleted. Would you like to continue?"

  ; GTK+ Section Prompts
  LangString GTK_INSTALL_ERROR			${LANG_ENGLISH} \
		"Error installing GTK+ runtime."
  LangString GTK_BAD_INSTALL_PATH		${LANG_ENGLISH} \
		"The path you entered can not be accessed or created."
  LangString GTK_DLL_CONFLICT_PROMPT		${LANG_ENGLISH} \
		"Duplicate GTK+ dlls were found in your Windows dll search path and will$\r \
		likely conflict with your GTK+ runtime installation. $\r$\r \
		$\r \
		Would you like to rename these dlls to avoid any possible conflicts?$\r \
		(E.G. somedll.dll to somedll.dll.prob)$\r \
		$\r \
		Note: Any applications relying on these dlls will no longer function.$\r \
		It is suggested that you contact the authors of these applications$\r \
		to notify them of this conflict."
  LangString GTK_INSTALL_TO_GAIM_DIR		${LANG_ENGLISH} \
		"Installing GTK+ runtime package to your Gaim installation directory.$\r \
		(This will prevent Gaim from using any of the duplicate dlls found in $\r \
		in your Windows dll search path)."
  LangString GTK_CAN_NOT_RENAME_CONFLICT_DLL	${LANG_ENGLISH} \
		"A duplicate GTK+ dll was found in your Windows dll search path and will$\r \
		likely conflict with your GTK+ runtime installation. $\r$\r \
                You do not have permission to rename this file.  To avoid any possible dll$\r \
                conflicts, you can install the GTK+ runtime files to the Gaim installation$\r \
                directory.  Do you wish to do so?$\r$\r \
                Note: You may also resolve these conflicts by logging on with an Admin account$\r \
                and running the Gaim installer once more. This will enable the Gaim installer to$\r \
                rename the conflict dlls."

  ; GTK+ Themes section
  LangString GTK_NO_THEME_INSTALL_RIGHTS	${LANG_ENGLISH} \
		"You do not have permission to install a GTK+ theme."

  ; Uninstall Section Prompts
  LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_ENGLISH} \
		"The uninstaller could not find registry entries for Gaim.$\r \
		It is likely that another user installed this application."
  LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_ENGLISH} \
		"You do not have permission to uninstall this application."


;--------------------------------
;Data
  
  LicenseData "./COPYING"

;--------------------------------
;Reserve Files
  ; Only need this if using bzip2 compression

  ReserveFile "${GAIM_NSIS_INCLUDE_PATH}\gtkInstall.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
  ReserveFile "${NSISDIR}\Plugins\AccessControl.dll"
  ReserveFile "${NSISDIR}\Plugins\UserInfo.dll"



;--------------------------------
;Uninstall any old version of Gaim

Section -SecUninstallOldGaim
  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  StrCmp $R0 "HKLM" gaim_hklm
  StrCmp $R0 "HKCU" gaim_hkcu done

  gaim_hkcu:
      ReadRegStr $R1 HKCU ${GAIM_REG_KEY} ""
      ReadRegStr $R2 HKCU ${GAIM_REG_KEY} "Version"
      ReadRegStr $R3 HKCU "${GAIM_UNINSTALL_KEY}" "UninstallString"
      Goto try_uninstall

  gaim_hklm:
      ReadRegStr $R1 HKLM ${GAIM_REG_KEY} ""
      ReadRegStr $R2 HKLM ${GAIM_REG_KEY} "Version"
      ReadRegStr $R3 HKLM "${GAIM_UNINSTALL_KEY}" "UninstallString"

  ; If previous version exists .. remove
  try_uninstall:
    StrCmp $R1 "" done
      ; Version key started with 0.60a3. Prior versions can't be 
      ; automaticlly uninstalled.
      StrCmp $R2 "" uninstall_problem
        ; Check if we have uninstall string..
        IfFileExists $R3 0 uninstall_problem
          ; Have uninstall string.. go ahead and uninstall.
          SetOverwrite on
          ; Need to copy uninstaller outside of the install dir
          ClearErrors
          CopyFiles /SILENT $R3 "$TEMP\${GAIM_UNINST_EXE}"
          SetOverwrite off
          IfErrors uninstall_problem
            ; Ready to uninstall..
            ClearErrors
	    ExecWait '"$TEMP\${GAIM_UNINST_EXE}" /S _?=$R1'
	    IfErrors exec_error
              Delete "$TEMP\${GAIM_UNINST_EXE}"
	      Goto done

	    exec_error:
              Delete "$TEMP\${GAIM_UNINST_EXE}"
              Goto uninstall_problem

        uninstall_problem:
	  ; In this case just wipe out previous Gaim install dir..
	  ; We get here because versions 0.60a1 and 0.60a2 don't have versions set in the registry
	  ; and versions 0.60 and lower did not correctly set the uninstall reg string 
	  ; (the string was set in quotes)
          MessageBox MB_YESNO $(GAIM_PROMPT_WIPEOUT) IDYES do_wipeout IDNO cancel_install
          cancel_install:
            Quit

          do_wipeout:
            StrCmp $R0 "HKLM" gaim_del_lm_reg gaim_del_cu_reg
            gaim_del_cu_reg:
              DeleteRegKey HKCU ${GAIM_REG_KEY}
              Goto uninstall_prob_cont
            gaim_del_lm_reg:
              DeleteRegKey HKLM ${GAIM_REG_KEY}

            uninstall_prob_cont:
	      RMDir /r "$R1"

  done:
SectionEnd


;--------------------------------
;GTK+ Runtime Install Section

!ifdef WITH_GTK
Section $(GTK_SECTION_TITLE) SecGtk
  SectionIn 1 RO

  Call CheckUserInstallRights
  Pop $R1

  SetOutPath $TEMP
  SetOverwrite on
  File /oname=gtk-runtime.exe ${GTK_RUNTIME_INSTALLER}
  SetOverwrite off

  ; This keeps track whether we install GTK+ or not..
  StrCpy $R5 "0"

  Call DoWeNeedGtk
  Pop $R0
  Pop $R6

  StrCmp $R0 "0" have_gtk
  StrCmp $R0 "1" upgrade_gtk
  StrCmp $R0 "2" no_gtk no_gtk

  no_gtk:
    StrCmp $R1 "NONE" gtk_no_install_rights
    !insertmacro MUI_INSTALLOPTIONS_READ $R2 "gtkInstall.ini" "Field 4" "State"
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /S /D=$R2'
    Goto gtk_install_cont

  upgrade_gtk:
    MessageBox MB_YESNO $(GTK_UPGRADE_PROMPT) IDNO done
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /S'
    Goto gtk_install_cont

  gtk_install_cont:
    IfErrors gtk_install_error
      StrCpy $R5 "1"  ; marker that says we installed...
      Goto done

    gtk_install_error:
      Delete "$TEMP\gtk-runtime.exe"
      MessageBox MB_OK $(GTK_INSTALL_ERROR) IDOK
      Quit

  have_gtk:
    StrCpy $R2 $R6 ; Copy GTK+ path
    StrCmp $R1 "NONE" done ; If we have no rights.. can't re-install..
    ; Even if we have a sufficient version of GTK+, we give user choice to re-install.
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /S'
    IfErrors gtk_install_error
    Goto done

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; end got_install rights

  gtk_no_install_rights:
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /S /D=$INSTDIR'
    IfErrors gtk_install_error
      SetOverwrite on
      ClearErrors
      CopyFiles /FILESONLY "$INSTDIR\lib\*.dll" $INSTDIR
      SetOverwrite off
      IfErrors gtk_install_error
        Delete "$INSTDIR\lib\*.dll"
        Goto done
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; end gtk_no_install_rights

  done:
    Delete "$TEMP\gtk-runtime.exe"
SectionEnd ; end of GTK+ section
!endif

;--------------------------------
;Gaim Install Section

Section $(GAIM_SECTION_TITLE) SecGaim
  SectionIn 1 RO

  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  ; Get GTK+ lib dir if we have it..

  StrCmp $R0 "NONE" gaim_none
  StrCmp $R0 "HKLM" gaim_hklm gaim_hkcu

  gaim_hklm:
    ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"
    WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "Path" "$R1\lib"
    WriteRegStr HKLM ${GAIM_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKLM ${GAIM_REG_KEY} "Version" "${GAIM_VERSION}"
    WriteRegStr HKLM "${GAIM_UNINSTALL_KEY}" "DisplayName" $(GAIM_UNINSTALL_DESC)
    WriteRegStr HKLM "${GAIM_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${GAIM_UNINST_EXE}"
    ; Sets scope of the desktop and Start Menu entries for all users.
    SetShellVarContext "all"
    Goto gaim_install_files

  gaim_hkcu:
    ReadRegStr $R1 HKCU ${GTK_REG_KEY} "Path"
    StrCmp $R1 "" 0 gaim_hkcu1
      ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"
    gaim_hkcu1:
    WriteRegStr HKCU ${GAIM_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKCU ${GAIM_REG_KEY} "Version" "${GAIM_VERSION}"
    WriteRegStr HKCU "${GAIM_UNINSTALL_KEY}" "DisplayName" $(GAIM_UNINSTALL_DESC)
    WriteRegStr HKCU "${GAIM_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${GAIM_UNINST_EXE}"
    Goto gaim_install_files

  gaim_none:
    ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"

  gaim_install_files:
    SetOutPath "$INSTDIR"
    ; Gaim files
    SetOverwrite on
    File /r .\win32-install-dir\*.*
    !ifdef DEBUG
    File ..\win32-dev\drmingw\exchndl.dll
    !endif

    ; If we don't have install rights and no hklm GTK install.. then Start in lnk property should
    ; remain gaim dir.. otherwise it should be set to the GTK lib dir. (to avoid dll hell)
    StrCmp $R0 "NONE" 0 startin_gtk
      StrCmp $R1 "" startin_gaim
    startin_gtk:
      SetOutPath "$R1\lib"     
    startin_gaim:
    CreateDirectory "$SMPROGRAMS\Gaim"
    CreateShortCut "$SMPROGRAMS\Gaim\Gaim.lnk" "$INSTDIR\gaim.exe"
    CreateShortCut "$DESKTOP\Gaim.lnk" "$INSTDIR\gaim.exe"
    SetOutPath "$INSTDIR"

    ; If we don't have install rights.. we're done
    StrCmp $R0 "NONE" done
    CreateShortCut "$SMPROGRAMS\Gaim\Uninstall.lnk" "$INSTDIR\${GAIM_UNINST_EXE}"
    SetOverwrite off

    ; write out uninstaller
    SetOverwrite on
    WriteUninstaller "$INSTDIR\${GAIM_UNINST_EXE}"
    SetOverwrite off

  done:
SectionEnd ; end of default Gaim section

;--------------------------------
;GTK+ Themes

SubSection /e $(GTK_THEMES_SECTION_TITLE) SecGtkThemes
  Section $(GTK_NOTHEME_SECTION_TITLE) SecGtkNone
    ; Do nothing..
  SectionEnd

  Section $(GTK_WIMP_SECTION_TITLE) SecGtkWimp
    Call CanWeInstallATheme
    Pop $R1
    StrCmp $R1 "" done

    SetOverwrite on
    Rename $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc.old
    SetOutPath $R1\${GTK_DEFAULT_THEME_ENGINE_DIR}
    File ${GTK_THEME_DIR}\engines\libwimp.dll
    SetOutPath $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}
    File ${GTK_THEME_DIR}\themes\gtkrc.gtkwimp
    File /oname=gtkrc ${GTK_THEME_DIR}\themes\gtkrc.gtkwimp
    SetOverwrite off
    
    done:
  SectionEnd

  Section $(GTK_BLUECURVE_SECTION_TITLE) SecGtkBluecurve
    Call CanWeInstallATheme
    Pop $R1
    StrCmp $R1 "" done

    SetOverwrite on
    Rename $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc.old
    SetOutPath $R1\${GTK_DEFAULT_THEME_ENGINE_DIR}
    File ${GTK_THEME_DIR}\engines\libbluecurve.dll
    SetOutPath $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}
    File ${GTK_THEME_DIR}\themes\gtkrc.bluecurve
    File /oname=gtkrc ${GTK_THEME_DIR}\themes\gtkrc.bluecurve
    SetOverwrite off

    done:
  SectionEnd

  Section $(GTK_LIGHTHOUSEBLUE_SECTION_TITLE) SecGtkLighthouseblue
    Call CanWeInstallATheme
    Pop $R1
    StrCmp $R1 "" done

    SetOverwrite on
    Rename $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}\gtkrc.old
    SetOutPath $R1\${GTK_DEFAULT_THEME_ENGINE_DIR}
    File ${GTK_THEME_DIR}\engines\liblighthouseblue.dll
    SetOutPath $R1\${GTK_DEFAULT_THEME_GTKRC_DIR}
    File ${GTK_THEME_DIR}\themes\gtkrc.lighthouseblue
    File /oname=gtkrc ${GTK_THEME_DIR}\themes\gtkrc.lighthouseblue
    SetOverwrite off

    done:
  SectionEnd
SubSectionEnd

;--------------------------------
;Uninstaller Section


Section Uninstall
  Call un.CheckUserInstallRights
  Pop $R0
  StrCmp $R0 "NONE" no_rights
  StrCmp $R0 "HKCU" try_hkcu try_hklm

  try_hkcu:
    ReadRegStr $R0 HKCU ${GAIM_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 cant_uninstall
      ; HKCU install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKCU ${GAIM_REG_KEY}
      DeleteRegKey HKCU "${GAIM_UNINSTALL_KEY}"
      Goto cont_uninstall

  try_hklm:
    ReadRegStr $R0 HKLM ${GAIM_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 try_hkcu
      ; HKLM install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKLM ${GAIM_REG_KEY}
      DeleteRegKey HKLM "${GAIM_UNINSTALL_KEY}"
      DeleteRegKey HKLM "${HKLM_APP_PATHS_KEY}"
      ; Sets start menu and desktop scope to all users..
      SetShellVarContext "all"

  cont_uninstall:
    ; The WinPrefs plugin may have left this behind..
    DeleteRegValue HKCU "${GAIM_STARTUP_RUN_KEY}" "Gaim"
    DeleteRegValue HKLM "${GAIM_STARTUP_RUN_KEY}" "Gaim"

    RMDir /r "$INSTDIR\locale"
    RMDir /r "$INSTDIR\pixmaps"
    Delete "$INSTDIR\plugins\autorecon.dll"
    Delete "$INSTDIR\plugins\iconaway.dll"
    Delete "$INSTDIR\plugins\libgg.dll"
    Delete "$INSTDIR\plugins\libirc.dll"
    Delete "$INSTDIR\plugins\libjabber.dll"
    Delete "$INSTDIR\plugins\libmsn.dll"
    Delete "$INSTDIR\plugins\liboscar.dll"
    Delete "$INSTDIR\plugins\libtoc.dll"
    Delete "$INSTDIR\plugins\libyahoo.dll"
    Delete "$INSTDIR\plugins\spellchk.dll"
    Delete "$INSTDIR\plugins\ticker.dll"
    Delete "$INSTDIR\plugins\win2ktrans.dll"
    Delete "$INSTDIR\plugins\winprefs.dll"
    RMDir "$INSTDIR\plugins"
    Delete "$INSTDIR\sounds\gaim\arrive.wav"
    Delete "$INSTDIR\sounds\gaim\leave.wav"
    Delete "$INSTDIR\sounds\gaim\receive.wav"
    Delete "$INSTDIR\sounds\gaim\redalert.wav"
    Delete "$INSTDIR\sounds\gaim\send.wav"
    RMDir "$INSTDIR\sounds\gaim"
    RMDir "$INSTDIR\sounds"
    Delete "$INSTDIR\gaim.dll"
    Delete "$INSTDIR\gaim.exe"
    Delete "$INSTDIR\${GAIM_UNINST_EXE}"
    Delete "$INSTDIR\idletrack.dll"
    Delete "$INSTDIR\libgtkspell.dll"
    Delete "$INSTDIR\perl56.dll"
    ;Remove possible GTK+ files and folders..
    RMDir \r "$INSTDIR\lib"
    RMDir \r "$INSTDIR\share"
    RMDir \r "$INSTDIR\locale"
    RMDir \r "$INSTDIR\bin"
    Delete "$INSTDIR\*.dll"
    ;Try to remove Gaim install dir .. if empty
    RMDir "$INSTDIR"

    ; Shortcuts..
    RMDir /r "$SMPROGRAMS\Gaim"
    Delete "$DESKTOP\Gaim.lnk"

    Goto done

  cant_uninstall:
    MessageBox MB_OK $(un.GAIM_UNINSTALL_ERROR_1) IDOK
    Quit

  no_rights:
    MessageBox MB_OK $(un.GAIM_UNINSTALL_ERROR_2) IDOK
    Quit

  done:
  ;Display the Finish header
  !insertmacro MUI_UNFINISHHEADER
SectionEnd ; end of uninstall section

;--------------------------------
;Descriptions
!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGaim} \
	$(GAIM_SECTION_DESCRIPTION)
!ifdef WITH_GTK
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtk} \
	$(GTK_SECTION_DESCRIPTION)
!endif
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtkThemes} \
	$(GTK_THEMES_SECTION_DESCRIPTION)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtkNone} \
        $(GTK_NO_THEME_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtkWimp} \
	$(GTK_WIMP_THEME_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtkBluecurve} \
        $(GTK_BLUECURVE_THEME_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGtkLighthouseblue} \
        $(GTK_LIGHTHOUSEBLUE_THEME_DESC)
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END

;--------------------------------
;Functions

;
; Usage:
;
; Call CanWeInstallATheme
; Pop $R0
;
; Return:
;   "" - If no
;   "root path of GTK+ installation" - if yes
;
Function CanWeInstallATheme
    Push $1
    Push $0

    ; Did we install GTK+ to the Gaim dir?
    IfFileExists "$INSTDIR\lib" 0 check_keys
      StrCpy $1 $INSTDIR
      Goto done

    check_keys:
    ; First see if we can install a theme..
    Call CheckUserInstallRights
    Pop $0

    StrCmp $0 "HKCU" hkcu
    StrCmp $0 "HKLM" hklm no_rights

    hkcu:
      ReadRegStr $1 HKCU ${GTK_REG_KEY} "Path"
      StrCmp $1 "" no_rights done

    hklm:
      ReadRegStr $1 HKLM ${GTK_REG_KEY} "Path"
      StrCmp $1 "" no_rights done

    no_rights:
      MessageBox MB_OK $(GTK_NO_THEME_INSTALL_RIGHTS) IDOK done
      StrCpy $1 ""

    done:
      Pop $0
      Exch $1
FunctionEnd


Function CheckUserInstallRights
	ClearErrors
	UserInfo::GetName
	IfErrors Win9x
	Pop $0
	UserInfo::GetAccountType
	Pop $1

	StrCmp $1 "Admin" 0 +3
                StrCpy $1 "HKLM"
		Goto done
	StrCmp $1 "Power" 0 +3
                StrCpy $1 "HKLM"
		Goto done
	StrCmp $1 "User" 0 +3
		StrCpy $1 "HKCU"
		Goto done
	StrCmp $1 "Guest" 0 +3
		StrCpy $1 "NONE"
		Goto done
	; Unknown error
	StrCpy $1 "NONE"
        Goto done

	Win9x:
		StrCpy $1 "HKLM"

	done:
        Push $1
FunctionEnd

Function un.CheckUserInstallRights
	ClearErrors
	UserInfo::GetName
	IfErrors Win9x
	Pop $0
	UserInfo::GetAccountType
	Pop $1
	StrCmp $1 "Admin" 0 +3
                StrCpy $1 "HKLM"
		Goto done
	StrCmp $1 "Power" 0 +3
                StrCpy $1 "HKLM"
		Goto done
	StrCmp $1 "User" 0 +3
		StrCpy $1 "HKCU"
		Goto done
	StrCmp $1 "Guest" 0 +3
		StrCpy $1 "NONE"
		Goto done
	; Unknown error
	StrCpy $1 "NONE"
        Goto done

	Win9x:
		StrCpy $1 "HKLM"

	done:
        Push $1
FunctionEnd

;
; Usage:
;   Push $0 ; Path string
;   Call VerifyDir
;   Pop $0 ; 0 - Bad path  1 - Good path
;
Function VerifyDir
  Pop $0
  Loop:
    IfFileExists $0 dir_exists
    StrCpy $1 $0 ; save last
    Push $0
    Call GetParent
    Pop $0
    StrLen $2 $0
    ; IfFileExists "C:" on xp returns true and on win2k returns false
    ; So we're done in such a case..
    StrCmp $2 "2" loop_done
    Goto Loop

  loop_done:
    StrCpy $1 "$0\GaImFooB"
    ; Check if we can create dir on this drive..
    ClearErrors
    CreateDirectory $1
    IfErrors DirBad DirGood

  dir_exists:
    ClearErrors
    FileOpen $1 "$0\gaimfoo.bar" w
    IfErrors PathBad PathGood

    DirGood:
      RMDir $1
      Goto PathGood1

    DirBad:
      RMDir $1
      Goto PathBad1

    PathBad:
      FileClose $1
      Delete "$0\gaimfoo.bar"
      PathBad1:
      StrCpy $0 "0"
      Push $0
      Return

    PathGood:
      FileClose $1
      Delete "$0\gaimfoo.bar"
      PathGood1:
      StrCpy $0 "1"
      Push $0
FunctionEnd

Function .onVerifyInstDir
  Push $INSTDIR
  Call VerifyDir
  Pop $0
  StrCmp $0 "0" 0 dir_good
    Abort
  dir_good:
FunctionEnd

; GetParent
; input, top of stack  (e.g. C:\Program Files\Poop)
; output, top of stack (replaces, with e.g. C:\Program Files)
; modifies no other variables.
;
; Usage:
;   Push "C:\Program Files\Directory\Whatever"
;   Call GetParent
;   Pop $R0
;   ; at this point $R0 will equal "C:\Program Files\Directory"
Function GetParent
   Exch $0 ; old $0 is on top of stack
   Push $1
   Push $2
   StrCpy $1 -1
   loop:
     StrCpy $2 $0 1 $1
     StrCmp $2 "" exit
     StrCmp $2 "\" exit
     IntOp $1 $1 - 1
   Goto loop
   exit:
     StrCpy $0 $0 $1
     Pop $2
     Pop $1
     Exch $0 ; put $0 on top of stack, restore $0 to original value
FunctionEnd


; CheckGtkVersion
; inputs: Push 2 GTK+ version strings to check. The major and minor values
; need to be equal, for success.  If the micro val to check is equal or greater
; to the refrence micro value, then we have success.
;
; Usage:
;   Push "2.2.0"  ; Refrence version
;   Push "2.2.1"  ; Version to check
;   Call CheckGtkVersion
;   Pop $R0
;   $R0 will now equal "0", because 2.2.0 is less than 2.2.1
;
Function CheckGtkVersion
  ; Version we want to check
  Pop $6 
  ; Reference version
  Pop $8 

  ; Check that the string to check is at least 5 chars long (i.e. x.x.x)
  StrLen $7 $6
  IntCmp $7 5 0 bad_version

  ; Major version check
  StrCpy $7 $6 1
  StrCpy $9 $8 1
  IntCmp $7 $9 check_minor
    Goto bad_version

  check_minor:
    StrCpy $7 $6 1 2
    StrCpy $9 $8 1 2
    IntCmp $7 $9 check_micro
      Goto bad_version

  check_micro:
    StrCpy $7 $6 1 4
    StrCpy $9 $8 1 4
    IntCmp $7 $9 good_version bad_version good_version

  bad_version:
    StrCpy $6 "0"
    Push $6
    Goto done

  good_version:
    StrCpy $6 "1"
    Push $6
  done:
FunctionEnd

;
; Usage:
; Call DoWeNeedGtk
; First Pop:
;   0 - We have the correct version
;       Second Pop: Key where Version was found
;   1 - We have an old version that needs to be upgraded
;       Second Pop: HKLM or HKCU depending on where GTK was found.
;   2 - We don't have Gtk+ at all
;       Second Pop: "NONE, HKLM or HKCU" depending on our rights..
;
Function DoWeNeedGtk
  ; Logic should be:
  ; - Check what user rights we have (HKLM or HKCU)
  ;   - If HKLM rights..
  ;     - Only check HKLM key for GTK+
  ;       - If installed to HKLM, check it and return.
  ;   - If HKCU rights..
  ;     - First check HKCU key for GTK+
  ;       - if good or bad exists stop and ret.
  ;     - If no hkcu gtk+ install, check HKLM
  ;       - If HKLM ver exists but old, return as if no ver exits.
  ;   - If no rights
  ;     - Check HKLM

  Call CheckUserInstallRights
  Pop $3
  StrCmp $3 "HKLM" check_hklm
  StrCmp $3 "HKCU" check_hkcu check_hklm
    check_hkcu:
      ReadRegStr $0 HKCU ${GTK_REG_KEY} "Version"
      StrCpy $5 "HKCU"
      StrCmp $0 "" check_hklm have_gtk

    check_hklm:
      ReadRegStr $0 HKLM ${GTK_REG_KEY} "Version"
      StrCpy $5 "HKLM"
      StrCmp $0 "" no_gtk have_gtk


  have_gtk:
    ; GTK+ is already installed.. check version.
    StrCpy $1 ${GTK_VERSION} ; Minimum GTK+ version needed
    Push $1
    Push $0
    Call CheckGtkVersion
    Pop $2
    StrCmp $2 "1" good_version bad_version
    bad_version:
      ; Bad version. If hklm ver and we have hkcu or no rights.. return no gtk
      StrCmp $3 "NONE" no_gtk  ; if no rights.. can't upgrade
      StrCmp $3 "HKCU" 0 upgrade_gtk ; if HKLM can upgrade..
        StrCmp $5 "HKLM" no_gtk upgrade_gtk ; have hkcu rights.. if found hklm ver can't upgrade..
  
      upgrade_gtk:
        StrCpy $2 "1"
        Push $5
        Push $2
        Goto done

  good_version:
    ; Just make sure we have it. There was a gtk+ uninstaller that 
    ; left behind reg entries after uninstalling..
    StrCmp $5 "HKLM" have_hklm_gtk have_hkcu_gtk
      have_hkcu_gtk:
        ; Have HKCU version
        ReadRegStr $4 HKCU ${GTK_REG_KEY} "Path"
        StrCpy $1 "$4\${GTK_INSTALL_VERIFIER}"
        IfFileExists $1 good_version_verified
          DeleteRegKey HKCU ${GTK_REG_KEY}
          Goto no_gtk

      have_hklm_gtk:
        ReadRegStr $4 HKLM ${GTK_REG_KEY} "Path"
        StrCpy $1 "$4\${GTK_INSTALL_VERIFIER}"
        IfFileExists $1 good_version_verified
          DeleteRegKey HKLM ${GTK_REG_KEY}
          Goto no_gtk

    good_version_verified:
      StrCpy $2 "0"
      Push $4  ; The path to existing GTK+
      Push $2
      Goto done

    no_gtk:
      StrCpy $2 "2"
      Push $3 ; our rights
      Push $2
      Goto done

  done:
FunctionEnd

Function .onInit
  ; If this installer dosn't have GTK, check whether we need it.
  !ifndef WITH_GTK
    Call DoWeNeedGtk
    Pop $0
    Pop $1

    StrCmp $0 "0" have_gtk need_gtk
    need_gtk:
      MessageBox MB_OK $(GTK_INSTALLER_NEEDED) IDOK
      Quit
    have_gtk:
  !else
  ;Extract InstallOptions INI Files
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT_AS "${GAIM_NSIS_INCLUDE_PATH}\gtkInstall.ini" "gtkInstall.ini"
  !endif

  Call CheckUserInstallRights
  Pop $0

  StrCmp $0 "HKLM" 0 user_dir
    StrCpy $INSTDIR "$PROGRAMFILES\Gaim"
    Goto instdir_done
  user_dir:
    StrCpy $2 "$SMPROGRAMS"
    Push $2
    Call GetParent
    Call GetParent
    Pop $2
    StrCpy $INSTDIR "$2\Gaim"

  instdir_done:

  ; Set up Theme sections..
  StrCpy $1 ${SecGtkNone} ; Sets global to remember which theme is set.
  !insertmacro SelectSection ${SecGtkNone}
  !insertmacro UnselectSection ${SecGtkWimp}
  !insertmacro UnselectSection ${SecGtkBluecurve}
  !insertmacro UnselectSection ${SecGtkLighthouseblue}

FunctionEnd

Function .onSelChange
  Push $0
  Push $2

  StrCpy $2 ${SF_SELECTED}
  SectionGetFlags ${SecGtkNone} $0
  IntOp $2 $2 & $0
  SectionGetFlags ${SecGtkWimp} $0
  IntOp $2 $2 & $0
  SectionGetFlags ${SecGtkBluecurve} $0
  IntOp $2 $2 & $0
  SectionGetFlags ${SecGtkLighthouseblue} $0
  IntOp $2 $2 & $0
  StrCmp $2 0 skip
    SectionSetFlags ${SecGtkNone} 0
    SectionSetFlags ${SecGtkWimp} 0
    SectionSetFlags ${SecGtkBluecurve} 0
    SectionSetFlags ${SecGtkLighthouseblue} 0
  skip:

  !insertmacro UnselectSection $1
 
  ; Remember old selection
  StrCpy $2 $1

  ; Now go through and see who is checked..
  SectionGetFlags ${SecGtkNone} $0
  IntOp $0 $0 & ${SF_SELECTED}
  IntCmp $0 ${SF_SELECTED} 0 +2 +2
    StrCpy $1 ${SecGtkNone}
  SectionGetFlags ${SecGtkWimp} $0
  IntOp $0 $0 & ${SF_SELECTED}
  IntCmp $0 ${SF_SELECTED} 0 +2 +2
    StrCpy $1 ${SecGtkWimp}
  SectionGetFlags ${SecGtkBluecurve} $0
  IntOp $0 $0 & ${SF_SELECTED}
  IntCmp $0 ${SF_SELECTED} 0 +2 +2
    StrCpy $1 ${SecGtkBluecurve}
  SectionGetFlags ${SecGtkLighthouseblue} $0
  IntOp $0 $0 & ${SF_SELECTED}
  IntCmp $0 ${SF_SELECTED} 0 +2 +2
    StrCpy $1 ${SecGtkLighthouseblue}

  StrCmp $2 $1 0 +2 ; selection hasn't changed
    !insertmacro SelectSection $1

  Pop $2
  Pop $0
FunctionEnd

Function ShowGtkInstallDirChooser
  Call DoWeNeedGtk
  Pop $0
  Pop $1

  StrCmp $0 "0" have_gtk
  StrCmp $0 "1" upgrade_gtk
  StrCmp $0 "2" no_gtk no_gtk

  ; Don't show dir selector.. Upgrades are done to existing path..
  have_gtk:
  upgrade_gtk:
    Abort

  no_gtk:
    StrCmp $1 "NONE" 0 no_gtk_cont
      ; Got no install rights..
      Abort
    no_gtk_cont:
      ; Suggest path..
      StrCmp $1 "HKCU" 0 hklm1
        StrCpy $2 "$SMPROGRAMS"
        Push $2
        Call GetParent
        Call GetParent
        Pop $2
        StrCpy $2 "$2\GTK\2.0"
        Goto got_path
      hklm1:
        StrCpy $2 "${GTK_DEFAULT_INSTALL_PATH}"

  got_path:
    !insertmacro MUI_INSTALLOPTIONS_WRITE "gtkInstall.ini" "Field 4" "State" $2

  !insertmacro MUI_INSTALLOPTIONS_WRITE "gtkInstall.ini" "Field 1" "Text" $(GTK_PAGE_INSTALL_MSG1)
  !insertmacro MUI_INSTALLOPTIONS_WRITE "gtkInstall.ini" "Field 2" "Text" $(GTK_PAGE_INSTALL_MSG2)
  !insertmacro MUI_HEADER_TEXT "$(GTK_PAGE_TITLE)" "$(GTK_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "gtkInstall.ini"
FunctionEnd

Function GtkInstallDirVerify
  !insertmacro MUI_INSTALLOPTIONS_READ $0 "gtkInstall.ini" "Field 4" "State"
  Push $0
  Call VerifyDir
  Pop $0
  StrCmp $0 "0" 0 done
    MessageBox MB_OK $(GTK_BAD_INSTALL_PATH) IDOK
    Abort
  done:
FunctionEnd


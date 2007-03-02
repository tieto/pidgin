; Installer script for win32 Pidgin
; Original Author: Herman Bloggs <hermanator12002@yahoo.com>
; Updated By: Daniel Atallah <daniel_atallah@yahoo.com>

; NOTE: this .NSI script is intended for NSIS 2.08
;

;--------------------------------
;Global Variables
Var name
Var GTK_FOLDER
Var GTK_THEME_SEL
Var ISSILENT
Var STARTUP_RUN_KEY
Var SPELLCHECK_SEL

;--------------------------------
;Configuration

;The name var is set in .onInit
Name $name

!ifdef WITH_GTK
OutFile "pidgin-${PIDGIN_VERSION}.exe"
!else
!ifdef DEBUG
OutFile "pidgin-${PIDGIN_VERSION}-debug.exe"
!else
OutFile "pidgin-${PIDGIN_VERSION}-no-gtk.exe"
!endif
!endif

SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
SetDateSave on

; $name and $INSTDIR are set in .onInit function..

!include "MUI.nsh"
!include "Sections.nsh"

!include "FileFunc.nsh"
!insertmacro GetParameters
!insertmacro GetOptions
!insertmacro GetParent

!include "WordFunc.nsh"
!insertmacro VersionCompare

!include "TextFunc.nsh"
!insertmacro ConfigWriteS

;--------------------------------
;Defines

!define PIDGIN_NSIS_INCLUDE_PATH		"."
!define PIDGIN_INSTALLER_DEPS			"..\..\..\..\win32-dev\pidgin-inst-deps"

; Remove these and the stuff that uses them at some point
!define OLD_GAIM_REG_KEY			"SOFTWARE\gaim"
!define OLD_GAIM_UNINSTALL_KEY			"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Gaim"
!define OLD_GAIM_UNINST_EXE			"gaim-uninst.exe"

!define PIDGIN_REG_KEY				"SOFTWARE\pidgin"
!define PIDGIN_UNINSTALL_KEY			"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Pidgin"

!define HKLM_APP_PATHS_KEY			"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\pidgin.exe"
!define STARTUP_RUN_KEY				"SOFTWARE\Microsoft\Windows\CurrentVersion\Run"
!define PIDGIN_UNINST_EXE			"pidgin-uninst.exe"

!define GTK_MIN_VERSION				"2.6.10"
!define GTK_REG_KEY				"SOFTWARE\GTK\2.0"
!define PERL_REG_KEY				"SOFTWARE\Perl"
!define PERL_DLL				"perl58.dll"
!define GTK_DEFAULT_INSTALL_PATH		"$COMMONFILES\GTK\2.0"
!define GTK_RUNTIME_INSTALLER			"..\..\..\..\gtk_installer\gtk-runtime*.exe"

!define ASPELL_REG_KEY				"SOFTWARE\Aspell"
!define DOWNLOADER_URL				"http://www.pidgin.im/win32/download_redir.php"

;--------------------------------
;Version resource
VIProductVersion "${PIDGIN_PRODUCT_VERSION}"
VIAddVersionKey "ProductName" "Pidgin"
VIAddVersionKey "FileVersion" "${PIDGIN_VERSION}"
VIAddVersionKey "ProductVersion" "${PIDGIN_VERSION}"
VIAddVersionKey "LegalCopyright" ""
!ifdef WITH_GTK
VIAddVersionKey "FileDescription" "Pidgin Installer (w/ GTK+ Installer)"
!else
!ifdef DEBUG
VIAddVersionKey "FileDescription" "Pidgin Installer (Debug Version)"
!else
VIAddVersionKey "FileDescription" "Pidgin Installer (w/o GTK+ Installer)"
!endif
!endif

;--------------------------------
;Modern UI Configuration

  !define MUI_ICON				".\pixmaps\pidgin-install.ico"
  !define MUI_UNICON				".\pixmaps\pidgin-install.ico"
  !define MUI_WELCOMEFINISHPAGE_BITMAP		".\pixmaps\pidgin-intro.bmp"
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP		".\pixmaps\pidgin-header.bmp"

  ; Alter License section
  !define MUI_LICENSEPAGE_BUTTON		$(PIDGIN_LICENSE_BUTTON)
  !define MUI_LICENSEPAGE_TEXT_BOTTOM		$(PIDGIN_LICENSE_BOTTOM_TEXT)

  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU"
  !define MUI_LANGDLL_REGISTRY_KEY ${PIDGIN_REG_KEY}
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

  !define MUI_COMPONENTSPAGE_SMALLDESC
  !define MUI_ABORTWARNING

  ;Finish Page config
  !define MUI_FINISHPAGE_RUN			"$INSTDIR\pidgin.exe"
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_FINISHPAGE_LINK			$(PIDGIN_FINISH_VISIT_WEB_SITE)
  !define MUI_FINISHPAGE_LINK_LOCATION		"http://www.pidgin.im/win32"

;--------------------------------
;Pages

  !define MUI_PAGE_CUSTOMFUNCTION_PRE		preWelcomePage
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE			"../../../COPYING"
  !insertmacro MUI_PAGE_COMPONENTS

!ifdef WITH_GTK
  ; GTK+ install dir page
  !define MUI_PAGE_CUSTOMFUNCTION_PRE		preGtkDirPage
  !define MUI_PAGE_CUSTOMFUNCTION_LEAVE		postGtkDirPage
  !define MUI_DIRECTORYPAGE_VARIABLE		$GTK_FOLDER
  !insertmacro MUI_PAGE_DIRECTORY
!endif

  ; Pidgin install dir page
  !insertmacro MUI_PAGE_DIRECTORY

  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  ;; English goes first because its the default. The rest are
  ;; in alphabetical order (at least the strings actually displayed
  ;; will be).

  !insertmacro MUI_LANGUAGE "English"

  !insertmacro MUI_LANGUAGE "Albanian"
  !insertmacro MUI_LANGUAGE "Bulgarian"
  !insertmacro MUI_LANGUAGE "Catalan"
  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "Danish"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "TradChinese"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "Hebrew"
  !insertmacro MUI_LANGUAGE "Italian"
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "Korean"
  !insertmacro MUI_LANGUAGE "Kurdish"
  !insertmacro MUI_LANGUAGE "Lithuanian"
  !insertmacro MUI_LANGUAGE "Hungarian"
  !insertmacro MUI_LANGUAGE "Dutch"
  !insertmacro MUI_LANGUAGE "Norwegian"
  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "PortugueseBR"
  !insertmacro MUI_LANGUAGE "Portuguese"
  !insertmacro MUI_LANGUAGE "Romanian"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "Serbian"
  !insertmacro MUI_LANGUAGE "Slovak"
  !insertmacro MUI_LANGUAGE "Slovenian"
  !insertmacro MUI_LANGUAGE "Finnish"
  !insertmacro MUI_LANGUAGE "Swedish"

;--------------------------------
;Translations

  !define PIDGIN_DEFAULT_LANGFILE "${PIDGIN_NSIS_INCLUDE_PATH}\translations\english.nsh"

  !include "${PIDGIN_NSIS_INCLUDE_PATH}\langmacros.nsh"

  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "ALBANIAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\albanian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "BULGARIAN"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\bulgarian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "CATALAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\catalan.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "CZECH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\czech.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "DANISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\danish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "DUTCH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\dutch.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "ENGLISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\english.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "FINNISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\finnish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "FRENCH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\french.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "GERMAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\german.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "HEBREW"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\hebrew.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "HUNGARIAN"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\hungarian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "ITALIAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\italian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "JAPANESE"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\japanese.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "KOREAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\korean.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "KURDISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\kurdish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "LITHUANIAN"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\lithuanian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "NORWEGIAN"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\norwegian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "POLISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\polish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "PORTUGUESE"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\portuguese.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "PORTUGUESEBR"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\portuguese-br.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "ROMANIAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\romanian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "RUSSIAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\russian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SERBIAN"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\serbian-latin.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SIMPCHINESE"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\simp-chinese.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SLOVAK"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\slovak.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SLOVENIAN"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\slovenian.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SPANISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\spanish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "SWEDISH"		"${PIDGIN_NSIS_INCLUDE_PATH}\translations\swedish.nsh"
  !insertmacro PIDGIN_MACRO_INCLUDE_LANGFILE "TRADCHINESE"	"${PIDGIN_NSIS_INCLUDE_PATH}\translations\trad-chinese.nsh"

;--------------------------------
;Reserve Files
  ; Only need this if using bzip2 compression

  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
  !insertmacro MUI_RESERVEFILE_LANGDLL
  ReserveFile "${NSISDIR}\Plugins\UserInfo.dll"


;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Start Install Sections ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;--------------------------------
;Uninstall any old version of Pidgin (or Gaim)

Section -SecUninstallOldPidgin
  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  ;First try to uninstall Pidgin
  StrCpy $R4 ${PIDGIN_REG_KEY}
  StrCpy $R5 ${PIDGIN_UNINSTALL_KEY}
  StrCpy $R6 ${PIDGIN_UNINST_EXE}
  StrCpy $R7 "Pidgin"

  start_comparison:
  ;If pidgin is currently set to run on startup,
  ;  save the section of the Registry where the setting is before uninstalling,
  ;  so we can put it back after installing the new version
  ClearErrors
  ReadRegStr $STARTUP_RUN_KEY HKCU "${STARTUP_RUN_KEY}" $R7
  IfErrors +3
  StrCpy $STARTUP_RUN_KEY "HKCU"
  Goto +4
  ClearErrors
  ReadRegStr $STARTUP_RUN_KEY HKLM "${STARTUP_RUN_KEY}" $R7
  IfErrors +2
  StrCpy $STARTUP_RUN_KEY "HKLM"

  StrCmp $R0 "HKLM" compare_hklm
  StrCmp $R0 "HKCU" compare_hkcu done

  compare_hkcu:
      ReadRegStr $R1 HKCU $R4 ""
      ReadRegStr $R2 HKCU $R4 "Version"
      ReadRegStr $R3 HKCU "$R5" "UninstallString"
      Goto try_uninstall

  compare_hklm:
      ReadRegStr $R1 HKLM $R4 ""
      ReadRegStr $R2 HKLM $R4 "Version"
      ReadRegStr $R3 HKLM "$R5" "UninstallString"

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
          CopyFiles /SILENT $R3 "$TEMP\$R6"
          SetOverwrite off
          IfErrors uninstall_problem
            ; Ready to uninstall..
            ClearErrors
            ExecWait '"$TEMP\$R6" /S _?=$R1'
            IfErrors exec_error
              Delete "$TEMP\$R6"
            Goto done

            exec_error:
              Delete "$TEMP\$R6"
              Goto uninstall_problem

        uninstall_problem:
          ; If we couldn't uninstall Pidgin, try to uninstall Gaim
          StrCmp $R4 ${PIDGIN_REG_KEY} cannot_uninstall
          StrCpy $R4 ${OLD_GAIM_REG_KEY}
          StrCpy $R5 ${OLD_GAIM_UNINSTALL_KEY}
          StrCpy $R6 ${OLD_GAIM_UNINST_EXE}
          StrCpy $R7 "Gaim"
          Goto start_comparison

          cannot_uninstall:
          ; We can't uninstall.  Either the user must manually uninstall or we ignore and reinstall over it.
          MessageBox MB_OKCANCEL $(PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL) /SD IDOK IDOK done
          Quit
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

  Call DoWeNeedGtk
  Pop $R0
  Pop $R6

  StrCmp $R0 "0" have_gtk
  StrCmp $R0 "1" upgrade_gtk
  StrCmp $R0 "2" upgrade_gtk
  StrCmp $R0 "3" no_gtk no_gtk

  no_gtk:
    StrCmp $R1 "NONE" gtk_no_install_rights
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /L=$LANGUAGE $ISSILENT /D=$GTK_FOLDER'
    IfErrors gtk_install_error done

  upgrade_gtk:
    StrCpy $GTK_FOLDER $R6
    StrCmp $R0 "2" +2 ; Upgrade isn't optional
    MessageBox MB_YESNO $(GTK_UPGRADE_PROMPT) /SD IDYES IDNO done
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /L=$LANGUAGE /S /D=$GTK_FOLDER'
    IfErrors gtk_install_error done

    gtk_install_error:
      Delete "$TEMP\gtk-runtime.exe"
      MessageBox MB_OK $(GTK_INSTALL_ERROR) /SD IDOK
      Quit

  have_gtk:
    StrCpy $GTK_FOLDER $R6
    StrCmp $R1 "NONE" done ; If we have no rights.. can't re-install..
    ; Even if we have a sufficient version of GTK+, we give user choice to re-install.
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /L=$LANGUAGE $ISSILENT'
    IfErrors gtk_install_error
    Goto done

  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; end got_install rights

  gtk_no_install_rights:
    ; Install GTK+ to Pidgin install dir
    StrCpy $GTK_FOLDER $INSTDIR
    ClearErrors
    ExecWait '"$TEMP\gtk-runtime.exe" /L=$LANGUAGE $ISSILENT /D=$GTK_FOLDER'
    IfErrors gtk_install_error
      SetOverwrite on
      ClearErrors
      CopyFiles /FILESONLY "$GTK_FOLDER\bin\*.dll" $GTK_FOLDER
      SetOverwrite off
      IfErrors gtk_install_error
        Delete "$GTK_FOLDER\bin\*.dll"
        Goto done
  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;
  ; end gtk_no_install_rights

  done:
    Delete "$TEMP\gtk-runtime.exe"
SectionEnd ; end of GTK+ section
!endif

;--------------------------------
;Pidgin Install Section

Section $(PIDGIN_SECTION_TITLE) SecPidgin
  SectionIn 1 RO

  ; Check install rights..
  Call CheckUserInstallRights
  Pop $R0

  ; Get GTK+ lib dir if we have it..

  StrCmp $R0 "NONE" pidgin_none
  StrCmp $R0 "HKLM" pidgin_hklm pidgin_hkcu

  pidgin_hklm:
    ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"
    WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "" "$INSTDIR\pidgin.exe"
    WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "Path" "$R1\bin"
    WriteRegStr HKLM ${PIDGIN_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKLM ${PIDGIN_REG_KEY} "Version" "${PIDGIN_VERSION}"
    WriteRegStr HKLM "${PIDGIN_UNINSTALL_KEY}" "DisplayName" $(PIDGIN_UNINSTALL_DESC)
    WriteRegStr HKLM "${PIDGIN_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${PIDGIN_UNINST_EXE}"
    ; Sets scope of the desktop and Start Menu entries for all users.
    SetShellVarContext "all"
    Goto pidgin_install_files

  pidgin_hkcu:
    ReadRegStr $R1 HKCU ${GTK_REG_KEY} "Path"
    StrCmp $R1 "" 0 +2
      ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"

    WriteRegStr HKCU ${PIDGIN_REG_KEY} "" "$INSTDIR"
    WriteRegStr HKCU ${PIDGIN_REG_KEY} "Version" "${PIDGIN_VERSION}"
    WriteRegStr HKCU "${PIDGIN_UNINSTALL_KEY}" "DisplayName" $(PIDGIN_UNINSTALL_DESC)
    WriteRegStr HKCU "${PIDGIN_UNINSTALL_KEY}" "UninstallString" "$INSTDIR\${PIDGIN_UNINST_EXE}"
    Goto pidgin_install_files

  pidgin_none:
    ReadRegStr $R1 HKLM ${GTK_REG_KEY} "Path"

  pidgin_install_files:
    SetOutPath "$INSTDIR"
    ; Pidgin files
    SetOverwrite on
    File /r ..\..\..\${PIDGIN_INSTALL_DIR}\*.*
    !ifdef DEBUG
    File "${PIDGIN_INSTALLER_DEPS}\exchndl.dll"
    !endif

    ; Install shfolder.dll if need be..
    SearchPath $R4 "shfolder.dll"
    StrCmp $R4 "" 0 got_shfolder
      SetOutPath "$SYSDIR"
      File "${PIDGIN_INSTALLER_DEPS}\shfolder.dll"
      SetOutPath "$INSTDIR"
    got_shfolder:

    ; Check if Perl is installed, if so add it to the AppPaths
    ReadRegStr $R2 HKLM ${PERL_REG_KEY} ""
    StrCmp $R2 "" 0 perl_exists
      ReadRegStr $R2 HKCU ${PERL_REG_KEY} ""
      StrCmp $R2 "" perl_done perl_exists

      perl_exists:
        IfFileExists "$R2\bin\${PERL_DLL}" 0 perl_done
        StrCmp $R0 "HKLM" 0 perl_done
          ReadRegStr $R3 HKLM "${HKLM_APP_PATHS_KEY}" "Path"
          WriteRegStr HKLM "${HKLM_APP_PATHS_KEY}" "Path" "$R3;$R2\bin"

    perl_done:

    ; If this is under NT4, delete the SILC support stuff
    ; there is a bug that will prevent any account from connecting
    ; See https://lists.silcnet.org/pipermail/silc-devel/2005-January/001588.html
    Call GetWindowsVersion
    Pop $R2
    StrCmp $R2 "NT 4.0" +1 +4
    Delete "$INSTDIR\plugins\libsilc.dll"
    Delete "$INSTDIR\silcclient.dll"
    Delete "$INSTDIR\silc.dll"

    SetOutPath "$INSTDIR"

    ; If we don't have install rights.. we're done
    StrCmp $R0 "NONE" done
    SetOverwrite off

    ; write out uninstaller
    SetOverwrite on
    WriteUninstaller "$INSTDIR\${PIDGIN_UNINST_EXE}"
    SetOverwrite off

    ; If we previously had pidgin set up to run on startup, make it do so again
    StrCmp $STARTUP_RUN_KEY "HKCU" +1 +2
    WriteRegStr HKCU "${STARTUP_RUN_KEY}" "Pidgin" "$INSTDIR\pidgin.exe"
    StrCmp $STARTUP_RUN_KEY "HKLM" +1 +2
    WriteRegStr HKLM "${STARTUP_RUN_KEY}" "Pidgin" "$INSTDIR\pidgin.exe"

  done:
SectionEnd ; end of default Pidgin section

;--------------------------------
;Shortcuts

SectionGroup /e $(PIDGIN_SHORTCUTS_SECTION_TITLE) SecShortcuts
  Section /o $(PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE) SecDesktopShortcut
    SetOverwrite on
    CreateShortCut "$DESKTOP\Pidgin.lnk" "$INSTDIR\pidgin.exe"
    SetOverwrite off
  SectionEnd
  Section $(PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE) SecStartMenuShortcut
    SetOverwrite on
    CreateDirectory "$SMPROGRAMS\Pidgin"
    CreateShortCut "$SMPROGRAMS\Pidgin\Pidgin.lnk" "$INSTDIR\pidgin.exe"
    SetOverwrite off
  SectionEnd
SectionGroupEnd

;--------------------------------
;GTK+ Themes

SectionGroup /e $(GTK_THEMES_SECTION_TITLE) SecGtkThemes
  Section /o $(GTK_NOTHEME_SECTION_TITLE) SecGtkNone
    Push "Raleigh"
    Call WriteGtkThemeConfig
  SectionEnd

  Section $(GTK_WIMP_SECTION_TITLE) SecGtkWimp
    Push "MS-Windows"
    Call WriteGtkThemeConfig
  SectionEnd

  Section /o $(GTK_BLUECURVE_SECTION_TITLE) SecGtkBluecurve
    Push "Bluecurve"
    Call WriteGtkThemeConfig
  SectionEnd

  Section /o $(GTK_LIGHTHOUSEBLUE_SECTION_TITLE) SecGtkLighthouseblue
    Push "Lighthouseblue"
    Call WriteGtkThemeConfig
  SectionEnd
SectionGroupEnd

;--------------------------------
;Spell Checking

SectionGroup /e $(PIDGIN_SPELLCHECK_SECTION_TITLE) SecSpellCheck
  Section /o $(PIDGIN_SPELLCHECK_BRETON) SecSpellCheckBreton
    Push ${SecSpellCheckBreton}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_CATALAN) SecSpellCheckCatalan
    Push ${SecSpellCheckCatalan}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_CZECH) SecSpellCheckCzech
    Push ${SecSpellCheckCzech}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_WELSH) SecSpellCheckWelsh
    Push ${SecSpellCheckWelsh}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_DANISH) SecSpellCheckDanish
    Push ${SecSpellCheckDanish}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_GERMAN) SecSpellCheckGerman
    Push ${SecSpellCheckGerman}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_GREEK) SecSpellCheckGreek
    Push ${SecSpellCheckGreek}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_ENGLISH) SecSpellCheckEnglish
    Push ${SecSpellCheckEnglish}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_ESPERANTO) SecSpellCheckEsperanto
    Push ${SecSpellCheckEsperanto}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_SPANISH) SecSpellCheckSpanish
    Push ${SecSpellCheckSpanish}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_FAROESE) SecSpellCheckFaroese
    Push ${SecSpellCheckFaroese}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_FRENCH) SecSpellCheckFrench
    Push ${SecSpellCheckFrench}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_ITALIAN) SecSpellCheckItalian
    Push ${SecSpellCheckItalian}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_DUTCH) SecSpellCheckDutch
    Push ${SecSpellCheckDutch}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_NORWEGIAN) SecSpellCheckNorwegian
    Push ${SecSpellCheckNorwegian}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_POLISH) SecSpellCheckPolish
    Push ${SecSpellCheckPolish}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_PORTUGUESE) SecSpellCheckPortuguese
    Push ${SecSpellCheckPortuguese}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_ROMANIAN) SecSpellCheckRomanian
    Push ${SecSpellCheckRomanian}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_RUSSIAN) SecSpellCheckRussian
    Push ${SecSpellCheckRussian}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_SLOVAK) SecSpellCheckSlovak
    Push ${SecSpellCheckSlovak}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_SWEDISH) SecSpellCheckSwedish
    Push ${SecSpellCheckSwedish}
    Call InstallAspellAndDict
  SectionEnd
  Section /o $(PIDGIN_SPELLCHECK_UKRAINIAN) SecSpellCheckUkrainian
    Push ${SecSpellCheckUkrainian}
    Call InstallAspellAndDict
  SectionEnd
SectionGroupEnd

;--------------------------------
;Uninstaller Section


Section Uninstall
  Call un.CheckUserInstallRights
  Pop $R0
  StrCmp $R0 "NONE" no_rights
  StrCmp $R0 "HKCU" try_hkcu try_hklm

  try_hkcu:
    ReadRegStr $R0 HKCU ${PIDGIN_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 cant_uninstall
      ; HKCU install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKCU ${PIDGIN_REG_KEY}
      DeleteRegKey HKCU "${PIDGIN_UNINSTALL_KEY}"
      Goto cont_uninstall

  try_hklm:
    ReadRegStr $R0 HKLM ${PIDGIN_REG_KEY} ""
    StrCmp $R0 $INSTDIR 0 try_hkcu
      ; HKLM install path matches our INSTDIR.. so uninstall
      DeleteRegKey HKLM ${PIDGIN_REG_KEY}
      DeleteRegKey HKLM "${PIDGIN_UNINSTALL_KEY}"
      DeleteRegKey HKLM "${HKLM_APP_PATHS_KEY}"
      ; Sets start menu and desktop scope to all users..
      SetShellVarContext "all"

  cont_uninstall:
    ; The WinPrefs plugin may have left this behind..
    DeleteRegValue HKCU "${STARTUP_RUN_KEY}" "Pidgin"
    DeleteRegValue HKLM "${STARTUP_RUN_KEY}" "Pidgin"
    ; Remove Language preference info (TODO: check if NSIS removes this)

    RMDir /r "$INSTDIR\locale"
    RMDir /r "$INSTDIR\pixmaps"
    RMDir /r "$INSTDIR\perlmod"
    Delete "$INSTDIR\plugins\autoaccept.dll"
    Delete "$INSTDIR\plugins\autoreply.dll"
    Delete "$INSTDIR\plugins\buddynote.dll"
    Delete "$INSTDIR\plugins\convcolors.dll"
    Delete "$INSTDIR\plugins\extplacement.dll"
    Delete "$INSTDIR\plugins\gaimrc.dll"
    Delete "$INSTDIR\plugins\history.dll"
    Delete "$INSTDIR\plugins\iconaway.dll"
    Delete "$INSTDIR\plugins\idle.dll"
    Delete "$INSTDIR\plugins\libaim.dll"
    Delete "$INSTDIR\plugins\libgg.dll"
    Delete "$INSTDIR\plugins\libicq.dll"
    Delete "$INSTDIR\plugins\libirc.dll"
    Delete "$INSTDIR\plugins\libjabber.dll"
    Delete "$INSTDIR\plugins\libmsn.dll"
    Delete "$INSTDIR\plugins\libnapster.dll"
    Delete "$INSTDIR\plugins\libnovell.dll"
    Delete "$INSTDIR\plugins\libqq.dll"
    Delete "$INSTDIR\plugins\libsametime.dll"
    Delete "$INSTDIR\plugins\libsilc.dll"
    Delete "$INSTDIR\plugins\libsimple.dll"
    Delete "$INSTDIR\plugins\libtoc.dll"
    Delete "$INSTDIR\plugins\libyahoo.dll"
    Delete "$INSTDIR\plugins\log_reader.dll"
    Delete "$INSTDIR\plugins\markerline.dll"
    Delete "$INSTDIR\plugins\newline.dll"
    Delete "$INSTDIR\plugins\notify.dll"
    Delete "$INSTDIR\plugins\offlinemsg.dll"
    Delete "$INSTDIR\plugins\perl.dll"
    Delete "$INSTDIR\plugins\psychic.dll"
    Delete "$INSTDIR\plugins\relnot.dll"
    Delete "$INSTDIR\plugins\spellchk.dll"
    Delete "$INSTDIR\plugins\ssl-nss.dll"
    Delete "$INSTDIR\plugins\ssl.dll"
    Delete "$INSTDIR\plugins\statenotify.dll"
    Delete "$INSTDIR\plugins\tcl.dll"
    Delete "$INSTDIR\plugins\ticker.dll"
    Delete "$INSTDIR\plugins\timestamp.dll"
    Delete "$INSTDIR\plugins\timestamp_format.dll"
    Delete "$INSTDIR\plugins\win2ktrans.dll"
    Delete "$INSTDIR\plugins\winprefs.dll"
    RMDir "$INSTDIR\plugins"
    Delete "$INSTDIR\sounds\gaim\alert.wav"
    Delete "$INSTDIR\sounds\gaim\login.wav"
    Delete "$INSTDIR\sounds\gaim\logout.wav"
    Delete "$INSTDIR\sounds\gaim\receive.wav"
    Delete "$INSTDIR\sounds\gaim\send.wav"
    RMDir "$INSTDIR\sounds\gaim"
    RMDir "$INSTDIR\sounds"
    Delete "$INSTDIR\freebl3.dll"
    Delete "$INSTDIR\idletrack.dll"
    Delete "$INSTDIR\libgtkspell.dll"
    Delete "$INSTDIR\liboscar.dll"
    Delete "$INSTDIR\libpurple.dll"
    Delete "$INSTDIR\libmeanwhile-1.dll"
    Delete "$INSTDIR\libxml2.dll"
    Delete "$INSTDIR\nspr4.dll"
    Delete "$INSTDIR\nss3.dll"
    Delete "$INSTDIR\nssckbi.dll"
    Delete "$INSTDIR\pidgin.exe"
    Delete "$INSTDIR\pidgin.dll"
    Delete "$INSTDIR\plc4.dll"
    Delete "$INSTDIR\plds4.dll"
    Delete "$INSTDIR\silc.dll"
    Delete "$INSTDIR\silcclient.dll"
    Delete "$INSTDIR\softokn3.dll"
    Delete "$INSTDIR\ssl3.dll"
    Delete "$INSTDIR\${PIDGIN_UNINST_EXE}"
    !ifdef DEBUG
    Delete "$INSTDIR\exchndl.dll"
    !endif
    Delete "$INSTDIR\install.log"

    ;Try to remove Pidgin install dir .. if empty
    RMDir "$INSTDIR"

    ; Shortcuts..
    RMDir /r "$SMPROGRAMS\Pidgin"
    Delete "$DESKTOP\Pidgin.lnk"

    Goto done

  cant_uninstall:
    MessageBox MB_OK $(un.PIDGIN_UNINSTALL_ERROR_1) /SD IDOK
    Quit

  no_rights:
    MessageBox MB_OK $(un.PIDGIN_UNINSTALL_ERROR_2) /SD IDOK
    Quit

  done:
SectionEnd ; end of uninstall section

;--------------------------------
;Descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPidgin} \
        $(PIDGIN_SECTION_DESCRIPTION)
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

  !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} \
        $(PIDGIN_SHORTCUTS_SECTION_DESCRIPTION)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktopShortcut} \
        $(PIDGIN_DESKTOP_SHORTCUT_DESC)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenuShortcut} \
        $(PIDGIN_STARTMENU_SHORTCUT_DESC)

  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheck} \
        $(PIDGIN_SPELLCHECK_SECTION_DESCRIPTION)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckBreton} \
        "$(PIDGIN_SPELLCHECK_BRETON) (862kb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckCatalan} \
        "$(PIDGIN_SPELLCHECK_CATALAN) (3.9Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckCzech} \
        "$(PIDGIN_SPELLCHECK_CZECH) (17Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckWelsh} \
        "$(PIDGIN_SPELLCHECK_WELSH) (4.2Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckDanish} \
        "$(PIDGIN_SPELLCHECK_DANISH) (6.9Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckGerman} \
        "$(PIDGIN_SPELLCHECK_GERMAN) (5.4Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckGreek} \
        "$(PIDGIN_SPELLCHECK_GREEK) (7.1Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckEnglish} \
        "$(PIDGIN_SPELLCHECK_ENGLISH) (2.3Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckEsperanto} \
        "$(PIDGIN_SPELLCHECK_ESPERANTO) (5.7Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckSpanish} \
        "$(PIDGIN_SPELLCHECK_SPANISH) (7.0Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckFaroese} \
        "$(PIDGIN_SPELLCHECK_FAROESE) (913kb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckFrench} \
        "$(PIDGIN_SPELLCHECK_FRENCH) (9.3Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckItalian} \
        "$(PIDGIN_SPELLCHECK_ITALIAN) (770kb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckDutch} \
        "$(PIDGIN_SPELLCHECK_DUTCH) (3.7Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckNorwegian} \
        "$(PIDGIN_SPELLCHECK_NORWEGIAN) (3.2Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckPolish} \
        "$(PIDGIN_SPELLCHECK_POLISH) (9.3Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckPortuguese} \
        "$(PIDGIN_SPELLCHECK_PORTUGUESE) (5.5Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckRomanian} \
        "$(PIDGIN_SPELLCHECK_ROMANIAN) (906kb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckRussian} \
        "$(PIDGIN_SPELLCHECK_RUSSIAN) (11Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckSlovak} \
        "$(PIDGIN_SPELLCHECK_SLOVAK) (8.0Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckSwedish} \
        "$(PIDGIN_SPELLCHECK_SWEDISH) (2.2Mb)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSpellCheckUkrainian} \
        "$(PIDGIN_SPELLCHECK_UKRAINIAN) (12Mb)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Functions

Function WriteGtkThemeConfig
  Exch $0
  Push $1
  Push $2
  Push $3

  Call CheckUserInstallRights
  Pop $2
  StrCmp $2 "HKLM" 0 user_theme

  ; Global Theme
  ClearErrors
  ReadRegStr $2 HKLM ${GTK_REG_KEY} "Path"
  IfErrors user_theme
  StrCpy $3 "$2\etc\gtk-2.0\gtkrc"
  Goto update_theme
  user_theme:
  StrCpy $3 "$PROFILE\.gtkrc-2.0"

  update_theme:
  IfFileExists $3 0 new_file
  ${ConfigWriteS} $3 "gtk-theme-name =" " $\"$0$\"" $1
  Goto done

  new_file:
  FileOpen $1 $3 w
  FileWrite $1 "gtk-theme-name = $\"$0$\""
  FileClose $1

  done:
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

!macro CheckUserInstallRightsMacro UN
Function ${UN}CheckUserInstallRights
  Push $0
  Push $1
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
    Exch $1
    Exch
    Pop $0
FunctionEnd
!macroend
!insertmacro CheckUserInstallRightsMacro ""
!insertmacro CheckUserInstallRightsMacro "un."

;
; Usage:
;   Push $0 ; Path string
;   Call VerifyDir
;   Pop $0 ; 0 - Bad path  1 - Good path
;
Function VerifyDir
  Exch $0
  Push $1
  Push $2
  Loop:
    IfFileExists $0 dir_exists
    StrCpy $1 $0 ; save last
    ${GetParent} $0 $0
    StrLen $2 $0
    ; IfFileExists "C:" on xp returns true and on win2k returns false
    ; So we're done in such a case..
    IntCmp $2 2 loop_done
    ; GetParent of "C:" returns ""
    IntCmp $2 0 loop_done
    Goto Loop

  loop_done:
    StrCpy $1 "$0\GaImFooB"
    ; Check if we can create dir on this drive..
    ClearErrors
    CreateDirectory $1
    IfErrors DirBad DirGood

  dir_exists:
    ClearErrors
    FileOpen $1 "$0\pidginfoo.bar" w
    IfErrors PathBad PathGood

    DirGood:
      RMDir $1
      Goto PathGood1

    DirBad:
      RMDir $1
      Goto PathBad1

    PathBad:
      FileClose $1
      Delete "$0\pidginfoo.bar"
      PathBad1:
      StrCpy $0 "0"
      Push $0
      Goto done

    PathGood:
      FileClose $1
      Delete "$0\pidginfoo.bar"
      PathGood1:
      StrCpy $0 "1"
      Push $0

  done:
  Exch 3 ; The top of the stack contains the output variable
  Pop $0
  Pop $2
  Pop $1
FunctionEnd

Function .onVerifyInstDir
  Push $0
  Push $INSTDIR
  Call VerifyDir
  Pop $0
  StrCmp $0 "0" 0 dir_good
  Pop $0
  Abort

  dir_good:
  Pop $0
FunctionEnd

;
; Usage:
; Call DoWeNeedGtk
; First Pop:
;   0 - We have the correct version
;       Second Pop: Key where Version was found
;   1 - We have an old version that should work, prompt user for optional upgrade
;       Second Pop: HKLM or HKCU depending on where GTK was found.
;   2 - We have an old version that needs to be upgraded
;       Second Pop: HKLM or HKCU depending on where GTK was found.
;   3 - We don't have Gtk+ at all
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
  Push $0
  Push $1
  Push $2
  Push $3

  Call CheckUserInstallRights
  Pop $1
  StrCmp $1 "HKLM" check_hklm
  StrCmp $1 "HKCU" check_hkcu check_hklm
    check_hkcu:
      ReadRegStr $0 HKCU ${GTK_REG_KEY} "Version"
      StrCpy $2 "HKCU"
      StrCmp $0 "" check_hklm have_gtk

    check_hklm:
      ReadRegStr $0 HKLM ${GTK_REG_KEY} "Version"
      StrCpy $2 "HKLM"
      StrCmp $0 "" no_gtk have_gtk

  have_gtk:
    ; GTK+ is already installed.. check version.
    ${VersionCompare} ${GTK_INSTALL_VERSION} $0 $3
    IntCmp $3 1 +1 good_version good_version
    ${VersionCompare} ${GTK_MIN_VERSION} $0 $3

      ; Bad version. If hklm ver and we have hkcu or no rights.. return no gtk
      StrCmp $1 "NONE" no_gtk ; if no rights.. can't upgrade
      StrCmp $1 "HKCU" 0 +2   ; if HKLM can upgrade..
      StrCmp $2 "HKLM" no_gtk ; have hkcu rights.. if found hklm ver can't upgrade..
      Push $2
    IntCmp $3 1 +3
      Push "1" ; Optional Upgrade
      Goto done
      Push "2" ; Mandatory Upgrade
      Goto done

  good_version:
    StrCmp $2 "HKLM" have_hklm_gtk have_hkcu_gtk
      have_hkcu_gtk:
        ; Have HKCU version
        ReadRegStr $0 HKCU ${GTK_REG_KEY} "Path"
        Goto good_version_cont

      have_hklm_gtk:
        ReadRegStr $0 HKLM ${GTK_REG_KEY} "Path"
        Goto good_version_cont

    good_version_cont:
      Push $0  ; The path to existing GTK+
      Push "0"
      Goto done

  no_gtk:
    Push $1 ; our rights
    Push "3"
    Goto done

  done:
  ; The top two items on the stack are what we want to return
  Exch 4 
  Pop $0
  Exch 4
  Pop $3
  Pop $2
  Pop $1
FunctionEnd


!macro RunCheckMacro UN
Function ${UN}RunCheck
  Push $R0
  System::Call 'kernel32::OpenMutex(i 2031617, b 0, t "pidgin_is_running") i .R0'
  IntCmp $R0 0 done
    MessageBox MB_OK|MB_ICONEXCLAMATION $(PIDGIN_IS_RUNNING) /SD IDOK
    Abort
  done:
  Pop $R0
FunctionEnd
!macroend
!insertmacro RunCheckMacro ""
!insertmacro RunCheckMacro "un."

Function .onInit
  Push $R0
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "pidgin_installer_running") i .r1 ?e'
  Pop $R0
  StrCmp $R0 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION $(INSTALLER_IS_RUNNING) /SD IDOK
    Abort
  Call RunCheck
  StrCpy $name "Pidgin ${PIDGIN_VERSION}"
  StrCpy $GTK_THEME_SEL ${SecGtkWimp}
  StrCpy $SPELLCHECK_SEL ""

  ;Try to copy the old Gaim installer Lang Reg. key
  ClearErrors
  ReadRegStr $R0 HKCU "${PIDGIN_REG_KEY}" "Installer Language"
  IfErrors 0 +5
  ClearErrors
  ReadRegStr $R0 HKCU "SOFTWARE\gaim" "Installer Language"
  IfErrors +2
  WriteRegStr HKCU "${PIDGIN_REG_KEY}" "Installer Language" "$R0"

  !insertmacro SetSectionFlag ${SecGtkThemes} ${SF_RO}
  !insertmacro UnselectSection ${SecGtkThemes}
  !insertmacro SelectSection $GTK_THEME_SEL
  !insertmacro SetSectionFlag ${SecSpellCheck} ${SF_RO}
  !insertmacro UnselectSection ${SecSpellCheck}

  ;Mark the dictionaries that are already installed as readonly
  Call SelectAndDisableInstalledDictionaries

  StrCpy $ISSILENT "/NOUI"

  ; GTK installer has two silent states.. one with Message boxes, one without
  ; If pidgin installer was run silently, we want to supress gtk installer msg boxes.
  IfSilent 0 set_gtk_normal
      StrCpy $ISSILENT "/S"
  set_gtk_normal:

  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/L=" $R0
  IfErrors +3
  StrCpy $LANGUAGE $R0
  Goto skip_lang

  ; Select Language
    ; Display Language selection dialog
    !insertmacro MUI_LANGDLL_DISPLAY
    skip_lang:

  ; If install path was set on the command, use it.
  StrCmp $INSTDIR "" 0 instdir_done

  ;  If pidgin or gaim is currently installed, we should default to where it is currently installed
  ClearErrors
  ReadRegStr $INSTDIR HKCU "${PIDGIN_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done
  ClearErrors
  ReadRegStr $INSTDIR HKLM "${PIDGIN_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done
  ClearErrors
  ReadRegStr $INSTDIR HKCU "${OLD_GAIM_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done
  ClearErrors
  ReadRegStr $INSTDIR HKLM "${OLD_GAIM_REG_KEY}" ""
  IfErrors +2
  StrCmp $INSTDIR "" 0 instdir_done

  Call CheckUserInstallRights
  Pop $R0

  StrCmp $R0 "HKLM" 0 user_dir
    StrCpy $INSTDIR "$PROGRAMFILES\Pidgin"
    Goto instdir_done
  user_dir:
    Push $SMPROGRAMS
    ${GetParent} $SMPROGRAMS $R2
    ${GetParent} $R2 $R2
    StrCpy $INSTDIR "$R2\Pidgin"

  instdir_done:
  Pop $R0
FunctionEnd

Function un.onInit
  Call un.RunCheck
  StrCpy $name "Pidgin ${PIDGIN_VERSION}"

  ; Get stored language preference
  !insertmacro MUI_UNGETLANGUAGE

FunctionEnd

; This is a modified StartRadioButtons (from Sections.nsh)
; The only difference is that it allows for nothing in the group to be selected
; In that case, the default variable should be set to ""
!macro StartRadioButtonsUnselectable var

  !define StartRadioButtons_Var "${var}"

  Push $R0
  Push $R1

   ;If we have no selection, don't try to unselect it
   StrCmp "${StartRadioButtons_Var}" "" +4
   SectionGetFlags "${StartRadioButtons_Var}" $R0
   IntOp $R1 $R0 & ${SF_SELECTED}
   IntOp $R0 $R0 & ${SECTION_OFF}
   SectionSetFlags "${StartRadioButtons_Var}" $R0

   ; If the previous value isn't currently selected,
   ; we don't want to select it at the end
   IntCmp $R1 ${SF_SELECTED} +2
   StrCpy "${StartRadioButtons_Var}" ""

   StrCpy $R1 "${StartRadioButtons_Var}"

!macroend

Function .onSelChange
  Push $0
  Push $1
  Push $2

  !insertmacro StartRadioButtonsUnselectable $GTK_THEME_SEL
    !insertmacro RadioButton ${SecGtkNone}
    !insertmacro RadioButton ${SecGtkWimp}
    !insertmacro RadioButton ${SecGtkBluecurve}
    !insertmacro RadioButton ${SecGtkLighthouseblue}
  !insertmacro EndRadioButtons

  ; Check that at most one of the non-readonly spelling dictionaries are selected
  ; We can't use $R0 or $R1 in this block since they're used in the macros
  !insertmacro StartRadioButtonsUnselectable $SPELLCHECK_SEL
    ; Start with the first language dictionary
    IntOp $2 ${SecSpellCheck} + 1

    start_spellcheck_radio:
    SectionGetFlags $2 $0

    IntOp $1 $0 & ${SF_SECGRPEND}
    ; If it is the end of the section group, stop
    IntCmp $1 ${SF_SECGRPEND} end_spellcheck_radio

    IntOp $0 $0 & ${SF_RO}
    IntCmp $0 ${SF_RO} after_button_insert
    ; If !readonly, then it is part of the radiobutton group
    !insertmacro RadioButton $2
    after_button_insert:

    IntOp $2 $2 + 1 ;Advance to the next section
    Goto start_spellcheck_radio

    end_spellcheck_radio:
  !insertmacro EndRadioButtons

  Pop $2
  Pop $1
  Pop $0
FunctionEnd

; Page enter and exit functions..

Function preWelcomePage
  Push R0

!ifndef WITH_GTK
  ; If this installer dosn't have GTK, check whether we need it.
  ; We do this here an not in .onInit because language change in
  ; .onInit doesn't take effect until it is finished.
  Call DoWeNeedGtk
  Pop $R0
  Pop $GTK_FOLDER

  IntCmp $R0 1 done done
  MessageBox MB_OK $(GTK_INSTALLER_NEEDED) /SD IDOK
  Quit

  done:

!else
  Push R1

  ; If on Win95/98/ME warn them that the GTK+ version wont work
  Call GetWindowsVersion
  Pop $R1
  StrCmp $R1 "95" win_ver_bad
  StrCmp $R1 "98" win_ver_bad
  StrCmp $R1 "ME" win_ver_bad
  Goto done

  win_ver_bad:
    !insertmacro UnselectSection ${SecGtk}
    !insertmacro SetSectionFlag ${SecGtkNone} ${SF_RO}
    !insertmacro UnselectSection ${SecGtkNone}
    !insertmacro SetSectionFlag ${SecGtkWimp} ${SF_RO}
    !insertmacro UnselectSection ${SecGtkWimp}
    !insertmacro SetSectionFlag ${SecGtkBluecurve} ${SF_RO}
    !insertmacro UnselectSection ${SecGtkBluecurve}
    !insertmacro SetSectionFlag ${SecGtkLighthouseblue} ${SF_RO}
    !insertmacro UnselectSection ${SecGtkLighthouseblue}
    MessageBox MB_OK $(GTK_WINDOWS_INCOMPATIBLE) /SD IDOK
    Call DoWeNeedGtk
    Pop $R0
    Pop $R1
    IntCmp $R0 1 done done ; Upgrade isn't optional - abort if we don't have a suitable version
    Quit

  done:
  Pop $R1
!endif
  Pop $R0
FunctionEnd

!ifdef WITH_GTK
Function preGtkDirPage
  Push $R0
  Push $R1
  Call DoWeNeedGtk
  Pop $R0
  Pop $R1

  IntCmp $R0 2 +2 +2 no_gtk
  StrCmp $R0 "3" no_gtk no_gtk

  ; Don't show dir selector.. Upgrades are done to existing path..
  Pop $R1
  Pop $R0
  Abort

  no_gtk:
    StrCmp $R1 "NONE" 0 no_gtk_cont
      ; Got no install rights..
      Pop $R1
      Pop $R0
      Abort
    no_gtk_cont:
      ; Suggest path..
      StrCmp $R1 "HKCU" 0 hklm1
        ${GetParent} $SMPROGRAMS $R0
        ${GetParent} $R0 $R0
        StrCpy $R0 "$R0\GTK\2.0"
        Goto got_path
      hklm1:
        StrCpy $R0 "${GTK_DEFAULT_INSTALL_PATH}"

   got_path:
     StrCpy $name "GTK+ ${GTK_INSTALL_VERSION}"
     StrCpy $GTK_FOLDER $R0
     Pop $R1
     Pop $R0
FunctionEnd

Function postGtkDirPage
  Push $R0
  StrCpy $name "Pidgin ${PIDGIN_VERSION}"
  Push $GTK_FOLDER
  Call VerifyDir
  Pop $R0
  StrCmp $R0 "0" 0 done
    MessageBox MB_OK $(GTK_BAD_INSTALL_PATH) /SD IDOK
    Pop $R0
    Abort
  done:
  Pop $R0
FunctionEnd
!endif

; GetWindowsVersion
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Updated by Joost Verburg
;
; Returns on top of stack
;
; Windows Version (95, 98, ME, NT x.x, 2000, XP, 2003, Vista)
; or
; '' (Unknown Windows Version)
;
; Usage:
;   Call GetWindowsVersion
;   Pop $R0
;
; at this point $R0 is "NT 4.0" or whatnot
Function GetWindowsVersion

  Push $R0
  Push $R1

  ClearErrors
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion

  IfErrors 0 lbl_winnt

  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber

  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error

  StrCpy $R1 $R0 3

  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98

  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done

  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done

  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done

  lbl_winnt:
    StrCpy $R1 $R0 1

    StrCmp $R1 '3' lbl_winnt_x
    StrCmp $R1 '4' lbl_winnt_x

    StrCpy $R1 $R0 3

    StrCmp $R1 '5.0' lbl_winnt_2000
    StrCmp $R1 '5.1' lbl_winnt_XP
    StrCmp $R1 '5.2' lbl_winnt_2003
    StrCmp $R1 '6.0' lbl_winnt_vista lbl_error

  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done

  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done

  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done

  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done

  lbl_winnt_vista:
    Strcpy $R0 'Vista'
  Goto lbl_done

  lbl_error:
    Strcpy $R0 ''
  lbl_done:

  Pop $R1
  Exch $R0
FunctionEnd

; SpellChecker Related Functions
;-------------------------------

; Convert the a Section index to the language code
; Push the section index onto the stack and pop off the language code after the call
; This will set the error code, if no match is found
Function GetLangCodeForSection
  ClearErrors
  Push $R0
  Exch
  Pop $R0 ;This is the section index

  IntCmp $R0 ${SecSpellCheckBreton} 0 +3 +3
  StrCpy $R0 "br"
  Goto done
  IntCmp $R0 ${SecSpellCheckCatalan} 0 +3 +3
  StrCpy $R0 "ca"
  Goto done
  IntCmp $R0 ${SecSpellCheckCzech} 0 +3 +3
  StrCpy $R0 "cs"
  Goto done
  IntCmp $R0 ${SecSpellCheckWelsh} 0 +3 +3
  StrCpy $R0 "cy"
  Goto done
  IntCmp $R0 ${SecSpellCheckDanish} 0 +3 +3
  StrCpy $R0 "da"
  Goto done
  IntCmp $R0 ${SecSpellCheckGerman} 0 +3 +3
  StrCpy $R0 "de"
  Goto done
  IntCmp $R0 ${SecSpellCheckGreek} 0 +3 +3
  StrCpy $R0 "el"
  Goto done
  IntCmp $R0 ${SecSpellCheckEnglish} 0 +3 +3
  StrCpy $R0 "en"
  Goto done
  IntCmp $R0 ${SecSpellCheckEsperanto} 0 +3 +3
  StrCpy $R0 "eo"
  Goto done
  IntCmp $R0 ${SecSpellCheckSpanish} 0 +3 +3
  StrCpy $R0 "es"
  Goto done
  IntCmp $R0 ${SecSpellCheckFaroese} 0 +3 +3
  StrCpy $R0 "fo"
  Goto done
  IntCmp $R0 ${SecSpellCheckFrench} 0 +3 +3
  StrCpy $R0 "fr"
  Goto done
  IntCmp $R0 ${SecSpellCheckItalian} 0 +3 +3
  StrCpy $R0 "it"
  Goto done
  IntCmp $R0 ${SecSpellCheckDutch} 0 +3 +3
  StrCpy $R0 "nl"
  Goto done
  IntCmp $R0 ${SecSpellCheckNorwegian} 0 +3 +3
  StrCpy $R0 "no"
  Goto done
  IntCmp $R0 ${SecSpellCheckPolish} 0 +3 +3
  StrCpy $R0 "pl"
  Goto done
  IntCmp $R0 ${SecSpellCheckPortuguese} 0 +3 +3
  StrCpy $R0 "pt"
  Goto done
  IntCmp $R0 ${SecSpellCheckRomanian} 0 +3 +3
  StrCpy $R0 "ro"
  Goto done
  IntCmp $R0 ${SecSpellCheckRussian} 0 +3 +3
  StrCpy $R0 "ru"
  Goto done
  IntCmp $R0 ${SecSpellCheckSlovak} 0 +3 +3
  StrCpy $R0 "sk"
  Goto done
  IntCmp $R0 ${SecSpellCheckSwedish} 0 +3 +3
  StrCpy $R0 "sv"
  Goto done
  IntCmp $R0 ${SecSpellCheckUkrainian} 0 +3 +3
  StrCpy $R0 "uk"
  Goto done

  SetErrors

  done:
  Exch $R0
FunctionEnd ;GetLangCodeForSection

; Select and Disable any Sections that have currently installed dictionaries
Function SelectAndDisableInstalledDictionaries
  Push $R0
  Push $R1
  Push $R2

  ; Start with the first language dictionary
  IntOp $R0 ${SecSpellCheck} + 1

  start:
  ; If it is the end of the section group, stop
  SectionGetFlags $R0 $R1
  IntOp $R2 $R1 & ${SF_SECGRPEND}
  IntCmp $R2 ${SF_SECGRPEND} done

  Push $R0
  Call GetLangCodeForSection
  Pop $R2
  IfErrors end_loop
  ReadRegStr $R2 HKLM "${ASPELL_REG_KEY}-$R2" "" ; Check that the dictionary is installed
  StrCmp $R2 "" end_loop ; If it isn't installed, skip to the next item
  IntOp $R1 $R1 | ${SF_RO} ; Mark Readonly
  IntOp $R1 $R1 | ${SF_SELECTED} ; Select
  SectionSetFlags $R0 $R1

  end_loop:
  IntOp $R0 $R0 + 1 ;Advance to the next section
  Goto start

  done:
  Pop $R2
  Pop $R1
  Pop $R0
FunctionEnd

Function InstallAspellAndDict
  Push $R0
  Exch
  Call GetLangCodeForSection
  Pop $R0 ;This is the language code
  Push $R1

  IfErrors done ; We weren't able to convert the section to lang code

  retry:
    Call InstallAspell
    Pop $R1
    StrCmp $R1 "" +3
    StrCmp $R1 "cancel" done
    MessageBox MB_RETRYCANCEL "$(PIDGIN_SPELLCHECK_ERROR) : $R1" /SD IDCANCEL IDRETRY retry IDCANCEL done

  retry_dict:
    Push $R0
    Call InstallAspellDictionary
    Pop $R1
    StrCmp $R1 "" +3
    StrCmp $R1 "cancel" done
    MessageBox MB_RETRYCANCEL "$(PIDGIN_SPELLCHECK_DICT_ERROR) : $R1" /SD IDCANCEL IDRETRY retry_dict

  done:

  Pop $R1
  Pop $R0
FunctionEnd

Function InstallAspell
  Push $R0
  Push $R1
  Push $R2

  check:
  ClearErrors
  ReadRegDWORD $R0 HKLM ${ASPELL_REG_KEY} "AspellVersion"
  IntCmp $R0 15 installed

  ; If this is the check after installation, don't infinite loop on failure
  StrCmp $R1 "$TEMP\aspell_installer.exe" 0 +3
    StrCpy $R0 $(ASPELL_INSTALL_FAILED)
    Goto done

  ; We need to download and install aspell
  StrCpy $R1 "$TEMP\aspell_installer.exe"
  StrCpy $R2 "${DOWNLOADER_URL}?version=${PIDGIN_VERSION}&dl_pkg=aspell_core"
  DetailPrint "Downloading Aspell... ($R2)"
  NSISdl::download $R2 $R1
  Pop $R0
  StrCmp $R0 "success" +2
    Goto done
  ExecWait '"$R1"'
  Delete $R1
  Goto check ; Check that it is now installed correctly

  installed: ;Aspell is currently installed, no error message
    DetailPrint "Aspell is installed"
    StrCpy $R0 ''

  done:
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

Function InstallAspellDictionary
  Push $R0
  Exch
  Pop $R0 ;This is the language code
  Push $R1
  Push $R2
  Push $R3

  check:
  ClearErrors
  ReadRegStr $R2 HKLM "${ASPELL_REG_KEY}-$R0" ""
  StrCmp $R2 "" 0 installed

  ; If this is the check after installation, don't infinite loop on failure
  StrCmp $R1 "$TEMP\aspell_dict-$R0.exe" 0 +3
    StrCpy $R0 $(ASPELL_INSTALL_FAILED)
    Goto done

  ; We need to download and install aspell
  StrCpy $R1 "$TEMP\aspell_dict-$R0.exe"
  StrCpy $R3 "${DOWNLOADER_URL}?version=${PIDGIN_VERSION}&dl_pkg=lang_$R0"
  DetailPrint "Downloading the Aspell $R0 Dictionary... ($R3)"
  NSISdl::download $R3 $R1
  Pop $R3
  StrCmp $R3 "success" +3
    StrCpy $R0 $R3
    Goto done
  ExecWait '"$R1"'
  Delete $R1
  Goto check ; Check that it is now installed correctly

  installed: ;The dictionary is currently installed, no error message
    DetailPrint "Aspell $R0 Dictionary is installed"
    StrCpy $R0 ''

  done:
  Pop $R3
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

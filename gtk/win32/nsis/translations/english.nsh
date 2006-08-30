;;
;;  english.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace "!insertmacro GAIM_MACRO_DEFAULT_STRING"
;;  with "!define".

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!insertmacro GAIM_MACRO_DEFAULT_STRING INSTALLER_IS_RUNNING			"The installer is already running."
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_IS_RUNNING				"An instance of Gaim is currently running. Exit Gaim and then try again."
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_INSTALLER_NEEDED			"The GTK+ runtime environment is either missing or needs to be upgraded.$\rPlease install v${GTK_VERSION} or higher of the GTK+ runtime"

; License Page
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_LICENSE_BUTTON			"Next >"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_LICENSE_BOTTOM_TEXT			"$(^Name) is released under the GNU General Public License (GPL). The license is provided here for information purposes only. $_CLICK"

; Components Page
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (required)"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_SECTION_TITLE			"GTK+ Runtime Environment (required)"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_THEMES_SECTION_TITLE			"GTK+ Themes"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_NOTHEME_SECTION_TITLE		"No Theme"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_WIMP_SECTION_TITLE			"Wimp Theme"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Theme"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_LIGHTHOUSEBLUE_SECTION_TITLE		"Light House Blue Theme"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SHORTCUTS_SECTION_TITLE		"Shortcuts"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menu"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SECTION_DESCRIPTION			"Core Gaim files and dlls"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_SECTION_DESCRIPTION			"A multi-platform GUI toolkit, used by Gaim"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_THEMES_SECTION_DESCRIPTION		"GTK+ Themes can change the look and feel of GTK+ applications."
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_NO_THEME_DESC			"Don't install a GTK+ theme"
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) is a GTK theme that blends well into the Windows desktop environment."
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_BLUECURVE_THEME_DESC			"The Bluecurve theme."
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_LIGHTHOUSEBLUE_THEME_DESC		"The Lighthouseblue theme."
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Shortcuts for starting Gaim"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_DESKTOP_SHORTCUT_DESC		"Create a shortcut to Gaim on the Desktop"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_STARTMENU_SHORTCUT_DESC		"Create a Start Menu entry for Gaim"

; GTK+ Directory Page
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_UPGRADE_PROMPT			"An old version of the GTK+ runtime was found. Do you wish to upgrade?$\rNote: Gaim may not work unless you do."

; Installer Finish Page
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_FINISH_VISIT_WEB_SITE		"Visit the Windows Gaim Web Page"

; Gaim Section Prompts and Texts
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_UNINSTALL_DESC			"Gaim (remove only)"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Unable to uninstall the currently installed version of Gaim. The new version will be installed without removing the currently installed version."

; GTK+ Section Prompts
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_INSTALL_ERROR			"Error installing GTK+ runtime."
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_BAD_INSTALL_PATH			"The path you entered can not be accessed or created."

; GTK+ Themes section
!insertmacro GAIM_MACRO_DEFAULT_STRING GTK_NO_THEME_INSTALL_RIGHTS		"You do not have permission to install a GTK+ theme."

; Uninstall Section Prompts
!insertmacro GAIM_MACRO_DEFAULT_STRING un.GAIM_UNINSTALL_ERROR_1		"The uninstaller could not find registry entries for Gaim.$\rIt is likely that another user installed this application."
!insertmacro GAIM_MACRO_DEFAULT_STRING un.GAIM_UNINSTALL_ERROR_2		"You do not have permission to uninstall this application."

; Spellcheck Section Prompts
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SECTION_TITLE		"Spellchecking Support"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ERROR			"Error Installing Spellchecking"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DICT_ERROR		"Error Installing Spellchecking Dictionary"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Support for Spellchecking.  (Internet connection required for installation)"
!insertmacro GAIM_MACRO_DEFAULT_STRING ASPELL_INSTALL_FAILED			"Installation Failed"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_BRETON			"Breton"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_CATALAN			"Catalan"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_CZECH			"Czech"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_WELSH			"Welsh"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DANISH			"Danish"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_GERMAN			"German"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_GREEK			"Greek"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ENGLISH			"English"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SPANISH			"Spanish"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_FAROESE			"Faroese"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_FRENCH			"French"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ITALIAN			"Italian"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DUTCH			"Dutch"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_NORWEGIAN		"Norwegian"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_POLISH			"Polish"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_PORTUGUESE		"Portuguese"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ROMANIAN			"Romanian"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_RUSSIAN			"Russian"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SLOVAK			"Slovak"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SWEDISH			"Swedish"
!insertmacro GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_UKRAINIAN		"Ukrainian"


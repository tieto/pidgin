;;
;;  english.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!insertmacro PIDGIN_MACRO_DEFAULT_STRING INSTALLER_IS_RUNNING			"The installer is already running."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_IS_RUNNING			"An instance of Pidgin is currently running.  Please exit Pidgin and try again."

; License Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_LICENSE_BUTTON			"Next >"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) is released under the GNU General Public License (GPL). The license is provided here for information purposes only. $_CLICK"

; Components Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (required)"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_SECTION_TITLE			"GTK+ Runtime (required if not present)"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SHORTCUTS_SECTION_TITLE		"Shortcuts"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menu"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING TRANSLATIONS_SECTION_TITLE		"Localizations"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SECTION_DESCRIPTION		"Core Pidgin files and dlls"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING GTK_SECTION_DESCRIPTION		"A multi-platform GUI toolkit, used by Pidgin"

!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Shortcuts for starting Pidgin"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_DESKTOP_SHORTCUT_DESC		"Create a shortcut to Pidgin on the Desktop"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_STARTMENU_SHORTCUT_DESC		"Create a Start Menu entry for Pidgin"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING DEBUG_SYMBOLS_SECTION_TITLE		"Debug Symbols (for reporting crashes)"

; GTK+ Directory Page

; Installer Finish Page
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_FINISH_VISIT_WEB_SITE		"Visit the Pidgin Web Page"

; Pidgin Section Prompts and Texts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Unable to uninstall the currently installed version of Pidgin. The new version will be installed without removing the currently installed version."

; GTK+ Section Prompts

; URL Handler section
!insertmacro PIDGIN_MACRO_DEFAULT_STRING URI_HANDLERS_SECTION_TITLE		"URI Handlers"

; Uninstall Section Prompts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING un.PIDGIN_UNINSTALL_ERROR_1		"The uninstaller could not find registry entries for Pidgin.$\rIt is likely that another user installed this application."
!insertmacro PIDGIN_MACRO_DEFAULT_STRING un.PIDGIN_UNINSTALL_ERROR_2		"You do not have permission to uninstall this application."

; Spellcheck Section Prompts
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SECTION_TITLE	"Spellchecking Support"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_ERROR		"Error Installing Spellchecking"
!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Support for Spellchecking.  (Internet connection required for installation)"

!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_DEBUGSYMBOLS_ERROR		"Error Installing Debug Symbols"

!insertmacro PIDGIN_MACRO_DEFAULT_STRING PIDGIN_GTK_DOWNLOAD_ERROR		"Error Downloading the GTK+ Runtime"


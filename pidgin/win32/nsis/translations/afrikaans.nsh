;;
;;  afrikaans.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Die installeerder loop reeds."
!define PIDGIN_IS_RUNNING			"Pidgin loop reeds êrens.  Verlaat Pidgin eers en probeer dan weer."
!define GTK_INSTALLER_NEEDED			"Die GTK+-looptydomgewing is óf soek óf moet opgegradeer word.$\rInstalleer asb. v{GTK_MIN_VERSION} of hoër van die GTK+-looptyd"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Volgende >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) word vrygestel onderhewig aan die GNU General Public License (GPL). Die lisensie word hier slegs vir kennisname verskaf. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin-kitsboodskapkliënt (benodigd)"
!define GTK_SECTION_TITLE			"GTK+-looptydomgewing (benodigd)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Kortpaaie"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Werkskerm"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Begin-kieslys"
!define PIDGIN_SECTION_DESCRIPTION		"Kern-Pidgin-lêers en DLL'e"
!define GTK_SECTION_DESCRIPTION		"Multi-platform-gereedskap vir programkoppelvlakke, gebruik deur Pidgin"
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Kortpaaie om Pidgin mee te begin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Skep 'n kortpad na Pidgin op die werkskerm"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Skep 'n Begin-kieslysinskrywing vir Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"'n Ou weergawe van die GTK+-looptyd is gevind. Wil u opgradeer?$\rLet wel: $(^Name) werk dalk net as u so maak."
!define GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me is onversoenbaar met GTK+ 2.8.0 en nuwer.  GTK+ ${GTK_INSTALL_VERSION} sal nie installeer nie.$\rIndien u nie GTK+ ${GTK_MIN_VERSION} of nuwer reeds geïnstalleer het nie, sal die installasie nou staak."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besoek die WinPidgin-webblad"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Kan nie die tans geïnstalleerde weergawe van Pidgin verwyder nie. Die nuwe weergawe sal geïnstalleer word sonder om die huidige een te verwyder."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fout met installering van GTK+-looptyd."
!define GTK_BAD_INSTALL_PATH			"Die pad wat u verskaf het, is ontoeganklik of kan nie geskep word nie."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-hanteerders"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Kon nie register-inskrywings vir Piding kry tydens verwydering nie.$\rDit is waarskynlik dat 'n ander gebruiker hierdie program geïnstalleer het."
!define un.PIDGIN_UNINSTALL_ERROR_2		"U het nie toestemming om hierdie toepassing te verwyder nie."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Speltoets-ondersteuning"
!define PIDGIN_SPELLCHECK_ERROR		"Fout met installering van speltoetser"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Fout met installering van speltoetswoordeboek"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Ondersteuning vir speltoeter.  (Internetverbinding benodigd vir installasie)"
!define ASPELL_INSTALL_FAILED			"Installasie het misluk"
!define PIDGIN_SPELLCHECK_BRETON		"Bretons"
!define PIDGIN_SPELLCHECK_CATALAN		"Katalaans"
!define PIDGIN_SPELLCHECK_CZECH		"Tsjeggies"
!define PIDGIN_SPELLCHECK_WELSH		"Wallies"
!define PIDGIN_SPELLCHECK_DANISH		"Deens"
!define PIDGIN_SPELLCHECK_GERMAN		"Duits"
!define PIDGIN_SPELLCHECK_GREEK		"Grieks"
!define PIDGIN_SPELLCHECK_ENGLISH		"Engels"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spaans"
!define PIDGIN_SPELLCHECK_FAROESE		"Faroëes"
!define PIDGIN_SPELLCHECK_FRENCH		"Frans"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italiaans"
!define PIDGIN_SPELLCHECK_DUTCH		"Nederlands"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Noorweegs"
!define PIDGIN_SPELLCHECK_POLISH		"Pools"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugees"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Roemeens"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russies"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slowaaks"
!define PIDGIN_SPELLCHECK_SWEDISH		"Sweeds"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Oekraïens"


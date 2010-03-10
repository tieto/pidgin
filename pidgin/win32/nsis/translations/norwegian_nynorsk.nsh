;;
;;  norwegian_nynorsk.nsh
;;
;;  Norwegian nynorsk language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installasjonsprogrammet kjører allereie."
!define PIDGIN_IS_RUNNING			"Pidgin kjører no. Lukk programmet og prøv igjen."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Neste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) blir utgjeve med ein GNU General Public License (GPL). Lisensen er berre gjeven her for opplysningsformål. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin lynmeldingsklient (påkravd)"
!define GTK_SECTION_TITLE			"GTK+-kjøremiljø (påkravd om det ikkje er til stades no)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Snarvegar"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmenyen"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin programfiler og DLL-ar"
!define GTK_SECTION_DESCRIPTION		"Ei grafisk brukargrensesnittverktøykasse på fleire plattformer som Pidgin nyttar"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Snarvegar for å starta Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Lag ein snarveg til Pidgin på skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Lag ein snarveg til Pidgin på startmenyen"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besøk Pidgin si nettside"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Klarte ikkje å avinstallera Pidgin-utgåva som er i bruk. Den nye utgåva kjem til å bli installert utan å ta vekk den gjeldande."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-referanse"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Avinstallasjonsprogrammet fann ikkje registerpostar for Pidgin.$\rTruleg har ein annan brukar installert denne applikasjonen."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikkje løyve til å kunna avinstallera denne applikasjonen."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Stavekontrollhjelp"
!define PIDGIN_SPELLCHECK_ERROR		"Klarte ikkje å installera stavekontrollen"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Klarte ikkje å installera stavekontrollordlista"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Stavekontrollhjelp (treng internettsamband for å installera)."
!define ASPELL_INSTALL_FAILED			"Installasjonen feila"
!define PIDGIN_SPELLCHECK_BRETON		"Bretonsk"
!define PIDGIN_SPELLCHECK_CATALAN		"Katalansk"
!define PIDGIN_SPELLCHECK_CZECH		"Tsjekkisk"
!define PIDGIN_SPELLCHECK_WELSH		"Walisisk"
!define PIDGIN_SPELLCHECK_DANISH		"Dansk"
!define PIDGIN_SPELLCHECK_GERMAN		"Tysk"
!define PIDGIN_SPELLCHECK_GREEK		"Gresk"
!define PIDGIN_SPELLCHECK_ENGLISH		"Engelsk"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spansk"
!define PIDGIN_SPELLCHECK_FAROESE		"Færøysk"
!define PIDGIN_SPELLCHECK_FRENCH		"Fransk"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italiensk"
!define PIDGIN_SPELLCHECK_DUTCH		"Nederlandsk"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norsk"
!define PIDGIN_SPELLCHECK_POLISH		"Polsk"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugisisk"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumensk"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russisk"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slovakisk"
!define PIDGIN_SPELLCHECK_SWEDISH		"Svensk"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainsk"


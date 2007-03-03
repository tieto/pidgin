;;  vim:syn=winbatch:encoding=cp1252
;;
;;  dutch.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Vincent van Adrighem <vincent@dirck.mine.nu>
;;  Version 2
;;
; Startup Checks
!define INSTALLER_IS_RUNNING			"Er is al een installatie actief."
!define PIDGIN_IS_RUNNING			"Pidgin wordt op dit moment uitgevoerd. Sluit Pidgin af en start de installatie opnieuw."
!define GTK_INSTALLER_NEEDED			"De GTK+ runtime-omgeving is niet aanwezig of moet vernieuwd worden.$\rInstalleer v${GTK_MIN_VERSION} of nieuwer van de GTK+ runtime-omgeving"


; License Page
!define PIDGIN_LICENSE_BUTTON			"Volgende >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) wordt uitgegeven onder de GPL licentie. Deze licentie wordt hier slechts ter informatie aangeboden. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (vereist)"
!define GTK_SECTION_TITLE			"GTK+ runtime-omgeving (vereist)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ thema's"
!define GTK_NOTHEME_SECTION_TITLE		"Geen thema"
!define GTK_WIMP_SECTION_TITLE		"Wimp thema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve thema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue thema"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin hoofdbestanden en dlls"
!define GTK_SECTION_DESCRIPTION		"Een multi-platform gebruikersinterface, gebruikt door Pidgin"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ thema's veranderen het uiterlijk en gedrag van GTK+ programma's."
!define GTK_NO_THEME_DESC			"Geen GTK+ thema installeren"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) is een GTK+ thema wat goed past in een windows omgeving."
!define GTK_BLUECURVE_THEME_DESC		"Het Bluecurve thema (standaardthema voor RedHat Linux)."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Het Lighthouseblue thema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Er is een oude versie van GTK+ gevonden. Wilt u deze bijwerken?$\rLet op: $(^Name) werkt misschien niet als u dit niet doet."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Neem een kijkje op de Windows Pidgin webpagina"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (alleen verwijderen)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fout bij installatie van GTK+ runtime omgeving."
!define GTK_BAD_INSTALL_PATH			"Het door u gegeven pad kan niet benaderd worden."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"U heeft geen toestemming om een GTK+ thema te installeren."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Het verwijderingsprogramma voor Pidgin kon geen register-ingangen voor Pidgin vinden.$\rWaarschijnlijk heeft een andere gebruiker het programma geïnstalleerd."
!define un.PIDGIN_UNINSTALL_ERROR_2         "U mag dit programma niet verwijderen."


; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Spellingscontrole"
!define PIDGIN_SPELLCHECK_ERROR			"Fout bij installatie van spellingscontrole"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Fout bij installatie van woordenboek voor spellingscontrole"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Ondersteuning voor spellingscontrole. (Internetverbinding nodig voor installatie)"
!define ASPELL_INSTALL_FAILED			"Installatie mislukt"
!define PIDGIN_SPELLCHECK_BRETON			"Bretons"
!define PIDGIN_SPELLCHECK_CATALAN			"Catalaans"
!define PIDGIN_SPELLCHECK_CZECH			"Tsjechisch"
!define PIDGIN_SPELLCHECK_WELSH			"Welsh"
!define PIDGIN_SPELLCHECK_DANISH			"Deens"
!define PIDGIN_SPELLCHECK_GERMAN			"Duits"
!define PIDGIN_SPELLCHECK_GREEK			"Grieks"
!define PIDGIN_SPELLCHECK_ENGLISH			"Engels"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Spaans"
!define PIDGIN_SPELLCHECK_FAROESE			"Faroese"
!define PIDGIN_SPELLCHECK_FRENCH			"Frans"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italiaans"
!define PIDGIN_SPELLCHECK_DUTCH			"Nederlands"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Noors"
!define PIDGIN_SPELLCHECK_POLISH			"Pools"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugees"
!define PIDGIN_SPELLCHECK_ROMANIAN			"Roemeens"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Russisch"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slowaaks"
!define PIDGIN_SPELLCHECK_SWEDISH			"Zweeds"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Oekraïns"

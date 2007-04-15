;;  vim:syn=winbatch:encoding=cp1252
;;
;;  dutch.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Vincent van Adrighem <vincent@dirck.mine.nu>
;;  Version 2
;;
; Startup Checks
!define INSTALLER_IS_RUNNING			"Er is al een installatie actief."
!define GAIM_IS_RUNNING			"Gaim wordt op dit moment uitgevoerd. Sluit Gaim af en start de installatie opnieuw."
!define GTK_INSTALLER_NEEDED			"De GTK+ runtime-omgeving is niet aanwezig of moet vernieuwd worden.$\rInstalleer v${GTK_MIN_VERSION} of nieuwer van de GTK+ runtime-omgeving"


; License Page
!define GAIM_LICENSE_BUTTON			"Volgende >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) wordt uitgegeven onder de GPL licentie. Deze licentie wordt hier slechts ter informatie aangeboden. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (vereist)"
!define GTK_SECTION_TITLE			"GTK+ runtime-omgeving (vereist)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ thema's"
!define GTK_NOTHEME_SECTION_TITLE		"Geen thema"
!define GTK_WIMP_SECTION_TITLE		"Wimp thema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve thema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue thema"
!define GAIM_SECTION_DESCRIPTION		"Gaim hoofdbestanden en dlls"
!define GTK_SECTION_DESCRIPTION		"Een multi-platform gebruikersinterface, gebruikt door Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ thema's veranderen het uiterlijk en gedrag van GTK+ programma's."
!define GTK_NO_THEME_DESC			"Geen GTK+ thema installeren"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) is een GTK+ thema wat goed past in een windows omgeving."
!define GTK_BLUECURVE_THEME_DESC		"Het Bluecurve thema (standaardthema voor RedHat Linux)."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Het Lighthouseblue thema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Er is een oude versie van GTK+ gevonden. Wilt u deze bijwerken?$\rLet op: Gaim werkt misschien niet als u dit niet doet."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Neem een kijkje op de Windows Gaim webpagina"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (alleen verwijderen)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fout bij installatie van GTK+ runtime omgeving."
!define GTK_BAD_INSTALL_PATH			"Het door u gegeven pad kan niet benaderd worden."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"U heeft geen toestemming om een GTK+ thema te installeren."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Het verwijderingsprogramma voor Gaim kon geen register-ingangen voor Gaim vinden.$\rWaarschijnlijk heeft een andere gebruiker het programma geïnstalleerd."
!define un.GAIM_UNINSTALL_ERROR_2         "U mag dit programma niet verwijderen."


; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Spellingscontrole"
!define GAIM_SPELLCHECK_ERROR			"Fout bij installatie van spellingscontrole"
!define GAIM_SPELLCHECK_DICT_ERROR		"Fout bij installatie van woordenboek voor spellingscontrole"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Ondersteuning voor spellingscontrole. (Internetverbinding nodig voor installatie)"
!define ASPELL_INSTALL_FAILED			"Installatie mislukt"
!define GAIM_SPELLCHECK_BRETON			"Bretons"
!define GAIM_SPELLCHECK_CATALAN			"Catalaans"
!define GAIM_SPELLCHECK_CZECH			"Tsjechisch"
!define GAIM_SPELLCHECK_WELSH			"Welsh"
!define GAIM_SPELLCHECK_DANISH			"Deens"
!define GAIM_SPELLCHECK_GERMAN			"Duits"
!define GAIM_SPELLCHECK_GREEK			"Grieks"
!define GAIM_SPELLCHECK_ENGLISH			"Engels"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Spaans"
!define GAIM_SPELLCHECK_FAROESE			"Faroese"
!define GAIM_SPELLCHECK_FRENCH			"Frans"
!define GAIM_SPELLCHECK_ITALIAN			"Italiaans"
!define GAIM_SPELLCHECK_DUTCH			"Nederlands"
!define GAIM_SPELLCHECK_NORWEGIAN		"Noors"
!define GAIM_SPELLCHECK_POLISH			"Pools"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugees"
!define GAIM_SPELLCHECK_ROMANIAN			"Roemeens"
!define GAIM_SPELLCHECK_RUSSIAN			"Russisch"
!define GAIM_SPELLCHECK_SLOVAK			"Slowaaks"
!define GAIM_SPELLCHECK_SWEDISH			"Zweeds"
!define GAIM_SPELLCHECK_UKRAINIAN		"Oekraïns"

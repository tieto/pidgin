;;
;;  norwegian.nsh
;;
;;  Norwegian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Jørgen_Vinne_Iversen <jorgenvi@tihlde.org>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installeren kjører allerede."
!define GAIM_IS_RUNNING				"En instans av Gaim kjører fra før. Avslutt Gaim og prøv igjen."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime environment mangler eller trenger en oppgradering.$\rVennligst installér GTK+ v${GTK_VERSION} eller høyere"

; License Page
!define GAIM_LICENSE_BUTTON			"Neste >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) er utgitt under GPL (GNU General Public License). Lisensen er oppgitt her kun med henblikk på informasjon. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Hurtigmeldingsklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Tema"
!define GTK_NOTHEME_SECTION_TITLE		"Ingen tema"
!define GTK_WIMP_SECTION_TITLE			"Wimp-tema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve-tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue-tema"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Snarveier"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebord"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmeny"
!define GAIM_SECTION_DESCRIPTION		"Gaims kjernefiler og dll'er"
!define GTK_SECTION_DESCRIPTION			"Et GUI-verktøy for flere ulike plattformer, brukes av Gaim."
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+ Tema kan endre utseendet og følelsen av GTK+ applikasjoner."
!define GTK_NO_THEME_DESC			"Ikke installér noe GTK+ tema."
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows-imitator) er et GTK-tema som passer godt inn i Windows-miljø."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve-tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue-tema."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Snarveier for å starte Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Lag en snarvei til Gaim på Skrivebordet"
!define GAIM_STARTMENU_SHORTCUT_DESC		"Legg til Gaim i Startmenyen"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"En eldre versjon av GTK+ runtime ble funnet. Ønsker du å oppgradere?$\rMerk: Gaim vil kanskje ikke virke hvis du ikke oppgraderer."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Besøk Gaim for Windows' Nettside"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (kun avinstallering)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"En feil oppstod ved installering av GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Stien du oppga kan ikke aksesseres eller lages."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har ikke rettigheter til å installere et GTK+ tema."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Avinstalleringsprogrammet kunne ikke finne noen registeroppføring for Gaim.$\rTrolig har en annen bruker avinstallert denne applikasjonen."
!define un.GAIM_UNINSTALL_ERROR_2		"Du har ikke rettigheter til å avinstallere denne applikasjonen."



; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Støtte for stavekontroll"
!define GAIM_SPELLCHECK_ERROR			"Det oppstod en feil ved installering av stavekontroll"
!define GAIM_SPELLCHECK_DICT_ERROR		"Det oppstod en feil ved installering av ordboken for stavekontroll"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Støtte for stavekontroll. (Internettoppkobling påkrevd for installasjon)"
!define ASPELL_INSTALL_FAILED			"Installasjonen mislyktes."
!define GAIM_SPELLCHECK_BRETON			"Bretagnsk"
!define GAIM_SPELLCHECK_CATALAN			"Katalansk"
!define GAIM_SPELLCHECK_CZECH			"Tsjekkisk"
!define GAIM_SPELLCHECK_WELSH			"Walisisk"
!define GAIM_SPELLCHECK_DANISH			"Dansk"
!define GAIM_SPELLCHECK_GERMAN			"Tysk"
!define GAIM_SPELLCHECK_GREEK			"Gresk"
!define GAIM_SPELLCHECK_ENGLISH			"Engelsk"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Spansk"
!define GAIM_SPELLCHECK_FAROESE			"Færøysk"
!define GAIM_SPELLCHECK_FRENCH			"Fransk"
!define GAIM_SPELLCHECK_ITALIAN			"Italiensk"
!define GAIM_SPELLCHECK_DUTCH			"Nederlandsk"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norsk"
!define GAIM_SPELLCHECK_POLISH			"Polsk"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugisisk"
!define GAIM_SPELLCHECK_ROMANIAN		"Rumensk"
!define GAIM_SPELLCHECK_RUSSIAN			"Russisk"
!define GAIM_SPELLCHECK_SLOVAK			"Slovakisk"
!define GAIM_SPELLCHECK_SWEDISH			"Svensk"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrainsk"

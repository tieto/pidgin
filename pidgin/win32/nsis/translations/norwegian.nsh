;;
;;  norwegian.nsh
;;
;;  Norwegian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Jørgen_Vinne_Iversen <jorgenvi@tihlde.org>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installeren kjører allerede."
!define PIDGIN_IS_RUNNING				"En instans av Pidgin kjører fra før. Avslutt Pidgin og prøv igjen."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime environment mangler eller trenger en oppgradering.$\rVennligst installér GTK+ v${GTK_MIN_VERSION} eller høyere"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Neste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) er utgitt under GPL (GNU General Public License). Lisensen er oppgitt her kun med henblikk på informasjon. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Hurtigmeldingsklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Tema"
!define GTK_NOTHEME_SECTION_TITLE		"Ingen tema"
!define GTK_WIMP_SECTION_TITLE			"Wimp-tema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve-tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue-tema"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Snarveier"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebord"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmeny"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgins kjernefiler og dll'er"
!define GTK_SECTION_DESCRIPTION			"Et GUI-verktøy for flere ulike plattformer, brukes av Pidgin."
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+ Tema kan endre utseendet og følelsen av GTK+ applikasjoner."
!define GTK_NO_THEME_DESC			"Ikke installér noe GTK+ tema."
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows-imitator) er et GTK-tema som passer godt inn i Windows-miljø."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve-tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue-tema."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Snarveier for å starte Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Lag en snarvei til Pidgin på Skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Legg til Pidgin i Startmenyen"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"En eldre versjon av GTK+ runtime ble funnet. Ønsker du å oppgradere?$\rMerk: $(^Name) vil kanskje ikke virke hvis du ikke oppgraderer."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besøk Pidgin for Windows' Nettside"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (kun avinstallering)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"En feil oppstod ved installering av GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Stien du oppga kan ikke aksesseres eller lages."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har ikke rettigheter til å installere et GTK+ tema."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Avinstalleringsprogrammet kunne ikke finne noen registeroppføring for Pidgin.$\rTrolig har en annen bruker avinstallert denne applikasjonen."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikke rettigheter til å avinstallere denne applikasjonen."



; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Støtte for stavekontroll"
!define PIDGIN_SPELLCHECK_ERROR			"Det oppstod en feil ved installering av stavekontroll"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Det oppstod en feil ved installering av ordboken for stavekontroll"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Støtte for stavekontroll. (Internettoppkobling påkrevd for installasjon)"
!define ASPELL_INSTALL_FAILED			"Installasjonen mislyktes."
!define PIDGIN_SPELLCHECK_BRETON			"Bretagnsk"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalansk"
!define PIDGIN_SPELLCHECK_CZECH			"Tsjekkisk"
!define PIDGIN_SPELLCHECK_WELSH			"Walisisk"
!define PIDGIN_SPELLCHECK_DANISH			"Dansk"
!define PIDGIN_SPELLCHECK_GERMAN			"Tysk"
!define PIDGIN_SPELLCHECK_GREEK			"Gresk"
!define PIDGIN_SPELLCHECK_ENGLISH			"Engelsk"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Spansk"
!define PIDGIN_SPELLCHECK_FAROESE			"Færøysk"
!define PIDGIN_SPELLCHECK_FRENCH			"Fransk"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italiensk"
!define PIDGIN_SPELLCHECK_DUTCH			"Nederlandsk"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norsk"
!define PIDGIN_SPELLCHECK_POLISH			"Polsk"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugisisk"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumensk"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Russisk"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovakisk"
!define PIDGIN_SPELLCHECK_SWEDISH			"Svensk"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainsk"

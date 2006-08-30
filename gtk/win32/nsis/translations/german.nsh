;; vim:syn=winbatch:encoding=cp1252:
;;
;;  german.nsh
;;
;;  German language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Bjoern Voigt <bjoern@cs.tu-berlin.de>, 2006.
;;  Version 3
;;
 
; Startup checks
!define INSTALLER_IS_RUNNING			"Der Installer läuft schon."
!define GAIM_IS_RUNNING				"Eine Instanz von Gaim läuft momentan schon. Beenden Sie Gaim und versuchen Sie es nochmal."
!define GTK_INSTALLER_NEEDED			"Die GTK+ Runtime Umgebung fehlt entweder oder muß aktualisiert werden.$\rBitte installieren Sie v${GTK_VERSION} oder höher der GTK+ Runtime"
 
; License Page
!define GAIM_LICENSE_BUTTON			"Weiter >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) wird unter der GNU General Public License (GPL) veröffentlicht. Die Lizenz dient hier nur der Information. $_CLICK"
 
; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (erforderlich)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Umgebung (erforderlich)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Themen"
!define GTK_NOTHEME_SECTION_TITLE		"Kein Thema"
!define GTK_WIMP_SECTION_TITLE		"Wimp Thema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Thema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue Thema"
!define GAIM_SHORTCUTS_SECTION_TITLE	"Verknüpfungen"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmenü"
!define GAIM_SECTION_DESCRIPTION		"Gaim-Basisdateien und -DLLs"
!define GTK_SECTION_DESCRIPTION		"Ein Multi-Plattform-GUI-Toolkit, verwendet von Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ Themen können Aussehen und Bedienung von GTK+ Anwendungen verändern."
!define GTK_NO_THEME_DESC			"Installiere kein GTK+ Thema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows Imitator) ist ein GTK Theme, das sich besonders gut in den Windows Desktop integriert."
!define GTK_BLUECURVE_THEME_DESC		"Das Bluecurve Thema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Das Lighthouseblue Thema."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Verknüpfungen zum Starten von Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   "Erstellt eine Verknüpfung zu Gaim auf dem Desktop"
!define GAIM_STARTMENU_SHORTCUT_DESC   "Erstellt einen Eintrag für Gaim im Startmenü"
 
; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Eine alte Version der GTK+ Runtime wurde gefunden. Möchten Sie aktualisieren?$\rHinweis: Gaim funktioniert evtl. nicht, wenn Sie nicht aktualisieren."
 
; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Besuchen Sie die Windows Gaim Webseite"
 
; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (nur entfernen)"
 
; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fehler beim Installieren der GTK+ Runtime."
!define GTK_BAD_INSTALL_PATH			"Der Pfad, den Sie eingegeben haben, existiert nicht und kann nicht erstellt werden."
 
; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Sie haben keine Berechtigung, um ein GTK+ Theme zu installieren."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Der Deinstaller konnte keine Registrierungsschlüssel für Gaim finden.$\rEs ist wahrscheinlich, daß ein anderer Benutzer diese Anwendunng installiert hat."
!define un.GAIM_UNINSTALL_ERROR_2		"Sie haben keine Berechtigung, diese Anwendung zu deinstallieren."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Unterstützung für Rechtschreibkontrolle"
!define GAIM_SPELLCHECK_ERROR			"Fehler bei der Installation der Rechtschreibkontrolle"
!define GAIM_SPELLCHECK_DICT_ERROR		"Fehler bei der Installation des Wörterbuches für die Rechtschreibkontrolle"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Unterstützung für Rechtschreibkontrolle.  (Für die Installation ist eine Internet-Verbindung nötig)"
!define ASPELL_INSTALL_FAILED			"Installation gescheitert"
!define GAIM_SPELLCHECK_BRETON			"Bretonisch"
!define GAIM_SPELLCHECK_CATALAN			"Katalanisch"
!define GAIM_SPELLCHECK_CZECH			"Tschechisch"
!define GAIM_SPELLCHECK_WELSH			"Walisisch"
!define GAIM_SPELLCHECK_DANISH			"Dänisch"
!define GAIM_SPELLCHECK_GERMAN			"Deutsch"
!define GAIM_SPELLCHECK_GREEK			"Griechisch"
!define GAIM_SPELLCHECK_ENGLISH			"Englisch"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Spanisch"
!define GAIM_SPELLCHECK_FAROESE			"Faröersprache"
!define GAIM_SPELLCHECK_FRENCH			"Französisch"
!define GAIM_SPELLCHECK_ITALIAN			"Italienisch"
!define GAIM_SPELLCHECK_DUTCH			"Holländisch"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norwegisch"
!define GAIM_SPELLCHECK_POLISH			"Polnisch"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugiesisch"
!define GAIM_SPELLCHECK_ROMANIAN		"Rumänisch"
!define GAIM_SPELLCHECK_RUSSIAN			"Russisch"
!define GAIM_SPELLCHECK_SLOVAK			"Slowakisch"
!define GAIM_SPELLCHECK_SWEDISH			"Schwedisch"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrainisch"

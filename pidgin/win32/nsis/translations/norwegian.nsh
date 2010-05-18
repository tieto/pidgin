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

; License Page
!define PIDGIN_LICENSE_BUTTON			"Neste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) er utgitt under GPL (GNU General Public License). Lisensen er oppgitt her kun med henblikk på informasjon. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Hurtigmeldingsklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Snarveier"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Skrivebord"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Startmeny"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgins kjernefiler og dll'er"
!define GTK_SECTION_DESCRIPTION			"Et GUI-verktøy for flere ulike plattformer, brukes av Pidgin."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Snarveier for å starte Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Lag en snarvei til Pidgin på Skrivebordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Legg til Pidgin i Startmenyen"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besøk Pidgin for Windows' Nettside"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Avinstalleringsprogrammet kunne ikke finne noen registeroppføring for Pidgin.$\rTrolig har en annen bruker avinstallert denne applikasjonen."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikke rettigheter til å avinstallere denne applikasjonen."



; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Støtte for stavekontroll"
!define PIDGIN_SPELLCHECK_ERROR			"Det oppstod en feil ved installering av stavekontroll"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Støtte for stavekontroll. (Internettoppkobling påkrevd for installasjon)"

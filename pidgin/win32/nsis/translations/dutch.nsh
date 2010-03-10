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


; License Page
!define PIDGIN_LICENSE_BUTTON			"Volgende >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) wordt uitgegeven onder de GPL licentie. Deze licentie wordt hier slechts ter informatie aangeboden. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (vereist)"
!define GTK_SECTION_TITLE			"GTK+ runtime-omgeving (vereist)"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin hoofdbestanden en dlls"
!define GTK_SECTION_DESCRIPTION		"Een multi-platform gebruikersinterface, gebruikt door Pidgin"


; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Neem een kijkje op de Windows Pidgin webpagina"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Het verwijderingsprogramma voor Pidgin kon geen register-ingangen voor Pidgin vinden.$\rWaarschijnlijk heeft een andere gebruiker het programma geïnstalleerd."
!define un.PIDGIN_UNINSTALL_ERROR_2         "U mag dit programma niet verwijderen."


; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Spellingscontrole"
!define PIDGIN_SPELLCHECK_ERROR			"Fout bij installatie van spellingscontrole"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Ondersteuning voor spellingscontrole. (Internetverbinding nodig voor installatie)"

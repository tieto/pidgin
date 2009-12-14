;;
;;  danish.nsh
;;
;;  Danish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Ewan Andreasen <wiredloose@myrealbox.com>
;;  Version 2
;;

; License Page
!define PIDGIN_LICENSE_BUTTON			"Næste >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) er frigivet under GPL licensen. Licensen er kun medtaget her til generel orientering. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define PIDGIN_SECTION_DESCRIPTION		"Basale Pidgin filer og biblioteker"
!define GTK_SECTION_DESCRIPTION		"Et multi-platform grafisk interface udviklingsværktøj, bruges af Pidgin"


; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besøg Windows Pidgin's hjemmeside"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Afinstallationen kunne ikke finde Pidgin i registreringsdatabasen.$\rMuligvis har en anden bruger installeret programmet."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Du har ikke tilladelse til at afinstallere dette program."


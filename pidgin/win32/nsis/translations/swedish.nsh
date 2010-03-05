;;
;;  swedish.nsh
;;
;;  Swedish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Tore Lundqvist <tlt@mima.x.se>, 2003.
;;  Author: Peter Hjalmarsson <xake@telia.com>, 2005.
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"Installationsprogrammet körs redan."
!define PIDGIN_IS_RUNNING			"En instans av Pidgin körs redan. Avsluta Pidgin och försök igen."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Nästa >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) är utgivet under GPL. Licensen finns tillgänglig här för informationssyften enbart. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Snabbmeddelandeklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+-körmiljö (obligatorisk)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Genvägar"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Skrivbord"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Startmeny"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgins kärnfiler och DLL:er"
!define GTK_SECTION_DESCRIPTION			"En GUI-verktygsuppsättning för flera olika plattformar som Pidgin använder."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Genvägar för att starta Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Skapar en genväg till Pidgin på skrivbordet"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   	"Skapar ett tillägg i startmenyn för Pidgin"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Besök Windows-Pidgin hemsida"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Kunde inte avinstallera den nuvarande versionen av Pidgin. Den nya versionen kommer att installeras utan att ta bort den för närvarande installerade versionen."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI Hanterare"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         	"Avinstalleraren kunde inte hitta registervärden för Pidgin.$\rAntagligen har en annan användare installerat applikationen."
!define un.PIDGIN_UNINSTALL_ERROR_2         	"Du har inte rättigheter att avinstallera den här applikationen."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Stöd för rättstavning"
!define PIDGIN_SPELLCHECK_ERROR			"Fel vid installation för rättstavning"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Stöd för Rättstavning.  (Internetanslutning krävs för installation)"

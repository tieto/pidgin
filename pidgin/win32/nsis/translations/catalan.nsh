;;  vim:syn=winbatch:encoding=cp1252:
;;
;;  catalan.nsh
;;
;;  Catalan language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: "Bernat López" <bernatl@adequa.net>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"L'instal.lador encara està executant-se."
!define PIDGIN_IS_RUNNING			"Hi ha una instància del Pidgin executant-se. Surt del Pidgin i torna a intentar-ho."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Següent >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) és distribuït sota llicència GPL. Podeu consultar la llicència, només per proposits informatius, aquí.  $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client Pidgin de Missatgeria Instantània (necessari)"
!define GTK_SECTION_TITLE			"Entorn d'Execució GTK+ (necessari)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Enllaços directes"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Escriptori"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Inici"
!define PIDGIN_SECTION_DESCRIPTION		"Fitxers i dlls del nucli de Pidgin"
!define GTK_SECTION_DESCRIPTION			"Una eina IGU multiplataforma, utilitzada per Pidgin"


!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Enllaços directes per iniciar el Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Afegir un enllaç directe al Pidgin a l'Escriptori"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Crear una entrada Pidgin al Menu Inici"


; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visita la pàgina web de Pidgin per Windows"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"L'instal.lador podria no trobar les entrades del registre de Pidgin.$\rProbablement un altre usuari ha instal.lat aquesta aplicació."
!define un.PIDGIN_UNINSTALL_ERROR_2		"No tens permís per desinstal.lar aquesta aplicació."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Suport a la Verificació de l'Ortografia "
!define PIDGIN_SPELLCHECK_ERROR			"Error instal.lant verificació de l'ortografia"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Suport per a Verificació de l'Ortografia.  (és necesaria connexió a internet per dur a terme la instal.lació)"


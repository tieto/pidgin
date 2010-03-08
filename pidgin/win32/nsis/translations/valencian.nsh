;;
;;  valencian.nsh
;;
;;  Valencian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace "!insertmacro PIDGIN_MACRO_DEFAULT_STRING"
;;  with "!define".

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"L'instalador encara està eixecutant-se."
!define PIDGIN_IS_RUNNING				"Una instancia de Pidgin està eixecutant-se. Ix del Pidgin i torna a intentar-ho."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Següent >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) es distribuit baix llicencia GNU General Public License (GPL). La llicencia es proporcionada per proposits informatius aci. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client Mensageria Instantanea Pidgin (necessari)"
!define GTK_SECTION_TITLE			"Entorn d'Eixecucio GTK+ (necessari)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Enllaços directes"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Escritori"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE 	"Menu d'Inici"
!define PIDGIN_SECTION_DESCRIPTION		"Archius i dlls del nucleu de Pidgin"
!define GTK_SECTION_DESCRIPTION			"Una ferramenta multi-plataforma GUI, usada per Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Enllaços directes per a iniciar Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Crear un enllaç directe a Pidgin en l'Escritori"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   		"Crear una entrada per a Pidgin en Menu Inici"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visita la pagina de Pidgin per a Windows"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"El desinstalador podria no trobar les entrades del registre de Pidgin.$\rProbablement un atre usuari instalà esta aplicacio."
!define un.PIDGIN_UNINSTALL_ERROR_2		"No tens permis per a desinstalar esta aplicacio."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Soport de Correccio Ortografica"
!define PIDGIN_SPELLCHECK_ERROR			"Erro Instalant Correccio Ortografica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Soport per a Correccio Ortografica.  (es requerix conexio a Internet per a fer l'instalacio)"


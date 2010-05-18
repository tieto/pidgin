;;
;;  spanish.nsh
;;
;;  Spanish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;  Translator: Javier Fernandez-Sanguino Peña
;;
;;  Version 2
;;

; License Page
!define PIDGIN_LICENSE_BUTTON			"Siguiente >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) se distribuye bajo la licencia GPL. Esta licencia se incluye aquí sólo con propósito informativo: $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de mensajería instantánea de Pidgin (necesario)"
!define GTK_SECTION_TITLE			"Entorno de ejecución de GTK+ (necesario)"
!define PIDGIN_SECTION_DESCRIPTION		"Ficheros y dlls principales de Core"
!define GTK_SECTION_DESCRIPTION		"Una suite de herramientas GUI multiplataforma, utilizada por Pidgin"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite la página Web de Pidgin Windows"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "El desinstalador no pudo encontrar las entradas en el registro de Pidgin.$\rEs probable que otro usuario instalara la aplicación."
!define un.PIDGIN_UNINSTALL_ERROR_2         "No tiene permisos para desinstalar esta aplicación."

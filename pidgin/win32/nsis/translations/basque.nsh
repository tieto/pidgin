;;
;;  basque.nsh
;;Abio-menua - Istanteko Mezularitza
;;  Basque language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Mikel Pascual Aldabaldetreku <mikel.paskual@gmail.com>, 2007.

; Startup Checks
!define INSTALLER_IS_RUNNING			"Instalatzailea martxan dago."
!define PIDGIN_IS_RUNNING			"Pidgin istantzia bat dago martxan. Pidgin itxi eta berriro saiatu."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Jarraitu >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"GNU Lizentzia Orokor Publikopean (GPL) argitaratzen da $(^Name). Informatzeko helburu soilarekin aurkezten da hemen lizentzia. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Istanteko Mezularitza Bezeroa (beharrezkoa)"
!define GTK_SECTION_TITLE			"GTK+ exekuzio ingurunea (beharrezkoa)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Lasterbideak"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Mahaigaina"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Abio-menua"
!define PIDGIN_SECTION_DESCRIPTION		"Funtsezko Pidgin fitxategi eta dll-ak"
!define GTK_SECTION_DESCRIPTION		"Plataforma anitzeko GUI tresna-sorta, Pidgin-ek erabilia"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin abiarazteko lasterbideak"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Pidgin-entzako lasterbidea Mahaigainean"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Pidgin-entzako lasterbidea Abio-Menuan"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Pidgin Webgunera etorri"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Ezin izan da jadanik instalatuta zegoen Pidgin bertsioa kendu. Aurreko bertsioa kendu gabe instalatuko da bertsio berria."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI Kudeatzaileak"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Ezin izan dira Pidgin-en erregistro-sarrerak aurkitu.$\rZiurrenik, beste erabiltzaile batek instalatu zuen aplikazio hau."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Ez daukazu aplikazio hau kentzeko baimenik."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Zuzentzaile Ortografikoa"
!define PIDGIN_SPELLCHECK_ERROR		"Errorea Zuzentzaile Ortografikoa instalatzean"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Zuzentzaile Ortografikoa.  (Internet konexioa behar du instalatzeko)"


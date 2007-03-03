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
!define GTK_INSTALLER_NEEDED			"L'entorn d'eixecucio GTK+ no es troba o necessita ser actualisat.$\rPer favor instala la versio${GTK_MIN_VERSION} o superior de l'entorn GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Següent >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) es distribuit baix llicencia GNU General Public License (GPL). La llicencia es proporcionada per proposits informatius aci. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client Mensageria Instantanea Pidgin (necessari)"
!define GTK_SECTION_TITLE			"Entorn d'Eixecucio GTK+ (necessari)"
!define GTK_THEMES_SECTION_TITLE		"Temes GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Sense Tema"
!define GTK_WIMP_SECTION_TITLE			"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Enllaços directes"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Escritori"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE 	"Menu d'Inici"
!define PIDGIN_SECTION_DESCRIPTION		"Archius i dlls del nucleu de Pidgin"
!define GTK_SECTION_DESCRIPTION			"Una ferramenta multi-plataforma GUI, usada per Pidgin"
!define GTK_THEMES_SECTION_DESCRIPTION		"Els Temes GTK+ poden canviar l'aspecte de les aplicacions GTK+."
!define GTK_NO_THEME_DESC			"No instalar un tema GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (imitador Windows) es un tema GTK que s'integra perfectament en l'entorn d'escritori de Windows."
!define GTK_BLUECURVE_THEME_DESC		"El tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"El tema Lighthouseblue."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Enllaços directes per a iniciar Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Crear un enllaç directe a Pidgin en l'Escritori"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   		"Crear una entrada per a Pidgin en Menu Inici"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Una versio antiua de l'entorn GTK+ fon trobada. ¿Vols actualisar-la?$\rNota: $(^Name) no funcionarà si no ho fas."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visita la pagina de Pidgin per a Windows"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (nomes borrar)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro instalant l'entorn GTK+."
!define GTK_BAD_INSTALL_PATH			"La ruta introduida no pot ser accedida o creada."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"No tens permissos per a instalar un tema GTK+."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"El desinstalador podria no trobar les entrades del registre de Pidgin.$\rProbablement un atre usuari instalà esta aplicacio."
!define un.PIDGIN_UNINSTALL_ERROR_2		"No tens permis per a desinstalar esta aplicacio."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Soport de Correccio Ortografica"
!define PIDGIN_SPELLCHECK_ERROR			"Erro Instalant Correccio Ortografica"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Erro Instalant Diccionari de Correccio Ortografica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Soport per a Correccio Ortografica.  (es requerix conexio a Internet per a fer l'instalacio)"
!define ASPELL_INSTALL_FAILED			"L'Instalacio fallà"
!define PIDGIN_SPELLCHECK_BRETON			"Breto"
!define PIDGIN_SPELLCHECK_CATALAN			"Català"
!define PIDGIN_SPELLCHECK_CZECH			"Chec"
!define PIDGIN_SPELLCHECK_WELSH			"Galés"
!define PIDGIN_SPELLCHECK_DANISH			"Danes"
!define PIDGIN_SPELLCHECK_GERMAN			"Alemà"
!define PIDGIN_SPELLCHECK_GREEK			"Grec"
!define PIDGIN_SPELLCHECK_ENGLISH			"Angles"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Espanyol"
!define PIDGIN_SPELLCHECK_FAROESE			"Feroes"
!define PIDGIN_SPELLCHECK_FRENCH			"Frances"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italià"
!define PIDGIN_SPELLCHECK_DUTCH			"Holandes"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Noruec"
!define PIDGIN_SPELLCHECK_POLISH			"Polac"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugues"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Romanes"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Rus"
!define PIDGIN_SPELLCHECK_SLOVAK			"Eslovac"
!define PIDGIN_SPELLCHECK_SWEDISH			"Suec"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ucranià"


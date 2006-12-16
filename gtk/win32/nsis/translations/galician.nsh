;;
;;  galician.nsh
;;
;;  Galician language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Translator: Ignacio Casal Quinteiro 
;;  Version 1
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"O entorno de execución de GTK+ falta ou necesita ser actualizado.$\rPor favor, instale a versión v${GTK_VERSION} do executable GTK+ ou algunha posterior."

; License Page
!define GAIM_LICENSE_BUTTON			"Seguinte >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) distribúese baixo a licencia GPL. Esta licencia inclúese aquí só con propósito informativo: $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Cliente de mensaxería instantánea de Gaim (necesario)"
!define GTK_SECTION_TITLE			"Entorno de execución de GTK+ (necesario)"
!define GTK_THEMES_SECTION_TITLE		"Temas GTK+" 
!define GTK_NOTHEME_SECTION_TITLE		"Sen tema"
!define GTK_WIMP_SECTION_TITLE		"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SECTION_DESCRIPTION		"Ficheiros e dlls principais de Core"
!define GTK_SECTION_DESCRIPTION		"Unha suite de ferramentas GUI multiplataforma, utilizada por Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Os temas poden cambiar a apariencia de aplicacións GTK+."
!define GTK_NO_THEME_DESC			"Non instalar un tema GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) é un tema GTK que se mestura moi bien co entorno de escritorio de Windows."
!define GTK_BLUECURVE_THEME_DESC		"O tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"O tema Lighthouseblue."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Atopouse unha versión antiga do executable de GTK+. ¿Desexa actualizala?$\rObservación: Gaim non funcionará a menos que o faga."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visite a páxina Web de Gaim Windows"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (sólo eliminar)"
!define GAIM_PROMPT_WIPEOUT			"O seu directorio antigo de Gaim vai ser borrado. ¿Desexa continuar?$\r$\rObservación: calquer aplique non estándar que puidera haber instalado borrarase.$\rIsto non afectará ás súas preferencias de usuario en Gaim."
!define GAIM_PROMPT_DIR_EXISTS		"O directorio de instalación que especificou xa existe. Todos os contidos$\rserán borrados. ¿Desexa continuar?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o executable GTK+."
!define GTK_BAD_INSTALL_PATH			"Non se puido acceder ou crear a ruta que vd. indicou."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Non ten permisos para instalar un tema GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "O desinstalador non puido atopar as entradas no rexistro de Gaim.$\rÉ probable que outro usuario instalara a aplicación."
!define un.GAIM_UNINSTALL_ERROR_2         "Non ten permisos para desinstalar esta aplicación."

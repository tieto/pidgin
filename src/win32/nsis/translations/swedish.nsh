;;
;;  swedish.nsh
;;
;;  Swedish language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Tore Lundqvist <tlt@mima.x.se>, 2003.
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"$\"GTK+ runtime environment$\" saknas eller behöver uppgraderas$\rInstallera v${GTK_VERSION} eller högre av GTK+."

; Components Page
!define GAIM_SECTION_TITLE			"Gaim-snabbmeddelandeklient (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+teman"
!define GTK_NOTHEME_SECTION_TITLE		"Inget tema"
!define GTK_WIMP_SECTION_TITLE		"Wimp-tema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve-tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue-tema"
!define GAIM_SECTION_DESCRIPTION		"Gaims känrfiler och DLL:er"
!define GTK_SECTION_DESCRIPTION		"En GUI-verktygsuppsättning för flera olika plattformar som Gaim behöver."
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+teman kan ändra känslan av och utseendet på GTK+applikationer."
!define GTK_NO_THEME_DESC			"Installera inte något GTK+tema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) ett GTK-tema som smälter bra in i Windows-miljön."
!define GTK_BLUECURVE_THEME_DESC		"The Bluecurve-tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"The Lighthouseblue-tema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"En äldre version av GTK+ runtime hittades, vill du uppgradera den?$\rOBS! Gaim kommer kanske inte att fungera om du inte uppgraderar."

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (enbart för avinstallation)"
!define GAIM_PROMPT_WIPEOUT			"Din gamla Gaim-katalog kommer att raderas, vill du fortsätta?$\r$\rOBS! om du har installerat några extra insticksmoduler kommer de raderas.$\rGaims användarinställningar kommer inte påverkas."
!define GAIM_PROMPT_DIR_EXISTS		"Den katalog du vill installera i finns redan. Allt i katalogen$\rkommer att raderas, vill du fortsätta?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fel vid installation av GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Den sökväg du angivit går inte att komma åt eller skapa."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har inte rättigheter att installera ett GTK+tema."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Avinstalleraren kunde inte hitta registervärden för Gaim.$\rAntagligen har en annan användare installerat applikationen."
!define un.GAIM_UNINSTALL_ERROR_2         "Du har inte rättigheter att avinstallera den här applikationen."

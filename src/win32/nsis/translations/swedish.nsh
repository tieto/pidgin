;;
;;  swedish.nsh
;;
;;  Swedish language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Tore Lundqvist <tlt@mima.x.se>, 2003.
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_SWEDISH} "$\"GTK+ runtime environment$\" saknas eller behöver uppgraderas$\rInstallera v${GTK_VERSION} eller högre av GTK+."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_SWEDISH} "Gaim-snabbmeddelandeklient (obligatorisk)"
LangString GTK_SECTION_TITLE				${LANG_SWEDISH} "GTK+ Runtime Environment (obligatorisk)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_SWEDISH} "GTK+teman"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_SWEDISH} "Inget tema"
LangString GTK_WIMP_SECTION_TITLE			${LANG_SWEDISH} "Wimp-tema"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_SWEDISH} "Bluecurve-tema"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_SWEDISH} "Light House Blue-tema"
LangString GAIM_SECTION_DESCRIPTION			${LANG_SWEDISH} "Gaims känrfiler och DLL:er"
LangString GTK_SECTION_DESCRIPTION			${LANG_SWEDISH} "En GUI-verktygsuppsättning för flera olika plattformar som Gaim behöver."
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_SWEDISH} "GTK+teman kan ändra känslan av och utseendet på GTK+applikationer."
LangString GTK_NO_THEME_DESC				${LANG_SWEDISH} "Installera inte något GTK+tema"
LangString GTK_WIMP_THEME_DESC			${LANG_SWEDISH} "GTK-Wimp (Windows impersonator) ett GTK-tema som smälter bra in i Windows-miljön."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_SWEDISH} "The Bluecurve-tema."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_SWEDISH} "The Lighthouseblue-tema."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_SWEDISH} "Plats för installation"
LangString GTK_PAGE_SUBTITLE				${LANG_SWEDISH} "Välj vilken katalog GTK+ ska installeras i"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_SWEDISH} "GTK+ kommer att installeras i följande katalog"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_SWEDISH} "För att installera i en annan katalog klicka på Bläddra, klicka på Nästa för att fortsätta."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_SWEDISH} "GTK+ i följande katalog kommer att uppgraderas"
LangString GTK_UPGRADE_PROMPT				${LANG_SWEDISH} "En äldre version av GTK+ runtime hittades, vill du uppgradera den?$\rOBS! Gaim kommer kanske inte att fungera om du inte uppgraderar."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_SWEDISH} "Gaim (enbart för avinstallation)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_SWEDISH} "Din gamla Gaim-katalog kommer att raderas, vill du fortsätta?$\r$\rOBS! om du har installerat några extra insticksmoduler kommer de raderas.$\rGaims användarinställningar kommer inte påverkas."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_SWEDISH} "Den katalog du vill installera i finns redan. Allt i katalogen$\rkommer att raderas, vill du fortsätta?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_SWEDISH} "Fel vid installation av GTK+ runtime."
LangString GTK_BAD_INSTALL_PATH			${LANG_SWEDISH} "Den sökväg du angivit går inte att komma åt eller skapa."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_SWEDISH} "Du har inte rättigheter att installera ett GTK+tema."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_SWEDISH} "Avinstalleraren kunde inte hitta registervärden för Gaim.$\rAntagligen har en annan användare installerat applikationen."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_SWEDISH} "Du har inte rättigheter att avinstallera den här applikationen."

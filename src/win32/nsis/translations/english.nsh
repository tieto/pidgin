;;
;;  english.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_ENGLISH} "The GTK+ runtime environment is either missing or needs to be upgraded.$\rPlease install v${GTK_VERSION} or higher of the GTK+ runtime"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_ENGLISH} "Gaim Instant Messenger (required)"
LangString GTK_SECTION_TITLE				${LANG_ENGLISH} "GTK+ Runtime Environment (required)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_ENGLISH} "GTK+ Themes"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_ENGLISH} "No Theme"
LangString GTK_WIMP_SECTION_TITLE			${LANG_ENGLISH} "Wimp Theme"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_ENGLISH} "Bluecurve Theme"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_ENGLISH} "Light House Blue Theme"
LangString GAIM_SECTION_DESCRIPTION			${LANG_ENGLISH} "Core Gaim files and dlls"
LangString GTK_SECTION_DESCRIPTION			${LANG_ENGLISH} "A multi-platform GUI toolkit, used by Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_ENGLISH} "GTK+ Themes can change the look and feel of GTK+ applications."
LangString GTK_NO_THEME_DESC				${LANG_ENGLISH} "Don't install a GTK+ theme"
LangString GTK_WIMP_THEME_DESC			${LANG_ENGLISH} "GTK-Wimp (Windows impersonator) is a GTK theme that blends well into the Windows desktop environment."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_ENGLISH} "The Bluecurve theme."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_ENGLISH} "The Lighthouseblue theme."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_ENGLISH} "Choose Install Location"
LangString GTK_PAGE_SUBTITLE				${LANG_ENGLISH} "Choose the folder in which to install GTK+"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_ENGLISH} "Setup will install GTK+ in the following folder"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_ENGLISH} "To install in a different folder, click Browse and select another folder. Click Next to continue."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_ENGLISH} "Setup will upgrade GTK+ found in the following folder"
LangString GTK_UPGRADE_PROMPT				${LANG_ENGLISH} "An old version of the GTK+ runtime was found. Do you wish to upgrade?$\rNote: Gaim may not work unless you do."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_ENGLISH} "Gaim (remove only)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_ENGLISH} "Your old Gaim directory is about to be deleted. Would you like to continue?$\r$\rNote: Any non-standard plugins that you may have installed will be deleted.$\rGaim user settings will not be affected."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_ENGLISH} "The installation directory you specified already exists. Any contents$\rwill be deleted. Would you like to continue?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_ENGLISH} "Error installing GTK+ runtime."
LangString GTK_BAD_INSTALL_PATH			${LANG_ENGLISH} "The path you entered can not be accessed or created."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_ENGLISH} "You do not have permission to install a GTK+ theme."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_ENGLISH} "The uninstaller could not find registry entries for Gaim.$\rIt is likely that another user installed this application."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_ENGLISH} "You do not have permission to uninstall this application."

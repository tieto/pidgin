;;
;;  dutch.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Vincent van Adrighem <vincent@dirck.mine.nu>
;;  Version 2
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_DUTCH} "De GTK+ runtime-omgeving is niet aanwezig of moet vernieuwd worden.$\rInstalleer v${GTK_VERSION} of nieuwer van de GTK+ runtime-omgeving"

; License Page
LangString GAIM_LICENSE_BUTTON			${LANG_DUTCH} "Volgende >"
LangString GAIM_LICENSE_BOTTOM_TEXT			${LANG_DUTCH} "$(^Name) wordt uitgegeven onder de GPL licentie. Deze licentie wordt hier slechts ter informatie aangeboden. $_CLICK"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_DUTCH} "Gaim Instant Messaging Client (vereist)"
LangString GTK_SECTION_TITLE				${LANG_DUTCH} "GTK+ runtime-omgeving (vereist)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_DUTCH} "GTK+ thema's"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_DUTCH} "Geen thema"
LangString GTK_WIMP_SECTION_TITLE			${LANG_DUTCH} "Wimp thema"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_DUTCH} "Bluecurve thema"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_DUTCH} "Light House Blue thema"
LangString GAIM_SECTION_DESCRIPTION			${LANG_DUTCH} "Gaim hoofdbestanden en dlls"
LangString GTK_SECTION_DESCRIPTION			${LANG_DUTCH} "Een multi-platform gebruikersinterface, gebruikt door Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_DUTCH} "GTK+ thema's veranderen het uiterlijk en gedrag van GTK+ programma's."
LangString GTK_NO_THEME_DESC				${LANG_DUTCH} "Geen GTK+ thema installeren"
LangString GTK_WIMP_THEME_DESC			${LANG_DUTCH} "GTK-Wimp (Windows impersonator) is een GTK+ thema wat goed past in een windows omgeving."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_DUTCH} "Het Bluecurve thema (standaardthema voor RedHat Linux)."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_DUTCH} "Het Lighthouseblue thema."

; GTK+ Directory Page
LangString GTK_UPGRADE_PROMPT				${LANG_DUTCH} "Er is een oude versie van GTK+ gevonden. Wilt u deze bijwerken?$\rLet op: Gaim werkt misschien niet als u dit niet doet."

; Installer Finish Page
LangString GAIM_FINISH_VISIT_WEB_SITE		${LANG_DUTCH} "Neem een kijkje op de Windows Gaim webpagina"

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_DUTCH} "Gaim (alleen verwijderen)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_DUTCH} "Uw oude Gaim map staat op het punt om verwijderd te worden. Wilt u doorgaan?$\r$\rLet op: Alle door uzelf geïnstalleerde plugins zullen ook verwijderd worden.$\rDe gebruikersinstellingen van Gaim worden niet aangeraakt."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_DUTCH} "De gegeven installatiemap bestaat al. Eventuele inhoud zal verwijderd worden. Wilt u doorgaan?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_DUTCH} "Fout bij installatie van GTK+ runtime omgeving."
LangString GTK_BAD_INSTALL_PATH			${LANG_DUTCH} "Het door u gegeven pad kan niet benaderd worden."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_DUTCH} "U heeft geen toestemming om een GTK+ thema te installeren."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_DUTCH} "De uninstaller kon geen register-ingangen voor Gaim vinden.$\rWaarschijnlijk heeft een andere gebruiker het programma geïnstalleerd."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_DUTCH} "U mag dit programma niet verwijderen."

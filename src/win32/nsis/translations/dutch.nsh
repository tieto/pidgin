;;
;;  dutch.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Vincent van Adrighem <vincent@dirck.mine.nu>
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_DUTCH} "De GTK+ runtime-omgeving is niet aanwezig of moet vernieuwd worden.$\rInstalleer v${GTK_VERSION} of nieuwer van de GTK+ runtime-omgeving"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_DUTCH} "Gaim Instant Messenger (vereist)"
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

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_DUTCH} "Kies inatllatiemap"
LangString GTK_PAGE_SUBTITLE				${LANG_DUTCH} "Kies de map waarin GTK+ geïnstalleerd moet worden"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_DUTCH} "Setup zal GTK+ in de volgende map installeren"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_DUTCH} "Om te installeren in een andere map klikt u op Bladeren en selecteert u een andere map. Klik op Volgende om door te gaan."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_DUTCH} "Setup zal de in deze map gevonden GTK+ bijwerken"
LangString GTK_UPGRADE_PROMPT				${LANG_DUTCH} "Er is een oude versie van GTK+ gevonden. Wilt u deze bijwerken?$\rLet op: Gaim werkt misschien niet als u dit niet doet."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_DUTCH} "Gaim (alleen verwijderen)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_DUTCH} "Uw aoude Gaim map staat op het punt om verwijderd te worden. Wilt u doorgaan?$\r$\rLet op: Alle door uzelf geïnstalleerde plugins zullen ook verwijderd worden.$\rDe gebruikersinstellingen van Gaim worden niet aangeraakt."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_DUTCH} "De gegeven installatiemap bestaat al. Eventuele inhoud zal verwijderd worden. Wilt u doorgaan?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_DUTCH} "Fout bij installatie van GTK+ runtime omgeving."
LangString GTK_BAD_INSTALL_PATH			${LANG_DUTCH} "Het door u gegeven pad kan niet benaderd worden."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_DUTCH} "U heeft geen toestemming om een GTK+ thema te installeren."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_DUTCH} "De uninstaller kon geen register-ingangen voor Gaim vinden.$\rWaarschijnlijk heeft een andere gebruiker het programma geïnstalleerd."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_DUTCH} "U mag dit programma niet verwijderen."

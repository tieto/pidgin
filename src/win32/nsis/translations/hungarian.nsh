;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Sutto Zoltan <suttozoltan@chello.hu
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_HUNGARIAN} "A GTK+ futtató környezet hiányzik vagy újabb verzió szükséges.$\rKérem installálja a v${GTK_VERSION} vagy magasabb verziójú GTK+ futtató környezetet."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_HUNGARIAN} "Gaim IM kliens (szükséges)"
LangString GTK_SECTION_TITLE				${LANG_HUNGARIAN} "GTK+ futtató környezet (szükséges)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_HUNGARIAN} "GTK+ témák"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_HUNGARIAN} "Nincs téma"
LangString GTK_WIMP_SECTION_TITLE			${LANG_HUNGARIAN} "Wimp téma"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_HUNGARIAN} "Bluecurve téma"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_HUNGARIAN} "Light House Blue téma"
LangString GAIM_SECTION_DESCRIPTION			${LANG_HUNGARIAN} "Gaim fájlok és dll-ek"
LangString GTK_SECTION_DESCRIPTION			${LANG_HUNGARIAN} "Gaim által használt több-platformos grafikus környezet"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_HUNGARIAN} "GTK+ témák megváltoztatják a GTK+ alkalmazások kinézetét."
LangString GTK_NO_THEME_DESC				${LANG_HUNGARIAN} "Ne installálja a GTK+ témákat"
LangString GTK_WIMP_THEME_DESC			${LANG_HUNGARIAN} "GTK-Wimp (Windows utánzat) egy Windows környezettel harmonizáló GTK téma."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_HUNGARIAN} "A Bluecurve téma."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_HUNGARIAN} "A Lighthouseblue téma."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_HUNGARIAN} "Telepítés helyének kiválasztása"
LangString GTK_PAGE_SUBTITLE				${LANG_HUNGARIAN} "Válassza ki azt a könyvtárat ahova a GTK+ kívánja telepíteni"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_HUNGARIAN} "A telepítõ a következõ könyvtárba fogja telepíteni a GTK+"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_HUNGARIAN} "Másik könyvtárba telepítéshez kattintson a Tallóz gombra és válasszon egy másik könyvtárat. Kattintson a Tovább gombra."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_HUNGARIAN} "A telepítõ frissíteni fogja a GTK+, melyet a következõ könyvtárban talált"
LangString GTK_UPGRADE_PROMPT				${LANG_HUNGARIAN} "Egy régi verziójú GTK+ futtató környezet van telepítve. Kívánja frissíteni?$\rMegjegyzés: Gaim valószínüleg nem fog mûködni amig nem frissíti."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_HUNGARIAN} "Gaim (csak eltávolítás)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_HUNGARIAN} "Az Ön korábbi Gaim könyvtára törlõdni fog. Folytatni szeretné?$\r$\rMegjegyzés: Minden Ön által telepített plugin törlõdni fog.$\rGaim felhasználói beállításokat ez nem érinti."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_HUNGARIAN} "A telepítéskor megadott könyvtár már létezik. Minden állomány törlõdni fog.$\rFolytatni szeretné?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_HUNGARIAN} "Hiba a GTK+ futtató telepítése közben."
LangString GTK_BAD_INSTALL_PATH			${LANG_HUNGARIAN} "A megadott elérési út nem elérhetõ vagy nem hozható létre."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_HUNGARIAN} "Nincs jogosultsága a GTK+ témák telepítéséhez."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_HUNGARIAN} "Az eltávolító nem talált Gaim registry bejegyzéseket.$\rValószínüleg egy másik felhasználó telepítette az alkalmazást."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_HUNGARIAN} "Nincs jogosultsága az alkalmazás eltávolításához."

;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Sutto Zoltan <suttozoltan@chello.hu
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"A GTK+ futtató környezet hiányzik vagy újabb verzió szükséges.$\rKérem installálja a v${GTK_VERSION} vagy magasabb verziójú GTK+ futtató környezetet."

; Components Page
!define GAIM_SECTION_TITLE			"Gaim IM kliens (szükséges)"
!define GTK_SECTION_TITLE			"GTK+ futtató környezet (szükséges)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ témák"
!define GTK_NOTHEME_SECTION_TITLE		"Nincs téma"
!define GTK_WIMP_SECTION_TITLE		"Wimp téma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve téma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue téma"
!define GAIM_SECTION_DESCRIPTION		"Gaim fájlok és dll-ek"
!define GTK_SECTION_DESCRIPTION		"Gaim által használt több-platformos grafikus környezet"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ témák megváltoztatják a GTK+ alkalmazások kinézetét."
!define GTK_NO_THEME_DESC			"Ne installálja a GTK+ témákat"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows utánzat) egy Windows környezettel harmonizáló GTK+ téma."
!define GTK_BLUECURVE_THEME_DESC		"A Bluecurve téma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"A Lighthouseblue téma."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Egy régi verziójú GTK+ futtató környezet van telepítve. Kívánja frissíteni?$\rMegjegyzés: Gaim valószínüleg nem fog mûködni amig nem frissíti."

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (csak eltávolítás)"
!define GAIM_PROMPT_WIPEOUT			"Az Ön korábbi Gaim könyvtára törlõdni fog. Folytatni szeretné?$\r$\rMegjegyzés: Minden Ön által telepített plugin törlõdni fog.$\rGaim felhasználói beállításokat ez nem érinti."
!define GAIM_PROMPT_DIR_EXISTS		"A telepítéskor megadott könyvtár már létezik. Minden állomány törlõdni fog.$\rFolytatni szeretné?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Hiba a GTK+ futtató telepítése közben."
!define GTK_BAD_INSTALL_PATH			"A megadott elérési út nem elérhetõ vagy nem hozható létre."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nincs jogosultsága a GTK+ témák telepítéséhez."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Az eltávolító nem talált Gaim registry bejegyzéseket.$\rValószínüleg egy másik felhasználó telepítette az alkalmazást."
!define un.GAIM_UNINSTALL_ERROR_2         "Nincs jogosultsága az alkalmazás eltávolításához."

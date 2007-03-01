;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Authors: Sutto Zoltan <suttozoltan@chello.hu>, 2003
;;           Gabor Kelemen <kelemeng@gnome.hu>, 2005
;;

; Startup Checks
!define GTK_INSTALLER_NEEDED			"A GTK+ futtató környezet hiányzik vagy frissítése szükséges.$\rKérem telepítse a v${GTK_MIN_VERSION} vagy magasabb verziójú GTK+ futtató környezetet."
!define INSTALLER_IS_RUNNING			"A telepíto már fut."
!define PIDGIN_IS_RUNNING				"Jelenleg fut a Gaim egy példánya. Lépjen ki a Gaimból és azután próbálja újra."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Tovább >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"A $(^Name) a GNU General Public License (GPL) alatt kerül terjesztésre. Az itt olvasható licenc csak tájékoztatási célt szolgál. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Gaim azonnali üzeno kliens (szükséges)"
!define GTK_SECTION_TITLE			"GTK+ futtató környezet (szükséges)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ témák"
!define GTK_NOTHEME_SECTION_TITLE		"Nincs téma"
!define GTK_WIMP_SECTION_TITLE			"Wimp téma"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve téma"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue téma"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Parancsikonok"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Asztal"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menü"
!define PIDGIN_SECTION_DESCRIPTION		"Gaim fájlok és dll-ek"
!define GTK_SECTION_DESCRIPTION			"A Gaim által használt többplatformos grafikus környezet"
!define GTK_THEMES_SECTION_DESCRIPTION		"A GTK+ témák megváltoztatják a GTK+ alkalmazások kinézetét."
!define GTK_NO_THEME_DESC			"Ne telepítse a GTK+ témákat"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows utánzat) egy Windows környezettel harmonizáló GTK téma."
!define GTK_BLUECURVE_THEME_DESC		"A Bluecurve téma."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"A Lighthouseblue téma."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Parancsikonok a Gaim indításához"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Parancsikon létrehozása a Gaimhoz az asztalon"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Start Menü bejegyzés létrehozása a Gaimhoz"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Egy régi verziójú GTK+ futtatókörnyezet van telepítve. Kívánja frissíteni?$\rMegjegyzés: a Gaim nem fog muködni, ha nem frissíti."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"A Windows Gaim weboldalának felkeresése"

; Gaim Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Gaim (csak eltávolítás)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Hiba a GTK+ futtatókörnyezet telepítése közben."
!define GTK_BAD_INSTALL_PATH			"A megadott elérési út nem érheto el, vagy nem hozható létre."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nincs jogosultsága a GTK+ témák telepítéséhez."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Az eltávolító nem találta a Gaim registry bejegyzéseket.$\rValószínüleg egy másik felhasználó telepítette az alkalmazást."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nincs jogosultsága az alkalmazás eltávolításához."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Helyesírásellenorzés támogatása"
!define PIDGIN_SPELLCHECK_ERROR			"Hiba a helyesírásellenorzés telepítése közben"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Hiba a helyesírásellenorzési szótár telepítése közben"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Helyesírásellenorzés támogatása. (Internetkapcsolat szükséges a telepítéshez)"
!define ASPELL_INSTALL_FAILED			"A telepítés sikertelen"
!define PIDGIN_SPELLCHECK_BRETON			"Breton"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalán"
!define PIDGIN_SPELLCHECK_CZECH			"Cseh"
!define PIDGIN_SPELLCHECK_WELSH			"Walesi"
!define PIDGIN_SPELLCHECK_DANISH			"Dán"
!define PIDGIN_SPELLCHECK_GERMAN			"Német"
!define PIDGIN_SPELLCHECK_GREEK			"Görög"
!define PIDGIN_SPELLCHECK_ENGLISH			"Angol"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Eszperantó"
!define PIDGIN_SPELLCHECK_SPANISH			"Spanyol"
!define PIDGIN_SPELLCHECK_FAROESE			"Faröai"
!define PIDGIN_SPELLCHECK_FRENCH			"Francia"
!define PIDGIN_SPELLCHECK_ITALIAN			"Olasz"
!define PIDGIN_SPELLCHECK_DUTCH			"Holland"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norvég"
!define PIDGIN_SPELLCHECK_POLISH			"Lengyel"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugál"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Román"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Orosz"
!define PIDGIN_SPELLCHECK_SLOVAK			"Szlovák"
!define PIDGIN_SPELLCHECK_SWEDISH			"Svéd"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrán"


;;
;;  hungarian.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Authors: Sutto Zoltan <suttozoltan@chello.hu>, 2003
;;           Gabor Kelemen <kelemeng@gnome.hu>, 2005
;;

; Startup Checks
!define GTK_INSTALLER_NEEDED			"A GTK+ futtató környezet hiányzik vagy frissítése szükséges.$\rKérem telepítse a v${GTK_MIN_VERSION} vagy magasabb verziójú GTK+ futtató környezetet."
!define INSTALLER_IS_RUNNING			"A telepítõ már fut."
!define PIDGIN_IS_RUNNING				"Jelenleg fut a Pidgin egy példánya. Lépjen ki a Pidginból és azután próbálja újra."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Tovább >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"A $(^Name) a GNU General Public License (GPL) alatt kerül terjesztésre. Az itt olvasható licenc csak tájékoztatási célt szolgál. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin azonnali üzenõ kliens (szükséges)"
!define GTK_SECTION_TITLE			"GTK+ futtató környezet (szükséges)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Parancsikonok"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Asztal"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Start Menü"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin fájlok és dll-ek"
!define GTK_SECTION_DESCRIPTION			"A Pidgin által használt többplatformos grafikus eszközkészlet"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Parancsikonok a Pidgin indításához"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Parancsikon létrehozása a Pidginhez az asztalon"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Start Menü bejegyzés létrehozása a Pidginhez"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Egy régi verziójú GTK+ futtatókörnyezet van telepítve. Kívánja frissíteni?$\rMegjegyzés: a Pidgin nem fog mûködni, ha nem frissíti."
!define GTK_WINDOWS_INCOMPATIBLE		"A Windows 95/98/Me nem kompatibillisek a GTK+ 2.8.0 vagy újabb változatokkal. A GTK+ ${GTK_INSTALL_VERSION} nem kerül telepítésre. $\rHa a GTK+ ${GTK_MIN_VERSION} vagy újabb még nincs telepítve, akkor a telepítés most megszakad."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"A Windows Pidgin weboldalának felkeresése"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"A Pidgin jelenleg telepített változata nem távolítható el. Az új verzió a jelenleg telepített verzió eltávolítása nélkül kerül telepítésre. "


; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Hiba a GTK+ futtatókörnyezet telepítése közben."
!define GTK_BAD_INSTALL_PATH			"A megadott elérési út nem érhetõ el, vagy nem hozható létre."

!define URI_HANDLERS_SECTION_TITLE		"URI kezelõk"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Az eltávolító nem találta a Pidgin registry bejegyzéseket.$\rValószínüleg egy másik felhasználó telepítette az alkalmazást."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nincs jogosultsága az alkalmazás eltávolításához."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Helyesírás-ellenõrzés támogatása"
!define PIDGIN_SPELLCHECK_ERROR			"Hiba a helyesírás-ellenõrzés telepítése közben"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Hiba a helyesírás-ellenõrzési szótár telepítése közben"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Helyesírás-ellenõrzés támogatása. (Internetkapcsolat szükséges a telepítéshez)"
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


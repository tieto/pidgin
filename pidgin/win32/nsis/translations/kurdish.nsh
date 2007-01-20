;;
;;  Kurdish.nsh
;;
;;  Kurdish translation if the language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1254
;;
;;  Erdal Ronahi <erdal.ronahi@gmail.com>

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Sazker jixwe dimeþe."
!define GAIM_IS_RUNNING			"Gaim niha jixwe dimeþe. Ji Gaimê derkeve û careke din biceribîne."
!define GTK_INSTALLER_NEEDED			"Derdora runtime ya GTK+ an tune an rojanekirina wê pêwîst e. $\rJi kerema xwe v${GTK_MIN_VERSION} an bilindtir a GTK+ saz bike."

; License Page
!define GAIM_LICENSE_BUTTON			"Pêþ >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) bin lîsansa Lîsansa Gelempera Gistî ya GNU (GPL) hatiye weþandin. Ji bo agahî, ev lîsans li vir tê xwendin. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (pêwîst)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (pêwîst)"
!define GTK_THEMES_SECTION_TITLE		"Dirbên GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Dirb tunebe"
!define GTK_WIMP_SECTION_TITLE		"Dirbê Wimp"
!define GTK_BLUECURVE_SECTION_TITLE	"Dirbê Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Dirbê Light House Blue"
!define GAIM_SHORTCUTS_SECTION_TITLE "Riyên kin"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "Sermasê"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "Menuya destpêkê"
!define GAIM_SECTION_DESCRIPTION		"Dosiyên cevherî ya Gaim û dll"
!define GTK_SECTION_DESCRIPTION		"Paketa amûrên GUI ji bo gelek platforman, ji hêla Gaim tê bikaranîn."
!define GTK_THEMES_SECTION_DESCRIPTION	"Dirbên GTK+ dikarin ruyê bernameyên GTK biguherînin."
!define GTK_NO_THEME_DESC			"Dirbeke GTK+ saz neke"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator) dirbeyke GTK ya wekî derdora sermaseyê ya Windowsê."
!define GTK_BLUECURVE_THEME_DESC		"Dirbê Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Dirbê Lighthouseblue."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "rêya kin a ji bo destpêkirina Gaim"
!define GAIM_DESKTOP_SHORTCUT_DESC   "rêya kin a Gaim di sermasêyê de çêke"
!define GAIM_STARTMENU_SHORTCUT_DESC   "Gaimê binivîse menuya destpêk"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Guhertoyeke kevn a GTK+ hatiye dîtin. Tu dixwazî bilind bikî?$\rNot: Heke tu nekî, dibe ku Gaim naxebite."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Were Malpera Gaim a Windowsê"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (bi tenê rake)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Di sazkirina GTK+ de çewtî derket."
!define GTK_BAD_INSTALL_PATH			"rêya te nivîsand nayê gihiþtin an afirandin."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS	"Destûra sazkirina dirbekî GTK+ tune."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Raker têketiyên registry yên Gaim nedît. $\rQey bikarhênereke din vê bername saz kir."
!define un.GAIM_UNINSTALL_ERROR_2		"Destûra te ji bo rakirina vê bernameyê tune."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Desteka kontrola rastnivîsê"
!define GAIM_SPELLCHECK_ERROR			"Di sazkirina kontrola rastnivîsê de çewtî derket."
!define GAIM_SPELLCHECK_DICT_ERROR		"Di sazkirina ferhenga rastnivîsê de çewtî derket."
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Desteka kontrola rastnivîsê.  (Ji bo sazkirinê înternet pêwîst e)"
!define ASPELL_INSTALL_FAILED			"Sazkirin Serneket"
!define GAIM_SPELLCHECK_BRETON			"Bretonî"
!define GAIM_SPELLCHECK_CATALAN			"Catalan"
!define GAIM_SPELLCHECK_CZECH			"Çekî"
!define GAIM_SPELLCHECK_WELSH			"Welsh"
!define GAIM_SPELLCHECK_DANISH			"Danikî"
!define GAIM_SPELLCHECK_GERMAN			"Almanî"
!define GAIM_SPELLCHECK_GREEK			"Yewnanî"
!define GAIM_SPELLCHECK_ENGLISH			"Îngilîzî"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto"
!define GAIM_SPELLCHECK_SPANISH			"Spanî"
!define GAIM_SPELLCHECK_FAROESE			"Faroese"
!define GAIM_SPELLCHECK_FRENCH			"Fransî"
!define GAIM_SPELLCHECK_ITALIAN			"Îtalî"
!define GAIM_SPELLCHECK_DUTCH			"Dutch"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norwecî"
!define GAIM_SPELLCHECK_POLISH			"Polî"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portekizî"
!define GAIM_SPELLCHECK_ROMANIAN			"Romanî"
!define GAIM_SPELLCHECK_RUSSIAN			"Rusî"
!define GAIM_SPELLCHECK_SLOVAK			"Slovakî"
!define GAIM_SPELLCHECK_SWEDISH			"Swêdî"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukraynî"


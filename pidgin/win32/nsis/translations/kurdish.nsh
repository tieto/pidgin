;;
;;  Kurdish.nsh
;;
;;  Kurdish translation if the language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1254
;;
;;  Erdal Ronahi <erdal.ronahi@gmail.com>

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Sazker jixwe dimeþe."
!define PIDGIN_IS_RUNNING			"Pidgin niha jixwe dimeþe. Ji Pidgin ê derkeve û careke din biceribîne."
!define GTK_INSTALLER_NEEDED			"Derdora runtime ya GTK+ an tune an rojanekirina wê pêwîst e. $\rJi kerema xwe v${GTK_MIN_VERSION} an bilindtir a GTK+ saz bike."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Pêþ >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) bin lîsansa Lîsansa Gelempera Gistî ya GNU (GPL) hatiye weþandin. Ji bo agahî, ev lîsans li vir tê xwendin. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (pêwîst)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (pêwîst)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Riyên kin"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Sermasê"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menuya destpêkê"
!define PIDGIN_SECTION_DESCRIPTION		"Dosiyên cevherî ya Pidgin û dll"
!define GTK_SECTION_DESCRIPTION		"Paketa amûrên GUI ji bo gelek platforman, ji hêla Pidgin tê bikaranîn."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "rêya kin a ji bo destpêkirina Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "rêya kin a Pidgin di sermasêyê de çêke"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Pidgin ê binivîse menuya destpêk"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Guhertoyeke kevn a GTK+ hatiye dîtin. Tu dixwazî bilind bikî?$\rNot: Heke tu nekî, dibe ku $(^Name) naxebite."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Were Malpera Pidgin a Windowsê"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Di sazkirina GTK+ de çewtî derket."
!define GTK_BAD_INSTALL_PATH			"rêya te nivîsand nayê gihiþtin an afirandin."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Raker têketiyên registry yên Pidgin nedît. $\rQey bikarhênereke din vê bername saz kir."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Destûra te ji bo rakirina vê bernameyê tune."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Desteka kontrola rastnivîsê"
!define PIDGIN_SPELLCHECK_ERROR			"Di sazkirina kontrola rastnivîsê de çewtî derket."
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Di sazkirina ferhenga rastnivîsê de çewtî derket."
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Desteka kontrola rastnivîsê.  (Ji bo sazkirinê înternet pêwîst e)"
!define ASPELL_INSTALL_FAILED			"Sazkirin Serneket"
!define PIDGIN_SPELLCHECK_BRETON			"Bretonî"
!define PIDGIN_SPELLCHECK_CATALAN			"Catalan"
!define PIDGIN_SPELLCHECK_CZECH			"Çekî"
!define PIDGIN_SPELLCHECK_WELSH			"Welsh"
!define PIDGIN_SPELLCHECK_DANISH			"Danikî"
!define PIDGIN_SPELLCHECK_GERMAN			"Almanî"
!define PIDGIN_SPELLCHECK_GREEK			"Yewnanî"
!define PIDGIN_SPELLCHECK_ENGLISH			"Îngilîzî"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH			"Spanî"
!define PIDGIN_SPELLCHECK_FAROESE			"Faroese"
!define PIDGIN_SPELLCHECK_FRENCH			"Fransî"
!define PIDGIN_SPELLCHECK_ITALIAN			"Îtalî"
!define PIDGIN_SPELLCHECK_DUTCH			"Dutch"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norwecî"
!define PIDGIN_SPELLCHECK_POLISH			"Polî"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portekizî"
!define PIDGIN_SPELLCHECK_ROMANIAN			"Romanî"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Rusî"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovakî"
!define PIDGIN_SPELLCHECK_SWEDISH			"Swêdî"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukraynî"


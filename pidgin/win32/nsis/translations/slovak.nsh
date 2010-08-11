;;  vim:syn=winbatch:encoding=cp1250:
;;
;;  slovak.nsh
;;
;;  Slovak language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: dominik@internetkosice.sk
;;  Version 2

; Startup Checks
!define INSTALLER_IS_RUNNING			"Inštalácia je už spustená"
!define PIDGIN_IS_RUNNING				"Pidgin je práve spustený. Vypnite ho a skúste znova."
!define GTK_INSTALLER_NEEDED			"GTK+ runtime prostredie chýba alebo musí byť upgradované.$\rNainštalujte, prosím, GTK+ runtime verziu v${GTK_MIN_VERSION}, alebo novšiu"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Ďalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) je vydaný pod GPL licenciou. Táto licencia je len pre informačné účely. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Klient (nevyhnutné)"
!define GTK_SECTION_TITLE			"GTK+ Runtime prostredie (nevyhnutné)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Zástupcovia"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Plocha"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Štart Menu"
!define PIDGIN_SECTION_DESCRIPTION		"Jadro Pidgin-u a nevyhnutné DLL súbory"
!define GTK_SECTION_DESCRIPTION			"Multiplatformové GUI nástroje, používané Pidgin-om"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Zástupcovia pre Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Vytvoriť zástupcu pre Pidgin na pracovnej ploche"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Vytvoriť odkaz na Pidgin v Štart Menu"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Bola nájdená staršia verzia GTK+ runtime. Prajete si upgradovať súčasnú verziu?$\rPoznámka: $(^Name) nemusí po upgradovaní fungovať správne."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Navštíviť webstránku Windows Pidgin"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba pri inštalácii GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadaná cesta nie je prístupná alebo ju nie je možné vytvoriť."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Inštalátoru sa nepodarilo nájsť položky v registri pre Pidgin.$\rJe možné, že túto aplikáciu nainštaloval iný používateľ."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nemáte oprávnenie na odinštaláciu tejto aplikácie."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Podpora kontroly pravopisu"
!define PIDGIN_SPELLCHECK_ERROR			"Chyba pri inštalácii kontroly pravopisu"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Chyba pri inštalácii slovníka kontroly pravopisu"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Podpora kontroly pravopisu (Nutné pripojenie k Internetu)"
!define ASPELL_INSTALL_FAILED			"Inštalácia zlyhala"
!define PIDGIN_SPELLCHECK_BRETON			"Bretónsky"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalánsky"
!define PIDGIN_SPELLCHECK_CZECH			"Český"
!define PIDGIN_SPELLCHECK_WELSH			"Welshský"
!define PIDGIN_SPELLCHECK_DANISH			"Dánsky"
!define PIDGIN_SPELLCHECK_GERMAN			"Nemecký"
!define PIDGIN_SPELLCHECK_GREEK			"Grécky"
!define PIDGIN_SPELLCHECK_ENGLISH			"Anglický"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperantský"
!define PIDGIN_SPELLCHECK_SPANISH			"Španielský"
!define PIDGIN_SPELLCHECK_FAROESE			"Faroeský"
!define PIDGIN_SPELLCHECK_FRENCH			"Francúzsky"
!define PIDGIN_SPELLCHECK_ITALIAN			"Talianský"
!define PIDGIN_SPELLCHECK_DUTCH			"Holandský"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Nórsky"
!define PIDGIN_SPELLCHECK_POLISH			"Poľský"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugalský"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumunský"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Ruský"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovenský"
!define PIDGIN_SPELLCHECK_SWEDISH			"Švédsky"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrajinský"


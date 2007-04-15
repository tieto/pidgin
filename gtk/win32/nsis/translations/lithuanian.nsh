;;
;;  lithuanian.nsh
;;
;;  Lithuanian translation strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 1

; Startup Checks
!define INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define GAIM_IS_RUNNING				"Ðiuo metu Gaim yra paleistas. Uþdarykite ðià programà ir pabandykite ið naujo."
!define GTK_INSTALLER_NEEDED			"GTK+ vykdymo meto aplinkos nëra arba ji turi bûti atnaujinta.$\rÁdiekite v${GTK_MIN_VERSION} arba naujesnæ GTK+ vykdymo meto aplinkos versijà"

; License Page
!define GAIM_LICENSE_BUTTON			"Toliau >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) yra iðleistas GNU bendrosios vieðosios licenzijos (GPL) sàlygomis.  Licenzija èia yra pateikta tik susipaþinimo tikslams. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim pokalbiø klientas (bûtinas)"
!define GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (bûtina)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ apipavidalinimai"
!define GTK_NOTHEME_SECTION_TITLE		"Jokio apipavidalinimo"
!define GTK_WIMP_SECTION_TITLE			"Wimp apipavidalinimas"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve apipavidalinimas"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue apipavidalinimas"
!define GAIM_SHORTCUTS_SECTION_TITLE		"Nuorodos"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"Darbalaukyje"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"Pradiniame meniu"
!define GAIM_SECTION_DESCRIPTION		"Pagrindiniai Gaim failai"
!define GTK_SECTION_DESCRIPTION			"Daugiaplatforminis vartotojo sàsajos priemoniø komplektas, naudojamas Gaim."
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+ apipavidalinimai gali pakeisti GTK+ programø iðvaizdà."
!define GTK_NO_THEME_DESC			"Neádiegti GTK+ apipavidalinimo."
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows imitatorius) yra gerai Windows aplinkoje derantis GTK apipavidalinimas."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve apipavidalinimas."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue apipavidalinimas."
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Gaim paleidimo nuorodos"
!define GAIM_DESKTOP_SHORTCUT_DESC		"Sukurti nuorodà á Gaim darbastalyje."
!define GAIM_STARTMENU_SHORTCUT_DESC		"Sukurti pradinio meniu áraðà, skirtà Gaim."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Rasta sena GTK+ vykdymo meto aplinkos versija. Ar norite jà atnaujinti?$\rPastaba: sklandþiam Gaim darbui atnaujinimas gali bûti reikalingas."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Aplankyti Windows Gaim tinklalapá"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (tik paðalinti)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ vykdymo meto aplinkos diegimo klaida."
!define GTK_BAD_INSTALL_PATH			"Jûsø nurodytas katalogas neprieinamas ar negali bûti sukurtas."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Neturite teisiø ádiegti GTK+ apipavidalinimà."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Ðalinimo programa nerado Gaim registro áraðø.$\rGalbût kitas vartotojas instaliavo ðià programà."
!define un.GAIM_UNINSTALL_ERROR_2		"Neturite teisiø paðalinti ðios programos."

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"Raðybos tikrinimo palaikymas"
!define GAIM_SPELLCHECK_ERROR			"Raðybos tikrinimo diegimo klaida"
!define GAIM_SPELLCHECK_DICT_ERROR		"Raðybos tikrinimo þodyno diegimo klaida"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Raðybos tikrinimo palaikymas.  Ádiegimui bûtina interneto jungtis."
!define ASPELL_INSTALL_FAILED			"Diegimas nepavyko"
!define GAIM_SPELLCHECK_BRETON			"Bretonø kalba"
!define GAIM_SPELLCHECK_CATALAN			"Katalonø kalba"
!define GAIM_SPELLCHECK_CZECH			"Èekø kalba"
!define GAIM_SPELLCHECK_WELSH			"Valø kalba"
!define GAIM_SPELLCHECK_DANISH			"Danø kalba"
!define GAIM_SPELLCHECK_GERMAN			"Vokieèiø kalba"
!define GAIM_SPELLCHECK_GREEK			"Graikø kalba"
!define GAIM_SPELLCHECK_ENGLISH			"Anglø kalba"
!define GAIM_SPELLCHECK_ESPERANTO		"Esperanto kalba"
!define GAIM_SPELLCHECK_SPANISH			"Ispanø kalba"
!define GAIM_SPELLCHECK_FAROESE			"Farerø kalba"
!define GAIM_SPELLCHECK_FRENCH			"Prancûzø kalba"
!define GAIM_SPELLCHECK_ITALIAN			"Italø kalba"
!define GAIM_SPELLCHECK_DUTCH			"Olandø kalba"
!define GAIM_SPELLCHECK_NORWEGIAN		"Norvegø kalba"
!define GAIM_SPELLCHECK_POLISH			"Lenkø kalba"
!define GAIM_SPELLCHECK_PORTUGUESE		"Portugalø kalba"
!define GAIM_SPELLCHECK_ROMANIAN		"Rumunø kalba"
!define GAIM_SPELLCHECK_RUSSIAN			"Rusø kalba"
!define GAIM_SPELLCHECK_SLOVAK			"Slovakø kalba"
!define GAIM_SPELLCHECK_SWEDISH			"Ðvedø kalba"
!define GAIM_SPELLCHECK_UKRAINIAN		"Ukrainieèiø kalba"

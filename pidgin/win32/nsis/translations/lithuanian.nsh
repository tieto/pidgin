;;
;;  lithuanian.nsh
;;
;;  Lithuanian translation strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 1

; Startup Checks
!define INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define PIDGIN_IS_RUNNING				"Ðiuo metu Pidgin yra paleistas. Uþdarykite ðià programà ir pabandykite ið naujo."
!define GTK_INSTALLER_NEEDED			"GTK+ vykdymo meto aplinkos nëra arba ji turi bûti atnaujinta.$\rÁdiekite v${GTK_MIN_VERSION} arba naujesnæ GTK+ vykdymo meto aplinkos versijà"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Toliau >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) yra iðleistas GNU bendrosios vieðosios licenzijos (GPL) sàlygomis.  Licenzija èia yra pateikta tik susipaþinimo tikslams. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin pokalbiø klientas (bûtinas)"
!define GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (bûtina)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ apipavidalinimai"
!define GTK_NOTHEME_SECTION_TITLE		"Jokio apipavidalinimo"
!define GTK_WIMP_SECTION_TITLE			"Wimp apipavidalinimas"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve apipavidalinimas"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue apipavidalinimas"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Darbalaukyje"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Pradiniame meniu"
!define PIDGIN_SECTION_DESCRIPTION		"Pagrindiniai Pidgin failai"
!define GTK_SECTION_DESCRIPTION			"Daugiaplatforminis vartotojo sàsajos priemoniø komplektas, naudojamas Pidgin."
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+ apipavidalinimai gali pakeisti GTK+ programø iðvaizdà."
!define GTK_NO_THEME_DESC			"Neádiegti GTK+ apipavidalinimo."
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows imitatorius) yra gerai Windows aplinkoje derantis GTK apipavidalinimas."
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve apipavidalinimas."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblue apipavidalinimas."
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin paleidimo nuorodos"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Sukurti nuorodà á Pidgin darbastalyje."
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Sukurti pradinio meniu áraðà, skirtà Pidgin."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Rasta sena GTK+ vykdymo meto aplinkos versija. Ar norite jà atnaujinti?$\rPastaba: sklandþiam $(^Name) darbui atnaujinimas gali bûti reikalingas."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Aplankyti Windows Pidgin tinklalapá"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (tik paðalinti)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ vykdymo meto aplinkos diegimo klaida."
!define GTK_BAD_INSTALL_PATH			"Jûsø nurodytas katalogas neprieinamas ar negali bûti sukurtas."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Neturite teisiø ádiegti GTK+ apipavidalinimà."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Ðalinimo programa nerado Pidgin registro áraðø.$\rGalbût kitas vartotojas instaliavo ðià programà."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Neturite teisiø paðalinti ðios programos."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Raðybos tikrinimo palaikymas"
!define PIDGIN_SPELLCHECK_ERROR			"Raðybos tikrinimo diegimo klaida"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Raðybos tikrinimo þodyno diegimo klaida"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Raðybos tikrinimo palaikymas.  Ádiegimui bûtina interneto jungtis."
!define ASPELL_INSTALL_FAILED			"Diegimas nepavyko"
!define PIDGIN_SPELLCHECK_BRETON			"Bretonø kalba"
!define PIDGIN_SPELLCHECK_CATALAN			"Katalonø kalba"
!define PIDGIN_SPELLCHECK_CZECH			"Èekø kalba"
!define PIDGIN_SPELLCHECK_WELSH			"Valø kalba"
!define PIDGIN_SPELLCHECK_DANISH			"Danø kalba"
!define PIDGIN_SPELLCHECK_GERMAN			"Vokieèiø kalba"
!define PIDGIN_SPELLCHECK_GREEK			"Graikø kalba"
!define PIDGIN_SPELLCHECK_ENGLISH			"Anglø kalba"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto kalba"
!define PIDGIN_SPELLCHECK_SPANISH			"Ispanø kalba"
!define PIDGIN_SPELLCHECK_FAROESE			"Farerø kalba"
!define PIDGIN_SPELLCHECK_FRENCH			"Prancûzø kalba"
!define PIDGIN_SPELLCHECK_ITALIAN			"Italø kalba"
!define PIDGIN_SPELLCHECK_DUTCH			"Olandø kalba"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norvegø kalba"
!define PIDGIN_SPELLCHECK_POLISH			"Lenkø kalba"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portugalø kalba"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumunø kalba"
!define PIDGIN_SPELLCHECK_RUSSIAN			"Rusø kalba"
!define PIDGIN_SPELLCHECK_SLOVAK			"Slovakø kalba"
!define PIDGIN_SPELLCHECK_SWEDISH			"Ðvedø kalba"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainieèiø kalba"

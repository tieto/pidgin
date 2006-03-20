;;
;;  lithuanian.nsh
;;
;;  Lithuanian translation strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1257
;;
;;  Version 1
;;  Note: If translating this file, replace "!insertmacro GAIM_MACRO_DEFAULT_STRING"
;;  with "!define".

; Make sure to update the GAIM_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define GAIM_MACRO_DEFAULT_STRING INSTALLER_IS_RUNNING			"Diegimo programa jau paleista."
!define GAIM_MACRO_DEFAULT_STRING GAIM_IS_RUNNING			"Ðiuo metu Gaim yra paleistas. Uþdarykite ðià programà ir pabandykite ið naujo."
!define GAIM_MACRO_DEFAULT_STRING GTK_INSTALLER_NEEDED			"GTK+ vykdymo meto aplinkos nëra arba ji turi bûti atnaujinta.$\rÁdiekite v${GTK_VERSION} arba naujesnæ GTK+ vykdymo meto aplinkos versijà"

; License Page
!define GAIM_MACRO_DEFAULT_STRING GAIM_LICENSE_BUTTON			"Toliau >"
!define GAIM_MACRO_DEFAULT_STRING GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) yra iðleistas GNU bendrosios vieðosios licenzijos (GPL) sàlygomis.  Licenzija èia yra pateikta tik susipaþinimo tikslams. $_CLICK"

; Components Page
!define GAIM_MACRO_DEFAULT_STRING GAIM_SECTION_TITLE			"Gaim pokalbiø klientas (bûtinas)"
!define GAIM_MACRO_DEFAULT_STRING GTK_SECTION_TITLE			"GTK+ vykdymo meto aplinka (bûtina)"
!define GAIM_MACRO_DEFAULT_STRING GTK_THEMES_SECTION_TITLE		"GTK+ apipavidalinimai"
!define GAIM_MACRO_DEFAULT_STRING GTK_NOTHEME_SECTION_TITLE		"Jokio apipavidalinimo"
!define GAIM_MACRO_DEFAULT_STRING GTK_WIMP_SECTION_TITLE		"Wimp apipavidalinimas"
!define GAIM_MACRO_DEFAULT_STRING GTK_BLUECURVE_SECTION_TITLE	"Bluecurve apipavidalinimas"
!define GAIM_MACRO_DEFAULT_STRING GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue apipavidalinimas"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SHORTCUTS_SECTION_TITLE "Nuorodos"
!define GAIM_MACRO_DEFAULT_STRING GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "Darbalaukyje"
!define GAIM_MACRO_DEFAULT_STRING GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "Pradiniame meniu"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SECTION_DESCRIPTION		"Pagrindiniai Gaim failai"
!define GAIM_MACRO_DEFAULT_STRING GTK_SECTION_DESCRIPTION		"Daugiaplatforminis vartotojo sàsajos priemoniø komplektas, naudojamas Gaim."
!define GAIM_MACRO_DEFAULT_STRING GTK_THEMES_SECTION_DESCRIPTION	"GTK+ apipavidalinimai gali pakeisti GTK+ programø iðvaizdà."
!define GAIM_MACRO_DEFAULT_STRING GTK_NO_THEME_DESC			"Neádiegti GTK+ apipavidalinimo."
!define GAIM_MACRO_DEFAULT_STRING GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows imitatorius) yra gerai Windows aplinkoje derantis GTK apipavidalinimas."
!define GAIM_MACRO_DEFAULT_STRING GTK_BLUECURVE_THEME_DESC		"Bluecurve apipavidalinimas."
!define GAIM_MACRO_DEFAULT_STRING GTK_LIGHTHOUSEBLUE_THEME_DESC	"Lighthouseblue apipavidalinimas."
!define GAIM_MACRO_DEFAULT_STRING GAIM_SHORTCUTS_SECTION_DESCRIPTION   "Gaim paleidimo nuorodos"
!define GAIM_MACRO_DEFAULT_STRING GAIM_DESKTOP_SHORTCUT_DESC   "Sukurti nuorodà á Gaim darbastalyje."
!define GAIM_MACRO_DEFAULT_STRING GAIM_STARTMENU_SHORTCUT_DESC   "Sukurti pradinio meniu áraðà, skirtà Gaim."

; GTK+ Directory Page
!define GAIM_MACRO_DEFAULT_STRING GTK_UPGRADE_PROMPT			"Rasta sena GTK+ vykdymo meto aplinkos versija. Ar norite jà atnaujinti?$\rPastaba: sklandþiam Gaim darbui atnaujinimas gali bûti reikalingas."

; Installer Finish Page
!define GAIM_MACRO_DEFAULT_STRING GAIM_FINISH_VISIT_WEB_SITE		"Aplankyti Windows Gaim tinklalapá"

; Gaim Section Prompts and Texts
!define GAIM_MACRO_DEFAULT_STRING GAIM_UNINSTALL_DESC			"Gaim (tik paðalinti)"
!define GAIM_MACRO_DEFAULT_STRING GAIM_PROMPT_WIPEOUT			"Jûsø senasis Gaim katalogas tuoj turëtø bûti iðtrintas. Ar norite tæsti?$\r$\rPastaba: bet kokie nestandartiniai papildiniai, jeigu tokiø ádiegëte, bus paðalinti.$\rGaim vartotojo nustatymai nebus paliesti."
!define GAIM_MACRO_DEFAULT_STRING GAIM_PROMPT_DIR_EXISTS		"Nurodytas diegimo katalogas jau yra. Jo turinys$\rbus paðalintas. Ar norite tæsti?"

; GTK+ Section Prompts
!define GAIM_MACRO_DEFAULT_STRING GTK_INSTALL_ERROR			"GTK+ vykdymo meto aplinkos diegimo klaida."
!define GAIM_MACRO_DEFAULT_STRING GTK_BAD_INSTALL_PATH			"Jûsø nurodytas katalogas neprieinamas ar negali bûti sukurtas."

; GTK+ Themes section
!define GAIM_MACRO_DEFAULT_STRING GTK_NO_THEME_INSTALL_RIGHTS	"Neturite teisiø ádiegti GTK+ apipavidalinimà."

; Uninstall Section Prompts
!define GAIM_MACRO_DEFAULT_STRING un.GAIM_UNINSTALL_ERROR_1		"Ðalinimo programa nerado Gaim registro áraðø.$\rGalbût kitas vartotojas instaliavo ðià programà."
!define GAIM_MACRO_DEFAULT_STRING un.GAIM_UNINSTALL_ERROR_2		"Neturite teisiø paðalinti ðios programos."

; Spellcheck Section Prompts
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SECTION_TITLE		"Raðybos tikrinimo palaikymas"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ERROR			"Raðybos tikrinimo diegimo klaida"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DICT_ERROR		"Raðybos tikrinimo þodyno diegimo klaida"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SECTION_DESCRIPTION	"Raðybos tikrinimo palaikymas.  Ádiegimui bûtina interneto jungtis."
!define GAIM_MACRO_DEFAULT_STRING ASPELL_INSTALL_FAILED			"Diegimas nepavyko"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_BRETON			"Bretonø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_CATALAN			"Katalonø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_CZECH			"Èekø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_WELSH			"Valø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DANISH			"Danø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_GERMAN			"Vokieèiø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_GREEK			"Graikø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ENGLISH			"Anglø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ESPERANTO		"Esperanto kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SPANISH			"Ispanø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_FAROESE			"Farerø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_FRENCH			"Prancûzø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ITALIAN			"Italø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_DUTCH			"Olandø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_NORWEGIAN		"Norvegø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_POLISH			"Lenkø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_PORTUGUESE		"Portugalø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_ROMANIAN			"Rumunø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_RUSSIAN			"Rusø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SLOVAK			"Slovakø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_SWEDISH			"Ðvedø kalba"
!define GAIM_MACRO_DEFAULT_STRING GAIM_SPELLCHECK_UKRAINIAN		"Ukrainieèiø kalba"

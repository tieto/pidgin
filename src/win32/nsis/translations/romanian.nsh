;;
;;  romanian.nsh
;;
;;  Romanian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Miºu Moldovan <dumol@go.ro>
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_ROMANIAN} "Mediul GTK+ nu e prezent sau aveþi o versiune prea veche.$\rInstalaþi cel puþin versiunea v${GTK_VERSION} a mediului GTK+"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_ROMANIAN} "Client de mesagerie instantanee (obligatoriu)"
LangString GTK_SECTION_TITLE				${LANG_ROMANIAN} "Mediu GTK+ (obligatoriu)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_ROMANIAN} "Teme GTK+"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_ROMANIAN} "Fãrã teme"
LangString GTK_WIMP_SECTION_TITLE			${LANG_ROMANIAN} "Temã Wimp"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_ROMANIAN} "Temã Bluecurve"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_ROMANIAN} "Temã Light House Blue"
LangString GAIM_SECTION_DESCRIPTION			${LANG_ROMANIAN} "Fiºiere Gaim ºi dll-uri"
LangString GTK_SECTION_DESCRIPTION			${LANG_ROMANIAN} "Un mediu de dezvoltare multiplatformã utilizat de Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_ROMANIAN} "Temele GTK+ schimbã aparenþa aplicaþiilor GTK+."
LangString GTK_NO_THEME_DESC				${LANG_ROMANIAN} "Nu instala o temã GTK+"
LangString GTK_WIMP_THEME_DESC			${LANG_ROMANIAN} "GTK-Wimp este o temã GTK în acord cu mediul Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_ROMANIAN} "Tema Bluecurve."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_ROMANIAN} "Tema Lighthouseblue."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_ROMANIAN} "Alegeþi locaþia instalãrii"
LangString GTK_PAGE_SUBTITLE				${LANG_ROMANIAN} "Alegeþi directorul în care doriþi sã instalaþi GTK+"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_ROMANIAN} "Instalarea va copia GTK+ în acest director"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_ROMANIAN} "Pentru a instala într-un alt director, apãsaþi Browse ºi alegeþi un alt director. Apãsaþi Next pentru a continua"
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_ROMANIAN} "Instalarea va actualiza mediul GTK+ prezent în directorul"
LangString GTK_UPGRADE_PROMPT				${LANG_ROMANIAN} "Aveþi o versiune veche a mediului GTK+. Doriþi sã o actualizaþi?$\rNotã: E posibil ca Gaim sã nu funcþioneze cu versiunea veche."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_ROMANIAN} "Gaim (doar dezinstalare)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_ROMANIAN} "Vechiul director Gaim va fi ºters. Doriþi sã continuaþi?$\r$\rNotã: Orice module externe vor fi ºterse.$\rSetãrile utilizatorilor Gaim nu vor fi afectate."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_ROMANIAN} "Directorul ales pentru instalare existã deja.$\rConþinutul sãu va fi ºters. Doriþi sã continuaþi?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_ROMANIAN} "Eroare la instalarea mediului GTK+."
LangString GTK_BAD_INSTALL_PATH			${LANG_ROMANIAN} "Directorul specificat nu poate fi accesat sau creat."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_ROMANIAN} "Nu aveþi drepturile de acces necesare instalãrii unei teme GTK+."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_ROMANIAN} "Programul de dezinstalare nu a gãsit intrãri Gaim în regiºtri.$\rProbabil un alt utilizator a instalat aceastã aplicaþie."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_ROMANIAN} "Nu aveþi drepturile de acces necesare dezinstalãrii acestei aplicaþii."

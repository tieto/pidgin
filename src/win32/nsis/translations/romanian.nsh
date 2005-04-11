;;
;;  romanian.nsh
;;
;;  Romanian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Miºu Moldovan <dumol@go.ro>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Mediul GTK+ nu e prezent sau aveþi o versiune prea veche.$\rInstalaþi cel puþin versiunea v${GTK_VERSION} a mediului GTK+"

; Components Page
!define GAIM_SECTION_TITLE			"Client de mesagerie instantanee (obligatoriu)"
!define GTK_SECTION_TITLE			"Mediu GTK+ (obligatoriu)"
!define GTK_THEMES_SECTION_TITLE		"Teme GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Fãrã teme"
!define GTK_WIMP_SECTION_TITLE		"Temã Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Temã Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Temã Light House Blue"
!define GAIM_SECTION_DESCRIPTION		"Fiºiere Gaim ºi dll-uri"
!define GTK_SECTION_DESCRIPTION		"Un mediu de dezvoltare multiplatformã utilizat de Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Temele GTK+ schimbã aparenþa aplicaþiilor GTK+."
!define GTK_NO_THEME_DESC			"Nu instala o temã GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp este o temã GTK+ în acord cu mediul Windows."
!define GTK_BLUECURVE_THEME_DESC		"Tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Tema Lighthouseblue."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Aveþi o versiune veche a mediului GTK+. Doriþi sã o actualizaþi?$\rNotã: E posibil ca Gaim sã nu funcþioneze cu versiunea veche."

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (doar dezinstalare)"
!define GAIM_PROMPT_WIPEOUT			"Vechiul director Gaim va fi ºters. Doriþi sã continuaþi?$\r$\rNotã: Orice module externe vor fi ºterse.$\rSetãrile utilizatorilor Gaim nu vor fi afectate."
!define GAIM_PROMPT_DIR_EXISTS		"Directorul ales pentru instalare existã deja.$\rConþinutul sãu va fi ºters. Doriþi sã continuaþi?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Eroare la instalarea mediului GTK+."
!define GTK_BAD_INSTALL_PATH			"Directorul specificat nu poate fi accesat sau creat."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nu aveþi drepturile de acces necesare instalãrii unei teme GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "Programul de dezinstalare nu a gãsit intrãri Gaim în regiºtri.$\rProbabil un alt utilizator a instalat aceastã aplicaþie."
!define un.GAIM_UNINSTALL_ERROR_2         "Nu aveþi drepturile de acces necesare dezinstalãrii acestei aplicaþii."

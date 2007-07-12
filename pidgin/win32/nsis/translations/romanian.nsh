;;
;;  romanian.nsh
;;
;;  Romanian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Miºu Moldovan <dumol@gnome.ro>, (c) 2004 - 2005.
;;

; Startup Checks
!define INSTALLER_IS_RUNNING                     "Instalarea este deja pornitã."
!define PIDGIN_IS_RUNNING                  "O instanþã a programului Pidgin este deja pornitã. Închideþi-o ºi încercaþi din nou."
!define GTK_INSTALLER_NEEDED			"Mediul GTK+ nu e prezent sau aveþi o versiune prea veche.$\rInstalaþi cel puþin versiunea v${GTK_MIN_VERSION} a mediului GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON                      "Înainte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT         "$(^Name) are licenþã GPL (GNU Public License). Licenþa este inclusã aici doar pentru scopuri informative. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Client de mesagerie instant (obligatoriu)"
!define GTK_SECTION_TITLE			"Mediu GTK+ (obligatoriu)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Scurtãturi"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Meniu Start"
!define PIDGIN_SECTION_DESCRIPTION		"Fiºiere Pidgin ºi dll-uri"
!define GTK_SECTION_DESCRIPTION		"Un mediu de dezvoltare multiplatformã utilizat de Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Scurtãturi pentru pornirea Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Creeazã iconiþe Pidgin pe Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Creeazã o intrare Pidgin în meniul Start"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Aveþi o versiune veche a mediului GTK+. Doriþi sã o actualizaþi?$\rNotã: E posibil ca $(^Name) sã nu funcþioneze cu versiunea veche."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE               "Vizitaþi pagina de web Windows Pidgin"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Eroare la instalarea mediului GTK+."
!define GTK_BAD_INSTALL_PATH			"Directorul specificat nu poate fi accesat sau creat."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Programul de dezinstalare nu a gãsit intrãri Pidgin în regiºtri.$\rProbabil un alt utilizator a instalat aceastã aplicaþie."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Nu aveþi drepturile de acces necesare dezinstalãrii acestei aplicaþii."

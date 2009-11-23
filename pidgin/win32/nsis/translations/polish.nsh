;;
;;  polish.nsh
;;
;;  Polish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Jan Eldenmalm <jan.eldenmalm@amazingports.com>
;;  Version 2
;;


; License Page
!define PIDGIN_LICENSE_BUTTON			"Dalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) jest wydzielone w licencji GPL. Udziela siê licencji wy³¹cznie do celów informacyjnych. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Wymagany jest Pidgin Instant Messaging Client"
!define GTK_SECTION_TITLE			"Wymagany jest runtime œrodowiska GTK+"
!define PIDGIN_SECTION_DESCRIPTION		"Zbiory Core Pidgin oraz dll"
!define GTK_SECTION_DESCRIPTION		"Wieloplatformowe narzêdzie GUI, u¿ywane w Pidgin"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"WejdŸ na stronê Pidgin Web Page"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Deinstalator nie mo¿e znaleŸæ rejestrów dla Pidgin.$\r Wskazuje to na to, ¿e instalacjê przeprowadzi³ inny u¿ytkownik."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nie masz uprawnieñ do deinstalacji tej aplikacji."

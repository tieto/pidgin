;;
;;  polish.nsh
;;
;;  Polish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.


; License Page
!define PIDGIN_LICENSE_BUTTON			"Dalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"Program $(^Name) jest rozpowszechniany na warunkach Powszechnej Licencji Publicznej GNU (GPL). Licencja jest tu podana wy³¹cznie w celach informacyjnych. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Klient komunikatora Pidgin (wymagany)"
!define GTK_SECTION_TITLE			"Biblioteka GTK+ (wymagana, jeœli nie jest obecna)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Skróty"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Pulpit"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Start"
!define PIDGIN_SECTION_DESCRIPTION		"G³ówne pliki programu Pidgin i biblioteki DLL"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Skróty do uruchamiania programu Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Utworzenie skrótu do programu Pidgin na pulpicie"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Utworzenie wpisu w menu Start dla programu Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Odnaleziono star¹ wersjê biblioteki GTK+. Zaktualizowaæ j¹?$\rUwaga: program $(^Name) mo¿e bez tego nie dzia³aæ."
!define GTK_WINDOWS_INCOMPATIBLE		"Systemy Windows 95/98/Me s¹ niezgodne z bibliotek¹ GTK+ 2.8.0 lub nowsz¹. Biblioteka GTK+ ${GTK_INSTALL_VERSION} nie zostanie zainstalowana.$\rJeœli brak zainstalowanej biblioteki GTK+ ${GTK_MIN_VERSION} lub nowszej, instalacja zostanie przerwana."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"OdwiedŸ stronê WWW programu Pidgin"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"B³¹d podczas instalowania biblioteki GTK+."
!define GTK_BAD_INSTALL_PATH			"Nie mo¿na uzyskaæ dostêpu do podanej œcie¿ki lub jej utworzyæ."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Instalator nie mo¿e odnaleŸæ wpisów w rejestrze dla programu Pidgin.$\rMo¿liwe, ¿e inny u¿ytkownik zainstalowa³ ten program."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Brak uprawnieñ do odinstalowania tego programu."
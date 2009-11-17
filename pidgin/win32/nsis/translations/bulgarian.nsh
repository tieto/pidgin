;;
;;  bulgarian.nsh
;;
;;  Bulgarian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Hristo Todorov <igel@bofh.bg>
;;


; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Клиент за Бързи Съобщения (изисква се)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Среда (required)"
!define PIDGIN_SECTION_DESCRIPTION		"Файлове на ядрото на Pidgin и библиотеки"
!define GTK_SECTION_DESCRIPTION			"Мултиплатфорен кит за графичен изглед, използван от Pidgin"


; GTK+ Directory Page

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Деисталатоа не може да намери записи в регистъра за Pidgin.$\rВероятно е бил инсталиран от друг потребител."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Нямате права да деинсталирате тази програма."

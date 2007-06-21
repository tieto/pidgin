;;
;;  bulgarian.nsh
;;
;;  Bulgarian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Hristo Todorov <igel@bofh.bg>
;;


; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime липсва или трябва да бъде обновена.$\rМоля инсталирайте версия v${GTK_MIN_VERSION} или по-нова"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Клиент за Бързи Съобщения (изисква се)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Среда (required)"
!define PIDGIN_SECTION_DESCRIPTION		"Файлове на ядрото на Pidgin и библиотеки"
!define GTK_SECTION_DESCRIPTION			"Мултиплатфорен кит за графичен изглед, използван от Pidgin"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Стара версия GTK+ runtime е открита. Искате ли да обновите?$\rNote: Pidgin може да не сработи ако не го направите."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Грешка при инсталиране на GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Въведеният път не може да бъде достъпен или създаден."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Деисталатоа не може да намери записи в регистъра за Pidgin.$\rВероятно е бил инсталиран от друг потребител."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Нямате права да деинсталирате тази програма."

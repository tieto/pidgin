;;
;;  russian.nsh
;;
;;  Russian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Tasselhof <anr@nm.ru>
;;  Version 2
;;

; License Page
!define PIDGIN_LICENSE_BUTTON			"Следующее >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) выпущено под лицензией GPL. Лицензия приведена здесь для ознакомительных целей. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - Клиент для мгновенного обмена сообщениями по различным протоколам (необходимо)."
!define GTK_SECTION_TITLE			"GTK+ окружение для запуска (необходимо)."
!define PIDGIN_SECTION_DESCRIPTION		"Основная часть Pidgin и библиотеки."
!define GTK_SECTION_DESCRIPTION		"Мультиплатформенный графический инструментарий, используемый Pidgin."

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Посетите веб-страницу Pidgin для пользователей Windows."

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Программа удаления не может найти данные Pidgin в регистре..$\rВероятно это приложение установил другой пользователь."
!define un.PIDGIN_UNINSTALL_ERROR_2		"У Вас нет прав на удаление этого приложения."

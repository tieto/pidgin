;;
;;  russian.nsh
;;
;;  Russian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Tasselhof <anr@nm.ru>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Окружение для запуска GTK+ недоступно или нуждается в обновлении.$\rПожалуйста установите v${GTK_MIN_VERSION} или более старшую версию GTK+."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Следующее >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) выпущено под лицензией GPL. Лицензия приведена здесь для ознакомительных целей. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - Клиент для мгновенного обмена сообщениями по различным протоколам (необходимо)."
!define GTK_SECTION_TITLE			"GTK+ окружение для запуска (необходимо)."
!define GTK_THEMES_SECTION_TITLE		"Темы для GTK+."
!define GTK_NOTHEME_SECTION_TITLE		"Без темы."
!define GTK_WIMP_SECTION_TITLE		"Тема 'Wimp Theme'"
!define GTK_BLUECURVE_SECTION_TITLE		"Тема 'Bluecurve'."
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Тема 'Light House Blue'."
!define PIDGIN_SECTION_DESCRIPTION		"Основная часть Pidgin и библиотеки."
!define GTK_SECTION_DESCRIPTION		"Мультиплатформенный графический инструментарий, используемый Pidgin."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Найдена старая версия GTK+. Вы не хотели бы обновить её ?$\rПримечание: Pidgin может не работать если Вы не сделаете этого."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Посетите веб-страницу Pidgin для пользователей Windows."

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"Pidgin (только удаление)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Ошибка при установке окружения GTK+."
!define GTK_BAD_INSTALL_PATH			"Путь который Вы ввели недоступен или не существует."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"У Вас нет прав на установление темы для GTK+."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Программа удаления не может найти данные Pidgin в регистре..$\rВероятно это приложение установил другой пользователь."
!define un.PIDGIN_UNINSTALL_ERROR_2		"У Вас нет прав на удаление этого приложения."

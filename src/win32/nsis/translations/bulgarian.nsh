;;
;;  bulgarian.nsh
;;
;;  Bulgarian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1251
;;
;;  Author: Hristo Todorov <igel@bofh.bg>
;;


; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_BULGARIAN} "GTK+ runtime липсва или трябва да бъде обновена.$\rМоля инсталирайте версия v${GTK_VERSION} или по-нова"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_BULGARIAN} "Gaim Клиент за Бързи Съобщения (изисква се)"
LangString GTK_SECTION_TITLE				${LANG_BULGARIAN} "GTK+ Runtime Среда (required)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_BULGARIAN} "GTK+ Теми"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_BULGARIAN} "Без Тема"
LangString GTK_WIMP_SECTION_TITLE			${LANG_BULGARIAN} "Wimp Тема"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_BULGARIAN} "Bluecurve Тема"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_BULGARIAN} "Light House Blue Тема"
LangString GAIM_SECTION_DESCRIPTION			${LANG_BULGARIAN} "Файлове на ядрото на Gaim и библиотеки"
LangString GTK_SECTION_DESCRIPTION			${LANG_BULGARIAN} "Мултиплатфорен кит за графичен изглед, използван от Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_BULGARIAN} "GTK+ темите могат да променят Изгледа на GTK+ приложения."
LangString GTK_NO_THEME_DESC				${LANG_BULGARIAN} "Не инсталирайте GTK+ тема"
LangString GTK_WIMP_THEME_DESC			${LANG_BULGARIAN} "GTK-Wimp (Windows impersonator) е GTK тема която се смесва добре със Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_BULGARIAN} "Bluecurve темата."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_BULGARIAN} "Lighthouseblue темата."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_BULGARIAN} "Изберете място на инсталацията"
LangString GTK_PAGE_SUBTITLE				${LANG_BULGARIAN} "Иберете папка за инсталиране на GTK+"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_BULGARIAN} "Setup ще инсталира GTK+ в следнтата папка"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_BULGARIAN} "За да инсталирате в друга папла, натиснете Browse и изберете друга папла. Натиснете Next за да продължите."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_BULGARIAN} "Setup ще обнови GTK+ в следната папка"
LangString GTK_UPGRADE_PROMPT				${LANG_BULGARIAN} "Стара версия GTK+ runtime е открита. Искате ли да обновите?$\rNote: Gaim може да не сработи ако не го направите."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_BULGARIAN} "Gaim (само премахване)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_BULGARIAN} "Вашата стара Gaim директория ще бъде изтрита. Искате ли да продължите?$\r$\rЗабележка: Всички нестандартни добавки които сте инсталирали ще бъдат изтрити.$\rНастройките на Gaim няма да бъдат повлияни."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_BULGARIAN} "Директорията която избрахте съществува. Всичко което е в нея$\rще бъде изтрито. Желаете ли да продължите?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_BULGARIAN} "Грешка при инсталиране на GTK+ runtime."
LangString GTK_BAD_INSTALL_PATH			${LANG_BULGARIAN} "Въведеният път не може да бъде достъпен или създаден."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_BULGARIAN} "Нямате права за да инсталирате GTK+ тема."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_BULGARIAN} "Деисталатоа не може да намери записи в регистъра за Gaim.$\rВероятно е бил инсталиран от друг потребител."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_BULGARIAN} "Нямате права да деинсталирате тази програма."

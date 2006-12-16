;;  vim:syn=winbatch:encoding=8bit-cp936:fileencoding=8bit-cp936:
;;  simp-chinese.nsh
;;
;;  Simplified Chinese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 936
;;
;;  Author: Funda Wang" <fundawang@linux.net.cn>
;;  Version 2
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"安装程序已经运行。"
!define GAIM_IS_RUNNING			"Gaim 的实例正在运行中。请退出 Gaim 然后再试一次。"
!define GTK_INSTALLER_NEEDED			"可能缺少 GTK+ 运行时刻环境，或者需要更新该环境。$\r请安装 v${GTK_VERSION} 或更高版本的 GTK+ 运行时刻环境"

; License Page
!define GAIM_LICENSE_BUTTON			"下一步 >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) 以 GPL 许可发布。在此提供此许可仅为参考。$_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim 即时通讯程序(必需)"
!define GTK_SECTION_TITLE			"GTK+ 运行时刻环境(必需)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ 主题"
!define GTK_NOTHEME_SECTION_TITLE		"无主题"
!define GTK_WIMP_SECTION_TITLE		"Wimp 主题"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve 主题"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue 主题"
!define GAIM_SHORTCUTS_SECTION_TITLE "快捷方式"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE "桌面"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE "开始菜单"
!define GAIM_SECTION_DESCRIPTION		"Gaim 核心文件和 DLLs"
!define GTK_SECTION_DESCRIPTION		"Gaim 所用的多平台 GUI 工具包"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ 主题可以更改 GTK+ 程序的观感。"
!define GTK_NO_THEME_DESC			"不安装 GTK+ 主题"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp 是适合 Windows 桌面环境的 GTK+ 主题。"
!define GTK_BLUECURVE_THEME_DESC		"Bluecurve 主题。"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Lighthouseblue 主题。"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION   "启动 Gaim 的快捷方式"
!define GAIM_DESKTOP_SHORTCUT_DESC   "在桌面上创建 Gaim 的快捷方式"
!define GAIM_STARTMENU_SHORTCUT_DESC   "在开始菜单中创建 Gaim 的快捷方式"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"发现了旧版本的 GTK+ 运行时刻。您想要升级吗?$\r注意: 除非您进行升级，否则 Gaim 可能无法工作。"

; Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"浏览 Windows Gaim 网页"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (只能删除)"
!define GAIM_PROMPT_WIPEOUT			"即将删除您的旧 Gaim 目录。您想要继续吗?$\r$\r注意: 您所安装的任何非标准的插件都将被删除。$\r但是不会影响 Gaim 用户设置。"
!define GAIM_PROMPT_DIR_EXISTS		"您指定的安装目录已经存在。$\r所有内容都将被删除。您想要继续吗?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"安装 GTK+ 运行时刻失败。"
!define GTK_BAD_INSTALL_PATH			"无法访问或创建您输入的路径。"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"您没有权限安装 GTK+ 主题。"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "卸载程序找不到 Gaim 的注册表项目。$\r可能是另外的用户安装了此程序。"
!define un.GAIM_UNINSTALL_ERROR_2         "您没有权限卸载此程序。"

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"拼写检查支持"
!define GAIM_SPELLCHECK_ERROR			"安装拼写检查出错"
!define GAIM_SPELLCHECK_DICT_ERROR		"安装拼写检查字典出错"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"拼写检查支持。(安装需要连接到 Internet)"
!define ASPELL_INSTALL_FAILED			"安装失败"
!define GAIM_SPELLCHECK_BRETON			"布里多尼语"
!define GAIM_SPELLCHECK_CATALAN			"加泰罗尼亚语"
!define GAIM_SPELLCHECK_CZECH			"捷克语"
!define GAIM_SPELLCHECK_WELSH			"威尔士语"
!define GAIM_SPELLCHECK_DANISH			"丹麦语"
!define GAIM_SPELLCHECK_GERMAN			"德语"
!define GAIM_SPELLCHECK_GREEK			"希腊语"
!define GAIM_SPELLCHECK_ENGLISH			"英语"
!define GAIM_SPELLCHECK_ESPERANTO		"世界语"
!define GAIM_SPELLCHECK_SPANISH			"西班牙语"
!define GAIM_SPELLCHECK_FAROESE			"法罗语"
!define GAIM_SPELLCHECK_FRENCH			"法语"
!define GAIM_SPELLCHECK_ITALIAN			"意大利语"
!define GAIM_SPELLCHECK_DUTCH			"荷兰语"
!define GAIM_SPELLCHECK_NORWEGIAN		"挪威语"
!define GAIM_SPELLCHECK_POLISH			"波兰语"
!define GAIM_SPELLCHECK_PORTUGUESE		"葡萄牙语"
!define GAIM_SPELLCHECK_ROMANIAN			"罗马尼亚语"
!define GAIM_SPELLCHECK_RUSSIAN			"俄语"
!define GAIM_SPELLCHECK_SLOVAK			"斯洛伐克语"
!define GAIM_SPELLCHECK_SWEDISH			"瑞典语"
!define GAIM_SPELLCHECK_UKRAINIAN		"乌克兰语"

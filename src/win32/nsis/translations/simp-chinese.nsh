;;
;;  simp-chinese.nsh
;;
;;  Simplified Chinese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 936
;;
;;  Author: Funda Wang" <fundawang@linux.net.cn>
;;  Version 2
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_SIMPCHINESE} "可能缺少 GTK+ 运行时刻环境，或者需要更新该环境。$\r请安装 v${GTK_VERSION} 或更高版本的 GTK+ 运行时刻环境"

; License Page
LangString GAIM_LICENSE_BUTTON			${LANG_SIMPCHINESE} "下一步 >"
LangString GAIM_LICENSE_BOTTOM_TEXT			${LANG_SIMPCHINESE} "$(^Name) 以 GPL 许可发布。在此提供此许可仅为参考。$_CLICK"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_SIMPCHINESE} "Gaim 即时通讯程序(必需)"
LangString GTK_SECTION_TITLE				${LANG_SIMPCHINESE} "GTK+ 运行时刻环境(必需)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_SIMPCHINESE} "GTK+ 主题"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_SIMPCHINESE} "无主题"
LangString GTK_WIMP_SECTION_TITLE			${LANG_SIMPCHINESE} "Wimp 主题"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_SIMPCHINESE} "Bluecurve 主题"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_SIMPCHINESE} "Light House Blue 主题"
LangString GAIM_SECTION_DESCRIPTION			${LANG_SIMPCHINESE} "Gaim 核心文件和 DLLs"
LangString GTK_SECTION_DESCRIPTION			${LANG_SIMPCHINESE} "Gaim 所用的多平台 GUI 工具包"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_SIMPCHINESE} "GTK+ 主题可以更改 GTK+ 程序的观感。"
LangString GTK_NO_THEME_DESC				${LANG_SIMPCHINESE} "不安装 GTK+ 主题"
LangString GTK_WIMP_THEME_DESC			${LANG_SIMPCHINESE} "GTK-Wimp (Windows impersonator)是 is a GTK theme that blends well into the Windows desktop environment."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_SIMPCHINESE} "Bluecurve 主题。"
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_SIMPCHINESE} "Lighthouseblue 主题。"

; GTK+ Directory Page
LangString GTK_UPGRADE_PROMPT				${LANG_SIMPCHINESE} "发现了旧版本的 GTK+ 运行时刻。您想要升级吗?$\r注意: 除非您进行升级，否则 Gaim 可能无法工作。"

; Finish Page
LangString GAIM_FINISH_VISIT_WEB_SITE		${LANG_SIMPCHINESE} "浏览 Windows Gaim 网页"

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_SIMPCHINESE} "Gaim (只能删除)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_SIMPCHINESE} "即将删除您的旧 Gaim 目录。您想要继续吗?$\r$\r注意: 您所安装的任何非标准的插件都将被删除。$\r但是不会影响 Gaim 用户设置。"
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_SIMPCHINESE} "您指定的安装目录已经存在。$\r所有内容都将被删除。您想要继续吗?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_SIMPCHINESE} "安装 GTK+ 运行时刻失败。"
LangString GTK_BAD_INSTALL_PATH			${LANG_SIMPCHINESE} "无法访问或创建您输入的路径。"

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_SIMPCHINESE} "您没有权限安装 GTK+ 主题。"

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_SIMPCHINESE} "卸载程序找不到 Gaim 的注册表项目。$\r可能是另外的用户安装了此程序。"
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_SIMPCHINESE} "您没有权限卸载此程序。"

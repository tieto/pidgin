;;  vim:syn=winbatch:encoding=8bit-cp936:fileencoding=8bit-cp936:
;;  simp-chinese.nsh
;;
;;  Simplified Chinese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 936
;;
;;  Author: Funda Wang" <fundawang@linux.net.cn>
;;  Version 2
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"安装程序已经运行。"
!define PIDGIN_IS_RUNNING			"Pidgin 的实例正在运行中。请退出 Pidgin 然后再试一次。"

; License Page
!define PIDGIN_LICENSE_BUTTON			"下一步 >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) 以 GPL 许可发布。在此提供此许可仅为参考。$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin 即时通讯程序(必需)"
!define GTK_SECTION_TITLE			"GTK+ 运行时刻环境(必需)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "快捷方式"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "桌面"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "开始菜单"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin 核心文件和 DLLs"
!define GTK_SECTION_DESCRIPTION		"Pidgin 所用的多平台 GUI 工具包"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "启动 Pidgin 的快捷方式"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "在桌面上创建 Pidgin 的快捷方式"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "在开始菜单中创建 Pidgin 的快捷方式"

; GTK+ Directory Page

; Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"浏览 Windows Pidgin 网页"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "卸载程序找不到 Pidgin 的注册表项目。$\r可能是另外的用户安装了此程序。"
!define un.PIDGIN_UNINSTALL_ERROR_2         "您没有权限卸载此程序。"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"拼写检查支持"
!define PIDGIN_SPELLCHECK_ERROR			"安装拼写检查出错"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"拼写检查支持。(安装需要连接到 Internet)"

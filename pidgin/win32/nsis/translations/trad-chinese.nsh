;;
;;  trad-chinese.nsh
;;
;;  Traditional Chineese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page:950
;;
;;  Author: Paladin R. Liu <paladin@ms1.hinet.net>
;;  Minor updates: Ambrose C. Li <acli@ada.dhs.org>
;;
;;  Last Updated: May 21, 2007
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"安裝程式正在執行中。"
!define PIDGIN_IS_RUNNING				"Pidgin 正在執行中，請先結束這個程式後再行安裝。"

; License Page
!define PIDGIN_LICENSE_BUTTON			"下一步 >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) 採用 GNU General Public License (GPL) 授權發佈。在此列出授權書，僅作為參考之用。$_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin 主程式 (必需)"
!define GTK_SECTION_TITLE			"GTK+ 執行環境 (必需)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"捷徑"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"桌面捷徑"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "開始功能表"
!define PIDGIN_SECTION_DESCRIPTION		"Pidgin 核心檔案及動態函式庫"
!define GTK_SECTION_DESCRIPTION			"Pidgin 所使用的跨平台圖形介面函式庫"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"建立 Pidgin 捷徑"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"在桌面建立捷徑"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"在開始功能表建立捷徑"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"拜訪 Windows Pidgin 網頁"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"無法移除目前已安裝的 Pidgin，新版本將在未經移除舊版本的情況下進行安裝。"

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI 處理程式"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"移除程式無法找到 Pidgin 的安裝資訊。$\r這應該是有其他的使用者重新安裝了這個程式。"
!define un.PIDGIN_UNINSTALL_ERROR_2		"您目前的權限無法移除 Pidgin。"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"拼字檢查功能"
!define PIDGIN_SPELLCHECK_ERROR			"安裝拼字檢查途中發生錯誤"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"拼字檢查支援（安裝須有網際網路連線）。"


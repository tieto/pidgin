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
!define GTK_INSTALLER_NEEDED			"找不到符合的 GTK+ 執行環境或是需要被更新。$\r請安裝 v${GTK_MIN_VERSION} 或以上版本的 GTK+ 執行環境。"

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
!define GTK_UPGRADE_PROMPT			"發現一個舊版的 GTK+ 執行環境。您要將它升級嗎？$\r請注意：如果您不升級， $(^Name) 可能無法正確的被執行。"
!define GTK_WINDOWS_INCOMPATIBLE		"自版本 2.8.0 開始，GTK＋ 與 Windows 95/98/Me 已不再相容，GTK+ ${GTK_INSTALL_VERSION} 因此將不會被安裝。$\r如果系統內未有已經安裝的 GTK+ ${GTK_MIN_VERSION} 或更新的版本，安裝程式將隨即結束。"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"拜訪 Windows Pidgin 網頁"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"無法移除目前已安裝的 Pidgin，新版本將在未經移除舊版本的情況下進行安裝。"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"安裝 GTK+ 執行環境時發生錯誤。"
!define GTK_BAD_INSTALL_PATH			"您所輸入的安裝目錄無法存取或建立。"

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI 處理程式"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"移除程式無法找到 Pidgin 的安裝資訊。$\r這應該是有其他的使用者重新安裝了這個程式。"
!define un.PIDGIN_UNINSTALL_ERROR_2		"您目前的權限無法移除 Pidgin。"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"拼字檢查功能"
!define PIDGIN_SPELLCHECK_ERROR			"安裝拼字檢查途中發生錯誤"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"安裝拼字檢查用的詞典途中發生錯誤"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"拼字檢查支援（安裝須有網際網路連線）。"
!define ASPELL_INSTALL_FAILED			"安裝失敗"
!define PIDGIN_SPELLCHECK_BRETON		"布里多尼文"
!define PIDGIN_SPELLCHECK_CATALAN		"加泰隆文"
!define PIDGIN_SPELLCHECK_CZECH			"捷克文"
!define PIDGIN_SPELLCHECK_WELSH			"威爾斯文"
!define PIDGIN_SPELLCHECK_DANISH		"丹麥文"
!define PIDGIN_SPELLCHECK_GERMAN		"德文"
!define PIDGIN_SPELLCHECK_GREEK			"希臘文"
!define PIDGIN_SPELLCHECK_ENGLISH		"英文"
!define PIDGIN_SPELLCHECK_ESPERANTO		"世界語"
!define PIDGIN_SPELLCHECK_SPANISH		"西班牙文"
!define PIDGIN_SPELLCHECK_FAROESE		"法羅群島文"
!define PIDGIN_SPELLCHECK_FRENCH		"法文"
!define PIDGIN_SPELLCHECK_ITALIAN		"意大利文"
!define PIDGIN_SPELLCHECK_DUTCH			"荷蘭文"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"挪威文"
!define PIDGIN_SPELLCHECK_POLISH		"波蘭文"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"萄文"
!define PIDGIN_SPELLCHECK_ROMANIAN		"羅馬尼亞文"
!define PIDGIN_SPELLCHECK_RUSSIAN		"俄文"
!define PIDGIN_SPELLCHECK_SLOVAK		"斯洛伐克文"
!define PIDGIN_SPELLCHECK_SWEDISH		"瑞典文"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"烏克蘭文"


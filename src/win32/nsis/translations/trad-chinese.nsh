;;
;;  trad-chinese.nsh
;;
;;  Traditional Chineese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page:950 
;;
;;  Author: Paladin R. Liu <paladin@ms1.hinet.net>
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_TRADCHINESE} "找不到符合的 GTK+ 執行環境。$\r請安裝 v${GTK_VERSION} 以上版本的 GTK+ 執行環境。"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_TRADCHINESE} "Gaim 主程式 (必需)"
LangString GTK_SECTION_TITLE				${LANG_TRADCHINESE} "GTK+ 執行環境 (必需)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_TRADCHINESE} "GTK+ 佈景主題"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_TRADCHINESE} "不安裝"
LangString GTK_WIMP_SECTION_TITLE			${LANG_TRADCHINESE} "Wimp Theme"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_TRADCHINESE} "Bluecurve Theme"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_TRADCHINESE} "Light House Blue Theme"
LangString GAIM_SECTION_DESCRIPTION			${LANG_TRADCHINESE} "Gaim 核心檔案及動態函式庫"
LangString GTK_SECTION_DESCRIPTION			${LANG_TRADCHINESE} "Gaim 所使用的跨平台圖形介面函式庫"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_TRADCHINESE} "GTK+ 佈景主題可以用來改變 GTK+ 應用程式的外觀。"
LangString GTK_NO_THEME_DESC				${LANG_TRADCHINESE} "不安裝 GTK+ 佈景主題"
LangString GTK_WIMP_THEME_DESC			${LANG_TRADCHINESE} "GTK-Wimp (Windows impersonator) is a GTK theme that blends well into the Windows desktop environment."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_TRADCHINESE} "The Bluecurve theme."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_TRADCHINESE} "The Lighthouseblue theme."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_TRADCHINESE} "選擇安裝目錄"
LangString GTK_PAGE_SUBTITLE				${LANG_TRADCHINESE} "選擇一個目錄以安裝 GTK+ 函式庫"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_TRADCHINESE} "安裝程式將會把 GTK+ 安裝在下列的目錄中"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_TRADCHINESE} "要安裝到其他的目錄，請按 [瀏覽 (...)] 並且選取其他目錄來進行安裝。按 [下一步(N)] 繼續。"
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_TRADCHINESE} "安裝程式將會為下列目錄中的 GTK+ 進行升級"
LangString GTK_UPGRADE_PROMPT				${LANG_TRADCHINESE} "發現一個舊版的 GTK+ 執行環境。您要將它升級嗎？$\r請注意：如果您不升級，Gaim 可能無法正確的被執行。"

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_TRADCHINESE} "Gaim v${GAIM_VERSION} (只供移除)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_TRADCHINESE} "您先前安裝於目錄中的舊版 Gaim 將會被移除。您要繼續嗎？$\r$\r請注意：任何您所安裝的非官方維護模組都將被刪除。$\r而 Gaim 的使用者設定將不會受到影響。"
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_TRADCHINESE} "您所選定的安裝目錄下的所有檔案都將被移除。$\r您要繼續嗎？"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_TRADCHINESE} "安裝 GTK+ 執行環境時發生錯誤。"
LangString GTK_BAD_INSTALL_PATH			${LANG_TRADCHINESE} "您所輸入的安裝目錄無法存取或建立。"

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_TRADCHINESE} "您目前的權限無法安裝 GTK+ 佈景主題。"

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_TRADCHINESE} "移除程式無法找到 Gaim 的安裝資訊。$\r這應該是有其他的使用者重新安裝了這個程式。"
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_TRADCHINESE} "您目前的權限無法移除 Gaim。"

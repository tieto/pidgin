;;  vim:syn=winbatch:encoding=cp932:
;;
;;  japanese.nsh
;;
;;  Japanese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 932
;;
;;  Author: "Takeshi Kurosawa" <t-kuro@abox23.so-net.ne.jp>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"インストーラが既に実行されています"
!define PIDGIN_IS_RUNNING				"Pidgin が実行されています。Pidgin を終了してから再度実行してください"

; License Page
!define PIDGIN_LICENSE_BUTTON			"次へ >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name)はGPLライセンスの元でリリースされています。ライセンスはここに参考のためだけに提供されています。 $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidginインスタントメッセンジャ (必須)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (必須)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"ショートカット"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"デスクトップ"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"スタートアップ"
!define PIDGIN_SECTION_DESCRIPTION		"Pidginの核となるファイルとdll"
!define GTK_SECTION_DESCRIPTION			"Pidginの使っているマルチプラットフォームGUIツールキット"


!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin を実行するためのショートカット"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"デスクトップに Pidgin のショートカットを作成する"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"スタートメニューに Pidgin の項目を作成する"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Windows PidginのWebページを訪れてください。"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"アンインストーラはPidginのレジストリエントリを発見できませんでした。$\rおそらく別のユーザにインストールされたでしょう。"
!define un.PIDGIN_UNINSTALL_ERROR_2		"あなたはこのアプリケーションをアンインストールする権限を持っていません。"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"スペルチェックのサポート"
!define PIDGIN_SPELLCHECK_ERROR			"スペルチェックのインストールに失敗しました"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"スペルチェックのサポート  (インターネット接続がインストールに必要です)"


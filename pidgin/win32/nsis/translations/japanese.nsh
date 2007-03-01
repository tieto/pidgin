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
!define GTK_INSTALLER_NEEDED			"GTK+ランタイム環境が無いかもしくはアップグレードする必要があります。$\rv${GTK_MIN_VERSION}もしくはそれ以上のGTK+ランタイムをインストールしてください。"

; License Page
!define PIDGIN_LICENSE_BUTTON			"次へ >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name)はGPLライセンスの元でリリースされています。ライセンスはここに参考のためだけに提供されています。 $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidginインスタントメッセンジャ (必須)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (必須)"
!define GTK_THEMES_SECTION_TITLE		"GTK+のテーマ"
!define GTK_NOTHEME_SECTION_TITLE		"テーマなし"
!define GTK_WIMP_SECTION_TITLE			"Wimpテーマ"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurveテーマ"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blueテーマ"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"ショートカット"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"デスクトップ"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"スタートアップ"
!define PIDGIN_SECTION_DESCRIPTION		"Pidginの核となるファイルとdll"
!define GTK_SECTION_DESCRIPTION			"Pidginの使っているマルチプラットフォームGUIツールキット"
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+のテーマは、GTK+のアプリケーションのルック＆フィールを変えられます。"
!define GTK_NO_THEME_DESC			"GTK+のテーマをインストールしない"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator)はWindowsのデスクトップ環境とよく調和したテーマです。"
!define GTK_BLUECURVE_THEME_DESC		"Bluecurveテーマ。"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblueテーマ。"
!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Pidgin を実行するためのショートカット"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"デスクトップに Pidgin のショートカットを作成する"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"スタートメニューに Pidgin の項目を作成する"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"古いバージョンのGTK+ランタイムが見つかりました。アップグレードしますか?$\r注意: $(^Name)はアップグレードしない限り動かないでしょう。"

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Windows PidginのWebページを訪れてください。"

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (削除のみ)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ランタイムのインストールでエラーが発生しました。"
!define GTK_BAD_INSTALL_PATH			"あなたの入力したパスにアクセスまたは作成できません。"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"あなたはGTK+のテーマをインストールする権限を持っていません。"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"アンインストーラはPidginのレジストリエントリを発見できませんでした。$\rおそらく別のユーザにインストールされたでしょう。"
!define un.PIDGIN_UNINSTALL_ERROR_2		"あなたはこのアプリケーションをアンインストールする権限を持っていません。"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"スペルチェックのサポート"
!define PIDGIN_SPELLCHECK_ERROR			"スペルチェックのインストールに失敗しました"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"スペルチェック辞書のインストールに失敗しました。"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"スペルチェックのサポート  (インターネット接続がインストールに必要です)"
!define ASPELL_INSTALL_FAILED			"インストールに失敗しました"
!define PIDGIN_SPELLCHECK_BRETON			"ブルターニュ語"
!define PIDGIN_SPELLCHECK_CATALAN			"カタルーニャ語"
!define PIDGIN_SPELLCHECK_CZECH			"チェコ語"
!define PIDGIN_SPELLCHECK_WELSH			"ウェールズ語"
!define PIDGIN_SPELLCHECK_DANISH			"デンマーク語"
!define PIDGIN_SPELLCHECK_GERMAN			"ドイツ語"
!define PIDGIN_SPELLCHECK_GREEK			"ギリシャ語"
!define PIDGIN_SPELLCHECK_ENGLISH			"英語"
!define PIDGIN_SPELLCHECK_ESPERANTO		"エスペラント語"
!define PIDGIN_SPELLCHECK_SPANISH			"スペイン語"
!define PIDGIN_SPELLCHECK_FAROESE			"フェロー語"
!define PIDGIN_SPELLCHECK_FRENCH			"フランス語"
!define PIDGIN_SPELLCHECK_ITALIAN			"イタリア語"
!define PIDGIN_SPELLCHECK_DUTCH			"オランダ語"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"ノルウェー後"
!define PIDGIN_SPELLCHECK_POLISH			"ポーランド語"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"ポルトガル語"
!define PIDGIN_SPELLCHECK_ROMANIAN		"ルーマニア語"
!define PIDGIN_SPELLCHECK_RUSSIAN			"ロシア語"
!define PIDGIN_SPELLCHECK_SLOVAK			"スロヴァキア語"
!define PIDGIN_SPELLCHECK_SWEDISH			"スウェーデン後"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"ウクライナ語"


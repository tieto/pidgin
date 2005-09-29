;;  vim:syn=winbatch:encoding=cp932:
;;
;;  japanese.nsh
;;
;;  Japanese language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 932
;;
;;  Author: "Takeshi Kurosawa" <t-kuro@abox23.so-net.ne.jp>
;;  Version 2
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"インストーラが既に実行されています"
!define GAIM_IS_RUNNING				"Gaim が実行されています。Gaim を終了してから再度実行してください"
!define GTK_INSTALLER_NEEDED			"GTK+ランタイム環境が無いかもしくはアップグレードする必要があります。$\rv${GTK_VERSION}もしくはそれ以上のGTK+ランタイムをインストールしてください。"

; License Page
!define GAIM_LICENSE_BUTTON			"次へ >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name)はGPLライセンスの元でリリースされています。ライセンスはここに参考のためだけに提供されています。 $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaimインスタントメッセンジャ (必須)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (必須)"
!define GTK_THEMES_SECTION_TITLE		"GTK+のテーマ"
!define GTK_NOTHEME_SECTION_TITLE		"テーマなし"
!define GTK_WIMP_SECTION_TITLE			"Wimpテーマ"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurveテーマ"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blueテーマ"
!define GAIM_SHORTCUTS_SECTION_TITLE		"ショートカット"
!define GAIM_DESKTOP_SHORTCUT_SECTION_TITLE	"デスクトップ"
!define GAIM_STARTMENU_SHORTCUT_SECTION_TITLE	"スタートアップ"
!define GAIM_SECTION_DESCRIPTION		"Gaimの核となるファイルとdll"
!define GTK_SECTION_DESCRIPTION			"Gaimの使っているマルチプラットフォームGUIツールキット"
!define GTK_THEMES_SECTION_DESCRIPTION		"GTK+のテーマは、GTK+のアプリケーションのルック＆フィールを変えられます。"
!define GTK_NO_THEME_DESC			"GTK+のテーマをインストールしない"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows impersonator)はWindowsのデスクトップ環境とよく調和したテーマです。"
!define GTK_BLUECURVE_THEME_DESC		"Bluecurveテーマ。"
!define GTK_LIGHTHOUSEBLUE_THEME_DESC		"Lighthouseblueテーマ。"
!define GAIM_SHORTCUTS_SECTION_DESCRIPTION	"Gaim を実行するためのショートカット"
!define GAIM_DESKTOP_SHORTCUT_DESC		"デスクトップに Gaim のショートカットを作成する"
!define GAIM_STARTMENU_SHORTCUT_DESC		"スタートメニューに Gaim の項目を作成する"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"古いバージョンのGTK+ランタイムが見つかりました。アップグレードしますか?$\r注意: Gaimはアップグレードしない限り動かないでしょう。"

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Windows GaimのWebページを訪れてください。"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (削除のみ)"
!define GAIM_PROMPT_WIPEOUT			"古いGaimのフォルダの削除に関して。続行しますか?$\r$\r注意: あなたのインストールしたすべての非標準なプラグインは削除されます。$\rGaimの設定は影響を受けません。"
!define GAIM_PROMPT_DIR_EXISTS			"あなたの指定したインストール先のフォルダはすでに存在しています。内容はすべて$\r削除されます。続行しますか?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ランタイムのインストールでエラーが発生しました。"
!define GTK_BAD_INSTALL_PATH			"あなたの入力したパスにアクセスまたは作成できません。"

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"あなたはGTK+のテーマをインストールする権限を持っていません。"

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"アンインストーラはGaimのレジストリエントリを発見できませんでした。$\rおそらく別のユーザにインストールされたでしょう。"
!define un.GAIM_UNINSTALL_ERROR_2		"あなたはこのアプリケーションをアンインストールする権限を持っていません。"

; Spellcheck Section Prompts
!define GAIM_SPELLCHECK_SECTION_TITLE		"スペルチェックのサポート"
!define GAIM_SPELLCHECK_ERROR			"スペルチェックのインストールに失敗しました"
!define GAIM_SPELLCHECK_DICT_ERROR		"スペルチェック辞書のインストールに失敗しました。"
!define GAIM_SPELLCHECK_SECTION_DESCRIPTION	"スペルチェックのサポート  (インターネット接続がインストールに必要です)"
!define ASPELL_INSTALL_FAILED			"インストールに失敗しました"
!define GAIM_SPELLCHECK_BRETON			"ブルターニュ語"
!define GAIM_SPELLCHECK_CATALAN			"カタルーニャ語"
!define GAIM_SPELLCHECK_CZECH			"チェコ語"
!define GAIM_SPELLCHECK_WELSH			"ウェールズ語"
!define GAIM_SPELLCHECK_DANISH			"デンマーク語"
!define GAIM_SPELLCHECK_GERMAN			"ドイツ語"
!define GAIM_SPELLCHECK_GREEK			"ギリシャ語"
!define GAIM_SPELLCHECK_ENGLISH			"英語"
!define GAIM_SPELLCHECK_ESPERANTO		"エスペラント語"
!define GAIM_SPELLCHECK_SPANISH			"スペイン語"
!define GAIM_SPELLCHECK_FAROESE			"フェロー語"
!define GAIM_SPELLCHECK_FRENCH			"フランス語"
!define GAIM_SPELLCHECK_ITALIAN			"イタリア語"
!define GAIM_SPELLCHECK_DUTCH			"オランダ語"
!define GAIM_SPELLCHECK_NORWEGIAN		"ノルウェー後"
!define GAIM_SPELLCHECK_POLISH			"ポーランド語"
!define GAIM_SPELLCHECK_PORTUGUESE		"ポルトガル語"
!define GAIM_SPELLCHECK_ROMANIAN		"ルーマニア語"
!define GAIM_SPELLCHECK_RUSSIAN			"ロシア語"
!define GAIM_SPELLCHECK_SLOVAK			"スロヴァキア語"
!define GAIM_SPELLCHECK_SWEDISH			"スウェーデン後"
!define GAIM_SPELLCHECK_UKRAINIAN		"ウクライナ語"


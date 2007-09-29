;;
;;  Arabic.nsh
;;
;;  Arabic language translated strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1256
;;
;;  Version 3

; Startup Checks
!define INSTALLER_IS_RUNNING			"ÇבדËÈרצÊ םÚדב ÈÇבÝÚב."
!define PIDGIN_IS_RUNNING			"ÈצÏתÌצה םÚדב ÍÇבםÇ.  דה ÝÖב‗ ÃÛבÞ ÈצÏתÌה Ëד ÃÚÏ ÇבדÍÇזבÉ."
!define GTK_INSTALLER_NEEDED			"ÈםÆÉ ÌÊ‗+ (GTK+) דÝÞזÏÉ Ãז ÊÍÊÇÌ בבÊÍÏםË.$\rדה ÝÖב‗ ËÈרצÊ v${GTK_MIN_VERSION} Ãז ÃÍÏË דה ÈםÆÉ ÌÊ‗+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"ÇבÊÇבם >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) םÎÖÚ בÑÎÕÉ Ìהז ÇבÚדזדםרÉ ÇבÚÇדÉ (GPL). ÇבÑÎÕÉ ÇבדÚØÇÉ והÇ בÛÑÖ ÇבÅÚבÇד ÝÞØ. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Úדםב ÇבÊÑÇÓב ÇבÝזÑם ÈצÏתÌצה (דØבזÈ)"
!define GTK_SECTION_TITLE			"ÈםÆÉ ÌÊ‗+ (דØבזÈÉ Åה בד Ê‗ה דזÌזÏÉ)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"ÇבÇÎÊÕÇÑÇÊ"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"ÓØÍ Çבד‗ÊÈ"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"ÞÇÆדÉ ÇÈÏÃ"
!define PIDGIN_SECTION_DESCRIPTION		" ז דבÝÇÊ בץÈ ÈצÏתÌצה dll"
!define GTK_SECTION_DESCRIPTION		"ÚÏרÉ זÇÌוÉ ÑÓזדםרÉ דÊÚÏÏÉ ÇבדהÕÇÊ¡ םÓÊÎÏדוÇ ÈצÏתÌצה."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"ÇÎÊÕÇÑÇÊ בÈÏÃ ÈצÏתÌצה"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"ÃהÔÆ ÇÎÊÕÇÑÇ בÈצÏתÌצה Úבל ÓØÍ Çבד‗ÊÈ"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"ÃהÔÆ דץÏÎבÉ בÈÏÌה Ýם ÞÇÆדÉ ÇÈÏÃ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"זץÌצÏÊ הÓÎÉ ÞÏםדÉ דה ÈםÆÉ ÌÊ‗+. ÃÊÑםÏ ÇבÊÍÏםË¿$\rבÇÍÙ: ÞÏ בÇ םÚדב $(^Name) דÇבד ÊÝÚב וÐÇ."
!define GTK_WINDOWS_INCOMPATIBLE		"בÇ םÊזÇÝÞ זםהÏזÒ 95/98/Me דÚ ÌÊ‗+ 2.8.0 Ãז ÃÍÏË.  ÌÊ‗+ ${GTK_INSTALL_VERSION} בה ÊץËÈרÊ.$\rÅÐÇ בד ם‗ה בÏם‗ ÌÊ‗+ ${GTK_MIN_VERSION} Ãז ÃÍÏË דËÈÊÉ ÈÇבÝÚב¡ ÓםץÍÈØ ÇבÊËÈםÊ."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Þד ÈÒםÇÑÉ ÕÝÍÉ ÈÏÌה Úבל ÇבזצÈ"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"ÊÚÐרÑ ÅÒÇבÉ הÓÎÉ ÈÏÌה ÇבדËÈרÊÉ. ËÊץËÈרÊ ÇבהÓÎÉ ÇבÍÏםËÉ ÈÏזה ÅÒÇבÉ ÇבהÓÎÉ ÇבדËÈרÊÉ דÓÈÞÇ."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"ÎØÃ ÃËהÇÁ ÊËÈםÊ ÈםÆÉ ÌÊ‗+."
!define GTK_BAD_INSTALL_PATH			"בÇ םד‗ה ÇבזÕזב Ãז ÅהÔÇÁ ÇבדÓÇÑ ÇבÐם ÍÏÏÊו."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"דÊÚÇדבÇÊ ÇבדÓÇÑÇÊ"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"בד םץÚËÑ ÇבדËÈÊ Úבל ÎÇהÇÊ ÇבÓÌב ÇבÎÇÕÉ ÈÈÏÌה.$\rÛÇבÈÇ ËÈÊ וÐÇ ÇבÈÑהÇדÌ דÓÊÎÏד ÂÎÑ."
!define un.PIDGIN_UNINSTALL_ERROR_2		"בÇ Êדב‗ ÇבÕבÇÍםÇÊ בÊËÈםÊ וÐÇ ÇבÊØÈםÞ."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"ÏÚד ÇבÊÏÞםÞ ÇבÅדבÇÆם"
!define PIDGIN_SPELLCHECK_ERROR		"ÎØÃ ÃËהÇÁ ÊËÈםÊ ÇבÊÏÞםÞ ÇבÅדבÇÆם"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"ÎØÃ ÃËהÇÁ ÊËÈםÊ ÞÇדזÓ ÇבÊÏÞםÞ ÇבÅדבÇÆם"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"ÏÚד ÇבÊÏÞםÞ ÇבÅדבÇÆם.  (דØבזÈ ÇÊÕÇב ÈÇבÅהÊÑהÊ בבÊËÈםÊ)"
!define ASPELL_INSTALL_FAILED			"ÝÔב ÇבÊËÈםÊ"
!define PIDGIN_SPELLCHECK_BRETON		"Breton"
!define PIDGIN_SPELLCHECK_CATALAN		"Catalan"
!define PIDGIN_SPELLCHECK_CZECH		"Czech"
!define PIDGIN_SPELLCHECK_WELSH		"Welsh"
!define PIDGIN_SPELLCHECK_DANISH		"Danish"
!define PIDGIN_SPELLCHECK_GERMAN		"German"
!define PIDGIN_SPELLCHECK_GREEK		"Greek"
!define PIDGIN_SPELLCHECK_ENGLISH		"English"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spanish"
!define PIDGIN_SPELLCHECK_FAROESE		"Faroese"
!define PIDGIN_SPELLCHECK_FRENCH		"French"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italian"
!define PIDGIN_SPELLCHECK_DUTCH		"Dutch"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norwegian"
!define PIDGIN_SPELLCHECK_POLISH		"Polish"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portuguese"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Romanian"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russian"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slovak"
!define PIDGIN_SPELLCHECK_SWEDISH		"Swedish"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ukrainian"


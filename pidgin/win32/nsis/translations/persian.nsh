;;
;;  persian.nsh
;;
;;  Default language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: CP1256
;;  As this file needs to be encoded in CP1256 and CP1256 doesn't support U+06CC 
;;  and U+0654 characters, I have removed all U+0654 characters and replaced U+06CC
;;  with U+064A in the middle of the words and with U+0649 at the end of the words.
;;  The Persian text will display correctly but the encoding is incorrect.
;;
;;  Author: Elnaz Sarbar <elnaz@farsiweb.info>, 2007

; Startup Checks
!define INSTALLER_IS_RUNNING			"äÕÈ˜ääÏå ÇÒ ŞÈá ÏÑ ÍÇá ÇÌÑÇ ÇÓÊ."
!define PIDGIN_IS_RUNNING			"ÈÑäÇãå íÌíä ÇÒ ŞÈá ÏÑ ÍÇá ÇÌÑÇ ÇÓÊ. áØİÇğ ÇÒ íÌíä ÎÇÑÌ ÔæÏ æ ÏæÈÇÑå ÓÚì ˜äíÏ."

; License Page
!define PIDGIN_LICENSE_BUTTON			"ÈÚÏ >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) ÊÍÊ ãÌæÒ Úãæãì åãÇäì äæ (GPL) ãäÊÔÑ ÔÏå ÇÓÊ. Çíä ãÌæÒ ÊäåÇ ÈÑÇì ÇØáÇÚÑÓÇäì ÇíäÌÇ ÇÑÇÆå ÔÏå ÇÓÊ. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"˜ÇÑíÑ íÛÇãÑÓÇä ÇíäÊÑäÊì íÌíä (ÇÌÈÇÑì)"
!define GTK_SECTION_TITLE			"ãÍíØ ÒãÇä ÇÌÑÇì GTK+ı (ÇÑ æÌæÏ äÏÇÑÏ ÇÌÈÇÑì ÇÓÊ)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"ãíÇäÈõÑåÇ"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"ÑæãíÒì"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"ãäæì ÂÛÇÒ"
!define PIDGIN_SECTION_DESCRIPTION		"ÑæäÏååÇ æ DLLåÇì ÇÕáì íÌíä"
!define GTK_SECTION_DESCRIPTION		"ÌÚÈåÇÈÒÇÑ ÑÇÈØ ˜ÇÑÈÑ ÑÇİí˜ì äÏ ÈÓÊÑì ˜å íÌíä ÇÒ Âä ÇÓÊİÇÏå ãì˜äÏ"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"ãíÇäÈõÑåÇì ÑÇåÇäÏÇÒì íÌíä"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"ÇíÌÇÏ ãíÇäÈõÑ Èå íÌíä Ñæì ÑæãíÒì"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"ÇíÌÇÏ ãæÑÏ ÈÑÇì íÌíä ÏÑ ãäæ ÂÛÇÒ"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"ÕİÍå æÈì íÌíä ÑÇ ÈÈíäíÏ"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"ÍĞİ äÓÎåÇì ÇÒ íÌíä ˜å ÏÑ ÍÇá ÍÇÖÑ äÕÈ ÇÓÊ ãã˜ä äíÓÊ. äÓÎå ÌÏíÏ ÈÏæä ÍĞİ äÓÎå ãæÌæÏ äÕÈ ãìÔæÏ."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"ãÊÕÏìåÇì äÔÇäì ÇíäÊÑäÊì"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"ÍĞİ˜ääÏå äãìÊæÇäÏ ãÏÇÎá registery ííä ÑÇ íÏÇ ˜äÏ.$\r ãã˜ä ÇÓÊ ˜ÇÑÈÑ ÏíÑì Çíä ÈÑäÇãå ÑÇ äÕÈ ˜ÑÏå ÈÇÔÏ."
!define un.PIDGIN_UNINSTALL_ERROR_2		"ÔãÇ ÇÌÇÒå áÇÒã ÈÑÇì ÍĞİ Çíä ÈÑäÇãå ÑÇ äÏÇÑíÏ."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"ÔÊíÈÇäì ÛáØíÇÈì ÇãáÇíì"
!define PIDGIN_SPELLCHECK_ERROR		"ÎØÇ åäÇã äÕÈ ÛáØíÇÈ ÇãáÇíì"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"ÔÊíÈÇäì ÛáØíÇÈì ÇãáÇíì. (ÈÑÇì äÕÈ ÇÊÕÇá ÇíäÊÑäÊì áÇÒã ÇÓÊ)"


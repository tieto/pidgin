;;
;;  hebrew.nsh
;;
;;  Hebrew language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1255
;;
;;  Updated: Shalom Craimer <scraimer@gmail.com>
;;  Origional Author: Eugene Shcherbina <eugene@websterworlds.com>
;;  Version 3
;;

; Startup checks
!define INSTALLER_IS_RUNNING			"תוכנת ההתקנה כבר רצה."
!define PIDGIN_IS_RUNNING			"עותק של פידג'ין כבר רץ. יש לסגור את פידג'ין ולנסות שנית."
!define GTK_INSTALLER_NEEDED			".לא נמצאת או צריכה שידרוג GTK+ סביבת$\rבבקשה התקן v${GTK_MIN_VERSION} .GTK+ או גבוהה יותר של סביבת"

; License Page
!define PIDGIN_LICENSE_BUTTON			"הבא >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) .הרישיון נמצא כאן בשביל מידע בלבד .GPL משוחרר תחת רישיון $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"(חובה) .Pidgin תוכנת"
!define GTK_SECTION_TITLE			"(חובה) .GTK+ סביבת"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"קיצורי דרך"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"שולחן העבודה"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"תפריט ההתחל"
!define PIDGIN_SECTION_DESCRIPTION		".בסיסיים DLL-ו Pidgin קבצי"
!define GTK_SECTION_DESCRIPTION		"מולטי-פלטפורמי, בו נעזר פידג'ין GUI כלי"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"קיצורי-דרך להפעלת פידג'ין"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"צור קיצור-דרך עבור פידג'ין על שולחן העבודה"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"צור קיצור-דרך עבור פידג'ין בתפריט ההתחל"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"A?נמצאה. לשדרג GTK+ סביבה ישנה של$\rNote: .יכול לא לעבוד אם לא $(^Name)"
!define GTK_WINDOWS_INCOMPATIBLE		"חלונות 95/98/מילניום אינם תואמים ל-GTK+ 2.8.0 ומעלה. GTK+ ${GTK_INSTALL_VERSION} לא יותקן.$\rאם אין לך GTK+ ${GTK_MIN_VERSION} ומעלה מותקן, ההתקנה כעת תעצור."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		".Pidginבקרו באתר של "

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"לא מסוגל להסיר את הגירסא המותקנת של פידג'ין. הגירסא החדשה תותקן ללא הסרת הגרסא המותקנת."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			".GTK+ שגיאה בהתקנת סביבת"
!define GTK_BAD_INSTALL_PATH			".המסלול שציינת לא יכול להיווצר"

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"מנהלי URI"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		".GTK+ ההתקנה לא מצאה את הרישומים של$\r.יכול להיות שמשמתמש אחר התקין את התוכנה הזאת"
!define un.PIDGIN_UNINSTALL_ERROR_2		".אין לך זכות למחוק תוכנה זאת"

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"תמיכה בבדיקות איות"
!define PIDGIN_SPELLCHECK_ERROR		"שגיאה בהתקנת בדיקות איות"
!define PIDGIN_SPELLCHECK_DICT_ERROR	"שגיאה בהתקנת מילון לבדיקות איות"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"תמיכה עבור בדיקות איות (דורש חיבור אינטרנט להתקנה)"
!define ASPELL_INSTALL_FAILED			"ההתקנה נכשלה"
!define PIDGIN_SPELLCHECK_BRETON		"ברטון"
!define PIDGIN_SPELLCHECK_CATALAN		"קטלונית"
!define PIDGIN_SPELLCHECK_CZECH		"צ'כית"
!define PIDGIN_SPELLCHECK_WELSH		"וולשית"
!define PIDGIN_SPELLCHECK_DANISH		"דנית"
!define PIDGIN_SPELLCHECK_GERMAN		"גרמנית"
!define PIDGIN_SPELLCHECK_GREEK		"יוונית"
!define PIDGIN_SPELLCHECK_ENGLISH		"אנגלית"
!define PIDGIN_SPELLCHECK_ESPERANTO		"אספרנטו"
!define PIDGIN_SPELLCHECK_SPANISH		"ספרדית"
!define PIDGIN_SPELLCHECK_FAROESE		"פרסית"
!define PIDGIN_SPELLCHECK_FRENCH		"צרפתית"
!define PIDGIN_SPELLCHECK_ITALIAN		"אילטקית"
!define PIDGIN_SPELLCHECK_DUTCH		"הולנדית"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"נורווגית"
!define PIDGIN_SPELLCHECK_POLISH		"פולנית"
!define PIDGIN_SPELLCHECK_PORTUGUESE	"פורטוגזית"
!define PIDGIN_SPELLCHECK_ROMANIAN		"רומנית"
!define PIDGIN_SPELLCHECK_RUSSIAN		"רוסית"
!define PIDGIN_SPELLCHECK_SLOVAK		"סלווקית"
!define PIDGIN_SPELLCHECK_SWEDISH		"שוודית"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"אוקראינית"


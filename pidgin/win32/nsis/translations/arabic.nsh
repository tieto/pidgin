;;
;;  Arabic.nsh
;;
;;  Arabic language translated strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"المثبِّت يعمل بالفعل."
!define PIDGIN_IS_RUNNING			"بِدْجِن يعمل حاليا.  من فضلك أغلق بِدْجن ثم أعد المحاولة."
!define GTK_INSTALLER_NEEDED			"بيئة جتك+ (GTK+) مفقودة أو تحتاج للتحديث.$\rمن فضلك ثبِّت v${GTK_MIN_VERSION} أو أحدث من بيئة جتك+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"التالي >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) يخضع لرخصة جنو العموميّة العامة (GPL). الرخصة المعطاة هنا لغرض الإعلام فقط. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"عميل التراسل الفوري بِدْجِن (مطلوب)"
!define GTK_SECTION_TITLE			"بيئة جتك+ (مطلوبة إن لم تكن موجودة)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"الاختصارات"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"سطح المكتب"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"قائمة ابدأ"
!define PIDGIN_SECTION_DESCRIPTION		" و ملفات لُب بِدْجِن dll"
!define GTK_SECTION_DESCRIPTION		"عدّة واجهة رسوميّة متعددة المنصات، يستخدمها بِدْجِن."

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"اختصارات لبدأ بِدْجِن"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"أنشئ اختصارا لبِدْجِن على سطح المكتب"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"أنشئ مُدخلة لبدجن في قائمة ابدأ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"وُجِدت نسخة قديمة من بيئة جتك+. أتريد التحديث؟$\rلاحظ: قد لا يعمل $(^Name) مالم تفعل هذا."
!define GTK_WINDOWS_INCOMPATIBLE		"لا يتوافق ويندوز 95/98/Me مع جتك+ 2.8.0 أو أحدث.  جتك+ ${GTK_INSTALL_VERSION} لن تُثبّت.$\rإذا لم يكن لديك جتك+ ${GTK_MIN_VERSION} أو أحدث مثبتة بالفعل، سيُحبط التثبيت."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"قم بزيارة صفحة بدجن على الوِب"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"تعذّر إزالة نسخة بدجن المثبّتة. ثتُثبّت النسخة الحديثة بدون إزالة النسخة المثبّتة مسبقا."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"خطأ أثناء تثبيت بيئة جتك+."
!define GTK_BAD_INSTALL_PATH			"لا يمكن الوصول أو إنشاء المسار الذي حددته."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"متعاملات المسارات"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"لم يُعثر المثبت على خانات السجل الخاصة ببدجن.$\rغالبا ثبت هذا البرنامج مستخدم آخر."
!define un.PIDGIN_UNINSTALL_ERROR_2		"لا تملك الصلاحيات لتثبيت هذا التطبيق."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"دعم التدقيق الإملائي"
!define PIDGIN_SPELLCHECK_ERROR		"خطأ أثناء تثبيت التدقيق الإملائي"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"خطأ أثناء تثبيت قاموس التدقيق الإملائي"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"دعم التدقيق الإملائي.  (مطلوب اتصال بالإنترنت للتثبيت)"
!define ASPELL_INSTALL_FAILED			"فشل التثبيت"
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


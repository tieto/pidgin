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
!define INSTALLER_IS_RUNNING			"הױָ˜ההֿו ַׂ Þָב ֿׁ ַֽב ַַּׁ ַ׃Ê."
!define PIDGIN_IS_RUNNING			"ָׁהַדו םּםה ַׂ Þָב ֿׁ ַֽב ַַּׁ ַ׃Ê. ב״Ýַנ ַׂ םּםה ־ַּׁ װזֿ ז ֿזַָׁו ׃Úל ˜הםֿ."
!define GTK_INSTALLER_NEEDED			"דֽם״ ׂדַה ַַּׁל GTK+ םַ זּזֿ הַֿֿׁ םַ בַׂד ַ׃Ê ַׁÊÞֱַ םַֿ ˜הֿ.$\rב״Ýַנ ה׃־ו ${GTK_MIN_VERSION} םַ ַָבַÊׁל ַׂ דֽם״ ׂדַה ַַּׁל GTK+ הױָ ˜הםֿ"

; License Page
!define PIDGIN_LICENSE_BUTTON			"ָÚֿ >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) ÊֽÊ דּזׂ Úדזדל ודַהל הז (GPL) דהÊװׁ װֿו ַ׃Ê. ַםה דּזׂ Êהוַ ַָׁל ַ״בַÚׁ׃ַהל ַםהַּ ֶַַׁו װֿו ַ׃Ê. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"˜ַׁםׁ םÛַדׁ׃ַה ַםהÊׁהÊל םּםה (ַַָּׁל)"
!define GTK_SECTION_TITLE			"דֽם״ ׂדַה ַַּׁל GTK+‎ (ַׁ זּזֿ הַֿֿׁ ַַָּׁל ַ׃Ê)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"דםַהָץׁוַ"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"ׁזדםׂל"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"דהזל ֲÛַׂ"
!define PIDGIN_SECTION_DESCRIPTION		"ׁזהֿווַ ז DLLוַל ַױבל םּםה"
!define GTK_SECTION_DESCRIPTION		"ּÚָוַַָׁׂ ַָׁ״ ˜ַָׁׁ ַׁÝם˜ל הֿ ָ׃Êׁל ˜ו םּםה ַׂ ֲה ַ׃ÊÝַֿו דל˜הֿ"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"דםַהָץׁוַל ַׁוַהַֿׂל םּםה"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"ַםַּֿ דםַהָץׁ ָו םּםה ׁזל ׁזדםׂל"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"ַםַּֿ דזֿׁ ַָׁל םּםה ֿׁ דהז ֲÛַׂ"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"ה׃־ו Þֿםדל דֽם״ ׂדַה ַַּׁל GTK+ םַֿ װֿ. ֲםַ דַםבםֿ ֲה ַׁ ַׁÊÞֱַ ֿוםֿ¿$\rÊזּו: $(^Name) דד˜ה ַ׃Ê Þָב ַׂ ַׁÊÞֱַ ˜ַׁ ה˜הֿ."
!define GTK_WINDOWS_INCOMPATIBLE		"זםהֿזׂ 95/98‏/Me ַָ GTK‎+‎ ה׃־ו 2.8.0 םַ ּֿםֿÊׁ ׃ַׂַׁ הם׃Ê. GTK+ ${GTK_INSTALL_VERSION} הױָ ה־זַוֿ װֿ.$\r ַׁ  GTK+ ${GTK_MIN_VERSION} םַ ּֿםֿÊׁ ַׁ Þָבַנ הױָ ה˜ֿׁוַםֿ¡ הױָ Þ״Ú ־זַוֿ װֿ."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"ױÝֽו זָל םּםה ַׁ ָָםהםֿ"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"ֽ׀Ý ה׃־וַל ַׂ םּםה ˜ו ֿׁ ַֽב ַֽײׁ הױָ ַ׃Ê דד˜ה הם׃Ê. ה׃־ו ּֿםֿ ָֿזה ֽ׀Ý ה׃־ו דזּזֿ הױָ דלװזֿ."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"־״ַ והַד הױָ דֽם״ ׂדַה ַַּׁל GTK+‎."
!define GTK_BAD_INSTALL_PATH			"ד׃םׁל ˜ו זַֿׁ ˜ֿׁוַםֿ Þַָב ֿ׃Êׁ׃ל םַ ַםַּֿ הם׃Ê."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"דÊױֿלוַל הװַהל ַםהÊׁהÊל"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"ֽ׀Ý˜ההֿו הדלÊזַהֿ דַֿ־ב registery םםה ַׁ םַֿ ˜הֿ.$\r דד˜ה ַ׃Ê ˜ַָׁׁ ֿםׁל ַםה ָׁהַדו ַׁ הױָ ˜ֿׁו ַָװֿ."
!define un.PIDGIN_UNINSTALL_ERROR_2		"װדַ ַַּׂו בַׂד ַָׁל ֽ׀Ý ַםה ָׁהַדו ַׁ הַֿׁםֿ."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"װÊםַָהל Ûב״םַָל ַדבַםל"
!define PIDGIN_SPELLCHECK_ERROR		"־״ַ והַד הױָ Ûב״םַָ ַדבַםל"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"־״ַ והַד הױָ בÛÊהַדו Ûב״םַָ ַדבַםל"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"װÊםַָהל Ûב״םַָל ַדבַםל. (ַָׁל הױָ ַÊױַב ַםהÊׁהÊל בַׂד ַ׃Ê)"
!define ASPELL_INSTALL_FAILED			"הױָ װ˜׃Ê ־זֿׁ"
!define PIDGIN_SPELLCHECK_BRETON		"ָׁÊַהםַםל"
!define PIDGIN_SPELLCHECK_CATALAN		"˜ַÊַבַה"
!define PIDGIN_SPELLCHECK_CZECH		"˜ל"
!define PIDGIN_SPELLCHECK_WELSH		"זםבׂל"
!define PIDGIN_SPELLCHECK_DANISH		"ַֿהדַׁ˜ל"
!define PIDGIN_SPELLCHECK_GERMAN		"ֲבדַהל"
!define PIDGIN_SPELLCHECK_GREEK		"םזהַהל"
!define PIDGIN_SPELLCHECK_ENGLISH		"ַהבם׃ל"
!define PIDGIN_SPELLCHECK_ESPERANTO		"ַ׃ַׁהÊז"
!define PIDGIN_SPELLCHECK_SPANISH		"ַ׃ַהםַםל"
!define PIDGIN_SPELLCHECK_FAROESE		"Ýַׁזםל"
!define PIDGIN_SPELLCHECK_FRENCH		"Ýַׁה׃זל"
!define PIDGIN_SPELLCHECK_ITALIAN		"ַםÊַבםַםל"
!define PIDGIN_SPELLCHECK_DUTCH		"ובהֿל"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"הׁזל"
!define PIDGIN_SPELLCHECK_POLISH		"בו׃Êַהל"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"ׁÊÛַבל"
!define PIDGIN_SPELLCHECK_ROMANIAN		"ׁזדַהםַםל"
!define PIDGIN_SPELLCHECK_RUSSIAN		"ׁז׃ל"
!define PIDGIN_SPELLCHECK_SLOVAK		"ַ׃בזַ˜ל"
!define PIDGIN_SPELLCHECK_SWEDISH		"׃זֶֿל"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"ַז˜ַׁםהל"


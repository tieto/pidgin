;;
;;  polish.nsh
;;
;;  Polish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Version 3
;;  Note: If translating this file, replace '!insertmacro PIDGIN_MACRO_DEFAULT_STRING'
;;  with '!define'.

; Make sure to update the PIDGIN_MACRO_LANGUAGEFILE_END macro in
; langmacros.nsh when updating this file

; Startup Checks
!define INSTALLER_IS_RUNNING			"Instalator jest ju¿ uruchomiony."
!define PIDGIN_IS_RUNNING			"Program Pidgin jest obecnie uruchomiony. Proszê zakoñczyæ dzia³anie programu Pidgin i spróbowaæ ponownie."
!define GTK_INSTALLER_NEEDED			"Brak biblioteki GTK+ lub wymaga zaktualizowania.$\rProszê zainstalowaæ wersjê ${GTK_MIN_VERSION} lub wy¿sz¹ biblioteki GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Dalej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"Program $(^Name) jest rozpowszechniany na warunkach Powszechnej Licencji Publicznej GNU (GPL). Licencja jest tu podana wy³¹cznie w celach informacyjnych. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Klient komunikatora Pidgin (wymagany)"
!define GTK_SECTION_TITLE			"Biblioteka GTK+ (wymagana, jeœli nie jest obecna)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Skróty"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Pulpit"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Start"
!define PIDGIN_SECTION_DESCRIPTION		"G³ówne pliki programu Pidgin i biblioteki DLL"
!define GTK_SECTION_DESCRIPTION		"Wieloplatformowy zestaw narzêdzi do tworzenia interfejsu graficznego, u¿ywany przez program Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Skróty do uruchamiania programu Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Utworzenie skrótu do programu Pidgin na pulpicie"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Utworzenie wpisu w menu Start dla programu Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Odnaleziono star¹ wersjê biblioteki GTK+. Zaktualizowaæ j¹?$\rUwaga: program $(^Name) mo¿e bez tego nie dzia³aæ."
!define GTK_WINDOWS_INCOMPATIBLE		"Systemy Windows 95/98/Me s¹ niezgodne z bibliotek¹ GTK+ 2.8.0 lub nowsz¹. Biblioteka GTK+ ${GTK_INSTALL_VERSION} nie zostanie zainstalowana.$\rJeœli brak zainstalowanej biblioteki GTK+ ${GTK_MIN_VERSION} lub nowszej, instalacja zostanie przerwana."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"OdwiedŸ stronê WWW programu Pidgin"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Nie mo¿na odinstalowaæ obecnie zainstalowanej wersji programu Pidgin. Nowa wersja zostanie zainstalowana bez usuwania obecnie zainstalowanej wersji."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"B³¹d podczas instalowania biblioteki GTK+."
!define GTK_BAD_INSTALL_PATH			"Nie mo¿na uzyskaæ dostêpu do podanej œcie¿ki lub jej utworzyæ."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"Obs³uga adresów URI"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Instalator nie mo¿e odnaleŸæ wpisów w rejestrze dla programu Pidgin.$\rMo¿liwe, ¿e inny u¿ytkownik zainstalowa³ ten program."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Brak uprawnieñ do odinstalowania tego programu."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Obs³uga sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_ERROR		"B³¹d podczas instalowania sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"B³¹d podczas instalowania s³ownika dla sprawdzania pisowni"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Obs³uga sprawdzania pisowni (do jej instalacji wymagane jest po³¹czenie z Internetem)."
!define ASPELL_INSTALL_FAILED			"Instalacja nie powiod³a siê"
!define PIDGIN_SPELLCHECK_BRETON		"bretoñski"
!define PIDGIN_SPELLCHECK_CATALAN		"kataloñski"
!define PIDGIN_SPELLCHECK_CZECH		"czeski"
!define PIDGIN_SPELLCHECK_WELSH		"walijski"
!define PIDGIN_SPELLCHECK_DANISH		"duñski"
!define PIDGIN_SPELLCHECK_GERMAN		"niemiecki"
!define PIDGIN_SPELLCHECK_GREEK		"grecki"
!define PIDGIN_SPELLCHECK_ENGLISH		"angielski"
!define PIDGIN_SPELLCHECK_ESPERANTO		"esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"hiszpañski"
!define PIDGIN_SPELLCHECK_FAROESE		"farerski"
!define PIDGIN_SPELLCHECK_FRENCH		"francuski"
!define PIDGIN_SPELLCHECK_ITALIAN		"w³oski"
!define PIDGIN_SPELLCHECK_DUTCH		"holenderski"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"norweski"
!define PIDGIN_SPELLCHECK_POLISH		"polski"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"portugalski"
!define PIDGIN_SPELLCHECK_ROMANIAN		"rumuñski"
!define PIDGIN_SPELLCHECK_RUSSIAN		"rosyjski"
!define PIDGIN_SPELLCHECK_SLOVAK		"s³owacki"
!define PIDGIN_SPELLCHECK_SWEDISH		"szwedzki"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"ukraiñski"


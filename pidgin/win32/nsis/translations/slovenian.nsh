;;
;;  slovenian.nsh
;;
;;  Slovenian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Martin Srebotnjak <miles@filmsi.net>
;;  Version 3
;;

; Startup GTK+ check
!define INSTALLER_IS_RUNNING			"Namešèanje že poteka."
!define PIDGIN_IS_RUNNING			"Trenutno že teèe ena razlièica Pidgina. Prosimo, zaprite aplikacijo in poskusite znova."
!define GTK_INSTALLER_NEEDED			"Izvajalno okolje GTK+ manjka ali pa ga je potrebno nadgraditi.$\rProsimo, namestite v${GTK_MIN_VERSION} ali novejšo razlièico izvajalnega okolja GTK+"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Naprej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) je izdan pod licenco GPL. Ta licenca je tu na voljo le v informativne namene. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - odjemalec za klepet (zahtevano)"
!define GTK_SECTION_TITLE			"Izvajalno okolje GTK+ (zahtevano)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Tipke za bližnjice"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Namizje"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Zaèetni meni"
!define PIDGIN_SECTION_DESCRIPTION		"Temeljne datoteke in knjižnice za Pidgin"
!define GTK_SECTION_DESCRIPTION			"Veèplatformna orodjarna GUI, ki jo uporablja Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Bližnjice za zagon Pidgina"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Ustvari bližnjico za Pidgin na namizju"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Ustvari izbiro Pidgin v meniju Start"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Namešèeno imate starejšo razlièico izvajalnega okolja GTK+. Jo želite nadgraditi?$\rOpomba: èe je ne boste nadgradili, $(^Name) morda ne bo deloval."
!define GTK_WINDOWS_INCOMPATIBLE		"Okolja Windows 95/98/Me z GTK+ 2.8.0 ali novejšimi niso združljiva.  GTK+ ${GTK_INSTALL_VERSION} will not be installed.$\rIf you don't have GTK+ ${GTK_MIN_VERSION} ali novejši je že namešèen, zato se bo namestitev zdaj prekinila."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Obišèite spletno stran Windows Pidgin"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Trenutno namešèene razlièice Pidgina ni mogoèe odstraniti. Nova razlièica bo namešèena brez odstranitve trenutno namešèene razlièice."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Napaka pri namestitvi izvajalnega okolja GTK+."
!define GTK_BAD_INSTALL_PATH			"Pot, ki ste jo vnesli, ni dosegljiva ali je ni mogoèe ustvariti."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI Handlers"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Vnosov za Pidgin v registru ni mogoèe najti.$\rNajverjetneje je ta program namestil drug uporabnik."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Za odstranitev programa nimate ustreznih pravic."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Podpora preverjanja èrkovanja"
!define PIDGIN_SPELLCHECK_ERROR			"Napaka pri namešèanju preverjanja èrkovanja"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Napaka pri namešèanju slovarja za preverjanje èrkovanja"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Podpora preverjanja èrkovanja.  (Za namestitev je potrebna spletna povezava)"
!define ASPELL_INSTALL_FAILED			"Namestitev ni uspela."
!define PIDGIN_SPELLCHECK_BRETON		"bretonski"
!define PIDGIN_SPELLCHECK_CATALAN		"katalonski"
!define PIDGIN_SPELLCHECK_CZECH			"èeški"
!define PIDGIN_SPELLCHECK_WELSH			"velški"
!define PIDGIN_SPELLCHECK_DANISH		"danski"
!define PIDGIN_SPELLCHECK_GERMAN		"nemški"
!define PIDGIN_SPELLCHECK_GREEK			"grški"
!define PIDGIN_SPELLCHECK_ENGLISH		"angleški"
!define PIDGIN_SPELLCHECK_ESPERANTO		"esperantski"
!define PIDGIN_SPELLCHECK_SPANISH		"španski"
!define PIDGIN_SPELLCHECK_FAROESE		"farojski"
!define PIDGIN_SPELLCHECK_FRENCH		"francoski"
!define PIDGIN_SPELLCHECK_ITALIAN		"italijanski"
!define PIDGIN_SPELLCHECK_DUTCH			"nizozemski"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"norveški"
!define PIDGIN_SPELLCHECK_POLISH		"poljski"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"portugalski"
!define PIDGIN_SPELLCHECK_ROMANIAN		"romunski"
!define PIDGIN_SPELLCHECK_RUSSIAN		"ruski"
!define PIDGIN_SPELLCHECK_SLOVAK		"slovaški"
!define PIDGIN_SPELLCHECK_SLOVENIAN		"slovenski"
!define PIDGIN_SPELLCHECK_SWEDISH		"švedski"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"ukrajinski"

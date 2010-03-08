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

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Obišèite spletno stran Windows Pidgin"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Trenutno namešèene razlièice Pidgina ni mogoèe odstraniti. Nova razlièica bo namešèena brez odstranitve trenutno namešèene razlièice."

; GTK+ Section Prompts

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI Handlers"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Vnosov za Pidgin v registru ni mogoèe najti.$\rNajverjetneje je ta program namestil drug uporabnik."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Za odstranitev programa nimate ustreznih pravic."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Podpora preverjanja èrkovanja"
!define PIDGIN_SPELLCHECK_ERROR			"Napaka pri namešèanju preverjanja èrkovanja"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Podpora preverjanja èrkovanja.  (Za namestitev je potrebna spletna povezava)"

;;
;;  slovenian.nsh
;;
;;  Slovenian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Martin Srebotnjak <miles@filmsi.net>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Izvajalno okolje GTK+ manjka ali pa ga je potrebno nadgraditi.$\rProsimo namestite v${GTK_VERSION} ali višjo razlièico izvajalnega okolja GTK+"

; License Page
!define GAIM_LICENSE_BUTTON			"Naprej >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) je na voljo pod licenco GPL. Ta licenca je tu na voljo le v informativne namene. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim - odjemalec za klepet (potrebno)"
!define GTK_SECTION_TITLE			"GTK+ izvajalno okolje (zahtevano)"
!define GTK_THEMES_SECTION_TITLE		"Teme GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Brez teme"
!define GTK_WIMP_SECTION_TITLE		"Tema Wimp"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema Bluecurve"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema Light House Blue"
!define GAIM_SECTION_DESCRIPTION		"Temeljne datoteke Gaima"
!define GTK_SECTION_DESCRIPTION		"Veèplatformna orodjarna GUI, ki jo uporablja Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Teme GTK+ lahko spremenijo izgled programov GTK+."
!define GTK_NO_THEME_DESC			"Brez namestitve teme GTK+"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (oponaševalec Oken) je tema GTK, ki se lepo vklaplja v namizno okolje Windows."
!define GTK_BLUECURVE_THEME_DESC		"Tema Bluecurve."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"Tema Lighthouseblue."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Našel sem staro razlièico izvajalnega okolja GTK+. Jo želite nadgraditi?$\rOpomba: Gaim morda ne bo deloval, èe je ne boste nadgradili."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Obišèite spletno stran Windows Gaim"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (samo odstrani)"
!define GAIM_PROMPT_WIPEOUT			"Vaš star imenik Gaim bo zbrisan. Želite nadaljevati?$\r$\rOpomba: Vsi nestandardni vtièniki, ki ste jih namestili, bodo zbrisani.$\rUporabniške nastavitve za Gaim se bodo ohranile."
!define GAIM_PROMPT_DIR_EXISTS		"Namestitveni imenik, ki ste ga navedli, že obstaja. Vsa vsebina$\rbo zbrisana. Želite nadaljevati?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Napaka pri namestitvi GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Pot, ki ste jo vnesli, ni dosegljiva ali je ni mogoèe ustvariti."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Nimate pravic za nametitev teme GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Ne morem najti vnosov v registru za Gaim.$\rNajverjetneje je ta program namestil drug uporabnik."
!define un.GAIM_UNINSTALL_ERROR_2		"Nimate pravic za odstranitev programa."

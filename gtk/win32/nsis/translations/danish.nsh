;;
;;  danish.nsh
;;
;;  Danish language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Ewan Andreasen <wiredloose@myrealbox.com>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime environment enten mangler eller skal opgraderes.$\rInstallér venligst GTK+ runtime version v${GTK_VERSION} eller højere."

; License Page
!define GAIM_LICENSE_BUTTON			"Næste >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) er frigivet under GPL licensen. Licensen er kun medtaget her til generel orientering. $_CLICK"

; Components Page
!define GAIM_SECTION_TITLE			"Gaim Instant Messaging Client (obligatorisk)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (obligatorisk)"
!define GTK_THEMES_SECTION_TITLE		"GTK+ Temaer"
!define GTK_NOTHEME_SECTION_TITLE		"Intet Tema"
!define GTK_WIMP_SECTION_TITLE		"Wimp Tema"
!define GTK_BLUECURVE_SECTION_TITLE		"Bluecurve Tema"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Light House Blue Tema"
!define GAIM_SECTION_DESCRIPTION		"Basale Gaim filer og biblioteker"
!define GTK_SECTION_DESCRIPTION		"Et multi-platform grafisk interface udviklingsværktøj, bruges af Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"GTK+ Temaer kan ændre GTK+ programmers generelle udseende."
!define GTK_NO_THEME_DESC			"Installér ikke noget GTK+ tema"
!define GTK_WIMP_THEME_DESC			"GTK-Wimp (Windows efterligner) er et GTK tema som falder i med Windows skrivebordsmiljøet."
!define GTK_BLUECURVE_THEME_DESC		"The Bluecurve tema."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"The Lighthouseblue tema."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Der blev fundet en ældre version af GTK+ runtime. Ønsker du at opgradere?$\rNB: Gaim virker muligvis ikke uden denne opgradering."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Besøg Windows Gaim's hjemmeside"

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (fjern)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Fejl under installeringen af GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Stien du har angivet kan ikke findes eller oprettes."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Du har ikke tilladelse til at installere et GTK+ tema."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1		"Afinstallationen kunne ikke finde Gaim i registreringsdatabasen.$\rMuligvis har en anden bruger installeret programmet."
!define un.GAIM_UNINSTALL_ERROR_2		"Du har ikke tilladelse til at afinstallere dette program."


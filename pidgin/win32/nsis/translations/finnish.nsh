;;
;;  finish.nsh
;;
;;  Finish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: "Toni_"Daigle"_Impiö" <toni.impio@pp1.inet.fi>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime ympäristö joko puuttuu tai tarvitsee päivitystä.$\rOle hyvä ja asenna v${GTK_MIN_VERSION} tai uudempi GTK+ runtime"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seuraava >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) on julkaistu GPL lisenssin alla. Lisenssi esitetään tässä vain tiedotuksena. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Suoraviestintäohjelma (required)"
!define GTK_SECTION_TITLE			"GTK+ runtime ympäristö (required)"
!define PIDGIN_SECTION_DESCRIPTION		"Pidfinin ytimen tiedostot ja dll:t"
!define GTK_SECTION_DESCRIPTION		"Monipohjainen GUI (käyttäjäulkoasu) työkalupakki, Pidginin käyttämä"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Vanha versio GTK+ runtimestä löytynyt. Tahdotko päivittää?$\rHuomio: $(^Name) ei välttämättä toimi mikäli jätät päivittämättä."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vieraile Pidinin Windows -sivustolla"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Virhe asennettaessa GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Antamasi polku ei toimi tai sitä ei voi luoda."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Asennuksen poistaja ei löytänyt reksiteristä tietoja Pidginista.$\rOn todennäköistä että joku muu käyttäjä on asentanut ohjelman."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sinulla ei ole valtuuksia poistaa ohjelmaa."

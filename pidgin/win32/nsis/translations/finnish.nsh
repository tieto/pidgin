;;
;;  finish.nsh
;;
;;  Finish language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Authors: Toni "Daigle" Impiö <toni.impio@pp1.inet.fi>
;;           Timo Jyrinki <timo.jyrinki@iki.fi>, 2008
;;           
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Ajonaikainen GTK+-ympäristö joko puuttuu tai tarvitsee päivitystä.$\rOle hyvä ja asenna v${GTK_MIN_VERSION} tai uudempi ajonaikainen GTK+-ympäristö."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seuraava >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) on julkaistu GPL-lisenssin alla. Lisenssi esitetään tässä vain tiedotuksena. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin-pikaviestin (vaaditaan)"
!define GTK_SECTION_TITLE			"Ajonaikainen GTK+-ympäristö (vaaditaan)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE 		"Pikakuvakkeet"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE 	"Työpöytä"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Käynnistysvalikko"
!define PIDGIN_SECTION_DESCRIPTION		"Pidginin ytimen tiedostot ja kirjastot"
!define GTK_SECTION_DESCRIPTION		"Monialustainen Pidginin käyttämä käyttöliittymäkirjasto"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   	"Pikakuvakkeet Pidginin käynnistämiseksi"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   		"Tee Pidgin-pikakuvake työpöydälle"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   	"Tee Pidgin-pikakuvake käynnistysvalikkoon"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Vanha versio ajonaikaisesta GTK+-ympäristöstä löytynyt. Tahdotko päivittää?$\rHuomio: $(^Name) ei välttämättä toimi mikäli jätät päivittämättä."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vieraile Pidginin WWW-sivustolla"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Virhe asennettaessa ajonaikaista GTK+-ympäristöä."
!define GTK_BAD_INSTALL_PATH			"Antamasi polku ei toimi tai sitä ei voi luoda."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"URI-käsittelijät"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Asennuksen poistaja ei löytänyt rekisteristä tietoja Pidginista.$\rOn todennäköistä että joku muu käyttäjä on asentanut ohjelman."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Sinulla ei ole valtuuksia poistaa ohjelmaa."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE		"Oikolukutuki"
!define PIDGIN_SPELLCHECK_ERROR			"Virhe asennettaessa oikolukua"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Virhe asennettaessa oikoluvun sanakirjaa"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Tuki oikoluvulle.  (Asennukseen tarvitaan Internet-yhteys)"
!define ASPELL_INSTALL_FAILED			"Asennus epäonnistui"


;;
;;  czech.nsh
;;
;;  Czech language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Jan Kolar <jan@e-kolar.net>
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ runtime buïto chybí, nebo je potøeba provést upgrade.$\rProveïte instalaci verze${GTK_MIN_VERSION} nebo vyšší."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Další >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"K použití $(^Name) se vztahuje GPL licence. Licence je zde uvedena pouze pro Vaší informaci. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin Instant Messaging Client (nutné)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (nutné)"
!define PIDGIN_SECTION_DESCRIPTION		"Základní soubory a DLL pro Pidgin"
!define GTK_SECTION_DESCRIPTION		"Multi-platform GUI toolkit používaný Pidginem"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Byla nalezena starší verze GTK+ runtime. Chcete provést upgrade?$\rUpozornìní: Bez upgradu $(^Name) nemusí pracovat správnì."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Navštívit Windows Pidgin Web Page"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Chyba pøi instalaci GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Zadaná cesta je nedostupná, nebo ji nelze vytvoøit."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Odinstalèní proces nemùže najít záznamy pro Pidgin v registrech.$\rPravdìpodobnì instalaci této aplikace provedl jiný uživatel."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nemáte oprávnìní k odinstalaci této aplikace."

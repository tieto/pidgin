;;
;;  albanian.nsh
;;
;;  Albanian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Version 2
;;  Author: Besnik Bleta <besnik@spymac.com>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"Ose mungon mjedisi GTK+ runtime ose lyp përditësim.$\rJu lutem instaloni GTK+ runtime v${GTK_MIN_VERSION} ose më të vonshëm"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Më tej >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) qarkullon nën licensën GPL. Licensa këtu sillet vetëm për qëllime njoftimi. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Klient Shkëmbimi Mesazhesh të Atypëratyshëm Pidgin (i nevojshëm)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (i nevojshëm)"
!define PIDGIN_SECTION_DESCRIPTION		"Kartela dhe dll bazë të Pidgin-it"
!define GTK_SECTION_DESCRIPTION		"Një grup mjetesh shumëplatformësh për GUI, përdorur nga Pidgin-i"


; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"U gjet një version i vjetër për GTK+ runtime. Doni të përditësohet?$\rShënim: Pidgin-i mund të mos punojë nëse nuk e bëni."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Vizitoni Faqen Web të Pidgin-it për Windows"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"gabim gjatë instalimit të GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Shtegu që treguat nuk mund të arrihet ose krijohet."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Çinstaluesi nuk gjeti dot zëra regjistri për Pidgin-in.$\rKa mundësi që këtë zbatim ta ketë instaluar një tjetër përdorues."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Nuk keni leje të çinstaloni këtë zbatim."

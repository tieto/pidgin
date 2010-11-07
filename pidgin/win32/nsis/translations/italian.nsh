;;
;;  italian.nsh
;;
;;  Italian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Claudio Satriano <satriano@na.infn.it>, 2003.
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"L'ambiente di runtime GTK+ non è presente o deve essere aggiornato.$\rInstallare GTK+ versione ${GTK_MIN_VERSION} o maggiore"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Avanti >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) è distribuito sotto licenza GPL. La licenza è mostrata qui solamente a scopo informativo. $_CLICK" 

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - Client per Messaggi Immediati (richiesto)"
!define GTK_SECTION_TITLE			"Ambiente di Runtime GTK+ (richiesto)"
!define PIDGIN_SECTION_DESCRIPTION		"File principali di Pidgin e dll"
!define GTK_SECTION_DESCRIPTION		"Un toolkit multipiattaforma per interfacce grafiche, usato da Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"È stata trovata una versione precedente di GTK+. Vuoi aggiornarla?$\rNota: $(^Name) potrebbe non funzionare senza l'aggiornamento."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visita la pagina web di Pidgin per Windows"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Errore di installazione di GTK+."
!define GTK_BAD_INSTALL_PATH			"Il percorso scelto non può essere raggiunto o creato."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Il programma di rimozione non è in grado di trovare le voci di registro per Pidgin.$\rProbabilmente un altro utente ha installato questa applicazione."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Non hai il permesso per rimuovere questa applicazione."

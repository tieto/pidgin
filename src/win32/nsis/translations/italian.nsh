;;
;;  italian.nsh
;;
;;  Italian language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Claudio Satriano <satriano@na.infn.it>, 2003.
;;  Version 2
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_ITALIAN} "L'ambiente di runtime GTK+ non è presente o deve essere aggiornato.$\rInstallare GTK+ versione ${GTK_VERSION} o maggiore"

; License Page
LangString GAIM_LICENSE_BUTTON			${LANG_ITALIAN} "Avanti >"
LangString GAIM_LICENSE_BOTTOM_TEXT			${LANG_ITALIAN} "$(^Name) è distribuito sotto licenza GPL. La licenza è mostrata qui solamente a scopo informativo. $_CLICK" 

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_ITALIAN} "Gaim - Client per Messaggi Immediati (richiesto)"
LangString GTK_SECTION_TITLE				${LANG_ITALIAN} "Ambiente di Runtime GTK+ (richiesto)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_ITALIAN} "Temi GTK+"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_ITALIAN} "Nessun Tema"
LangString GTK_WIMP_SECTION_TITLE			${LANG_ITALIAN} "Tema Wimp"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_ITALIAN} "Tema Bluecurve"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_ITALIAN} "Tema Light House Blue"
LangString GAIM_SECTION_DESCRIPTION			${LANG_ITALIAN} "File principali di Gaim e dll"
LangString GTK_SECTION_DESCRIPTION			${LANG_ITALIAN} "Un toolkit multipiattaforma per interfacce grafiche, usato da Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_ITALIAN} "I temi GTK+ modificano l'aspetto delle applicazioni GTK+."
LangString GTK_NO_THEME_DESC				${LANG_ITALIAN} "Non installare nessun tema GTK+"
LangString GTK_WIMP_THEME_DESC			${LANG_ITALIAN} "GTK-Wimp (Windows impersonator) è un tema GTK che si adatta bene all'aspetto di Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_ITALIAN} "Il tema Bluecurve."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_ITALIAN} "Il tema Lighthouseblue."

; GTK+ Directory Page
LangString GTK_UPGRADE_PROMPT				${LANG_ITALIAN} "È stata trovata una versione precedente di GTK+. Vuoi aggiornarla?$\rNota: Gaim potrebbe non funzionare senza l'aggiornamento."

; Installer Finish Page
LangString GAIM_FINISH_VISIT_WEB_SITE		${LANG_ITALIAN} "Visita la pagina web di Gaim per Windows" 

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_ITALIAN} "Gaim (solo rimozione)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_ITALIAN} "La tua vecchia directory di Gaim sta per essere cancellata. Vuoi andare avanti?$\r$\rNota: Tutti i plugin non standard che hai installato saranno cancellati.$\rLe impostazioni di Gaim non saranno rimosse."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_ITALIAN} "La directory di installazione specificata esiste già. Tutto il contenuto$\rverrà cancellato. Vuoi continuare?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_ITALIAN} "Errore di installazione di GTK+."
LangString GTK_BAD_INSTALL_PATH			${LANG_ITALIAN} "Il percorso scelto non può essere raggiunto o creato."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_ITALIAN} "Non hai il permesso per installare un tema GTK+."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_ITALIAN} "Il programma di rimozione non è in grado di trovare le voci di registro per Gaim.$\rProbabilmente un altro utente ha installato questa applicazione."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_ITALIAN} "Non hai il permesso per rimuovere questa applicazione."

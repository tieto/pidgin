;;
;;  italian.nsh
;;
;;  Italian language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Claudio Satriano <satriano@na.infn.it>, 2003-2009.
;;  Version 3
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"Il programma di installazione è già in esecuzione"
!define PIDGIN_IS_RUNNING			"È attualmente in esecuzione un'istanza di Pidgin. Esci da Pidgin e riprova."
!define GTK_INSTALLER_NEEDED			"L'ambiente di runtime GTK+ non è presente o deve essere aggiornato.$\rInstallare GTK+ versione ${GTK_MIN_VERSION} o maggiore"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Avanti >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) è distribuito sotto la GNU General Public License (GPL). La licenza è mostrata qui solamente a scopo informativo. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin - Client per Messaggi Immediati (richiesto)"
!define GTK_SECTION_TITLE			"Ambiente di Runtime GTK+ (richiesto se non presente)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE		"Collegamenti"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE	"Desktop"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE	"Menu Avvio"
!define PIDGIN_SECTION_DESCRIPTION		"File principali di Pidgin e dll"
!define GTK_SECTION_DESCRIPTION		"Un toolkit multipiattaforma per interfacce grafiche, usato da Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION	"Collegamenti per avviare Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC		"Crea un collegamento a Pidgin sul desktop"
!define PIDGIN_STARTMENU_SHORTCUT_DESC		"Crea una voce per Pidgin nel Menu Avvio"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"È stata trovata una versione precedente di GTK+. Vuoi aggiornarla?$\rNota: $(^Name) potrebbe non funzionare senza l'aggiornamento."
!define GTK_WINDOWS_INCOMPATIBLE		"Windows 95/98/Me non è incompatible con GTK+ 2.8.0 o successivo. GTK+ ${GTK_INSTALL_VERSION} non sarà installato.$\rSe non hai GTK+ ${GTK_MIN_VERSION} o successivo già installato sul tuo computer, questa installazione sarà interrotta."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visita la pagina web di Pidgin"

; Pidgin Section Prompts and Texts
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL	"Impossibile rimuovere la versione di Pidgin attualmente presente sul tuo computer. La nuova versione sarà installata senza rimuovere la versione precedente."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Error nell'installazione del runtime GTK+."
!define GTK_BAD_INSTALL_PATH			"Il percorso scelto non può essere raggiunto o creato."

; URL Handler section
!define URI_HANDLERS_SECTION_TITLE		"Gestori degli URI"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Il programma di rimozione non è in grado di trovare le voci di registro per Pidgin.$\rProbabilmente questa applicazione è stata installata da un altro utente."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Non hai il permesso per rimuovere questa applicazione."

; Spellcheck Section Prompts
!define PIDGIN_SPELLCHECK_SECTION_TITLE	"Supporto per il correttore ortografico"
!define PIDGIN_SPELLCHECK_ERROR		"Errore nell'installazione del correttore ortografico"
!define PIDGIN_SPELLCHECK_DICT_ERROR		"Errore nell'installazione del dizionario per il correttore ortografico"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION	"Supporto per il correttore ortografico.  (È richiesta una connessione a internet per l'installazione)"
!define ASPELL_INSTALL_FAILED			"Installazione fallita"
!define PIDGIN_SPELLCHECK_BRETON		"Bretone"
!define PIDGIN_SPELLCHECK_CATALAN		"Catalano"
!define PIDGIN_SPELLCHECK_CZECH		"Ceco"
!define PIDGIN_SPELLCHECK_WELSH		"Gallese"
!define PIDGIN_SPELLCHECK_DANISH		"Danese"
!define PIDGIN_SPELLCHECK_GERMAN		"Tedesco"
!define PIDGIN_SPELLCHECK_GREEK		"Greco"
!define PIDGIN_SPELLCHECK_ENGLISH		"Inglese"
!define PIDGIN_SPELLCHECK_ESPERANTO		"Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH		"Spagnolo"
!define PIDGIN_SPELLCHECK_FAROESE		"Faroese"
!define PIDGIN_SPELLCHECK_FRENCH		"Francese"
!define PIDGIN_SPELLCHECK_ITALIAN		"Italiano"
!define PIDGIN_SPELLCHECK_DUTCH		"Olandese"
!define PIDGIN_SPELLCHECK_NORWEGIAN		"Norvegese"
!define PIDGIN_SPELLCHECK_POLISH		"Polacco"
!define PIDGIN_SPELLCHECK_PORTUGUESE		"Portoghese"
!define PIDGIN_SPELLCHECK_ROMANIAN		"Rumeno"
!define PIDGIN_SPELLCHECK_RUSSIAN		"Russo"
!define PIDGIN_SPELLCHECK_SLOVAK		"Slovacco"
!define PIDGIN_SPELLCHECK_SWEDISH		"Svedese"
!define PIDGIN_SPELLCHECK_UKRAINIAN		"Ucraino"


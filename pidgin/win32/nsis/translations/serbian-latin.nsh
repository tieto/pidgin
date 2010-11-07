;;
;;  serbian-latin.nsh
;;
;;  Serbian (Latin) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Danilo Segan <dsegan@gmx.net>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ okolina za izvršavanje ili nije naðena ili se moraunaprediti.$\rMolimo instalirajte v${GTK_MIN_VERSION} ili veæu GTK+ okoline za izvršavanje"

; Components Page
!define PIDGIN_SECTION_TITLE			"Pidgin klijent za brze poruke (neophodno)"
!define GTK_SECTION_TITLE			"GTK+ okolina za izvršavanje (neophodno)"
!define PIDGIN_SECTION_DESCRIPTION		"Osnovne Pidgin datoteke i dinamièke biblioteke"
!define GTK_SECTION_DESCRIPTION		"Skup oruða za grafièko okruženje, za više platformi, koristi ga Pidgin "

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Naðena je stara verzija GTK+ izvršne okoline. Da li želite da je unapredite?$\rPrimedba: Ukoliko to ne uradite, $(^Name) možda neæe raditi."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Greška prilikom instalacije GTK+ okoline za izvršavanje."
!define GTK_BAD_INSTALL_PATH			"Putanja koju ste naveli se ne može ni napraviti niti joj se može priæi."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "Program za uklanjanje instalacije ne može da pronaðe stavke registra za Pidgin.$\rVerovatno je ovu aplikaciju instalirao drugi korisnik."
!define un.PIDGIN_UNINSTALL_ERROR_2         "Nemate ovlašæenja za deinstalaciju ove aplikacije."

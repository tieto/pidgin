;;
;;  serbian-latin.nsh
;;
;;  Serbian (Latin) language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1250
;;
;;  Author: Danilo Segan <dsegan@gmx.net>
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_SERBIAN} "GTK+ okolina za izvršavanje ili nije naðena ili se moraunaprediti.$\rMolimo instalirajte v${GTK_VERSION} ili veæu GTK+ okoline za izvršavanje"

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_SERBIAN} "Gaim klijent za brze poruke (neophodno)"
LangString GTK_SECTION_TITLE				${LANG_SERBIAN} "GTK+ okolina za izvršavanje (neophodno)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_SERBIAN} "GTK+ teme"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_SERBIAN} "Bez teme"
LangString GTK_WIMP_SECTION_TITLE			${LANG_SERBIAN} "Wimp tema"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_SERBIAN} "Bluecurve tema"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_SERBIAN} "Light House Blue tema"
LangString GAIM_SECTION_DESCRIPTION			${LANG_SERBIAN} "Osnovne Gaim datoteke i dinamièke biblioteke"
LangString GTK_SECTION_DESCRIPTION			${LANG_SERBIAN} "Skup oruða za grafièko okruženje, za više platformi, koristi ga Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_SERBIAN} "GTK+ teme menjaju izgled i naèin rada GTK+ aplikacija."
LangString GTK_NO_THEME_DESC				${LANG_SERBIAN} "Ne instaliraj GTK+ temu"
LangString GTK_WIMP_THEME_DESC			${LANG_SERBIAN} "GTK-Wimp (Windows imitator) je GTK tema koja se dobro uklapa u Windows radno okruženje."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_SERBIAN} "Bluecurve tema."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_SERBIAN} "Lighthouseblue tema."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_SERBIAN} "Izaberite mesto gde æe se instalirati program"
LangString GTK_PAGE_SUBTITLE				${LANG_SERBIAN} "Izaberite folder u koji želite da se GTK+ instalira"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_SERBIAN} "Program æe instalirati GTK+ u ovaj folder"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_SERBIAN} "Da biste instalirali u neki drugi folder, kliknite na Pregled i izaberite neki drugi folder. Kliknite na Nastavak da bi produžili dalje."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_SERBIAN} "Program æe unaprediti GTK+ koji je naðen u ovom folderu"
LangString GTK_UPGRADE_PROMPT				${LANG_SERBIAN} "Naðena je stara verzija GTK+ izvršne okoline. Da li želite da je unapredite?$\rPrimedba: Ukoliko to ne uradite, Gaim možda neæe raditi."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_SERBIAN} "Gaim (samo uklanjanje)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_SERBIAN} "Vaš stari Gaim direktorijum æe biti obrisan. Da li želite da nastavite?$\r$\rPrimedba: Svi nestandardni dodaci koje ste instalirali æe biti obrisani.$\rGaim postavke korisnika neæe biti promenjene."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_SERBIAN} "Instalacioni direktorijum koji ste naveli veæ postoji. Sav sadržaj$\ræe biti obrisan. Da li želite da nastavite?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_SERBIAN} "Greška prilikom instalacije GTK+ okoline za izvršavanje."
LangString GTK_BAD_INSTALL_PATH			${LANG_SERBIAN} "Putanja koju ste naveli se ne može ni napraviti niti joj se može priæi."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_SERBIAN} "Nemate ovlašæenja za instalaciju GTK+ teme."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_SERBIAN} "Program za uklanjanje instalacije ne može da pronaðe stavke registra za Gaim.$\rVerovatno je ovu aplikaciju instalirao drugi korisnik."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_SERBIAN} "Nemate ovlašæenja za deinstalaciju ove aplikacije."

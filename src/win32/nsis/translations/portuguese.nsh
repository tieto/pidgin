;;
;;  portuguese.nsh
;;
;;  Portuguese (PT) language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Duarte Serrano Gonçalves Henriques <duarte_henriques@myrealbox.com>, 2003.
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_PORTUGUESE} "O ambiente de tempo de execução do GTK+ está ausente ou precisa de ser actualizado.$\rPor favor instale a versão v${GTK_VERSION} ou superior do ambiente de tempo de execução do GTK+."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_PORTUGUESE} "Cliente de mensagens instantâneas Gaim (obrigatório)"
LangString GTK_SECTION_TITLE				${LANG_PORTUGUESE} "Ambiente de tempo de execução do GTK+ (obrigatório)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_PORTUGUESE} "Temas do GTK+"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_PORTUGUESE} "Nenhum tema"
LangString GTK_WIMP_SECTION_TITLE			${LANG_PORTUGUESE} "Tema 'Wimp'"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_PORTUGUESE} "Tema 'Bluecurve'"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_PORTUGUESE} "Tema 'Light House Blue'"
LangString GAIM_SECTION_DESCRIPTION			${LANG_PORTUGUESE} "Ficheiros e bibliotecas principais do Gaim"
LangString GTK_SECTION_DESCRIPTION			${LANG_PORTUGUESE} "Um conjunto de ferramentas de interface gráfica multi-plataforma, usado pelo Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_PORTUGUESE} "Os temas do GTK+ podem mudar a aparência dos programas GTK+."
LangString GTK_NO_THEME_DESC				${LANG_PORTUGUESE} "Não instalar um tema do GTK+"
LangString GTK_WIMP_THEME_DESC			${LANG_PORTUGUESE} "O tema 'GTK-Wimp' ('Windows impersonator', personificador do Windows) é um tema GTK que combina bem com o ambiente de trabalho do Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_PORTUGUESE} "O tema 'Bluecurve'."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_PORTUGUESE} "O tema 'Lighthouseblue'."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_PORTUGUESE} "Escolha o local da instalação"
LangString GTK_PAGE_SUBTITLE				${LANG_PORTUGUESE} "Escolha a pasta em que o GTK+ será instalado"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_PORTUGUESE} "O instalador irá instalar o GTK+ na seguinte pasta"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_PORTUGUESE} "Para instalar numa pasta diferente, clique em Procurar e seleccione outra pasta. Clique em Próximo para continuar."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_PORTUGUESE} "O instalador irá actualizar o GTK+ encontrado na seguinte pasta"
LangString GTK_UPGRADE_PROMPT				${LANG_PORTUGUESE} "Foi encontrada ma versão antiga do ambiente de tempo de execução do GTK+. Deseja actualizá-lo?$\rNota: O Gaim poderá não funcionar se não o fizer."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_PORTUGUESE} "Gaim (remover apenas)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_PORTUGUESE} "A sua antiga instalação do Gaim está prestes a ser removida. Deseja continuar?$\r$\rNota: Quaisquer plugins não-padrão que poderá ter instalado serão removidos.$\rAs configurações de utilizador do Gaim não serão afectadas."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_PORTUGUESE} "A directoria de instalação do que especificou já existe. Qualquer conteúdo$\rserá apagado. Deseja continuar?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_PORTUGUESE} "Erro ao instalar o ambiente de tempo de execução do GTK+."
LangString GTK_BAD_INSTALL_PATH			${LANG_PORTUGUESE} "Impossível aceder ou criar o caminho que digitou."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_PORTUGUESE} "Não tem permissão para instalar um tema do GTK+."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_PORTUGUESE} "O desinstalador não pôde encontrar entradas de registo do Gaim.$\rÉ provável que outro utilizador tenha instalado este programa."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_PORTUGUESE} "Não tem permissão para desinstalar este programa."

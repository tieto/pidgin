;;
;;  brazilian.nsh
;;
;;  Portuguese (Brazilian) language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Maurício de Lemos Rodrigues Collares Neto <mauricioc@myrealbox.com>, 2003.
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_BRAZILIAN} "O ambiente de tempo de execução do GTK+ está ausente ou precisa ser atualizado.$\rFavor instalar a versão v${GTK_VERSION} ou superior do ambiente de tempo de execução do GTK+."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_BRAZILIAN} "Cliente de mensagens instantâneas Gaim (requerido)"
LangString GTK_SECTION_TITLE				${LANG_BRAZILIAN} "Ambiente de tempo de execução do GTK+ (requerido)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_BRAZILIAN} "Temas do GTK+"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_BRAZILIAN} "Nenhum tema"
LangString GTK_WIMP_SECTION_TITLE			${LANG_BRAZILIAN} "Tema 'Wimp'"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_BRAZILIAN} "Tema 'Bluecurve'"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_BRAZILIAN} "Tema 'Light House Blue'"
LangString GAIM_SECTION_DESCRIPTION			${LANG_BRAZILIAN} "Arquivos e bibliotecas principais do Gaim"
LangString GTK_SECTION_DESCRIPTION			${LANG_BRAZILIAN} "Um conjunto de ferramentas multi-plataforma para interface do usuário, usado pelo Gaim"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_BRAZILIAN} "Os temas do GTK+ podem mudar a aparência e o funcionamento dos aplicativos GTK+."
LangString GTK_NO_THEME_DESC				${LANG_BRAZILIAN} "Não instalar um tema do GTK+"
LangString GTK_WIMP_THEME_DESC			${LANG_BRAZILIAN} "O tema 'GTK-Wimp' ('Windows impersonator', personificador do Windows) é um tema GTK que combina bem com o ambiente de área de trabalho do Windows."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_BRAZILIAN} "O tema 'Bluecurve'."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_BRAZILIAN} "O tema 'Lighthouseblue'."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_BRAZILIAN} "Escolha o local da instalação"
LangString GTK_PAGE_SUBTITLE				${LANG_BRAZILIAN} "Escolha a pasta em que o GTK+ será instalado"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_BRAZILIAN} "O instalador irá instalar o GTK+ na seguinte pasta"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_BRAZILIAN} "Para instalar numa pasta diferente, clique em Procurar e selecione outra pasta. Aperte Próximo para continuar."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_BRAZILIAN} "O instalador irá atualizar o GTK+ encontrado na seguinte pasta"
LangString GTK_UPGRADE_PROMPT				${LANG_BRAZILIAN} "Uma versão antiga do ambiente de tempo de execução do GTK+ foi encontrada. Você deseja atualizá-lo?$\rNota: O Gaim poderá não funcionar a menos que você o faça."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_BRAZILIAN} "Gaim (apenas remover)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_BRAZILIAN} "Sua antiga instalação do Gaim está prestes a ser removida. Você gostaria de continuar?$\r$\rNota: Quaisquer plugins não-padrão que você pode ter instalado serão removidos.$\rAs configurações de usuário do Gaim não serão afetadas."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_BRAZILIAN} "O diretório de instalação do que você especificou já existe. Qualquer conteúdo$\rserá deletado. Deseja continuar?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_BRAZILIAN} "Erro ao instalar o ambiente de tempo de execução do GTK+."
LangString GTK_BAD_INSTALL_PATH			${LANG_BRAZILIAN} "O caminho que você digitou não pôde ser acessado ou criado."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_BRAZILIAN} "Você não tem permissão para instalar um tema do GTK+."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_BRAZILIAN} "O desinstalador não pôde encontrar entradas de registro do Gaim.$\rÉ provável que outro usuário tenha instalado esta aplicação."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_BRAZILIAN} "Você não tem permissão para desinstalar essa aplicação."

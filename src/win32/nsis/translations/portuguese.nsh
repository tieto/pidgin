;;
;;  portuguese.nsh
;;
;;  Portuguese (PT) language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Duarte Serrano Gonçalves Henriques <duarte_henriques@myrealbox.com>, 2003.
;;  Version 2
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"O ambiente de tempo de execução do GTK+ está ausente ou precisa de ser actualizado.$\rPor favor instale a versão v${GTK_VERSION} ou superior do ambiente de tempo de execução do GTK+."

; License Page
!define GAIM_LICENSE_BUTTON			"Seguinte >"
!define GAIM_LICENSE_BOTTOM_TEXT		"$(^Name) está disponível sob alicença GPL. O texto da licença é fornecido meramente a título informativo. $_CLICK" 

; Components Page
!define GAIM_SECTION_TITLE			"Cliente de mensagens instantâneas Gaim (obrigatório)"
!define GTK_SECTION_TITLE			"Ambiente de tempo de execução do GTK+ (obrigatório)"
!define GTK_THEMES_SECTION_TITLE		"Temas do GTK+"
!define GTK_NOTHEME_SECTION_TITLE		"Nenhum tema"
!define GTK_WIMP_SECTION_TITLE		"Tema 'Wimp'"
!define GTK_BLUECURVE_SECTION_TITLE		"Tema 'Bluecurve'"
!define GTK_LIGHTHOUSEBLUE_SECTION_TITLE	"Tema 'Light House Blue'"
!define GAIM_SECTION_DESCRIPTION		"Ficheiros e bibliotecas principais do Gaim"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas de interface gráfica multi-plataforma, usado pelo Gaim"
!define GTK_THEMES_SECTION_DESCRIPTION	"Os temas do GTK+ podem mudar a aparência dos programas GTK+."
!define GTK_NO_THEME_DESC			"Não instalar um tema do GTK+"
!define GTK_WIMP_THEME_DESC			"O tema 'GTK-Wimp' ('Windows impersonator', personificador do Windows) é um tema GTK+ que combina bem com o ambiente de trabalho do Windows."
!define GTK_BLUECURVE_THEME_DESC		"O tema 'Bluecurve'."
!define GTK_LIGHTHOUSEBLUE_THEME_DESC	"O tema 'Lighthouseblue'."

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Foi encontrada ma versão antiga do ambiente de tempo de execução do GTK+. Deseja actualizá-lo?$\rNota: O Gaim poderá não funcionar se não o fizer."

; Installer Finish Page
!define GAIM_FINISH_VISIT_WEB_SITE		"Visite a página web do Gaim para Windows" 

; Gaim Section Prompts and Texts
!define GAIM_UNINSTALL_DESC			"Gaim (remover apenas)"
!define GAIM_PROMPT_WIPEOUT			"A sua antiga instalação do Gaim está prestes a ser removida. Deseja continuar?$\r$\rNota: Quaisquer plugins não-padrão que poderá ter instalado serão removidos.$\rAs configurações de utilizador do Gaim não serão afectadas."
!define GAIM_PROMPT_DIR_EXISTS		"A directoria de instalação do que especificou já existe. Qualquer conteúdo$\rserá apagado. Deseja continuar?"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de tempo de execução do GTK+."
!define GTK_BAD_INSTALL_PATH			"Impossível aceder ou criar o caminho que digitou."

; GTK+ Themes section
!define GTK_NO_THEME_INSTALL_RIGHTS		"Não tem permissão para instalar um tema do GTK+."

; Uninstall Section Prompts
!define un.GAIM_UNINSTALL_ERROR_1         "O desinstalador não pôde encontrar entradas de registo do Gaim.$\rÉ provável que outro utilizador tenha instalado este programa."
!define un.GAIM_UNINSTALL_ERROR_2         "Não tem permissão para desinstalar este programa."

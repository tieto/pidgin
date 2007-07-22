;;
;;  portuguese-br.nsh
;;
;;  Portuguese (BR) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Maurício de Lemos Rodrigues Collares Neto <mauricioc@myrealbox.com>, 2003-2005.
;;  Version 3
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"O ambiente de tempo de execução do GTK+ está ausente ou precisa ser atualizado.$\rFavor instalar a versão v${GTK_MIN_VERSION} ou superior do ambiente de tempo de execução do GTK+."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Avançar >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) é distribuído sob a licença GPL. Esta licença é disponibilizada aqui apenas para fins informativos. $_CLICK" 

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de mensagens instantâneas Pidgin (requerido)"
!define GTK_SECTION_TITLE			"Ambiente de tempo de execução do GTK+ (requerido)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Atalhos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Área de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menu Iniciar"
!define PIDGIN_SECTION_DESCRIPTION		"Arquivos e bibliotecas principais do Pidgin"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas multi-plataforma para interface do usuário, usado pelo Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Crie um atalho para o Pidgin na Área de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Crie uma entrada no Menu Iniciar para o Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Uma versão antiga do ambiente de tempo de execução do GTK+ foi encontrada. Você deseja atualizá-lo?$\rNota: O $(^Name) poderá não funcionar a menos que você o faça."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de tempo de execução do GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que você digitou não pôde ser acessado ou criado."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a página da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador não pôde encontrar entradas de registro do Pidgin.$\rÉ provável que outro usuário tenha instalado esta aplicação."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Você não tem permissão para desinstalar essa aplicação."

!define INSTALLER_IS_RUNNING                   "O instalador já está em execução."
!define PIDGIN_IS_RUNNING                      "Uma instância do Pidgin está em execução. Feche o Pidgin e tente novamente."
!define GTK_WINDOWS_INCOMPATIBLE               "O Windows 95/98/Me é incompatível com o GTK+ 2.8.0 ou superior. O GTK+ ${GTK_INSTALL_VERSION} não será instalado.$\rSe você não possuir o GTK+ versão ${GTK_MIN_VERSION} ou superior já instalado, o instalador irá fechar agora."
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL       "Não foi possível desinstalar a versão do Pidgin que está instalada atualmente. A nova versão será instalada sem que a versão antiga seja removida."
!define URI_HANDLERS_SECTION_TITLE             "Handlers para endereços"
!define PIDGIN_SPELLCHECK_SECTION_TITLE        "Suporte a verificação ortográfica"
!define PIDGIN_SPELLCHECK_ERROR                "Erro ao instalar a verificação ortográfica"
!define PIDGIN_SPELLCHECK_DICT_ERROR           "Erro ao instalar o dicionário da verificação ortográfica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION  "Suporte a verificação ortográfica (A instalação necessita de conexão a internet)"
!define ASPELL_INSTALL_FAILED                  "Falha na instalação"
!define PIDGIN_SPELLCHECK_BRETON               "Bretão"
!define PIDGIN_SPELLCHECK_CATALAN              "Catalão"
!define PIDGIN_SPELLCHECK_CZECH                "Tcheco"
!define PIDGIN_SPELLCHECK_WELSH                "Galês" 
!define PIDGIN_SPELLCHECK_DANISH               "Dinamarquês"
!define PIDGIN_SPELLCHECK_GERMAN               "Alemão" 
!define PIDGIN_SPELLCHECK_GREEK                "Grego"
!define PIDGIN_SPELLCHECK_ENGLISH              "Inglês"
!define PIDGIN_SPELLCHECK_ESPERANTO            "Esperanto"
!define PIDGIN_SPELLCHECK_SPANISH              "Espanhol"
!define PIDGIN_SPELLCHECK_FAROESE              "Feroês"
!define PIDGIN_SPELLCHECK_FRENCH               "Francês"
!define PIDGIN_SPELLCHECK_ITALIAN              "Italiano"
!define PIDGIN_SPELLCHECK_DUTCH                "Holandês"
!define PIDGIN_SPELLCHECK_NORWEGIAN            "Norueguês" 
!define PIDGIN_SPELLCHECK_POLISH               "Polonês"
!define PIDGIN_SPELLCHECK_PORTUGUESE           "Português"
!define PIDGIN_SPELLCHECK_ROMANIAN             "Romeno"
!define PIDGIN_SPELLCHECK_RUSSIAN              "Russo" 
!define PIDGIN_SPELLCHECK_SLOVAK               "Eslovaco"
!define PIDGIN_SPELLCHECK_SWEDISH              "Sueco"
!define PIDGIN_SPELLCHECK_UKRAINIAN            "Ucraniano"

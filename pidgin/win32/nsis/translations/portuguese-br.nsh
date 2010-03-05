;;
;;  portuguese-br.nsh
;;
;;  Portuguese (BR) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Maurício de Lemos Rodrigues Collares Neto <mauricioc@myrealbox.com>, 2003-2005.
;;  Version 3
;;


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

; GTK+ Section Prompts

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a página da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador não pôde encontrar entradas de registro do Pidgin.$\rÉ provável que outro usuário tenha instalado esta aplicação."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Você não tem permissão para desinstalar essa aplicação."

!define INSTALLER_IS_RUNNING                   "O instalador já está em execução."
!define PIDGIN_IS_RUNNING                      "Uma instância do Pidgin está em execução. Feche o Pidgin e tente novamente."
!define PIDGIN_PROMPT_CONTINUE_WITHOUT_UNINSTALL       "Não foi possível desinstalar a versão do Pidgin que está instalada atualmente. A nova versão será instalada sem que a versão antiga seja removida."
!define URI_HANDLERS_SECTION_TITLE             "Handlers para endereços"
!define PIDGIN_SPELLCHECK_SECTION_TITLE        "Suporte a verificação ortográfica"
!define PIDGIN_SPELLCHECK_ERROR                "Erro ao instalar a verificação ortográfica"
!define PIDGIN_SPELLCHECK_SECTION_DESCRIPTION  "Suporte a verificação ortográfica (A instalação necessita de conexão a internet)"

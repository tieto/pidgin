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

; Pidgin Section Prompts and Texts
!define PIDGIN_UNINSTALL_DESC			"$(^Name) (apenas remover)"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Erro ao instalar o ambiente de tempo de execução do GTK+."
!define GTK_BAD_INSTALL_PATH			"O caminho que você digitou não pôde ser acessado ou criado."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a página da web do Pidgin para Windows"

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador não pôde encontrar entradas de registro do Pidgin.$\rÉ provável que outro usuário tenha instalado esta aplicação."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Você não tem permissão para desinstalar essa aplicação."

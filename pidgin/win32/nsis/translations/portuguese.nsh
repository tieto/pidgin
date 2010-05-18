;;
;;  portuguese.nsh
;;
;;  Portuguese (PT) language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1252
;;
;;  Author: Duarte Henriques <duarte.henriques@gmail.com>, 2003-2005.
;;  Version 3
;;

; Startup Checks
!define INSTALLER_IS_RUNNING			"O instalador já está a ser executado."
!define PIDGIN_IS_RUNNING			"Uma instância do Pidgin já está a ser executada. Saia do Pidgin e tente de novo."

; License Page
!define PIDGIN_LICENSE_BUTTON			"Seguinte >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) está disponível sob a licença GNU General Public License (GPL). O texto da licença é fornecido aqui meramente a título informativo. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Cliente de Mensagens Instantâneas Pidgin (obrigatório)"
!define GTK_SECTION_TITLE			"Ambiente de Execução GTK+ (obrigatório)"
!define PIDGIN_SHORTCUTS_SECTION_TITLE "Atalhos"
!define PIDGIN_DESKTOP_SHORTCUT_SECTION_TITLE "Ambiente de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_SECTION_TITLE "Menu de Iniciar"
!define PIDGIN_SECTION_DESCRIPTION		"Ficheiros e bibliotecas principais do Pidgin"
!define GTK_SECTION_DESCRIPTION		"Um conjunto de ferramentas de interface gráfica multi-plataforma, usado pelo Pidgin"

!define PIDGIN_SHORTCUTS_SECTION_DESCRIPTION   "Atalhos para iniciar o Pidgin"
!define PIDGIN_DESKTOP_SHORTCUT_DESC   "Criar um atalho para o Pidgin no Ambiente de Trabalho"
!define PIDGIN_STARTMENU_SHORTCUT_DESC   "Criar uma entrada para o Pidgin na Barra de Iniciar"

; GTK+ Directory Page

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Visite a Página Web do Pidgin para Windows"

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"O desinstalador não encontrou entradas de registo do Pidgin.$\rÉ provável que outro utilizador tenha instalado este programa."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Não tem permissão para desinstalar este programa."

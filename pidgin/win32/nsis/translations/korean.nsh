;;
;;  korean.nsh
;;
;;  Korean language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 949
;;
;;  Author: Kyung-uk Son <vvs740@chol.com>
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"GTK+ 런타임 환경에 문제가 있거나 업그레이드가 필요합니다.$\rGTK+ 런타임 환경을 v${GTK_MIN_VERSION}이나 그 이상 버전으로 설치해주세요."

; Components Page
!define PIDGIN_SECTION_TITLE			"가임 메신저 (필수)"
!define GTK_SECTION_TITLE			"GTK+ 런타임 환경 (필수)"
!define PIDGIN_SECTION_DESCRIPTION		"가임의 코어 파일과 dll"
!define GTK_SECTION_DESCRIPTION		"가임이 사용하는 멀티 플랫폼 GUI 툴킷"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"옛날 버전 GTK+ 런타임을 찾았습니다. 업그레이드할까요?$\rNote: 업그레이드하지 않으면 가임이 동작하지 않을 수도 있습니다."

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"GTK+ 런타임 설치 중 오류 발생."
!define GTK_BAD_INSTALL_PATH			"입력하신 경로에 접근할 수 없거나 만들 수 없었습니다."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "언인스톨러가 가임의 레지스트리 엔트리를 찾을 수 없습니다.$\r이 프로그램을 다른 유저 권한으로 설치한 것 같습니다."
!define un.PIDGIN_UNINSTALL_ERROR_2         "이 프로그램을 제거할 수 있는 권한이 없습니다."

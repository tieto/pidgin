;;
;;  korean.nsh
;;
;;  Korean language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 949
;;
;;  Author: Kyung-uk Son <vvs740@chol.com>
;;


; Components Page
!define PIDGIN_SECTION_TITLE			"가임 메신저 (필수)"
!define GTK_SECTION_TITLE			"GTK+ 런타임 환경 (필수)"
!define PIDGIN_SECTION_DESCRIPTION		"가임의 코어 파일과 dll"
!define GTK_SECTION_DESCRIPTION		"가임이 사용하는 멀티 플랫폼 GUI 툴킷"

; GTK+ Directory Page

; GTK+ Section Prompts

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1         "언인스톨러가 가임의 레지스트리 엔트리를 찾을 수 없습니다.$\r이 프로그램을 다른 유저 권한으로 설치한 것 같습니다."
!define un.PIDGIN_UNINSTALL_ERROR_2         "이 프로그램을 제거할 수 있는 권한이 없습니다."

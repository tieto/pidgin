;;
;;  korean.nsh
;;
;;  Korean language strings for the Windows Gaim NSIS installer.
;;  Windows Code page: 949
;;
;;  Author: Kyung-uk Son <vvs740@chol.com>
;;

; Startup GTK+ check
LangString GTK_INSTALLER_NEEDED			${LANG_KOREAN} "GTK+ 런타임 환경에 문제가 있거나 업그레이드가 필요합니다.$\rGTK+ 런타임 환경을 v${GTK_VERSION}이나 그 이상 버전으로 설치해주세요."

; Components Page
LangString GAIM_SECTION_TITLE				${LANG_KOREAN} "가임 메신저 (필수)"
LangString GTK_SECTION_TITLE				${LANG_KOREAN} "GTK+ 런타임 환경 (필수)"
LangString GTK_THEMES_SECTION_TITLE			${LANG_KOREAN} "GTK+ 테마"
LangString GTK_NOTHEME_SECTION_TITLE		${LANG_KOREAN} "테마 없음"
LangString GTK_WIMP_SECTION_TITLE			${LANG_KOREAN} "윔프 테마"
LangString GTK_BLUECURVE_SECTION_TITLE		${LANG_KOREAN} "블루커브 테마"
LangString GTK_LIGHTHOUSEBLUE_SECTION_TITLE	${LANG_KOREAN} "Light House Blue 테마"
LangString GAIM_SECTION_DESCRIPTION			${LANG_KOREAN} "가임의 코어 파일과 dll"
LangString GTK_SECTION_DESCRIPTION			${LANG_KOREAN} "가임이 사용하는 멀티 플랫폼 GUI 툴킷"
LangString GTK_THEMES_SECTION_DESCRIPTION		${LANG_KOREAN} "GTK+ 테마는 GTK+ 프로그램의 룩앤필을 바꿀 수 있습니다."
LangString GTK_NO_THEME_DESC				${LANG_KOREAN} "GTK+ 테마를 설치하지 않습니다."
LangString GTK_WIMP_THEME_DESC			${LANG_KOREAN} "GTK-Wimp (Windows impersonator)는 윈도우 데스크탑 환경에 잘 조화되는 GTK 테마입니다."
LangString GTK_BLUECURVE_THEME_DESC			${LANG_KOREAN} "블루커브 테마."
LangString GTK_LIGHTHOUSEBLUE_THEME_DESC		${LANG_KOREAN} "The Lighthouseblue theme."

; Extra GTK+ Dir Selector Page
LangString GTK_PAGE_TITLE				${LANG_KOREAN} "설치 위치 선택"
LangString GTK_PAGE_SUBTITLE				${LANG_KOREAN} "GTK+를 설치할 폴더를 선택"
LangString GTK_PAGE_INSTALL_MSG1			${LANG_KOREAN} "설치 프로그램이 아래 폴더에 가임을 설치합니다"
LangString GTK_PAGE_INSTALL_MSG2			${LANG_KOREAN} "다른 폴더에 설치하시려면 탐색을 클릭하고 다른 폴더를 선택하세요. 설치를 클릭하면 설치를 실행합니다."
LangString GTK_PAGE_UPGRADE_MSG1			${LANG_KOREAN} "설치 프로그램이 아래 폴더에 GTK+를 업그레이드합니다"
LangString GTK_UPGRADE_PROMPT				${LANG_KOREAN} "옛날 버전 GTK+ 런타임을 찾았습니다. 업그레이드할까요?$\rNote: 업그레이드하지 않으면 가임이 동작하지 않을 수도 있습니다."

; Gaim Section Prompts and Texts
LangString GAIM_UNINSTALL_DESC			${LANG_KOREAN} "Gaim (remove only)"
LangString GAIM_PROMPT_WIPEOUT			${LANG_KOREAN} "가임 디렉토리가 지워질 것입니다. 계속 할까요?$\r$\rNote: 비표준 플러그인은 지워지지 않을 수도 있습니다.$\r가임 사용자 설정에는 영향을 미치지 않습니다."
LangString GAIM_PROMPT_DIR_EXISTS			${LANG_KOREAN} "입력하신 설치 디렉토리가 이미 있습니다. 안에 들은 내용이 지워질 수도 있습니다. 계속할까요?"

; GTK+ Section Prompts
LangString GTK_INSTALL_ERROR				${LANG_KOREAN} "GTK+ 런타임 설치 중 오류 발생."
LangString GTK_BAD_INSTALL_PATH			${LANG_KOREAN} "입력하신 경로에 접근할 수 없거나 만들 수 없었습니다."

; GTK+ Themes section
LangString GTK_NO_THEME_INSTALL_RIGHTS		${LANG_KOREAN} "GTK+ 테마를 설치할 권한이 없습니다."

; Uninstall Section Prompts
LangString un.GAIM_UNINSTALL_ERROR_1         	${LANG_KOREAN} "언인스톨러가 가임의 레지스트리 엔트리를 찾을 수 없습니다.$\r이 프로그램을 다른 유저 권한으로 설치한 것 같습니다."
LangString un.GAIM_UNINSTALL_ERROR_2         	${LANG_KOREAN} "이 프로그램을 제거할 수 있는 권한이 없습니다."

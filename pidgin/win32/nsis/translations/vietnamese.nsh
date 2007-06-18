;;
;;  vietnamese.nsh
;;
;;  Vietnamese language strings for the Windows Pidgin NSIS installer.
;;  Windows Code page: 1258
;;
;;  Version 2
;;  Note: The NSIS Installer does not yet have Vietnamese translations. Until
;;  it does, these translations can not be used. 
;;

; Startup GTK+ check
!define GTK_INSTALLER_NEEDED			"The GTK+ runtime environment không có hoặc cần được nâng cấp.$\rHãy cài đặt GTK+ runtime v${GTK_MIN_VERSION} hoặc mới hơn"

; License Page
!define PIDGIN_LICENSE_BUTTON			"Tiếp theo >"
!define PIDGIN_LICENSE_BOTTOM_TEXT		"$(^Name) được phát hành theo giấy  phép GPL. Giấy phép thấy ở đây chỉ là để cung cấp thông tin mà thôi. $_CLICK"

; Components Page
!define PIDGIN_SECTION_TITLE			"Trình Khách Thông Điệp Tức Thời Pidgin (phải có)"
!define GTK_SECTION_TITLE			"GTK+ Runtime Environment (phải có)"
!define PIDGIN_SECTION_DESCRIPTION		"Các tập tin Pidgin chính và dlls"
!define GTK_SECTION_DESCRIPTION		"Bộ công cụ giao diện đồ họa đa nền để dùng cho Pidgin"

; GTK+ Directory Page
!define GTK_UPGRADE_PROMPT			"Phát hiện thấy có phiên bản cũ của  GTK+ runtime. Bạn muốn nâng cấp không?$\rNote: $(^Name) có thể không chạy nếu không nâng cấp."

; Installer Finish Page
!define PIDGIN_FINISH_VISIT_WEB_SITE		"Hãy xem trang chủ Windows Pidgin"

; GTK+ Section Prompts
!define GTK_INSTALL_ERROR			"Lỗi cài đặt GTK+ runtime."
!define GTK_BAD_INSTALL_PATH			"Đường dẫn mà bạn nhập có thể không truy cập được hay không tạo được."

; Uninstall Section Prompts
!define un.PIDGIN_UNINSTALL_ERROR_1		"Trình gỡ cài đặt không tìm được các  registry entry cho Pidgin.$\rCó thể là chương trình được người dùng khác cài đặt."
!define un.PIDGIN_UNINSTALL_ERROR_2		"Bạn không có quyền hạn để gỡ bỏ chương trình này."

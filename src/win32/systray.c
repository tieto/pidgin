/*
 *  systray.c
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: November, 2002
 *  Description: Gaim systray functionality
 */
#include <windows.h>
#include <gdk/gdkwin32.h>
#include "resource.h"
#include "gaim.h"
#include "win32dep.h"
#include "MinimizeToTray.h"

/*
 *  DEFINES, MACROS & DATA TYPES
 */
#define GAIM_SYSTRAY_HINT _("Gaim Instant Messenger")
#define GAIM_SYSTRAY_DISCONN_HINT _("Gaim Instant Messenger - Signed off")
#define WM_TRAYMESSAGE WM_USER /* User defined WM Message */

enum _GAIM_WIN {
	GAIM_LOGIN_WIN,
	GAIM_BUDDY_WIN,
	GAIM_WIN_COUNT
};

typedef enum _GAIM_WIN GAIM_WIN;

enum _GAIM_STATE {
	GAIM_STATE_NONE = -1,
	GAIM_STATE_HIDDEN,
	GAIM_STATE_SHOWN,
	GAIM_STATE_COUNT
};

typedef enum _GAIM_STATE GAIM_STATE;

/*
 *  LOCALS
 */
static HWND systray_hwnd=0;
static HICON sysicon_disconn=0;
static HICON sysicon_conn=0;
static NOTIFYICONDATA wgaim_nid;
static GAIM_WIN gaim_main_win=-1;
static HWND gaim_windows[GAIM_WIN_COUNT];
static GAIM_STATE main_win_state=-1;

/*
 *  PRIVATE CODE
 */

static LRESULT CALLBACK systray_mainmsg_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	switch(msg) {
	case WM_CREATE:
		debug_printf("WM_CREATE\n");
		break;
		
	case WM_TIMER:
		debug_printf("WM_TIMER\n");
		break;

	case WM_DESTROY:
		debug_printf("WM_DESTROY\n");
		break;

	case WM_COMMAND:
		debug_printf("WM_COMMAND\n");
		break;

	case WM_TRAYMESSAGE:
	{
		if( lparam == WM_LBUTTONDBLCLK ) {
			/* If Gaim main win is hidden.. restore it */
			if( main_win_state == GAIM_STATE_HIDDEN ) {
				RestoreWndFromTray(gaim_windows[gaim_main_win]);
			}
			debug_printf("Systray got double click\n");
		}
		if( lparam == WM_LBUTTONUP ) {
			debug_printf("Systray got Left button up\n");
		}		
	}
	default:
	}/* end switch */

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND create_hidenwin() {
	WNDCLASSEX wcex;
	TCHAR wname[32];

	strcpy(wname, "GaimWin");

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style	        = 0;
	wcex.lpfnWndProc	= (WNDPROC)systray_mainmsg_handler;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= wgaim_hinstance();
	wcex.hIcon		= NULL;
	wcex.hCursor		= NULL,
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= wname;
	wcex.hIconSm		= NULL;

	RegisterClassEx(&wcex);

	// Create the window
	return (CreateWindow(wname, "", 0, 0, 0, 0, 0, GetDesktopWindow(), NULL, wgaim_hinstance(), 0));
}

static void systray_init_icon(HWND hWnd, HICON icon) {
	ZeroMemory(&wgaim_nid,sizeof(wgaim_nid));
	wgaim_nid.cbSize=sizeof(NOTIFYICONDATA);
	wgaim_nid.hWnd=hWnd;
	wgaim_nid.uID=0;
	wgaim_nid.uFlags=NIF_ICON | NIF_MESSAGE | NIF_TIP;
	wgaim_nid.uCallbackMessage=WM_TRAYMESSAGE;
	wgaim_nid.hIcon=icon;
	strcpy(wgaim_nid.szTip,GAIM_SYSTRAY_DISCONN_HINT);

	Shell_NotifyIcon(NIM_ADD,&wgaim_nid);
}

static void systray_change_icon(HICON icon, char* text) {
	wgaim_nid.hIcon = icon;
	lstrcpy(wgaim_nid.szTip, text);
	Shell_NotifyIcon(NIM_MODIFY,&wgaim_nid);
}

static void systray_remove_nid(void) {
	Shell_NotifyIcon(NIM_DELETE,&wgaim_nid);
}

static void systray_minimize_win(HWND hWnd) {
	MinimizeWndToTray(hWnd);
	main_win_state = GAIM_STATE_HIDDEN;
}

static GdkFilterReturn buddywin_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data) {

	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_SHOWWINDOW:
		if(msg->wParam) {
			debug_printf("Buddy window is being shown\n");
			/* Keep track of the main Gaim window */
			gaim_main_win = GAIM_BUDDY_WIN;
			main_win_state = GAIM_STATE_SHOWN;
			systray_change_icon(sysicon_conn, GAIM_SYSTRAY_HINT);
		}
		else
			debug_printf("Buddy window is being hidden\n");
		break;
	case WM_SYSCOMMAND:
		if( msg->wParam == SC_MINIMIZE ) {
			systray_minimize_win(msg->hwnd);
			return GDK_FILTER_REMOVE;
		}
		break;
	case WM_CLOSE:
		systray_minimize_win(msg->hwnd);
		debug_printf("Buddy window closed\n");
		return GDK_FILTER_REMOVE;
	case WM_DESTROY:
		debug_printf("Buddy window destroyed\n");
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn loginwin_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data) {
	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_SHOWWINDOW:
		if(msg->wParam) {
			debug_printf("Login window is being shown\n");
			/* Keep track of the main Gaim window */
			gaim_main_win = GAIM_LOGIN_WIN;
			main_win_state = GAIM_STATE_SHOWN;
			systray_change_icon(sysicon_disconn, GAIM_SYSTRAY_DISCONN_HINT);
		}
		else
			debug_printf("Login window is being hidden\n");
		break;
#if 0
	case WM_SYSCOMMAND:
		if( msg->wParam == SC_MINIMIZE ) {
			systray_minimize_win(msg->hwnd);
			return GDK_FILTER_REMOVE;
		}
		break;
#endif
	case WM_CLOSE:
		systray_minimize_win(msg->hwnd);
		debug_printf("Login window closed\n");
		return GDK_FILTER_REMOVE;
	case WM_DESTROY:
		debug_printf("Login window destroyed\n");
		break;
	}

	return GDK_FILTER_CONTINUE;
}

/*
 *  PUBLIC CODE
 */

/* Create a hidden window and associate it with the systray icon.
   We use this hidden window to proccess WM_TRAYMESSAGE msgs. */
void wgaim_systray_init(void) {
	/* dummy window to process systray messages */
	systray_hwnd = create_hidenwin();

	/* Load icons, and init systray notify icon */
	sysicon_disconn = LoadIcon(wgaim_hinstance(),MAKEINTRESOURCE(IDI_ICON2));
	sysicon_conn = LoadIcon(wgaim_hinstance(),MAKEINTRESOURCE(IDI_ICON3));

	/* Create icon in systray */
	systray_init_icon(systray_hwnd, sysicon_disconn);
}

void wgaim_systray_cleanup(void) {
	systray_remove_nid();
}

/* This function is called after the buddy list has been created */
void wgaim_created_blistwin( GtkWidget *blist ) {
	gdk_window_add_filter (GTK_WIDGET(blist)->window,
			       buddywin_filter,
			       NULL);
	gaim_main_win = GAIM_BUDDY_WIN;
	gaim_windows[GAIM_BUDDY_WIN] = GDK_WINDOW_HWND(GTK_WIDGET(blist)->window);
}

/* This function is called after the login window has been created */
void wgaim_created_loginwin( GtkWidget *loginwin ) {
	gdk_window_add_filter (GTK_WIDGET(loginwin)->window,
			       loginwin_filter,
			       NULL);
	gaim_main_win = GAIM_LOGIN_WIN;
	gaim_windows[GAIM_LOGIN_WIN] = GDK_WINDOW_HWND(GTK_WIDGET(loginwin)->window);
	/*systray_init_icon(GDK_WINDOW_HWND(GTK_WIDGET(loginwin)->window), sysicon_disconn);*/
}


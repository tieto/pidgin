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
#include "ui.h"

/*
 *  DEFINES, MACROS & DATA TYPES
 */
#define GAIM_SYSTRAY_HINT _("Gaim Instant Messenger")
#define GAIM_SYSTRAY_DISCONN_HINT _("Gaim Instant Messenger - Signed off")
#define GAIM_SYSTRAY_AWAY_HINT _("Gaim Instant Messenger - Away")
#define WM_TRAYMESSAGE WM_USER /* User defined WM Message */
#define MAX_AWY_MESSAGES 50

enum _SYSTRAY_STATE {
	SYSTRAY_STATE_CONN,
	SYSTRAY_STATE_CONNECTING,
	SYSTRAY_STATE_DISCONN,
	SYSTRAY_STATE_AWAY,
	SYSTRAY_STATE_COUNT
};
typedef enum _SYSTRAY_STATE SYSTRAY_STATE;

enum _SYSTRAY_CMND {
	SYSTRAY_CMND_MENU_EXIT,
	SYSTRAY_CMND_SIGNON,
	SYSTRAY_CMND_SIGNOFF,
	SYSTRAY_CMND_AUTOLOGIN,
	SYSTRAY_CMND_PREFS,
	SYSTRAY_CMND_BACK,
	SYSTRAY_CMND_SET_AWY_NEW,
	SYSTRAY_CMND_SET_AWY,
	SYSTRAY_CMND_SET_AWY_LAST=SYSTRAY_CMND_SET_AWY+MAX_AWY_MESSAGES
};
typedef enum _SYSTRAY_CMND SYSTRAY_CMND;

/*
 *  LOCALS
 */
static HWND systray_hwnd=0;
static HICON sysicon_disconn=0;
static HICON sysicon_conn=0;
static HICON sysicon_away=0;
static NOTIFYICONDATA wgaim_nid;
static SYSTRAY_STATE st_state=SYSTRAY_STATE_DISCONN;
static HMENU systray_menu=0;
static HMENU systray_away_menu=0;

/*
 *  GLOBALS
 */
extern GtkWidget *imaway;

/*
 *  PRIVATE CODE
 */

/*
 * SYSTRAY HELPERS
 ********************/

/* Returns 1 if menu item exists, 0 if not */
static int IsMenuItem( HMENU hMenu, UINT id ) {
	if(0xFFFFFFFF == GetMenuState(hMenu, id, MF_BYCOMMAND))
		return 0;
	else
		return 1;
}

/*
 * WGAIM SYSTRAY GUI
 ********************/

static HMENU systray_create_awy_menu(void) {
	int item_count = SYSTRAY_CMND_SET_AWY;
	struct away_message *a = NULL;
	GSList *awy = away_messages;

	/* Delete previous away submenu */
	if(systray_away_menu) {
		DestroyMenu(systray_away_menu);
		systray_away_menu = 0;
	}		
	systray_away_menu = CreatePopupMenu();
	while (awy && (item_count <= SYSTRAY_CMND_SET_AWY+MAX_AWY_MESSAGES)) {
		a = (struct away_message *)awy->data;
		AppendMenu(systray_away_menu, MF_STRING, item_count, a->name);
		awy = g_slist_next(awy);
		item_count+=1;
	}
	AppendMenu(systray_away_menu, MF_SEPARATOR, 0, 0);
	AppendMenu(systray_away_menu, MF_STRING, SYSTRAY_CMND_SET_AWY_NEW, _("New"));
	return systray_away_menu;
}

static void systray_show_menu(int x, int y, BOOL connected) {
	/* need to call this so that the menu disappears if clicking outside
           of the menu scope */
	SetForegroundWindow(systray_hwnd);

	/* Different menus depending on signed on/off state */
	if(connected) {
		/* If signoff item dosn't exist.. create it */
		if(!IsMenuItem(systray_menu, SYSTRAY_CMND_SIGNOFF)) {
			DeleteMenu(systray_menu, SYSTRAY_CMND_SIGNON, MF_BYCOMMAND);
			InsertMenu(systray_menu, SYSTRAY_CMND_MENU_EXIT, 
				   MF_BYCOMMAND | MF_STRING, SYSTRAY_CMND_SIGNOFF, _("Signoff"));
		}
		/* if away menu exists, remove and rebuild it */
		if(systray_away_menu) {
			if(!DeleteMenu(systray_menu, (UINT)systray_away_menu, MF_BYCOMMAND))
				debug_printf("Error using DeleteMenu\n");
		}
		InsertMenu(systray_menu, SYSTRAY_CMND_PREFS, 
			   MF_BYCOMMAND | MF_POPUP | MF_STRING, (UINT)systray_create_awy_menu(),
			   _("Set Away Message"));
		EnableMenuItem(systray_menu, SYSTRAY_CMND_AUTOLOGIN, MF_GRAYED);
		/* If away, put "I'm Back" option in menu */
		if(st_state == SYSTRAY_STATE_AWAY) {
			if(!IsMenuItem(systray_menu, SYSTRAY_CMND_BACK)) {
				InsertMenu(systray_menu, (UINT)systray_away_menu, 
					   MF_BYCOMMAND | MF_STRING, SYSTRAY_CMND_BACK,
					   _("I'm Back"));
			}
		} else {
			/* Delete I'm Back item if it exists */
			DeleteMenu(systray_menu, SYSTRAY_CMND_BACK, MF_BYCOMMAND);
		}
	} else {
		/* If signon item dosn't exist.. create it */
		if(!IsMenuItem(systray_menu, SYSTRAY_CMND_SIGNON)) {
			DeleteMenu(systray_menu, SYSTRAY_CMND_SIGNOFF, MF_BYCOMMAND);
			InsertMenu(systray_menu, SYSTRAY_CMND_MENU_EXIT, 
				   MF_BYCOMMAND | MF_STRING, SYSTRAY_CMND_SIGNON, _("Sign On"));
		}
		EnableMenuItem(systray_menu, SYSTRAY_CMND_AUTOLOGIN, MF_ENABLED);
		EnableMenuItem(systray_menu, (UINT)systray_away_menu, MF_GRAYED);
		/* Delete I'm Back item if it exists */
		DeleteMenu(systray_menu, SYSTRAY_CMND_BACK, MF_BYCOMMAND);
	}

	TrackPopupMenu(systray_menu,         // handle to shortcut menu
		       TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
		       x,                   // horizontal position, in screen coordinates
		       y,                   // vertical position, in screen coordinates
		       0,                   // reserved, must be zero
		       systray_hwnd,        // handle to owner window
		       NULL                 // ignored
		       );
}

/* Set nth away message from away_messages list */
static void systray_set_away(int nth) {
	int item_count = 0;
	GSList *awy = away_messages;
	struct away_message *a = NULL;

	while (awy && (item_count != nth)) {
		awy = g_slist_next(awy);
		item_count+=1;
	}
	if(awy) {
		a = (struct away_message *)awy->data;
		do_away_message(NULL, a);
	}
}

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
		switch(LOWORD(wparam)) {
		case SYSTRAY_CMND_MENU_EXIT:
			do_quit();
			break;
		case SYSTRAY_CMND_SIGNON:
			debug_printf("signon\n");
			show_login();
			break;
		case SYSTRAY_CMND_SIGNOFF:
			debug_printf("signoff\n");
			signoff_all();
			break;
		case SYSTRAY_CMND_AUTOLOGIN:
			debug_printf("autologin\n");
			auto_login();
			break;
		case SYSTRAY_CMND_PREFS:
			debug_printf("Prefs\n");
			show_prefs();
			break;
		case SYSTRAY_CMND_BACK:
			debug_printf("I'm back\n");
			do_im_back(NULL, NULL);
			break;
		case SYSTRAY_CMND_SET_AWY_NEW:
			debug_printf("New away item\n");
			create_away_mess(NULL, NULL);
			break;
		default:
			/* SYSTRAY_CMND_SET_AWY */
			if((LOWORD(wparam) >= SYSTRAY_CMND_SET_AWY) &&
			   (LOWORD(wparam) <= (SYSTRAY_CMND_SET_AWY + MAX_AWY_MESSAGES))) {
				debug_printf("Set away message\n");
				systray_set_away(LOWORD(wparam)-SYSTRAY_CMND_SET_AWY);
			}
		}
		break;
	case WM_TRAYMESSAGE:
	{
		if( lparam == WM_LBUTTONDBLCLK ) {
			/* Either hide or show current window (login or buddy) */
			docklet_toggle();
			/* if away.. hide/show I'm back win too */
			if(st_state == SYSTRAY_STATE_AWAY) {
				if(GTK_WIDGET_VISIBLE(blist) && !GTK_WIDGET_VISIBLE(imaway)) {
					RestoreWndFromTray(GDK_WINDOW_HWND(GTK_WIDGET(imaway)->window));
					gtk_window_present(GTK_WINDOW(imaway));
				} else if(!GTK_WIDGET_VISIBLE(blist) && GTK_WIDGET_VISIBLE(imaway)) {
					wgaim_systray_minimize(imaway);
					gtk_widget_hide(imaway);
				}
			}
			debug_printf("Systray got double click\n");
		}
		if( lparam == WM_RBUTTONUP ) {
			POINT mpoint;
			GetCursorPos(&mpoint);
			/* Are we connected ? */
			if(st_state == SYSTRAY_STATE_CONNECTING)
				break; /* no menu when connecting */
			if(st_state == SYSTRAY_STATE_DISCONN)
				systray_show_menu(mpoint.x, mpoint.y, 0);
			else
				systray_show_menu(mpoint.x, mpoint.y, 1);
		}
		break;
	}
	default:
	}/* end switch */

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

/* Create hidden window to process systray messages */
static HWND systray_create_hiddenwin() {
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

static void systray_create_menu(void) {
	/* create popup menu */
	if((systray_menu = CreatePopupMenu())) {
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_PREFS, _("Preferences")))
			debug_printf("AppendMenu error: %d\n", GetLastError());
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_AUTOLOGIN, _("Auto-login")))
			debug_printf("AppendMenu error: %d\n", GetLastError());
		if(!AppendMenu(systray_menu, MF_SEPARATOR, 0, 0))
			debug_printf("AppendMenu error: %d\n", GetLastError());
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_MENU_EXIT, _("Exit")))
			debug_printf("AppendMenu error: %d\n", GetLastError());
	} else
		debug_printf("CreatePopupMenu error: %d\n", GetLastError());
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

static void systray_update_icon() {
	switch(st_state) {
	case SYSTRAY_STATE_CONN:
		systray_change_icon(sysicon_conn, GAIM_SYSTRAY_HINT);
		break;
	case SYSTRAY_STATE_CONNECTING:
		break;
	case SYSTRAY_STATE_DISCONN:
		systray_change_icon(sysicon_disconn, GAIM_SYSTRAY_DISCONN_HINT);
		break;
	case SYSTRAY_STATE_AWAY:
		systray_change_icon(sysicon_away, GAIM_SYSTRAY_AWAY_HINT);
		break;
	}
}

static void systray_update_status() {
	SYSTRAY_STATE old_state = st_state;

	if(connections) {
		if(awaymessage) {
			st_state = SYSTRAY_STATE_AWAY;
		} else if(connecting_count) {
			st_state = SYSTRAY_STATE_CONNECTING;
		} else {
			st_state = SYSTRAY_STATE_CONN;
		}
	} else {
		if(connecting_count) {
			st_state = SYSTRAY_STATE_CONNECTING;
		} else {
			st_state = SYSTRAY_STATE_DISCONN;
		}
	}
	if(st_state != old_state) {
		systray_update_icon();
	}
}

/*
 * GAIM WINDOW FILTERS 
 **********************/

static GdkFilterReturn st_buddywin_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data) {

	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_SYSCOMMAND:
		if( msg->wParam == SC_MINIMIZE ) {
			hide_buddy_list();
			return GDK_FILTER_REMOVE;
		}
		break;
	case WM_CLOSE:
		hide_buddy_list();
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn st_loginwin_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data) {
	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_CLOSE:
		wgaim_systray_minimize(mainwindow);
		gtk_widget_hide(mainwindow);
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn st_backwin_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data) {
	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_SYSCOMMAND:
		if( msg->wParam == SC_MINIMIZE ) {
			if(imaway) {
				wgaim_systray_minimize(imaway);
				gtk_widget_hide(imaway);
			}
			return GDK_FILTER_REMOVE;
		}
		break;
	}
	return GDK_FILTER_CONTINUE;
}

/*
 * GAIM EVENT CALLBACKS
 ***********************/

static void st_signon(struct gaim_connection *gc, void *data) {
	systray_update_status();
}

static void st_signoff(struct gaim_connection *gc, void *data) {
	systray_update_status();
}

static void st_away(struct gaim_connection *gc, void *data) {
	systray_update_status();
}

static void st_back(struct gaim_connection *gc, void *data) {
	systray_update_status();
}

static void st_im_recieve(struct gaim_connection *gc, void *data) {
	
}

/*
 *  PUBLIC CODE
 */

/* Create a hidden window and associate it with the systray icon.
   We use this hidden window to proccess WM_TRAYMESSAGE msgs. */
void wgaim_systray_init(void) {
	docklet_add();

	/* dummy window to process systray messages */
	systray_hwnd = systray_create_hiddenwin();

	systray_create_menu();

	/* Load icons, and init systray notify icon */
	sysicon_disconn = LoadIcon(wgaim_hinstance(),MAKEINTRESOURCE(IDI_ICON2));
	sysicon_conn = LoadIcon(wgaim_hinstance(),MAKEINTRESOURCE(IDI_ICON3));
	sysicon_away = LoadIcon(wgaim_hinstance(),MAKEINTRESOURCE(IDI_ICON4));

	/* Create icon in systray */
	systray_init_icon(systray_hwnd, sysicon_disconn);

	/* Register Gaim event callbacks */
	gaim_signal_connect(NULL, event_signon, st_signon, NULL);
	gaim_signal_connect(NULL, event_signoff, st_signoff, NULL);
	gaim_signal_connect(NULL, event_away, st_away, NULL);
	gaim_signal_connect(NULL, event_back, st_back, NULL);
	/*gaim_signal_connect(NULL, event_connecting, wgaim_st_connecting, NULL);
	  gaim_signal_connect(NULL, event_im_displayed_rcvd, wgaim_st_im_displayed_recv, NULL);*/
}

void wgaim_systray_cleanup(void) {
	docklet_remove();
	systray_remove_nid();
	DestroyMenu(systray_menu);
	DestroyWindow(systray_hwnd);
}

/* This function is called after the buddy list has been created */
void wgaim_created_blistwin( GtkWidget *blist ) {
	gdk_window_add_filter (GTK_WIDGET(blist)->window,
			       st_buddywin_filter,
			       NULL);
}

/* This function is called after the login window has been created */
void wgaim_created_loginwin( GtkWidget *loginwin ) {
	gdk_window_add_filter (GTK_WIDGET(loginwin)->window,
			       st_loginwin_filter,
			       NULL);
}

void wgaim_created_backwin( GtkWidget *backwin ) {
	gdk_window_add_filter (GTK_WIDGET(backwin)->window,
			       st_backwin_filter,
			       NULL);
}

void wgaim_systray_minimize( GtkWidget *window ) {
	MinimizeWndToTray(GDK_WINDOW_HWND(window->window));
}

void wgaim_systray_maximize( GtkWidget *window ) {
	RestoreWndFromTray(GDK_WINDOW_HWND(window->window));
}

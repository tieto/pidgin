/*
 * gaim - Windows Gaim systray module
 *
 * File: systray.c
 * Date: November, 2002
 * 
 * Copyright (C) 2002, Herman Bloggs <hermanator12002@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <windows.h>
#include <gdk/gdkwin32.h>
#include "resource.h"
#include "gaim.h"
#include "win32dep.h"
#include "MinimizeToTray.h"
#include "ui.h"
#include "gtkblist.h"

/*
 *  DEFINES, MACROS & DATA TYPES
 */
#define GAIM_SYSTRAY_HINT _("Gaim Instant Messenger")
#define GAIM_SYSTRAY_DISCONN_HINT _("Gaim Instant Messenger - Signed off")
#define GAIM_SYSTRAY_AWAY_HINT _("Gaim Instant Messenger - Away")
#define WM_TRAYMESSAGE WM_USER /* User defined WM Message */
#define MAX_AWY_MESSAGES 50

enum _SYSTRAY_STATE {
	SYSTRAY_STATE_ONLINE,
	SYSTRAY_STATE_ONLINE_CONNECTING,
	SYSTRAY_STATE_OFFLINE,
	SYSTRAY_STATE_OFFLINE_CONNECTING,
	SYSTRAY_STATE_AWAY,
	SYSTRAY_STATE_COUNT
};
typedef enum _SYSTRAY_STATE SYSTRAY_STATE;

enum _SYSTRAY_CMND {
	SYSTRAY_CMND_MENU_EXIT=100,
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
static SYSTRAY_STATE st_state=SYSTRAY_STATE_OFFLINE;
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

static int GetMenuItemPosition( HMENU hMenu, UINT id ) {
	int count  = GetMenuItemCount(hMenu);
	int i;

	for(i=0;i<count;i++) {
		if(GetMenuItemID(hMenu, i)==id)
			return i;
	}
	return -1;
}

/*
 * WGAIM SYSTRAY GUI
 ********************/

static HMENU systray_create_awy_menu(void) {
	int item_count = SYSTRAY_CMND_SET_AWY;
	struct away_message *a = NULL;
	GSList *awy = away_messages;
	char* locenc=NULL;

	/* Delete previous away submenu */
	if(systray_away_menu) {
		DestroyMenu(systray_away_menu);
		systray_away_menu = 0;
	}		
	systray_away_menu = CreatePopupMenu();
	while (awy && (item_count <= SYSTRAY_CMND_SET_AWY+MAX_AWY_MESSAGES)) {
		a = (struct away_message *)awy->data;
		locenc = g_locale_from_utf8(a->name, -1, NULL, NULL, NULL);
		AppendMenu(systray_away_menu, MF_STRING, item_count, locenc);
		g_free(locenc);
		awy = g_slist_next(awy);
		item_count+=1;
	}
	AppendMenu(systray_away_menu, MF_SEPARATOR, 0, 0);
	locenc = g_locale_from_utf8(_("New"), -1, NULL, NULL, NULL);
	AppendMenu(systray_away_menu, MF_STRING, SYSTRAY_CMND_SET_AWY_NEW, locenc);
	g_free(locenc);
	return systray_away_menu;
}

static void systray_show_menu(int x, int y, BOOL connected) {
	char* locenc=NULL;
	/* need to call this so that the menu disappears if clicking outside
           of the menu scope */
	SetForegroundWindow(systray_hwnd);

	/* in both connected and disconnected case delete away message menu */
	if(systray_away_menu) {
		if(!DeleteMenu(systray_menu, (UINT)systray_away_menu, MF_BYCOMMAND))
			debug_printf("Error using DeleteMenu\n");
	}


	/* Different menus depending on signed on/off state */
	if(connected) {
		/* If signoff item dosn't exist.. create it */
		if(!IsMenuItem(systray_menu, SYSTRAY_CMND_SIGNOFF)) {
			int pos;
			DeleteMenu(systray_menu, SYSTRAY_CMND_SIGNON, MF_BYCOMMAND);
			locenc = g_locale_from_utf8(_("Signoff"), -1, NULL, NULL, NULL);
			pos = GetMenuItemPosition(systray_menu, SYSTRAY_CMND_MENU_EXIT) - 1;
			if(!InsertMenu(systray_menu, pos, 
				       MF_BYPOSITION | MF_STRING, SYSTRAY_CMND_SIGNOFF, locenc))
				debug_printf("InsertMenu failed: %s\n", GetLastError());
			debug_printf("Inserted Menu with ID: %d\n", SYSTRAY_CMND_SIGNOFF);
			g_free(locenc);
		}
		locenc = g_locale_from_utf8(_("Set Away Message"), -1, NULL, NULL, NULL);
		InsertMenu(systray_menu, SYSTRAY_CMND_PREFS, 
			   MF_BYCOMMAND | MF_POPUP | MF_STRING, (UINT)systray_create_awy_menu(),
			   locenc);
		g_free(locenc);
		EnableMenuItem(systray_menu, SYSTRAY_CMND_AUTOLOGIN, MF_GRAYED);
		/* If away, put "I'm Back" option in menu */
		if(st_state == SYSTRAY_STATE_AWAY) {
			if(!IsMenuItem(systray_menu, SYSTRAY_CMND_BACK)) {
				locenc = g_locale_from_utf8(_("I'm Back"), -1, NULL, NULL, NULL);
				InsertMenu(systray_menu, (UINT)systray_away_menu, 
					   MF_BYCOMMAND | MF_STRING, SYSTRAY_CMND_BACK,
					   locenc);
				g_free(locenc);
			}
		} else {
			/* Delete I'm Back item if it exists */
			DeleteMenu(systray_menu, SYSTRAY_CMND_BACK, MF_BYCOMMAND);
		}
	} else {
		/* If signon item dosn't exist.. create it */
		if(!IsMenuItem(systray_menu, SYSTRAY_CMND_SIGNON)) {
			int pos;
			DeleteMenu(systray_menu, SYSTRAY_CMND_SIGNOFF, MF_BYCOMMAND);
			locenc = g_locale_from_utf8(_("Sign On"), -1, NULL, NULL, NULL);
			pos = GetMenuItemPosition(systray_menu, SYSTRAY_CMND_MENU_EXIT) - 1;
			if(!InsertMenu(systray_menu, pos, 
				       MF_BYPOSITION | MF_STRING, SYSTRAY_CMND_SIGNON, locenc))
				debug_printf("InsertMenu failed: %s\n", GetLastError());
			g_free(locenc);
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
	static UINT taskbarRestartMsg; /* static here means value is kept across multiple calls to this func */

	switch(msg) {
	case WM_CREATE:
		debug_printf("WM_CREATE\n");
		taskbarRestartMsg = RegisterWindowMessage("TaskbarCreated");
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
			/* Double Click */
			/* Either hide or show current window (login or buddy) */
			gaim_gtk_blist_docklet_toggle();
			debug_printf("Systray got double click\n");
		}
		if( lparam == WM_RBUTTONUP ) {
			/* Right Click */
			POINT mpoint;
			GetCursorPos(&mpoint);

			switch(st_state) {
			case SYSTRAY_STATE_OFFLINE:
			case SYSTRAY_STATE_OFFLINE_CONNECTING:
				systray_show_menu(mpoint.x, mpoint.y, 0);
				break;
			default:
				systray_show_menu(mpoint.x, mpoint.y, 1);
			}
		}
		break;
	}
	default: 
		if (msg == taskbarRestartMsg) {
			/* explorer crashed and left us hanging... 
			   This will put the systray icon back in it's place, when it restarts */
			Shell_NotifyIcon(NIM_ADD,&wgaim_nid);
		}
		break;
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
	char* locenc=NULL;

	/* create popup menu */
	if((systray_menu = CreatePopupMenu())) {
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_PREFS, 
			       (locenc=g_locale_from_utf8(_("Preferences"), -1, NULL, NULL, NULL))))
			debug_printf("AppendMenu error: %ld\n", GetLastError());
		g_free(locenc);
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_AUTOLOGIN, 
			       (locenc=g_locale_from_utf8(_("Auto-login"), -1, NULL, NULL, NULL))))
			debug_printf("AppendMenu error: %ld\n", GetLastError());
		g_free(locenc);
		if(!AppendMenu(systray_menu, MF_SEPARATOR, 0, 0))
			debug_printf("AppendMenu error: %ld\n", GetLastError());
		if(!AppendMenu(systray_menu, MF_STRING, SYSTRAY_CMND_MENU_EXIT,
			       (locenc=g_locale_from_utf8(_("Exit"), -1, NULL, NULL, NULL))))
			debug_printf("AppendMenu error: %ld\n", GetLastError());
		g_free(locenc);
	} else
		debug_printf("CreatePopupMenu error: %ld\n", GetLastError());
}

static void systray_init_icon(HWND hWnd, HICON icon) {
	char* locenc=NULL;

	ZeroMemory(&wgaim_nid,sizeof(wgaim_nid));
	wgaim_nid.cbSize=sizeof(NOTIFYICONDATA);
	wgaim_nid.hWnd=hWnd;
	wgaim_nid.uID=0;
	wgaim_nid.uFlags=NIF_ICON | NIF_MESSAGE | NIF_TIP;
	wgaim_nid.uCallbackMessage=WM_TRAYMESSAGE;
	wgaim_nid.hIcon=icon;
	locenc=g_locale_from_utf8(GAIM_SYSTRAY_DISCONN_HINT, -1, NULL, NULL, NULL);
	strcpy(wgaim_nid.szTip, locenc);
	g_free(locenc);
	Shell_NotifyIcon(NIM_ADD,&wgaim_nid);
}

static void systray_change_icon(HICON icon, char* text) {
	char *locenc=NULL;
	wgaim_nid.hIcon = icon;
	locenc = g_locale_from_utf8(text, -1, NULL, NULL, NULL);
	lstrcpy(wgaim_nid.szTip, locenc);
	g_free(locenc);
	Shell_NotifyIcon(NIM_MODIFY,&wgaim_nid);
}

static void systray_remove_nid(void) {
	Shell_NotifyIcon(NIM_DELETE,&wgaim_nid);
}

static void systray_update_icon() {
	switch(st_state) {
	case SYSTRAY_STATE_ONLINE:
		systray_change_icon(sysicon_conn, GAIM_SYSTRAY_HINT);
		break;
	case SYSTRAY_STATE_ONLINE_CONNECTING:
	case SYSTRAY_STATE_OFFLINE_CONNECTING:
		break;
	case SYSTRAY_STATE_OFFLINE:
		systray_change_icon(sysicon_disconn, GAIM_SYSTRAY_DISCONN_HINT);
		break;
	case SYSTRAY_STATE_AWAY:
		systray_change_icon(sysicon_away, GAIM_SYSTRAY_AWAY_HINT);
		break;
	case SYSTRAY_STATE_COUNT: /* not a state, here to avoid warning */
	default:
	}
}

static void systray_update_status() {
	SYSTRAY_STATE old_state = st_state;

	if(connections) {
		if(awaymessage) {
			st_state = SYSTRAY_STATE_AWAY;
		} else if(connecting_count) {
			st_state = SYSTRAY_STATE_ONLINE_CONNECTING;
		} else {
			st_state = SYSTRAY_STATE_ONLINE;
		}
	} else {
		if(connecting_count) {
			/* Don't rely on this state.. signoff in multi.c sends
			   event_signoff before decrementing connecting_count
			   for a reason unknown to me..
			*/
			st_state = SYSTRAY_STATE_OFFLINE_CONNECTING;
		} else {
			st_state = SYSTRAY_STATE_OFFLINE;
		}
	}
	if(st_state != old_state) {
		systray_update_icon();
	}
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

/*
 *  PUBLIC CODE
 */

/* Create a hidden window and associate it with the systray icon.
   We use this hidden window to proccess WM_TRAYMESSAGE msgs. */
void wgaim_systray_init(void) {
	gaim_gtk_blist_docklet_add();

	/* dummy window to process systray messages */
	systray_hwnd = systray_create_hiddenwin();

	systray_create_menu();

	/* Load icons, and init systray notify icon */
	sysicon_disconn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_OFFLINE_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
	sysicon_conn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
	sysicon_away = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_AWAY_TRAY_ICON), IMAGE_ICON, 16, 16, 0);

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
	/*gaim_gtk_blist_docklet_remove();*/
	systray_remove_nid();
	DestroyMenu(systray_menu);
	DestroyWindow(systray_hwnd);
}

void wgaim_systray_minimize( GtkWidget *window ) {
	MinimizeWndToTray(GDK_WINDOW_HWND(window->window));
}

void wgaim_systray_maximize( GtkWidget *window ) {
	RestoreWndFromTray(GDK_WINDOW_HWND(window->window));
}

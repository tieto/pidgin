/*
 * System tray icon (aka docklet) plugin for Gaim
 *
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <windows.h>
#include <gdk/gdkwin32.h>
#include <gdk/gdk.h>

#include "internal.h"
#include "gtkblist.h"
#include "gtkprefs.h"
#include "debug.h"

#include "gaim.h"
#include "gtkdialogs.h"

#include "resource.h"
#include "MinimizeToTray.h"
#include "docklet.h"

/*
 *  DEFINES, MACROS & DATA TYPES
 */
#define GAIM_SYSTRAY_HINT _("Gaim")
#define GAIM_SYSTRAY_DISCONN_HINT _("Gaim - Signed off")
#define GAIM_SYSTRAY_AWAY_HINT _("Gaim - Away")
#define WM_TRAYMESSAGE WM_USER /* User defined WM Message */

/*
 *  LOCALS
 */
static HWND systray_hwnd=0;
static HICON sysicon_disconn=0;
static HICON sysicon_conn=0;
static HICON sysicon_away=0;
static HICON sysicon_pend=0;
static HICON sysicon_awypend=0;
static HICON sysicon_blank=0;
static NOTIFYICONDATA wgaim_nid;


static LRESULT CALLBACK systray_mainmsg_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	static UINT taskbarRestartMsg; /* static here means value is kept across multiple calls to this func */

	switch(msg) {
	case WM_CREATE:
		gaim_debug(GAIM_DEBUG_INFO, "tray icon", "WM_CREATE\n");
		taskbarRestartMsg = RegisterWindowMessage("TaskbarCreated");
		break;
		
	case WM_TIMER:
		gaim_debug(GAIM_DEBUG_INFO, "tray icon", "WM_TIMER\n");
		break;

	case WM_DESTROY:
		gaim_debug(GAIM_DEBUG_INFO, "tray icon", "WM_DESTROY\n");
		break;

	case WM_TRAYMESSAGE:
	{
		int type = 0;

		/* We'll use Double Click - Single click over on linux */
		if( lparam == WM_LBUTTONDBLCLK )
			type = 1;
		else if( lparam == WM_MBUTTONUP )
			type = 2;
		else if( lparam == WM_RBUTTONUP )
			type = 3;
		else
			break;

		docklet_clicked(type);
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

	/* Create the window */
	return (CreateWindow(wname, "", 0, 0, 0, 0, 0, GetDesktopWindow(), NULL, wgaim_hinstance(), 0));
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
	docklet_embedded();
}

static void systray_change_icon(HICON icon, char* text) {
	char *locenc=NULL;
	wgaim_nid.hIcon = icon;
        if(text) {
                locenc = g_locale_from_utf8(text, -1, NULL, NULL, NULL);
                lstrcpy(wgaim_nid.szTip, locenc);
                g_free(locenc);
        }
	Shell_NotifyIcon(NIM_MODIFY,&wgaim_nid);
}

static void systray_remove_nid(void) {
	Shell_NotifyIcon(NIM_DELETE,&wgaim_nid);
}

static void wgaim_tray_update_icon(enum docklet_status icon) {
	switch (icon) {
		case offline:
			systray_change_icon(sysicon_disconn, GAIM_SYSTRAY_DISCONN_HINT);
			break;
		case offline_connecting:
		case online_connecting:
			break;
		case online:
			systray_change_icon(sysicon_conn, GAIM_SYSTRAY_HINT);
			break;
		case online_pending:
			systray_change_icon(sysicon_pend, GAIM_SYSTRAY_HINT);
			break;
		case away:
			systray_change_icon(sysicon_away, GAIM_SYSTRAY_AWAY_HINT);
			break;
		case away_pending:
			systray_change_icon(sysicon_awypend, GAIM_SYSTRAY_AWAY_HINT);
			break;
	}
}

static void wgaim_tray_blank_icon() {
        systray_change_icon(sysicon_blank, NULL);
}

static void wgaim_tray_create() {
	OSVERSIONINFO osinfo;
	/* dummy window to process systray messages */
	systray_hwnd = systray_create_hiddenwin();

	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osinfo);

	/* Load icons, and init systray notify icon
	 * NOTE: Windows > XP only supports displaying 4-bit images in the Systray,
	 *  2K and ME will use the highest color depth that the desktop will support,
	 *  but will scale it back to 4-bits for display.
	 * That is why we use custom 4-bit icons for pre XP Windowses */
	if (osinfo.dwMajorVersion == 5 && osinfo.dwMinorVersion > 0) {
		sysicon_disconn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_OFFLINE_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
		sysicon_conn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
		sysicon_away = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_AWAY_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
		sysicon_pend = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_PEND_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
		sysicon_awypend = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_AWAYPEND_TRAY_ICON), IMAGE_ICON, 16, 16, 0);
	} else {
		sysicon_disconn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_OFFLINE_TRAY_ICON_4BIT), IMAGE_ICON, 16, 16, 0);
		sysicon_conn = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_TRAY_ICON_4BIT), IMAGE_ICON, 16, 16, 0);
		sysicon_away = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_AWAY_TRAY_ICON_4BIT), IMAGE_ICON, 16, 16, 0);
		sysicon_pend = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_PEND_TRAY_ICON_4BIT), IMAGE_ICON, 16, 16, 0);
		sysicon_awypend = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_AWAYPEND_TRAY_ICON_4BIT), IMAGE_ICON, 16, 16, 0);
	}
	sysicon_blank = (HICON)LoadImage(wgaim_hinstance(), MAKEINTRESOURCE(GAIM_BLANK_TRAY_ICON), IMAGE_ICON, 16, 16, 0);

	/* Create icon in systray */
	systray_init_icon(systray_hwnd, sysicon_disconn);
	gaim_debug(GAIM_DEBUG_INFO, "tray icon", "created\n");
}

static void wgaim_tray_destroy() {
	systray_remove_nid();
	DestroyWindow(systray_hwnd);
	docklet_remove(TRUE);
}

static struct docklet_ui_ops wgaim_tray_ops =
{
	wgaim_tray_create,
	wgaim_tray_destroy,
	wgaim_tray_update_icon,
	wgaim_tray_blank_icon,
	NULL
};

/* Used by docklet's plugin load func */
void docklet_ui_init() {
	docklet_set_ui_ops(&wgaim_tray_ops);
}

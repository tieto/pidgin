/*
 *  win32dep.c
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: June, 2002
 *  Description: Windows dependant code for Gaim
 */
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include "gaim.h"

#include "stdafx.h"
#include "resource.h"

/*
 *  DEFINES & MACROS
 */
#define WM_TRAYMESSAGE WM_USER
#define GAIM_SYSTRAY_HINT "Gaim Instant Messenger"

/*
 * LOCALS
 */
static char install_dir[MAXPATHLEN];
static char lib_dir[MAXPATHLEN];
static char locale_dir[MAXPATHLEN];
static int bhide_icon;

/*
 *  GLOBALS
 */
HINSTANCE g_hInstance = 0;

/*
 *  STATIC CODE
 */

static void ShowNotifyIcon(HWND hWnd,BOOL bAdd)
{
	NOTIFYICONDATA nid;
	ZeroMemory(&nid,sizeof(nid));
	nid.cbSize=sizeof(NOTIFYICONDATA);
	nid.hWnd=hWnd;
	nid.uID=0;
	nid.uFlags=NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage=WM_TRAYMESSAGE;
	nid.hIcon=LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_ICON2));
	lstrcpy(nid.szTip,TEXT(GAIM_SYSTRAY_HINT));
	
	if(bAdd)
		Shell_NotifyIcon(NIM_ADD,&nid);
	else
		Shell_NotifyIcon(NIM_DELETE,&nid);
}

static GdkFilterReturn traymsg_filter_func( GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	MSG *msg = (MSG*)xevent;

	if( msg->lParam == WM_LBUTTONDBLCLK ) {
	  RestoreWndFromTray(msg->hwnd);
	  bhide_icon = TRUE;
	  return GDK_FILTER_REMOVE;
	}

	if( msg->lParam == WM_LBUTTONUP ) {
		if(bhide_icon) {
			ShowNotifyIcon(msg->hwnd,FALSE);
			bhide_icon = FALSE;
		}
	}
	return GDK_FILTER_REMOVE;
}

/*
 *  PUBLIC CODE
 */

char* wgaim_install_dir(void) {
	HMODULE hmod;
	char* buf;

	hmod = GetModuleHandle(NULL);
	if( hmod == 0 ) {
		buf = g_win32_error_message( GetLastError() );
		debug_printf("GetModuleHandle error: %s\n", buf);
		free(buf);
		return NULL;
	}
	if(GetModuleFileName( hmod, (char*)&install_dir, MAXPATHLEN ) == 0) {
		buf = g_win32_error_message( GetLastError() );
		debug_printf("GetModuleFileName error: %s\n", buf);
		free(buf);
		return NULL;
	}
	buf = g_path_get_dirname( install_dir );
	strcpy( (char*)&install_dir, buf );
	free( buf );

	return (char*)&install_dir;
}

char* wgaim_lib_dir(void) {
	strcpy(lib_dir, wgaim_install_dir());
	strcat(lib_dir, G_DIR_SEPARATOR_S "plugins");
	return (char*)&lib_dir;
}

char* wgaim_locale_dir(void) {
	strcpy(locale_dir, wgaim_install_dir());
	strcat(locale_dir, G_DIR_SEPARATOR_S "locale");
	return (char*)&locale_dir;
}

GdkFilterReturn wgaim_window_filter( GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	MSG *msg = (MSG*)xevent;

	switch( msg->message ) {
	case WM_SYSCOMMAND:
		if( msg->wParam == SC_MINIMIZE ) {
			MinimizeWndToTray(msg->hwnd);
			ShowNotifyIcon(msg->hwnd,TRUE);
			
			SetWindowLong(msg->hwnd,DWL_MSGRESULT,0);
			return GDK_FILTER_REMOVE;
		}
		break;
	case WM_CLOSE:
		MinimizeWndToTray(msg->hwnd);
		ShowNotifyIcon(msg->hwnd,TRUE);
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

void wgaim_init(void) {
	/* Filter to catch systray events */
	gdk_add_client_message_filter (GDK_POINTER_TO_ATOM (WM_TRAYMESSAGE),
				       traymsg_filter_func,
				       NULL);
}

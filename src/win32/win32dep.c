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
#include <gdk/gdkwin32.h>
#include "gaim.h"

#include "stdafx.h"
#include "resource.h"
#include "MinimizeToTray.h"
#include "systray.h"
#include "winuser_extra.h"
#include "IdleTracker.h"

/*
 *  DEFINES & MACROS
 */

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
HINSTANCE gaimexe_hInstance = 0;
HINSTANCE gaimdll_hInstance = 0;

/*
 *  STATIC CODE
 */



/*
 *  PUBLIC CODE
 */

/* Misc Wingaim functions */
HINSTANCE wgaim_hinstance(void) {
	return gaimexe_hInstance;
}

/* Determine Gaim Paths during Runtime */

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

/* Miscellaneous */

/* FlashWindowEx is only supported by Win98+ and WinNT5+ */
void wgaim_im_blink(GtkWidget *window) {
	FLASHWINFO info;

	info.cbSize = sizeof(FLASHWINFO);
	info.hwnd = GDK_WINDOW_HWND(window->window);
	info.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
	info.dwTimeout = 0;
	FlashWindowEx(&info);
}

/* Windows Initializations */

void wgaim_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	char* locale=0;
	char newenv[128];

	debug_printf("wgaim_init\n");

	/* Initialize Wingaim systray icon */
	wgaim_systray_init();

	/*
	 *  Winsock init
	 */
	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		return 1;
	}

	/* Confirm that the winsock DLL supports 2.2 */
	/* Note that if the DLL supports versions greater than
	   2.2 in addition to 2.2, it will still return 2.2 in 
	   wVersion since that is the version we requested. */

	if ( LOBYTE( wsaData.wVersion ) != 2 ||
			HIBYTE( wsaData.wVersion ) != 2 ) {
		debug_printf("Could not find a usable WinSock DLL.  Oh well.\n");
		WSACleanup( );
		return 1;
	}

	/* get default locale */
	locale = g_win32_getlocale();
	debug_printf("Language profile used: %s\n", locale);

	/*
	 *  Aspell config
	 */
	/* Set LANG env var */
	sprintf(newenv, "LANG=%s", locale);
	if(putenv(newenv)<0)
		debug_printf("putenv failed\n");
	g_free(locale);

	/*
	 *  IdleTracker Initialization
	 */
	if(!IdleTrackerInit())
		debug_printf("IdleTracker failed to initialize\n");
}

/* Windows Cleanup */

void wgaim_cleanup(void) {
	debug_printf("wgaim_cleanup\n");

	/* winsock cleanup */
	WSACleanup( );

	/* IdleTracker cleanup */
	IdleTrackerTerm();
	
	/* Remove systray icon */
	wgaim_systray_cleanup();
}

/* DLL initializer */
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved ) {
	gaimdll_hInstance = hinstDLL;
	return TRUE;
}

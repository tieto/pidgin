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
#include <winuser.h>
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
 * DATA STRUCTS
 */
struct _WGAIM_FLASH_INFO {
	guint t_handle;
	guint sig_handler;
};
typedef struct _WGAIM_FLASH_INFO WGAIM_FLASH_INFO;

/*
 * LOCALS
 */
static char install_dir[MAXPATHLEN];
static char lib_dir[MAXPATHLEN];
static char locale_dir[MAXPATHLEN];
static int bhide_icon;
static int imalpha = 255;

/*
 *  GLOBALS
 */
HINSTANCE gaimexe_hInstance = 0;
HINSTANCE gaimdll_hInstance = 0;

/*
 *  PROTOS
 */
BOOL (*MyFlashWindowEx)(PFLASHWINFO pfwi)=NULL;
BOOL (*MySetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags)=NULL;
void wgaim_set_wintrans(GtkWidget *window, int trans);

/*
 *  STATIC CODE
 */

/* Window flasher */
static gboolean flash_window_cb(gpointer data) {
	FlashWindow((HWND)data, TRUE);
	return TRUE;
}

static void halt_flash_filter(GtkWidget *widget, GdkEventFocus *event, WGAIM_FLASH_INFO *finfo) {
	/* Stop flashing and remove filter */
	debug_printf("Removing timeout\n");
	gtk_timeout_remove(finfo->t_handle);
	debug_printf("Disconnecting signal handler\n");
	g_signal_handler_disconnect(G_OBJECT(widget),finfo->sig_handler);
	debug_printf("done\n");
}

/* Determine whether the specified dll contains the specified procedure.
   If so, load it (if not already loaded). */
static FARPROC find_and_loadproc( char* dllname, char* procedure ) {
	HMODULE hmod;
	int did_load=0;
	FARPROC proc = 0;

	if(!(hmod=GetModuleHandle(dllname))) {
		debug_printf("%s not found. Loading it..\n", dllname);
		if(!(hmod = LoadLibrary(dllname))) {
			debug_printf("Could not load: %s\n", dllname);
			return NULL;
		}
		else
			did_load = 1;
 	}

	if((proc=GetProcAddress(hmod, procedure))) {
		debug_printf("This version of %s contains %s\n", dllname, procedure);
		return proc;
	}
	else {
		debug_printf("Function: %s not found in dll: %s\n", procedure, dllname);
		if(did_load) {
			/* unload dll */
			FreeLibrary(hmod);
		}
		return NULL;
	}
}

static void load_winver_specific_procs(void) {
	/* Used for Win98+ and WinNT5+ */
	MyFlashWindowEx = (void*)find_and_loadproc("user32.dll", "FlashWindowEx" );
	/* Used for Win2k+ */
	MySetLayeredWindowAttributes = (void*)find_and_loadproc("user32.dll", "SetLayeredWindowAttributes" );
}

/* Transparency slider callbacks */
static void change_im_alpha(GtkWidget *w, gpointer data) {
	wgaim_set_wintrans(GTK_WIDGET(data), gtk_range_get_value(GTK_RANGE(w)));
}

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

/* FlashWindowEx is only supported by Win98+ and WinNT5+. If its
   not supported we do it our own way */
void wgaim_im_blink(GtkWidget *window) {
	if(MyFlashWindowEx) {
		FLASHWINFO info;

		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = GDK_WINDOW_HWND(window->window);
		info.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
		info.dwTimeout = 0;
		MyFlashWindowEx(&info);
	}
	else {
		WGAIM_FLASH_INFO *finfo = g_new0(WGAIM_FLASH_INFO, 1);

		/* Start Flashing window */
		finfo->t_handle = gtk_timeout_add(1000, flash_window_cb, GDK_WINDOW_HWND(window->window));
		finfo->sig_handler = g_signal_connect(G_OBJECT(window), "focus-in-event", G_CALLBACK(halt_flash_filter), finfo);
	}
}

/* Set window transparency level */
void wgaim_set_wintrans(GtkWidget *window, int trans) {
	if(MySetLayeredWindowAttributes) {
		HWND hWnd = GDK_WINDOW_HWND(window->window);
		SetWindowLong(hWnd,GWL_EXSTYLE,GetWindowLong(hWnd,GWL_EXSTYLE) | WS_EX_LAYERED);
		MySetLayeredWindowAttributes(hWnd,0,trans,LWA_ALPHA);
	}
}

void wgaim_set_imalpha(int val) {
	imalpha = val;
}

int wgaim_get_imalpha(int val) {
	return imalpha;
}

int wgaim_has_transparency() {
	return MySetLayeredWindowAttributes ? TRUE : FALSE;
}

GtkWidget *wgaim_wintrans_slider(GtkWidget *win) {
	GtkWidget *hbox;
	GtkWidget *label, *slider;

	hbox = gtk_hbox_new(FALSE, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_hscale_new_with_range(50,255,1);
	gtk_range_set_value(GTK_RANGE(slider), imalpha);
	gtk_widget_set_usize(GTK_WIDGET(slider), 200, -1);
	
	/* On slider val change, update window's transparency level */
	gtk_signal_connect(GTK_OBJECT(slider), "value-changed", GTK_SIGNAL_FUNC(change_im_alpha), win);
	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	/* Set the initial transparency level */
	wgaim_set_wintrans(win, imalpha);

	gtk_widget_show_all(hbox);

	return hbox;
}

/* Windows Initializations */

void wgaim_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	char* locale=0;
	char newenv[128];

	debug_printf("wgaim_init\n");

	load_winver_specific_procs();

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

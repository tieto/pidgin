/*
 * gaim
 *
 * File: win32dep.c
 * Date: June, 2002
 * Description: Windows dependant code for Gaim
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <winuser.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <gdk/gdkwin32.h>

#include "gaim.h"
#include "debug.h"
#include "notify.h"

#include "stdafx.h"
#include "resource.h"
#include "MinimizeToTray.h"
#include "winuser_extra.h"
#include "idletrack.h"
#include "zlib.h"
#include "untar.h"

#  include <libintl.h>
#  define _(x) gettext(x)

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
static char app_data_dir[MAX_PATH];
static char install_dir[MAXPATHLEN];
static char lib_dir[MAXPATHLEN];
static char locale_dir[MAXPATHLEN];
static gboolean blink_turned_on = TRUE;

/*
 *  GLOBALS
 */
HINSTANCE gaimexe_hInstance = 0;
HINSTANCE gaimdll_hInstance = 0;

/*
 *  PROTOS
 */
BOOL (*MyFlashWindowEx)(PFLASHWINFO pfwi)=NULL;
HRESULT (*SHGetFolderPath)(HWND, int, HANDLE, DWORD, LPTSTR) = NULL;

FARPROC wgaim_find_and_loadproc(char*, char*);
extern void wgaim_gtkspell_init();

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
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Removing timeout\n");
	g_source_remove(finfo->t_handle);
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Disconnecting signal handler\n");
	g_signal_handler_disconnect(G_OBJECT(widget),finfo->sig_handler);
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "done\n");
}

static void load_winver_specific_procs(void) {
	/* Used for Win98+ and WinNT5+ */
	MyFlashWindowEx = (void*)wgaim_find_and_loadproc("user32.dll", "FlashWindowEx" );
}

/*
 *  PUBLIC CODE
 */

void wgaim_set_hinstance(HINSTANCE hint) {
	gaimexe_hInstance = hint;
}

HINSTANCE wgaim_hinstance(void) {
	return gaimexe_hInstance;
}

/* Escape windows dir separators.  This is needed when paths are saved,
   and on being read back have their '\' chars used as an escape char.
   Returns an allocated string which needs to be freed.
*/
char* wgaim_escape_dirsep( char* filename ) {
	int sepcount=0;
	char* ret=NULL;
	int cnt=0;

	ret = filename;
	while(*ret) {
		if(*ret == '\\')
			sepcount++;
		ret++;
	}
	ret = g_malloc0(strlen(filename) + sepcount + 1);
	while(*filename) {
		ret[cnt] = *filename;
		if(*filename == '\\')
			ret[++cnt] = '\\';
		filename++;
		cnt++;
	}
	ret[cnt] = '\0';
	return ret;
}

/* Determine whether the specified dll contains the specified procedure.
   If so, load it (if not already loaded). */
FARPROC wgaim_find_and_loadproc( char* dllname, char* procedure ) {
	HMODULE hmod;
	int did_load=0;
	FARPROC proc = 0;

	if(!(hmod=GetModuleHandle(dllname))) {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "%s not found. Loading it..\n", dllname);
		if(!(hmod = LoadLibrary(dllname))) {
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "Could not load: %s\n", dllname);
			return NULL;
		}
		else
			did_load = 1;
 	}

	if((proc=GetProcAddress(hmod, procedure))) {
		gaim_debug(GAIM_DEBUG_INFO, "wgaim", "This version of %s contains %s\n",
			     dllname, procedure);
		return proc;
	}
	else {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "Function %s not found in dll %s\n", 
			     procedure, dllname);
		if(did_load) {
			/* unload dll */
			FreeLibrary(hmod);
		}
		return NULL;
	}
}

/* Determine Gaim Paths during Runtime */

char* wgaim_install_dir(void) {
	HMODULE hmod;
	char* buf;

	hmod = GetModuleHandle(NULL);
	if( hmod == 0 ) {
		buf = g_win32_error_message( GetLastError() );
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "GetModuleHandle error: %s\n", buf);
		free(buf);
		return NULL;
	}
	if(GetModuleFileName( hmod, (char*)&install_dir, MAXPATHLEN ) == 0) {
		buf = g_win32_error_message( GetLastError() );
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "GetModuleFileName error: %s\n", buf);
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

char* wgaim_data_dir(void) {
        return (char*)&app_data_dir;
}

/* Miscellaneous */

gboolean wgaim_read_reg_string(HKEY key, char* sub_key, char* val_name, LPBYTE data, LPDWORD data_len) {
        HKEY hkey;
        gboolean ret = FALSE;

        if(ERROR_SUCCESS == RegOpenKeyEx(key, 
                                         sub_key, 
					 0,  KEY_QUERY_VALUE, &hkey)) {
                if(ERROR_SUCCESS == RegQueryValueEx(hkey, val_name, 0, NULL, data, data_len))
                        ret = TRUE;
                RegCloseKey(key);
        }
        return ret;
}

/* FlashWindowEx is only supported by Win98+ and WinNT5+. If its
   not supported we do it our own way */
void wgaim_conv_im_blink(GtkWidget *window) {
        if(!blink_turned_on)
                return;
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
		finfo->t_handle = g_timeout_add(1000, 
						flash_window_cb, 
						GDK_WINDOW_HWND(window->window));
		finfo->sig_handler = g_signal_connect(G_OBJECT(window), 
						      "focus-in-event", 
						      G_CALLBACK(halt_flash_filter), finfo);
	}
}

void wgaim_conv_im_blink_state(gboolean val) {
        blink_turned_on = val;
}

int wgaim_gz_decompress(const char* in, const char* out) {
	gzFile fin;
	FILE *fout;
	char buf[1024];
	int ret;

	if((fin = gzopen(in, "rb"))) {
		if(!(fout = fopen(out, "wb"))) {
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_decompress", "Error opening file: %s\n", out);
			gzclose(fin);
			return 0;
		}
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_decompress", "gzopen failed to open: %s\n", in);
		return 0;
	}

	while((ret=gzread(fin, buf, 1024))) {
		if(fwrite(buf, 1, ret, fout) < ret) {
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_decompress", "Error writing %d bytes to file\n", ret);
			gzclose(fin);
			fclose(fout);
			return 0;
		}
	}
	fclose(fout);
	gzclose(fin);

	if(ret < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_decompress", "gzread failed while reading: %s\n", in);
		return 0;
	}

	return 1;
}

int wgaim_gz_untar(const char* filename, const char* destdir) {
	char tmpfile[_MAX_PATH];
	char template[]="wgaimXXXXXX";

	sprintf(tmpfile, "%s%s%s", g_get_tmp_dir(), G_DIR_SEPARATOR_S, _mktemp(template));
	if(wgaim_gz_decompress(filename, tmpfile)) {
		int ret;
		if(untar(tmpfile, destdir, UNTAR_FORCE | UNTAR_QUIET))
			ret=1;
		else {
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_untar", "Failure untaring %s\n", tmpfile);
			ret=0;
		}
		unlink(tmpfile);
		return ret;
	}
	else {
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim_gz_untar", "Failed to gz decompress %s\n", filename);
		return 0;
	}
}

/* Moved over from old systray.c */
void wgaim_systray_minimize( GtkWidget *window ) {
	MinimizeWndToTray(GDK_WINDOW_HWND(window->window));
}

void wgaim_systray_maximize( GtkWidget *window ) {
	RestoreWndFromTray(GDK_WINDOW_HWND(window->window));
}

char* wgaim_lcid_to_posix(LCID lcid) {
        switch(lcid) {
        case 1026: return "bg"; /* bulgarian */
        case 1027: return "ca"; /* catalan */
        case 1050: return "hr"; /* croation */
        case 1029: return "cs"; /* czech */
        case 1030: return "da"; /* danaish */
        case 1043: return "nl"; /* dutch - netherlands */
        case 1033: return "en"; /* english - us */
        case 1035: return "fi"; /* finish */
        case 1036: return "fr"; /* french - france */
        case 1031: return "de"; /* german - germany */
        case 1032: return "el"; /* greek */
        case 1037: return "he"; /* hebrew */
        case 1038: return "hu"; /* hungarian */
        case 1040: return "it"; /* italian - italy */
        case 1041: return "ja"; /* japanese */
        case 1042: return "ko"; /* korean */
        case 1063: return "lt"; /* lithuanian */
        case 1071: return "mk"; /* macedonian */
        case 1045: return "pl"; /* polish */
        case 2070: return "pt"; /* portuguese - portugal */
        case 1046: return "pt_BR"; /* portuguese - brazil */
        case 1048: return "ro"; /* romanian - romania */
        case 1049: return "ru"; /* russian - russia */
        case 2074: return "sr@Latn"; /* serbian - latin */
        case 3098: return "sr"; /* serbian - cyrillic */
        case 2052: return "zh_CN"; /* chinese - china (simple) */
        case 1051: return "sk"; /* slovak */
        case 1060: return "sl"; /* slovenian */
        case 1034: return "es"; /* spanish */
        case 1053: return "sv"; /* swedish */
        case 1054: return "th"; /* thai */
        case 1028: return "zh_TW"; /* chinese - taiwan (traditional) */
        case 1055: return "tr"; /* turkish */
        case 1058: return "uk"; /* ukrainian */
        default:
                gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "Could not find posix code for LCID: %d\n", lcid);
                return NULL;
        }
}

/* Determine and set Gaim locale as follows (in order of priority):
   - Check LANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
void wgaim_set_locale() {
        HKEY hkey;
	char* locale=NULL;
        char envstr[25];
        LCID lcid;

        /* Check if user set LANG env var */
        if((locale = (char*)g_getenv("LANG"))) {
                gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Using locale set by the LANG env var.\n");
                goto finish;
        }

        /* Check reg key set at install time */
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, 
                                         "SOFTWARE\\gaim", 
					 0,  KEY_QUERY_VALUE, &hkey)) {
                BYTE data[10];
                DWORD ds = 10;
                if(ERROR_SUCCESS == RegQueryValueEx(hkey, "Installer Language", 0, NULL, (LPBYTE)&data, &ds)) {
			gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Using locale set by the installer\n");
                        if((locale = wgaim_lcid_to_posix(atoi(data))))
                                goto finish;
		}
        }

        lcid = GetUserDefaultLCID();
        if((locale = wgaim_lcid_to_posix(lcid)))
                goto finish;

        finish:
        if(!locale)
                locale = "en";

        sprintf(envstr, "LANG=%s", locale);
        if(putenv(envstr)<0)
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "putenv failed\n");

	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Locale set to: %s\n", locale);
}

/* Windows Initializations */
typedef enum {
    SHGFP_TYPE_CURRENT  = 0,   // current value for user, verify it exists
    SHGFP_TYPE_DEFAULT  = 1,   // default value, may not exist
} SHGFP_TYPE;

#define CSIDL_APPDATA 0x001a

void wgaim_pre_plugin_init(void) {
        char *perlenv, *newenv;

        /* Tell perl where to find Gaim's perl modules */
        perlenv = (char*)g_getenv("PERL5LIB");
        newenv = g_strdup_printf("PERL5LIB=%s%s%s%s",
                                 perlenv ? perlenv : "", 
                                 perlenv ? ";" : "", 
                                 wgaim_install_dir(), 
                                 "\\perlmod;");
        if(putenv(newenv)<0)
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "putenv failed\n");
        g_free(newenv);

        /* Set app data dir, where to save Gaim user settings */
        newenv = (char*)g_getenv("HOME");
        if(!newenv) {
                SHGetFolderPath = (void*)wgaim_find_and_loadproc("shfolder.dll", "SHGetFolderPathA");
                if(SHGetFolderPath) {
                        HRESULT hrResult = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, app_data_dir);
                        if(hrResult != S_FALSE && hrResult != E_FAIL && hrResult != E_INVALIDARG) {
                                gaim_debug(GAIM_DEBUG_INFO, "wgaim", "APP DATA PATH set to: %s\n", app_data_dir);
                        }
                        else
                                strcpy(app_data_dir, "C:");
                }

                /* As of 0.69, using SHGetFolderPath to determine app settings directory.
                   Move app settings to new location if need be. */
                { 
                        char *old_home = g_strdup_printf("%s%s", g_get_home_dir() ? g_get_home_dir() : "C:", "\\.gaim");
                        char *new_home = g_strdup_printf("%s\\.gaim", wgaim_data_dir());
                        GDir *dir_old, *dir_new;

                        dir_old = g_dir_open(old_home, 0, NULL);
                        dir_new = g_dir_open(new_home, 0, NULL);
                        if(dir_old && !dir_new) {
                                gaim_notify_message(NULL, 
                                                    GAIM_NOTIFY_MSG_INFO,
                                                    _("Notification"), 
                                                    _("Moving Gaim user settings directory to:"),
                                                    new_home,
                                                    NULL,
                                                    NULL);
                                if(MoveFile(old_home, new_home) != 0)
                                        gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Success moving '.gaim' directory\n");
                        }
                        g_free(new_home);
                        g_free(old_home);
                        if(dir_old) g_dir_close(dir_old);
                        if(dir_new) g_dir_close(dir_new);
                }
        }
        else {
                strcpy(app_data_dir, newenv);
        }
}

void wgaim_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	char newenv[128];

	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_init\n");

	load_winver_specific_procs();

	/*
	 *  Winsock init
	 */
	wVersionRequested = MAKEWORD( 2, 2 );

	WSAStartup( wVersionRequested, &wsaData );

	/* Confirm that the winsock DLL supports 2.2 */
	/* Note that if the DLL supports versions greater than
	   2.2 in addition to 2.2, it will still return 2.2 in 
	   wVersion since that is the version we requested. */

	if ( LOBYTE( wsaData.wVersion ) != 2 ||
			HIBYTE( wsaData.wVersion ) != 2 ) {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "Could not find a usable WinSock DLL.  Oh well.\n");
		WSACleanup( );
	}

        /* Set locale - determines which translations to user, and which
           aspell dictionary to use */
        wgaim_set_locale();

	/* Disable PANGO UNISCRIBE (for GTK 2.2.0). This may not be necessary in the
	   future because there will most likely be a check to see if we need this,
	   but for now we need to set this in order to avoid poor performance for some 
	   windows machines.
	*/
	sprintf(newenv, "PANGO_WIN32_NO_UNISCRIBE=1");
	if(putenv(newenv)<0)
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "putenv failed\n");

	/*
	 *  IdleTracker Initialization
	 */
	if(!wgaim_set_idlehooks())
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "Failed to initialize idle tracker\n");

	wgaim_gtkspell_init();
}

/* Windows Cleanup */

void wgaim_cleanup(void) {
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_cleanup\n");

	/* winsock cleanup */
	WSACleanup( );

	/* Idle tracker cleanup */
	wgaim_remove_idlehooks();
}

/* DLL initializer */
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved ) {
	gaimdll_hInstance = hinstDLL;
	return TRUE;
}

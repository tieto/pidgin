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

#include <libintl.h>

/*
 *  DEFINES & MACROS
 */
#define _(x) gettext(x)

/*
 * DATA STRUCTS
 */

/* For shfolder.dll */
typedef HRESULT (CALLBACK* LPFNSHGETFOLDERPATH)(HWND, int, HANDLE, DWORD, LPTSTR);

typedef enum {
    SHGFP_TYPE_CURRENT  = 0,   // current value for user, verify it exists
    SHGFP_TYPE_DEFAULT  = 1,   // default value, may not exist
} SHGFP_TYPE;

#define CSIDL_APPDATA 0x001a
#define CSIDL_FLAG_CREATE 0x8000

/* flash info */
typedef BOOL (CALLBACK* LPFNFLASHWINDOWEX)(PFLASHWINFO);

struct _WGAIM_FLASH_INFO {
	guint t_handle;
	guint sig_handler;
};
typedef struct _WGAIM_FLASH_INFO WGAIM_FLASH_INFO;

/*
 * LOCALS
 */
static char app_data_dir[MAX_PATH] = "C:";
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
LPFNFLASHWINDOWEX MyFlashWindowEx = NULL;
LPFNSHGETFOLDERPATH MySHGetFolderPath = NULL;

FARPROC wgaim_find_and_loadproc(char*, char*);
extern void wgaim_gtkspell_init();
char* wgaim_data_dir(void);

/*
 *  STATIC CODE
 */

/* Window flasher */
static gboolean flash_window_cb(gpointer data) {
	FlashWindow((HWND)data, TRUE);
	return TRUE;
}

static int halt_flash_filter(GtkWidget *widget, GdkEventFocus *event, gpointer data) {
        if(MyFlashWindowEx) {
                HWND hWnd = data;
                FLASHWINFO info;

                if(!IsWindow(hWnd))
                        return 0;
                memset(&info, 0, sizeof(FLASHWINFO));
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = hWnd;
		info.dwFlags = FLASHW_STOP;
		info.dwTimeout = 0;
		MyFlashWindowEx(&info);
        }
        else {
                WGAIM_FLASH_INFO *finfo = data;        
                /* Stop flashing and remove filter */
                gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Removing timeout\n");
                gaim_timeout_remove(finfo->t_handle);
                gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Disconnecting signal handler\n");
                g_signal_handler_disconnect(G_OBJECT(widget),finfo->sig_handler);
                gaim_debug(GAIM_DEBUG_INFO, "wgaim", "done\n");
                g_free(finfo);
        }
	return 0;
}

static void load_winver_specific_procs(void) {
	/* Used for Win98+ and WinNT5+ */
	MyFlashWindowEx = (LPFNFLASHWINDOWEX)wgaim_find_and_loadproc("user32.dll", "FlashWindowEx" );
}

static char* base_name(char* path) {
        char *tmp = path;
        char *prev = NULL;

        while((tmp=strchr(tmp, '\\'))) {
                prev = tmp;
                tmp += 1;
        }
        if(prev)
                return ++prev;
        else
                return NULL;
}

BOOL folder_exists(char *folder) {
        BOOL ret = FALSE;
        WIN32_FIND_DATA fileinfo;
        HANDLE fh;

        memset(&fileinfo, 0, sizeof(WIN32_FIND_DATA));
        if((fh=FindFirstFile(folder, &fileinfo))) {
                if(fileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        ret = TRUE;
                        SetLastError(ERROR_SUCCESS);
                }
                else
                        SetLastError(ERROR_FILE_EXISTS);
                FindClose(fh);
        }
        return ret;
}

/* Recursively create directories in the dest path */
static BOOL CreateDirectoryR(char *dest) {
        static BOOL start = TRUE;
        BOOL ret = FALSE;

        if(!dest)
                return ret;

        if(start) {
                char *str = g_strdup(dest);
                start = FALSE;
                ret = CreateDirectoryR(str);
                g_free(str);
                start = TRUE;
        }
        else {
                char *tmp1 = dest;
                char *tmp=NULL;

                while((tmp1=strchr(tmp1, '\\'))) {
                        tmp = tmp1;
                        tmp1+=1;
                }
                
                if(tmp) {
                        tmp[0] = '\0';
                        CreateDirectoryR(dest);
                        tmp[0] = '\\';
                        if(CreateDirectory(dest, NULL) == 0 && GetLastError() != ERROR_ALREADY_EXISTS) {
                                gaim_debug(GAIM_DEBUG_ERROR, "wgaim",
                                           "Error creating directory: %s. Errno: %u\n", 
                                           dest, (UINT)GetLastError());
                        }
                        else
                                ret = TRUE;
                }
        }
        return ret;
}

#if 0
static BOOL move_folder(char *src, char* dest, char* copytitle, BOOL overwrite) {
        char *tsrc, *tdest;
        SHFILEOPSTRUCT dirmove;
        BOOL ret = FALSE;

        g_return_val_if_fail(src!=NULL, ret);
        g_return_val_if_fail(dest!=NULL, ret);

        if(!folder_exists(src)) {
                gaim_debug(GAIM_DEBUG_WARNING, "wgaim", 
                           "move_folder: Source folder %s, does not exist\n", src);
                return ret;
        }
        if(!overwrite) {
                char *dstpath = g_strdup_printf("%s\\%s", dest, base_name(src));

                if(folder_exists(dstpath)) {
                        gaim_debug(GAIM_DEBUG_WARNING, "wgaim", 
                                   "move_folder: Destination Folder %s, already exists\n", dstpath);
                        g_free(dstpath);
                        return ret;
                }
                g_free(dstpath);
        }

        /* Create dest folder if it doesn't exist */
        if(!CreateDirectoryR(dest)) {
                gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "Error creating directory: %s\n", dest);
                return ret;
        }

        tsrc = g_strdup_printf("%s%c", src, '\0');
        tdest = g_strdup_printf("%s%c", dest, '\0');

        memset(&dirmove, 0, sizeof(SHFILEOPSTRUCT));
        dirmove.wFunc = FO_MOVE;
        dirmove.pFrom = tsrc;
        dirmove.pTo = tdest;
        dirmove.fFlags = FOF_NOCONFIRMATION | FOF_SIMPLEPROGRESS;
        dirmove.hNameMappings = 0;
        dirmove.lpszProgressTitle = copytitle;

        if(SHFileOperation(&dirmove)==0)
                ret = TRUE;

        g_free(tsrc);
        g_free(tdest);
        return ret;
}
#endif

static void wgaim_debug_print(GaimDebugLevel level, const char *category, const char *format, va_list args) {
        char *str = g_strdup_vprintf(format, args);
        printf("%s%s%s", category?category:"", category?": ":"",str);
        g_free(str);
}

static GaimDebugUiOps ops =  {
	wgaim_debug_print
};

/*
 *  PUBLIC CODE
 */

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
		g_free(buf);
		return NULL;
	}
	if(GetModuleFileName( hmod, (char*)&install_dir, MAXPATHLEN ) == 0) {
		buf = g_win32_error_message( GetLastError() );
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "GetModuleFileName error: %s\n", buf);
		g_free(buf);
		return NULL;
	}
	buf = g_path_get_dirname( install_dir );
	strcpy( (char*)&install_dir, buf );
	g_free( buf );

	return (char*)&install_dir;
}

char* wgaim_lib_dir(void) {
	strcpy(lib_dir, wgaim_install_dir());
	g_strlcat(lib_dir, G_DIR_SEPARATOR_S "plugins", sizeof(lib_dir));
	return (char*)&lib_dir;
}

char* wgaim_locale_dir(void) {
	strcpy(locale_dir, wgaim_install_dir());
	g_strlcat(locale_dir, G_DIR_SEPARATOR_S "locale", sizeof(locale_dir));
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
                if(GetForegroundWindow() == GDK_WINDOW_HWND(window->window))
                        return;
                memset(&info, 0, sizeof(FLASHWINFO));
		info.cbSize = sizeof(FLASHWINFO);
		info.hwnd = GDK_WINDOW_HWND(window->window);
		info.dwFlags = FLASHW_ALL | FLASHW_TIMER;
		info.dwTimeout = 0;
		MyFlashWindowEx(&info);
                /* Stop flashing when window receives focus */
                g_signal_connect(G_OBJECT(window), 
                                 "focus-in-event", 
                                 G_CALLBACK(halt_flash_filter), info.hwnd);
	}
	else {
		WGAIM_FLASH_INFO *finfo = g_new0(WGAIM_FLASH_INFO, 1);

		/* Start Flashing window */
		finfo->t_handle = gaim_timeout_add(1000, 
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

void wgaim_init(HINSTANCE hint) {
	WORD wVersionRequested;
	WSADATA wsaData;
	char *perlenv;
        char *newenv;

        gaim_debug_set_ui_ops(&ops);
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_init start\n");

	gaimexe_hInstance = hint;

	load_winver_specific_procs();

	/* Winsock init */
	wVersionRequested = MAKEWORD( 2, 2 );
	WSAStartup( wVersionRequested, &wsaData );

	/* Confirm that the winsock DLL supports 2.2 */
	/* Note that if the DLL supports versions greater than
	   2.2 in addition to 2.2, it will still return 2.2 in 
	   wVersion since that is the version we requested. */
	if ( LOBYTE( wsaData.wVersion ) != 2 ||
             HIBYTE( wsaData.wVersion ) != 2 ) {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "Could not find a usable WinSock DLL.  Oh well.\n");
		WSACleanup();
	}

        /* Set Environmental Variables */
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

        /* Set app data dir, used by gaim_home_dir */
        newenv = (char*)g_getenv("GAIMHOME");
        if(!newenv) {
                if((MySHGetFolderPath = (LPFNSHGETFOLDERPATH)wgaim_find_and_loadproc("shfolder.dll", "SHGetFolderPathA"))) {
                        MySHGetFolderPath(NULL,
                                          CSIDL_APPDATA, 
                                          NULL, SHGFP_TYPE_CURRENT, app_data_dir);
                }
                else
                        strcpy(app_data_dir, "C:");
        }
        else {
                g_strlcpy(app_data_dir, newenv, sizeof(app_data_dir));
        }
        gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Gaim settings dir: %s\n", app_data_dir);

	/* IdleTracker Initialization */
	if(!wgaim_set_idlehooks())
		gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "Failed to initialize idle tracker\n");

	wgaim_gtkspell_init();
        gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_init end\n");
}

/* Windows Cleanup */

void wgaim_cleanup(void) {
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_cleanup\n");

	/* winsock cleanup */
	WSACleanup();

	/* Idle tracker cleanup */
	wgaim_remove_idlehooks();
}

/* DLL initializer */
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved ) {
	gaimdll_hInstance = hinstDLL;
	return TRUE;
}

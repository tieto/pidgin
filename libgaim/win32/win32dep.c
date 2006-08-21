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
#define _WIN32_IE 0x500
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <winuser.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gaim.h"
#include "debug.h"
#include "notify.h"

#include <libintl.h>

#include "win32dep.h"

/*
 *  DEFINES & MACROS
 */
#define _(x) gettext(x)

/* For shfolder.dll */
typedef HRESULT (CALLBACK* LPFNSHGETFOLDERPATHA)(HWND, int, HANDLE, DWORD, LPSTR);
typedef HRESULT (CALLBACK* LPFNSHGETFOLDERPATHW)(HWND, int, HANDLE, DWORD, LPWSTR);

/*
 * LOCALS
 */
static char *app_data_dir = NULL, *install_dir = NULL,
	*lib_dir = NULL, *locale_dir = NULL;

static HINSTANCE libgaimdll_hInstance = 0;


/*
 *  STATIC CODE
 */

static void wgaim_debug_print(GaimDebugLevel level, const char *category,
		const char *arg_s) {
	if(category)
		printf("%s: %s", category, arg_s);
	else
		printf(arg_s);
}

static GaimDebugUiOps ops = {
	wgaim_debug_print
};

/*
 *  PUBLIC CODE
 */

/* Escape windows dir separators.  This is needed when paths are saved,
   and on being read back have their '\' chars used as an escape char.
   Returns an allocated string which needs to be freed.
*/
char *wgaim_escape_dirsep(const char *filename) {
	int sepcount = 0;
	const char *tmp = filename;
	char *ret;
	int cnt = 0;

	g_return_val_if_fail(filename != NULL, NULL);

	while(*tmp) {
		if(*tmp == '\\')
			sepcount++;
		tmp++;
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
FARPROC wgaim_find_and_loadproc(const char *dllname, const char *procedure) {
	HMODULE hmod;
	BOOL did_load = FALSE;
	FARPROC proc = 0;

	if(!(hmod = GetModuleHandle(dllname))) {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "%s not already loaded; loading it...\n", dllname);
		if(!(hmod = LoadLibrary(dllname))) {
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim", "Could not load: %s\n", dllname);
			return NULL;
		}
		else
			did_load = TRUE;
	}

	if((proc = GetProcAddress(hmod, procedure))) {
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

/* Get paths to special Windows folders. */
char *wgaim_get_special_folder(int folder_type) {
	static LPFNSHGETFOLDERPATHA MySHGetFolderPathA = NULL;
	static LPFNSHGETFOLDERPATHW MySHGetFolderPathW = NULL;
	char *retval = NULL;

	if (!MySHGetFolderPathW) {
		MySHGetFolderPathW = (LPFNSHGETFOLDERPATHW)
			wgaim_find_and_loadproc("shfolder.dll", "SHGetFolderPathW");
	}

	if (MySHGetFolderPathW) {
		wchar_t utf_16_dir[MAX_PATH + 1];

		if (SUCCEEDED(MySHGetFolderPathW(NULL, folder_type, NULL,
						SHGFP_TYPE_CURRENT, utf_16_dir))) {
			retval = g_utf16_to_utf8(utf_16_dir, -1, NULL, NULL, NULL);
		}
	}

	if (!retval) {
		if (!MySHGetFolderPathA) {
			MySHGetFolderPathA = (LPFNSHGETFOLDERPATHA)
				wgaim_find_and_loadproc("shfolder.dll", "SHGetFolderPathA");
		}
		if (MySHGetFolderPathA) {
			char locale_dir[MAX_PATH + 1];

			if (SUCCEEDED(MySHGetFolderPathA(NULL, folder_type, NULL,
							SHGFP_TYPE_CURRENT, locale_dir))) {
				retval = g_locale_to_utf8(locale_dir, -1, NULL, NULL, NULL);
			}
		}
	}

	return retval;
}

const char *wgaim_install_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		char *tmp = NULL;
		if (G_WIN32_HAVE_WIDECHAR_API()) {
			wchar_t winstall_dir[MAXPATHLEN];
			if (GetModuleFileNameW(NULL, winstall_dir,
					MAXPATHLEN) > 0) {
				tmp = g_utf16_to_utf8(winstall_dir, -1,
					NULL, NULL, NULL);
			}
		} else {
			gchar cpinstall_dir[MAXPATHLEN];
			if (GetModuleFileNameA(NULL, cpinstall_dir,
					MAXPATHLEN) > 0) {
				tmp = g_locale_to_utf8(cpinstall_dir,
					-1, NULL, NULL, NULL);
			}
		}

		if (tmp == NULL) {
			tmp = g_win32_error_message(GetLastError());
			gaim_debug(GAIM_DEBUG_ERROR, "wgaim",
				"GetModuleFileName error: %s\n", tmp);
			g_free(tmp);
			return NULL;
		} else {
			install_dir = g_path_get_dirname(tmp);
			g_free(tmp);
			initialized = TRUE;
		}
	}

	return install_dir;
}

const char *wgaim_lib_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		const char *inst_dir = wgaim_install_dir();
		if (inst_dir != NULL) {
			lib_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "plugins", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
	}

	return lib_dir;
}

const char *wgaim_locale_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		const char *inst_dir = wgaim_install_dir();
		if (inst_dir != NULL) {
			locale_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "locale", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
	}

	return locale_dir;
}

const char *wgaim_data_dir(void) {

	if (!app_data_dir) {
		/* Set app data dir, used by gaim_home_dir */
		const char *newenv = g_getenv("GAIMHOME");
		if (newenv)
			app_data_dir = g_strdup(newenv);
		else {
			app_data_dir = wgaim_get_special_folder(CSIDL_APPDATA);
			if (!app_data_dir)
				app_data_dir = g_strdup("C:");
		}
		gaim_debug(GAIM_DEBUG_INFO, "wgaim", "Gaim settings dir: %s\n",
			app_data_dir);
	}

	return app_data_dir;
}

/* Miscellaneous */

gboolean wgaim_write_reg_string(HKEY rootkey, const char *subkey, const char *valname,
		const char *value) {
	HKEY reg_key;
	gboolean success = FALSE;

	if(G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wc_subkey = g_utf8_to_utf16(subkey, -1, NULL,
			NULL, NULL);

		if(RegOpenKeyExW(rootkey, wc_subkey, 0,
				KEY_SET_VALUE, &reg_key) == ERROR_SUCCESS) {
			wchar_t *wc_valname = NULL;

			if (valname)
				wc_valname = g_utf8_to_utf16(valname, -1,
					NULL, NULL, NULL);

			if(value) {
				wchar_t *wc_value = g_utf8_to_utf16(value, -1,
					NULL, NULL, NULL);
				int len = (wcslen(wc_value) * sizeof(wchar_t)) + 1;
				if(RegSetValueExW(reg_key, wc_valname, 0, REG_SZ,
						(LPBYTE)wc_value, len
						) == ERROR_SUCCESS)
					success = TRUE;
				g_free(wc_value);
			} else
				if(RegDeleteValueW(reg_key, wc_valname) ==  ERROR_SUCCESS)
					success = TRUE;

			g_free(wc_valname);
		}
		g_free(wc_subkey);
	} else {
		char *cp_subkey = g_locale_from_utf8(subkey, -1, NULL,
			NULL, NULL);
		if(RegOpenKeyExA(rootkey, cp_subkey, 0,
				KEY_SET_VALUE, &reg_key) == ERROR_SUCCESS) {
			char *cp_valname = NULL;
			if(valname)
				cp_valname = g_locale_from_utf8(valname, -1,
					NULL, NULL, NULL);

			if (value) {
				char *cp_value = g_locale_from_utf8(value, -1,
					NULL, NULL, NULL);
				int len = strlen(cp_value) + 1;
				if(RegSetValueExA(reg_key, cp_valname, 0, REG_SZ,
						cp_value, len
						) == ERROR_SUCCESS)
					success = TRUE;
				g_free(cp_value);
			} else
				if(RegDeleteValueA(reg_key, cp_valname) ==  ERROR_SUCCESS)
					success = TRUE;

			g_free(cp_valname);
		}
		g_free(cp_subkey);
	}

	if(reg_key != NULL)
		RegCloseKey(reg_key);

	return success;
}

char *wgaim_read_reg_string(HKEY rootkey, const char *subkey, const char *valname) {

	DWORD type;
	DWORD nbytes;
	HKEY reg_key;
	char *result = NULL;

	if(G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wc_subkey = g_utf8_to_utf16(subkey, -1, NULL,
			NULL, NULL);

		if(RegOpenKeyExW(rootkey, wc_subkey, 0,
				KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS) {
			wchar_t *wc_valname = NULL;
			if (valname)
				wc_valname = g_utf8_to_utf16(valname, -1,
					NULL, NULL, NULL);

			if(RegQueryValueExW(reg_key, wc_valname, 0, &type,
					NULL, &nbytes) == ERROR_SUCCESS
					&& type == REG_SZ) {
				wchar_t *wc_temp =
					g_new(wchar_t, ((nbytes + 1) / sizeof(wchar_t)) + 1);

				if(RegQueryValueExW(reg_key, wc_valname, 0,
						&type, (LPBYTE) wc_temp,
						&nbytes) == ERROR_SUCCESS) {
					wc_temp[nbytes / sizeof(wchar_t)] = '\0';
					result = g_utf16_to_utf8(wc_temp, -1,
						NULL, NULL, NULL);
				}
				g_free(wc_temp);
			}
			g_free(wc_valname);
		}
		g_free(wc_subkey);
	} else {
		char *cp_subkey = g_locale_from_utf8(subkey, -1, NULL,
			NULL, NULL);
		if(RegOpenKeyExA(rootkey, cp_subkey, 0,
				KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS) {
			char *cp_valname = NULL;
			if(valname)
				cp_valname = g_locale_from_utf8(valname, -1,
					NULL, NULL, NULL);

			if(RegQueryValueExA(reg_key, cp_valname, 0, &type,
					NULL, &nbytes) == ERROR_SUCCESS
					&& type == REG_SZ) {
				char *cp_temp = g_malloc(nbytes + 1);

				if(RegQueryValueExA(reg_key, cp_valname, 0,
						&type, cp_temp,
						&nbytes) == ERROR_SUCCESS) {
					cp_temp[nbytes] = '\0';
					result = g_locale_to_utf8(cp_temp, -1,
						NULL, NULL, NULL);
				}
				g_free (cp_temp);
			}
			g_free(cp_valname);
		}
		g_free(cp_subkey);
	}

	if(reg_key != NULL)
		RegCloseKey(reg_key);

	return result;
}

void wgaim_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	const char *perlenv;
	char *newenv;

	gaim_debug_set_ui_ops(&ops);
	gaim_debug_info("wgaim", "wgaim_init start\n");

	gaim_debug_info("wgaim", "Glib:%u.%u.%u\n",
		glib_major_version, glib_minor_version, glib_micro_version);

	/* Winsock init */
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	/* Confirm that the winsock DLL supports 2.2 */
	/* Note that if the DLL supports versions greater than
	   2.2 in addition to 2.2, it will still return 2.2 in
	   wVersion since that is the version we requested. */
	if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "Could not find a usable WinSock DLL.  Oh well.\n");
		WSACleanup();
	}

	/* Set Environmental Variables */
	/* Tell perl where to find Gaim's perl modules */
	perlenv = g_getenv("PERL5LIB");
	newenv = g_strdup_printf("%s%s%s" G_DIR_SEPARATOR_S "perlmod;",
		perlenv ? perlenv : "",
		perlenv ? ";" : "",
		wgaim_install_dir());
	if (!g_setenv("PERL5LIB", newenv, TRUE))
		gaim_debug(GAIM_DEBUG_WARNING, "wgaim", "putenv failed\n");
	g_free(newenv);

	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_init end\n");
}

/* Windows Cleanup */

void wgaim_cleanup(void) {
	gaim_debug(GAIM_DEBUG_INFO, "wgaim", "wgaim_cleanup\n");

	/* winsock cleanup */
	WSACleanup();

	g_free(app_data_dir);
	app_data_dir = NULL;

	libgaimdll_hInstance = NULL;
}

/* DLL initializer */
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved ) {
	libgaimdll_hInstance = hinstDLL;
	return TRUE;
}

/*
 * purple
 *
 * File: win32dep.c
 * Date: June, 2002
 * Description: Windows dependant code for Purple
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

#include "debug.h"
#include "notify.h"

#include <libintl.h>

#include "win32dep.h"

/*
 *  DEFINES & MACROS
 */
#define _(x) gettext(x)

#define WIN32_PROXY_REGKEY "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"

/* For shfolder.dll */
typedef HRESULT (CALLBACK* LPFNSHGETFOLDERPATHA)(HWND, int, HANDLE, DWORD, LPSTR);
typedef HRESULT (CALLBACK* LPFNSHGETFOLDERPATHW)(HWND, int, HANDLE, DWORD, LPWSTR);

/*
 * LOCALS
 */
static char *app_data_dir = NULL, *install_dir = NULL,
	*lib_dir = NULL, *locale_dir = NULL;

static HINSTANCE libpurpledll_hInstance = 0;

static HANDLE proxy_change_event = NULL;
static HKEY proxy_regkey = NULL;

/*
 *  PUBLIC CODE
 */

/* Escape windows dir separators.  This is needed when paths are saved,
   and on being read back have their '\' chars used as an escape char.
   Returns an allocated string which needs to be freed.
*/
char *wpurple_escape_dirsep(const char *filename) {
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
FARPROC wpurple_find_and_loadproc(const char *dllname, const char *procedure) {
	HMODULE hmod;
	BOOL did_load = FALSE;
	FARPROC proc = 0;

	if(!(hmod = GetModuleHandle(dllname))) {
		purple_debug_warning("wpurple", "%s not already loaded; loading it...\n", dllname);
		if(!(hmod = LoadLibrary(dllname))) {
			purple_debug_error("wpurple", "Could not load: %s\n", dllname);
			return NULL;
		}
		else
			did_load = TRUE;
	}

	if((proc = GetProcAddress(hmod, procedure))) {
		purple_debug_info("wpurple", "This version of %s contains %s\n",
			dllname, procedure);
		return proc;
	}
	else {
		purple_debug_warning("wpurple", "Function %s not found in dll %s\n",
			procedure, dllname);
		if(did_load) {
			/* unload dll */
			FreeLibrary(hmod);
		}
		return NULL;
	}
}

/* Determine Purple Paths during Runtime */

/* Get paths to special Windows folders. */
char *wpurple_get_special_folder(int folder_type) {
	static LPFNSHGETFOLDERPATHA MySHGetFolderPathA = NULL;
	static LPFNSHGETFOLDERPATHW MySHGetFolderPathW = NULL;
	char *retval = NULL;

	if (!MySHGetFolderPathW) {
		MySHGetFolderPathW = (LPFNSHGETFOLDERPATHW)
			wpurple_find_and_loadproc("shfolder.dll", "SHGetFolderPathW");
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
				wpurple_find_and_loadproc("shfolder.dll", "SHGetFolderPathA");
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

const char *wpurple_install_dir(void) {
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
			purple_debug_error("wpurple",
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

const char *wpurple_lib_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		const char *inst_dir = wpurple_install_dir();
		if (inst_dir != NULL) {
			lib_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "plugins", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
	}

	return lib_dir;
}

const char *wpurple_locale_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		const char *inst_dir = wpurple_install_dir();
		if (inst_dir != NULL) {
			locale_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "locale", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
	}

	return locale_dir;
}

const char *wpurple_data_dir(void) {

	if (!app_data_dir) {
		/* Set app data dir, used by purple_home_dir */
		const char *newenv = g_getenv("PURPLEHOME");
		if (newenv)
			app_data_dir = g_strdup(newenv);
		else {
			app_data_dir = wpurple_get_special_folder(CSIDL_APPDATA);
			if (!app_data_dir)
				app_data_dir = g_strdup("C:");
		}
		purple_debug_info("wpurple", "Purple settings dir: %s\n",
			app_data_dir);
	}

	return app_data_dir;
}

/* Miscellaneous */

gboolean wpurple_write_reg_string(HKEY rootkey, const char *subkey, const char *valname,
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

static HKEY _reg_open_key(HKEY rootkey, const char *subkey, REGSAM access) {
	HKEY reg_key = NULL;
	LONG rv;

	if(G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wc_subkey = g_utf8_to_utf16(subkey, -1, NULL,
			NULL, NULL);
		rv = RegOpenKeyExW(rootkey, wc_subkey, 0, access, &reg_key);
		g_free(wc_subkey);
	} else {
		char *cp_subkey = g_locale_from_utf8(subkey, -1, NULL,
			NULL, NULL);
		rv = RegOpenKeyExA(rootkey, cp_subkey, 0, access, &reg_key);
		g_free(cp_subkey);
	}

	if (rv != ERROR_SUCCESS) {
		char *errmsg = g_win32_error_message(rv);
		purple_debug_info("wpurple", "Could not open reg key '%s' subkey '%s'.\nMessage: (%ld) %s\n",
					((rootkey == HKEY_LOCAL_MACHINE) ? "HKLM" :
					 (rootkey == HKEY_CURRENT_USER) ? "HKCU" :
					  (rootkey == HKEY_CLASSES_ROOT) ? "HKCR" : "???"),
					subkey, rv, errmsg);
		g_free(errmsg);
	}

	return reg_key;
}

static gboolean _reg_read(HKEY reg_key, const char *valname, LPDWORD type, LPBYTE data, LPDWORD data_len) {
	LONG rv;

	if(G_WIN32_HAVE_WIDECHAR_API()) {
		wchar_t *wc_valname = NULL;
		if (valname)
			wc_valname = g_utf8_to_utf16(valname, -1, NULL, NULL, NULL);
		rv = RegQueryValueExW(reg_key, wc_valname, 0, type, data, data_len);
		g_free(wc_valname);
	} else {
		char *cp_valname = NULL;
		if(valname)
			cp_valname = g_locale_from_utf8(valname, -1, NULL, NULL, NULL);
		rv = RegQueryValueExA(reg_key, cp_valname, 0, type, data, data_len);
		g_free(cp_valname);
	}

	if (rv != ERROR_SUCCESS) {
		char *errmsg = g_win32_error_message(rv);
		purple_debug_info("wpurple", "Could not read from reg key value '%s'.\nMessage: (%ld) %s\n",
					valname, rv, errmsg);
		g_free(errmsg);
	}

	return (rv == ERROR_SUCCESS);
}

gboolean wpurple_read_reg_dword(HKEY rootkey, const char *subkey, const char *valname, LPDWORD result) {

	DWORD type;
	DWORD nbytes;
	HKEY reg_key = _reg_open_key(rootkey, subkey, KEY_QUERY_VALUE);
	gboolean success = FALSE;

	if(reg_key) {
		if(_reg_read(reg_key, valname, &type, (LPBYTE)result, &nbytes))
			success = TRUE;
		RegCloseKey(reg_key);
	}

	return success;
}

char *wpurple_read_reg_string(HKEY rootkey, const char *subkey, const char *valname) {

	DWORD type;
	DWORD nbytes;
	HKEY reg_key = _reg_open_key(rootkey, subkey, KEY_QUERY_VALUE);
	char *result = NULL;

	if(reg_key) {
		if(_reg_read(reg_key, valname, &type, NULL, &nbytes) && type == REG_SZ) {
			LPBYTE data;
			if(G_WIN32_HAVE_WIDECHAR_API())
				data = (LPBYTE) g_new(wchar_t, ((nbytes + 1) / sizeof(wchar_t)) + 1);
			else
				data = (LPBYTE) g_malloc(nbytes + 1);

			if(_reg_read(reg_key, valname, &type, data, &nbytes)) {
				if(G_WIN32_HAVE_WIDECHAR_API()) {
					wchar_t *wc_temp = (wchar_t*) data;
					wc_temp[nbytes / sizeof(wchar_t)] = '\0';
					result = g_utf16_to_utf8(wc_temp, -1,
						NULL, NULL, NULL);
				} else {
					char *cp_temp = (char*) data;
					cp_temp[nbytes] = '\0';
					result = g_locale_to_utf8(cp_temp, -1,
						NULL, NULL, NULL);
				}
			}
			g_free(data);
		}
		RegCloseKey(reg_key);
	}

	return result;
}

static void wpurple_refresh_proxy(void) {
	gboolean set_proxy = FALSE;
	DWORD enabled = 0;

	wpurple_read_reg_dword(HKEY_CURRENT_USER, WIN32_PROXY_REGKEY,
				"ProxyEnable", &enabled);

	if (enabled & 1) {
		char *c = NULL;
		char *tmp = wpurple_read_reg_string(HKEY_CURRENT_USER, WIN32_PROXY_REGKEY,
				"ProxyServer");

		/* There are proxy settings for several protocols */
		if (tmp && (c = g_strstr_len(tmp, strlen(tmp), "http="))) {
			char *d;
			c += strlen("http=");
			d = strchr(c, ';');
			if (d)
				*d = '\0';
			/* c now points the proxy server (and port) */

		/* There is only a global proxy */
		} else if (tmp && strlen(tmp) > 0 && !strchr(tmp, ';')) {
			c = tmp;
		}

		if (c) {
			purple_debug_info("wpurple", "Setting HTTP Proxy: 'http://%s'\n", c);
			g_setenv("HTTP_PROXY", c, TRUE);
			set_proxy = TRUE;
		}
		g_free(tmp);
	}

	/* If there previously was a proxy set and there isn't one now, clear it */
	if (getenv("HTTP_PROXY") && !set_proxy) {
		purple_debug_info("wpurple", "Clearing HTTP Proxy\n");
		g_unsetenv("HTTP_PROXY");
	}
}

static void watch_for_proxy_changes(void) {
	LONG rv;
	DWORD filter = REG_NOTIFY_CHANGE_NAME |
			REG_NOTIFY_CHANGE_LAST_SET;

	if (!proxy_regkey &&
			!(proxy_regkey = _reg_open_key(HKEY_CURRENT_USER,
				WIN32_PROXY_REGKEY, KEY_NOTIFY))) {
		return;
	}

	if (!(proxy_change_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		char *errmsg = g_win32_error_message(GetLastError());
		purple_debug_error("wpurple", "Unable to watch for proxy changes: %s\n", errmsg);
		g_free(errmsg);
		return;
	}

	rv = RegNotifyChangeKeyValue(proxy_regkey, TRUE, filter, proxy_change_event, TRUE);
	if (rv != ERROR_SUCCESS) {
		char *errmsg = g_win32_error_message(rv);
		purple_debug_error("wpurple", "Unable to watch for proxy changes: %s\n", errmsg);
		g_free(errmsg);
		CloseHandle(proxy_change_event);
		proxy_change_event = NULL;
	}

}

gboolean wpurple_check_for_proxy_changes(void) {
	gboolean changed = FALSE;

	if (proxy_change_event && WaitForSingleObject(proxy_change_event, 0) == WAIT_OBJECT_0) {
		CloseHandle(proxy_change_event);
		proxy_change_event = NULL;
		changed = TRUE;
		wpurple_refresh_proxy();
		watch_for_proxy_changes();
	}

	return changed;
}

void wpurple_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;
	const char *perlenv;
	char *newenv;

	purple_debug_info("wpurple", "wpurple_init start\n");
	purple_debug_info("wpurple", "libpurple version: " VERSION "\n");


	purple_debug_info("wpurple", "Glib:%u.%u.%u\n",
		glib_major_version, glib_minor_version, glib_micro_version);

	/* Winsock init */
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);

	/* Confirm that the winsock DLL supports 2.2 */
	/* Note that if the DLL supports versions greater than
	   2.2 in addition to 2.2, it will still return 2.2 in
	   wVersion since that is the version we requested. */
	if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		purple_debug_error("wpurple", "Could not find a usable WinSock DLL.  Oh well.\n");
		WSACleanup();
	}

	/* Set Environmental Variables */
	/* Tell perl where to find Purple's perl modules */
	perlenv = g_getenv("PERL5LIB");
	newenv = g_strdup_printf("%s%s%s" G_DIR_SEPARATOR_S "perlmod;",
		perlenv ? perlenv : "",
		perlenv ? ";" : "",
		wpurple_install_dir());
	if (!g_setenv("PERL5LIB", newenv, TRUE))
		purple_debug_warning("wpurple", "putenv failed for PERL5LIB\n");
	g_free(newenv);

	if (!g_thread_supported())
		g_thread_init(NULL);

	/* If the proxy server environment variables are already set,
	 * we shouldn't override them */
	if (!getenv("HTTP_PROXY") && !getenv("http_proxy") && !getenv("HTTPPROXY")) {
		wpurple_refresh_proxy();
		watch_for_proxy_changes();
	} else {
		purple_debug_info("wpurple", "HTTP_PROXY env. var already set.  Ignoring win32 Internet Settings.\n");
	}

	purple_debug_info("wpurple", "wpurple_init end\n");
}

/* Windows Cleanup */

void wpurple_cleanup(void) {
	purple_debug_info("wpurple", "wpurple_cleanup\n");

	/* winsock cleanup */
	WSACleanup();

	g_free(app_data_dir);
	app_data_dir = NULL;

	if (proxy_regkey) {
		RegCloseKey(proxy_regkey);
		proxy_regkey = NULL;
	}

	libpurpledll_hInstance = NULL;
}

/* DLL initializer */
/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	libpurpledll_hInstance = hinstDLL;
	return TRUE;
}

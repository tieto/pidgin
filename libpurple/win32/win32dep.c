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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"
#include <winuser.h>

#include "debug.h"
#include "glibcompat.h"
#include "notify.h"

#define MAX_PATH_LEN 2048

/*
 * LOCALS
 */
static char *app_data_dir = NULL, *bin_dir = NULL, *data_dir = NULL,
	*lib_dir = NULL, *locale_dir = NULL, *sysconf_dir = NULL;

static HINSTANCE libpurpledll_hInstance = NULL;

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

	wchar_t *wc_dllname = g_utf8_to_utf16(dllname, -1, NULL, NULL, NULL);

	if(!(hmod = GetModuleHandleW(wc_dllname))) {
		if (purple_debug_is_verbose())
			purple_debug_info("wpurple", "%s not already loaded; loading it...\n", dllname);
		if(!(hmod = LoadLibraryW(wc_dllname))) {
			purple_debug_error("wpurple", "Could not load: %s (%s)\n", dllname,
				g_win32_error_message(GetLastError()));
			g_free(wc_dllname);
			return NULL;
		}
		else
			did_load = TRUE;
	}

	g_free(wc_dllname);
	wc_dllname = NULL;

	if((proc = GetProcAddress(hmod, procedure))) {
		if (purple_debug_is_verbose()) {
			purple_debug_info("wpurple", "This version of %s contains %s\n",
				dllname, procedure);
		}
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
gchar *wpurple_get_special_folder(int folder_type) {
	gchar *retval = NULL;
	wchar_t utf_16_dir[MAX_PATH + 1];

	if (SUCCEEDED(SHGetFolderPathW(NULL, folder_type, NULL,
					SHGFP_TYPE_CURRENT, utf_16_dir))) {
		retval = g_utf16_to_utf8(utf_16_dir, -1, NULL, NULL, NULL);
	}

	return retval;
}

const char *wpurple_bin_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
		char *tmp = NULL;
		wchar_t winstall_dir[MAXPATHLEN];

		/* We might use g_win32_get_package_installation_directory_of_module
		 * here, but we won't because this routine strips bin or lib
		 * part of the path.
		 */
		if (GetModuleFileNameW(libpurpledll_hInstance, winstall_dir,
				MAXPATHLEN) > 0) {
			tmp = g_utf16_to_utf8(winstall_dir, -1,
				NULL, NULL, NULL);
		}

		if (tmp == NULL) {
			tmp = g_win32_error_message(GetLastError());
			purple_debug_error("wpurple",
				"GetModuleFileName error: %s\n", tmp);
			g_free(tmp);
			return NULL;
		} else {
			bin_dir = g_path_get_dirname(tmp);
			g_free(tmp);
			initialized = TRUE;
		}
	}

	return bin_dir;
}

#ifdef USE_WIN32_FHS
static gchar *
wpurple_install_relative_path(const gchar *abspath)
{
	const gchar *bindir = WIN32_FHS_BINDIR;
	const gchar *relpath;
	int i, last_dirsep = -1, bin_esc_cnt;
	gchar *ret;
	GString *bin_esc;

	g_return_val_if_fail(bindir != NULL, NULL);
	g_return_val_if_fail(bindir[0] != '\0', NULL);
	g_return_val_if_fail(abspath != NULL, NULL);
	g_return_val_if_fail(abspath[0] != '\0', NULL);

	/* let's find the common prefix of those paths */
	for (i = 0; bindir[i] == abspath[i]; i++) {
		if (bindir[i] == '\0')
			break;
		if (bindir[i] == '\\' || bindir[i] == '/')
			last_dirsep = i;
	}
	if (bindir[i] == '\0' && (abspath[i] == '\\' || abspath[i] == '/'))
		last_dirsep = i;
	if (abspath[i] == '\0' && (bindir[i] == '\\' || bindir[i] == '/'))
		last_dirsep = i;

	/* there is no common prefix, return absolute path */
	if (last_dirsep == -1)
		return g_strdup(abspath);

	/* let's check, how many dirs we need to go up to the common prefix */
	bin_esc_cnt = 0;
	for (i = last_dirsep; bindir[i]; i++) {
		if (bindir[i] != '\\' && bindir[i] != '/')
			continue;
		if (bindir[i + 1] == '\0') /* trailing dir separator */
			break;
		bin_esc_cnt++;
	}
	bin_esc = g_string_new("");
	for (i = 0; i < bin_esc_cnt; i++)
		g_string_append(bin_esc, ".." G_DIR_SEPARATOR_S);

	/* now, we need to go back deeper into the directory tree */
	relpath = &abspath[last_dirsep];
	if (relpath[0] != '\0')
		relpath++;

	/* - enter bin dir
	 * - escape it to the common prefix
	 * - dive into the abspath dir
	 */
	ret = g_build_filename(wpurple_bin_dir(), bin_esc->str, relpath, NULL);
	g_string_free(bin_esc, TRUE);

	purple_debug_misc("wpurple", "wpurple_install_relative_path(%s) = %s",
		abspath, ret);

	return ret;
}
#endif

const char *
wpurple_data_dir(void) {
#ifdef USE_WIN32_FHS
	static gboolean initialized = FALSE;
	if (initialized)
		return data_dir;
	data_dir = wpurple_install_relative_path(WIN32_FHS_DATADIR);
	initialized = TRUE;

	return data_dir;
#else
	return wpurple_bin_dir();
#endif
}


const char *wpurple_lib_dir(const char *subdir)
{
	static gboolean initialized = FALSE;
	static gchar subpath[MAX_PATH_LEN];

	if (!initialized) {
#ifdef USE_WIN32_FHS
		lib_dir = wpurple_install_relative_path(WIN32_FHS_LIBDIR);
		initialized = TRUE;
#else
		const char *inst_dir = wpurple_bin_dir();
		if (inst_dir != NULL) {
			lib_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "plugins", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
#endif
	}

	if (subdir == NULL)
		return lib_dir;

	g_snprintf(subpath, sizeof(subpath),
		"%s" G_DIR_SEPARATOR_S "%s", lib_dir, subdir);
	return subpath;
}

const char *wpurple_locale_dir(void) {
	static gboolean initialized = FALSE;

	if (!initialized) {
#ifdef USE_WIN32_FHS
		locale_dir = wpurple_install_relative_path(WIN32_FHS_LOCALEDIR);
		initialized = TRUE;
#else
		const char *inst_dir = wpurple_bin_dir();
		if (inst_dir != NULL) {
			locale_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "locale", inst_dir);
			initialized = TRUE;
		} else {
			return NULL;
		}
#endif
	}

	return locale_dir;
}

const char *wpurple_home_dir(void) {

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

const char *wpurple_sysconf_dir(void)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
#ifdef USE_WIN32_FHS
		sysconf_dir = wpurple_install_relative_path(WIN32_FHS_SYSCONFDIR);
#else
		sysconf_dir = wpurple_get_special_folder(CSIDL_COMMON_APPDATA);
#endif
		initialized = TRUE;
	}

	return sysconf_dir;
}

/* Miscellaneous */

gboolean wpurple_write_reg_string(HKEY rootkey, const char *subkey, const char *valname,
		const char *value) {
	HKEY reg_key;
	gboolean success = FALSE;

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

	if(reg_key != NULL)
		RegCloseKey(reg_key);

	return success;
}

static HKEY _reg_open_key(HKEY rootkey, const char *subkey, REGSAM access) {
	HKEY reg_key = NULL;
	LONG rv;

	wchar_t *wc_subkey = g_utf8_to_utf16(subkey, -1, NULL,
		NULL, NULL);
	rv = RegOpenKeyExW(rootkey, wc_subkey, 0, access, &reg_key);

	g_free(wc_subkey);

	if (rv != ERROR_SUCCESS) {
		char *errmsg = g_win32_error_message(rv);
		purple_debug_error("wpurple", "Could not open reg key '%s' subkey '%s'.\nMessage: (%ld) %s\n",
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

	wchar_t *wc_valname = NULL;
	if (valname)
		wc_valname = g_utf8_to_utf16(valname, -1, NULL, NULL, NULL);
	rv = RegQueryValueExW(reg_key, wc_valname, 0, type, data, data_len);
	g_free(wc_valname);

	if (rv != ERROR_SUCCESS) {
		char *errmsg = g_win32_error_message(rv);
		purple_debug_error("wpurple", "Could not read from reg key value '%s'.\nMessage: (%ld) %s\n",
					valname, rv, errmsg);
		g_free(errmsg);
	}

	return (rv == ERROR_SUCCESS);
}

gboolean wpurple_reg_val_exists(HKEY rootkey, const char *subkey, const char *valname)
{
	HKEY hkey;
	LONG retv;
	DWORD index;
	wchar_t name_buffer[100];
	BOOL exists = FALSE;
	wchar_t *wc_valname = NULL;
	wchar_t *wc_subkey;

	if (subkey == NULL)
		return FALSE;

	wc_subkey = g_utf8_to_utf16(subkey, -1, NULL, NULL, NULL);
	retv = RegOpenKeyExW(rootkey, wc_subkey, 0, KEY_ENUMERATE_SUB_KEYS, &hkey);
	g_free(wc_subkey);

	if (retv != ERROR_SUCCESS)
		return FALSE;

	if (valname[0] == '\0' || valname == NULL) {
		RegCloseKey(hkey);
		return TRUE;
	}

	wc_valname = g_utf8_to_utf16(valname, -1, NULL, NULL, NULL);
	index = 0;
	while (TRUE)
	{
		DWORD name_size = sizeof(name_buffer);
		retv = RegEnumValueW(hkey, index++, name_buffer, &name_size,
			NULL, NULL, NULL, NULL);
		if (retv != ERROR_SUCCESS)
			break;
		name_size /= sizeof(wchar_t);
		if (wcsncmp(name_buffer, wc_valname, name_size) == 0) {
			exists = TRUE;
			break;
		}
	}
	g_free(wc_valname);

	RegCloseKey(hkey);
	return exists;
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
			LPBYTE data = (LPBYTE) g_new(wchar_t, ((nbytes + 1) / sizeof(wchar_t)) + 1);

			if(_reg_read(reg_key, valname, &type, data, &nbytes)) {
				wchar_t *wc_temp = (wchar_t*) data;
				wc_temp[nbytes / sizeof(wchar_t)] = '\0';
				result = g_utf16_to_utf8(wc_temp, -1,
					NULL, NULL, NULL);
			}
			g_free(data);
		}
		RegCloseKey(reg_key);
	}

	return result;
}

int wpurple_input_pipe(int pipefd[2])
{
	SOCKET sock_server, sock_client, sock_server_established;
	struct sockaddr_in saddr_in;
	struct sockaddr * const saddr_p = (struct sockaddr *)&saddr_in;
	int saddr_len = sizeof(struct sockaddr_in);
	u_long arg;
	fd_set select_set;
	char succ = 1;

	sock_server = sock_client = sock_server_established = INVALID_SOCKET;

	purple_debug_misc("wpurple", "wpurple_input_pipe(0x%x[%d,%d])\n",
		(unsigned int)pipefd, pipefd[0], pipefd[1]);

	/* create client and passive server sockets */
	sock_server = socket(AF_INET, SOCK_STREAM, 0);
	sock_client = socket(AF_INET, SOCK_STREAM, 0);
	succ = (sock_server != INVALID_SOCKET || sock_client != INVALID_SOCKET);

	/* set created sockets into nonblocking mode */
	arg = 1;
	succ = (succ &&
		ioctlsocket(sock_server, FIONBIO, &arg) != SOCKET_ERROR);
	arg = 1;
	succ = (succ &&
		ioctlsocket(sock_client, FIONBIO, &arg) != SOCKET_ERROR);

	/* listen on server socket */
	memset(&saddr_in, 0, saddr_len);
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = 0;
	saddr_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	succ = (succ &&
		bind(sock_server, saddr_p, saddr_len) != SOCKET_ERROR &&
		listen(sock_server, 1) != SOCKET_ERROR &&
		getsockname(sock_server, saddr_p, &saddr_len) != SOCKET_ERROR);

	/* request a connection from client to server socket */
	succ = (succ &&
		connect(sock_client, saddr_p, saddr_len) == SOCKET_ERROR &&
		WSAGetLastError() == WSAEWOULDBLOCK);

	/* ensure, that server socket is readable */
	if (succ)
	{
		FD_ZERO(&select_set);
		FD_SET(sock_server, &select_set);
	}
	succ = (succ &&
		select(0, &select_set, NULL, NULL, NULL) != SOCKET_ERROR &&
		FD_ISSET(sock_server, &select_set));

	/* accept (establish) connection from client socket */
	if (succ)
	{
		sock_server_established =
			accept(sock_server, saddr_p, &saddr_len);
		succ = (sock_server_established != INVALID_SOCKET);
	}

	/* ensure, that client socket is writable */
	if (succ)
	{
		FD_ZERO(&select_set);
		FD_SET(sock_client, &select_set);
	}
	succ = (succ &&
		select(0, NULL, &select_set, NULL, NULL) != SOCKET_ERROR &&
		FD_ISSET(sock_client, &select_set));

	/* set sockets into blocking mode */
	arg = 0;
	succ = (succ &&
		ioctlsocket(sock_client, FIONBIO, &arg) != SOCKET_ERROR);
	arg = 0;
	succ = (succ &&
		ioctlsocket(sock_server_established, FIONBIO, &arg)
			!= SOCKET_ERROR);

	/* we don't need (passive) server socket anymore */
	if (sock_server != INVALID_SOCKET)
		closesocket(sock_server);

	if (succ)
	{
		purple_debug_misc("wpurple",
			"wpurple_input_pipe created pipe [%d,%d]\n",
			sock_client, sock_server_established);
		pipefd[0] = sock_client; /* for reading */
		pipefd[1] = sock_server_established; /* for writing */
		return 0;
	}
	else
	{
		purple_debug_error("wpurple", "wpurple_input_pipe failed\n");
		if (sock_client != INVALID_SOCKET)
			closesocket(sock_client);
		if (sock_server_established != INVALID_SOCKET)
			closesocket(sock_server_established);
		errno = EMFILE;
		return -1;
	}
}

void wpurple_init(void) {
	WORD wVersionRequested;
	WSADATA wsaData;

#if !GLIB_CHECK_VERSION(2, 32, 0)
	/* GLib threading system is automaticaly initialized since 2.32.
	 * For earlier versions, it have to be initialized before calling any
	 * Glib or GTK+ functions.
	 */
	if (!g_thread_supported())
		g_thread_init(NULL);
#endif

	if (purple_debug_is_verbose())
		purple_debug_misc("wpurple", "wpurple_init start\n");

	purple_debug_info("wpurple", "libpurple version: " DISPLAY_VERSION "\n");
	purple_debug_info("wpurple", "Glib: %u.%u.%u\n",
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

	if (purple_debug_is_verbose())
		purple_debug_misc("wpurple", "wpurple_init end\n");
}

/* Windows Cleanup */

void wpurple_cleanup(void) {
	purple_debug_info("wpurple", "wpurple_cleanup\n");

	/* winsock cleanup */
	WSACleanup();

	g_free(app_data_dir);
	g_free(bin_dir);
	g_free(data_dir);
	g_free(lib_dir);
	g_free(locale_dir);
	g_free(sysconf_dir);

	app_data_dir = NULL;
	bin_dir = NULL;
	data_dir = NULL;
	lib_dir = NULL;
	locale_dir = NULL;
	sysconf_dir = NULL;

	libpurpledll_hInstance = NULL;
}

long
wpurple_get_tz_offset() {
	TIME_ZONE_INFORMATION tzi;
	DWORD ret;
	long off = -1;

	if ((ret = GetTimeZoneInformation(&tzi)) != TIME_ZONE_ID_INVALID)
	{
		off = -(tzi.Bias * 60);
		if (ret == TIME_ZONE_ID_DAYLIGHT)
			off -= tzi.DaylightBias * 60;
	}

	return off;
}

/* DLL initializer */
/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	libpurpledll_hInstance = hinstDLL;
	return TRUE;
}

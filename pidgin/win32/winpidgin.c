/*
 *  winpidgin.c
 *
 *  Date: June, 2002
 *  Description: Entry point for win32 pidgin, and various win32 dependant
 *  routines.
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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

/* This is for ATTACH_PARENT_PROCESS */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif
#include <windows.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* These will hopefully be in the win32api next time it is updated - at which point, we'll remove them */
#ifndef LANG_PERSIAN
#define LANG_PERSIAN 0x29
#endif
#ifndef LANG_BOSNIAN
#define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN	0x05
#define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC	0x08
#endif
#ifndef SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN
#define SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN	0x04
#endif
#ifndef LANG_XHOSA
#define LANG_XHOSA 0x34
#endif


typedef int (CALLBACK* LPFNPIDGINMAIN)(HINSTANCE, int, char**);
typedef void (CALLBACK* LPFNSETDLLDIRECTORY)(LPCTSTR);
typedef BOOL (CALLBACK* LPFNATTACHCONSOLE)(DWORD);

static BOOL portable_mode = FALSE;

/*
 *  PROTOTYPES
 */
static LPFNPIDGINMAIN pidgin_main = NULL;
static LPFNSETDLLDIRECTORY MySetDllDirectory = NULL;

static const char *get_win32_error_message(DWORD err) {
	static char err_msg[512];

	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &err_msg, sizeof(err_msg), NULL);

	return err_msg;
}

static BOOL read_reg_string(HKEY key, char* sub_key, char* val_name, LPBYTE data, LPDWORD data_len) {
	HKEY hkey;
	BOOL ret = FALSE;
	LONG retv;

	if (ERROR_SUCCESS == (retv = RegOpenKeyEx(key, sub_key, 0,
					KEY_QUERY_VALUE, &hkey))) {
		if (ERROR_SUCCESS == (retv = RegQueryValueEx(hkey, val_name,
						NULL, NULL, data, data_len)))
			ret = TRUE;
		else {
			const char *err_msg = get_win32_error_message(retv);

			printf("Could not read reg key '%s' subkey '%s' value: '%s'.\nMessage: (%ld) %s\n",
					(key == HKEY_LOCAL_MACHINE) ? "HKLM"
					 : ((key == HKEY_CURRENT_USER) ? "HKCU" : "???"),
					sub_key, val_name, retv, err_msg);
		}
		RegCloseKey(hkey);
	}
	else {
		TCHAR szBuf[80];

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, retv, 0,
				(LPTSTR) &szBuf, sizeof(szBuf), NULL);
		printf("Could not open reg subkey: %s\nError: (%ld) %s\n",
				sub_key, retv, szBuf);
	}

	return ret;
}

static void common_dll_prep(const char *path) {
	HMODULE hmod;
	HKEY hkey;

	printf("GTK+ path found: %s\n", path);

	if ((hmod = GetModuleHandle("kernel32.dll"))) {
		MySetDllDirectory = (LPFNSETDLLDIRECTORY) GetProcAddress(
			hmod, "SetDllDirectoryA");
		if (!MySetDllDirectory)
			printf("SetDllDirectory not supported\n");
	} else
		printf("Error getting kernel32.dll module handle\n");

	/* For Windows XP SP1+ / Server 2003 we use SetDllDirectory to avoid dll hell */
	if (MySetDllDirectory) {
		printf("Using SetDllDirectory\n");
		MySetDllDirectory(path);
	}

	/* For the rest, we set the current directory and make sure
	 * SafeDllSearch is set to 0 where needed. */
	else {
		OSVERSIONINFO osinfo;

		printf("Setting current directory to GTK+ dll directory\n");
		SetCurrentDirectory(path);
		/* For Windows 2000 (SP3+) / WinXP (No SP):
		 * If SafeDllSearchMode is set to 1, Windows system directories are
		 * searched for dlls before the current directory. Therefore we set it
		 * to 0.
		 */
		osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&osinfo);
		if ((osinfo.dwMajorVersion == 5 &&
			osinfo.dwMinorVersion == 0 &&
			strcmp(osinfo.szCSDVersion, "Service Pack 3") >= 0) ||
			(osinfo.dwMajorVersion == 5 &&
			osinfo.dwMinorVersion == 1 &&
			strcmp(osinfo.szCSDVersion, "") >= 0)
		) {
			DWORD regval = 1;
			DWORD reglen = sizeof(DWORD);

			printf("Using Win2k (SP3+) / WinXP (No SP)... Checking SafeDllSearch\n");
			read_reg_string(HKEY_LOCAL_MACHINE,
				"System\\CurrentControlSet\\Control\\Session Manager",
				"SafeDllSearchMode",
				(LPBYTE) &regval,
				&reglen);

			if (regval != 0) {
				printf("Trying to set SafeDllSearchMode to 0\n");
				regval = 0;
				if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					"System\\CurrentControlSet\\Control\\Session Manager",
					0,  KEY_SET_VALUE, &hkey
				) == ERROR_SUCCESS) {
					if (RegSetValueEx(hkey,
						"SafeDllSearchMode", 0,
						REG_DWORD, (LPBYTE) &regval,
						sizeof(DWORD)
					) != ERROR_SUCCESS)
						printf("Error writing SafeDllSearchMode. Error: %u\n",
						(UINT) GetLastError());
					RegCloseKey(hkey);
				} else
					printf("Error opening Session Manager key for writing. Error: %u\n",
						(UINT) GetLastError());
			} else
				printf("SafeDllSearchMode is set to 0\n");
		}/*end else*/
	}
}

static void portable_mode_dll_prep(const char *pidgin_dir) {
	/* need to be able to fit MAX_PATH + "PIDGIN_ASPELL_DIR=\\Aspell\\bin" in path2 */
	char path[MAX_PATH + 1];
	char path2[MAX_PATH + 33];
	const char *prev = NULL;

	/* We assume that GTK+ is installed under \\path\to\Pidgin\..\GTK
	 * First we find \\path\to
	 */
	if (*pidgin_dir) {
		/* pidgin_dir points to \\path\to\Pidgin */
		const char *tmp = pidgin_dir;

		while ((tmp = strchr(tmp, '\\'))) {
			prev = tmp;
			tmp++;
		}
	}

	if (prev) {
		int cnt = (prev - pidgin_dir);
		strncpy(path, pidgin_dir, cnt);
		path[cnt] = '\0';
	} else {
		printf("Unable to determine current executable path. \n"
			"This will prevent the settings dir from being set.\n"
			"Assuming GTK+ is in the PATH.\n");
		return;
	}

	/* Set $HOME so that the GTK+ settings get stored in the right place */
	_snprintf(path2, sizeof(path2), "HOME=%s", path);
	_putenv(path2);

	/* Set up the settings dir base to be \\path\to
	 * The actual settings dir will be \\path\to\.purple */
	_snprintf(path2, sizeof(path2), "PURPLEHOME=%s", path);
	printf("Setting settings dir: %s\n", path2);
	_putenv(path2);

	_snprintf(path2, sizeof(path2), "PIDGIN_ASPELL_DIR=%s\\Aspell\\bin", path);
	printf("%s\n", path2);
	_putenv(path2);

	/* set the GTK+ path to be \\path\to\GTK\bin */
	strcat(path, "\\GTK\\bin");

	common_dll_prep(path);
}

static void dll_prep() {
	char path[MAX_PATH + 1];
	HKEY hkey;
	char gtkpath[MAX_PATH + 1];
	DWORD plen;

	plen = sizeof(gtkpath);
	hkey = HKEY_CURRENT_USER;
	if (!read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "Path",
			(LPBYTE) &gtkpath, &plen)) {
		hkey = HKEY_LOCAL_MACHINE;
		if (!read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "Path",
				(LPBYTE) &gtkpath, &plen)) {
			printf("GTK+ Path Registry Key not found. "
				"Assuming GTK+ is in the PATH.\n");
			return;
		}
	}

	/* this value is replaced during a successful RegQueryValueEx() */
	plen = sizeof(path);
	/* Determine GTK+ dll path .. */
	if (!read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "DllPath",
				(LPBYTE) &path, &plen)) {
		strcpy(path, gtkpath);
		strcat(path, "\\bin");
	}

	common_dll_prep(path);
}

static char* winpidgin_lcid_to_posix(LCID lcid) {
	char *posix = NULL;
	int lang_id = PRIMARYLANGID(lcid);
	int sub_id = SUBLANGID(lcid);

	switch (lang_id) {
		case LANG_AFRIKAANS: posix = "af"; break;
		case LANG_ARABIC: posix = "ar"; break;
		case LANG_AZERI: posix = "az"; break;
		case LANG_BENGALI: posix = "bn"; break;
		case LANG_BULGARIAN: posix = "bg"; break;
		case LANG_CATALAN: posix = "ca"; break;
		case LANG_CZECH: posix = "cs"; break;
		case LANG_DANISH: posix = "da"; break;
		case LANG_ESTONIAN: posix = "et"; break;
		case LANG_PERSIAN: posix = "fa"; break;
		case LANG_GERMAN: posix = "de"; break;
		case LANG_GREEK: posix = "el"; break;
		case LANG_ENGLISH:
			switch (sub_id) {
				case SUBLANG_ENGLISH_UK:
					posix = "en_GB"; break;
				case SUBLANG_ENGLISH_AUS:
					posix = "en_AU"; break;
				case SUBLANG_ENGLISH_CAN:
					posix = "en_CA"; break;
				default:
					posix = "en"; break;
			}
			break;
		case LANG_SPANISH: posix = "es"; break;
		case LANG_BASQUE: posix = "eu"; break;
		case LANG_FINNISH: posix = "fi"; break;
		case LANG_FRENCH: posix = "fr"; break;
		case LANG_GALICIAN: posix = "gl"; break;
		case LANG_GUJARATI: posix = "gu"; break;
		case LANG_HEBREW: posix = "he"; break;
		case LANG_HINDI: posix = "hi"; break;
		case LANG_HUNGARIAN: posix = "hu"; break;
		case LANG_ICELANDIC: break;
		case LANG_INDONESIAN: posix = "id"; break;
		case LANG_ITALIAN: posix = "it"; break;
		case LANG_JAPANESE: posix = "ja"; break;
		case LANG_GEORGIAN: posix = "ka"; break;
		case LANG_KANNADA: posix = "kn"; break;
		case LANG_KOREAN: posix = "ko"; break;
		case LANG_LITHUANIAN: posix = "lt"; break;
		case LANG_MACEDONIAN: posix = "mk"; break;
		case LANG_DUTCH: posix = "nl"; break;
		case LANG_NEPALI: posix = "ne"; break;
		case LANG_NORWEGIAN:
			switch (sub_id) {
				case SUBLANG_NORWEGIAN_BOKMAL:
					posix = "nb"; break;
				case SUBLANG_NORWEGIAN_NYNORSK:
					posix = "nn"; break;
			}
			break;
		case LANG_PUNJABI: posix = "pa"; break;
		case LANG_POLISH: posix = "pl"; break;
		case LANG_PASHTO: posix = "ps"; break;
		case LANG_PORTUGUESE:
			switch (sub_id) {
				case SUBLANG_PORTUGUESE_BRAZILIAN:
					posix = "pt_BR"; break;
				default:
				posix = "pt"; break;
			}
			break;
		case LANG_ROMANIAN: posix = "ro"; break;
		case LANG_RUSSIAN: posix = "ru"; break;
		case LANG_SLOVAK: posix = "sk"; break;
		case LANG_SLOVENIAN: posix = "sl"; break;
		case LANG_ALBANIAN: posix = "sq"; break;
		/* LANG_CROATIAN == LANG_SERBIAN == LANG_BOSNIAN */
		case LANG_SERBIAN:
			switch (sub_id) {
				case SUBLANG_SERBIAN_LATIN:
					posix = "sr@Latn"; break;
				case SUBLANG_SERBIAN_CYRILLIC:
					posix = "sr"; break;
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC:
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = "bs"; break;
				case SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = "hr"; break;
			}
			break;
		case LANG_SWEDISH: posix = "sv"; break;
		case LANG_TAMIL: posix = "ta"; break;
		case LANG_TELUGU: posix = "te"; break;
		case LANG_THAI: posix = "th"; break;
		case LANG_TURKISH: posix = "tr"; break;
		case LANG_UKRAINIAN: posix = "uk"; break;
		case LANG_VIETNAMESE: posix = "vi"; break;
		case LANG_XHOSA: posix = "xh"; break;
		case LANG_CHINESE:
			switch (sub_id) {
				case SUBLANG_CHINESE_SIMPLIFIED:
					posix = "zh_CN"; break;
				case SUBLANG_CHINESE_TRADITIONAL:
					posix = "zh_TW"; break;
				default:
					posix = "zh"; break;
			}
			break;
		case LANG_URDU: break;
		case LANG_BELARUSIAN: break;
		case LANG_LATVIAN: break;
		case LANG_ARMENIAN: break;
		case LANG_FAEROESE: break;
		case LANG_MALAY: break;
		case LANG_KAZAK: break;
		case LANG_KYRGYZ: break;
		case LANG_SWAHILI: break;
		case LANG_UZBEK: break;
		case LANG_TATAR: break;
		case LANG_ORIYA: break;
		case LANG_MALAYALAM: break;
		case LANG_ASSAMESE: break;
		case LANG_MARATHI: break;
		case LANG_SANSKRIT: break;
		case LANG_MONGOLIAN: break;
		case LANG_KONKANI: break;
		case LANG_MANIPURI: break;
		case LANG_SINDHI: break;
		case LANG_SYRIAC: break;
		case LANG_KASHMIRI: break;
		case LANG_DIVEHI: break;
	}

	/* Deal with exceptions */
	if (posix == NULL) {
		switch (lcid) {
			case 0x0455: posix = "my_MM"; break; /* Myanmar (Burmese) */
			case 9999: posix = "ku"; break; /* Kurdish (from NSIS) */
		}
	}

	return posix;
}

/* Determine and set Pidgin locale as follows (in order of priority):
   - Check PIDGINLANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
static const char *winpidgin_get_locale() {
	const char *locale = NULL;
	LCID lcid;
	char data[10];
	DWORD datalen = 10;

	/* Check if user set PIDGINLANG env var */
	if ((locale = getenv("PIDGINLANG")))
		return locale;

	if (!portable_mode && read_reg_string(HKEY_CURRENT_USER, "SOFTWARE\\pidgin",
			"Installer Language", (LPBYTE) &data, &datalen)) {
		if ((locale = winpidgin_lcid_to_posix(atoi(data))))
			return locale;
	}

	lcid = GetUserDefaultLCID();
	if ((locale = winpidgin_lcid_to_posix(lcid)))
		return locale;

	return "en";
}

static void winpidgin_set_locale() {
	const char *locale = NULL;
	char envstr[25];

	locale = winpidgin_get_locale();

	_snprintf(envstr, 25, "LANG=%s", locale);
	printf("Setting locale: %s\n", envstr);
	_putenv(envstr);
}


static void winpidgin_add_stuff_to_path() {
	char perl_path[MAX_PATH + 1];
	char *ppath = NULL;
	char mit_kerberos_path[MAX_PATH + 1];
	char *mpath = NULL;
	DWORD plen;

	printf("%s", "Looking for Perl... ");

	plen = sizeof(perl_path);
	if (read_reg_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\Perl", "",
			    (LPBYTE) &perl_path, &plen)) {
		/* We *could* check for perl510.dll, but it seems unnecessary. */
		printf("found in '%s'.\n", perl_path);

		if (perl_path[strlen(perl_path) - 1] != '\\')
			strcat(perl_path, "\\");
		strcat(perl_path, "bin");

		ppath = perl_path;
	} else
		printf("%s", "not found.\n");

	printf("%s", "Looking for MIT Kerberos... ");

	plen = sizeof(mit_kerberos_path);
	if (read_reg_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\MIT\\Kerberos", "InstallDir",
			    (LPBYTE) &mit_kerberos_path, &plen)) {
		/* We *could* check for gssapi32.dll */
		printf("found in '%s'.\n", mit_kerberos_path);

		if (mit_kerberos_path[strlen(mit_kerberos_path) - 1] != '\\')
			strcat(mit_kerberos_path, "\\");
		strcat(mit_kerberos_path, "bin");

		mpath = mit_kerberos_path;
	} else
		printf("%s", "not found.\n");

	if (ppath != NULL || mpath != NULL) {
		const char *path = getenv("PATH");
		BOOL add_ppath = ppath != NULL && (path == NULL || !strstr(path, ppath));
		BOOL add_mpath = mpath != NULL && (path == NULL || !strstr(path, mpath));
		char *newpath;
		int newlen;

		if (add_ppath || add_mpath) {
			/* Enough to add "PATH=" + path + ";"  + ppath + ";" + mpath + \0 */
			newlen = 6 + (path ? strlen(path) + 1 : 0);
			if (add_ppath)
				newlen += strlen(ppath) + 1;
			if (add_mpath)
				newlen += strlen(mpath) + 1;
			newpath = malloc(newlen);
			*newpath = '\0';

			_snprintf(newpath, newlen, "PATH=%s%s%s%s%s%s",
				  path ? path : "",
				  path ? ";" : "",
				  add_ppath ? ppath : "",
				  add_ppath ? ";" : "",
				  add_mpath ? mpath : "",
				  add_mpath ? ";" : "");

			printf("New PATH: %s\n", newpath);

			_putenv(newpath);
			free(newpath);
		}
	}
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static BOOL winpidgin_set_running(BOOL fail_if_running) {
	HANDLE h;

	if ((h = CreateMutex(NULL, FALSE, "pidgin_is_running"))) {
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) {
			if (fail_if_running) {
				HWND msg_win;

				printf("An instance of Pidgin is already running.\n");

				if((msg_win = FindWindowEx(NULL, NULL, TEXT("WinpidginMsgWinCls"), NULL)))
					if(SendMessage(msg_win, PIDGIN_WM_FOCUS_REQUEST, (WPARAM) NULL, (LPARAM) NULL))
						return FALSE;

				/* If we get here, the focus request wasn't successful */

				MessageBox(NULL,
					"An instance of Pidgin is already running",
					NULL, MB_OK | MB_TOPMOST);

				return FALSE;
			}
		} else if (err != ERROR_SUCCESS)
			printf("Error (%u) accessing \"pidgin_is_running\" mutex.\n", (UINT) err);
	}
	return TRUE;
}

#define PROTO_HANDLER_SWITCH "--protocolhandler="

static void handle_protocol(char *cmd) {
	char *remote_msg, *tmp1, *tmp2;
	int len;
	SIZE_T len_written;
	HWND msg_win;
	DWORD pid;
	HANDLE process;

	/* The start of the message */
	tmp1 = cmd + strlen(PROTO_HANDLER_SWITCH);

	/* The end of the message */
	if ((tmp2 = strchr(tmp1, ' ')))
		len = (tmp2 - tmp1);
	else
		len = strlen(tmp1);

	if (len == 0) {
		printf("No protocol message specified.\n");
		return;
	}

	if (!(msg_win = FindWindowEx(NULL, NULL, TEXT("WinpidginMsgWinCls"), NULL))) {
		printf("Unable to find an instance of Pidgin to handle protocol message.\n");
		return;
	}

	GetWindowThreadProcessId(msg_win, &pid);
	if (!(process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid))) {
		DWORD dw = GetLastError();
		const char *err_msg = get_win32_error_message(dw);
		printf("Unable to open Pidgin process. (%u) %s\n", (UINT) dw, err_msg);
		return;
	}

	printf("Trying to handle protocol message:\n'%.*s'\n", len, tmp1);

	/* MEM_COMMIT initializes the memory to zero,
	 * so we don't need to worry that our section of tmp1 isn't nul-terminated */
	if ((remote_msg = (char*) VirtualAllocEx(process, NULL, len + 1, MEM_COMMIT, PAGE_READWRITE))) {
		if (WriteProcessMemory(process, remote_msg, tmp1, len, &len_written)) {
			if (!SendMessage(msg_win, PIDGIN_WM_PROTOCOL_HANDLE, len_written, (LPARAM) remote_msg))
				printf("Unable to send protocol message to Pidgin instance.\n");
		} else {
			DWORD dw = GetLastError();
			const char *err_msg = get_win32_error_message(dw);
			printf("Unable to write to remote memory. (%u) %s\n", (UINT) dw, err_msg);
		}

		VirtualFreeEx(process, remote_msg, 0, MEM_RELEASE);
	} else {
		DWORD dw = GetLastError();
		const char *err_msg = get_win32_error_message(dw);
		printf("Unable to allocate remote memory. (%u) %s\n", (UINT) dw, err_msg);
	}

	CloseHandle(process);
}


int _stdcall
WinMain (struct HINSTANCE__ *hInstance, struct HINSTANCE__ *hPrevInstance,
		char *lpszCmdLine, int nCmdShow) {
	char errbuf[512];
	char pidgin_dir[MAX_PATH];
	char exe_name[MAX_PATH];
	HMODULE hmod;
	char *tmp;
	int pidgin_argc = __argc;
	char **pidgin_argv = __argv;
	int i;
	BOOL debug = FALSE, help = FALSE, version = FALSE, multiple = FALSE;

	/* If debug or help or version flag used, create console for output */
	for (i = 1; i < __argc; i++) {
		if (strlen(__argv[i]) > 1 && __argv[i][0] == '-') {
			/* check if we're looking at -- or - option */
			if (__argv[i][1] == '-') {
				if (strstr(__argv[i], "--debug") == __argv[i])
					debug = TRUE;
				else if (strstr(__argv[i], "--help") == __argv[i])
					help = TRUE;
				else if (strstr(__argv[i], "--version") == __argv[i])
					version = TRUE;
				else if (strstr(__argv[i], "--multiple") == __argv[i])
					multiple = TRUE;
			} else {
				if (strchr(__argv[i], 'd'))
					debug = TRUE;
				else if (strchr(__argv[i], 'h'))
					help = TRUE;
				else if (strchr(__argv[i], 'v'))
					version = TRUE;
				else if (strchr(__argv[i], 'm'))
					multiple = TRUE;
			}
		}
	}

	if (debug || help || version) {
		/* If stdout hasn't been redirected to a file, alloc a console
		 *  (_istty() doesn't work for stuff using the GUI subsystem) */
		if (_fileno(stdout) == -1 || _fileno(stdout) == -2) {
			LPFNATTACHCONSOLE MyAttachConsole = NULL;
			if ((hmod = GetModuleHandle("kernel32.dll"))) {
				MyAttachConsole =
					(LPFNATTACHCONSOLE)
					GetProcAddress(hmod, "AttachConsole");
			}
			if ((MyAttachConsole && MyAttachConsole(ATTACH_PARENT_PROCESS))
					|| AllocConsole()) {
				freopen("CONOUT$", "w", stdout);
				freopen("CONOUT$", "w", stderr);
			}
		}
	}

	/* If this is a protocol handler invocation, deal with it accordingly */
	if ((tmp = strstr(lpszCmdLine, PROTO_HANDLER_SWITCH)) != NULL) {
		handle_protocol(tmp);
		return 0;
	}

	/* Load exception handler if we have it */
	if (GetModuleFileName(NULL, pidgin_dir, MAX_PATH) != 0) {
		char *prev = NULL;
		tmp = pidgin_dir;

		/* primitive dirname() */
		while ((tmp = strchr(tmp, '\\'))) {
			prev = tmp;
			tmp++;
		}

		if (prev) {
			prev[0] = '\0';

			/* prev++ will now point to the executable file name */
			strcpy(exe_name, prev + 1);

			strcat(pidgin_dir, "\\exchndl.dll");
			if (LoadLibrary(pidgin_dir))
				printf("Loaded exchndl.dll\n");

			prev[0] = '\0';
		}
	} else {
		DWORD dw = GetLastError();
		const char *err_msg = get_win32_error_message(dw);
		_snprintf(errbuf, 512,
			"Error getting module filename.\nError: (%u) %s",
			(UINT) dw, err_msg);
		printf("%s\n", errbuf);
		MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
		pidgin_dir[0] = '\0';
	}

	/* Determine if we're running in portable mode */
	if (strstr(lpszCmdLine, "--portable-mode")
			|| (exe_name != NULL && strstr(exe_name, "-portable.exe"))) {
		int i = 0, c = 0;

		printf("Running in PORTABLE mode.\n");
		portable_mode = TRUE;

		/* Remove the --portable-mode arg from the args passed to pidgin so it doesn't choke */
		pidgin_argv = malloc(sizeof(char*) * pidgin_argc);
		for (; i < __argc; i++) {
			if (strstr(__argv[i], "--portable-mode") == NULL)
				pidgin_argv[c++] = __argv[i];
			else
				pidgin_argc--;
		}
	}

	if (portable_mode)
		portable_mode_dll_prep(pidgin_dir);
	else if (!getenv("PIDGIN_NO_DLL_CHECK"))
		dll_prep();

	winpidgin_set_locale();

	winpidgin_add_stuff_to_path();

	/* If help, version or multiple flag used, do not check Mutex */
	if (!help && !version)
		if (!winpidgin_set_running(getenv("PIDGIN_MULTI_INST") == NULL && !multiple))
			return 0;

	/* Now we are ready for Pidgin .. */
	if ((hmod = LoadLibrary("pidgin.dll")))
		pidgin_main = (LPFNPIDGINMAIN) GetProcAddress(hmod, "pidgin_main");

	if (!pidgin_main) {
		DWORD dw = GetLastError();
		BOOL mod_not_found = (dw == ERROR_MOD_NOT_FOUND || dw == ERROR_DLL_NOT_FOUND);
		const char *err_msg = get_win32_error_message(dw);

		_snprintf(errbuf, 512, "Error loading pidgin.dll.\nError: (%u) %s%s%s",
			(UINT) dw, err_msg,
			mod_not_found ? "\n" : "",
			mod_not_found ? "This probably means that GTK+ can't be found." : "");
		printf("%s\n", errbuf);
		MessageBox(NULL, errbuf, TEXT("Error"), MB_OK | MB_TOPMOST);

		return 0;
	}

	return pidgin_main(hInstance, pidgin_argc, pidgin_argv);
}

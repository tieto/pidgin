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
#define UNICODE
#define _UNICODE
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif
#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"

typedef int (CALLBACK* LPFNPIDGINMAIN)(HINSTANCE, int, char**);
typedef void (CALLBACK* LPFNSETDLLDIRECTORY)(LPCWSTR);
typedef BOOL (CALLBACK* LPFNATTACHCONSOLE)(DWORD);

static BOOL portable_mode = FALSE;

/*
 *  PROTOTYPES
 */
static LPFNPIDGINMAIN pidgin_main = NULL;
static LPFNSETDLLDIRECTORY MySetDllDirectory = NULL;

static const TCHAR *get_win32_error_message(DWORD err) {
	static TCHAR err_msg[512];

	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &err_msg, sizeof(err_msg) / sizeof(TCHAR), NULL);

	return err_msg;
}

static BOOL read_reg_string(HKEY key, TCHAR *sub_key, TCHAR *val_name, LPBYTE data, LPDWORD data_len) {
	HKEY hkey;
	BOOL ret = FALSE;
	LONG retv;

	if (ERROR_SUCCESS == (retv = RegOpenKeyEx(key, sub_key, 0,
					KEY_QUERY_VALUE, &hkey))) {
		if (ERROR_SUCCESS == (retv = RegQueryValueEx(hkey, val_name,
						NULL, NULL, data, data_len)))
			ret = TRUE;
		else {
			const TCHAR *err_msg = get_win32_error_message(retv);

			_tprintf(_T("Could not read reg key '%s' subkey '%s' value: '%s'.\nMessage: (%ld) %s\n"),
					(key == HKEY_LOCAL_MACHINE) ? _T("HKLM")
					 : ((key == HKEY_CURRENT_USER) ? _T("HKCU") : _T("???")),
					sub_key, val_name, retv, err_msg);
		}
		RegCloseKey(hkey);
	}
	else {
		TCHAR szBuf[80];

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, retv, 0,
				(LPTSTR) &szBuf, sizeof(szBuf) / sizeof(TCHAR), NULL);
		_tprintf(_T("Could not open reg subkey: %s\nError: (%ld) %s\n"),
				sub_key, retv, szBuf);
	}

	return ret;
}

static BOOL common_dll_prep(const TCHAR *path) {
	HMODULE hmod;
	HKEY hkey;
	struct _stat stat_buf;
	TCHAR test_path[MAX_PATH + 1];

	_sntprintf(test_path, sizeof(test_path) / sizeof(TCHAR),
		_T("%s\\libgtk-win32-2.0-0.dll"), path);
	test_path[sizeof(test_path) / sizeof(TCHAR) - 1] = _T('\0');

	if (_tstat(test_path, &stat_buf) != 0) {
		printf("Unable to determine GTK+ path. \n"
			"Assuming GTK+ is in the PATH.\n");
		return FALSE;
	}


	_tprintf(_T("GTK+ path found: %s\n"), path);

	if ((hmod = GetModuleHandle(_T("kernel32.dll")))) {
		MySetDllDirectory = (LPFNSETDLLDIRECTORY) GetProcAddress(
			hmod, "SetDllDirectoryW");
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
		if ((osinfo.dwMajorVersion == 5
				&& osinfo.dwMinorVersion == 0
				&& _tcscmp(osinfo.szCSDVersion, _T("Service Pack 3")) >= 0)
			||
			(osinfo.dwMajorVersion == 5
				&& osinfo.dwMinorVersion == 1
				&& _tcscmp(osinfo.szCSDVersion, _T("")) >= 0)
		) {
			DWORD regval = 1;
			DWORD reglen = sizeof(DWORD);

			printf("Using Win2k (SP3+) / WinXP (No SP)... Checking SafeDllSearch\n");
			read_reg_string(HKEY_LOCAL_MACHINE,
				_T("System\\CurrentControlSet\\Control\\Session Manager"),
				_T("SafeDllSearchMode"),
				(LPBYTE) &regval,
				&reglen);

			if (regval != 0) {
				printf("Trying to set SafeDllSearchMode to 0\n");
				regval = 0;
				if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					_T("System\\CurrentControlSet\\Control\\Session Manager"),
					0,  KEY_SET_VALUE, &hkey
				) == ERROR_SUCCESS) {
					if (RegSetValueEx(hkey,
						_T("SafeDllSearchMode"), 0,
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

	return TRUE;
}

static BOOL dll_prep(const TCHAR *pidgin_dir) {
	TCHAR path[MAX_PATH + 1];
	path[0] = _T('\0');

	if (*pidgin_dir) {
		_sntprintf(path, sizeof(path) / sizeof(TCHAR), _T("%s\\Gtk\\bin"), pidgin_dir);
		path[sizeof(path) / sizeof(TCHAR)] = _T('\0');
	}

	return common_dll_prep(path);
}

static void portable_mode_dll_prep(const TCHAR *pidgin_dir) {
	/* need to be able to fit MAX_PATH + "PIDGIN_ASPELL_DIR=\\Aspell\\bin" in path2 */
	TCHAR path[MAX_PATH + 1];
	TCHAR path2[MAX_PATH + 33];
	const TCHAR *prev = NULL;

	/* We assume that GTK+ is installed under \\path\to\Pidgin\..\GTK
	 * First we find \\path\to
	 */
	if (*pidgin_dir)
		/* pidgin_dir points to \\path\to\Pidgin */
		prev = _tcsrchr(pidgin_dir, _T('\\'));

	if (prev) {
		int cnt = (prev - pidgin_dir);
		_tcsncpy(path, pidgin_dir, cnt);
		path[cnt] = _T('\0');
	} else {
		printf("Unable to determine current executable path. \n"
			"This will prevent the settings dir from being set.\n"
			"Assuming GTK+ is in the PATH.\n");
		return;
	}

	/* Set $HOME so that the GTK+ settings get stored in the right place */
	_sntprintf(path2, sizeof(path2) / sizeof(TCHAR), _T("HOME=%s"), path);
	_tputenv(path2);

	/* Set up the settings dir base to be \\path\to
	 * The actual settings dir will be \\path\to\.purple */
	_sntprintf(path2, sizeof(path2) / sizeof(TCHAR), _T("PURPLEHOME=%s"), path);
	_tprintf(_T("Setting settings dir: %s\n"), path2);
	_tputenv(path2);

	_sntprintf(path2, sizeof(path2) / sizeof(TCHAR), _T("PIDGIN_ASPELL_DIR=%s\\Aspell\\bin"), path);
	_tprintf(_T("%s\n"), path2);
	_tputenv(path2);

	if (!dll_prep(pidgin_dir)) {
		/* set the GTK+ path to be \\path\to\GTK\bin */
		_tcscat(path, _T("\\GTK\\bin"));
		common_dll_prep(path);
	}
}

static TCHAR* winpidgin_lcid_to_posix(LCID lcid) {
	TCHAR *posix = NULL;
	int lang_id = PRIMARYLANGID(lcid);
	int sub_id = SUBLANGID(lcid);

	switch (lang_id) {
		case LANG_AFRIKAANS: posix = _T("af"); break;
		case LANG_ARABIC: posix = _T("ar"); break;
		case LANG_AZERI: posix = _T("az"); break;
		case LANG_BENGALI: posix = _T("bn"); break;
		case LANG_BULGARIAN: posix = _T("bg"); break;
		case LANG_CATALAN: posix = _T("ca"); break;
		case LANG_CZECH: posix = _T("cs"); break;
		case LANG_DANISH: posix = _T("da"); break;
		case LANG_ESTONIAN: posix = _T("et"); break;
		case LANG_PERSIAN: posix = _T("fa"); break;
		case LANG_GERMAN: posix = _T("de"); break;
		case LANG_GREEK: posix = _T("el"); break;
		case LANG_ENGLISH:
			switch (sub_id) {
				case SUBLANG_ENGLISH_UK:
					posix = _T("en_GB"); break;
				case SUBLANG_ENGLISH_AUS:
					posix = _T("en_AU"); break;
				case SUBLANG_ENGLISH_CAN:
					posix = _T("en_CA"); break;
				default:
					posix = _T("en"); break;
			}
			break;
		case LANG_SPANISH: posix = _T("es"); break;
		case LANG_BASQUE: posix = _T("eu"); break;
		case LANG_FINNISH: posix = _T("fi"); break;
		case LANG_FRENCH: posix = _T("fr"); break;
		case LANG_GALICIAN: posix = _T("gl"); break;
		case LANG_GUJARATI: posix = _T("gu"); break;
		case LANG_HEBREW: posix = _T("he"); break;
		case LANG_HINDI: posix = _T("hi"); break;
		case LANG_HUNGARIAN: posix = _T("hu"); break;
		case LANG_ICELANDIC: break;
		case LANG_INDONESIAN: posix = _T("id"); break;
		case LANG_ITALIAN: posix = _T("it"); break;
		case LANG_JAPANESE: posix = _T("ja"); break;
		case LANG_GEORGIAN: posix = _T("ka"); break;
		case LANG_KANNADA: posix = _T("kn"); break;
		case LANG_KOREAN: posix = _T("ko"); break;
		case LANG_LITHUANIAN: posix = _T("lt"); break;
		case LANG_MACEDONIAN: posix = _T("mk"); break;
		case LANG_DUTCH: posix = _T("nl"); break;
		case LANG_NEPALI: posix = _T("ne"); break;
		case LANG_NORWEGIAN:
			switch (sub_id) {
				case SUBLANG_NORWEGIAN_BOKMAL:
					posix = _T("nb"); break;
				case SUBLANG_NORWEGIAN_NYNORSK:
					posix = _T("nn"); break;
			}
			break;
		case LANG_PUNJABI: posix = _T("pa"); break;
		case LANG_POLISH: posix = _T("pl"); break;
		case LANG_PASHTO: posix = _T("ps"); break;
		case LANG_PORTUGUESE:
			switch (sub_id) {
				case SUBLANG_PORTUGUESE_BRAZILIAN:
					posix = _T("pt_BR"); break;
				default:
				posix = _T("pt"); break;
			}
			break;
		case LANG_ROMANIAN: posix = _T("ro"); break;
		case LANG_RUSSIAN: posix = _T("ru"); break;
		case LANG_SLOVAK: posix = _T("sk"); break;
		case LANG_SLOVENIAN: posix = _T("sl"); break;
		case LANG_ALBANIAN: posix = _T("sq"); break;
		/* LANG_CROATIAN == LANG_SERBIAN == LANG_BOSNIAN */
		case LANG_SERBIAN:
			switch (sub_id) {
				case SUBLANG_SERBIAN_LATIN:
					posix = _T("sr@Latn"); break;
				case SUBLANG_SERBIAN_CYRILLIC:
					posix = _T("sr"); break;
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC:
				case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = _T("bs"); break;
				case SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN:
					posix = _T("hr"); break;
			}
			break;
		case LANG_SWEDISH: posix = _T("sv"); break;
		case LANG_TAMIL: posix = _T("ta"); break;
		case LANG_TELUGU: posix = _T("te"); break;
		case LANG_THAI: posix = _T("th"); break;
		case LANG_TURKISH: posix = _T("tr"); break;
		case LANG_UKRAINIAN: posix = _T("uk"); break;
		case LANG_VIETNAMESE: posix = _T("vi"); break;
		case LANG_XHOSA: posix = _T("xh"); break;
		case LANG_CHINESE:
			switch (sub_id) {
				case SUBLANG_CHINESE_SIMPLIFIED:
					posix = _T("zh_CN"); break;
				case SUBLANG_CHINESE_TRADITIONAL:
					posix = _T("zh_TW"); break;
				default:
					posix = _T("zh"); break;
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
			case 0x0455: posix = _T("my_MM"); break; /* Myanmar (Burmese) */
			case 9999: posix = _T("ku"); break; /* Kurdish (from NSIS) */
		}
	}

	return posix;
}

/* Determine and set Pidgin locale as follows (in order of priority):
   - Check PIDGINLANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
static const TCHAR *winpidgin_get_locale() {
	const TCHAR *locale = NULL;
	LCID lcid;
	TCHAR data[10];
	DWORD datalen = sizeof(data) / sizeof(TCHAR);

	/* Check if user set PIDGINLANG env var */
	if ((locale = _tgetenv(_T("PIDGINLANG"))))
		return locale;

	if (!portable_mode && read_reg_string(HKEY_CURRENT_USER, _T("SOFTWARE\\pidgin"),
			_T("Installer Language"), (LPBYTE) &data, &datalen)) {
		if ((locale = winpidgin_lcid_to_posix(_ttoi(data))))
			return locale;
	}

	lcid = GetUserDefaultLCID();
	if ((locale = winpidgin_lcid_to_posix(lcid)))
		return locale;

	return _T("en");
}

static void winpidgin_set_locale() {
	const TCHAR *locale;
	TCHAR envstr[25];

	locale = winpidgin_get_locale();

	_sntprintf(envstr, sizeof(envstr) / sizeof(TCHAR), _T("LANG=%s"), locale);
	_tprintf(_T("Setting locale: %s\n"), envstr);
	_tputenv(envstr);
}


static void winpidgin_add_stuff_to_path() {
	TCHAR perl_path[MAX_PATH + 1];
	TCHAR *ppath = NULL;
	TCHAR mit_kerberos_path[MAX_PATH + 1];
	TCHAR *mpath = NULL;
	DWORD plen;

	printf("%s", "Looking for Perl... ");

	plen = sizeof(perl_path) / sizeof(TCHAR);
	if (read_reg_string(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Perl"), _T(""),
			    (LPBYTE) &perl_path, &plen)) {
		/* We *could* check for perl510.dll, but it seems unnecessary. */
		_tprintf(_T("found in '%s'.\n"), perl_path);

		if (perl_path[_tcslen(perl_path) - 1] != _T('\\'))
			_tcscat(perl_path, _T("\\"));
		_tcscat(perl_path, _T("bin"));

		ppath = perl_path;
	} else
		printf("%s", "not found.\n");

	printf("%s", "Looking for MIT Kerberos... ");

	plen = sizeof(mit_kerberos_path) / sizeof(TCHAR);
	if (read_reg_string(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\MIT\\Kerberos"), _T("InstallDir"),
			    (LPBYTE) &mit_kerberos_path, &plen)) {
		/* We *could* check for gssapi32.dll */
		_tprintf(_T("found in '%s'.\n"), mit_kerberos_path);

		if (mit_kerberos_path[_tcslen(mit_kerberos_path) - 1] != _T('\\'))
			_tcscat(mit_kerberos_path, _T("\\"));
		_tcscat(mit_kerberos_path, _T("bin"));

		mpath = mit_kerberos_path;
	} else
		printf("%s", "not found.\n");

	if (ppath != NULL || mpath != NULL) {
		const TCHAR *path = _tgetenv(_T("PATH"));
		BOOL add_ppath = ppath != NULL && (path == NULL || !_tcsstr(path, ppath));
		BOOL add_mpath = mpath != NULL && (path == NULL || !_tcsstr(path, mpath));
		TCHAR *newpath;
		int newlen;

		if (add_ppath || add_mpath) {
			/* Enough to add "PATH=" + path + ";"  + ppath + ";" + mpath + \0 */
			newlen = 6 + (path ? _tcslen(path) + 1 : 0);
			if (add_ppath)
				newlen += _tcslen(ppath) + 1;
			if (add_mpath)
				newlen += _tcslen(mpath) + 1;
			newpath = malloc(newlen * sizeof(TCHAR));

			_sntprintf(newpath, newlen, _T("PATH=%s%s%s%s%s%s"),
				  path ? path : _T(""),
				  path ? _T(";") : _T(""),
				  add_ppath ? ppath : _T(""),
				  add_ppath ? _T(";") : _T(""),
				  add_mpath ? mpath : _T(""),
				  add_mpath ? _T(";") : _T(""));

			_tprintf(_T("New PATH: %s\n"), newpath);

			_tputenv(newpath);
			free(newpath);
		}
	}
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static BOOL winpidgin_set_running(BOOL fail_if_running) {
	HANDLE h;

	if ((h = CreateMutex(NULL, FALSE, _T("pidgin_is_running")))) {
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) {
			if (fail_if_running) {
				HWND msg_win;

				printf("An instance of Pidgin is already running.\n");

				if((msg_win = FindWindowEx(NULL, NULL, _T("WinpidginMsgWinCls"), NULL)))
					if(SendMessage(msg_win, PIDGIN_WM_FOCUS_REQUEST, (WPARAM) NULL, (LPARAM) NULL))
						return FALSE;

				/* If we get here, the focus request wasn't successful */

				MessageBox(NULL,
					_T("An instance of Pidgin is already running"),
					NULL, MB_OK | MB_TOPMOST);

				return FALSE;
			}
		} else if (err != ERROR_SUCCESS)
			printf("Error (%u) accessing \"pidgin_is_running\" mutex.\n", (UINT) err);
	}
	return TRUE;
}

#define PROTO_HANDLER_SWITCH L"--protocolhandler="

static void handle_protocol(wchar_t *cmd) {
	char *remote_msg, *utf8msg;
	wchar_t *tmp1, *tmp2;
	int len, wlen;
	SIZE_T len_written;
	HWND msg_win;
	DWORD pid;
	HANDLE process;

	/* The start of the message */
	tmp1 = cmd + wcslen(PROTO_HANDLER_SWITCH);

	/* The end of the message */
	if ((tmp2 = wcschr(tmp1, L' ')))
		wlen = (tmp2 - tmp1);
	else
		wlen = wcslen(tmp1);

	if (wlen == 0) {
		printf("No protocol message specified.\n");
		return;
	}

	if (!(msg_win = FindWindowEx(NULL, NULL, _T("WinpidginMsgWinCls"), NULL))) {
		printf("Unable to find an instance of Pidgin to handle protocol message.\n");
		return;
	}

	len = WideCharToMultiByte(CP_UTF8, 0, tmp1,
			wlen, NULL, 0, NULL, NULL);
	if (len) {
		utf8msg = malloc(len * sizeof(char));
		len = WideCharToMultiByte(CP_UTF8, 0, tmp1,
			wlen, utf8msg, len, NULL, NULL);
	}

	if (len == 0) {
		printf("No protocol message specified.\n");
		return;
	}

	GetWindowThreadProcessId(msg_win, &pid);
	if (!(process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid))) {
		DWORD dw = GetLastError();
		const TCHAR *err_msg = get_win32_error_message(dw);
		_tprintf(_T("Unable to open Pidgin process. (%u) %s\n"), (UINT) dw, err_msg);
		return;
	}

	wprintf(L"Trying to handle protocol message:\n'%.*s'\n", wlen, tmp1);

	/* MEM_COMMIT initializes the memory to zero
	 * so we don't need to worry that our section of utf8msg isn't nul-terminated */
	if ((remote_msg = (char*) VirtualAllocEx(process, NULL, len + 1, MEM_COMMIT, PAGE_READWRITE))) {
		if (WriteProcessMemory(process, remote_msg, utf8msg, len, &len_written)) {
			if (!SendMessageA(msg_win, PIDGIN_WM_PROTOCOL_HANDLE, len_written, (LPARAM) remote_msg))
				printf("Unable to send protocol message to Pidgin instance.\n");
		} else {
			DWORD dw = GetLastError();
			const TCHAR *err_msg = get_win32_error_message(dw);
			_tprintf(_T("Unable to write to remote memory. (%u) %s\n"), (UINT) dw, err_msg);
		}

		VirtualFreeEx(process, remote_msg, 0, MEM_RELEASE);
	} else {
		DWORD dw = GetLastError();
		const TCHAR *err_msg = get_win32_error_message(dw);
		_tprintf(_T("Unable to allocate remote memory. (%u) %s\n"), (UINT) dw, err_msg);
	}

	CloseHandle(process);
	free(utf8msg);
}


int _stdcall
WinMain (struct HINSTANCE__ *hInstance, struct HINSTANCE__ *hPrevInstance,
		char *lpszCmdLine, int nCmdShow) {
	TCHAR errbuf[512];
	TCHAR pidgin_dir[MAX_PATH];
	TCHAR exe_name[MAX_PATH];
	HMODULE hmod;
	TCHAR *tmp;
	wchar_t *wtmp;
	int pidgin_argc;
	char **pidgin_argv; /* This is in utf-8 */
	int i, j, k;
	BOOL debug = FALSE, help = FALSE, version = FALSE, multiple = FALSE, success;
	LPWSTR *szArglist;
	LPWSTR cmdLine;

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
			if ((hmod = GetModuleHandle(_T("kernel32.dll")))) {
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

	cmdLine = GetCommandLineW();

	/* If this is a protocol handler invocation, deal with it accordingly */
	if ((wtmp = wcsstr(cmdLine, PROTO_HANDLER_SWITCH)) != NULL) {
		handle_protocol(wtmp);
		return 0;
	}

	/* Load exception handler if we have it */
	if (GetModuleFileName(NULL, pidgin_dir, MAX_PATH) != 0) {

		/* primitive dirname() */
		tmp = _tcsrchr(pidgin_dir, _T('\\'));

		if (tmp) {
			HMODULE hmod;
			tmp[0] = _T('\0');

			/* tmp++ will now point to the executable file name */
			_tcscpy(exe_name, tmp + 1);

			_tcscat(pidgin_dir, _T("\\exchndl.dll"));
			if ((hmod = LoadLibrary(pidgin_dir))) {
				FARPROC proc;
				/* exchndl.dll is built without UNICODE */
				char debug_dir[MAX_PATH];
				printf("Loaded exchndl.dll\n");
				/* Temporarily override exchndl.dll's logfile
				 * to something sane (Pidgin will override it
				 * again when it initializes) */
				proc = GetProcAddress(hmod, "SetLogFile");
				if (proc) {
					if (GetTempPathA(sizeof(debug_dir) * sizeof(char), debug_dir) != 0) {
						strcat(debug_dir, "pidgin.RPT");
						printf(" Setting exchndl.dll LogFile to %s\n",
							debug_dir);
						(proc)(debug_dir);
					}
				}
				proc = GetProcAddress(hmod, "SetDebugInfoDir");
				if (proc) {
					char *pidgin_dir_ansi = NULL;
					tmp[0] = _T('\0');
#ifdef _UNICODE
					i = WideCharToMultiByte(CP_ACP, 0, pidgin_dir,
						-1, NULL, 0, NULL, NULL);
					if (i != 0) {
						pidgin_dir_ansi = malloc(i * sizeof(char));
						i = WideCharToMultiByte(CP_ACP, 0, pidgin_dir,
							-1, pidgin_dir_ansi, i, NULL, NULL);
						if (i == 0) {
							free(pidgin_dir_ansi);
							pidgin_dir_ansi = NULL;
						}
					}
#else
					pidgin_dir_ansi = pidgin_dir;
#endif
					if (pidgin_dir_ansi != NULL) {
						_snprintf(debug_dir, sizeof(debug_dir) / sizeof(char),
							"%s\\pidgin-%s-dbgsym",
							pidgin_dir_ansi,  VERSION);
						debug_dir[sizeof(debug_dir) / sizeof(char) - 1] = '\0';
						printf(" Setting exchndl.dll DebugInfoDir to %s\n",
							debug_dir);
						(proc)(debug_dir);
#ifdef _UNICODE
						free(pidgin_dir_ansi);
#endif
					}
				}

			}

			tmp[0] = _T('\0');
		}
	} else {
		DWORD dw = GetLastError();
		const TCHAR *err_msg = get_win32_error_message(dw);
		_sntprintf(errbuf, 512,
			_T("Error getting module filename.\nError: (%u) %s"),
			(UINT) dw, err_msg);
		_tprintf(_T("%s\n"), errbuf);
		MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
		pidgin_dir[0] = _T('\0');
	}

	/* Determine if we're running in portable mode */
	if (wcsstr(cmdLine, L"--portable-mode")
			|| (exe_name != NULL && _tcsstr(exe_name, _T("-portable.exe")))) {
		printf("Running in PORTABLE mode.\n");
		portable_mode = TRUE;
	}

	if (portable_mode)
		portable_mode_dll_prep(pidgin_dir);
	else if (!getenv("PIDGIN_NO_DLL_CHECK"))
		dll_prep(pidgin_dir);

	winpidgin_set_locale();

	winpidgin_add_stuff_to_path();

	/* If help, version or multiple flag used, do not check Mutex */
	if (!help && !version)
		if (!winpidgin_set_running(getenv("PIDGIN_MULTI_INST") == NULL && !multiple))
			return 0;

	/* Now we are ready for Pidgin .. */
	if ((hmod = LoadLibrary(_T("pidgin.dll"))))
		pidgin_main = (LPFNPIDGINMAIN) GetProcAddress(hmod, "pidgin_main");

	if (!pidgin_main) {
		DWORD dw = GetLastError();
		BOOL mod_not_found = (dw == ERROR_MOD_NOT_FOUND || dw == ERROR_DLL_NOT_FOUND);
		const TCHAR *err_msg = get_win32_error_message(dw);

		_sntprintf(errbuf, 512, _T("Error loading pidgin.dll.\nError: (%u) %s%s%s"),
			(UINT) dw, err_msg,
			mod_not_found ? _T("\n") : _T(""),
			mod_not_found ? _T("This probably means that GTK+ can't be found.") : _T(""));
		_tprintf(_T("%s\n"), errbuf);
		MessageBox(NULL, errbuf, _T("Error"), MB_OK | MB_TOPMOST);

		return 0;
	}

	/* Convert argv to utf-8*/
	szArglist = CommandLineToArgvW(cmdLine, &j);
	pidgin_argc = j;
	pidgin_argv = malloc(pidgin_argc* sizeof(char*));
	k = 0;
	for (i = 0; i < j; i++) {
		success = FALSE;
		/* Remove the --portable-mode arg from the args passed to pidgin so it doesn't choke */
		if (wcsstr(szArglist[i], L"--portable-mode") == NULL) {
			int len = WideCharToMultiByte(CP_UTF8, 0, szArglist[i],
				-1, NULL, 0, NULL, NULL);
			if (len != 0) {
				char *arg = malloc(len * sizeof(char));
				len = WideCharToMultiByte(CP_UTF8, 0, szArglist[i],
					-1, arg, len, NULL, NULL);
				if (len != 0) {
					pidgin_argv[k++] = arg;
					success = TRUE;
				}
			}
			if (!success)
				wprintf(L"Error converting argument '%s' to UTF-8\n",
					szArglist[i]);
		}
		if (!success)
			pidgin_argc--;
	}
	LocalFree(szArglist);


	return pidgin_main(hInstance, pidgin_argc, pidgin_argv);
}

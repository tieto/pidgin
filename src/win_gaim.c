/*
 *  win_gaim.c
 *
 *  Date: June, 2002
 *  Description: Entry point for win32 gaim, and various win32 dependant
 *  routines.
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <windows.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef int (CALLBACK* LPFNGAIMMAIN)(HINSTANCE, int, char**);
typedef void (CALLBACK* LPFNSETDLLDIRECTORY)(LPCTSTR);

/*
 *  PROTOTYPES
 */
static LPFNGAIMMAIN gaim_main = NULL;
static LPFNSETDLLDIRECTORY MySetDllDirectory = NULL;


static BOOL read_reg_string(HKEY key, char* sub_key, char* val_name, LPBYTE data, LPDWORD data_len) {
        HKEY hkey;
        BOOL ret = FALSE;
        int retv;

        if(ERROR_SUCCESS == RegOpenKeyEx(key, 
                                         sub_key, 
					 0,  KEY_QUERY_VALUE, &hkey)) {
                if(ERROR_SUCCESS == (retv=RegQueryValueEx(hkey, val_name, 0, NULL, data, data_len)))
                        ret = TRUE;
                else
                        printf("Could not read reg key '%s' subkey '%s' value: '%s'\nError: %u\n",
                               ((key == HKEY_LOCAL_MACHINE) ? "HKLM" : (key == HKEY_CURRENT_USER) ? "HKCU" : "???"),
                               sub_key, val_name, (UINT)GetLastError());
                RegCloseKey(key);
        }
        else
                printf("Could not open reg subkey: %s\nError: %u\n", sub_key, (UINT)GetLastError());

        return ret;
}

static void dll_prep() {
        char gtkpath[MAX_PATH];
        char path[MAX_PATH];
        DWORD plen = MAX_PATH;
        int gotreg=FALSE;
        HKEY hkey;
        HMODULE hmod;

        if(!(gotreg = read_reg_string((hkey=HKEY_LOCAL_MACHINE), "SOFTWARE\\GTK\\2.0", "Path", (LPBYTE)&gtkpath, &plen)))
                gotreg = read_reg_string((hkey=HKEY_CURRENT_USER), "SOFTWARE\\GTK\\2.0", "Path", (LPBYTE)&gtkpath, &plen);

        if(!gotreg)
                return;

        /* Determine GTK+ dll path .. */
        if(!read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "DllPath", (LPBYTE)&path, &plen)) {
                char version[10];
                char inst[10];
                DWORD len = 10;

                strcpy(path, gtkpath);
                if(read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "Version", (LPBYTE)&version, &len) &&
                   read_reg_string(hkey, "SOFTWARE\\GTK\\2.0", "Installer", (LPBYTE)&inst, &len)) {
                        if(strcmp(version, "2.2.2") >= 0 &&
                           strcmp(inst, "NSIS") == 0) {
                                strcat(path, "\\bin");
                        }
                        else
                                strcat(path, "\\lib");
                }
                else
                        strcat(path, "\\lib");
        }

        printf("GTK+ path found: %s\n", path);

        if((hmod=GetModuleHandle("kernel32.dll"))) {
                MySetDllDirectory = (LPFNSETDLLDIRECTORY)GetProcAddress(hmod, "SetDllDirectory");
                if(!MySetDllDirectory)
                        printf("SetDllDirectory not supported\n");
        }
        else
                printf("Error getting kernel32.dll module handle\n");

        /* For Windows XP SP1+ / Server 2003 we use SetDllDirectory to avoid dll hell */
        if(MySetDllDirectory) {
                printf("Using SetDllDirectory\n");
                MySetDllDirectory(path);
        }

        /* For the rest, we set the current directory and make sure SafeDllSearch is set
           to 0 where needed. */
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
                if((osinfo.dwMajorVersion == 5 &&
                    osinfo.dwMinorVersion == 0 &&
                    strcmp(osinfo.szCSDVersion, "Service Pack 3") >= 0) ||
                   (osinfo.dwMajorVersion == 5 &&
                    osinfo.dwMinorVersion == 1 &&
                    strcmp(osinfo.szCSDVersion, "") >= 0)
                   ) {
                        DWORD regval = 1;
                        DWORD reglen = sizeof(DWORD);

                        printf("Using Win2k (SP3+) / WinXP (No SP).. Checking SafeDllSearch\n");
                        read_reg_string(HKEY_LOCAL_MACHINE,
                                        "System\\CurrentControlSet\\Control\\Session Manager",
                                        "SafeDllSearchMode",
                                        (LPBYTE)&regval,
                                        &reglen);

                        if(regval != 0) {
                                printf("Trying to set SafeDllSearchMode to 0\n");
                                regval = 0;
                                if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                                "System\\CurrentControlSet\\Control\\Session Manager", 
                                                0,  KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
                                        if(RegSetValueEx(hkey, 
                                                         "SafeDllSearchMode",
                                                         0,
                                                         REG_DWORD,
                                                         (LPBYTE) &regval,
                                                         sizeof(DWORD)) != ERROR_SUCCESS)
                                                printf("Error writing SafeDllSearchMode. Error: %u\n",
                                                       (UINT)GetLastError());
                                        RegCloseKey(hkey);
                                }
                                else
                                        printf("Error opening Session Manager key for writing. Error: %u\n",
                                               (UINT)GetLastError());
                        }
                        else
                                printf("SafeDllSearchMode is set to 0\n");
                }/*end else*/
        }
}

static char* wgaim_lcid_to_posix(LCID lcid) {
        switch(lcid) {
        case 1026: return "bg"; /* bulgarian */
        case 2125: return "my_MM"; /* burmese (Myanmar) */
        case 1027: return "ca"; /* catalan */
        case 1050: return "hr"; /* croatian */
        case 1029: return "cs"; /* czech */
        case 1030: return "da"; /* danish */
        case 1043: return "nl"; /* dutch - netherlands */
        case 1033: return "en"; /* english - us */
        case 3081: return "en_AU"; /* english - australia */
        case 4105: return "en_CA"; /* english - canada */
        case 2057: return "en_GB"; /* english - great britain */
        case 1035: return "fi"; /* finnish */
        case 1036: return "fr"; /* french - france */
        case 1031: return "de"; /* german - germany */
        case 1032: return "el"; /* greek */
        case 1037: return "he"; /* hebrew */
        case 1081: return "hi"; /* hindi */
        case 1038: return "hu"; /* hungarian */
        case 1040: return "it"; /* italian - italy */
        case 1041: return "ja"; /* japanese */
        case 1042: return "ko"; /* korean */
        case 1063: return "lt"; /* lithuanian */
        case 1071: return "mk"; /* macedonian */
        case 1044: return "no"; /* norwegian */
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
        case 1052: return "sq"; /* albanian */
        case 1053: return "sv"; /* swedish */
        case 1054: return "th"; /* thai */
        case 1028: return "zh_TW"; /* chinese - taiwan (traditional) */
        case 1055: return "tr"; /* turkish */
        case 1058: return "uk"; /* ukrainian */
        case 1066: return "vi"; /* vietnamese */
        default:
                return NULL;
        }
}

/* Determine and set Gaim locale as follows (in order of priority):
   - Check GAIMLANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
static const char* wgaim_get_locale() {
        const char* locale=NULL;
        char data[10];
        DWORD datalen = 10;
        LCID lcid;

        /* Check if user set GAIMLANG env var */
        if((locale = getenv("GAIMLANG")))
                return locale;

        if(read_reg_string(HKEY_CURRENT_USER, "SOFTWARE\\gaim", "Installer Language", (LPBYTE)&data, &datalen)) {
                if((locale = wgaim_lcid_to_posix(atoi(data))))
                        return locale;
        }

        lcid = GetUserDefaultLCID();
        if((locale = wgaim_lcid_to_posix(lcid)))
                return locale;

		return "en";
}

static void wgaim_set_locale() {
        const char* locale=NULL;
        char envstr[25];

        locale = wgaim_get_locale();

        snprintf(envstr, 25, "LANG=%s", locale);
        printf("Setting locale: %s\n", envstr);
        putenv(envstr);
}

static BOOL wgaim_set_running() {
	HANDLE h;

	if((h=CreateMutex(NULL, FALSE, "gaim_is_running"))) {
		if(GetLastError() == ERROR_ALREADY_EXISTS) {
			MessageBox(NULL, "An instance of Gaim is already running", NULL, MB_OK | MB_TOPMOST);
			return FALSE;
		}
	}
	return TRUE;
}

#ifdef __GNUC__
#  ifndef _stdcall
#    define _stdcall  __attribute__((stdcall))
#  endif
#endif

int _stdcall
WinMain (struct HINSTANCE__ *hInstance, 
	 struct HINSTANCE__ *hPrevInstance,
	 char               *lpszCmdLine,
	 int                 nCmdShow)
{
        char errbuf[512];
        char gaimdir[MAX_PATH];
        HMODULE hmod;

        /* If debug flag used, create console for output */
        if(strstr(lpszCmdLine, "-d")) {
                if(AllocConsole())
                        freopen ("CONOUT$", "w", stdout);
        }

        /* Load exception handler if we have it */
        if(GetModuleFileName(NULL, gaimdir, MAX_PATH) != 0) {
                char *tmp = gaimdir;
                char *prev = NULL;

                while((tmp=strchr(tmp, '\\'))) {
                        prev = tmp;
                        tmp+=1;
                }
                if(prev) {
                        prev[0] = '\0';
                        strcat(gaimdir, "\\exchndl.dll");
                        if(LoadLibrary(gaimdir))
                                printf("Loaded exchndl.dll\n");
                }
        }
        else {
                snprintf(errbuf, 512, "Error getting module filename. Error: %u", (UINT)GetLastError());
                MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
        }

        if(!getenv("GAIM_NO_DLL_CHECK"))
                dll_prep();

        wgaim_set_locale();
		if(!getenv("GAIM_MULTI_INST") && !wgaim_set_running())
			return 0;

        /* Now we are ready for Gaim .. */
        if((hmod=LoadLibrary("gaim.dll"))) {
                gaim_main = (LPFNGAIMMAIN)GetProcAddress(hmod, "gaim_main");
        }

        if(!gaim_main) {
                snprintf(errbuf, 512, "Error loading gaim.dll. Error: %u", (UINT)GetLastError());
                MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
                return 0;
        }
        else
                return gaim_main (hInstance, __argc, __argv);
}


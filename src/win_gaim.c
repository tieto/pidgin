/*
 *  win_gaim.c
 *
 *  Date: June, 2002
 *  Description: Entry point for win32 gaim, and various win32 dependant
 *  routines.
 *
 *  Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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

/*
 *  PROTOTYPES
 */
static int (*gaim_main)( HINSTANCE, int, char** ) = NULL;
static void (*MySetDllDirectory)(LPCTSTR lpPathName) = NULL;


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
                        ret = FALSE;
                RegCloseKey(key);
        }
        return ret;
}

static void run_dll_prep() {
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

        if((hmod=LoadLibrary("kernel32.dll"))) {
                MySetDllDirectory = (void*)GetProcAddress(hmod, "SetDllDirectory");
        }

        /* For Windows XP SP1 / Server 2003 we use SetDllDirectory to avoid dll hell */
        if(MySetDllDirectory)
                MySetDllDirectory(path);
        /* For the rest, we set the current directory */
        else {
                OSVERSIONINFO osinfo;

                SetCurrentDirectory(path);
                /* For Windows 2000 SP3 and higher:
                 * If SafeDllSearchMode is set to 1, Windows system directories are
                 * searched for dlls before the current directory. Therefore we set it
                 * to 0.
                 */
                osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
                GetVersionEx(&osinfo);
                if(osinfo.dwMajorVersion == 5 &&
                   osinfo.dwMinorVersion == 0 &&
                   strcmp(osinfo.szCSDVersion, "Service Pack 3") >= 0) {
                        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                                                         "System\\CurrentControlSet\\Control\\Session Manager", 
                                                         0,  KEY_SET_VALUE, &hkey)) {
                                DWORD regval = 0;
                                RegSetValueEx(hkey, 
                                              "SafeDllSearchMode",
                                              0,
                                              REG_DWORD,
                                              (LPBYTE) &regval,
                                              sizeof(regval));
                                RegCloseKey(hkey);
                        }
                }
        }
}

static char* wgaim_lcid_to_posix(LCID lcid) {
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
                return NULL;
        }
}

/* Determine and set Gaim locale as follows (in order of priority):
   - Check LANG env var
   - Check NSIS Installer Language reg value
   - Use default user locale
*/
static void wgaim_set_locale() {
        HKEY hkey;
	char* locale=NULL;
        char envstr[25];
        LCID lcid;

        /* Check if user set LANG env var */
        if((locale = (char*)getenv("LANG"))) {
                goto finish;
        }

        /* Check reg key set at install time */
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, 
                                         "SOFTWARE\\gaim", 
					 0,  KEY_QUERY_VALUE, &hkey)) {
                BYTE data[10];
                DWORD ds = 10;
                if(ERROR_SUCCESS == RegQueryValueEx(hkey, "Installer Language", 0, NULL, (LPBYTE)&data, &ds)) {
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
        putenv(envstr);
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

        /* If GAIM_NO_DLL_CHECK is set, don't run the dll check */
        if(!getenv("GAIM_NO_DLL_CHECK"))
                run_dll_prep();

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
                        LoadLibrary(gaimdir);
                }
        }
        else {
                snprintf(errbuf, 512, "Error getting module filename. Error: %u", (UINT)GetLastError());
                MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
        }

        /* Set Gaim locale */
        wgaim_set_locale();

        /* Set global file mode to binary so that we don't do any dos file translations */
        _fmode = _O_BINARY;

        /* Now we are ready for Gaim .. */
        if((hmod=LoadLibrary("gaim.dll"))) {
                gaim_main = (void*)GetProcAddress(hmod, "gaim_main");
        }

        if(!gaim_main) {
                snprintf(errbuf, 512, "Error loading gaim.dll. Error: %u", (UINT)GetLastError());
                MessageBox(NULL, errbuf, NULL, MB_OK | MB_TOPMOST);
                return 0;
        }
        else
                return gaim_main (hInstance, __argc, __argv);
}


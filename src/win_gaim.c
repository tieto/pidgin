/*
 *  win_aim.c
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: June, 2002
 *  Description: Entry point for win32 gaim, and various win32 dependant
 *  routines.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 *  GLOBALS
 */
__declspec(dllimport) HINSTANCE gaimexe_hInstance;

/*
 *  LOCALS
 */
static char msg1[] = "The following duplicate of ";
static char msg2[] = " has been found in your dll search path and will likely\x0d\x0a"
              "cause Gaim to malfunction:\x0d\x0a\x0d\x0a";

static char msg3[] = "\x0d\x0a\x0d\x0aWould you like to rename this dll to ";
static char msg4[] = ".prob in order to avoid any possible conflicts?\x0d\x0a"
                     "\x0d\x0a"
                     "Note: Doing so will likely cause the application that installed this dll to stop functioning.\x0d\x0a"
                     "You may wish to inform the author of the responsible application so that future versions\x0d\x0a"
                     "do not cause 'Dll Hell'.";

/*
 *  PROTOTYPES
 */
int (*gaim_main)( HINSTANCE, int, char** ) = NULL;

static void check_dll(char* dll, char* orig) {
        char tmp[MAX_PATH];
        char *last;

        if(SearchPath(NULL, dll, NULL, MAX_PATH, tmp, &last)) {
                char* patha = (char*)malloc(strlen(orig) + strlen(dll) + 4);
                strcpy(patha, orig);
                strcat(patha, "\\");
                strcat(patha, dll);
                /* Make sure that 2 paths are not the same */
                if(strcasecmp(patha, tmp) != 0) {
                        char *warning = (char*)malloc(strlen(msg1)+
                                                      strlen(msg2)+
                                                      strlen(msg3)+
                                                      strlen(msg4)+
                                                      strlen(tmp)+
                                                      (strlen(dll)*2)+4);
                        sprintf(warning, "%s%s%s%s%s%s%s", msg1, dll, msg2, tmp, msg3, dll, msg4);
                        if(MessageBox(NULL, warning, "Gaim Warning", MB_YESNO | MB_TOPMOST)==IDYES) {
                                char *newname = (char*)malloc(strlen(tmp)+6);
                                /* Rename offending dll */
                                sprintf(newname, "%s%s", tmp, ".prob");
                                if(rename(tmp, newname) != 0) {
                                        MessageBox(NULL, "Error renaming file.", NULL, MB_OK | MB_TOPMOST);
                                }
                                else
                                        check_dll(dll, orig);
                                free(newname);
                        }
                        free(warning);
                }
                free(patha);
        }
}

static void dll_hell_check(char* gtkpath) {
        HANDLE myHandle;
        WIN32_FIND_DATA fd;
        char* srchstr = (char*)malloc(strlen(gtkpath) + 8);
        
        sprintf(srchstr, "%s%s", gtkpath, "\\*.dll");
        myHandle = FindFirstFile(srchstr, &fd );
        if(myHandle != INVALID_HANDLE_VALUE) {
                check_dll(fd.cFileName, gtkpath);
                while(FindNextFile(myHandle, &fd)) {
                        check_dll(fd.cFileName, gtkpath);
                }
        }
        free(srchstr);
}

BOOL read_reg_string(HKEY key, char* sub_key, char* val_name, LPBYTE data, LPDWORD data_len) {
        HKEY hkey;
        BOOL ret = FALSE;
        int retv;

        if(ERROR_SUCCESS == RegOpenKeyEx(key, 
                                         sub_key, 
					 0,  KEY_QUERY_VALUE, &hkey)) {
                if(ERROR_SUCCESS == (retv=RegQueryValueEx(hkey, val_name, 0, NULL, data, data_len)))
                        ret = TRUE;
                else {
                        printf("Error reading registry string value: %d\n", retv);
                }
                RegCloseKey(key);
        }
        return ret;
}

void run_dll_check() {
        char path[MAX_PATH];
        DWORD plen = MAX_PATH;
        int gotreg=FALSE;

        /* Dll Hell Check.. Only run check if we are the same gaim as found in the
           gaim registry key */
        if(!(gotreg = read_reg_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\gaim", "", (LPBYTE)&path, &plen)))
                gotreg = read_reg_string(HKEY_CURRENT_USER, "SOFTWARE\\gaim", "", (LPBYTE)&path, &plen);

        if(gotreg) {
                char modpath[MAX_PATH];
                
                strcat(path, "\\gaim.exe");
                GetModuleFileName(NULL, modpath, MAX_PATH);
                if(strcasecmp(path, modpath) == 0) {
                        plen = MAX_PATH;
                        if(!(gotreg = read_reg_string(HKEY_LOCAL_MACHINE, "SOFTWARE\\GTK\\2.0", "Path", (LPBYTE)&path, &plen)))
                                gotreg = read_reg_string(HKEY_CURRENT_USER, "SOFTWARE\\GTK\\2.0", "Path", (LPBYTE)&path, &plen);
                        if(gotreg) {
                                strcat(path, "\\bin");
                                dll_hell_check(path);
                        }
                }
        }
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
        char gaimdir[MAX_PATH];
        char *point;
        HMODULE hmod;

        /* If GAIM_NO_DLL_CHECK is set, don't run the dll check */
        if(!getenv("GAIM_NO_DLL_CHECK"))
                run_dll_check();

        /* Load exception handler if we have it */
        GetModuleFileName(NULL, gaimdir, MAX_PATH);
        if((point=strstr(gaimdir, "gaim.exe"))) {
                point[0] = '\0';
                strcat(gaimdir, "exchndl.dll");
                LoadLibrary(gaimdir);
        }

        /* Now we are ready for Gaim .. */
        if((hmod=LoadLibrary("gaim.dll"))) {
                gaim_main = (void*)GetProcAddress(hmod, "gaim_main");
        }

        if(!gaim_main) {
                MessageBox(NULL, "Error loading gaim.dll entry point.", NULL, MB_OK | MB_TOPMOST);
                return 0;
        }
        else
                return gaim_main (hInstance, __argc, __argv);
}


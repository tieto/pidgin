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
#include <glib.h>

/*
 *  GLOBALS
 */
__declspec(dllimport) HINSTANCE gaimexe_hInstance;

/*
 *  LOCALS
 */

/*
 *  PROTOTYPES
 */
extern int gaim_main( int, char** );
extern char* wgaim_install_dir();

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
        char* drmingw;
	gaimexe_hInstance = hInstance;

	/* Load exception handler if we have it */
	drmingw = g_build_filename(wgaim_install_dir(), "exchndl.dll", NULL);
	LoadLibrary(drmingw);
	g_free(drmingw);

	return gaim_main (__argc, __argv);
}


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
	gaimexe_hInstance = hInstance;
	return gaim_main (__argc, __argv);
}


/**
 * IdleTracker - a DLL that tracks the user's idle input time
 *               system-wide.
 *
 * Usage
 * =====
 * - call IdleTrackerInit() when you want to start monitoring.
 * - call IdleTrackerTerm() when you want to stop monitoring.
 * - to get the time past since last user input, do the following:
 *    GetTickCount() - IdleTrackerGetLastTickCount()
 *
 * Author: Sidney Chong
 * Date: 25/5/2000
 * Version: 1.0
 *
 * From: http://www.codeproject.com/dll/trackuseridle.asp
 *
 * Modified by: Herman Bloggs <hermanator12002@yahoo.com>
 * Date: 11/5/2002
 **/

#include <windows.h>
#ifndef __GNUC__
#include <crtdbg.h>
#endif

/**
 * The following global data is SHARED among all instances of the DLL
 * (processes); i.e., these are system-wide globals.
 **/ 
#ifndef __GNUC__
#pragma data_seg(".IdleTrac")	// you must define as SHARED in .def
HHOOK 	g_hHkKeyboard = NULL;	// handle to the keyboard hook
HHOOK 	g_hHkMouse = NULL;	// handle to the mouse hook
DWORD	g_dwLastTick = 0;	// tick time of last input event
LONG	g_mouseLocX = -1;	// x-location of mouse position
LONG	g_mouseLocY = -1;	// y-location of mouse position
#pragma data_seg()
#pragma comment(linker, "/section:.IdleTrac,rws")
#else
/* Shared data section for mingw */
asm (".section .IdleTrac,\"d\"\n" \
     "_g_hHkKeyboard:\n\t.long 0\n" \
     "_g_hHkMouse:\n\t.long 0\n" \
     "_g_dwLastTick:\n\t.long 0\n" \
     "_g_mouseLocX:\n\t.long -1\n" \
     "_g_mouseLocY:\n\t.long -1\n" \
     );
extern HHOOK 	g_hHkKeyboard;
extern HHOOK 	g_hHkMouse;
extern DWORD	g_dwLastTick;
extern LONG	g_mouseLocX;
extern LONG	g_mouseLocY;

typedef struct tagMOUSEHOOKSTRUCT {
    POINT pt; 
    HWND  hwnd; 
    UINT  wHitTestCode; 
    DWORD dwExtraInfo; 
} MOUSEHOOKSTRUCT; 

#endif
HINSTANCE g_hInstance;

/**
 * Get tick count of last keyboard or mouse event
 **/
__declspec(dllexport) DWORD IdleTrackerGetLastTickCount()
{
	return g_dwLastTick;
}

/**
 * Keyboard hook: record tick count
 **/
LRESULT CALLBACK KeyboardTracker(int code, WPARAM wParam, LPARAM lParam)
{
	if (code==HC_ACTION) {
		g_dwLastTick = GetTickCount();
	}
	return CallNextHookEx(g_hHkKeyboard, code, wParam, lParam);
}

/**
 * Mouse hook: record tick count
 **/
LRESULT CALLBACK MouseTracker(int code, WPARAM wParam, LPARAM lParam)
{
	if (code==HC_ACTION) {
		MOUSEHOOKSTRUCT* pStruct = (MOUSEHOOKSTRUCT*)lParam;
		//we will assume that any mouse msg with the same locations as spurious
		if (pStruct->pt.x != g_mouseLocX || pStruct->pt.y != g_mouseLocY)
		{
			g_mouseLocX = pStruct->pt.x;
			g_mouseLocY = pStruct->pt.y;
			g_dwLastTick = GetTickCount();
		}
	}
	return CallNextHookEx(g_hHkMouse, code, wParam, lParam);
}

/**
 * Initialize DLL: install kbd/mouse hooks.
 **/
__declspec(dllexport) BOOL IdleTrackerInit()
{
	if (g_hHkKeyboard == NULL) {
		g_hHkKeyboard = SetWindowsHookEx(WH_KEYBOARD, KeyboardTracker, g_hInstance, 0);
	}
	if (g_hHkMouse == NULL) {
		g_hHkMouse = SetWindowsHookEx(WH_MOUSE, MouseTracker, g_hInstance, 0);
	}
#ifndef __GNUC__
	_ASSERT(g_hHkKeyboard);
	_ASSERT(g_hHkMouse);
#endif
	g_dwLastTick = GetTickCount(); // init count

	if (!g_hHkKeyboard || !g_hHkMouse)
		return FALSE;
	else
		return TRUE;
}

/**
 * Terminate DLL: remove hooks.
 **/
__declspec(dllexport) void IdleTrackerTerm()
{
	BOOL bResult;
	if (g_hHkKeyboard)
	{
		bResult = UnhookWindowsHookEx(g_hHkKeyboard);
#ifndef __GNUC__
		_ASSERT(bResult);
#endif
		g_hHkKeyboard = NULL;
	}
	if (g_hHkMouse)
	{
		bResult = UnhookWindowsHookEx(g_hHkMouse);
#ifndef __GNUC__
		_ASSERT(bResult);
#endif
		g_hHkMouse = NULL;
	}
}

/**
 * DLL's entry point
 **/
int WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch(dwReason) {
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls(hInstance);
			g_hInstance = hInstance;
			break;
		case DLL_PROCESS_DETACH:
			//we do an unhook here just in case the user has forgotten.
			IdleTrackerTerm();
			break;
	}
	return TRUE;
}

#if 0
/**
 * This is to prevent the CRT from loading, thus making this a smaller
 * and faster dll.
 **/
extern "C" BOOL __stdcall _DllMainCRTStartup( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    return DllMain( hinstDLL, fdwReason, lpvReserved );
}
#endif

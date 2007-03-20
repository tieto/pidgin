/*
 *  idletrack.c
 *
 *  Authors: mrgentry @ http://www.experts-exchange.com
 *           Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: February, 2003
 *  Description: Track user inactivity.
 *
 *  Andrew Whewell <awhewell@users.sourceforge.net> - 25th June 2004. Added
 *  support for GetLastInputInfo under Windows 2000 and above. This avoids having
 *  IDLETRACK.DLL hook itself into every process on the machine, which makes
 *  upgrades easier. The hook mechanism is also used by key loggers, so not
 *  using hooks doesn't put the willys up programs that keep an eye out for
 *  loggers.
 *
 *  Windows 9x doesn't have GetLastInputInfo - when Purple runs on these machines
 *  the code silently falls back onto the old hooking scheme.
 */
#define _WIN32_WINNT 0x0500
#include "idletrack.h"

#define EXPORT __declspec(dllexport)

static HANDLE hMapObject = NULL;
static DWORD *lastTime = NULL;
static HHOOK keyHook = NULL;
static HHOOK mouseHook = NULL;
static HINSTANCE g_hInstance = NULL;
static POINT g_point;

/* GetLastInputInfo address and module - if g_GetLastInputInfo == NULL then
 * we fall back on the old "hook the world" method. GetLastInputInfo was brought
 * in with Windows 2000 so Windows 9x will still hook everything.
 */
typedef BOOL (WINAPI *GETLASTINPUTINFO)(LASTINPUTINFO *);
static HMODULE g_user32 = NULL;
static GETLASTINPUTINFO g_GetLastInputInfo = NULL;

static DWORD* setup_shared_mem() {
	BOOL fInit;

	/* Set up the shared memory. */
	hMapObject = CreateFileMapping((HANDLE) 0xFFFFFFFF, /* use paging file        */
								   NULL,                /* no security attributes */
								   PAGE_READWRITE,      /* read/write access      */
								   0,                   /* size: high 32-bits     */
								   sizeof(DWORD),       /* size: low 32-bits      */
								   "timermem");         /* name of map object     */

	if(hMapObject == NULL)
		return NULL;

	/* The first process to attach initializes memory. */
	fInit = (GetLastError() != ERROR_ALREADY_EXISTS);

	/* Get a pointer to the file-mapped shared memory. */
	lastTime = (DWORD*) MapViewOfFile(hMapObject,     /* object to map view of    */
									  FILE_MAP_WRITE, /* read/write access        */
									  0,              /* high offset:  map from   */
									  0,              /* low offset:   beginning  */
									  0);             /* default: map entire file */

	if(lastTime == NULL)
		return NULL;

	*lastTime = GetTickCount();

	return lastTime;
}


static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
	if(!(code < 0)) {
		if(lastTime == NULL)
			lastTime = setup_shared_mem();

		if(lastTime)
			*lastTime = GetTickCount();
	}
	return CallNextHookEx(keyHook, code, wParam, lParam);
}


static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam) {
	/* We need to verify that the Mouse pointer has actually moved. */
	if(!(code < 0) &&
			!((g_point.x == ((MOUSEHOOKSTRUCT*) lParam)->pt.x) &&
			(g_point.y == ((MOUSEHOOKSTRUCT*) lParam)->pt.y))) {
		g_point.x = ((MOUSEHOOKSTRUCT*) lParam)->pt.x;
		g_point.y = ((MOUSEHOOKSTRUCT*) lParam)->pt.y;

		if(lastTime == NULL)
			lastTime = setup_shared_mem();

		if(lastTime)
			*lastTime = GetTickCount();
	}
	return CallNextHookEx(mouseHook, code, wParam, lParam);
}


EXPORT DWORD winpidgin_get_lastactive() {
	DWORD result = 0;

	/* If we have GetLastInputInfo then use it, otherwise use the hooks*/
	if(g_GetLastInputInfo != NULL) {
		LASTINPUTINFO lii;
		memset(&lii, 0, sizeof(lii));
		lii.cbSize = sizeof(lii);
		if(g_GetLastInputInfo(&lii)) {
			result = lii.dwTime;
		}
	} else {
		if(lastTime == NULL)
			lastTime = setup_shared_mem();

		if(lastTime)
			result = *lastTime;
	}

	return result;
}


EXPORT BOOL winpidgin_set_idlehooks() {
	/* Is GetLastInputInfo available?*/
	g_user32 = LoadLibrary("user32.dll");
	if(g_user32) {
		g_GetLastInputInfo = (GETLASTINPUTINFO) GetProcAddress(g_user32, "GetLastInputInfo");
	}

	/* If we couldn't find GetLastInputInfo then fall back onto the hooking scheme*/
	if(g_GetLastInputInfo == NULL) {
		/* Set up the shared memory.*/
		lastTime = setup_shared_mem();
		if(lastTime == NULL)
			return FALSE;
		*lastTime = GetTickCount();

	/* Set up the keyboard hook.*/
		keyHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, g_hInstance, 0);
		if(keyHook == NULL) {
			UnmapViewOfFile(lastTime);
			CloseHandle(hMapObject);
			return FALSE;
		}

		/* Set up the mouse hook.*/
		mouseHook = SetWindowsHookEx(WH_MOUSE, MouseProc, g_hInstance, 0);
		if(mouseHook == NULL) {
			UnhookWindowsHookEx(keyHook);
			UnmapViewOfFile(lastTime);
			CloseHandle(hMapObject);
			return FALSE;
		}
	}

	return TRUE;
}


EXPORT void winpidgin_remove_idlehooks() {
	if(g_user32 != NULL)
		FreeLibrary(g_user32);
	if(keyHook)
		UnhookWindowsHookEx(keyHook);
	if(mouseHook)
		UnhookWindowsHookEx(mouseHook);
	if(lastTime)
		UnmapViewOfFile(lastTime);
	if(hMapObject)
		CloseHandle(hMapObject);
}

/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
	switch(dwReason) {
		case DLL_PROCESS_ATTACH:
			g_hInstance = hInstance;
			g_point.x = 0;
			g_point.y = 0;
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

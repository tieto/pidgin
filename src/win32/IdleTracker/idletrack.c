/*
 *  idletrack.c
 *
 *  Authors: mrgentry @ http://www.experts-exchange.com
 *           Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: February, 2003
 *  Description: Track user inactivity.
 */
#include <windows.h>

#define EXPORT __declspec(dllexport)

/* from msdn docs */
typedef struct tagMOUSEHOOKSTRUCT {
    POINT pt; 
    HWND  hwnd; 
    UINT  wHitTestCode; 
    DWORD dwExtraInfo; 
} MOUSEHOOKSTRUCT;

static HANDLE hMapObject = NULL;
static DWORD *lastTime = NULL;
static HHOOK keyHook = NULL;
static HHOOK mouseHook = NULL;
static HINSTANCE g_hInstance = NULL;
static POINT g_point;

static DWORD* setup_shared_mem() {
	BOOL fInit;

	// Set up the shared memory.
	hMapObject = CreateFileMapping((HANDLE) 0xFFFFFFFF, // use paging file
				       NULL,                // no security attributes
				       PAGE_READWRITE,      // read/write access
				       0,                   // size: high 32-bits
				       sizeof(DWORD),       // size: low 32-bits
				       "timermem");         // name of map object
	
	if (hMapObject == NULL)
		return NULL;
	
	// The first process to attach initializes memory.
	fInit = (GetLastError() != ERROR_ALREADY_EXISTS);
	
	// Get a pointer to the file-mapped shared memory.
	lastTime = (DWORD*) MapViewOfFile(hMapObject,     // object to map view of
					  FILE_MAP_WRITE, // read/write access
					  0,              // high offset:  map from
					  0,              // low offset:   beginning
					  0);             // default: map entire file
	
	if (lastTime == NULL)
		return NULL;
	
	*lastTime = GetTickCount();
	
	return lastTime;
}


LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
	if (code < 0)
		return CallNextHookEx(keyHook, code, wParam, lParam);
	if (lastTime == NULL)
		lastTime = setup_shared_mem();
	
	if (lastTime)
		*lastTime = GetTickCount();
	
	return CallNextHookEx(keyHook, code, wParam, lParam);
}


LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam) {
	if (code < 0)
		return CallNextHookEx(mouseHook, code, wParam, lParam);

	/* We need to verify that the Mouse pointer has actually moved. */
	if((g_point.x == ((MOUSEHOOKSTRUCT*)lParam)->pt.x) &&
	   (g_point.y == ((MOUSEHOOKSTRUCT*)lParam)->pt.y))
		return 0;

	g_point.x = ((MOUSEHOOKSTRUCT*)lParam)->pt.x;
	g_point.y = ((MOUSEHOOKSTRUCT*)lParam)->pt.y;
	
	if (lastTime == NULL)
		lastTime = setup_shared_mem();
	
	if (lastTime)
		*lastTime = GetTickCount();
	
	return CallNextHookEx(mouseHook, code, wParam, lParam);
}


EXPORT DWORD wgaim_get_lastactive() {
	if (lastTime == NULL)
		lastTime = setup_shared_mem();
	
	if (lastTime)
		return *lastTime;
	
	return 0;
}


EXPORT BOOL wgaim_set_idlehooks() {
	// Set up the shared memory.
	lastTime = setup_shared_mem();
	if (lastTime == NULL)
		return FALSE;
	*lastTime = GetTickCount();
	
	// Set up the keyboard hook.
	keyHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, g_hInstance, 0);
	if (keyHook == NULL) {
		UnmapViewOfFile(lastTime);
		CloseHandle(hMapObject);
		return FALSE;
	}
	
	// Set up the mouse hook.
	mouseHook = SetWindowsHookEx(WH_MOUSE, MouseProc, g_hInstance, 0);
	if (mouseHook == NULL) {
		UnhookWindowsHookEx(keyHook);
		UnmapViewOfFile(lastTime);
		CloseHandle(hMapObject);
		return FALSE;
	}
	
	return TRUE;
}


EXPORT void wgaim_remove_idlehooks() {
	if (keyHook)
		UnhookWindowsHookEx(keyHook);
	if (mouseHook)
		UnhookWindowsHookEx(mouseHook);
	if (lastTime)
		UnmapViewOfFile(lastTime);
	if (hMapObject)
		CloseHandle(hMapObject);
}

int WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
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

/*
 *  wintransparency.h
 */

#ifndef _WINTRANSPARENCY_H_
#define _WINTRANSPARENCY_H_
#include <gdk/gdkevents.h>

extern int alpha;

BOOL WINAPI SetLayeredWindowAttributes(HWND,COLORREF,BYTE,DWORD);

/* These defines aren't found in the current version of mingw */
#ifndef LWA_ALPHA
#define LWA_ALPHA               0x00000002
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED           0x00080000
#endif


extern GdkFilterReturn wgaim_window_filter(GdkXEvent *xevent, 
					   GdkEvent *event, 
					   gpointer data);

/* Needed for accessing global variables outside the current module */

extern void wgaim_init(void);

#define unlink _unlink
#define bzero( dest, size ) memset( ## dest ##, 0, ## size ## )
#define sleep(x) Sleep((x)*1000)
#define snprintf _snprintf
#define vsnprintf _vsnprintf

#endif /* _WINTRANSPARENCY_H_ */


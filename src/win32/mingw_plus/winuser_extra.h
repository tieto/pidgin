/* 
 * winuser_extra.h
 *
 * Description: The following is missing from mingw 1.1
 */

#ifndef _WINUSER_EXTRA_H_
#define _WINUSER_EXTRA_H_

/* From MSDN Lib - Mingw dosn't have */
/*#if(WINVER >= 0x0500)*/
#define FLASHW_STOP         0
#define FLASHW_CAPTION      0x00000001
#define FLASHW_TRAY         0x00000002
#define FLASHW_ALL          (FLASHW_CAPTION | FLASHW_TRAY)
#define FLASHW_TIMER        0x00000004
#define FLASHW_TIMERNOFG    0x0000000C
/*#endif /* WINVER >= 0x0500 */


/* From MSDN Lib - Mingw dosn't have */
typedef struct {
    UINT  cbSize;
    HWND  hwnd;
    DWORD dwFlags;
    UINT  uCount;
    DWORD dwTimeout;
} FLASHWINFO, *PFLASHWINFO;

/* These defines aren't found in the current version of mingw */
#ifndef LWA_ALPHA
#define LWA_ALPHA               0x00000002
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED           0x00080000
#endif

#endif /* _WINUSER_EXTRA_H_ */

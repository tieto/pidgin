/*
 * idletrack.h
 */
#include <windows.h>

extern DWORD wgaim_get_lastactive(void);
extern BOOL wgaim_set_idlehooks(void);
extern void wgaim_remove_idlehooks(void);

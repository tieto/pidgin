/*
 * idletrack.h
 */
#include <windows.h>

DWORD wgaim_get_lastactive(void);
BOOL wgaim_set_idlehooks(void);
void wgaim_remove_idlehooks(void);

/*
 * idletrack.h
 */
#include <windows.h>

DWORD winpidgin_get_lastactive(void);
BOOL winpidgin_set_idlehooks(void);
void winpidgin_remove_idlehooks(void);

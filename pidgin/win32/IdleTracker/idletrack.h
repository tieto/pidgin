/*
 * idletrack.h
 */
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

DWORD winpidgin_get_lastactive(void);
BOOL winpidgin_set_idlehooks(void);
void winpidgin_remove_idlehooks(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

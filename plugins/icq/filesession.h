
#ifndef _FILE_SESSION_H
#define _FILE_SESSION_H

#include "icq.h"
#include "icqtypes.h"

icq_FileSession *icq_FileSessionNew(ICQLINK *);
void icq_FileSessionDelete(void *);
void icq_FileSessionSetStatus(icq_FileSession *, int);
icq_FileSession *icq_FindFileSession(ICQLINK *, unsigned long, unsigned long);
void icq_FileSessionSetHandle(icq_FileSession *, const char *);
void icq_FileSessionSetCurrentFile(icq_FileSession *, const char *);
void icq_FileSessionPrepareNextFile(icq_FileSession *);
void icq_FileSessionSendData(icq_FileSession *);

#endif

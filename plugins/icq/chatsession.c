
#include <stdlib.h>

#include "chatsession.h"
#include "list.h"

icq_ChatSession *icq_ChatSessionNew(ICQLINK *icqlink) 
{
  icq_ChatSession *p=(icq_ChatSession *)malloc(sizeof(icq_ChatSession));

  if (p)
  {
    p->remote_handle=0L;
    p->status=0;
    p->id=0L;
    p->icqlink=icqlink;
    list_insert(icqlink->icq_ChatSessions, 0, p);
  }
	
  return p;
}

void icq_ChatSessionDelete(void *p)
{
  free(p);
}

void icq_ChatSessionClose(icq_ChatSession *p)
{
  list_remove(p->icqlink->icq_ChatSessions, p);
  icq_ChatSessionDelete(p);
}

int _icq_FindChatSession(void *p, va_list data)
{
  DWORD uin=va_arg(data, DWORD);
  return (((icq_ChatSession *)p)->remote_uin == uin);
}

icq_ChatSession *icq_FindChatSession(ICQLINK *icqlink, DWORD uin)
{
  return list_traverse(icqlink->icq_ChatSessions,
    _icq_FindChatSession, uin);
}

void icq_ChatSessionSetStatus(icq_ChatSession *p, int status)
{
  p->status=status;
  if(p->id)
    if(p->icqlink->icq_RequestNotify)
      (*p->icqlink->icq_RequestNotify)(p->icqlink, p->id, ICQ_NOTIFY_CHAT, status, 0);
}


#include <stdlib.h>

#include "icq.h"
#include "icqlib.h"
#include "tcplink.h"
#include "chatsession.h"
#include "list.h"

icq_ChatSession *icq_ChatSessionNew(ICQLINK *icqlink) 
{
  icq_ChatSession *p=(icq_ChatSession *)malloc(sizeof(icq_ChatSession));

  if (p)
  {
    p->status=0;
    p->id=0L;
    p->icqlink=icqlink;
    p->tcplink=NULL;
    list_insert(icqlink->d->icq_ChatSessions, 0, p);
  }
	
  return p;
}

void icq_ChatSessionDelete(void *p)
{
  icq_ChatSession *pchat = (icq_ChatSession *)p;
  invoke_callback(pchat->icqlink, icq_ChatNotify)(pchat, CHAT_NOTIFY_CLOSE,
    0, NULL);
  
  free(p);
}

int _icq_FindChatSession(void *p, va_list data)
{
  DWORD uin=va_arg(data, DWORD);
  return (((icq_ChatSession *)p)->remote_uin == uin);
}

icq_ChatSession *icq_FindChatSession(ICQLINK *icqlink, DWORD uin)
{
  return list_traverse(icqlink->d->icq_ChatSessions,
    _icq_FindChatSession, uin);
}

void icq_ChatSessionSetStatus(icq_ChatSession *p, int status)
{
  p->status=status;
  if(p->id)
    invoke_callback(p->icqlink, icq_ChatNotify)(p, CHAT_NOTIFY_STATUS,
      status, NULL);
}

/* public */

/** Closes a chat session. 
 * @param session desired icq_ChatSession
 * @ingroup ChatSession
 */
void icq_ChatSessionClose(icq_ChatSession *session)
{
  icq_TCPLink *plink = session->tcplink;

  /* if we're attached to a tcplink, unattach so the link doesn't try
   * to close us, and then close the tcplink */
  if (plink)
  {
    plink->session=NULL;
    icq_TCPLinkClose(plink);
  }
  
  icq_ChatSessionDelete(session);

  list_remove(session->icqlink->d->icq_ChatSessions, session);
}

/** Sends chat data to the remote uin.
 * @param session desired icq_ChatSession
 * @param data pointer to data buffer, null-terminated
 * @ingroup ChatSession
 */
void icq_ChatSessionSendData(icq_ChatSession *session, const char *data)
{
  icq_TCPLink *plink = session->tcplink;
  int data_length = strlen(data);
  char *buffer;

  if(!plink)
    return;

  buffer = (char *)malloc(data_length);
  strcpy(buffer, data);
  icq_ChatRusConv_n("kw", buffer, data_length);
  
  send(plink->socket, buffer, data_length, 0);  
  
  free(buffer);
}

/** Sends chat data to the remote uin.
 * @param session desired icq_ChatSession
 * @param data pointer to data buffer
 * @param length length of data
 * @ingroup ChatSession
 */
void icq_ChatSessionSendData_n(icq_ChatSession *session, const char *data,
  int length)
{
  icq_TCPLink *plink = session->tcplink;
  char *buffer;

  if(!plink)
    return;
  
  buffer = (char *)malloc(length);
  memcpy(buffer, data, length);
  icq_ChatRusConv_n("kw", buffer, length);
    
  send(plink->socket, buffer, length, 0);

  free(buffer);
}


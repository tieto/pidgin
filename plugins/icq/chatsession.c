/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Copyright (C) 1998-2001, Denis V. Dmitrienko <denis@null.net> and
 *                          Bill Soudan <soudan@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>

#include "icqlib.h"
#include "tcp.h"

#include "chatsession.h"

icq_ChatSession *icq_ChatSessionNew(icq_Link *icqlink) 
{
  icq_ChatSession *p=(icq_ChatSession *)malloc(sizeof(icq_ChatSession));

  if (p)
  {
    p->status=0;
    p->id=0L;
    p->icqlink=icqlink;
    p->tcplink=NULL;
    p->user_data=NULL;
    icq_ListInsert(icqlink->d->icq_ChatSessions, 0, p);
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

icq_ChatSession *icq_FindChatSession(icq_Link *icqlink, DWORD uin)
{
  return icq_ListTraverse(icqlink->d->icq_ChatSessions,
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
  icq_TCPLink *tcplink = session->tcplink;

  /* if we're attached to a tcplink, unattach so the link doesn't try
   * to close us, and then close the tcplink */
  if (tcplink)
  {
    tcplink->session=NULL;
    icq_TCPLinkClose(tcplink);
  }
  
  icq_ChatSessionDelete(session);

  icq_ListRemove(session->icqlink->d->icq_ChatSessions, session);
}

/** Sends chat data to the remote uin.
 * @param session desired icq_ChatSession
 * @param data pointer to data buffer, null-terminated
 * @ingroup ChatSession
 */
void icq_ChatSessionSendData(icq_ChatSession *session, const char *data)
{
  icq_TCPLink *tcplink = session->tcplink;
  int data_length = strlen(data);
  char *buffer;

  if(!tcplink)
    return;

  buffer = (char *)malloc(data_length);
  strcpy(buffer, data);
  icq_ChatRusConv_n("kw", buffer, data_length);
  
  send(tcplink->socket, buffer, data_length, 0);  
  
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
  icq_TCPLink *tcplink = session->tcplink;
  char *buffer;

  if(!tcplink)
    return;
  
  buffer = (char *)malloc(length);
  memcpy(buffer, data, length);
  icq_ChatRusConv_n("kw", buffer, length);
    
  send(tcplink->socket, buffer, length, 0);

  free(buffer);
}


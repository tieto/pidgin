/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: tcp.c 2096 2001-07-31 01:00:39Z warmenhoven $
 *
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

/*
 * Peer-to-peer ICQ protocol implementation
 *
 * Uses version 2 of the ICQ protocol
 *
 * Thanks to Douglas F. McLaughlin and many others for
 * packet details (see tcp02.txt)
 *
 */

#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock.h>
#endif

#include <sys/stat.h>

#include "icqlib.h"

#include "tcp.h"
#include "stdpackets.h"
#include "chatsession.h"
#include "filesession.h"

/**
 Initializes structures necessary for TCP use.  Not required by user
 programs.

 \return true on error
*/
 
int icq_TCPInit(icq_Link *icqlink)
{
  icq_TCPLink *plink;

  /* allocate lists */
  icqlink->d->icq_TCPLinks=icq_ListNew();
  icqlink->d->icq_ChatSessions=icq_ListNew();
  icqlink->d->icq_FileSessions=icq_ListNew();

  /* only the main listening socket gets created upon initialization -
   * the other two are created when necessary */
  plink=icq_TCPLinkNew(icqlink);
  icq_TCPLinkListen(plink);
  icqlink->icq_TCPSrvPort=ntohs(plink->socket_address.sin_port);

  /* reset tcp sequence number */
  icqlink->d->icq_TCPSequence=0xfffffffe;

  return 0;
}

void icq_TCPDone(icq_Link *icqlink)
{
  /* close and deallocate all tcp links, this will also close any attached 
   * file or chat sessions */
  if (icqlink->d->icq_TCPLinks) {
    icq_ListDelete(icqlink->d->icq_TCPLinks, icq_TCPLinkDelete);
    icqlink->d->icq_TCPLinks = 0;
  }
  if (icqlink->d->icq_ChatSessions) {
    icq_ListDelete(icqlink->d->icq_ChatSessions, icq_ChatSessionDelete);
    icqlink->d->icq_ChatSessions = 0;
  }
  if (icqlink->d->icq_FileSessions) {
    icq_ListDelete(icqlink->d->icq_FileSessions, icq_FileSessionDelete);
    icqlink->d->icq_FileSessions = 0;
  }
}

icq_TCPLink *icq_TCPCheckLink(icq_Link *icqlink, DWORD uin, int type)
{
  icq_TCPLink *plink=icq_FindTCPLink(icqlink, uin, type);

  if(!plink)
  {
    plink=icq_TCPLinkNew(icqlink);
    if(type==TCP_LINK_MESSAGE)
      icq_TCPLinkConnect(plink, uin, 0);
  }

  return plink;
}

DWORD icq_TCPSendMessage(icq_Link *icqlink, DWORD uin, const char *message)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[ICQ_MAX_MESSAGE_SIZE] ;
  
  strncpy(data,message,sizeof(data)) ;
  data[sizeof(data)-1]='\0';
  icq_RusConv("kw", data) ;

  plink=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);

  /* create and send the message packet */
  p=icq_TCPCreateMessagePacket(plink, data);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("message packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

DWORD icq_TCPSendURL(icq_Link *icqlink, DWORD uin, const char *message, const char *url)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[ICQ_MAX_MESSAGE_SIZE];

  plink=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  
  strncpy(data, message, sizeof(data));
  data[sizeof(data)-1] = '\0';
  icq_RusConv("kw", data);

  /* create and send the url packet */
  p=icq_TCPCreateURLPacket(plink, data, url);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("url packet queued for uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

DWORD icq_TCPSendAwayMessageReq(icq_Link *icqlink, DWORD uin, int status)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  WORD type;
  
  plink=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);

  /* create and send the message packet */
  switch(status)
  {
    case STATUS_AWAY:
      type=ICQ_TCP_MSG_READAWAY;
      break;
    case STATUS_DND:
      type=ICQ_TCP_MSG_READDND;
      break;
    case STATUS_NA:
      type=ICQ_TCP_MSG_READNA;
      break;
    case STATUS_OCCUPIED:
      type=ICQ_TCP_MSG_READOCCUPIED;
      break;
    case STATUS_FREE_CHAT:
      type=ICQ_TCP_MSG_READFFC;
      break;
    default:
      type=ICQ_TCP_MSG_READAWAY;
      break;
  }
  p=icq_TCPCreateAwayReqPacket(plink, type);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("away msg request packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

DWORD icq_SendChatRequest(icq_Link *icqlink, DWORD uin, const char *message)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[ICQ_MAX_MESSAGE_SIZE];

  plink=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  
  strncpy(data, message, sizeof(data));
  data[sizeof(data)-1] = '\0';
  icq_RusConv("kw", data);

  /* create and send the url packet */
  p=icq_TCPCreateChatReqPacket(plink, data);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("chat req packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

unsigned long icq_SendFileRequest(icq_Link *icqlink, unsigned long uin,
  const char *message, char **files)
{
  icq_TCPLink *plink;
  icq_FileSession *pfile;
  icq_Packet *p;
  unsigned long sequence;
  char filename[64];
  char data[ICQ_MAX_MESSAGE_SIZE];
  char **filesiterator;
  char **pfilesiterator;

  plink=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);

  /* create the file session, this will be linked to the incoming icq_TCPLink
   * in icq_HandleFileAck */ 
  pfile=icq_FileSessionNew(icqlink);
  pfile->remote_uin=uin;
  pfile->files=files;
  pfile->direction=FILE_STATUS_SENDING;

  /* count the number and size of the files */
  pfile->total_files=0;
  filesiterator = files;
  while(*filesiterator) {
#ifdef _WIN32
    struct _stat file_status;
    if(_stat(*filesiterator, &file_status)==0) {
#else
    struct stat file_status;
    if(stat(*filesiterator, &file_status)==0) {
#endif
      pfile->total_files++;
      pfile->total_bytes+=file_status.st_size;
    }
    filesiterator++;
  }

  pfile->files=(char **)malloc(sizeof(char *)*(pfile->total_files +1));
  filesiterator = files;
  pfilesiterator = pfile->files;
  while (*filesiterator) {
    *pfilesiterator=(char *)malloc(strlen(*filesiterator)+1);
    strcpy(*pfilesiterator,*filesiterator);
    filesiterator++;
    pfilesiterator++;
  }
  *pfilesiterator = NULL;

  strncpy(filename, *(pfile->files), 64);

  strncpy(data, message, sizeof(data));
  data[sizeof(data)-1]='\0';
  icq_RusConv("kw", data);  

  /* create and send the file req packet */
  p=icq_TCPCreateFileReqPacket(plink, (char *)data, filename, 
    pfile->total_bytes);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);
  pfile->id=sequence;

#ifdef TCP_PACKET_TRACE
  printf("file req packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}            

void icq_AcceptChatRequest(icq_Link *icqlink, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage, *plisten;
  icq_ChatSession *pchat;
  icq_Packet *p;

  pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);

  /* create the chat listening socket if necessary */
  if(!(plisten=icq_FindTCPLink(icqlink, 0, TCP_LINK_CHAT)))
  {
    plisten=icq_TCPLinkNew(icqlink);
    plisten->type=TCP_LINK_CHAT;
    icq_TCPLinkListen(plisten);
  }

  /* create the chat session, this will be linked to the incoming icq_TCPLink
   * in TCPProcessHello */ 
  pchat=icq_ChatSessionNew(icqlink);
  pchat->id=sequence;
  pchat->remote_uin=uin;

  /* create and send the ack packet */
  p=icq_TCPCreateChatReqAck(pmessage,
    ntohs(plisten->socket_address.sin_port));
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("chat req ack sent to uin %lu { sequence=%lx }\n", uin, sequence);
#endif
}

void icq_TCPSendChatData(icq_Link *icqlink, DWORD uin, const char *data)
{
  icq_TCPLink *plink=icq_FindTCPLink(icqlink, uin, TCP_LINK_CHAT);
  char data1[ICQ_MAX_MESSAGE_SIZE];
  int data1_len;

  if(!plink)
    return;  

  strncpy(data1,data,sizeof(data1)) ;
  data1[sizeof(data1)-1] = '\0';
  data1_len = strlen(data);
  icq_ChatRusConv_n("kw", data1, data1_len);

  send(plink->socket, data1, data1_len, 0);
}

void icq_TCPSendChatData_n(icq_Link *icqlink, DWORD uin, const char *data, int len)
{
  icq_TCPLink *plink=icq_FindTCPLink(icqlink, uin, TCP_LINK_CHAT);
  char *data1;

  if(!plink)
    return;

  data1 = (char *)malloc(len);
  memcpy(data1, data, len);
  icq_ChatRusConv_n("kw", data1, len);  

  send(plink->socket, data1, len, 0);
}

icq_FileSession *icq_AcceptFileRequest(icq_Link *icqlink, DWORD uin,
  unsigned long sequence)
{
  icq_TCPLink *pmessage, *plisten;
  icq_FileSession *pfile;
  icq_Packet *p;

  pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);

  /* create the file listening socket if necessary */
  if(!(plisten=icq_FindTCPLink(icqlink, 0, TCP_LINK_FILE)))
  {
    plisten=icq_TCPLinkNew(icqlink);
    plisten->type=TCP_LINK_FILE;
    icq_TCPLinkListen(plisten);
  }

  /* create the file session, this will be linked to the incoming icq_TCPLink
   * in TCPProcessHello */ 
  pfile=icq_FileSessionNew(icqlink);
  pfile->id=sequence;
  pfile->remote_uin=uin;
  pfile->direction=FILE_STATUS_RECEIVING;
  pfile->tcplink=plisten;
  icq_FileSessionSetStatus(pfile, FILE_STATUS_LISTENING);

  /* create and send the ack packet */
  p=icq_TCPCreateFileReqAck(pmessage, 
    ntohs(plisten->socket_address.sin_port));
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("file req ack sent to uin %lu { sequence=%lx }\n", uin, sequence);
#endif

  return pfile;
}

void icq_RefuseFileRequest(icq_Link *icqlink, DWORD uin, 
  unsigned long sequence, const char *reason)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  icq_Packet *p;

  /* create and send the refuse packet */
  p=icq_TCPCreateFileReqRefuse(pmessage,
    ntohs(pmessage->socket_address.sin_port), reason);
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("file req refuse sent to uin %lu { sequence=%lx, reason=\"%s\" }\n",
    uin, sequence, reason);
#endif
}

void icq_CancelFileRequest(icq_Link *icqlink, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  icq_FileSession *psession=icq_FindFileSession(icqlink, uin, sequence);
  icq_Packet *p;

  if (psession)
    icq_FileSessionClose(psession);

  /* create and send the cancel packet */
  p=icq_TCPCreateFileReqCancel(pmessage,
    ntohs(pmessage->socket_address.sin_port));
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);
#ifdef TCP_PACKET_TRACE
  printf("file req cancel sent to uin %lu { sequence=%lx }\n", uin, sequence);
#endif
}

void icq_RefuseChatRequest(icq_Link *icqlink, DWORD uin, 
  unsigned long sequence, const char *reason)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  icq_Packet *p;

  /* create and send the refuse packet */
  p=icq_TCPCreateChatReqRefuse(pmessage,
    ntohs(pmessage->socket_address.sin_port), reason);
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("chat req refuse sent to uin %lu { sequence=%lx, reason=\"%s\" }\n",
    uin, sequence, reason);
#endif
}

void icq_CancelChatRequest(icq_Link *icqlink, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(icqlink, uin, TCP_LINK_MESSAGE);
  icq_ChatSession *psession=icq_FindChatSession(icqlink, uin);
  icq_Packet *p;

  if (psession)
    icq_ChatSessionClose(psession);

  /* create and send the cancel packet */
  p=icq_TCPCreateChatReqCancel(pmessage,
    ntohs(pmessage->socket_address.sin_port));
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("chat req cancel sent to uin %lu { sequence=%lx }\n", uin, sequence);
#endif
}

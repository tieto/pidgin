/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: tcphandle.c 2096 2001-07-31 01:00:39Z warmenhoven $
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

#include <stdlib.h>

#include "icqlib.h"

#include "tcp.h"
#include "stdpackets.h"

void icq_TCPOnMessageReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id, icq_TCPLink *plink);
void icq_TCPOnURLReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id);
void icq_TCPOnContactListReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id);
void icq_TCPOnChatReqReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id);
void icq_TCPOnFileReqReceived(icq_Link *icqlink, DWORD uin, const char *message, 
   const char *filename, unsigned long filesize, DWORD id);
void icq_TCPProcessAck(icq_Link *icqlink, icq_Packet *p);
void icq_HandleChatAck(icq_TCPLink *plink, icq_Packet *p, int port);
void icq_HandleChatHello(icq_TCPLink *plink);
void icq_HandleFileHello(icq_TCPLink *plink);
void icq_HandleFileAck(icq_TCPLink *plink, icq_Packet *p, int port);

void icq_TCPProcessPacket(icq_Packet *p, icq_TCPLink *plink)
{
  DWORD uin;
  WORD version;
  WORD command;
  WORD type;
  WORD status;
  DWORD command_type;
  DWORD filesize = 0;
  DWORD port = 0;
  
  const char *message;
  const char *filename = 0;

  icq_PacketBegin(p);
  (void)icq_PacketRead32(p);
  version=icq_PacketRead16(p);
  command=icq_PacketRead16(p);
  (void)icq_PacketRead16(p);

  uin=icq_PacketRead32(p);
  type=icq_PacketRead16(p);
  message=icq_PacketReadString(p);
  (void)icq_PacketRead32(p);
  (void)icq_PacketRead32(p);
  (void)icq_PacketRead32(p);
  (void)icq_PacketRead8(p);
  status=icq_PacketRead16(p);
  command_type=icq_PacketRead16(p);

  switch(type & ~ICQ_TCP_MASS_MASK)
  {
    case ICQ_TCP_MSG_MSG:
    case ICQ_TCP_MSG_URL:
    case ICQ_TCP_MSG_CONTACTLIST:
    case ICQ_TCP_MSG_READAWAY:
    case ICQ_TCP_MSG_READNA:
    case ICQ_TCP_MSG_READDND:
    case ICQ_TCP_MSG_READOCCUPIED:
    case ICQ_TCP_MSG_READFFC:
      p->id=icq_PacketRead32(p);
      break;

    case ICQ_TCP_MSG_CHAT:
      (void)icq_PacketReadString(p);
      (void)icq_PacketRead16(p);
      (void)icq_PacketRead16(p);
      port=icq_PacketRead32(p);
      p->id=icq_PacketRead32(p);
      break;

    case ICQ_TCP_MSG_FILE:
      (void)icq_PacketRead16(p);
      (void)icq_PacketRead16(p);
      filename=icq_PacketReadString(p);
      filesize=icq_PacketRead32(p);
      port=icq_PacketRead32(p);
      p->id=icq_PacketRead32(p);
      break;

    default:
      icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown message packet, type %x\n", type);
  }

#ifdef TCP_PROCESS_TRACE
  printf("packet processed from uin: %lu:\n", uin);
  printf("   command: %x\ttype: %x\n", command, type);
  printf("   status: %x\tcommand_type: %x\n", status, (int)command_type);
  printf("   message %s\n", message);
  printf("   id: %x\n", (int)p->id);
#endif

  switch(command)
  {
    case ICQ_TCP_MESSAGE:
      switch(type & ~ICQ_TCP_MASS_MASK)
      {
        case ICQ_TCP_MSG_MSG:
          icq_TCPOnMessageReceived(plink->icqlink, uin, message, p->id, plink);
          break;

        case ICQ_TCP_MSG_URL:
          icq_TCPOnURLReceived(plink->icqlink, uin, message, p->id);
          break;

        case ICQ_TCP_MSG_CHAT:
          icq_TCPOnChatReqReceived(plink->icqlink, uin, message, p->id);
          break;

        case ICQ_TCP_MSG_FILE:
          icq_TCPOnFileReqReceived(plink->icqlink, uin, message, filename, filesize, p->id);
          break;

        case ICQ_TCP_MSG_CONTACTLIST:
          icq_TCPOnContactListReceived(plink->icqlink, uin, message, p->id);
          break;

        default:
          icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown message type %d!\n", type);
          break;
      }
      break;

    case ICQ_TCP_ACK:
      invoke_callback(plink->icqlink, icq_RequestNotify)
        (plink->icqlink, p->id, ICQ_NOTIFY_ACK, status, (void *)message);
      switch(type)
      {
        case ICQ_TCP_MSG_CHAT:
          icq_HandleChatAck(plink, p, port);
          break;

        case ICQ_TCP_MSG_FILE:
          icq_HandleFileAck(plink, p, port);
          break;

        case ICQ_TCP_MSG_MSG:
        case ICQ_TCP_MSG_URL:
          icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, "received ack %d\n", 
            p->id);
          break;

        case ICQ_TCP_MSG_READAWAY:
        case ICQ_TCP_MSG_READNA:
        case ICQ_TCP_MSG_READDND:
        case ICQ_TCP_MSG_READOCCUPIED:
        case ICQ_TCP_MSG_READFFC:
          icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, 
            "received away msg, seq %d\n", p->id);
          invoke_callback(plink->icqlink, icq_RecvAwayMsg)
            (plink->icqlink, p->id, message);
          break;
      }
      invoke_callback(plink->icqlink, icq_RequestNotify)
        (plink->icqlink, p->id, ICQ_NOTIFY_SUCCESS, 0, NULL);
      break;

    case ICQ_TCP_CANCEL:
      /* icq_TCPProcessCancel(p); */
      break;

    default:
      icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, 
                 "unknown packet command %d!\n", command);
  }
}

void icq_TCPProcessCancel(icq_Packet *p)
{
  (void)p;

/*
  find packet in queue
  call notification function
  remove packet from queue
*/
}

int icq_TCPProcessHello(icq_Packet *p, icq_TCPLink *plink)
{
  /* TCP Hello packet */
  BYTE code;                /* 0xFF - init packet code */
  DWORD version;            /* tcp version */
  DWORD remote_port;        /* remote message listen port */
  DWORD remote_uin;         /* remote uin */
  DWORD remote_ip;          /* remote IP as seen by ICQ server */
  DWORD remote_real_ip;     /* remote IP as seen by client */
  BYTE flags;               /* tcp flags */
  DWORD remote_other_port;  /* remote chat or file listen port */

  icq_PacketBegin(p);
  
  code=icq_PacketRead8(p);
  version=icq_PacketRead32(p);

  if (!(p->length>=26 && code==ICQ_TCP_HELLO))
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, 
      "malformed hello packet received from %s:%d, closing link\n",
      inet_ntoa(*((struct in_addr *)(&(plink->remote_address.sin_addr)))),
      ntohs(plink->remote_address.sin_port));

    icq_TCPLinkClose(plink);
    return 0;
  }
  remote_port=icq_PacketRead32(p);
  remote_uin=icq_PacketRead32(p);
  remote_ip=icq_PacketRead32(p);
  remote_real_ip=icq_PacketRead32(p);
  flags=icq_PacketRead8(p);
  remote_other_port=icq_PacketRead32(p);

  icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, 
    "hello packet received from %lu { version=%d }\n", remote_uin, version);

  plink->remote_version=version;
  plink->remote_uin=remote_uin;
  plink->flags=flags;
  plink->mode&=~TCP_LINK_MODE_HELLOWAIT;

  /* file and chat sessions require additional handling */
  if(plink->type==TCP_LINK_CHAT) icq_HandleChatHello(plink);
  if(plink->type==TCP_LINK_FILE) icq_HandleFileHello(plink);

  return 1;
}

void icq_TCPOnMessageReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id, icq_TCPLink *plink)
{
  char data[ICQ_MAX_MESSAGE_SIZE];

  /* use the current system time for time received */
  time_t t=time(0);
  struct tm *ptime=localtime(&t);
  icq_Packet *pack;

#ifdef TCP_PACKET_TRACE
  printf("tcp message packet received from %lu { sequence=%x }\n",
         uin, (int)id);
#endif

  strncpy(data,message,sizeof(data));
  data[sizeof(data)-1]='\0';
  icq_RusConv("wk",data);

  invoke_callback(icqlink,icq_RecvMessage)(icqlink, uin, ptime->tm_hour, 
    ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, data);

  /*
  icq_TCPLink *preallink=icq_FindTCPLink(icqlink, uin, TCP_LINK_MESSAGE);
  if(plink != preallink)
    invoke_callback(icqlink,icq_SpoofedMessage)(uin, ...)
  */

  /* send an acknowledgement to the remote client */
  pack=icq_TCPCreateMessageAck(plink,0);
  icq_PacketAppend32(pack, id);
  icq_PacketSend(pack, plink->socket);
#ifdef TCP_PACKET_TRACE
  printf("tcp message ack sent to uin %lu { sequence=%lx }\n", uin, id);
#endif
  icq_PacketDelete(pack);
}

void icq_TCPOnURLReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id)
{
  /* use the current system time for time received */
  time_t t=time(0);
  struct tm *ptime=localtime(&t);
  icq_Packet *pack;
  char *pfe;
  icq_TCPLink *plink=icq_FindTCPLink(icqlink, uin, TCP_LINK_MESSAGE);

#ifdef TCP_PACKET_TRACE
  printf("tcp url packet received from %lu { sequence=%lx }\n",
     uin, id);
#endif /*TCP_PACKET_TRACE*/

  /* the URL is split from the description by 0xFE */
  pfe=strchr(message, '\xFE');
  *pfe=0;
  icq_RusConv("wk", (char*)message);

  invoke_callback(icqlink,icq_RecvURL)(icqlink, uin, ptime->tm_hour, 
    ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, 
    pfe+1, message);

  /* send an acknowledgement to the remote client */
  pack=icq_TCPCreateURLAck(plink,0);
  icq_PacketAppend32(pack, id);
  icq_PacketSend(pack, plink->socket);
#ifdef TCP_PACKET_TRACE
  printf("tcp message ack sent to %lu { sequence=%lx }\n", uin, id);
#endif
  icq_PacketDelete(pack);
}

void icq_TCPOnContactListReceived(icq_Link *icqlink, DWORD uin, const char *message, DWORD id)
{
  /* use the current system time for time received */
  time_t t=time(0);
  struct tm *ptime=localtime(&t);
  icq_Packet *pack;
  icq_List *strList = icq_ListNew();
  int i, k, nr = icq_SplitFields(strList, message);
  const char **contact_uin  = (const char **)malloc((nr - 2) /2);
  const char **contact_nick = (const char **)malloc((nr - 2) /2);
  icq_TCPLink *plink=icq_FindTCPLink(icqlink, uin, TCP_LINK_MESSAGE);

#ifdef TCP_PACKET_TRACE
  printf("tcp contactlist packet received from %lu { sequence=%lx }\n", uin, id);
#endif /* TCP_PACKET_TRACE */

  /* split message */
  for (i = 1, k = 0; i < (nr - 1); k++)
  {
    contact_uin[k]  = icq_ListAt(strList, i);
    contact_nick[k] = icq_ListAt(strList, i + 1);
    i += 2;
  }

  invoke_callback(icqlink,icq_RecvContactList)(icqlink, uin, 
    ptime->tm_hour, ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1, 
    ptime->tm_year+1900, k, contact_uin, contact_nick);

  /* send an acknowledement to the remote client */
  pack=icq_TCPCreateContactListAck(plink, 0);
  icq_PacketAppend32(pack, id);
  icq_PacketSend(pack, plink->socket);
#ifdef TCP_PACKET_TRACE
  printf("tcp message ack sent to %lu { sequence=%lx }\n", uin, id);
#endif /* TCP_PACKE_TRACE */
  icq_PacketDelete(pack);

  free(contact_nick);
  free(contact_uin);
  icq_ListDelete(strList, free);
}

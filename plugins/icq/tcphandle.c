/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: tcphandle.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.14  2000/12/06 05:15:45  denis
Handling for mass TCP messages has been added based on patch by
Konstantin Klyagin <konst@konst.org.ua>

Revision 1.13  2000/08/13 19:44:41  denis
Cyrillic recoding on received URL description added.

Revision 1.12  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.11  2000/06/25 16:36:16  denis
'\n' was added at the end of log messages.

Revision 1.10  2000/05/04 15:57:20  bills
Reworked file transfer notification, small bugfixes, and cleanups.

Revision 1.9  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.8  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.7  2000/01/20 20:06:00  bills
removed debugging printfs

Revision 1.6  2000/01/20 19:59:15  bills
first implementation of sending file requests

Revision 1.5  1999/11/30 09:51:42  bills
more file xfer logic added

Revision 1.4  1999/11/11 15:10:30  guruz
- Added Base for Webpager Messages. Please type "make fixme"
- Removed Segfault when kicq is started the first time

Revision 1.3  1999/10/01 02:28:51  bills
icq_TCPProcessHello returns something now :)

Revision 1.2  1999/10/01 00:49:20  lord
some compilation problems are fixed.

Revision 1.1  1999/09/29 19:47:21  bills
reworked chat/file handling.  fixed chat. (it's been broke since I put
non-blocking connects in)

Revision 1.15  1999/07/16 15:45:59  denis
Cleaned up.

Revision 1.14  1999/07/16 12:10:10  denis
tcp_packet* functions renamed to icq_Packet*
Cleaned up.

Revision 1.13  1999/07/12 15:13:41  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.12  1999/06/30 13:51:25  bills
cleanups

Revision 1.11  1999/05/03 21:41:30  bills
initial file xfer support added- untested

Revision 1.10  1999/04/29 09:36:06  denis
Cleanups, warning removed

Revision 1.9  1999/04/17 19:40:33  bills
reworked code to use icq_TCPLinks instead of icq_ContactItem entries.
modified ProcessChatPacket to negotiate both sending and receiving chat
requests properly.

Revision 1.8  1999/04/14 15:12:02  denis
Cleanups for "strict" compiling (-ansi -pedantic)
icq_ContactItem parameter added to function icq_TCPOnMessageReceived()
Segfault fixed on spoofed messages.

*/

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"

#include "tcp.h"
#include "stdpackets.h"
#include "tcplink.h"

void icq_TCPOnMessageReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id, icq_TCPLink *plink);
void icq_TCPOnURLReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id);
void icq_TCPOnChatReqReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id);
void icq_TCPOnFileReqReceived(ICQLINK *link, DWORD uin, const char *message, 
   const char *filename, unsigned long filesize, DWORD id);
void icq_TCPProcessAck(ICQLINK *link, icq_Packet *p);
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

        default:
          icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown message type %d!\n", type);
          break;
      }
      break;

    case ICQ_TCP_ACK:
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
          if(plink->icqlink->icq_RequestNotify)
          {
            icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, "received ack %d\n", p->id);
            (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_ACK, status,
                                               (void *)message);
            (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_SUCCESS, 0, 0);
          }
          break;
      }
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

void icq_TCPOnMessageReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id, icq_TCPLink *plink)
{
  char data[512] ;
#ifdef TCP_PACKET_TRACE
  printf("tcp message packet received from %lu { sequence=%x }\n",
         uin, (int)id);
#endif

  if(link->icq_RecvMessage)
  {
    /* use the current system time for time received */
    time_t t=time(0);
    struct tm *ptime=localtime(&t);
    icq_Packet *pack;
    icq_TCPLink *preallink=icq_FindTCPLink(link, uin, TCP_LINK_MESSAGE);

    strncpy(data,message,512) ;
    icq_RusConv("wk",data) ;

    (*link->icq_RecvMessage)(link, uin, ptime->tm_hour, ptime->tm_min,
      ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, data);

    if(plink != preallink)
    {
/*      if(icq_SpoofedMessage)
        (*icq_SpoofedMessage(uin, ...));*/
    }

    if(plink)
    {
      /* send an acknowledgement to the remote client */
      pack=icq_TCPCreateMessageAck(plink,0);
      icq_PacketAppend32(pack, id);
      icq_PacketSend(pack, plink->socket);
#ifdef TCP_PACKET_TRACE
      printf("tcp message ack sent to uin %lu { sequence=%lx }\n", uin, id);
#endif
      icq_PacketDelete(pack);
    }
  }
}

void icq_TCPOnURLReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id)
{
#ifdef TCP_PACKET_TRACE
  printf("tcp url packet received from %lu { sequence=%lx }\n",
     uin, id);
#endif /*TCP_PACKET_TRACE*/

  if(link->icq_RecvURL)
  {
    /* use the current system time for time received */
    time_t t=time(0);
    struct tm *ptime=localtime(&t);
    icq_Packet *pack;
    char *pfe;
    icq_TCPLink *plink=icq_FindTCPLink(link, uin, TCP_LINK_MESSAGE);

    /* the URL is split from the description by 0xFE */
    pfe=strchr(message, '\xFE');
    *pfe=0;
    icq_RusConv("wk", (char*)message);
    (*link->icq_RecvURL)(link, uin, ptime->tm_hour, ptime->tm_min,
       ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, pfe+1, message);

    /* send an acknowledgement to the remote client */
    pack=icq_TCPCreateURLAck(plink,0);
    icq_PacketAppend32(pack, id);
    icq_PacketSend(pack, plink->socket);
#ifdef TCP_PACKET_TRACE
    printf("tcp message ack sent to %lu { sequence=%lx }\n", uin, id);
#endif
    icq_PacketDelete(pack);
  }
}

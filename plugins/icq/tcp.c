/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: tcp.c 1162 2000-11-28 02:22:42Z warmenhoven $
$Log$
Revision 1.1  2000/11/28 02:22:42  warmenhoven
icq. whoop de doo

Revision 1.36  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.35  2000/06/15 01:52:16  bills
added Cancel and Refuse functions for chat and file reqs, changed packet
sending code to use new icq_TCPLinkSendSeq function to elimitane duplicate
code, removed *Seq functions, renamed chat req functions

Revision 1.34  2000/05/04 15:57:20  bills
Reworked file transfer notification, small bugfixes, and cleanups.

Revision 1.33  2000/04/10 18:11:45  denis
ANSI cleanups.

Revision 1.32  2000/04/10 16:36:04  denis
Some more Win32 compatibility from Guillaume Rosanis <grs@mail.com>

Revision 1.31  2000/04/06 16:38:04  denis
icq_*Send*Seq() functions with specified sequence number were added.

Revision 1.30  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.29  2000/02/15 04:02:41  bills
warning cleanup

Revision 1.28  2000/02/15 03:58:20  bills
use new icq_ChatRusConv_n function in icq_TCPSendChatData,
new icq_TCPSendChatData_n function

Revision 1.27  2000/02/07 02:40:23  bills
new code for SOCKS connections, more cyrillic translations

Revision 1.26  2000/01/20 19:59:15  bills
first implementation of sending file requests

Revision 1.25  2000/01/16 21:28:24  bills
renamed icq_TCPAcceptFileReq to icq_AcceptFileRequest, moved file request
functions to new file session code

Revision 1.24  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.23  1999/12/27 16:10:04  bills
fixed buy in icq_TCPAcceptFileReq, added icq_TCPFileSetSpeed

Revision 1.22  1999/12/21 00:29:59  bills
moved _process_packet logic into tcplink::icq_TCPLinkProcessReceived,
removed unnecessary icq_TCPSendFile??Packet functions

Revision 1.21  1999/12/14 03:31:48  bills
fixed double delete bug in _handle_ready_sockets, added code to implement
connect timeout

Revision 1.20  1999/11/30 09:44:31  bills
added file session logic

Revision 1.19  1999/09/29 20:07:12  bills
cleanups, moved connect logic from _handle_ready_sockets to
icq_TCPLinkOnConnect, tcp_link->icq_TCPLink

Revision 1.18  1999/09/29 17:08:48  denis
Cleanups.

Revision 1.17  1999/07/18 20:19:56  bills
added better log messages

Revision 1.16  1999/07/16 15:45:56  denis
Cleaned up.

Revision 1.15  1999/07/16 12:14:13  denis
tcp_packet* functions renamed to icq_Packet*
Cleaned up.

Revision 1.14  1999/07/12 15:13:34  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.13  1999/07/03 06:33:49  lord
. byte order conversion macros added
. some compilation warnings removed

Revision 1.12  1999/06/30 13:52:22  bills
implemented non-blocking connects

Revision 1.11  1999/05/03 21:41:26  bills
initial file xfer support added- untested

Revision 1.10  1999/04/29 09:35:41  denis
Cleanups, warning removed

Revision 1.9  1999/04/17 19:30:50  bills
_major_ restructuring.  all tcp sockets (including listening sockets) are
kept in global linked list, icq_TCPLinks. accept and listen functions
moved to tcplink.c.  changed return values of Send* functions to DWORD.

Revision 1.8  1999/04/14 14:57:05  denis
Cleanups for "strict" compiling (-ansi -pedantic)
Parameter port added to function icq_TCPCreateListeningSocket()

*/

/*
   Peer-to-peer ICQ protocol implementation

   Uses version 2 of the ICQ protocol

   Thanks to Douglas F. McLaughlin and many others for
   packet details (see tcp02.txt)

*/

#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif

#include <sys/stat.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "icqtypes.h"
#include "icqlib.h"

#include "tcp.h"
#include "stdpackets.h"
#include "list.h"
#include "tcplink.h"
#include "chatsession.h"
#include "filesession.h"

/**
 Initializes structures necessary for TCP use.  Not required by user
 programs.

 \return true on error
*/
 
int icq_TCPInit(ICQLINK *link)
{
  icq_TCPLink *plink;

  /* allocate lists */
  link->icq_TCPLinks=list_new();
  link->icq_ChatSessions=list_new();
  link->icq_FileSessions=list_new();

  /* only the main listening socket gets created upon initialization -
   * the other two are created when necessary */
  plink=icq_TCPLinkNew( link );
  icq_TCPLinkListen(plink);
  link->icq_TCPSrvPort=ntohs(plink->socket_address.sin_port);

  /* reset tcp sequence number */
  link->icq_TCPSequence=0xfffffffe;

  return 0;
}

void icq_TCPDone(ICQLINK *link)
{
  /* close and deallocate all tcp links, this will also close any attached 
   * file or chat sessions */
  list_delete(link->icq_TCPLinks, icq_TCPLinkDelete);
  list_delete(link->icq_ChatSessions, icq_ChatSessionDelete);
  list_delete(link->icq_FileSessions, icq_FileSessionDelete);
}

/* helper function for icq_TCPMain */
int _generate_fds(void *p, va_list data)
{
  icq_TCPLink *plink=(icq_TCPLink *)p;
  ICQLINK *icqlink = plink->icqlink;

  (void)data;

  if(plink->socket>-1)
  {
    int socket=plink->socket;

    FD_SET(socket, &icqlink->TCP_readfds);

    /* we only care about writing if socket is trying to connect */
    if(plink->mode & TCP_LINK_MODE_CONNECTING)
    {
      if(plink->mode & (TCP_LINK_SOCKS_AUTHORIZATION | TCP_LINK_SOCKS_NOAUTHSTATUS | TCP_LINK_SOCKS_AUTHSTATUS | TCP_LINK_SOCKS_CONNSTATUS))
        FD_SET(socket, &icqlink->TCP_readfds);
      else
        FD_SET(socket, &icqlink->TCP_writefds);
    }

    if(socket+1>icqlink->TCP_maxfd)
      icqlink->TCP_maxfd=socket+1;
  }

  return 0; /* traverse the entire list */
}

/* helper function for icq_TCPMain */
int _handle_ready_sockets(void *p, va_list data)
{
  icq_TCPLink *plink=(icq_TCPLink *)p;
  ICQLINK *icqlink = plink->icqlink;
  int socket=plink->socket;

  (void)data;

  /* handle connecting sockets */
  if (plink->mode & TCP_LINK_MODE_CONNECTING)
  {
    if(socket>-1 && (FD_ISSET(socket, &icqlink->TCP_writefds) || FD_ISSET(socket, &icqlink->TCP_readfds)))
    {
      icq_TCPLinkOnConnect(plink);
      return 0; 
    }

    if((time(0L) - plink->connect_time) > TCP_LINK_CONNECT_TIMEOUT)
    {
      icq_TCPLinkClose(plink);
      return 0;
    }
  }

  /* handle ready for read sockets- either a connection is waiting on *
   * the listen sockets or data is ready to be read */
  if(socket>-1 && FD_ISSET(socket, &icqlink->TCP_readfds))
  {
    if(plink->mode & TCP_LINK_MODE_LISTEN)
      (void)icq_TCPLinkAccept(plink);
    else {

      int result=icq_TCPLinkOnDataReceived(plink);

      /* close the link if there was a receive error or if *
       * the remote end has closed the connection */
      if (result < 1) 
        icq_TCPLinkClose(plink);

    }
  }

  return 0; /* traverse the entire list */
}

/* helper function for icq_TCPMain */
int _process_links(void *p, va_list data)
{
  icq_TCPLink *plink=(icq_TCPLink *)p;

  (void)data;

  /* receive any packets watiting on the link */
  icq_TCPLinkProcessReceived(plink);

  /* if this a currently sending file link, send data! */
  if(plink->type==TCP_LINK_FILE) {
    icq_FileSession *psession=plink->session;
    if(psession && psession->status==FILE_STATUS_SENDING)
      icq_FileSessionSendData(psession);
  }

  return 0; /* traverse entire list */
}

void icq_TCPMain(ICQLINK *link)
{
  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  link->TCP_maxfd = 0;
  FD_ZERO(&link->TCP_readfds);
  FD_ZERO(&link->TCP_writefds);

  /* generate the fd sets for all open tcp links */
  (void)list_traverse(link->icq_TCPLinks, _generate_fds);

  /* determine which sockets require maintenance */
  select(link->TCP_maxfd, &link->TCP_readfds, &link->TCP_writefds, 0, &tv);

  /* call icq_TCPLinkOnDataReceived for any sockets with ready data,
   * send all packets on send queue if socket has connected, and
   * accept() from any listening sockets with pending connections */ 
  (void)list_traverse(link->icq_TCPLinks, _handle_ready_sockets, 0, 0);

  /* process all packets waiting for each TCPLink */
  (void)list_traverse(link->icq_TCPLinks, _process_links, 0, 0);
}

icq_TCPLink *icq_TCPCheckLink(ICQLINK *link, DWORD uin, int type)
{
  icq_TCPLink *plink=icq_FindTCPLink(link, uin, type);

  if(!plink)
  {
    plink=icq_TCPLinkNew( link );
    if(type==TCP_LINK_MESSAGE)
      icq_TCPLinkConnect(plink, uin, 0);
  }

  return plink;

}

DWORD icq_TCPSendMessage(ICQLINK *link, DWORD uin, const char *message)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[512] ;
  
  strncpy(data,message,512) ;
  icq_RusConv("kw", data) ;

  plink=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);

  /* create and send the message packet */
  p=icq_TCPCreateMessagePacket(plink, (unsigned char *)data);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("message packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

DWORD icq_TCPSendURL(ICQLINK *link, DWORD uin, const char *message, const char *url)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[512];

  plink=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
  
  strncpy(data, message, 512);
  data[511] = '\0';
  icq_RusConv("kw", data);

  /* create and send the url packet */
  p=icq_TCPCreateURLPacket(plink, data, url);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("url packet queued for uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

DWORD icq_SendChatRequest(ICQLINK *link, DWORD uin, const char *message)
{
  icq_TCPLink *plink;
  icq_Packet *p;
  DWORD sequence;
  char data[512];

  plink=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
  
  strncpy(data, message, 512);
  data[511] = '\0';
  icq_RusConv("kw", data);

  /* create and send the url packet */
  p=icq_TCPCreateChatReqPacket(plink, (unsigned char *)data);
  sequence=icq_TCPLinkSendSeq(plink, p, 0);

#ifdef TCP_PACKET_TRACE
  printf("chat req packet sent to uin %lu { sequence=%lx }\n", uin, p->id);
#endif

  return sequence;
}

unsigned long icq_SendFileRequest(ICQLINK *link, unsigned long uin,
  const char *message, char **files)
{
  icq_TCPLink *plink;
  icq_FileSession *pfile;
  icq_Packet *p;
  unsigned long sequence;
  char filename[64];
  char data[512];

  plink=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);

  /* create the file session, this will be linked to the incoming icq_TCPLink
   * in icq_HandleFileAck */ 
  pfile=icq_FileSessionNew(link);
  pfile->remote_uin=uin;
  pfile->files=files;
  pfile->direction=FILE_STATUS_SENDING;

  /* count the number and size of the files */
  pfile->total_files=0;
  while(*files) {
    struct stat file_status;

    if(stat(*files, &file_status)==0) {
      pfile->total_files++;
      pfile->total_bytes+=file_status.st_size;
    }
    files++;
  }

  strncpy(filename, *(pfile->files), 64);

  strncpy(data, message, 512);
  data[511] = '\0';
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

void icq_AcceptChatRequest(ICQLINK *link, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage, *plisten;
  icq_ChatSession *pchat;
  icq_Packet *p;

  pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);

  /* create the chat listening socket if necessary */
  if(!(plisten=icq_FindTCPLink(link, 0, TCP_LINK_CHAT)))
  {
    plisten=icq_TCPLinkNew( link );
    plisten->type=TCP_LINK_CHAT;
    icq_TCPLinkListen(plisten);
  }

  /* create the chat session, this will be linked to the incoming icq_TCPLink
   * in TCPProcessHello */ 
  pchat=icq_ChatSessionNew(link);
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

void icq_TCPSendChatData(ICQLINK *link, DWORD uin, const char *data)
{
  icq_TCPLink *plink=icq_FindTCPLink(link, uin, TCP_LINK_CHAT);
  char data1[512];
  int data1_len;

  if(!plink)
    return;  

  strncpy(data1,data,512) ;
  data1[511] = '\0';
  data1_len = strlen(data);
  icq_ChatRusConv_n("kw", data1, data1_len);

  send(plink->socket, data1, data1_len, 0);

}

void icq_TCPSendChatData_n(ICQLINK *link, DWORD uin, const char *data, int len)
{
  icq_TCPLink *plink=icq_FindTCPLink(link, uin, TCP_LINK_CHAT);
  char *data1;

  if(!plink)
    return;

  data1 = (char *)malloc(len);
  memcpy(data1, data, len);
  icq_ChatRusConv_n("kw", data1, len);  

  send(plink->socket, data1, len, 0);

}

void icq_TCPCloseChat(ICQLINK *link, unsigned long uin)
{
  icq_TCPLink *plink=icq_FindTCPLink(link, uin, TCP_LINK_CHAT);

  if(plink)
    icq_TCPLinkClose(plink);

}

icq_FileSession *icq_AcceptFileRequest(ICQLINK *link, DWORD uin,
  unsigned long sequence)
{
  icq_TCPLink *pmessage, *plisten;
  icq_FileSession *pfile;
  icq_Packet *p;

  pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);

  /* create the file listening socket if necessary */
  if(!(plisten=icq_FindTCPLink(link, 0, TCP_LINK_FILE)))
  {
    plisten=icq_TCPLinkNew( link );
    plisten->type=TCP_LINK_FILE;
    icq_TCPLinkListen(plisten);
  }

  /* create the file session, this will be linked to the incoming icq_TCPLink
   * in TCPProcessHello */ 
  pfile=icq_FileSessionNew(link);
  pfile->id=sequence;
  pfile->remote_uin=uin;
  pfile->direction=FILE_STATUS_RECEIVING;
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

void icq_RefuseFileRequest(ICQLINK *link, DWORD uin, 
  unsigned long sequence, const char *reason)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
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

void icq_CancelFileRequest(ICQLINK *link, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
  icq_FileSession *psession=icq_FindFileSession(link, uin, sequence);
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

void icq_RefuseChatRequest(ICQLINK *link, DWORD uin, 
  unsigned long sequence, const char *reason)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
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

void icq_CancelChatRequest(ICQLINK *link, DWORD uin, unsigned long sequence)
{
  icq_TCPLink *pmessage=icq_TCPCheckLink(link, uin, TCP_LINK_MESSAGE);
  icq_FileSession *psession=icq_FindFileSession(link, uin, sequence);
  icq_Packet *p;

  if (psession)
    icq_FileSessionClose(psession);

  /* create and send the cancel packet */
  p=icq_TCPCreateChatReqCancel(pmessage,
    ntohs(pmessage->socket_address.sin_port));
  (void)icq_TCPLinkSendSeq(pmessage, p, sequence);

#ifdef TCP_PACKET_TRACE
  printf("chat req cancel sent to uin %lu { sequence=%lx }\n", uin, sequence);
#endif

}

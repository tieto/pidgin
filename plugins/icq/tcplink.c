/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: tcplink.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.40  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.39  2000/12/03 21:57:15  bills
fixed bug #105068

Revision 1.38  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.37  2000/06/15 01:53:17  bills
added icq_TCPLinkSendSeq function

Revision 1.36  2000/05/04 15:57:20  bills
Reworked file transfer notification, small bugfixes, and cleanups.

Revision 1.35  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.34  2000/04/10 18:11:45  denis
ANSI cleanups.

Revision 1.33  2000/04/10 16:36:04  denis
Some more Win32 compatibility from Guillaume Rosanis <grs@mail.com>

Revision 1.32  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.31  2000/02/15 04:00:16  bills
 new icq_ChatRusConv_n function

Revision 1.30  2000/02/07 02:46:29  bills
implemented non-blocking TCP connects over SOCKS, cyrillic translation for chat

Revision 1.29  2000/01/20 19:59:35  bills
fixed bug in icq_TCPLinkConnect that caused file/chat connection attempts
to go to the wrong port

Revision 1.28  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.27  1999/12/27 16:13:29  bills
fixed bug in icq_TCPOnDataReceived, removed flags variable ;)

Revision 1.26  1999/12/27 10:56:10  denis
Unused "flags" variable commented out.

Revision 1.25  1999/12/21 00:30:53  bills
added icq_TCPLinkProcessReceived to support processing receive queue
before delete (packets used to get dropped in this instance, oops),
reworked icq_TCPLinkOnDataReceived to handle quick, large streams of data,
changed icq_TCPLinkOnConnect and *Accept to make all sockets non-blocking.

Revision 1.24  1999/12/14 03:33:34  bills
icq_TCPLinkConnect now uses real_ip when our IP is same as remote IP to make
connection, added code to implement connect timeout

Revision 1.23  1999/11/30 09:57:44  bills
buffer overflow check added, tcplinks will now close if buffer overflows.
increased icq_TCPLinkBufferSize to 4096 to support file transfer packets

Revision 1.22  1999/11/29 17:15:51  denis
Absence of socklen_t type fixed.

Revision 1.21  1999/10/01 00:49:21  lord
some compilation problems are fixed.

Revision 1.20  1999/09/29 20:26:41  bills
ack forgot the args :)

Revision 1.19  1999/09/29 20:21:45  bills
renamed denis' new function

Revision 1.18  1999/09/29 20:11:29  bills
renamed tcp_link* to icq_TCPLink*.  many cleanups, added icq_TCPLinkOnConnect

Revision 1.17  1999/09/29 17:10:05  denis
TCP code SOCKS-ification. Not finished.

Revision 1.16  1999/07/18 20:21:34  bills
fixed fail notification bug introduced during ICQLINK changes, changed to
use new byte-order functions & contact list functions, added better log
messages

Revision 1.15  1999/07/16 15:45:57  denis
Cleaned up.

Revision 1.14  1999/07/16 12:02:58  denis
tcp_packet* functions renamed to icq_Packet*
Cleaned up.

Revision 1.13  1999/07/12 15:13:36  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.12  1999/07/03 06:33:51  lord
. byte order conversion macros added
. some compilation warnings removed

Revision 1.11  1999/06/30 13:52:23  bills
implemented non-blocking connects

Revision 1.10  1999/05/03 21:39:41  bills
removed exit calls

Revision 1.9  1999/04/29 09:35:54  denis
Cleanups, warning removed

Revision 1.8  1999/04/17 19:34:49  bills
fixed bug in icq_TCPLinkOnDataReceived, multiple packets that come in on
one recv call are now handled correctly.  added icq_TCPLinkAccept and
icq_TCPLinkListen.  started using mode and type structure entries.  added
icq_TCPLinks list and icq_FindTCPLink function.  changed received_queue and
send_queue to lists.

Revision 1.7  1999/04/14 15:02:45  denis
Cleanups for "strict" compiling (-ansi -pedantic)

Revision 1.6  1999/04/05 18:47:17  bills
initial chat support implemented

Revision 1.5  1999/03/31 01:50:54  bills
wrapped up many tcp details- tcp code now handles incoming and outgoing
tcp messages and urls!

Revision 1.4  1999/03/28 03:27:49  bills
fixed icq_TCPLinkConnect so it really connects to remote ip instead of
always my local test computer O:)

Revision 1.3  1999/03/26 20:02:41  bills
fixed C++ comments, cleaned up

Revision 1.2  1999/03/25 22:21:59  bills
added necessary includes

Revision 1.1  1999/03/25 21:09:07  bills
tcp link functions
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
#define EINPROGRESS   WSAEINPROGRESS
#define ENETUNREACH   WSAENETUNREACH
#define ECONNREFUSED  WSAECONNREFUSED
#define ETIMEDOUT     WSAETIMEDOUT
#define EOPNOTSUPP    WSAEOPNOTSUPP
#define EAFNOSUPPORT  WSAEAFNOSUPPORT
#define EWOULDBLOCK   WSAEWOULDBLOCK
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"
#include "tcplink.h"
#include "stdpackets.h"
#include "util.h"
#include "tcp.h"
#include "errno.h"
#include "chatsession.h"
#include "filesession.h"

icq_TCPLink *icq_TCPLinkNew(ICQLINK *link)
{
  icq_TCPLink *p=(icq_TCPLink *)malloc(sizeof(icq_TCPLink));

  p->socket=-1;
  p->icqlink=link;
  p->mode=0;
  p->session=0L;
  p->type=TCP_LINK_MESSAGE;
  p->buffer_count=0;
  p->send_queue=list_new();
  p->received_queue=list_new();
  p->id=0;
  p->remote_uin=0;
  p->remote_version=0;
  p->flags=0;
  p->proxy_status = 0;

  if(p)
    list_enqueue(link->d->icq_TCPLinks, p);

  return p;
}

int _icq_TCPLinkDelete(void *pv, va_list data)
{
  icq_Packet *p=(icq_Packet *)pv;
  ICQLINK *icqlink=va_arg(data, ICQLINK *);

  /* notify the app the packet didn't make it */
  if(p->id)
    (*icqlink->icq_RequestNotify)(icqlink, p->id, ICQ_NOTIFY_FAILED, 0, 0);

  return 0;
}

void icq_TCPLinkDelete(void *pv)
{
  icq_TCPLink *p=(icq_TCPLink *)pv;

  /* process anything left in the received queue */
  icq_TCPLinkProcessReceived(p);

  /* make sure we notify app that packets in send queue didn't make it */
  (void)list_traverse(p->send_queue, _icq_TCPLinkDelete, p->icqlink);

  /* destruct all packets still waiting on queues */
  list_delete(p->send_queue, icq_PacketDelete);
  list_delete(p->received_queue, icq_PacketDelete);

  /* notify app if this was a chat or file xfer session and there is an
   * id assigned */
  if((p->type==TCP_LINK_CHAT || p->type==TCP_LINK_FILE) && p->id)
    if(p->icqlink->icq_RequestNotify)
      (*p->icqlink->icq_RequestNotify)(p->icqlink, p->id, ICQ_NOTIFY_SUCCESS, 0, 0);

  /* if this is a chat or file link, delete the associated session as
   * well, but make sure we unassociate ourself first so the session
   * doesn't try to close us */
  if(p->session)
  {
    if(p->type==TCP_LINK_CHAT)
    {
      icq_ChatSession *psession=p->session;
      /*psession->tcplink=0L;*/
      icq_ChatSessionClose(psession);
    }

    if(p->type==TCP_LINK_FILE) {
      icq_FileSession *psession=p->session;
      psession->tcplink=0L;
      icq_FileSessionClose(psession);
    }
  }

  /* close the socket after we notify app so app can read errno if necessary */
  if (p->socket > -1)
  {
#ifdef _WIN32
    closesocket(p->socket);
#else
    close(p->socket);
#endif
  }

  free(p);
}

void icq_TCPLinkClose(icq_TCPLink *plink)
{
  list_remove(plink->icqlink->d->icq_TCPLinks, plink);
  icq_TCPLinkDelete(plink);
}

int icq_TCPLinkProxyConnect(icq_TCPLink *plink, DWORD uin, int port)
{
  struct sockaddr_in prsin;
  struct hostent *host_struct;
  int conct;

  (void)uin; (void)port;

  prsin.sin_addr.s_addr = htonl(plink->icqlink->icq_ProxyIP);
  if(prsin.sin_addr.s_addr  == (unsigned long)-1)
  {
    prsin.sin_addr.s_addr = inet_addr(plink->icqlink->icq_ProxyHost);
    if(prsin.sin_addr.s_addr  == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
    {
      host_struct = gethostbyname(plink->icqlink->icq_ProxyHost);
      if(host_struct == 0L)
      {
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Can't find hostname: %s\n",
                   plink->icqlink->icq_ProxyHost);
        return -1;
      }
      prsin.sin_addr = *((struct in_addr *)host_struct->h_addr);
    }
  }
  prsin.sin_family = AF_INET; /* we're using the inet not appletalk*/
  prsin.sin_port = htons(plink->icqlink->icq_ProxyPort); /* port */
/*   flags = fcntl(plink->socket, F_GETFL, 0); */
/*   fcntl(plink->socket, F_SETFL, flags & (~O_NONBLOCK)); */
  plink->mode |= TCP_LINK_SOCKS_CONNECTING;
  conct = connect(plink->socket, (struct sockaddr *) &prsin, sizeof(prsin));
  if(conct == -1) /* did we connect ?*/
  {
    if(errno != EINPROGRESS)
    {
      conct = errno;
      icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
      return conct;
    }
    return 1;
  }
  return 0;
}

int icq_TCPLinkProxyRequestAuthorization(icq_TCPLink *plink)
{
  char buf[1024];

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_CONNECTING));
  buf[0] = 5; /* protocol version */
  buf[1] = 1; /* number of methods */
  if(!strlen(plink->icqlink->icq_ProxyName) || !strlen(plink->icqlink->icq_ProxyPass) ||
     !plink->icqlink->icq_ProxyAuth)
  {
    buf[2] = 0; /* no authorization required */
    plink->mode |= TCP_LINK_SOCKS_NOAUTHSTATUS;
  }
  else
  {
    buf[2] = 2; /* method username/password */
    plink->mode |= TCP_LINK_SOCKS_AUTHORIZATION;
  }
#ifdef _WIN32
  if(send(plink->socket, buf, 3, 0) != 3)
    return errno;
#else
  if(write(plink->socket, buf, 3) != 3)
    return errno;
#endif
  return 0;
}

int icq_TCPLinkProxyAuthorization(icq_TCPLink *plink)
{
  int res;
  char buf[1024];

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_AUTHORIZATION)) | TCP_LINK_SOCKS_AUTHSTATUS;
#ifdef _WIN32
  res = recv(plink->socket, buf, 2, 0);
#else
  res = read(plink->socket, buf, 2);
#endif
  if(res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication*/
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
#ifdef _WIN32
    closesocket(plink->socket);
#else
    close(plink->socket);
#endif
    return -1;
  }
  buf[0] = 1; /* version of subnegotiation */
  buf[1] = strlen(plink->icqlink->icq_ProxyName);
  memcpy(&buf[2], plink->icqlink->icq_ProxyName, buf[1]);
  buf[2+buf[1]] = strlen(plink->icqlink->icq_ProxyPass);
  memcpy(&buf[3+buf[1]], plink->icqlink->icq_ProxyPass, buf[2+buf[1]]);
#ifdef _WIN32
  if(send(plink->socket, buf, buf[1]+buf[2+buf[1]]+3, 0) != buf[1] + buf[2+buf[1]]+3)
    return errno;
#else
  if(write(plink->socket, buf, buf[1]+buf[2+buf[1]]+3) != buf[1] + buf[2+buf[1]]+3)
    return errno;
#endif
  return 0;
}

int icq_TCPLinkProxyAuthStatus(icq_TCPLink *plink)
{
  int res;
  char buf[20];

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_AUTHSTATUS)) | TCP_LINK_SOCKS_CROSSCONNECT;
#ifdef _WIN32
  res = recv(plink->socket, buf, 2, 0);
#else
  res = read(plink->socket, buf, 2);
#endif
  if(res != 2 || buf[0] != 1 || buf[1] != 0)
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Authorization failure\n");
#ifdef _WIN32
    closesocket(plink->socket);
#else
    close(plink->socket);
#endif
    return -1;
  }
  return 0;
}

int icq_TCPLinkProxyNoAuthStatus(icq_TCPLink *plink)
{
  int res;
  char buf[20];

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_NOAUTHSTATUS)) | TCP_LINK_SOCKS_CROSSCONNECT;
#ifdef _WIN32
  res = recv(plink->socket, buf, 2, 0);
#else
  res = read(plink->socket, buf, 2);
#endif
  if(res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
#ifdef _WIN32
    closesocket(plink->socket);
#else
    close(plink->socket);
#endif
    return -1;
  }
  return 0;
}

int icq_TCPLinkProxyCrossConnect(icq_TCPLink *plink)
{
  char buf[20];

  plink->mode = (plink->mode & ~(TCP_LINK_SOCKS_CROSSCONNECT)) | TCP_LINK_SOCKS_CONNSTATUS;
  buf[0] = 5; /* protocol version */
  buf[1] = 1; /* command connect */
  buf[2] = 0; /* reserved */
  buf[3] = 1; /* address type IP v4 */
  memcpy(&buf[4], &plink->remote_address.sin_addr.s_addr, 4);
  memcpy(&buf[8], &plink->remote_address.sin_port, 2);
#ifdef _WIN32
  if(send(plink->socket, buf, 10, 0) != 10)
    return errno;
#else
  if(write(plink->socket, buf, 10) != 10)
    return errno;
#endif
  return 0;
}

int icq_TCPLinkProxyConnectStatus(icq_TCPLink *plink)
{
  int res;
  char buf[1024];

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_CONNSTATUS));
#ifdef _WIN32
  res = recv(plink->socket, buf, 10, 0);
#else
  res = read(plink->socket, buf, 10);
#endif
  if(res != 10 || buf[0] != 5 || buf[1] != 0)
  {
    switch(buf[1])
    {
      case 1:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] General SOCKS server failure\n");
        res = EFAULT;
        break;
      case 2:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection not allowed by ruleset\n");
        res = EACCES;
        break;
      case 3:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Network unreachable\n");
        res = ENETUNREACH;
        break;
      case 4:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Host unreachable\n");
        res = ENETUNREACH;
        break;
      case 5:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
        res = ECONNREFUSED;
        break;
      case 6:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] TTL expired\n");
        res = ETIMEDOUT;
        break;
      case 7:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Command not supported\n");
        res = EOPNOTSUPP;
        break;
      case 8:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Address type not supported\n");
        res = EAFNOSUPPORT;
        break;
      default:
        icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Unknown SOCKS server failure\n");
        res = EFAULT;
        break;
    }
#ifdef _WIN32
    closesocket(plink->socket);
#else
    close(plink->socket);
#endif
    return res;
  }
  return 0;
}

int icq_TCPLinkConnect(icq_TCPLink *plink, DWORD uin, int port)
{
  icq_ContactItem *pcontact=icq_ContactFind(plink->icqlink, uin);
  icq_Packet *p;
  int result;

#ifndef _WIN32
  int flags;
#else
  u_long iosflag;
#endif

  /* these return values never and nowhere checked */
  /*                                   denis.      */
  if(!pcontact)
    return -2;

  if((plink->socket=socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -3;

/*   bzero(&(plink->remote_address), sizeof(plink->remote_address));   Win32 incompatible... */
  memset(&(plink->remote_address), 0, sizeof(plink->remote_address));
  plink->remote_address.sin_family = AF_INET;

  /* if our IP is the same as the remote user's ip, connect to real_ip
     instead since we're both probably behind a firewall */
  icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE,
    "local IP is %08X:%d, remote real IP is %08X:%d, remote IP is %08X:%d, port is %d\n",
    plink->icqlink->icq_OurIP,
    plink->icqlink->icq_OurPort,
    pcontact->remote_real_ip,
    pcontact->remote_port,
    pcontact->remote_ip,
    pcontact->remote_port,
    port
    );
  if (plink->icqlink->icq_OurIP == pcontact->remote_ip) 
    plink->remote_address.sin_addr.s_addr = htonl(pcontact->remote_real_ip);
  else 
    plink->remote_address.sin_addr.s_addr = htonl(pcontact->remote_ip);

  if(plink->type==TCP_LINK_MESSAGE)
  {
    plink->remote_address.sin_port = htons(pcontact->remote_port);
    icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, 
      "initiating message connect to %d (%s:%d)\n", uin, 
      inet_ntoa(*((struct in_addr *)(&(plink->remote_address.sin_addr)))),
      pcontact->remote_port);
  }
  else
  {
  plink->remote_address.sin_port = htons(port);
    icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, 
      "initiating file/chat connect to %d (%s:%d)\n", uin, 
      inet_ntoa(*((struct in_addr *)(&(plink->remote_address.sin_addr)))),
      port);
  }

  /* set the socket to non-blocking */
#ifdef _WIN32
  iosflag = TRUE;
  ioctlsocket(plink->socket, FIONBIO, &iosflag);
#else
  flags=fcntl(plink->socket, F_GETFL, 0);
  fcntl(plink->socket, F_SETFL, flags | O_NONBLOCK);
#endif

  if(!plink->icqlink->icq_UseProxy)
    result=connect(plink->socket, (struct sockaddr *)&(plink->remote_address),
       sizeof(plink->remote_address));
  else /* SOCKS proxy support */
    result=icq_TCPLinkProxyConnect(plink, uin, port);
  /* FIXME: Here we should check for errors on connection */
  /* because of proxy support - it can't be checked       */
  /* by getsockopt() later in _handle_ready_sockets()     */
  /*                                  denis.              */

  plink->mode|=TCP_LINK_MODE_CONNECTING;

  plink->remote_uin=uin;

  plink->connect_time=time(0L);

  /* Send the hello packet */
  p=icq_TCPCreateInitPacket(plink);
  icq_TCPLinkSend(plink, p);

#ifdef TCP_PACKET_TRACE
  printf("hello packet queued for %lu\n", uin);
#endif /* TCP_PACKET_TRACE */

  return 1;
}

icq_TCPLink *icq_TCPLinkAccept(icq_TCPLink *plink)
{
#ifdef _WIN32
  u_long iosflag;
#else
  int flags;
#endif
  int socket;
  size_t remote_length;
  icq_TCPLink *pnewlink=icq_TCPLinkNew( plink->icqlink );
  
  if(pnewlink)
  {
		socket=accept(plink->socket, (struct sockaddr *)&(plink->remote_address),
                  &remote_length);

		icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE,
			"accepting tcp connection from %s:%d\n",
			inet_ntoa(*((struct in_addr *)(&(plink->remote_address.sin_addr)))),
			ntohs(plink->remote_address.sin_port));

		/* FIXME: make sure accept succeeded */

    pnewlink->type=plink->type;
    pnewlink->socket=socket;

    /* first packet sent on an icq tcp link is always the hello packet */
    pnewlink->mode|=TCP_LINK_MODE_HELLOWAIT;
  }

  /* set the socket to non-blocking */
#ifdef _WIN32
  iosflag = TRUE;
  ioctlsocket(plink->socket, FIONBIO, &iosflag);
#else
  flags=fcntl(pnewlink->socket, F_GETFL, 0);
  fcntl(pnewlink->socket, F_SETFL, flags | O_NONBLOCK);
#endif
  
  return pnewlink;
}

int icq_TCPLinkListen(icq_TCPLink *plink)
{
  unsigned int t;

  /* listening links have 0 uin */
  plink->remote_uin=0;

  /* create tcp listen socket */
  if((plink->socket=socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  /* must use memset, no bzero for Win32! */
  memset(&plink->socket_address, 0, sizeof(struct sockaddr_in));
  plink->socket_address.sin_family=AF_INET;
  plink->socket_address.sin_addr.s_addr=htonl(INADDR_ANY);
  plink->socket_address.sin_port=0;

  if(bind(plink->socket, (struct sockaddr *)&plink->socket_address, sizeof(struct sockaddr_in)) < 0)
    return -2;

  if(listen(plink->socket, 5) < 0)
    return -3;

  t=sizeof(struct sockaddr_in);
  if(getsockname(plink->socket, (struct sockaddr *)&plink->socket_address, &t) < 0)
    return -4;

  icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE,
             "created tcp listening socket %d, local address=%s:%d\n",
             plink->socket,
             inet_ntoa(*((struct in_addr *)(&plink->socket_address.sin_addr))),
             ntohs(plink->socket_address.sin_port));

  plink->mode|=TCP_LINK_MODE_LISTEN;
  
  return 0;
}

/* Doing Cyrillic translations for Chat dialog sessions */
void icq_ChatRusConv_n(const char to[4], char *t_in, int t_len)
{
  int i, j;
  
  for(i = j = 0; i < t_len; ++i)
  {
    if((((unsigned char)t_in[i]) < ' ') && (t_in[i] != '\r'))
    {
      if(i - 1 > j)
        icq_RusConv_n(to, &t_in[j], i - j - 1);
      switch(t_in[i])
      {
        case '\x07': /* Bell */
        case '\x08': /* BackSpace */
        case '\x03': /* Chat is active */
        case '\x04': /* Chat is not active */
             break;
        case '\x00': /* Foregroung color (RR GG BB ?? ) */
        case '\x01': /* Background color (RR GG BB ?? ) */
        case '\x11': /* Font style change (Bold - 1, Italic - 2, Underline - 4) */
        case '\x12': /* Font size change */
             i += 4;
             break;
        case '\x10': /* Font family and encoding change */
             i += t_in[i+1] + 2 + 2;
             icq_RusConv_n(to, &t_in[i+3], t_in[i+1]);
             break;
      }
      j = i + 1;
    }
  }
  if(i > t_len)
    i = t_len;
  if(j > t_len)
    j = t_len;
  if(i > j)
    icq_RusConv_n(to, &t_in[j], i - j);
}

int icq_TCPLinkOnDataReceived(icq_TCPLink *plink)
{
  int process_count=0, recv_result=0;
  char *buffer=plink->buffer;

  do { /* while recv_result > 0 */

    int done=0;

    /* append received data onto end of buffer */
    if((recv_result=recv(plink->socket, buffer+plink->buffer_count,
      icq_TCPLinkBufferSize-plink->buffer_count, 0)) < 1)
    {
      /* either there was an error or the remote side has closed 
       * the connection - fall out of the loop */
      continue;
    };
  
    plink->buffer_count+=recv_result;

#ifdef TCP_BUFFER_TRACE
    printf("received %d bytes from link %x, new buffer count %d\n",
      recv_result, plink, plink->buffer_count);

    hex_dump(plink->buffer, plink->buffer_count);
#endif /*TCP_BUFFER_TRACE*/

    process_count+=recv_result;

    /* don't do any packet processing if we're in raw mode */
    if(plink->mode & TCP_LINK_MODE_RAW) {
      /* notify the app with the new data */
      if(plink->type == TCP_LINK_CHAT)
        icq_ChatRusConv_n("wk", plink->buffer, plink->buffer_count);
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, plink->id, ICQ_NOTIFY_CHATDATA,
                           plink->buffer_count, plink->buffer);
      plink->buffer_count=0;
      continue;
    }

    /* remove packets from the buffer until the buffer is empty
     * or the remaining bytes do not equal a full packet */
    while((unsigned)plink->buffer_count>sizeof(WORD) && !done)
    {
      WORD packet_size=(*((WORD *)buffer));

      /* warn if the buffer is too small to hold the whole packet */
      if(packet_size>icq_TCPLinkBufferSize-sizeof(WORD))
      {
        icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "tcplink buffer "
          "overflow, packet size = %d, buffer size = %d, closing link\n",
          packet_size, icq_TCPLinkBufferSize);
        return 0;
      }

      if(packet_size+sizeof(WORD) <= (unsigned)plink->buffer_count)
      {
        /* copy the packet into memory */
        icq_Packet *p=icq_PacketNew();
        icq_PacketAppend(p, buffer+sizeof(WORD), packet_size);

        /* remove it from the buffer */
        memcpy(buffer, buffer+packet_size+sizeof(WORD),
             plink->buffer_count-packet_size-sizeof(WORD));

        plink->buffer_count-=(packet_size+sizeof(WORD));

        icq_TCPLinkOnPacketReceived(plink, p);
      }
      else
      {
        /* not enough bytes in buffer to form the complete packet.
         * we're done for now */
        done=1;
      }
    } /* while packets remain in buffer */

  } while (recv_result > 0);

  if (recv_result < 0 && errno!=EWOULDBLOCK) {

    /* receive error - log it */
    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "recv failed from %d (%d-%s),"
      " closing link\n", plink->remote_uin, errno, strerror(errno));

  }

  return process_count;
}

void icq_TCPLinkOnPacketReceived(icq_TCPLink *plink, icq_Packet *p)
{

#ifdef TCP_RAW_TRACE
  printf("packet received! { length=%d }\n", p->length);
  icq_PacketDump(p);
#endif

  /* Stick packet on ready packet linked list */
  list_enqueue(plink->received_queue, p);
}

void icq_TCPLinkOnConnect(icq_TCPLink *plink)
{
#ifdef _WIN32
  int len;
#else
  size_t len;
#endif
  int error;
  
  /* check getsockopt */
  len=sizeof(error);

#ifdef _WIN32
  getsockopt(plink->socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
#else
  getsockopt(plink->socket, SOL_SOCKET, SO_ERROR, &error, &len);
#endif
  if(!error && (plink->mode & (TCP_LINK_SOCKS_CONNECTING | TCP_LINK_SOCKS_AUTHORIZATION |
                               TCP_LINK_SOCKS_AUTHSTATUS | TCP_LINK_SOCKS_NOAUTHSTATUS |
                               TCP_LINK_SOCKS_CROSSCONNECT | TCP_LINK_SOCKS_CONNSTATUS)))
  {
    if(plink->mode & TCP_LINK_SOCKS_CONNECTING)
       error = icq_TCPLinkProxyRequestAuthorization(plink);
    else if(plink->mode & TCP_LINK_SOCKS_AUTHORIZATION)
      error = icq_TCPLinkProxyAuthorization(plink);
    else if(plink->mode & TCP_LINK_SOCKS_AUTHSTATUS)
      error = icq_TCPLinkProxyAuthStatus(plink);
    else if(plink->mode & TCP_LINK_SOCKS_NOAUTHSTATUS)
      error = icq_TCPLinkProxyNoAuthStatus(plink);
    else if(plink->mode & TCP_LINK_SOCKS_CROSSCONNECT)
      error = icq_TCPLinkProxyCrossConnect(plink);
    else if(plink->mode & TCP_LINK_SOCKS_CONNSTATUS)
      error = icq_TCPLinkProxyConnectStatus(plink);
    else
      error = EINVAL;
  }

  if(error)
  {
    /* connection failed- close the link, which takes care
     * of notifying the app about packets that didn't make it */
    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "connect failed to %d (%d-%s),"
       " closing link\n", plink->remote_uin, error, strerror(error));

    icq_TCPLinkClose(plink);
    return;
  }

  if(plink->mode & (TCP_LINK_SOCKS_CONNECTING | TCP_LINK_SOCKS_AUTHORIZATION | TCP_LINK_SOCKS_AUTHSTATUS | TCP_LINK_SOCKS_NOAUTHSTATUS | TCP_LINK_SOCKS_CROSSCONNECT | TCP_LINK_SOCKS_CONNSTATUS))
    return;

  len=sizeof(plink->socket_address);
  getsockname(plink->socket, (struct sockaddr *)&plink->socket_address, &len);

  icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE,
             "connected to uin %d, socket=%d local address=%s:%d remote address=%s:%d\n",
             plink->remote_uin, plink->socket,
             inet_ntoa(*((struct in_addr *)(&plink->socket_address.sin_addr))),
             ntohs(plink->socket_address.sin_port),
             inet_ntoa(*((struct in_addr *)(&plink->remote_address.sin_addr))),
             ntohs(plink->remote_address.sin_port));

  plink->mode&= ~TCP_LINK_MODE_CONNECTING;

  /* socket is now connected, notify each request that connection
   * has been established and send pending data */
  while(plink->send_queue->count>0)
  {
    icq_Packet *p=list_dequeue(plink->send_queue);
    if(p->id)
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_CONNECTED, 0, 0);
    icq_TCPLinkSend(plink, p);
  }

  /* yeah this probably shouldn't be here.  oh well :) */
  if(plink->type==TCP_LINK_CHAT)
  {
    icq_ChatSessionSetStatus((icq_ChatSession *)plink->session,
      CHAT_STATUS_CONNECTED);
    icq_ChatSessionSetStatus((icq_ChatSession *)plink->session, 
      CHAT_STATUS_WAIT_ALLINFO);
  }

  if(plink->type==TCP_LINK_FILE)
  {
    icq_FileSessionSetStatus((icq_FileSession *)plink->session,
      FILE_STATUS_CONNECTED);
  }

}

unsigned long icq_TCPLinkSendSeq(icq_TCPLink *plink, icq_Packet *p,
  unsigned long sequence)
{
  /* append the next sequence number on the packet */
  if (!sequence)
    sequence=plink->icqlink->d->icq_TCPSequence--;
  p->id=sequence;
  icq_PacketEnd(p);
  icq_PacketAppend32(p, sequence);

  /* if the link is currently connecting, queue the packets for
   * later, else send immediately */
  if(plink->mode & TCP_LINK_MODE_CONNECTING) {
    list_insert(plink->send_queue, 0, p);
    if(plink->icqlink->icq_RequestNotify)
      (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_CONNECTING, 0, 0);
  } else {
    icq_PacketSend(p, plink->socket);
    if(p->id)
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_SENT, 0, 0);
    icq_PacketDelete(p);
  }
  return sequence;
}

void icq_TCPLinkSend(icq_TCPLink *plink, icq_Packet *p)
{
  /* if the link is currently connecting, queue the packets for
   * later, else send immediately */
  if(plink->mode & TCP_LINK_MODE_CONNECTING) {
    list_insert(plink->send_queue, 0, p);
    if(plink->icqlink->icq_RequestNotify)
      (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_CONNECTING, 0, 0);
  } else {
    icq_PacketSend(p, plink->socket);
    if(p->id)
      if(plink->icqlink->icq_RequestNotify)
        (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_SENT, 0, 0);
    icq_PacketDelete(p);
  }
}

void icq_TCPLinkProcessReceived(icq_TCPLink *plink)
{
  list *plist=plink->received_queue;
  while(plist->count>0)

  {
    icq_Packet *p=list_dequeue(plist);

    if(plink->mode & TCP_LINK_MODE_HELLOWAIT)
    {
      icq_TCPProcessHello(p, plink);
    }
    else
    {

      switch (plink->type) {

        case TCP_LINK_MESSAGE:
          icq_TCPProcessPacket(p, plink);
          break;

        case TCP_LINK_CHAT:
          icq_TCPProcessChatPacket(p, plink);
          break;

        case TCP_LINK_FILE:
          icq_TCPProcessFilePacket(p, plink);
          break;

      }
    }

    icq_PacketDelete(p);
  }

}

int _icq_FindTCPLink(void *p, va_list data)
{
  icq_TCPLink *plink=(icq_TCPLink *)p;
  unsigned long uin=va_arg(data, unsigned long);
  int type=va_arg(data, int);

  return ( (plink->remote_uin == uin ) && (plink->type == type) );
}

icq_TCPLink *icq_FindTCPLink(ICQLINK *link, unsigned long uin, int type)
{
  return list_traverse(link->d->icq_TCPLinks, _icq_FindTCPLink, uin, type);
}

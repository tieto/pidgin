/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: tcplink.c 1987 2001-06-09 14:46:51Z warmenhoven $
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

#include <fcntl.h>
#include <errno.h>

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
#include <netdb.h>
#endif

#include "icqlib.h"
#include "stdpackets.h"
#include "tcp.h"
#include "errno.h"
#include "chatsession.h"
#include "filesession.h"
#include "contacts.h"

icq_TCPLink *icq_TCPLinkNew(icq_Link *icqlink)
{
  icq_TCPLink *p=(icq_TCPLink *)malloc(sizeof(icq_TCPLink));

  p->socket=-1;
  p->icqlink=icqlink;
  p->mode=0;
  p->session=0L;
  p->type=TCP_LINK_MESSAGE;
  p->buffer_count=0;
  p->send_queue=icq_ListNew();
  p->received_queue=icq_ListNew();
  p->id=0;
  p->remote_uin=0;
  p->remote_version=0;
  p->flags=0;
  p->proxy_status = 0;
  p->connect_timeout = NULL;

  if(p)
    icq_ListEnqueue(icqlink->d->icq_TCPLinks, p);

  return p;
}

int _icq_TCPLinkDelete(void *pv, va_list data)
{
  icq_Packet *p=(icq_Packet *)pv;
  icq_Link *icqlink=va_arg(data, icq_Link *);

  /* notify the app the packet didn't make it */
  if(p->id)
    invoke_callback(icqlink, icq_RequestNotify)(icqlink, p->id,
      ICQ_NOTIFY_FAILED, 0, 0);

  return 0;
}

void icq_TCPLinkDelete(void *pv)
{
  icq_TCPLink *p=(icq_TCPLink *)pv;

  /* process anything left in the received queue */
  icq_TCPLinkProcessReceived(p);

  /* make sure we notify app that packets in send queue didn't make it */
  (void)icq_ListTraverse(p->send_queue, _icq_TCPLinkDelete, p->icqlink);

  /* destruct all packets still waiting on queues */
  icq_ListDelete(p->send_queue, icq_PacketDelete);
  icq_ListDelete(p->received_queue, icq_PacketDelete);

  /* if this is a chat or file link, delete the associated session as
   * well, but make sure we unassociate ourself first so the session
   * doesn't try to close us */
  if(p->session)
  {
    if(p->type==TCP_LINK_CHAT)
    {
      icq_ChatSession *psession=p->session;
      psession->tcplink=NULL;
      icq_ChatSessionClose(psession);
    }

    if(p->type==TCP_LINK_FILE) {
      icq_FileSession *psession=p->session;
      psession->tcplink=NULL;
      icq_FileSessionClose(psession);
    }
  }

  /* close the socket after we notify app so app can read errno if necessary */
  if (p->socket > -1)
  {
    icq_SocketDelete(p->socket);
  }

  if (p->connect_timeout)
  {
    icq_TimeoutDelete(p->connect_timeout);
  }

  free(p);
}

void icq_TCPLinkClose(icq_TCPLink *plink)
{
  icq_ListRemove(plink->icqlink->d->icq_TCPLinks, plink);
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

  int hasName = plink->icqlink->icq_ProxyName &&
    strlen(plink->icqlink->icq_ProxyName);
  int hasPass = plink->icqlink->icq_ProxyPass &&
    strlen(plink->icqlink->icq_ProxyPass);
  int authEnabled = hasName && hasPass && plink->icqlink->icq_ProxyAuth;

  plink->mode = (plink->mode & (~TCP_LINK_SOCKS_CONNECTING));
  buf[0] = 5; /* protocol version */
  buf[1] = 1; /* number of methods */
  buf[2] = authEnabled ? 2 : 0; /* authentication method */

  plink->mode |= authEnabled ? TCP_LINK_SOCKS_AUTHORIZATION :
    TCP_LINK_SOCKS_NOAUTHSTATUS;

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

  plink->mode &= ~TCP_LINK_SOCKS_AUTHORIZATION;
  plink->mode |= TCP_LINK_SOCKS_AUTHSTATUS;

#ifdef _WIN32
  res = recv(plink->socket, buf, 2, 0);
#else
  res = read(plink->socket, buf, 2);
#endif
  if(res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication*/
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
    icq_SocketDelete(plink->socket);
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
    icq_SocketDelete(plink->socket);
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
    icq_SocketDelete(plink->socket);
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
    icq_SocketDelete(plink->socket);
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

  if((plink->socket=icq_SocketNew(AF_INET, SOCK_STREAM, 0)) < 0)
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

  /* Send the hello packet */
  p=icq_TCPCreateInitPacket(plink);
  icq_TCPLinkSend(plink, p);

#ifdef TCP_PACKET_TRACE
  printf("hello packet queued for %lu\n", uin);
#endif /* TCP_PACKET_TRACE */

  icq_SocketSetHandler(plink->socket, ICQ_SOCKET_WRITE,
    icq_TCPLinkOnConnect, plink);
  plink->connect_timeout=icq_TimeoutNew(TCP_LINK_CONNECT_TIMEOUT,
    (icq_TimeoutHandler)icq_TCPLinkClose, plink);
  
  return 1;
}

icq_TCPLink *icq_TCPLinkAccept(icq_TCPLink *plink)
{
#ifdef _WIN32
  u_long iosflag;
#else
  int flags;
#endif
  int socket_fd;
  size_t remote_length;
  icq_TCPLink *pnewlink=icq_TCPLinkNew(plink->icqlink);
  
  if(pnewlink)
  {
    remote_length = sizeof(struct sockaddr_in);
    socket_fd=icq_SocketAccept(plink->socket,
      (struct sockaddr *)&(plink->remote_address), &remote_length);

    icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE,
      "accepting tcp connection from %s:%d\n",
      inet_ntoa(*((struct in_addr *)(&(plink->remote_address.sin_addr)))),
      ntohs(plink->remote_address.sin_port));

    /* FIXME: make sure accept succeeded */

    pnewlink->type=plink->type;
    pnewlink->socket=socket_fd;

    /* first packet sent on an icq tcp link is always the hello packet */
    pnewlink->mode|=TCP_LINK_MODE_HELLOWAIT;

    /* install socket handler for new socket */
    icq_SocketSetHandler(socket_fd, ICQ_SOCKET_READ, 
      icq_TCPLinkOnDataReceived, pnewlink);
  }

  /* set the socket to non-blocking */
#ifdef _WIN32
  iosflag = TRUE;
  ioctlsocket(pnewlink->socket, FIONBIO, &iosflag);
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
  if((plink->socket=icq_SocketNew(AF_INET, SOCK_STREAM, 0)) < 0)
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

  icq_SocketSetHandler(plink->socket, ICQ_SOCKET_READ, icq_TCPLinkAccept,
    plink);

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

void icq_TCPLinkOnDataReceived(icq_TCPLink *plink)
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
      invoke_callback(plink->icqlink, icq_ChatNotify)(plink->session,
        CHAT_NOTIFY_DATA, plink->buffer_count, plink->buffer);
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
        return;
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

#ifdef _WIN32
  if (recv_result <= 0 && WSAGetLastError()!=EWOULDBLOCK) {
    /* receive error - log it */
    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "recv failed from %d (%d),"
      " closing link\n", plink->remote_uin, WSAGetLastError());
#else
  if (recv_result <= 0 && errno!=EWOULDBLOCK) {
    /* receive error - log it */
    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "recv failed from %d (%d-%s),"
      " closing link\n", plink->remote_uin, errno, strerror(errno));
#endif

    icq_TCPLinkClose(plink);

  } else {

    icq_TCPLinkProcessReceived(plink);

  }

}

void icq_TCPLinkOnPacketReceived(icq_TCPLink *plink, icq_Packet *p)
{

#ifdef TCP_RAW_TRACE
  printf("packet received! { length=%d }\n", p->length);
  icq_PacketDump(p);
#endif

  /* Stick packet on ready packet linked icq_List */
  icq_ListEnqueue(plink->received_queue, p);
}

void icq_TCPLinkOnConnect(icq_TCPLink *plink)
{
#ifdef _WIN32
  int len;
#else
  size_t len;
#endif
  int error;
  
  icq_TimeoutDelete(plink->connect_timeout);
  plink->connect_timeout = NULL;

  /* check getsockopt */
  len=sizeof(error);

#ifndef __BEOS__
#ifdef _WIN32
  getsockopt(plink->socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len);
#else
  getsockopt(plink->socket, SOL_SOCKET, SO_ERROR, &error, &len);
#endif
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
  {
    icq_SocketSetHandler(plink->socket, ICQ_SOCKET_WRITE, NULL, NULL);
    icq_SocketSetHandler(plink->socket, ICQ_SOCKET_READ, 
      icq_TCPLinkOnConnect, plink);
    return;
  }

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

  icq_SocketSetHandler(plink->socket, ICQ_SOCKET_READ, 
    icq_TCPLinkOnDataReceived, plink);
  icq_SocketSetHandler(plink->socket, ICQ_SOCKET_WRITE, NULL, NULL);

  /* socket is now connected, notify each request that connection
   * has been established and send pending data */
  while(plink->send_queue->count>0)
  {
    icq_Packet *p=icq_ListDequeue(plink->send_queue);
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
    icq_ListInsert(plink->send_queue, 0, p);
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
    icq_ListInsert(plink->send_queue, 0, p);
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
  icq_List *plist=plink->received_queue;
  while(plist->count>0)

  {
    icq_Packet *p=icq_ListDequeue(plist);

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

icq_TCPLink *icq_FindTCPLink(icq_Link *icqlink, unsigned long uin, int type)
{
  return icq_ListTraverse(icqlink->d->icq_TCPLinks, _icq_FindTCPLink, uin, type);
}

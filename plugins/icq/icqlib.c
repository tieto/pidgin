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

#include "icqlib.h"

#include <stdlib.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

#include <sys/stat.h>

#include "util.h"
#include "icq.h"
#include "udp.h"
#include "tcp.h"
#include "queue.h"
#include "socketmanager.h"
#include "contacts.h"

int icq_Russian = FALSE;
BYTE icq_LogLevel = 0;

DWORD icq_SendMessage(icq_Link *icqlink, DWORD uin, const char *text, 
  BYTE thruSrv)
{
  if(thruSrv==ICQ_SEND_THRUSERVER)
    return icq_UDPSendMessage(icqlink, uin, text);
  else if(thruSrv==ICQ_SEND_DIRECT)
    return icq_TCPSendMessage(icqlink, uin, text);
  else if(thruSrv==ICQ_SEND_BESTWAY)
  {
    icq_ContactItem *pcontact=icq_ContactFind(icqlink, uin);
    if(pcontact)
    {
      if(pcontact->tcp_flag == 0x04)
        return icq_TCPSendMessage(icqlink, uin, text);
      else
        return icq_UDPSendMessage(icqlink, uin, text);
    }
    else
    {
      return icq_UDPSendMessage(icqlink, uin, text);
    }
  }
  return 0;
}

DWORD icq_SendURL(icq_Link *icqlink, DWORD uin, const char *url, 
  const char *descr, BYTE thruSrv)
{
  if(thruSrv==ICQ_SEND_THRUSERVER)
    return icq_UDPSendURL(icqlink, uin, url, descr);
  else if(thruSrv==ICQ_SEND_DIRECT)
    return icq_TCPSendURL(icqlink, uin, descr, url);
  else if(thruSrv==ICQ_SEND_BESTWAY)
  {
    icq_ContactItem *pcontact=icq_ContactFind(icqlink, uin);
    if(pcontact)
    {
      if(pcontact->tcp_flag == 0x04)
        return icq_TCPSendURL(icqlink, uin, descr, url);
      else
        return icq_UDPSendURL(icqlink, uin, url, descr);
    }
    else
    {
      return icq_UDPSendURL(icqlink, uin, url, descr);
    }
  }
  return 0;
}

static int icqlib_initialized = 0;

void icq_LibInit()
{
  srand(time(0L));

  /* initialize internal lists, if necessary */
  if (!icq_SocketList)
    icq_SocketList = icq_ListNew();

  if (!icq_TimeoutList)
  {
    icq_TimeoutList = icq_ListNew();
    icq_TimeoutList->compare_function =
      (icq_ListCompareFunc)icq_TimeoutCompare;
  }

  icqlib_initialized = 1;
}

icq_Link *icq_LinkNew(DWORD uin, const char *password, const char *nick,
                        unsigned char useTCP)
{
  icq_Link *icqlink = (icq_Link *)malloc(sizeof(icq_Link));

  icq_LinkInit(icqlink, uin, password, nick, useTCP);

  return icqlink;
}

void icq_LinkInit(icq_Link *icqlink, DWORD uin, const char *password, 
  const char *nick, unsigned char useTCP)
{
  icqlink->d = (icq_LinkPrivate *)malloc(sizeof(icq_LinkPrivate));

  if (!icqlib_initialized)
    icq_LibInit();

  /* Initialize all callbacks */
  icqlink->icq_Logged = 0L;
  icqlink->icq_Disconnected = 0L;
  icqlink->icq_RecvMessage = 0L;
  icqlink->icq_RecvURL = 0L;
  icqlink->icq_RecvContactList = 0L;
  icqlink->icq_RecvWebPager = 0L;
  icqlink->icq_RecvMailExpress = 0L;
  icqlink->icq_RecvChatReq = 0L;
  icqlink->icq_RecvFileReq = 0L;
  icqlink->icq_RecvAdded = 0L;
  icqlink->icq_RecvAuthReq = 0L;
  icqlink->icq_UserFound = 0L;
  icqlink->icq_SearchDone = 0L;
  icqlink->icq_UserOnline = 0L;
  icqlink->icq_UserOffline = 0L;
  icqlink->icq_UserStatusUpdate = 0L;
  icqlink->icq_InfoReply = 0L;
  icqlink->icq_ExtInfoReply = 0L;
  icqlink->icq_WrongPassword = 0L;
  icqlink->icq_InvalidUIN = 0L;
  icqlink->icq_Log = 0L;
  icqlink->icq_SrvAck = 0L;
  icqlink->icq_RequestNotify = 0L;
  icqlink->icq_NewUIN = 0L;
  icqlink->icq_MetaUserFound = 0L;
  icqlink->icq_MetaUserInfo = 0L;
  icqlink->icq_MetaUserWork = 0L;
  icqlink->icq_MetaUserMore = 0L;
  icqlink->icq_MetaUserAbout = 0L;
  icqlink->icq_MetaUserInterests = 0L;
  icqlink->icq_MetaUserAffiliations = 0L;
  icqlink->icq_MetaUserHomePageCategory = 0L;

  /* General stuff */
  icqlink->icq_Uin = uin;
  icqlink->icq_Password = strdup(password);
  icqlink->icq_Nick = strdup(nick);
  icqlink->icq_OurIP = -1;
  icqlink->icq_OurPort = 0;
  icqlink->d->icq_ContactList = icq_ListNew();
  icqlink->icq_Status = -1;
  icqlink->icq_UserData = 0L;

  /* UDP stuff */
  icqlink->icq_UDPSok = -1;
  memset(icqlink->d->icq_UDPServMess, FALSE, 
    sizeof(icqlink->d->icq_UDPServMess));
  icqlink->d->icq_UDPSeqNum1 = 0;
  icqlink->d->icq_UDPSeqNum2 = 0;
  icqlink->d->icq_UDPSession = 0;
  icq_UDPQueueNew(icqlink);

  /* TCP stuff */
  icqlink->icq_UseTCP = useTCP;
  if (useTCP)
    icq_TCPInit(icqlink);

  /* Proxy stuff */
  icqlink->icq_UseProxy = 0;
  icqlink->icq_ProxyHost = 0L;
  icqlink->icq_ProxyIP = -1;
  icqlink->icq_ProxyPort = 0;
  icqlink->icq_ProxyAuth = 0;
  icqlink->icq_ProxyName = 0L;
  icqlink->icq_ProxyPass = 0L;
  icqlink->icq_ProxySok = -1;
  icqlink->icq_ProxyOurPort = 0;
  icqlink->icq_ProxyDestIP = -1;
  icqlink->icq_ProxyDestPort = 0;
}

void icq_LinkDestroy(icq_Link *icqlink)
{
  icq_TCPDone(icqlink);
  if(icqlink->icq_Password)
    free(icqlink->icq_Password);
  if(icqlink->icq_Nick)
    free(icqlink->icq_Nick);
  if(icqlink->d->icq_ContactList)
    icq_ListDelete(icqlink->d->icq_ContactList, icq_ContactDelete);
  icq_UDPQueueDelete(icqlink);
  free(icqlink->d);
}

void icq_LinkDelete(icq_Link *icqlink)
{
  icq_LinkDestroy(icqlink);
  free(icqlink);
}

/******************************
Main function connects gets icq_Uin
and icq_Password and logins in and sits
in a loop waiting for server responses.
*******************************/
void icq_Main()
{
  icq_SocketPoll();
}

/**********************************
Connects to hostname on port port
hostname can be DNS or nnn.nnn.nnn.nnn
write out messages to the FD aux
***********************************/
int icq_Connect(icq_Link *icqlink, const char *hostname, int port)
{
  char buf[1024]; /*, un = 1;*/
/*  char tmpbuf[256], our_host[256]*/
  int conct, res;
  unsigned int length;
  struct sockaddr_in saddr, prsin;  /* used to store inet addr stuff */
  struct hostent *host_struct; /* used in DNS llokup */

  /* create the unconnected socket*/
  icqlink->icq_UDPSok = icq_SocketNew(AF_INET, SOCK_DGRAM, 0);

  if(icqlink->icq_UDPSok == -1)
  {
    icq_FmtLog(icqlink, ICQ_LOG_FATAL, "Socket creation failed\n");
    return -1;
  }
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Socket created attempting to connect\n");
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_family = AF_INET; /* we're using the inet not appletalk*/
  saddr.sin_port = 0;
  if(bind(icqlink->icq_UDPSok, (struct sockaddr*)&saddr, sizeof(struct sockaddr))<0)
  {
    icq_FmtLog(icqlink, ICQ_LOG_FATAL, "Can't bind socket to free port\n");
    icq_SocketDelete(icqlink->icq_UDPSok);
    icqlink->icq_UDPSok = -1;
    return -1;
  }
  length = sizeof(saddr);
  getsockname(icqlink->icq_UDPSok, (struct sockaddr*)&saddr, &length);
  icqlink->icq_ProxyOurPort = ntohs(saddr.sin_port);
  if(icqlink->icq_UseProxy)
  {
    icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "[SOCKS] Trying to use SOCKS5 proxy\n");
    prsin.sin_addr.s_addr = inet_addr(icqlink->icq_ProxyHost);
    if(prsin.sin_addr.s_addr  == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
    {
      host_struct = gethostbyname(icqlink->icq_ProxyHost);
      if(host_struct == 0L)
      {
        icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Can't find hostname: %s\n", icqlink->icq_ProxyHost);
        return -1;
      }
      prsin.sin_addr = *((struct in_addr*)host_struct->h_addr);
    }
    icqlink->icq_ProxyIP = ntohl(prsin.sin_addr.s_addr);
    prsin.sin_family = AF_INET; /* we're using the inet not appletalk*/
    prsin.sin_port = htons(icqlink->icq_ProxyPort); /* port */

    /* create the unconnected socket*/
    icqlink->icq_ProxySok = icq_SocketNew(AF_INET, SOCK_STREAM, 0);

    if(icqlink->icq_ProxySok == -1)
    {
      icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Socket creation failed\n");
      return -1;
    }
    icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "[SOCKS] Socket created attempting to connect\n");
    conct = connect(icqlink->icq_ProxySok, (struct sockaddr *) &prsin, sizeof(prsin));
    if(conct == -1) /* did we connect ?*/
    {
      icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
      return -1;
    }
    buf[0] = 5; /* protocol version */
    buf[1] = 1; /* number of methods */
    if(!strlen(icqlink->icq_ProxyName) || !strlen(icqlink->icq_ProxyPass) || !icqlink->icq_ProxyAuth)
      buf[2] = 0; /* no authorization required */
    else
      buf[2] = 2; /* method username/password */
#ifdef _WIN32
    send(icqlink->icq_ProxySok, buf, 3, 0);
    res = recv(icqlink->icq_ProxySok, buf, 2, 0);
#else
    write(icqlink->icq_ProxySok, buf, 3);
    res = read(icqlink->icq_ProxySok, buf, 2);
#endif
    if(strlen(icqlink->icq_ProxyName) && strlen(icqlink->icq_ProxyPass) && icqlink->icq_ProxyAuth)
    {
      if(res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication*/
      {
        icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
        icq_SocketDelete(icqlink->icq_ProxySok);
        return -1;
      }
      buf[0] = 1; /* version of subnegotiation */
      buf[1] = strlen(icqlink->icq_ProxyName);
      memcpy(&buf[2], icqlink->icq_ProxyName, buf[1]);
      buf[2+buf[1]] = strlen(icqlink->icq_ProxyPass);
      memcpy(&buf[3+buf[1]], icqlink->icq_ProxyPass, buf[2+buf[1]]);
#ifdef _WIN32
      send(icqlink->icq_ProxySok, buf, buf[1]+buf[2+buf[1]]+3, 0);
      res = recv(icqlink->icq_ProxySok, buf, 2, 0);
#else
      write(icqlink->icq_ProxySok, buf, buf[1]+buf[2+buf[1]]+3);
      res = read(icqlink->icq_ProxySok, buf, 2);
#endif
      if(res != 2 || buf[0] != 1 || buf[1] != 0)
      {
        icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Authorization failure\n");
        icq_SocketDelete(icqlink->icq_ProxySok);
        return -1;
      }
    }
    else
    {
      if(res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
      {
        icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
        icq_SocketDelete(icqlink->icq_ProxySok);
        return -1;
      }
    }
    buf[0] = 5; /* protocol version */
    buf[1] = 3; /* command UDP associate */
    buf[2] = 0; /* reserved */
    buf[3] = 1; /* address type IP v4 */
    buf[4] = (char)0;
    buf[5] = (char)0;
    buf[6] = (char)0;
    buf[7] = (char)0;
    *(unsigned short*)&buf[8] = htons(icqlink->icq_ProxyOurPort);
/*     memcpy(&buf[8], &icqlink->icq_ProxyOurPort, 2); */
#ifdef _WIN32
    send(icqlink->icq_ProxySok, buf, 10, 0);
    res = recv(icqlink->icq_ProxySok, buf, 10, 0);
#else
    write(icqlink->icq_ProxySok, buf, 10);
    res = read(icqlink->icq_ProxySok, buf, 10);
#endif
    if(res != 10 || buf[0] != 5 || buf[1] != 0)
    {
      switch(buf[1])
      {
        case 1:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] General SOCKS server failure\n");
          break;
        case 2:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection not allowed by ruleset\n");
          break;
        case 3:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Network unreachable\n");
          break;
        case 4:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Host unreachable\n");
          break;
        case 5:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
          break;
        case 6:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] TTL expired\n");
          break;
        case 7:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Command not supported\n");
          break;
        case 8:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Address type not supported\n");
          break;
        default:
          icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Unknown SOCKS server failure\n");
          break;
      }
      icq_SocketDelete(icqlink->icq_ProxySok);
      icqlink->icq_ProxySok = -1;
      return -1;
    }
  }
  saddr.sin_addr.s_addr = inet_addr(hostname); /* checks for n.n.n.n notation */
  if(saddr.sin_addr.s_addr == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
  {
    host_struct = gethostbyname(hostname);
    if(host_struct == 0L)
    {
      icq_FmtLog(icqlink, ICQ_LOG_FATAL, "Can't find hostname: %s\n", hostname);
      if(icqlink->icq_UseProxy)
      {
        icq_SocketDelete(icqlink->icq_ProxySok);
      }
      return -1;
    }
    saddr.sin_addr = *((struct in_addr *)host_struct->h_addr);
  }
  if(icqlink->icq_UseProxy)
  {
    icqlink->icq_ProxyDestIP = ntohl(saddr.sin_addr.s_addr);
    memcpy(&saddr.sin_addr.s_addr, &buf[4], 4);
  }
  saddr.sin_family = AF_INET; /* we're using the inet not appletalk*/
  saddr.sin_port = htons(port); /* port */
  if(icqlink->icq_UseProxy)
  {
    icqlink->icq_ProxyDestPort = port;
    memcpy(&saddr.sin_port, &buf[8], 2);
  }
  conct = connect(icqlink->icq_UDPSok, (struct sockaddr*)&saddr, sizeof(saddr));
  if(conct == -1) /* did we connect ?*/
  {
    icq_FmtLog(icqlink, ICQ_LOG_FATAL, "Connection refused\n");
    if(icqlink->icq_UseProxy)
    {
      icq_SocketDelete(icqlink->icq_ProxySok);
    }
    return -1;
  }
  length = sizeof(saddr) ;
  getsockname(icqlink->icq_UDPSok, (struct sockaddr*)&saddr, &length);
  icqlink->icq_OurIP = ntohl(saddr.sin_addr.s_addr);
  icqlink->icq_OurPort = ntohs(saddr.sin_port);

  /* sockets are ready to receive data - install handlers */
  icq_SocketSetHandler(icqlink->icq_UDPSok, ICQ_SOCKET_READ,
    (icq_SocketHandler)icq_HandleServerResponse, icqlink);
  if (icqlink->icq_UseProxy)
    icq_SocketSetHandler(icqlink->icq_ProxySok, ICQ_SOCKET_READ,
      (icq_SocketHandler)icq_HandleProxyResponse, icqlink);
  return icqlink->icq_UDPSok;
}

void icq_Disconnect(icq_Link *icqlink)
{
  icq_SocketDelete(icqlink->icq_UDPSok);
  if(icqlink->icq_UseProxy)
  {
    icq_SocketDelete(icqlink->icq_ProxySok);
  }
  icq_UDPQueueFree(icqlink);
}

/*
void icq_InitNewUser(const char *hostname, DWORD port)
{
  srv_net_icq_pak pak;
  int s;
  struct timeval tv;
  fd_set readfds;

  icq_Connect(hostname, port);
  if((icq_UDPSok == -1) || (icq_UDPSok == 0))
  {
    printf("Couldn't establish connection\n");
    exit(1);
  }
  icq_RegNewUser(icq_Password);
  for(;;)
  {
    tv.tv_sec = 2;
    tv.tv_usec = 500000;

    FD_ZERO(&readfds);
    FD_SET(icq_UDPSok, &readfds);

    select(icq_UDPSok+1, &readfds, 0L, 0L, &tv);

    if(FD_ISSET(icq_UDPSok, &readfds))
    {
      s = icq_UDPSockRead(icq_UDPSok, &pak.head, sizeof(pak));
      if(icqtohs(pak.head.cmd) == SRV_NEW_UIN)
      {
        icq_Uin = icqtohl(&pak.data[2]);
        return;
      }
    }
  }
}
*/

/************************
icq_UDPServMess functions
*************************/
BOOL icq_GetServMess(icq_Link *icqlink, WORD num)
{
  return ((icqlink->d->icq_UDPServMess[num/8] & (1 << (num%8))) >> (num%8));
}

void icq_SetServMess(icq_Link *icqlink, WORD num)
{
  icqlink->d->icq_UDPServMess[num/8] |= (1 << (num%8));
}

int icq_GetSok(icq_Link *icqlink)
{
  return icqlink->icq_UDPSok;
}

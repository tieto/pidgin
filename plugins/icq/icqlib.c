/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: icqlib.c 1442 2001-01-28 01:52:27Z warmenhoven $
$Log$
Revision 1.3  2001/01/28 01:52:27  warmenhoven
icqlib 1.1.5

Revision 1.49  2001/01/17 01:29:17  bills
Rework chat and file session interfaces; implement socket notifications.

Revision 1.48  2001/01/15 06:20:24  denis
Cleanup.

Revision 1.47  2000/12/19 21:29:51  bills
actually return the link in icq_ICQLINKNew

Revision 1.46  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.45  2000/11/02 07:29:07  denis
Ability to disable TCP protocol has been added.

Revision 1.44  2000/07/24 03:10:08  bills
added support for real nickname during TCP transactions like file and
chat, instead of using Bill all the time (hmm, where'd I get that from? :)

Revision 1.43  2000/07/09 22:05:11  bills
removed unnecessary functions
*/

#include "icqlib.h"

#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <time.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

#include <sys/stat.h>

#ifndef _WIN32
#include <sys/time.h>
#include <netinet/in.h>
#endif

#include "util.h"
#include "icqtypes.h"
#include "icq.h"
#include "udp.h"
#include "tcp.h"
#include "queue.h"
#include "socketmanager.h"

int icq_Russian = FALSE;
BYTE icq_LogLevel = 0;

void (*icq_SocketNotify)(int socket, int type, int status);

DWORD icq_SendMessage(ICQLINK *link, DWORD uin, const char *text, BYTE thruSrv)
{
  if(thruSrv==ICQ_SEND_THRUSERVER)
    return icq_UDPSendMessage(link, uin, text);
  else if(thruSrv==ICQ_SEND_DIRECT)
    return icq_TCPSendMessage(link, uin, text);
  else if(thruSrv==ICQ_SEND_BESTWAY)
  {
    icq_ContactItem *pcontact=icq_ContactFind(link, uin);
    if(pcontact)
    {
      if(pcontact->tcp_flag == 0x04)
        return icq_TCPSendMessage(link, uin, text);
      else
        return icq_UDPSendMessage(link, uin, text);
    }
    else
    {
      return icq_UDPSendMessage(link, uin, text);
    }
  }
  return 0;
}

DWORD icq_SendURL(ICQLINK *link, DWORD uin, const char *url, const char *descr, BYTE thruSrv)
{
  if(thruSrv==ICQ_SEND_THRUSERVER)
    return icq_UDPSendURL(link, uin, url, descr);
  else if(thruSrv==ICQ_SEND_DIRECT)
    return icq_TCPSendURL(link, uin, descr, url);
  else if(thruSrv==ICQ_SEND_BESTWAY)
  {
    icq_ContactItem *pcontact=icq_ContactFind(link, uin);
    if(pcontact)
    {
      if(pcontact->tcp_flag == 0x04)
        return icq_TCPSendURL(link, uin, descr, url);
      else
        return icq_UDPSendURL(link, uin, url, descr);
    }
    else
    {
      return icq_UDPSendURL(link, uin, url, descr);
    }
  }
  return 0;
}

ICQLINK *icq_ICQLINKNew(DWORD uin, const char *password, const char *nick,
                        unsigned char useTCP)
{
  ICQLINK *link = (ICQLINK*)malloc(sizeof(ICQLINK));
  link->d = (ICQLINK_private*)malloc(sizeof(ICQLINK_private));

  srand(time(0L));
  /* initialize icq_SocketList on first call */
  if (!icq_SocketList)
    icq_SocketList = list_new();

  /* Initialize all callbacks */
  link->icq_Logged = 0L;
  link->icq_Disconnected = 0L;
  link->icq_RecvMessage = 0L;
  link->icq_RecvURL = 0L;
  link->icq_RecvWebPager = 0L;
  link->icq_RecvMailExpress = 0L;
  link->icq_RecvChatReq = 0L;
  link->icq_RecvFileReq = 0L;
  link->icq_RecvAdded = 0L;
  link->icq_RecvAuthReq = 0L;
  link->icq_UserFound = 0L;
  link->icq_SearchDone = 0L;
  link->icq_UserOnline = 0L;
  link->icq_UserOffline = 0L;
  link->icq_UserStatusUpdate = 0L;
  link->icq_InfoReply = 0L;
  link->icq_ExtInfoReply = 0L;
  link->icq_WrongPassword = 0L;
  link->icq_InvalidUIN = 0L;
  link->icq_Log = 0L;
  link->icq_SrvAck = 0L;
  link->icq_RequestNotify = 0L;
  link->icq_NewUIN = 0L;
  link->icq_SetTimeout = 0L;
  link->icq_MetaUserFound = 0L;
  link->icq_MetaUserInfo = 0L;
  link->icq_MetaUserWork = 0L;
  link->icq_MetaUserMore = 0L;
  link->icq_MetaUserAbout = 0L;
  link->icq_MetaUserInterests = 0L;
  link->icq_MetaUserAffiliations = 0L;
  link->icq_MetaUserHomePageCategory = 0L;

  /* General stuff */
  link->icq_Uin = uin;
  link->icq_Password = strdup(password);
  link->icq_Nick = strdup(nick);
  link->icq_OurIP = -1;
  link->icq_OurPort = 0;
  link->d->icq_ContactList = list_new();
  link->icq_Status = -1;

  /* UDP stuff */
  link->icq_UDPSok = -1;
  memset(link->d->icq_UDPServMess, FALSE, sizeof(link->d->icq_UDPServMess));
  link->d->icq_UDPSeqNum1 = 0;
  link->d->icq_UDPSeqNum2 = 0;
  link->d->icq_UDPSession = 0;
  icq_UDPQueueNew(link);

  /* TCP stuff */
  icq_TCPInit(link);
  link->icq_UseTCP = useTCP;

  /* Proxy stuff */
  link->icq_UseProxy = 0;
  link->icq_ProxyHost = 0L;
  link->icq_ProxyIP = -1;
  link->icq_ProxyPort = 0;
  link->icq_ProxyAuth = 0;
  link->icq_ProxyName = 0L;
  link->icq_ProxyPass = 0L;
  link->icq_ProxySok = -1;
  link->icq_ProxyOurPort = 0;
  link->icq_ProxyDestIP = -1;
  link->icq_ProxyDestPort = 0;

  return link;
}

void icq_ICQLINKDelete(ICQLINK *link)
{
  icq_TCPDone(link);
  if(link->icq_Password)
    free(link->icq_Password);
  if(link->icq_Nick)
    free(link->icq_Nick);
  if(link->d->icq_ContactList)
    list_delete(link->d->icq_ContactList, icq_ContactDelete);
  icq_UDPQueueDelete(link);
  free(link->d);
  free(link);
}

/******************************
Main function connects gets icq_Uin
and icq_Password and logins in and sits
in a loop waiting for server responses.
*******************************/
void icq_Main(ICQLINK *link)
{
  icq_SocketPoll();
}

/**********************************
Connects to hostname on port port
hostname can be DNS or nnn.nnn.nnn.nnn
write out messages to the FD aux
***********************************/
int icq_Connect(ICQLINK *link, const char *hostname, int port)
{
  char buf[1024]; /*, un = 1;*/
/*  char tmpbuf[256], our_host[256]*/
  int conct, res;
  unsigned int length;
  struct sockaddr_in sin, prsin;  /* used to store inet addr stuff */
  struct hostent *host_struct; /* used in DNS llokup */

  /* create the unconnected socket*/
  link->icq_UDPSok = icq_SocketNew(AF_INET, SOCK_DGRAM, 0);

  if(link->icq_UDPSok == -1)
  {
    icq_FmtLog(link, ICQ_LOG_FATAL, "Socket creation failed\n");
    return -1;
  }
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Socket created attempting to connect\n");
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
  sin.sin_port = 0;
  if(bind(link->icq_UDPSok, (struct sockaddr*)&sin, sizeof(struct sockaddr))<0)
  {
    icq_FmtLog(link, ICQ_LOG_FATAL, "Can't bind socket to free port\n");
    icq_SocketDelete(link->icq_UDPSok);
    link->icq_UDPSok = -1;
    return -1;
  }
  length = sizeof(sin);
  getsockname(link->icq_UDPSok, (struct sockaddr*)&sin, &length);
  link->icq_ProxyOurPort = ntohs(sin.sin_port);
  if(link->icq_UseProxy)
  {
    icq_FmtLog(link, ICQ_LOG_MESSAGE, "[SOCKS] Trying to use SOCKS5 proxy\n");
    prsin.sin_addr.s_addr = inet_addr(link->icq_ProxyHost);
    if(prsin.sin_addr.s_addr  == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
    {
      host_struct = gethostbyname(link->icq_ProxyHost);
      if(host_struct == 0L)
      {
        icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Can't find hostname: %s\n", link->icq_ProxyHost);
        return -1;
      }
      prsin.sin_addr = *((struct in_addr*)host_struct->h_addr);
    }
    link->icq_ProxyIP = ntohl(prsin.sin_addr.s_addr);
    prsin.sin_family = AF_INET; /* we're using the inet not appletalk*/
    prsin.sin_port = htons(link->icq_ProxyPort); /* port */

    /* create the unconnected socket*/
    link->icq_ProxySok = icq_SocketNew(AF_INET, SOCK_STREAM, 0);

    if(link->icq_ProxySok == -1)
    {
      icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Socket creation failed\n");
      return -1;
    }
    icq_FmtLog(link, ICQ_LOG_MESSAGE, "[SOCKS] Socket created attempting to connect\n");
    conct = connect(link->icq_ProxySok, (struct sockaddr *) &prsin, sizeof(prsin));
    if(conct == -1) /* did we connect ?*/
    {
      icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
      return -1;
    }
    buf[0] = 5; /* protocol version */
    buf[1] = 1; /* number of methods */
    if(!strlen(link->icq_ProxyName) || !strlen(link->icq_ProxyPass) || !link->icq_ProxyAuth)
      buf[2] = 0; /* no authorization required */
    else
      buf[2] = 2; /* method username/password */
#ifdef _WIN32
    send(link->icq_ProxySok, buf, 3, 0);
    res = recv(link->icq_ProxySok, buf, 2, 0);
#else
    write(link->icq_ProxySok, buf, 3);
    res = read(link->icq_ProxySok, buf, 2);
#endif
    if(strlen(link->icq_ProxyName) && strlen(link->icq_ProxyPass) && link->icq_ProxyAuth)
    {
      if(res != 2 || buf[0] != 5 || buf[1] != 2) /* username/password authentication*/
      {
        icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
        icq_SocketDelete(link->icq_ProxySok);
        return -1;
      }
      buf[0] = 1; /* version of subnegotiation */
      buf[1] = strlen(link->icq_ProxyName);
      memcpy(&buf[2], link->icq_ProxyName, buf[1]);
      buf[2+buf[1]] = strlen(link->icq_ProxyPass);
      memcpy(&buf[3+buf[1]], link->icq_ProxyPass, buf[2+buf[1]]);
#ifdef _WIN32
      send(link->icq_ProxySok, buf, buf[1]+buf[2+buf[1]]+3, 0);
      res = recv(link->icq_ProxySok, buf, 2, 0);
#else
      write(link->icq_ProxySok, buf, buf[1]+buf[2+buf[1]]+3);
      res = read(link->icq_ProxySok, buf, 2);
#endif
      if(res != 2 || buf[0] != 1 || buf[1] != 0)
      {
        icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Authorization failure\n");
        icq_SocketDelete(link->icq_ProxySok);
        return -1;
      }
    }
    else
    {
      if(res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
      {
        icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
        icq_SocketDelete(link->icq_ProxySok);
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
    *(unsigned short*)&buf[8] = htons(link->icq_ProxyOurPort);
/*     memcpy(&buf[8], &link->icq_ProxyOurPort, 2); */
#ifdef _WIN32
    send(link->icq_ProxySok, buf, 10, 0);
    res = recv(link->icq_ProxySok, buf, 10, 0);
#else
    write(link->icq_ProxySok, buf, 10);
    res = read(link->icq_ProxySok, buf, 10);
#endif
    if(res != 10 || buf[0] != 5 || buf[1] != 0)
    {
      switch(buf[1])
      {
        case 1:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] General SOCKS server failure\n");
          break;
        case 2:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Connection not allowed by ruleset\n");
          break;
        case 3:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Network unreachable\n");
          break;
        case 4:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Host unreachable\n");
          break;
        case 5:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Connection refused\n");
          break;
        case 6:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] TTL expired\n");
          break;
        case 7:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Command not supported\n");
          break;
        case 8:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Address type not supported\n");
          break;
        default:
          icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Unknown SOCKS server failure\n");
          break;
      }
      icq_SocketDelete(link->icq_ProxySok);
      link->icq_ProxySok = -1;
      return -1;
    }
  }
  sin.sin_addr.s_addr = inet_addr(hostname); /* checks for n.n.n.n notation */
  if(sin.sin_addr.s_addr == (unsigned long)-1) /* name isn't n.n.n.n so must be DNS */
  {
    host_struct = gethostbyname(hostname);
    if(host_struct == 0L)
    {
      icq_FmtLog(link, ICQ_LOG_FATAL, "Can't find hostname: %s\n", hostname);
      if(link->icq_UseProxy)
      {
        icq_SocketDelete(link->icq_ProxySok);
      }
      return -1;
    }
    sin.sin_addr = *((struct in_addr *)host_struct->h_addr);
  }
  if(link->icq_UseProxy)
  {
    link->icq_ProxyDestIP = ntohl(sin.sin_addr.s_addr);
    memcpy(&sin.sin_addr.s_addr, &buf[4], 4);
  }
  sin.sin_family = AF_INET; /* we're using the inet not appletalk*/
  sin.sin_port = htons(port); /* port */
  if(link->icq_UseProxy)
  {
    link->icq_ProxyDestPort = port;
    memcpy(&sin.sin_port, &buf[8], 2);
  }
  conct = connect(link->icq_UDPSok, (struct sockaddr*)&sin, sizeof(sin));
  if(conct == -1) /* did we connect ?*/
  {
    icq_FmtLog(link, ICQ_LOG_FATAL, "Connection refused\n");
    if(link->icq_UseProxy)
    {
      icq_SocketDelete(link->icq_ProxySok);
    }
    return -1;
  }
  length = sizeof(sin) ;
  getsockname(link->icq_UDPSok, (struct sockaddr*)&sin, &length);
  link->icq_OurIP = ntohl(sin.sin_addr.s_addr);
  link->icq_OurPort = ntohs(sin.sin_port);

  /* sockets are ready to receive data - install handlers */
  icq_SocketSetHandler(link->icq_UDPSok, ICQ_SOCKET_READ,
    icq_HandleServerResponse, link);
  if (link->icq_UseProxy)
    icq_SocketSetHandler(link->icq_ProxySok, ICQ_SOCKET_READ,
      icq_HandleProxyResponse, link);
  return link->icq_UDPSok;
}

void icq_Disconnect(ICQLINK *link)
{
  icq_SocketDelete(link->icq_UDPSok);
  if(link->icq_UseProxy)
  {
    icq_SocketDelete(link->icq_ProxySok);
  }
  icq_UDPQueueFree(link);
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
BOOL icq_GetServMess(ICQLINK *link, WORD num)
{
  return ((link->d->icq_UDPServMess[num/8] & (1 << (num%8))) >> (num%8));
}

void icq_SetServMess(ICQLINK *link, WORD num)
{
  link->d->icq_UDPServMess[num/8] |= (1 << (num%8));
}

int icq_GetSok(ICQLINK *link)
{
  return link->icq_UDPSok;
}

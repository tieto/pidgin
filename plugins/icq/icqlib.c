/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: icqlib.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.46  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.45  2000/11/02 07:29:07  denis
Ability to disable TCP protocol has been added.

Revision 1.44  2000/07/24 03:10:08  bills
added support for real nickname during TCP transactions like file and
chat, instead of using Bill all the time (hmm, where'd I get that from? :)

Revision 1.43  2000/07/09 22:05:11  bills
removed unnecessary functions

Revision 1.42  2000/07/09 18:28:07  denis
Initial memset() in icq_Init() replaced by callback's clearance.

Revision 1.41  2000/06/15 01:50:39  bills
removed *Seq functions

Revision 1.40  2000/05/10 19:06:59  denis
UDP outgoing packet queue was implemented.

Revision 1.39  2000/05/03 18:12:36  denis
Unfinished UDP queue was commented out.

Revision 1.38  2000/04/10 16:36:04  denis
Some more Win32 compatibility from Guillaume Rosanis <grs@mail.com>

Revision 1.37  2000/04/06 16:38:04  denis
icq_*Send*Seq() functions with specified sequence number were added.

Revision 1.36  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.35  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.34  1999/12/27 16:06:32  bills
cleanups

Revision 1.33  1999/10/03 21:35:55  tim
Fixed "url" and "descr" parameters order when sending a URL via TCP.

Revision 1.32  1999/09/29 16:49:43  denis
Host/network/icq byteorder systemized.
icq_Init() cleaned up.

Revision 1.31  1999/07/18 20:15:55  bills
changed to use new byte-order functions & contact list functions

Revision 1.30  1999/07/16 12:27:06  denis
Other global variables moved to ICQLINK structure.
Initialization of random number generator added in icq_Init()
Cleaned up.

Revision 1.29  1999/07/12 15:13:31  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.28  1999/07/03 02:26:02  bills
added new code to support thruSrv arg to SendMessage and SendURL

Revision 1.27  1999/04/17 19:21:37  bills
modified Send* Functions to return DWORD instead of WORD

Revision 1.26  1999/04/14 14:48:18  denis
Switched from icq_Log callback to icq_Fmt function.
Cleanups for "strict" compiling (-ansi -pedantic)

Revision 1.25  1999/04/05 13:14:57  denis
Send messages and URLs to 'not in list' users fixed.

Revision 1.24  1999/03/31 01:43:40  bills
added TCP support to SendURL

Revision 1.23  1999/03/30 22:47:44  lord
list of countries now sorted.

Revision 1.22  1999/03/28 03:18:22  bills
enable tcp messaging in icq_SendMessage, uncommented icq_OurPort and
icq_OurIp and fixed function names so icqlib compiles

Revision 1.21  1999/03/25 22:16:43  bills
added #include "util.h"

Revision 1.20  1999/03/24 11:37:36  denis
Underscored files with TCP stuff renamed.
TCP stuff cleaned up
Function names changed to corresponding names.
icqlib.c splitted to many small files by subject.
C++ comments changed to ANSI C comments.

Revision 1.19  1999/03/22 20:51:28  bills
added code in icq_HandleUserOnline to set/clear new struct entries in
icq_ContactItem; added cleanup code in icq_HandleUserOffline for same

Revision 1.18  1999/03/09 13:14:05  denis
Cyrillic recoding removed from URLs

Revision 1.17  1999/03/05 13:57:54  denis
Some cosmetic changes...

Revision 1.16  1998/12/08 16:00:59  denis
Cleaned up a little before releasing

Revision 1.15  1998/11/25 19:18:16  denis
Added close icq_ProxySok in icq_Disconnect

Revision 1.14  1998/11/25 09:48:49  denis
icq_GetProxySok and icq_HandleProxyResponse methods added
Connection terminated support added

Revision 1.13  1998/11/19 12:22:48  denis
SOCKS support cleaned a little
icq_RecvUrl renamed to icq_RecvURL
icq_ProxyAuth added for Username/Password Authentication
URL/Description order inverted
icq_Quit splitted to icq_Logout and icq_Disconnect
icq_ProxyName and icq_ProxyPass range checking added

Revision 1.12  1998/11/18 16:21:29  denis
Fixed SOCKS5 proxy support

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

int icq_Russian = FALSE;
BYTE icq_LogLevel = 0;

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
  ICQLINK *link = (ICQLINK *)malloc(sizeof(ICQLINK));
  link->d = (ICQLINK_private *)malloc(sizeof(ICQLINK_private));

  srand(time(0L));

/*   memset(link, 0, sizeof(ICQLINK)); */

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
  struct timeval tv;
  fd_set readfds;

  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&readfds);
  FD_SET(link->icq_UDPSok, &readfds);
  select(link->icq_UDPSok+1, &readfds, 0L, 0L, &tv);
  if(FD_ISSET(link->icq_UDPSok, &readfds))
    icq_HandleServerResponse(link);
  icq_TCPMain(link);
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

  link->icq_UDPSok = socket(AF_INET, SOCK_DGRAM, 0);/* create the unconnected socket*/
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
    link->icq_ProxySok = socket(AF_INET, SOCK_STREAM, 0);/* create the unconnected socket*/
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
#ifdef _WIN32
        closesocket(link->icq_ProxySok);
#else
        close(link->icq_ProxySok);
#endif
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
#ifdef _WIN32
        closesocket(link->icq_ProxySok);
#else
        close(link->icq_ProxySok);
#endif
        return -1;
      }
    }
    else
    {
      if(res != 2 || buf[0] != 5 || buf[1] != 0) /* no authentication required */
      {
        icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Authentication method incorrect\n");
#ifdef _WIN32
        closesocket(link->icq_ProxySok);
#else
        close(link->icq_ProxySok);
#endif
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
#ifdef _WIN32
      closesocket(link->icq_ProxySok);
#else
      close(link->icq_ProxySok);
#endif
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
#ifdef _WIN32
        closesocket(link->icq_ProxySok);
#else
        close(link->icq_ProxySok);
#endif
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
#ifdef _WIN32
      closesocket(link->icq_ProxySok);
#else
      close(link->icq_ProxySok);
#endif
    }
    return -1;
  }
  length = sizeof(sin) ;
  getsockname(link->icq_UDPSok, (struct sockaddr*)&sin, &length);
  link->icq_OurIP = ntohl(sin.sin_addr.s_addr);
  link->icq_OurPort = ntohs(sin.sin_port);
  return link->icq_UDPSok;
}

void icq_Disconnect(ICQLINK *link)
{
#ifdef _WIN32
  closesocket(link->icq_UDPSok);
#else
  close(link->icq_UDPSok);
#endif
  if(link->icq_UseProxy)
  {
#ifdef _WIN32
    closesocket(link->icq_ProxySok);
#else
    close(link->icq_ProxySok);
#endif
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

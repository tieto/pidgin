/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: proxy.c 1162 2000-11-28 02:22:42Z warmenhoven $
$Log$
Revision 1.1  2000/11/28 02:22:42  warmenhoven
icq. whoop de doo

Revision 1.8  2000/05/10 18:51:23  denis
icq_Disconnect() now called before icq_Disconnected callback to
prevent high CPU usage in kicq's "reconnect on disconnect" code.

Revision 1.7  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.6  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.5  1999/10/07 18:00:59  denis
proxy.h file removed.

Revision 1.4  1999/07/16 12:01:06  denis
ICQLINK support added.

Revision 1.3  1999/07/12 15:13:33  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.2  1999/04/14 14:51:42  denis
Switched from icq_Log callback to icq_Fmt function.
Cleanups for "strict" compiling (-ansi -pedantic)

Revision 1.1  1999/03/24 11:37:38  denis
Underscored files with TCP stuff renamed.
TCP stuff cleaned up
Function names changed to corresponding names.
icqlib.c splitted to many small files by subject.
C++ comments changed to ANSI C comments.

*/

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#endif

#include <stdlib.h>

#include "util.h"
#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"

void icq_HandleProxyResponse(ICQLINK *link)
{
  int s;
  char buf[256];
#ifdef _WIN32
  s = recv(link->icq_ProxySok, buf, sizeof(buf), 0);
#else
  s = read(link->icq_ProxySok, &buf, sizeof(buf));
#endif
  if(s<=0)
  {
    icq_FmtLog(link, ICQ_LOG_FATAL, "[SOCKS] Connection terminated\n");
    icq_Disconnect(link);
    if(link->icq_Disconnected)
      (*link->icq_Disconnected)(link);
  }
}

/*******************
SOCKS5 Proxy support
********************/
void icq_SetProxy(ICQLINK *link, const char *phost, unsigned short pport, int pauth, const char *pname, const char *ppass)
{
  if(link->icq_ProxyHost)
    free(link->icq_ProxyHost);
  if(link->icq_ProxyName)
    free(link->icq_ProxyName);
  if(link->icq_ProxyPass)
    free(link->icq_ProxyPass);
  if(strlen(pname)>255)
  {
    icq_FmtLog(link, ICQ_LOG_ERROR, "[SOCKS] User name greater than 255 chars\n");
    link->icq_UseProxy = 0;
    return;
  }
  if(strlen(ppass)>255)
  {
    icq_FmtLog(link, ICQ_LOG_ERROR, "[SOCKS] User password greater than 255 chars\n");
    link->icq_UseProxy = 0;
    return;
  }
  link->icq_UseProxy = 1;
  link->icq_ProxyHost = strdup(phost);
  link->icq_ProxyPort = pport;
  link->icq_ProxyAuth = pauth;
  link->icq_ProxyName = strdup(pname);
  link->icq_ProxyPass = strdup(ppass);
}

void icq_UnsetProxy(ICQLINK *link)
{
  link->icq_UseProxy = 0;
}

int icq_GetProxySok(ICQLINK *link)
{
  return link->icq_ProxySok;
}

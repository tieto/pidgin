/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: proxy.c 1987 2001-06-09 14:46:51Z warmenhoven $
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

#ifdef _WIN32
#include <winsock.h>
#endif

#include <stdlib.h>

#include "icqlib.h"

void icq_HandleProxyResponse(icq_Link *icqlink)
{
  int s;
  char buf[256];
#ifdef _WIN32
  s = recv(icqlink->icq_ProxySok, buf, sizeof(buf), 0);
#else
  s = read(icqlink->icq_ProxySok, &buf, sizeof(buf));
#endif
  if(s<=0)
  {
    icq_FmtLog(icqlink, ICQ_LOG_FATAL, "[SOCKS] Connection terminated\n");
    icq_Disconnect(icqlink);
    invoke_callback(icqlink, icq_Disconnected)(icqlink);
  }
}

/*******************
SOCKS5 Proxy support
********************/
void icq_SetProxy(icq_Link *icqlink, const char *phost, unsigned short pport, 
  int pauth, const char *pname, const char *ppass)
{
  if(icqlink->icq_ProxyHost)
    free(icqlink->icq_ProxyHost);
  if(icqlink->icq_ProxyName)
    free(icqlink->icq_ProxyName);
  if(icqlink->icq_ProxyPass)
    free(icqlink->icq_ProxyPass);

  if(!phost)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "[SOCKS] Proxy host is empty\n");
    icqlink->icq_UseProxy = 0;
    return;
  }
  if(!pname)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "[SOCKS] User name is empty\n");
    icqlink->icq_UseProxy = 0;
    return;
  }
  if(!pname)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "[SOCKS] User password is empty\n");
    icqlink->icq_UseProxy = 0;
    return;
  }

  if(strlen(pname)>255)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "[SOCKS] User name greater than 255 chars\n");
    icqlink->icq_UseProxy = 0;
    return;
  }
  if(strlen(ppass)>255)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "[SOCKS] User password greater than 255 chars\n");
    icqlink->icq_UseProxy = 0;
    return;
  }

  icqlink->icq_UseProxy = 1;
  icqlink->icq_ProxyHost = strdup(phost);
  icqlink->icq_ProxyPort = pport;
  icqlink->icq_ProxyAuth = pauth;
  icqlink->icq_ProxyName = strdup(pname);
  icqlink->icq_ProxyPass = strdup(ppass);
}

void icq_UnsetProxy(icq_Link *icqlink)
{
  icqlink->icq_UseProxy = 0;
}

int icq_GetProxySok(icq_Link *icqlink)
{
  return icqlink->icq_ProxySok;
}

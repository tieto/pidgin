/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: icqlib.h 2096 2001-07-31 01:00:39Z warmenhoven $
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

#ifndef _ICQLIB_H_
#define _ICQLIB_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* for strdup(), bzero() and snprintf() declarations */
#ifndef __USE_BSD
#define __USE_BSD 1
#define __need_bsd_undef 1
#endif
#include <string.h>
#include <stdio.h>
#ifdef __need_bsd_undef
#undef __USE_BSD
#endif

#include "icq.h"
#include "util.h"

BOOL icq_GetServMess(icq_Link *icqlink, WORD num);
void icq_SetServMess(icq_Link *icqlink, WORD num);
void icq_RusConv(const char to[4], char *t_in);
void icq_RusConv_n(const char to[4], char *t_in, int len);

#ifndef _WIN32
#ifndef inet_addr
extern unsigned long inet_addr(const char *cp);
#endif /* inet_addr */
#ifndef inet_aton
extern int inet_aton(const char *cp, struct in_addr *inp);
#endif /* inet_aton */
#ifndef inet_ntoa
extern char *inet_ntoa(struct in_addr in);
#endif /* inet_ntoa */
#ifndef strdup
extern char *strdup(const char *s);
#endif /* strdup */
#endif /* _WIN32 */

/** Private ICQLINK data.  These are members that are internal to
 * icqlib, but must be contained in the per-connection ICQLINK
 * struct.
 */
struct icq_LinkPrivate_s
{
  icq_List *icq_ContactList;

  /* 65536 seqs max, 1 bit per seq -> 65536/8 = 8192 */
  unsigned char icq_UDPServMess[8192]; 

  unsigned short icq_UDPSeqNum1, icq_UDPSeqNum2;
  unsigned long icq_UDPSession;
  
  icq_List *icq_UDPQueue;

  int icq_TCPSequence;
  icq_List *icq_TCPLinks;
  icq_List *icq_ChatSessions;
  icq_List *icq_FileSessions;
};

#define invoke_callback(plink, callback) \
   if (plink->callback) (*(plink->callback))

#endif /* _ICQLIB_H_ */

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: contacts.h 1987 2001-06-09 14:46:51Z warmenhoven $
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

#ifndef _CONTACTS_H
#define _CONTACTS_H

#include "icq.h"
#include "icqtypes.h"

typedef struct icq_ContactItem_s
{
  icq_Link *icqlink;
  unsigned long uin;
  int vis_list;
  int invis_list;
  unsigned long remote_ip;
  unsigned long remote_real_ip;
  unsigned long remote_port;
  unsigned char tcp_flag;
} icq_ContactItem;  

icq_ContactItem *icq_ContactNew(icq_Link *icqlink);
void icq_ContactDelete(void *pcontact);
icq_ContactItem *icq_ContactFind(icq_Link *icqlink, DWORD cuin);
icq_ContactItem *icq_ContactGetFirst(icq_Link *icqlink);
icq_ContactItem *icq_ContactGetNext(icq_ContactItem *pcontact);

#endif /* _CONTACTS_H */

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

#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icq.h"
#include "icqpacket.h"
#include "timeout.h"

typedef struct icq_UDPQueueItem_s
{
  unsigned char attempts;
  icq_Timeout *timeout;
  icq_Packet *pack;
  icq_Link *icqlink;
} icq_UDPQueueItem;

void icq_UDPQueueNew(icq_Link *);
void icq_UDPQueueFree(icq_Link *);
void icq_UDPQueuePut(icq_Link *, icq_Packet*);
void icq_UDPQueueDelete(icq_Link *);
void icq_UDPQueueFree(icq_Link *);
void icq_UDPQueueDelSeq(icq_Link *, WORD);
void icq_UDPQueueItemResend(icq_UDPQueueItem *pitem);

#endif

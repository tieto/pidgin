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

#include <stdlib.h>

#include "icqlib.h"
#include "queue.h"

void icq_UDPQueueNew(icq_Link *icqlink)
{
  icqlink->d->icq_UDPQueue = icq_ListNew();
  icqlink->icq_UDPExpireInterval = 15; /* expire interval = 15 sec */
}

void icq_UDPQueuePut(icq_Link *icqlink, icq_Packet *p)
{
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)malloc(sizeof(icq_UDPQueueItem));
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueuePut(seq=0x%04X, cmd=0x%04X)\n", icq_PacketReadUDPOutSeq1(p),
         icq_PacketReadUDPOutCmd(p));
#endif
  ptr->attempts = 1;
  ptr->timeout = icq_TimeoutNew(icqlink->icq_UDPExpireInterval, 
    (icq_TimeoutHandler)icq_UDPQueueItemResend, ptr);
  ptr->timeout->single_shot = 0;
  ptr->pack = p;
  ptr->icqlink = icqlink;
#ifdef QUEUE_DEBUG
  printf("enqueuing queueitem %p\n", ptr);
#endif
  icq_ListEnqueue(icqlink->d->icq_UDPQueue, ptr);
}

void _icq_UDPQueueItemFree(void *p)
{
  icq_UDPQueueItem *pitem=(icq_UDPQueueItem *)p;

#ifdef QUEUE_DEBUG
  printf("_icq_UDPQueueItemFree(%p)\n", p);
#endif

  if (pitem->pack)
    icq_PacketDelete(pitem->pack);

  if (pitem->timeout)
    icq_TimeoutDelete(pitem->timeout);

  free(p);
}

/* Frees the queue and dispose it */
void icq_UDPQueueDelete(icq_Link *icqlink)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelete\n");
#endif
  if(icqlink->d->icq_UDPQueue)
  {
    icq_ListDelete(icqlink->d->icq_UDPQueue, _icq_UDPQueueItemFree);
    icqlink->d->icq_UDPQueue = 0;
  }
}

/* Only frees the queue */
void icq_UDPQueueFree(icq_Link *icqlink)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueFree\n");
#endif
  if(icqlink->d->icq_UDPQueue)
    icq_ListFree(icqlink->d->icq_UDPQueue, _icq_UDPQueueItemFree);
}

int icq_UDPQueueFindSeq(void *p, va_list data)
{
  WORD seq=va_arg(data, int);
  return icq_PacketReadUDPOutSeq1(((icq_UDPQueueItem *)p)->pack) == seq;
}

void icq_UDPQueueDelSeq(icq_Link *icqlink, WORD seq)
{
  icq_UDPQueueItem *ptr;
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelSeq(seq=0x%04X", seq);
#endif
  ptr = icq_ListTraverse(icqlink->d->icq_UDPQueue, icq_UDPQueueFindSeq, seq);
  if(ptr)
  {
#ifdef QUEUE_DEBUG
    printf(", cmd=0x%04X",icq_PacketReadUDPOutCmd(ptr->pack));
#endif
    icq_ListRemove(icqlink->d->icq_UDPQueue, ptr);
    _icq_UDPQueueItemFree(ptr);
  }
#ifdef QUEUE_DEBUG
  printf(")\n");
#endif
}

void icq_UDPQueueItemResend(icq_UDPQueueItem *p)
{
  p->attempts++;
  if (p->attempts > 6)
  {
    icq_Disconnect(p->icqlink);
    invoke_callback(p->icqlink, icq_Disconnected)(p->icqlink);
    return;
  }

  icq_UDPSockWriteDirect(p->icqlink, p->pack);
}


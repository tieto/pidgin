/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: queue.c 1508 2001-02-22 23:07:34Z warmenhoven $
$Log$
Revision 1.5  2001/02/22 23:07:34  warmenhoven
updating icqlib

Revision 1.13  2001/02/22 05:40:04  bills
port tcp connect timeout code and UDP queue to new timeout manager

Revision 1.12  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.11  2000/12/06 05:15:45  denis
Handling for mass TCP messages has been added based on patch by
Konstantin Klyagin <konst@konst.org.ua>

Revision 1.10  2000/12/03 21:56:38  bills
fixed compilation with gcc-2.96

Revision 1.9  2000/07/10 01:31:17  bills
oops - removed #define LIST_TRACE and #define QUEUE_DEBUG

Revision 1.8  2000/07/10 01:26:56  bills
added more trace messages, reworked packet delete handling: now happens
during _icq_UDEQueueItemFree rather than during icq_UDPQueueDelSeq - fixes
memory leak

Revision 1.7  2000/07/09 22:07:37  bills
use new list_free

Revision 1.6  2000/06/25 16:30:05  denis
Some sanity checks were added to icq_UDPQueueDelete() and
icq_UDPQueueFree()

Revision 1.5  2000/05/10 19:06:59  denis
UDP outgoing packet queue was implemented.

Revision 1.4  2000/03/30 14:15:28  denis
Fixed FreeBSD warning about obsolete malloc.h header.

Revision 1.3  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.2  1999/09/29 17:06:47  denis
ICQLINK compatibility added.

Revision 1.1  1999/07/16 12:12:13  denis
Initial support for outgoing packet queue added.

*/

#include <stdlib.h>
#include <time.h>

#include "icqlib.h"
#include "queue.h"
#include "list.h"

void icq_UDPQueueNew(ICQLINK *link)
{
  link->d->icq_UDPQueue = list_new();
  link->icq_UDPExpireInterval = 15; /* expire interval = 15 sec */
}

void icq_UDPQueuePut(ICQLINK *link, icq_Packet *p)
{
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)malloc(sizeof(icq_UDPQueueItem));
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueuePut(seq=0x%04X, cmd=0x%04X)\n", icq_PacketReadUDPOutSeq1(p),
         icq_PacketReadUDPOutCmd(p));
#endif
  ptr->attempts = 1;
  ptr->timeout = icq_TimeoutNew(link->icq_UDPExpireInterval, 
    (icq_TimeoutHandler)icq_UDPQueueItemResend, ptr);
  ptr->timeout->single_shot = 0;
  ptr->pack = p;
  ptr->icqlink = link;
#ifdef QUEUE_DEBUG
  printf("enqueuing queueitem %p\n", ptr);
#endif
  list_enqueue(link->d->icq_UDPQueue, ptr);
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
void icq_UDPQueueDelete(ICQLINK *link)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelete\n");
#endif
  if(link->d->icq_UDPQueue)
  {
    list_delete(link->d->icq_UDPQueue, _icq_UDPQueueItemFree);
    link->d->icq_UDPQueue = 0;
  }
}

/* Only frees the queue */
void icq_UDPQueueFree(ICQLINK *link)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueFree\n");
#endif
  if(link->d->icq_UDPQueue)
    list_free(link->d->icq_UDPQueue, _icq_UDPQueueItemFree);
}

int icq_UDPQueueFindSeq(void *p, va_list data)
{
  WORD seq=va_arg(data, int);
  return icq_PacketReadUDPOutSeq1(((icq_UDPQueueItem *)p)->pack) == seq;
}

void icq_UDPQueueDelSeq(ICQLINK *link, WORD seq)
{
  icq_UDPQueueItem *ptr;
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelSeq(seq=0x%04X", seq);
#endif
  ptr = list_traverse(link->d->icq_UDPQueue, icq_UDPQueueFindSeq, seq);
  if(ptr)
  {
#ifdef QUEUE_DEBUG
    printf(", cmd=0x%04X",icq_PacketReadUDPOutCmd(ptr->pack));
#endif
    list_remove(link->d->icq_UDPQueue, ptr);
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


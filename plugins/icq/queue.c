/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: queue.c 1162 2000-11-28 02:22:42Z warmenhoven $
$Log$
Revision 1.1  2000/11/28 02:22:42  warmenhoven
icq. whoop de doo

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

#include "queue.h"
#include "list.h"

void icq_UDPQueueNew(ICQLINK *link)
{
  link->icq_UDPQueue = list_new();
  link->icq_UDPExpireInterval = 15; /* expire interval = 15 sec */
}

void icq_UDPQueuePut(ICQLINK *link, icq_Packet *p, int attempt)
{
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)malloc(sizeof(icq_UDPQueueItem));
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueuePut(seq=0x%04X, cmd=0x%04X)\n", icq_PacketReadUDPOutSeq1(p),
         icq_PacketReadUDPOutCmd(p));
#endif
  ptr->attempts = attempt;
  ptr->expire = time(0L)+link->icq_UDPExpireInterval;
  ptr->pack = p;
#ifdef QUEUE_DEBUG
  printf("enqueuing queueitem %p\n", ptr);
#endif
  list_enqueue(link->icq_UDPQueue, ptr);
}

icq_Packet *icq_UDPQueueGet(ICQLINK *link)
{
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)list_first(link->icq_UDPQueue);
  icq_Packet *pack = 0L;
  if(ptr)
  {
    pack = ptr->pack;
    list_remove(link->icq_UDPQueue, (list_node*)ptr);
  }
#ifdef QUEUE_DEBUG
  if(pack)
    printf("icq_UDPQueueGet(cmd=0x%04X)\n", icq_PacketReadUDPOutCmd(pack));
#endif
  return pack;
}

icq_Packet *icq_UDPQueuePeek(ICQLINK *link)
{
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)list_first(link->icq_UDPQueue);
  if(ptr)
    return ptr->pack;
  else
    return 0L;
}

void _icq_UDPQueueItemFree(void *p)
{
  icq_UDPQueueItem *pitem=(icq_UDPQueueItem *)p;

#ifdef QUEUE_DEBUG
  printf("_icq_UDPQueueItemFree(%p)\n", p);
#endif

  if (pitem->pack)
    icq_PacketDelete(pitem->pack);

  free(p);
}

/* Frees the queue and dispose it */
void icq_UDPQueueDelete(ICQLINK *link)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelete\n");
#endif
  if(link->icq_UDPQueue)
  {
    list_delete(link->icq_UDPQueue, _icq_UDPQueueItemFree);
    link->icq_UDPQueue = 0;
  }
}

/* Only frees the queue */
void icq_UDPQueueFree(ICQLINK *link)
{
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueFree\n");
#endif
  if(link->icq_UDPQueue)
    list_free(link->icq_UDPQueue, _icq_UDPQueueItemFree);
}

int icq_UDPQueueFindSeq(void *p, va_list data)
{
  WORD seq=va_arg(data, WORD);
  return icq_PacketReadUDPOutSeq1(((icq_UDPQueueItem *)p)->pack) == seq;
}

void icq_UDPQueueDelSeq(ICQLINK *link, WORD seq)
{
  icq_UDPQueueItem *ptr;
#ifdef QUEUE_DEBUG
  printf("icq_UDPQueueDelSeq(seq=0x%04X", seq);
#endif
  ptr = list_traverse(link->icq_UDPQueue, icq_UDPQueueFindSeq, seq);
  if(ptr)
  {
#ifdef QUEUE_DEBUG
    printf(", cmd=0x%04X",icq_PacketReadUDPOutCmd(ptr->pack));
#endif
    list_remove(link->icq_UDPQueue, ptr);
    _icq_UDPQueueItemFree(ptr);
  }
#ifdef QUEUE_DEBUG
  printf(")\n");
#endif
}

long icq_UDPQueueInterval(ICQLINK *link)
{
  long interval;
  icq_UDPQueueItem *ptr = (icq_UDPQueueItem*)list_first(link->icq_UDPQueue);
  if(ptr)
  {
    interval = ptr->expire - time(0L);
    return interval>=0?interval:0;
  }
  return -1;
}

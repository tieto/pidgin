/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icq.h"
#include "icqpacket.h"
#include "timeout.h"

typedef struct udp_item
{
  unsigned char attempts;
  icq_Timeout *timeout;
  icq_Packet *pack;
  ICQLINK *icqlink;
} icq_UDPQueueItem;

void icq_UDPQueueNew(ICQLINK*);
void icq_UDPQueueFree(ICQLINK*);
void icq_UDPQueuePut(ICQLINK*, icq_Packet*);
void icq_UDPQueueDelete(ICQLINK*);
void icq_UDPQueueFree(ICQLINK*);
void icq_UDPQueueDelSeq(ICQLINK*, WORD);
void icq_UDPQueueItemResend(icq_UDPQueueItem *pitem);

#endif

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _QUEUE_H_
#define _QUEUE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icq.h"
#include "icqpacket.h"

typedef struct udp_item
{
  unsigned char attempts;
  unsigned long expire;
  icq_Packet *pack;
} icq_UDPQueueItem;

void icq_UDPQueueNew(ICQLINK*);
void icq_UDPQueueFree(ICQLINK*);
void icq_UDPQueuePut(ICQLINK*, icq_Packet*, int);
icq_Packet *icq_UDPQueueGet(ICQLINK*);
icq_Packet *icq_UDPQueuePeek(ICQLINK*);
void icq_UDPQueueDelete(ICQLINK*);
void icq_UDPQueueFree(ICQLINK*);
void icq_UDPQueueDelSeq(ICQLINK*, WORD);
long icq_UDPQueueInterval(ICQLINK *);

#endif

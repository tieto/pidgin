/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef _ICQTCP_H
#define _ICQTCP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icq.h"
#include "tcplink.h"
#include "util.h"

/* initialize icq_TCPSocket and icq_TCPSocketAddress
 * returns < 0 on failure */
int icq_TCPInit(ICQLINK *link);

/* close icq_TCPSocket, internal cleanup */
void icq_TCPDone(ICQLINK *link);

int icq_TCPProcessHello(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessPacket(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessChatPacket(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessFilePacket(icq_Packet *p, icq_TCPLink *plink);

/* Debugging */
/* trace packet process results */
/* #define TCP_PROCESS_TRACE */

/* trace sent and received icq packets */
/* #define TCP_PACKET_TRACE     */

/* trace raw sent and received packet data */
/* #define TCP_RAW_TRACE  */

/* trace recv buffer in tcplink.c*/
#undef TCP_BUFFER_TRACE

/* trace queueing operations in list.c */
#undef TCP_QUEUE_TRACE

#endif /* _ICQTCP_H */

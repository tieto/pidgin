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

#ifndef _ICQTCP_H
#define _ICQTCP_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tcplink.h"

/* initialize icq_TCPSocket and icq_TCPSocketAddress
 * returns < 0 on failure */
int icq_TCPInit(icq_Link *icqlink);

/* close icq_TCPSocket, internal cleanup */
void icq_TCPDone(icq_Link *icqlink);

int icq_TCPProcessHello(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessPacket(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessChatPacket(icq_Packet *p, icq_TCPLink *plink);
void icq_TCPProcessFilePacket(icq_Packet *p, icq_TCPLink *plink);

/* Debugging */
/* trace packet process results */
#undef TCP_PROCESS_TRACE 

/* trace sent and received icq packets */
#undef TCP_PACKET_TRACE    

/* trace raw sent and received packet data */
#undef TCP_RAW_TRACE 

/* trace recv buffer in tcplink.c*/
#undef TCP_BUFFER_TRACE

/* trace queueing operations in list.c */
#undef TCP_QUEUE_TRACE

#endif /* _ICQTCP_H */

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: stdpackets.h 2096 2001-07-31 01:00:39Z warmenhoven $
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

#ifndef _ICQTCPPACKETS_H
#define _ICQTCPPACKETS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define ICQ_UDP_VER            0x0005
#define ICQ_TCP_VER            0x0003

/* TCP Packet Commands */
#define ICQ_TCP_HELLO            0xFF
#define ICQ_TCP_CANCEL           0x07D0
#define ICQ_TCP_ACK              0x07DA
#define ICQ_TCP_MESSAGE          0x07EE

/* TCP Message Types */
#define ICQ_TCP_MSG_MSG          0x0001
#define ICQ_TCP_MSG_CHAT         0x0002
#define ICQ_TCP_MSG_FILE         0x0003
#define ICQ_TCP_MSG_URL          0x0004
#define ICQ_TCP_MSG_CONTACTLIST  0x0013
#define ICQ_TCP_MSG_READAWAY     0x03E8
#define ICQ_TCP_MSG_READOCCUPIED 0x03E9
#define ICQ_TCP_MSG_READNA       0x03EA
#define ICQ_TCP_MSG_READDND      0x03EB
#define ICQ_TCP_MSG_READFFC      0x03EC
#define ICQ_TCP_MASS_MASK        0x8000

/* TCP Message Command Types */
#define ICQ_TCP_MSG_ACK          0x0000
#define ICQ_TCP_MSG_AUTO         0x0000
#define ICQ_TCP_MSG_REAL         0x0010
#define ICQ_TCP_MSG_LIST         0x0020
#define ICQ_TCP_MSG_URGENT       0x0040
#define ICQ_TCP_MSG_INVISIBLE    0x0090
#define ICQ_TCP_MSG_UNK_1        0x00A0
#define ICQ_TCP_MSG_AWAY         0x0110
#define ICQ_TCP_MSG_OCCUPIED     0x0210
#define ICQ_TCP_MSG_UNK_2        0x0802
#define ICQ_TCP_MSG_NA           0x0810
#define ICQ_TCP_MSG_NA_2         0x0820
#define ICQ_TCP_MSG_DND          0x1010

/* TCP Message Statuses */
#define ICQ_TCP_STATUS_ONLINE      0x0000
#define ICQ_TCP_STATUS_REFUSE      0x0001
#define ICQ_TCP_STATUS_AWAY        0x0004
#define ICQ_TCP_STATUS_OCCUPIED    0x0009
#define ICQ_TCP_STATUS_DND         0x000A
#define ICQ_TCP_STATUS_NA          0x000E
#define ICQ_TCP_STATUS_FREE_CHAT   ICQ_TCP_STATUS_ONLINE
#define ICQ_TCP_STATUS_INVISIBLE   ICQ_TCP_STATUS_ONLINE

#include "tcplink.h"

icq_Packet *icq_TCPCreateInitPacket(icq_TCPLink *plink);
icq_Packet *icq_TCPCreateStdPacket(icq_TCPLink *plink, WORD icq_TCPCommand,
               WORD type, const char *msg, WORD status, WORD msg_command);
icq_Packet *icq_TCPCreateMessagePacket(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateURLPacket(icq_TCPLink *plink, const char *message,
   const char *url);
icq_Packet *icq_TCPCreateAwayReqPacket(icq_TCPLink *plink, WORD statusMode);
icq_Packet *icq_TCPCreateChatReqPacket(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateFileReqPacket(icq_TCPLink *plink, 
   const char *message, const char *filename, DWORD size);
void icq_TCPAppendSequence(icq_Link *icqlink, icq_Packet *p);
void icq_TCPAppendSequenceN(icq_Link *icqlink, icq_Packet *p, DWORD seq);

icq_Packet *icq_TCPCreateMessageAck(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateURLAck(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateContactListAck(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateWebPagerAck(icq_TCPLink *plink, const char *message);
icq_Packet *icq_TCPCreateChatReqAck(icq_TCPLink *plink, WORD port);
icq_Packet *icq_TCPCreateChatReqCancel(icq_TCPLink *plink, WORD port);
icq_Packet *icq_TCPCreateChatReqRefuse(icq_TCPLink *plink, WORD port,
   const char *reason);
icq_Packet *icq_TCPCreateFileReqAck(icq_TCPLink *plink, WORD port);
icq_Packet *icq_TCPCreateFileReqCancel(icq_TCPLink *plink, WORD port);
icq_Packet *icq_TCPCreateFileReqRefuse(icq_TCPLink *plink, WORD port,
   const char *reason);

icq_Packet *icq_TCPCreateChatInfoPacket(icq_TCPLink *plink, const char *name, 
   DWORD foreground, DWORD background);
icq_Packet *icq_TCPCreateChatInfo2Packet(icq_TCPLink *plink, const char *name,
   DWORD foreground, DWORD background);
icq_Packet *icq_TCPCreateChatFontInfoPacket(icq_TCPLink *plink);

icq_Packet *icq_TCPCreateFile00Packet(DWORD num_files, DWORD total_bytes, DWORD speed, const char *name);
icq_Packet *icq_TCPCreateFile01Packet(DWORD speed, const char *name);
icq_Packet *icq_TCPCreateFile02Packet(const char *filename, DWORD filesize, DWORD speed);
icq_Packet *icq_TCPCreateFile03Packet(DWORD filesize, DWORD speed);
icq_Packet *icq_TCPCreateFile04Packet(DWORD filenum);
icq_Packet *icq_TCPCreateFile05Packet(DWORD speed);
icq_Packet *icq_TCPCreateFile06Packet(int length, void *data);

icq_Packet *icq_UDPCreateStdPacket(icq_Link *icqlink, WORD cmd);
icq_Packet *icq_UDPCreateStdSeqPacket(icq_Link *icqlink, WORD cmd, WORD seq);

#endif /* _ICQTCPPACKETS_H */

/* From `tcppackets.c': */


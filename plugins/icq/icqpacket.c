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

/*
 * ICQ packet abstraction
 */

#include <stdlib.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif

#include "icqlib.h"
#include "tcp.h"
#include "icqbyteorder.h"

icq_Packet *icq_PacketNew()
{
  icq_Packet *p=(icq_Packet*)malloc(sizeof(icq_Packet));

  p->length=0;
  p->cursor=0;
  p->id=0;

  return p;
}

void icq_PacketDelete(void *p)
{
  free(p);
}

void icq_PacketAppend32(icq_Packet *p, DWORD i)
{
  DWORD val=i;

  *(unsigned long*)((p->data)+(p->cursor))=htoicql(val);
  icq_PacketAdvance(p, sizeof(DWORD));
}

void icq_PacketAppend32n(icq_Packet *p, DWORD i)
{
  DWORD val=i;

  *(DWORD *)((p->data)+(p->cursor)) = val;
  icq_PacketAdvance(p, sizeof(DWORD));
}

DWORD icq_PacketRead32(icq_Packet *p)
{
  DWORD val;

  val = icqtohl(*(DWORD *)((p->data)+(p->cursor)));
  icq_PacketAdvance(p, sizeof(DWORD));

  return val;
}

DWORD icq_PacketRead32n(icq_Packet *p)
{
  DWORD val;

  val = *(DWORD*)((p->data)+(p->cursor));
  icq_PacketAdvance(p, sizeof(DWORD));

  return val;
}

void icq_PacketAppend16(icq_Packet *p, WORD i)
{
  WORD val=i;

  *(WORD *)((p->data)+(p->cursor)) = htoicqs(val);
  icq_PacketAdvance(p, sizeof(WORD));
}

void icq_PacketAppend16n(icq_Packet *p, WORD i)
{
  WORD val=i;

  *(WORD *)((p->data)+(p->cursor)) = val;
  icq_PacketAdvance(p, sizeof(WORD));
}

WORD icq_PacketRead16(icq_Packet *p)
{
  WORD val;

  val = icqtohs(*(WORD *)((p->data)+(p->cursor)));
  icq_PacketAdvance(p, sizeof(WORD));

  return val;
}

WORD icq_PacketRead16n(icq_Packet *p)
{
  WORD val;

  val = *(WORD*)((p->data)+(p->cursor));
  icq_PacketAdvance(p, sizeof(WORD));

  return val;
}

void icq_PacketAppend8(icq_Packet *p, BYTE i)
{
  BYTE val=i;

  memcpy((p->data)+(p->cursor), &val, sizeof(BYTE));
  icq_PacketAdvance(p, sizeof(BYTE));
}

BYTE icq_PacketRead8(icq_Packet *p)
{
  BYTE val;

  memcpy(&val, (p->data)+(p->cursor), sizeof(BYTE));
  icq_PacketAdvance(p, sizeof(BYTE));

  return val;
}

void icq_PacketAppendString(icq_Packet *p, const char *s)
{
  if(s)
  {
    int length=strlen(s)+1;

    icq_PacketAppend16(p, length);
    icq_PacketAppend(p, s, length);
  }
  else
  {
    icq_PacketAppend16(p, 1);
    icq_PacketAppend8(p,0);
  }
}

const char *icq_PacketReadString(icq_Packet *p)
{
  int length=icq_PacketRead16(p);

  return (const char *)icq_PacketRead(p, length);
}

char *icq_PacketReadStringNew(icq_Packet *p)
{
  char *ptr;
  int length=icq_PacketRead16(p);

  ptr = (char*)malloc(length);
  if(!ptr)
    return 0L;
  strncpy(ptr, icq_PacketRead(p, length), length);
  return ptr;
}

void icq_PacketAppendStringFE(icq_Packet *p, const char *s)
{
  if(s)
  {
    int length=strlen(s);
    icq_PacketAppend(p, s, length);
  }
  icq_PacketAppend8(p, 0xFE);
}

void icq_PacketAppendString0(icq_Packet *p, const char *s)
{
  if(s)
  {
    int length=strlen(s);
    icq_PacketAppend(p, s, length);
  }
  icq_PacketAppend8(p, 0);
}

WORD icq_PacketReadUDPOutVer(icq_Packet *p)
{
  icq_PacketGoto(p, 0);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPOutCmd(icq_Packet *p)
{
  icq_PacketGoto(p, 14 /*2*/);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPOutSeq1(icq_Packet *p)
{
  icq_PacketGoto(p, 16);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPOutSeq2(icq_Packet *p)
{
  icq_PacketGoto(p, 18 /*4*/);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPInVer(icq_Packet *p)
{
  icq_PacketGoto(p, 0);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPInCmd(icq_Packet *p)
{
  icq_PacketGoto(p, 7);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPInSeq1(icq_Packet *p)
{
  icq_PacketGoto(p, 9);
  return icq_PacketRead16(p);
}

WORD icq_PacketReadUDPInSeq2(icq_Packet *p)
{
  icq_PacketGoto(p, 11);
  return icq_PacketRead16(p);
}

DWORD icq_PacketReadUDPInUIN(icq_Packet *p)
{
  icq_PacketGoto(p, 13);
  return icq_PacketRead32(p);
}

void icq_PacketAppend(icq_Packet *p, const void *data, int length)
{
  memcpy((p->data)+(p->cursor), data, length);
  icq_PacketAdvance(p, length);
}

const void *icq_PacketRead(icq_Packet *p, int length)
{
  const void *data=(p->data)+(p->cursor);

  icq_PacketAdvance(p, length);

  return data;
}

void icq_PacketShortDump(icq_Packet *p)
{
  printf("icq_Packet %x { id=%d, cursor=%x, length=%d }\n", 
         (int)p, (int)p->id, p->cursor, p->length);
}

void icq_PacketDump(icq_Packet *p)
{
  icq_PacketShortDump(p);
  hex_dump((char*)&(p->length), p->length+sizeof(WORD));
}

void icq_PacketUDPDump(icq_Packet *p)
{
  icq_PacketShortDump(p);
  hex_dump((char*)&(p->data), p->length);
}

void icq_PacketBegin(icq_Packet *p)
{
  p->cursor=0;
}

void icq_PacketEnd(icq_Packet *p)
{
  p->cursor=p->length;
}

void icq_PacketAdvance(icq_Packet *p, int i)
{
  p->cursor+=i;

  if(p->cursor > p->length)
    p->length=p->cursor;

/* Do nothing, because we don't have ICQLINK here */
/*   if(p->cursor > ICQ_PACKET_DATA_SIZE) */
/*     icq_FmtLog(0L, ICQ_LOG_WARNING, "icq_Packet data overflow\n"); */
}

void icq_PacketGoto(icq_Packet *p, int i)
{
  icq_PacketBegin(p);
  icq_PacketAdvance(p, i);
}

void icq_PacketGotoUDPOutData(icq_Packet *p, int i)
{
  /* Go to data in UDP _client_ packet. */
  icq_PacketGoto(p, 24+i);
}

void icq_PacketGotoUDPInData(icq_Packet *p, int i)
{
  /* Go to data in UDP _server_ packet. */
  icq_PacketGoto(p, 21+i);
}

WORD icq_PacketPos(icq_Packet *p)
{
  return p->cursor;
}

int icq_PacketSend(icq_Packet *p, int socket_fd)
{
  int result;

  result=send(socket_fd, (const char*)&(p->length), p->length+sizeof(WORD), 0);

#ifdef TCP_RAW_TRACE
  printf("%d bytes sent on socket %d, result %d\n", 
    p->length+1, socket, result);
  icq_PacketDump(p);
#endif /* TCP_RAW_TRACE */

  return result;
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: icqpacket.c 1162 2000-11-28 02:22:42Z warmenhoven $
$Log$
Revision 1.1  2000/11/28 02:22:42  warmenhoven
icq. whoop de doo

Revision 1.10  2000/07/07 15:26:35  denis
"icq_Packet data overflow" log message temporarily commented out.

Revision 1.9  2000/06/25 16:26:36  denis
icq_PacketUDPDump() tweaked a little.

Revision 1.8  2000/05/03 18:20:40  denis
icq_PacketReadUDPInUIN() was added.

Revision 1.7  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.6  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.5  1999/09/29 19:58:20  bills
cleanup

Revision 1.4  1999/09/29 16:56:10  denis
Cleanups.

Revision 1.3  1999/07/18 20:18:43  bills
changed to use new byte-order functions

Revision 1.2  1999/07/16 15:45:05  denis
Cleaned up.

Revision 1.1  1999/07/16 12:11:36  denis
UDP packet support added.
tcp_packet* functions renamed to icq_Packet*

Revision 1.8  1999/07/12 15:13:38  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.7  1999/07/03 06:33:52  lord
. byte order conversion macros added
. some compilation warnings removed

Revision 1.6  1999/04/17 19:36:50  bills
added tcp_packet_node_delete function and changed structure to include
list_node for new list routines.

Revision 1.5  1999/04/14 15:08:04  denis
Cleanups for "strict" compiling (-ansi -pedantic)

*/

/*
 * ICQ packet abstraction
 */

#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"
#include "icqpacket.h"
#include "tcp.h"
#include "util.h"
#include "icqbyteorder.h"
#include "list.h"

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

int icq_PacketSend(icq_Packet *p, int socket)
{
  int result;

  result=send(socket, (const char*)&(p->length), p->length+sizeof(WORD), 0);

#ifdef TCP_RAW_TRACE
  printf("%d bytes sent on socket %d, result %d\n", 
    p->length+1, socket, result);
  icq_PacketDump(p);
#endif /* TCP_RAW_TRACE */

  return result;
}

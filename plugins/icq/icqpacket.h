/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _ICQ_PACKET_H_
#define _ICQ_PACKET_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icqtypes.h"

#define ICQ_PACKET_DATA_SIZE   4096

typedef struct icq_Packet_s
{
  DWORD id;
  WORD cursor;
  WORD length;
  BYTE data[ICQ_PACKET_DATA_SIZE];
} icq_Packet;

icq_Packet *icq_PacketNew();
void icq_PacketDelete(void *);

void icq_PacketAppend(icq_Packet *, const void *, int);
void icq_PacketAppend32(icq_Packet *, DWORD);
void icq_PacketAppend32n(icq_Packet *, DWORD);
void icq_PacketAppend16(icq_Packet *, WORD);
void icq_PacketAppend16n(icq_Packet *, WORD);
void icq_PacketAppend8(icq_Packet *, BYTE);
void icq_PacketAppendString(icq_Packet *, const char *);
void icq_PacketAppendStringFE(icq_Packet *, const char *);
void icq_PacketAppendString0(icq_Packet *, const char *);

const void *icq_PacketRead(icq_Packet*, int);
DWORD icq_PacketRead32(icq_Packet*);
DWORD icq_PacketRead32n(icq_Packet*);
WORD icq_PacketRead16(icq_Packet*);
WORD icq_PacketRead16n(icq_Packet*);
BYTE icq_PacketRead8(icq_Packet*);
const char *icq_PacketReadString(icq_Packet*);
char *icq_PacketReadStringNew(icq_Packet*);
WORD icq_PacketReadUDPOutVer(icq_Packet*);
WORD icq_PacketReadUDPOutCmd(icq_Packet*);
WORD icq_PacketReadUDPOutSeq1(icq_Packet*);
WORD icq_PacketReadUDPOutSeq2(icq_Packet*);
WORD icq_PacketReadUDPInVer(icq_Packet*);
WORD icq_PacketReadUDPInCmd(icq_Packet*);
WORD icq_PacketReadUDPInSeq1(icq_Packet*);
WORD icq_PacketReadUDPInSeq2(icq_Packet*);
DWORD icq_PacketReadUDPInUIN(icq_Packet*);

void icq_PacketDump(icq_Packet*);
void icq_PacketUDPDump(icq_Packet*);
void icq_PacketBegin(icq_Packet*);
void icq_PacketEnd(icq_Packet*);
void icq_PacketAdvance(icq_Packet*, int);
void icq_PacketGoto(icq_Packet*, int);
void icq_PacketGotoUDPOutData(icq_Packet*, int);
void icq_PacketGotoUDPInData(icq_Packet*, int);
WORD icq_PacketPos(icq_Packet*);
int icq_PacketSend(icq_Packet*, int);

#endif /* _ICQ_PACKET_H_ */

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

#ifdef _WIN32
#include <winsock.h>
#endif

#include <stdlib.h>

#include "icqlib.h"
#include "udp.h"
#include "queue.h"

#include "stdpackets.h"
#include "icqbyteorder.h"

#include "contacts.h"

static const BYTE icq_UDPTable[] = {
  0x59, 0x60, 0x37, 0x6B, 0x65, 0x62, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x60, 0x57, 0x5B, 0x3D,
  0x5E, 0x34, 0x6D, 0x36, 0x50, 0x3F, 0x6F, 0x67, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x47, 0x63, 0x39,
  0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x43, 0x69, 0x48, 0x33, 0x31, 0x64, 0x35, 0x5A, 0x4A, 0x42,
  0x56, 0x40, 0x67, 0x53, 0x41, 0x07, 0x6C, 0x49, 0x58, 0x3B, 0x4D, 0x46, 0x68, 0x43, 0x69, 0x48,
  0x33, 0x31, 0x44, 0x65, 0x62, 0x46, 0x48, 0x53, 0x41, 0x07, 0x6C, 0x69, 0x48, 0x33, 0x51, 0x54,
  0x5D, 0x4E, 0x6C, 0x49, 0x38, 0x4B, 0x55, 0x4A, 0x62, 0x46, 0x48, 0x33, 0x51, 0x34, 0x6D, 0x36,
  0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F, 0x47, 0x63, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64, 0x35, 0x5A,
  0x6A, 0x52, 0x6E, 0x3C, 0x51, 0x34, 0x6D, 0x36, 0x50, 0x5F, 0x5F, 0x3F, 0x4F, 0x37, 0x4B, 0x35,
  0x5A, 0x4A, 0x62, 0x66, 0x58, 0x3B, 0x4D, 0x66, 0x58, 0x5B, 0x5D, 0x4E, 0x6C, 0x49, 0x58, 0x3B,
  0x4D, 0x66, 0x58, 0x3B, 0x4D, 0x46, 0x48, 0x53, 0x61, 0x4C, 0x59, 0x40, 0x67, 0x33, 0x31, 0x64,
  0x55, 0x6A, 0x32, 0x3E, 0x44, 0x45, 0x52, 0x6E, 0x3C, 0x31, 0x64, 0x55, 0x6A, 0x52, 0x4E, 0x6C,
  0x69, 0x48, 0x53, 0x61, 0x4C, 0x39, 0x30, 0x6F, 0x47, 0x63, 0x59, 0x60, 0x57, 0x5B, 0x3D, 0x3E,
  0x64, 0x35, 0x3A, 0x3A, 0x5A, 0x6A, 0x52, 0x4E, 0x6C, 0x69, 0x48, 0x53, 0x61, 0x6C, 0x49, 0x58,
  0x3B, 0x4D, 0x46, 0x68, 0x63, 0x39, 0x50, 0x5F, 0x5F, 0x3F, 0x6F, 0x67, 0x53, 0x41, 0x25, 0x41,
  0x3C, 0x51, 0x54, 0x3D, 0x5E, 0x54, 0x5D, 0x4E, 0x4C, 0x39, 0x50, 0x5F, 0x5F, 0x5F, 0x3F, 0x6F,
  0x47, 0x43, 0x69, 0x48, 0x33, 0x51, 0x54, 0x5D, 0x6E, 0x3C, 0x31, 0x64, 0x35, 0x5A, 0x00, 0x00,
};

DWORD icq_UDPCalculateCheckCode(icq_Packet *p)
{
  DWORD num1, num2;
  DWORD r1,r2;

  num1 = p->data[8];
  num1 <<= 8;
  num1 += p->data[4];
  num1 <<= 8;
  num1 += p->data[2];
  num1 <<= 8;
  num1 += p->data[6];

  r1 = 0x18 + (rand() % (p->length - 0x18));
  r2 = rand() & 0xff;

  num2 = r1;
  num2 <<= 8;
  num2 += p->data[r1];
  num2 <<= 8;
  num2 += r2;   
  num2 <<= 8;
  num2 += icq_UDPTable[r2];
  num2 ^= 0xFF00FF;

  return num1 ^ num2;
}

DWORD icq_UDPScramble(DWORD cc)
{
  DWORD a[5];

  a[0] = cc & 0x0000001F;
  a[1] = cc & 0x03E003E0;
  a[2] = cc & 0xF8000400;
  a[3] = cc & 0x0000F800;
  a[4] = cc & 0x041F0000;

  a[0] <<= 0x0C;
  a[1] <<= 0x01;
  a[2] >>= 0x0A;
  a[3] <<= 0x10;
  a[4] >>= 0x0F;

  return a[0] + a[1] + a[2] + a[3] + a[4];
}

void icq_UDPEncode(icq_Packet *p, char *buffer)
{
  DWORD checkcode = icq_UDPCalculateCheckCode(p);
  DWORD code1, code2, code3;
  DWORD pos;

  memcpy(buffer, p->data, p->length);

  *(DWORD *)(buffer+0x14)=htoicql(checkcode);
  code1 = p->length * 0x68656c6cL;
  code2 = code1 + checkcode;
  pos = 0x0A;

  for(; pos < p->length; pos+=4)
  {
    DWORD data = icqtohl(*(DWORD *)((p->data)+pos));
    code3 = code2 + icq_UDPTable[pos & 0xFF];
    data ^= code3;
    *(DWORD*)(buffer+pos)=htoicql(data);
  }
  checkcode = icq_UDPScramble(checkcode);
  *(DWORD *)(buffer+0x14)=htoicql(checkcode);
}

/*********************************************************
icq_UDPSockWrite and icq_UDPSockRead are for _UDP_ packets
proxy support for TCP sockets is different!
*********************************************************/
int icq_UDPSockWriteDirect(icq_Link *icqlink, icq_Packet *p)
{
  char tmpbuf[ICQ_PACKET_DATA_SIZE+10];

  if(icqlink->icq_UDPSok <= 3)
  {
    icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad socket!\n");
    return -1;
  }

  icq_UDPEncode(p, tmpbuf+10);

  if(!icqlink->icq_UseProxy)
  {
#ifdef _WIN32
    return send(icqlink->icq_UDPSok, tmpbuf+10, p->length, 0);
#else
    return write(icqlink->icq_UDPSok, tmpbuf+10, p->length);
#endif
  }
  else
  {
    tmpbuf[0] = 0; /* reserved */
    tmpbuf[1] = 0; /* reserved */
    tmpbuf[2] = 0; /* standalone packet */
    tmpbuf[3] = 1; /* address type IP v4 */
    *(unsigned long*)&tmpbuf[4] = htonl(icqlink->icq_ProxyDestIP);
    *(unsigned short*)&tmpbuf[8] = htons(icqlink->icq_ProxyDestPort);
#ifdef _WIN32
    return send(icqlink->icq_UDPSok, tmpbuf, p->length+10, 0)-10;
#else
    return write(icqlink->icq_UDPSok, tmpbuf, p->length+10)-10;
#endif
  }
}

int icq_UDPSockWrite(icq_Link *icqlink, icq_Packet *p)
{
  icq_UDPQueuePut(icqlink, p);

  return icq_UDPSockWriteDirect(icqlink, p);
}

int icq_UDPSockRead(icq_Link *icqlink, icq_Packet *p)
{
  int res;
  char tmpbuf[ICQ_PACKET_DATA_SIZE];

  if(!icqlink->icq_UseProxy)
  {
#ifdef _WIN32
    res = recv(icqlink->icq_UDPSok, p->data, ICQ_PACKET_DATA_SIZE, 0);
#else
    res = read(icqlink->icq_UDPSok, p->data, ICQ_PACKET_DATA_SIZE);
#endif
    p->length = res;
    return res;
  }
  else
  {
#ifdef _WIN32
    res = recv(icqlink->icq_UDPSok, tmpbuf, ICQ_PACKET_DATA_SIZE, 0);
#else
    res = read(icqlink->icq_UDPSok, tmpbuf, ICQ_PACKET_DATA_SIZE);
#endif
    if(res<0)
      return res;
    memcpy(p->data, &tmpbuf[10], res-10);
    p->length = res-10;
    return res-10;
  }
}

/****************************************
This must be called every 2 min.
so the server knows we're still alive.
JAVA client sends two different commands
so we do also :)
*****************************************/
WORD icq_KeepAlive(icq_Link *icqlink) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(icqlink, UDP_CMD_KEEP_ALIVE, icqlink->d->icq_UDPSeqNum1++);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWriteDirect(icqlink, p); /* don't queue keep alive packets! */
  icq_PacketDelete(p);

  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Send Keep Alive packet to the server\n");

  return icqlink->d->icq_UDPSeqNum1-1;
}

/**********************************
This must be called to remove
messages from the server
***********************************/
void icq_SendGotMessages(icq_Link *icqlink) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_ACK_MESSAGES);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(icqlink, p);
}

/*************************************
this sends over the contact list
*************************************/
void icq_SendContactList(icq_Link *icqlink) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(icqlink);

  while(ptr)
  {
    icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_CONT_LIST);

    num_used = 0;
    icq_PacketAdvance(p,1);
    while(ptr && num_used<64)
    {
      icq_PacketAppend32(p, ptr->uin);
      num_used++;
      ptr = icq_ContactGetNext(ptr);
    }
    icq_PacketGotoUDPOutData(p, 0);
    icq_PacketAppend8(p, num_used);
    icq_UDPSockWrite(icqlink, p);
  }
}

void icq_SendNewUser(icq_Link *icqlink, unsigned long uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_ADD_TO_LIST);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(icqlink, p);
}

/*************************************
this sends over the visible list
that allows certain users to see you
if you're invisible.
*************************************/
void icq_SendVisibleList(icq_Link *icqlink) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(icqlink);
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_VIS_LIST);

  num_used = 0;
  icq_PacketAdvance(p,1);
  while(ptr)
  {
    if(ptr->vis_list)
    {
      icq_PacketAppend32(p, ptr->uin);
      num_used++;
    }
    ptr = icq_ContactGetNext(ptr);
  }
  if(num_used != 0)
  {
    icq_PacketGotoUDPOutData(p, 0);
    icq_PacketAppend8(p, num_used);
    icq_UDPSockWrite(icqlink, p);
  }
  else
    icq_PacketDelete(p);
}

void icq_SendInvisibleList(icq_Link *icqlink) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(icqlink);
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_INVIS_LIST);

  num_used = 0;
  icq_PacketAdvance(p,1);
  while(ptr)
  {
    if(ptr->invis_list)
    {
      icq_PacketAppend32(p, ptr->uin);
      num_used++;
    }
    ptr = icq_ContactGetNext(ptr);
  }
  if(num_used != 0)
  {
    icq_PacketGotoUDPOutData(p, 0);
    icq_PacketAppend8(p, num_used);
    icq_UDPSockWrite(icqlink, p);
  }
  else
    icq_PacketDelete(p);
}

/**************************************
This sends the second login command
this is necessary to finish logging in.
***************************************/
void icq_SendLogin1(icq_Link *icqlink) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_LOGIN_1);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(icqlink, p);
}

/************************************
This procedure logins into the server with icq_Uin and pass
on the socket icq_Sok and gives our ip and port.
It does NOT wait for any kind of a response.
*************************************/
void icq_Login(icq_Link *icqlink, DWORD status) /* V5 */
{
  icq_Packet *p;

  memset(icqlink->d->icq_UDPServMess, FALSE, sizeof(icqlink->d->icq_UDPServMess));
  icqlink->d->icq_UDPSession = rand() & 0x3FFFFFFF;
  icqlink->d->icq_UDPSeqNum1 = rand() & 0x7FFF;
  icqlink->d->icq_UDPSeqNum2 = 1;

  p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_LOGIN);
  icq_PacketAppend32(p, time(0L));
  icq_PacketAppend32n(p, icqlink->icq_TCPSrvPort);
  /*icq_PacketAppend16(p, 0);
  icq_PacketAppend16n(p, htons(icqlink->icq_OurPort));*/
  icq_PacketAppendString(p, icqlink->icq_Password);
  icq_PacketAppend32(p, LOGIN_X1_DEF);
  if(icqlink->icq_UseTCP)
  {
    if(icqlink->icq_UseProxy)
    {
      icq_PacketAppend32n(p, htonl(icqlink->icq_ProxyIP));
      icq_PacketAppend8(p, LOGIN_SNDONLY_TCP);
    }
    else
    {
      icq_PacketAppend32n(p, htonl(icqlink->icq_OurIP));
      icq_PacketAppend8(p, LOGIN_SNDRCV_TCP);
    }
  }
  else
  {
    icq_PacketAppend32n(p, htonl(icqlink->icq_ProxyIP));
    icq_PacketAppend8(p, LOGIN_NO_TCP);
  }
  icq_PacketAppend32(p, status);
  icq_PacketAppend32(p, LOGIN_X3_DEF);
  icq_PacketAppend32(p, LOGIN_X4_DEF);
  icq_PacketAppend32(p, LOGIN_X5_DEF);

  icq_UDPSockWrite(icqlink, p);
}

/**********************
Logs off ICQ
***********************/
void icq_Logout(icq_Link *icqlink) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(icqlink, UDP_CMD_SEND_TEXT_CODE, icqlink->d->icq_UDPSeqNum1++);
  icq_PacketAppendString(p, "B_USER_DISCONNECTED");
  icq_PacketAppend8(p, 5);
  icq_PacketAppend8(p, 0);
  icq_UDPSockWriteDirect(icqlink, p); /* don't queue */
  icq_PacketDelete(p);
}

/*******************************
This routine sends the aknowlegement cmd to the
server it appears that this must be done after
everything the server sends us
*******************************/
void icq_UDPAck(icq_Link *icqlink, int seq) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(icqlink, UDP_CMD_ACK, seq);
  icq_PacketAppend32(p, rand());

  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Acking\n");
  icq_UDPSockWriteDirect(icqlink, p);
  icq_PacketDelete(p);
}

/***************************************************
Sends a message thru the server to uin.  Text is the
message to send.
***************************************************/
WORD icq_UDPSendMessage(icq_Link *icqlink, DWORD uin, const char *text) /* V5 */
{
  char buf[512]; /* message may be only 450 bytes long */
  icq_Packet *p;

  strncpy(buf, text, 512);
  icq_RusConv("kw", buf);

  p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend16(p, TYPE_MSG);
  icq_PacketAppendString(p, buf);

  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

WORD icq_UDPSendURL(icq_Link *icqlink, DWORD uin, const char *url, const char *descr) /* V5 */
{
  char buf1[512], buf2[512];
  icq_Packet *p;

  strncpy(buf1, descr, 512);
  icq_RusConv("kw", buf1);
  strncpy(buf2, url, 512);

  p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend16(p, TYPE_URL);
  icq_PacketAppend16(p, strlen(buf1)+strlen(buf2)+2); /* length + the NULL + 0xFE delimiter */
  icq_PacketAppendStringFE(p, buf1);
  icq_PacketAppendString0(p, buf2);

  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

/**************************************************
Sends a authorization to the server so the Mirabilis
client can add the user.
***************************************************/
WORD icq_SendAuthMsg(icq_Link *icqlink, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend32(p, TYPE_AUTH);
  icq_PacketAppend16(p, 0);
  icq_UDPSockWrite(icqlink, p);

  return icqlink->d->icq_UDPSeqNum1-1;
}

/**************************************************
Changes the users status on the server
***************************************************/
void icq_ChangeStatus(icq_Link *icqlink, DWORD status) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_STATUS_CHANGE);
  icq_PacketAppend32(p, status);
  icqlink->icq_Status = status;
  icq_UDPSockWrite(icqlink, p);
}

/********************************************************
Sends a request to the server for info on a specific user
*********************************************************/
WORD icq_SendInfoReq(icq_Link *icqlink, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_INFO_REQ);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

/********************************************************
Sends a request to the server for info on a specific user
*********************************************************/
WORD icq_SendExtInfoReq(icq_Link *icqlink, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_EXT_INFO_REQ);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

/**************************************************************
Initializes a server search for the information specified
***************************************************************/
void icq_SendSearchReq(icq_Link *icqlink, const char *email, const char *nick, const char *first,
                       const char *last) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_SEARCH_USER);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
  icq_UDPSockWrite(icqlink, p);
}

/**************************************************************
Initializes a server search for the information specified
***************************************************************/
void icq_SendSearchUINReq(icq_Link *icqlink, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_SEARCH_UIN);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(icqlink, p);
}

/**************************************************
Registers a new uin in the ICQ network
***************************************************/
void icq_RegNewUser(icq_Link *icqlink, const char *pass) /* V5 */
{
  char pass8[9];
  icq_Packet *p = icq_UDPCreateStdSeqPacket(icqlink, UDP_CMD_REG_NEW_USER, icqlink->d->icq_UDPSeqNum1++);
  strncpy(pass8, pass, 8);
  icq_PacketAppendString(p, pass8);
  icq_PacketAppend32(p, 0xA0);
  icq_PacketAppend32(p, 0x2461);
  icq_PacketAppend32(p, 0xA00000);
  icq_PacketAppend32(p, 0x00);
  icq_PacketGoto(p, 6);
  icq_PacketAppend32(p, 0);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(icqlink, p);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Send RegNewUser packet to the server\n");
}

WORD icq_UpdateUserInfo(icq_Link *icqlink, const char *nick, const char *first, const char *last,
                        const char *email) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_UPDATE_INFO);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
/* auth (byte)? */
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

WORD icq_UpdateAuthInfo(icq_Link *icqlink, DWORD auth) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_UPDATE_AUTH);
  icq_PacketAppend32(p, auth); /* NOT auth? */
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

WORD icq_UpdateMetaInfoSet(icq_Link *icqlink, const char *nick, const char *first, const char *last,
                           const char *email, const char *email2, const char *email3,
                           const char *city, const char *state, const char *phone, const char *fax,
                           const char *street, const char *cellular, unsigned long zip,
                           unsigned short cnt_code, unsigned char cnt_stat, unsigned char emailhide)
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_SET_INFO);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
  icq_PacketAppendString(p, email2);
  icq_PacketAppendString(p, email3);
  icq_PacketAppendString(p, city);
  icq_PacketAppendString(p, state);
  icq_PacketAppendString(p, phone);
  icq_PacketAppendString(p, fax);
  icq_PacketAppendString(p, street);
  icq_PacketAppendString(p, cellular);
  icq_PacketAppend32(p, zip);
  icq_PacketAppend16(p, cnt_code);
  icq_PacketAppend8(p, cnt_stat);
  icq_PacketAppend8(p, emailhide);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoHomepage(icq_Link *icqlink, unsigned char age, const char *homepage,
                                unsigned char year, unsigned char month, unsigned char day,
                                unsigned char lang1, unsigned char lang2, unsigned char lang3)
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_META_USER);
  (void)lang1;
  (void)lang2;
  (void)lang3;
  icq_PacketAppend16(p, META_CMD_SET_HOMEPAGE);
  icq_PacketAppend8(p, age);
  icq_PacketAppend16(p, 0x0200);
  icq_PacketAppendString(p, homepage);
  icq_PacketAppend8(p, year);
  icq_PacketAppend8(p, month);
  icq_PacketAppend8(p, day);
  icq_PacketAppend8(p, 0xFF /* lang1 */);
  icq_PacketAppend8(p, 0xFF /* lang2 */);
  icq_PacketAppend8(p, 0xFF /* lang3 */);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoAbout(icq_Link *icqlink, const char *about)
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_SET_ABOUT);
  icq_PacketAppendString(p, about);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoSecurity(icq_Link *icqlink, unsigned char reqauth, unsigned char webpresence,
                                unsigned char pubip)
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_SET_SECURE);
  icq_PacketAppend8(p, !reqauth);
  icq_PacketAppend8(p, webpresence);
  icq_PacketAppend8(p, pubip);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum2-1;
}

WORD icq_UpdateNewUserInfo(icq_Link *icqlink, const char *nick, const char *first, const char *last,
                           const char *email) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_NEW_USER_INFO);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
  icq_PacketAppend8(p, 1);
  icq_PacketAppend8(p, 1);
  icq_PacketAppend8(p, 1);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum1-1;
}

WORD icq_SendMetaInfoReq(icq_Link *icqlink, unsigned long uin)
{
  icq_Packet *p = icq_UDPCreateStdPacket(icqlink, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_REQ_INFO);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(icqlink, p);
  return icqlink->d->icq_UDPSeqNum2-1;
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: udp.c 1162 2000-11-28 02:22:42Z warmenhoven $
$Log$
Revision 1.1  2000/11/28 02:22:42  warmenhoven
icq. whoop de doo

Revision 1.23  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.22  2000/07/09 18:25:44  denis
icq_UpdateNewUserInfo() now returns seq1 instead of seq2 since it
isn't META function.

Revision 1.21  2000/06/25 16:43:19  denis
icq_SendMetaInfoReq() was added.
All icq_*Meta*() functions now returns sequence number 2 because their
replies from the server are synced with it.

Revision 1.20  2000/06/15 01:50:39  bills
removed *Seq functions

Revision 1.19  2000/05/10 19:06:59  denis
UDP outgoing packet queue was implemented.

Revision 1.18  2000/05/03 18:34:43  denis
icq_UpdateNewUserInfo() was added.
All icq_UpdateMetaInfo*() now return their sequence number.

Revision 1.17  2000/04/10 16:36:04  denis
Some more Win32 compatibility from Guillaume Rosanis <grs@mail.com>

Revision 1.16  2000/04/06 19:03:07  denis
return sequence number

Revision 1.15  2000/04/06 16:36:18  denis
So called "Online List problem" bug with Long Contact List was fixed.
icq_*Send*Seq() functions with specified sequence number were added.

Revision 1.14  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.13  1999/12/27 11:12:35  denis
icq_UpdateMetaInfoSecurity() added for setting "My authorization is
required", "Web Aware" and "IP Publishing".

Revision 1.12  1999/10/14 11:43:28  denis
icq_UpdateMetaInfo* functions added.

Revision 1.11  1999/10/07 18:36:27  denis
proxy.h file removed.

Revision 1.10  1999/10/04 13:36:17  denis
Cleanups.

Revision 1.9  1999/09/29 20:15:30  bills
tcp port wasn't being sent properly in login packet

Revision 1.8  1999/09/29 17:13:45  denis
Webaware functions enabled without success even with UDP v5 - need more
investigations.

Revision 1.7  1999/07/18 20:22:16  bills
changed to use new byte-order functions & contact list functions

Revision 1.6  1999/07/16 15:46:00  denis
Cleaned up.

Revision 1.5  1999/07/16 12:40:53  denis
ICQ UDP v5 implemented.
Encription for ICQ UDP v5 implemented.
icq_Packet* unified interface for UDP packets implemented.
Multipacket support of ICQ UDP v5 support added.

Revision 1.4  1999/07/12 15:13:43  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.3  1999/04/29 09:40:52  denis
Unsuccessful attempt to implement web presence (webaware) feature

Revision 1.2  1999/04/14 15:04:13  denis
Cleanups for "strict" compiling (-ansi -pedantic)
Switched from icq_Log callback to icq_Fmt function.

Revision 1.1  1999/03/24 11:37:38  denis
Underscored files with TCP stuff renamed.
TCP stuff cleaned up
Function names changed to corresponding names.
icqlib.c splitted to many small files by subject.
C++ comments changed to ANSI C comments.

*/

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#endif

#include <stdlib.h>

#include "icqtypes.h"
#include "icqlib.h"
#include "udp.h"
#include "queue.h"

#include "stdpackets.h"
#include "icqbyteorder.h"

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

void icq_UDPCheckCode(icq_Packet *p)
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

  icq_PacketGoto(p, 0x14);
  icq_PacketAppend32(p, num1 ^ num2);
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

void icq_UDPEncode(icq_Packet *p)
{
  DWORD checkcode;
  DWORD code1, code2, code3;
  DWORD pos;
  DWORD data;

  icq_UDPCheckCode(p);
  icq_PacketGoto(p, 20);
  checkcode = icq_PacketRead32(p);
  code1 = p->length * 0x68656c6cL;
  code2 = code1 + checkcode;
  pos = 0x0A;

  for(; pos < p->length; pos+=4)
  {
    code3 = code2 + icq_UDPTable[pos & 0xFF];
    data = icqtohl(*(DWORD *)((p->data)+pos));
    data ^= code3;
    *(DWORD*)((p->data)+pos)=htoicql(data);
  }
  checkcode = icq_UDPScramble(checkcode);
  *(DWORD *)((p->data)+0x14)=htoicql(checkcode);
}

/*********************************************************
icq_UDPSockWrite and icq_UDPSockRead are for _UDP_ packets
proxy support for TCP sockets is different!
*********************************************************/
int icq_UDPSockWriteDirect(ICQLINK *link, icq_Packet *p)
{
  char tmpbuf[ICQ_PACKET_DATA_SIZE];

  if(link->icq_UDPSok <= 3)
  {
    icq_FmtLog(link, ICQ_LOG_ERROR, "Bad socket!\n");
    return -1;
  }

  icq_UDPEncode(p);
  if(!link->icq_UseProxy)
  {
#ifdef _WIN32
    return send(link->icq_UDPSok, p->data, p->length, 0);
#else
    return write(link->icq_UDPSok, p->data, p->length);
#endif
  }
  else
  {
    tmpbuf[0] = 0; /* reserved */
    tmpbuf[1] = 0; /* reserved */
    tmpbuf[2] = 0; /* standalone packet */
    tmpbuf[3] = 1; /* address type IP v4 */
    *(unsigned long*)&tmpbuf[4] = htonl(link->icq_ProxyDestIP);
    *(unsigned short*)&tmpbuf[8] = htons(link->icq_ProxyDestPort);
    memcpy(&tmpbuf[10], p->data, p->length);
#ifdef _WIN32
    return send(link->icq_UDPSok, tmpbuf, p->length+10, 0)-10;
#else
    return write(link->icq_UDPSok, tmpbuf, p->length+10)-10;
#endif
  }
}

int icq_UDPSockWrite(ICQLINK *link, icq_Packet *p)
{
  icq_Packet *qp;
  WORD cmd = icq_PacketReadUDPOutCmd(p);
  if(cmd != UDP_CMD_ACK && cmd != UDP_CMD_SEND_TEXT_CODE)
  {
    qp = (icq_Packet*)malloc(sizeof(icq_Packet));
    memcpy(qp, p, sizeof(icq_Packet));
    icq_UDPQueuePut(link, qp, 1);
    if(link->icq_SetTimeout)
      link->icq_SetTimeout(link, icq_UDPQueueInterval(link));
  }
  return icq_UDPSockWriteDirect(link, p);
}

int icq_UDPSockRead(ICQLINK *link, icq_Packet *p)
{
  int res;
  char tmpbuf[ICQ_PACKET_DATA_SIZE];

  if(!link->icq_UseProxy)
  {
#ifdef _WIN32
    res = recv(link->icq_UDPSok, p->data, ICQ_PACKET_DATA_SIZE, 0);
#else
    res = read(link->icq_UDPSok, p->data, ICQ_PACKET_DATA_SIZE);
#endif
    p->length = res;
    return res;
  }
  else
  {
#ifdef _WIN32
    res = recv(link->icq_UDPSok, tmpbuf, ICQ_PACKET_DATA_SIZE, 0);
#else
    res = read(link->icq_UDPSok, tmpbuf, ICQ_PACKET_DATA_SIZE);
#endif
    if(res<0)
      return res;
    memcpy(p->data, &tmpbuf[10], res-10);
    p->length = res-10;
    return res-10;
  }
}

void icq_HandleTimeout(ICQLINK *link)
{
  icq_UDPQueueItem *ptr = 0;
  icq_Packet *sp = 0, *pack = 0;
  int attempt;
  while(icq_UDPQueueInterval(link) == 0)
  {
    ptr = (icq_UDPQueueItem*)list_first(link->icq_UDPQueue);
    attempt = ptr->attempts + 1;
    if(attempt > 6)
    {
      icq_Disconnect(link);
      if(link->icq_Disconnected)
        link->icq_Disconnected(link);
      return;
    }
    pack = icq_UDPQueueGet(link);
    sp = (icq_Packet*)malloc(sizeof(icq_Packet));
    memcpy(sp, pack, sizeof(icq_Packet));
    icq_UDPQueuePut(link, pack, attempt);
    if(link->icq_SetTimeout)
      link->icq_SetTimeout(link, icq_UDPQueueInterval(link));
    icq_UDPSockWriteDirect(link, sp);
    icq_PacketDelete(sp);
  }
}

/****************************************
This must be called every 2 min.
so the server knows we're still alive.
JAVA client sends two different commands
so we do also :)
*****************************************/
WORD icq_KeepAlive(ICQLINK *link) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(link, UDP_CMD_KEEP_ALIVE, link->icq_UDPSeqNum1++);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);

/*  icq_Packet *p = icq_UDPCreateStdPacket(UDP_CMD_KEEP_ALIVE);
  icq_UDPSockWrite(icq_UDPSok, p);
  icq_PacketDelete(p);*/
/*  p = icq_UDPCreateStdPacket(UDP_CMD_KEEP_ALIVE2);
  icq_UDPSockWrite(icq_Sok, p);
  icq_PacketDelete(p);*/

  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Send Keep Alive packet to the server\n");

  return link->icq_UDPSeqNum1-1;
}

/**********************************
This must be called to remove
messages from the server
***********************************/
void icq_SendGotMessages(ICQLINK *link) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_ACK_MESSAGES);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/*************************************
this sends over the contact list
*************************************/
void icq_SendContactList(ICQLINK *link) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(link);

  while(ptr)
  {
    icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_CONT_LIST);

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
    icq_UDPSockWrite(link, p);
    icq_PacketDelete(p);
  }
}

void icq_SendNewUser(ICQLINK *link, unsigned long uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_ADD_TO_LIST);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/*************************************
this sends over the visible list
that allows certain users to see you
if you're invisible.
*************************************/
void icq_SendVisibleList(ICQLINK *link) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(link);
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_VIS_LIST);

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
    icq_UDPSockWrite(link, p);
  }
  icq_PacketDelete(p);
}

void icq_SendInvisibleList(ICQLINK *link) /* V5 */
{
  char num_used;
  icq_ContactItem *ptr = icq_ContactGetFirst(link);
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_INVIS_LIST);

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
    icq_UDPSockWrite(link, p);
  }
  icq_PacketDelete(p);
}

/**************************************
This sends the second login command
this is necessary to finish logging in.
***************************************/
void icq_SendLogin1(ICQLINK *link) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_LOGIN_1);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/************************************
This procedure logins into the server with icq_Uin and pass
on the socket icq_Sok and gives our ip and port.
It does NOT wait for any kind of a response.
*************************************/
void icq_Login(ICQLINK *link, DWORD status) /* V5 */
{
  icq_Packet *p;

  memset(link->icq_UDPServMess, FALSE, sizeof(link->icq_UDPServMess));
  link->icq_UDPSession = rand() & 0x3FFFFFFF;
  link->icq_UDPSeqNum1 = rand() & 0x7FFF;
  link->icq_UDPSeqNum2 = 1;

  p = icq_UDPCreateStdPacket(link, UDP_CMD_LOGIN);
  icq_PacketAppend32(p, time(0L));
  icq_PacketAppend32n(p, link->icq_TCPSrvPort);
  /*icq_PacketAppend16(p, 0);
  icq_PacketAppend16n(p, htons(link->icq_OurPort));*/
  icq_PacketAppendString(p, link->icq_Password);
  icq_PacketAppend32(p, LOGIN_X1_DEF);
  icq_PacketAppend32n(p, htonl(link->icq_OurIP));
  if(link->icq_UseProxy)
    icq_PacketAppend8(p, LOGIN_SNDONLY_TCP);
  else
    icq_PacketAppend8(p, LOGIN_SNDRCV_TCP);
  icq_PacketAppend32(p, status);
  icq_PacketAppend32(p, LOGIN_X3_DEF);
  icq_PacketAppend32(p, LOGIN_X4_DEF);
  icq_PacketAppend32(p, LOGIN_X5_DEF);

  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/**********************
Logs off ICQ
***********************/
void icq_Logout(ICQLINK *link) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(link, UDP_CMD_SEND_TEXT_CODE, link->icq_UDPSeqNum1++);
  icq_PacketAppendString(p, "B_USER_DISCONNECTED");
  icq_PacketAppend8(p, 5);
  icq_PacketAppend8(p, 0);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/*******************************
This routine sends the aknowlegement cmd to the
server it appears that this must be done after
everything the server sends us
*******************************/
void icq_UDPAck(ICQLINK *link, int seq) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdSeqPacket(link, UDP_CMD_ACK, seq);
  icq_PacketAppend32(p, rand());

  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Acking\n");
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/***************************************************
Sends a message thru the server to uin.  Text is the
message to send.
***************************************************/
WORD icq_UDPSendMessage(ICQLINK *link, DWORD uin, const char *text) /* V5 */
{
  char buf[512]; /* message may be only 450 bytes long */
  icq_Packet *p;

  strncpy(buf, text, 512);
  icq_RusConv("kw", buf);

  p = icq_UDPCreateStdPacket(link, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend16(p, TYPE_MSG);
  icq_PacketAppendString(p, buf);

  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

WORD icq_UDPSendURL(ICQLINK *link, DWORD uin, const char *url, const char *descr) /* V5 */
{
  char buf1[512], buf2[512];
  icq_Packet *p;

  strncpy(buf1, descr, 512);
  icq_RusConv("kw", buf1);
  strncpy(buf2, url, 512);

  p = icq_UDPCreateStdPacket(link, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend16(p, TYPE_URL);
  icq_PacketAppend16(p, strlen(buf1)+strlen(buf2)+2); /* length + the NULL + 0xFE delimiter */
  icq_PacketAppendStringFE(p, buf1);
  icq_PacketAppendString0(p, buf2);

  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

/**************************************************
Sends a authorization to the server so the Mirabilis
client can add the user.
***************************************************/
WORD icq_SendAuthMsg(ICQLINK *link, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_SEND_THRU_SRV);
  icq_PacketAppend32(p, uin);
  icq_PacketAppend32(p, TYPE_AUTH);
  icq_PacketAppend16(p, 0);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);

  return link->icq_UDPSeqNum1-1;
}

/**************************************************
Changes the users status on the server
***************************************************/
void icq_ChangeStatus(ICQLINK *link, DWORD status) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_STATUS_CHANGE);
  icq_PacketAppend32(p, status);
  link->icq_Status = status;
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/********************************************************
Sends a request to the server for info on a specific user
*********************************************************/
WORD icq_SendInfoReq(ICQLINK *link, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_INFO_REQ);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

/********************************************************
Sends a request to the server for info on a specific user
*********************************************************/
WORD icq_SendExtInfoReq(ICQLINK *link, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_EXT_INFO_REQ);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

/**************************************************************
Initializes a server search for the information specified
***************************************************************/
void icq_SendSearchReq(ICQLINK *link, const char *email, const char *nick, const char *first,
                       const char *last) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_SEARCH_USER);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/**************************************************************
Initializes a server search for the information specified
***************************************************************/
void icq_SendSearchUINReq(ICQLINK *link, DWORD uin) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_SEARCH_UIN);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
}

/**************************************************
Registers a new uin in the ICQ network
***************************************************/
void icq_RegNewUser(ICQLINK *link, const char *pass) /* V5 */
{
  char pass8[9];
  icq_Packet *p = icq_UDPCreateStdSeqPacket(link, UDP_CMD_REG_NEW_USER, link->icq_UDPSeqNum1++);
  strncpy(pass8, pass, 8);
  icq_PacketAppendString(p, pass8);
  icq_PacketAppend32(p, 0xA0);
  icq_PacketAppend32(p, 0x2461);
  icq_PacketAppend32(p, 0xA00000);
  icq_PacketAppend32(p, 0x00);
  icq_PacketGoto(p, 6);
  icq_PacketAppend32(p, 0);
  icq_PacketAppend32(p, rand());
  icq_UDPSockWrite(link, p);
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Send RegNewUser packet to the server\n");
  icq_PacketDelete(p);
}

WORD icq_UpdateUserInfo(ICQLINK *link, const char *nick, const char *first, const char *last,
                        const char *email) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_UPDATE_INFO);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
/* auth (byte)? */
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

WORD icq_UpdateAuthInfo(ICQLINK *link, DWORD auth) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_UPDATE_AUTH);
  icq_PacketAppend32(p, auth); /* NOT auth? */
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

WORD icq_UpdateMetaInfoSet(ICQLINK *link, const char *nick, const char *first, const char *last,
                           const char *email, const char *email2, const char *email3,
                           const char *city, const char *state, const char *phone, const char *fax,
                           const char *street, const char *cellular, unsigned long zip,
                           unsigned short cnt_code, unsigned char cnt_stat, unsigned char emailhide)
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_META_USER);
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
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoHomepage(ICQLINK *link, unsigned char age, const char *homepage,
                                unsigned char year, unsigned char month, unsigned char day,
                                unsigned char lang1, unsigned char lang2, unsigned char lang3)
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_META_USER);
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
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoAbout(ICQLINK *link, const char *about)
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_SET_ABOUT);
  icq_PacketAppendString(p, about);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum2-1;
}

WORD icq_UpdateMetaInfoSecurity(ICQLINK *link, unsigned char reqauth, unsigned char webpresence,
                                unsigned char pubip)
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_SET_SECURE);
  icq_PacketAppend8(p, !reqauth);
  icq_PacketAppend8(p, webpresence);
  icq_PacketAppend8(p, pubip);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum2-1;
}

WORD icq_UpdateNewUserInfo(ICQLINK *link, const char *nick, const char *first, const char *last,
                           const char *email) /* V5 */
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_NEW_USER_INFO);
  icq_PacketAppendString(p, nick);
  icq_PacketAppendString(p, first);
  icq_PacketAppendString(p, last);
  icq_PacketAppendString(p, email);
  icq_PacketAppend8(p, 1);
  icq_PacketAppend8(p, 1);
  icq_PacketAppend8(p, 1);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum1-1;
}

WORD icq_SendMetaInfoReq(ICQLINK *link, unsigned long uin)
{
  icq_Packet *p = icq_UDPCreateStdPacket(link, UDP_CMD_META_USER);
  icq_PacketAppend16(p, META_CMD_REQ_INFO);
  icq_PacketAppend32(p, uin);
  icq_UDPSockWrite(link, p);
  icq_PacketDelete(p);
  return link->icq_UDPSeqNum2-1;
}

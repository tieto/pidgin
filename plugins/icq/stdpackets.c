/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: stdpackets.c 1442 2001-01-28 01:52:27Z warmenhoven $
$Log$
Revision 1.3  2001/01/28 01:52:27  warmenhoven
icqlib 1.1.5

Revision 1.12  2001/01/24 05:11:14  bills
applied patch from Robin Ericsson <lobbin@localhost.nu> which implements
receiving contact lists.  See new icq_RecvContactList callback.

Revision 1.11  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.10  2000/06/15 01:51:23  bills
added creation functions for cancel and refuse operations

Revision 1.9  2000/05/04 15:50:38  bills
warning cleanups

Revision 1.8  2000/04/10 18:11:45  denis
ANSI cleanups.

Revision 1.7  2000/04/06 16:38:04  denis
icq_*Send*Seq() functions with specified sequence number were added.

Revision 1.6  2000/02/07 02:35:13  bills
slightly modified chat packets

Revision 1.5  2000/01/20 19:59:15  bills
first implementation of sending file requests

Revision 1.4  1999/09/29 20:12:32  bills
tcp_link*->icq_TCPLink*

Revision 1.3  1999/09/29 17:07:48  denis
Host/network byteorder cleanups.

Revision 1.2  1999/07/16 15:45:20  denis
Cleaned up.

Revision 1.1  1999/07/16 12:13:11  denis
UDP packets support added.
tcppackets.[ch] files renamed to stdpackets.[ch]

Revision 1.9  1999/07/12 15:13:39  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.8  1999/05/03 21:41:28  bills
initial file xfer support added- untested

Revision 1.7  1999/04/17 19:39:09  bills
added new functions to create chat packets. removed unnecessary code.
added new function to create URL ack packet.

Revision 1.6  1999/04/14 15:08:39  denis
Cleanups for "strict" compiling (-ansi -pedantic)

*/

#include <stdlib.h>

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"
#include "tcp.h"
#include "stdpackets.h"

icq_Packet *icq_TCPCreateInitPacket(icq_TCPLink *plink)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    int type=plink->type;

    icq_PacketAppend8(p, 0xFF);
    icq_PacketAppend32(p, ICQ_TCP_VER);
    if(type==TCP_LINK_MESSAGE)
      icq_PacketAppend32n(p, htons(plink->icqlink->icq_TCPSrvPort));
    else
      icq_PacketAppend32(p, 0x00000000);
    icq_PacketAppend32(p, plink->icqlink->icq_Uin);
    icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
    icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
    icq_PacketAppend8(p, 0x04);
    if(type==TCP_LINK_FILE || type==TCP_LINK_CHAT)
      icq_PacketAppend32(p, ntohs(plink->socket_address.sin_port));
    else
      icq_PacketAppend32(p, 0x00000000);

  }

  return p;
}

icq_Packet *icq_TCPCreateStdPacket(icq_TCPLink *plink, WORD icq_TCPCommand,
               WORD type, const unsigned char *msg, WORD status,
               WORD msg_command)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend32(p, plink->icqlink->icq_Uin);
    icq_PacketAppend16(p, ICQ_TCP_VER);
    icq_PacketAppend16(p, icq_TCPCommand);
    icq_PacketAppend16(p, 0x0000);
    icq_PacketAppend32(p, plink->icqlink->icq_Uin);

    icq_PacketAppend16(p, type);
    icq_PacketAppendString(p, (char*)msg);
      
    /* FIXME: this should be the address the server returns to us,
     * link->icq_OurIp */
    icq_PacketAppend32(p, plink->socket_address.sin_addr.s_addr); 
    icq_PacketAppend32(p, plink->socket_address.sin_addr.s_addr);
    icq_PacketAppend32(p, ntohs(plink->socket_address.sin_port));
    icq_PacketAppend8(p, 0x04);
    icq_PacketAppend16(p, status);
    icq_PacketAppend16(p, msg_command);
  }

  return p;
}

icq_Packet *icq_TCPCreateMessagePacket(icq_TCPLink *plink, const unsigned char *message)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_MESSAGE,
    ICQ_TCP_MSG_MSG,
    message,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  return p;
}

icq_Packet *icq_TCPCreateURLPacket(icq_TCPLink *plink, const char *message,
   const char *url)
{
  icq_Packet *p;
  unsigned char *str=(unsigned char*)malloc(strlen(message)+strlen(url)+2);

  strcpy((char*)str, message);
  *(str+strlen(message))=0xFE;
  strcpy((char*)(str+strlen(message)+1), url);

  p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_MESSAGE,
    ICQ_TCP_MSG_URL,
    str,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  free(str);

  return p;
}

icq_Packet *icq_TCPCreateChatReqPacket(icq_TCPLink *plink, const unsigned char *message)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_MESSAGE,
    ICQ_TCP_MSG_CHAT,
    message,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  icq_PacketAppendString(p, 0);

  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend32(p, 0x00000000);

  return p;
}

icq_Packet *icq_TCPCreateChatReqAck(icq_TCPLink *plink, WORD port)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_CHAT,
    0,
    0, /* status */
    ICQ_TCP_MSG_ACK);

  icq_PacketAppendString(p, 0);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateChatReqRefuse(icq_TCPLink *plink, WORD port,
  const char *reason)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_CHAT,
    reason,
    ICQ_TCP_STATUS_REFUSE,
    ICQ_TCP_MSG_ACK);

  icq_PacketAppendString(p, 0);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateChatReqCancel(icq_TCPLink *plink, WORD port)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_CANCEL,
    ICQ_TCP_MSG_CHAT,
    0,
    0, /* status */
    ICQ_TCP_MSG_ACK);

  icq_PacketAppendString(p, 0);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateFileReqAck(icq_TCPLink *plink, WORD port)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_FILE,
    0,
    0, /* status */
    ICQ_TCP_MSG_ACK);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppendString(p, 0);
  icq_PacketAppend32(p, 0x00000000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateFileReqRefuse(icq_TCPLink *plink, WORD port,
  const char *reason)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_FILE,
    reason,
    ICQ_TCP_STATUS_REFUSE,
    ICQ_TCP_MSG_ACK);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppendString(p, 0);
  icq_PacketAppend32(p, 0x00000000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateFileReqCancel(icq_TCPLink *plink, WORD port)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_CANCEL,
    ICQ_TCP_MSG_FILE,
    0,
    0, /* status */
    ICQ_TCP_MSG_ACK);

  icq_PacketAppend16(p, htons(port));
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppendString(p, 0);
  icq_PacketAppend32(p, 0x00000000);
  icq_PacketAppend32(p, port);

  return p;
}

icq_Packet *icq_TCPCreateChatInfoPacket(icq_TCPLink *plink, const char *name, 
   DWORD foreground, DWORD background)
{
   icq_Packet *p=icq_PacketNew();

   icq_PacketAppend32(p, 0x00000065);
   icq_PacketAppend32(p, 0xfffffffa);
   icq_PacketAppend32(p, plink->icqlink->icq_Uin);
   icq_PacketAppendString(p, name);
   icq_PacketAppend16(p, plink->socket_address.sin_port);
   icq_PacketAppend32(p, foreground);
   icq_PacketAppend32(p, background);
   icq_PacketAppend8(p, 0x00);

   return p;

}

icq_Packet *icq_TCPCreateChatInfo2Packet(icq_TCPLink *plink, const char *name, 
   DWORD foreground, DWORD background)
{
   icq_Packet *p=icq_PacketNew();

   icq_PacketAppend32(p, 0x00000064);
   icq_PacketAppend32(p, plink->icqlink->icq_Uin);
   icq_PacketAppendString(p, name);
   icq_PacketAppend32(p, foreground);
   icq_PacketAppend32(p, background);

   icq_PacketAppend32(p, 0x00070004);
   icq_PacketAppend32(p, 0x00000000);
   icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
   icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
   icq_PacketAppend8(p, 0x04);
   icq_PacketAppend16(p, 0x0000);
   icq_PacketAppend32(p, 0x000a);
   icq_PacketAppend32(p, 0x0000);
   icq_PacketAppendString(p, "Courier New");
   icq_PacketAppend8(p, 204);
   icq_PacketAppend8(p, 49);
   icq_PacketAppend8(p, 0x00);

   return p;
}

icq_Packet *icq_TCPCreateChatFontInfoPacket(icq_TCPLink *plink)
{
   icq_Packet *p=icq_PacketNew();
   
   icq_PacketAppend32(p, 0x00070004);
   icq_PacketAppend32(p, 0x00000000);
   icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
   icq_PacketAppend32n(p, htonl(plink->icqlink->icq_OurIP));
   icq_PacketAppend8(p, 0x04);
   icq_PacketAppend16(p, ntohs(plink->socket_address.sin_port)); /* Zero ? */
   icq_PacketAppend32(p, 0x000a);
   icq_PacketAppend32(p, 0x0000);
   icq_PacketAppendString(p, "Courier New");
   icq_PacketAppend8(p, 204);
   icq_PacketAppend8(p, 49);
   icq_PacketAppend8(p, 0x00);

   return p;
}


icq_Packet *icq_TCPCreateFileReqPacket(icq_TCPLink *plink, 
  const char *message, const char *filename, unsigned long size)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_MESSAGE,
    ICQ_TCP_MSG_FILE,
    (const unsigned char*)message,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend16(p, 0x0000);

  icq_PacketAppendString(p, filename);

  icq_PacketAppend32(p, size);
  icq_PacketAppend32(p, 0x00000000);

  return p;
}

void icq_TCPAppendSequence(ICQLINK *link, icq_Packet *p)
{
  p->id=link->d->icq_TCPSequence--;
  icq_PacketEnd(p);
  icq_PacketAppend32(p, p->id);
}

void icq_TCPAppendSequenceN(ICQLINK *link, icq_Packet *p, DWORD seq)
{
  (void)link;
  p->id=seq;
  icq_PacketEnd(p);
  icq_PacketAppend32(p, p->id);
}

icq_Packet *icq_TCPCreateMessageAck(icq_TCPLink *plink, const unsigned char *message)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_MSG,
    message,
    0, /* status */
    ICQ_TCP_MSG_ACK);

   return p;
}

icq_Packet *icq_TCPCreateURLAck(icq_TCPLink *plink, const unsigned char *message)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_URL,
    message,
    0, /* status */
    ICQ_TCP_MSG_ACK);

   return p;
}

icq_Packet *icq_TCPCreateContactListAck(icq_TCPLink *plink, const unsigned char *message)
{
  icq_Packet *p=icq_TCPCreateStdPacket(
    plink,
    ICQ_TCP_ACK,
    ICQ_TCP_MSG_CONTACTLIST,
    message,
    0, /* status */
    ICQ_TCP_MSG_ACK);

  return p;
}

icq_Packet *icq_TCPCreateFile00Packet(DWORD num_files, DWORD total_bytes,
  DWORD speed, const char *name)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x00);
    icq_PacketAppend32(p, 0x00000000);
    icq_PacketAppend32(p, num_files);
    icq_PacketAppend32(p, total_bytes);
    icq_PacketAppend32(p, speed);
    icq_PacketAppendString(p, name);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile01Packet(DWORD speed, const char *name)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x01);
    icq_PacketAppend32(p, speed);
    icq_PacketAppendString(p, name);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile02Packet(const char *filename, DWORD filesize,
  DWORD speed)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x02);
    icq_PacketAppend8(p, 0x00);
    icq_PacketAppendString(p, filename);
    icq_PacketAppendString(p, 0);
    icq_PacketAppend32(p, filesize);
    icq_PacketAppend32(p, 0x00000000);    
    icq_PacketAppend32(p, speed);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile03Packet(DWORD filesize, DWORD speed)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x03);
    icq_PacketAppend32(p, filesize);
    icq_PacketAppend32(p, 0x00000000);
    icq_PacketAppend32(p, speed);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile04Packet(DWORD filenum)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x04);
    icq_PacketAppend32(p, filenum);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile05Packet(DWORD speed)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x05);
    icq_PacketAppend32(p, speed);
  }

  return p;
}

icq_Packet *icq_TCPCreateFile06Packet(int length, void *data)
{
  icq_Packet *p=icq_PacketNew();

  if(p)
  {
    icq_PacketAppend8(p, 0x06);
    icq_PacketAppend(p, data, length);
  }

  return p;
}

icq_Packet *icq_UDPCreateStdPacket(ICQLINK *link, WORD cmd)
{
  icq_Packet *p = icq_PacketNew();

/*  if(!link->d->icq_UDPSession)
    link->d->icq_UDPSession = rand() & 0x3FFFFFFF;
  if(!link->d->icq_UDPSeqNum2)
    link->d->icq_UDPSeqNum2 = rand() & 0x7FFF;*/

  icq_PacketAppend16(p, ICQ_UDP_VER);            /* ver */
  icq_PacketAppend32(p, 0);                      /* zero */
  icq_PacketAppend32(p, link->icq_Uin);          /* uin */
  icq_PacketAppend32(p, link->d->icq_UDPSession);   /* session */
  icq_PacketAppend16(p, cmd);                    /* cmd */
  icq_PacketAppend16(p, link->d->icq_UDPSeqNum1++); /* seq1 */
  icq_PacketAppend16(p, link->d->icq_UDPSeqNum2++); /* seq2 */
  icq_PacketAppend32(p, 0);                      /* checkcode */

  return p;
}

icq_Packet *icq_UDPCreateStdSeqPacket(ICQLINK *link, WORD cmd, WORD seq)
{
  icq_Packet *p = icq_PacketNew();

  icq_PacketAppend16(p, ICQ_UDP_VER);            /* ver */
  icq_PacketAppend32(p, 0);                      /* zero */
  icq_PacketAppend32(p, link->icq_Uin);          /* uin */
  icq_PacketAppend32(p, link->d->icq_UDPSession);   /* session */
  icq_PacketAppend16(p, cmd);                    /* cmd */
  icq_PacketAppend16(p, seq);                    /* seq1 */
  icq_PacketAppend16(p, 0);                      /* seq2 */
  icq_PacketAppend32(p, 0);                      /* checkcode */

  return p;
}

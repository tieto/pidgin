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

#include <stdlib.h>

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
               WORD type, const char *msg, WORD status,
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

icq_Packet *icq_TCPCreateMessagePacket(icq_TCPLink *plink, const char *message)
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
    (const char *)str,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  free(str);

  return p;
}

icq_Packet *icq_TCPCreateChatReqPacket(icq_TCPLink *plink, const char *message)
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
    (const char*)message,
    0, /* status */
    ICQ_TCP_MSG_REAL);

  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend16(p, 0x0000);

  icq_PacketAppendString(p, filename);

  icq_PacketAppend32(p, size);
  icq_PacketAppend32(p, 0x00000000);

  return p;
}

void icq_TCPAppendSequence(icq_Link *icqlink, icq_Packet *p)
{
  p->id=icqlink->d->icq_TCPSequence--;
  icq_PacketEnd(p);
  icq_PacketAppend32(p, p->id);
}

void icq_TCPAppendSequenceN(icq_Link *icqlink, icq_Packet *p, DWORD seq)
{
  (void)icqlink;
  p->id=seq;
  icq_PacketEnd(p);
  icq_PacketAppend32(p, p->id);
}

icq_Packet *icq_TCPCreateMessageAck(icq_TCPLink *plink, const char *message)
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

icq_Packet *icq_TCPCreateURLAck(icq_TCPLink *plink, const char *message)
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

icq_Packet *icq_TCPCreateContactListAck(icq_TCPLink *plink, const char *message)
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

icq_Packet *icq_UDPCreateStdPacket(icq_Link *icqlink, WORD cmd)
{
  icq_Packet *p = icq_PacketNew();

/*  if(!link->d->icq_UDPSession)
    link->d->icq_UDPSession = rand() & 0x3FFFFFFF;
  if(!link->d->icq_UDPSeqNum2)
    link->d->icq_UDPSeqNum2 = rand() & 0x7FFF;*/

  icq_PacketAppend16(p, ICQ_UDP_VER);                  /* ver */
  icq_PacketAppend32(p, 0);                            /* zero */
  icq_PacketAppend32(p, icqlink->icq_Uin);             /* uin */
  icq_PacketAppend32(p, icqlink->d->icq_UDPSession);   /* session */
  icq_PacketAppend16(p, cmd);                          /* cmd */
  icq_PacketAppend16(p, icqlink->d->icq_UDPSeqNum1++); /* seq1 */
  icq_PacketAppend16(p, icqlink->d->icq_UDPSeqNum2++); /* seq2 */
  icq_PacketAppend32(p, 0);                            /* checkcode */

  return p;
}

icq_Packet *icq_UDPCreateStdSeqPacket(icq_Link *icqlink, WORD cmd, WORD seq)
{
  icq_Packet *p = icq_PacketNew();

  icq_PacketAppend16(p, ICQ_UDP_VER);                /* ver */
  icq_PacketAppend32(p, 0);                          /* zero */
  icq_PacketAppend32(p, icqlink->icq_Uin);           /* uin */
  icq_PacketAppend32(p, icqlink->d->icq_UDPSession); /* session */
  icq_PacketAppend16(p, cmd);                        /* cmd */
  icq_PacketAppend16(p, seq);                        /* seq1 */
  icq_PacketAppend16(p, 0);                          /* seq2 */
  icq_PacketAppend32(p, 0);                          /* checkcode */

  return p;
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: tcpchathandle.c 1987 2001-06-09 14:46:51Z warmenhoven $
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

#include <stdlib.h>

#include "icqlib.h"

#include "stdpackets.h"
#include "chatsession.h"

void icq_HandleChatAck(icq_TCPLink *plink, icq_Packet *p, int port)
{
  icq_TCPLink *pchatlink;
  icq_ChatSession *pchat;
  icq_Packet *p2;

  pchatlink=icq_TCPLinkNew(plink->icqlink);
  pchatlink->type=TCP_LINK_CHAT;
  pchatlink->id=p->id;

  /* once the ack packet has been processed, create a new chat session */
  pchat=icq_ChatSessionNew(plink->icqlink);

  pchat->id=p->id;
  pchat->remote_uin=plink->remote_uin;
  pchat->tcplink=pchatlink;

  invoke_callback(plink->icqlink, icq_RequestNotify)(plink->icqlink, p->id,
    ICQ_NOTIFY_CHATSESSION, sizeof(icq_ChatSession), pchat);

  icq_ChatSessionSetStatus(pchat, CHAT_STATUS_CONNECTING);
  icq_TCPLinkConnect(pchatlink, plink->remote_uin, port);

  pchatlink->session=pchat;

  p2=icq_TCPCreateChatInfoPacket(pchatlink, plink->icqlink->icq_Nick,
    0x00ffffff, 0x00000000);
  icq_TCPLinkSend(pchatlink, p2);

}

void icq_HandleChatHello(icq_TCPLink *plink)
{

  /* once the hello packet has been processed and we know which uin this
   * link is for, we can link up with a chat session */
  icq_ChatSession *pchat=icq_FindChatSession(plink->icqlink,
    plink->remote_uin);

  if(pchat)
  {
    plink->id=pchat->id;
    plink->session=pchat;
    icq_ChatSessionSetStatus(pchat, CHAT_STATUS_WAIT_NAME);

  } else {

    icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING,
      "unexpected chat hello received from %d, closing link\n",
      plink->remote_uin);
    icq_TCPLinkClose(plink);
  }

}

void icq_TCPOnChatReqReceived(icq_Link *icqlink, DWORD uin,
  const char *message, DWORD id)
{
  /* use the current system time for time received */
  time_t t=time(0);
  struct tm *ptime=localtime(&t);

#ifdef TCP_PACKET_TRACE
  printf("chat request packet received from %lu { sequence=%lx, message=%s }\n",
     uin, id, message);
#endif /* TCP_PACKET_TRACE */

  invoke_callback(icqlink,icq_RecvChatReq)(icqlink, uin, ptime->tm_hour, 
    ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900,
    message, id);

  /* don't send an acknowledgement to the remote client!
   * GUI is responsible for sending acknowledgement once user accepts
   * or denies using icq_TCPSendChatAck */
}

void icq_TCPChatUpdateFont(icq_ChatSession *psession, const char *font, 
  WORD encoding, DWORD style, DWORD size)
{
  icq_Link *icqlink = psession->icqlink;
  int packet_len, fontlen;
  char *buffer;

  buffer = malloc(packet_len = (2 + (fontlen = strlen(font) + 1)) + 
    2 + 1 + (4+1)*2);
  buffer[0] = '\x11';
  *((DWORD *)&buffer[1]) = style;
  buffer[5] = '\x12';
  *((DWORD *)&buffer[6]) = size;
  buffer[10] = '\x10';
  *((WORD *)&buffer[11]) = fontlen;
  strcpy(&buffer[13], font);

  icq_RusConv("wk", &buffer[13]);

  *((WORD *)&buffer[13 + fontlen]) = encoding;

  invoke_callback(icqlink, icq_ChatNotify)(psession, CHAT_NOTIFY_DATA,
    packet_len, buffer);

  free(buffer);
}

void icq_TCPChatUpdateColors(icq_ChatSession *psession, DWORD foreground, 
  DWORD background)
{
  icq_Link *icqlink = psession->icqlink;
  char buffer[10];

  buffer[0] = '\x00';
  *((DWORD *)&buffer[1]) = foreground;
  buffer[5] = '\x01';
  *((DWORD *)&buffer[6]) = background;

  invoke_callback(icqlink, icq_ChatNotify)(psession, 
    CHAT_NOTIFY_DATA, sizeof(buffer), buffer);
}

void icq_TCPProcessChatPacket(icq_Packet *p, icq_TCPLink *plink)
{
  DWORD code;
  DWORD remote_uin;
  DWORD ip1, ip2;
  DWORD fg, bg, fontstyle, fontsize;
  WORD port1, encoding;
  icq_Packet *presponse;
  icq_ChatSession *pchat=(icq_ChatSession *)plink->session;
  const char *font, *user;
	
  icq_PacketBegin(p);

  code=icq_PacketRead32(p);
  remote_uin=icq_PacketRead32(p);

  if(code==0x00000006 || code==0x00070004)
  {
    font = (char *)NULL;
    encoding = 0;
    fontstyle = 0;
    fontsize = 0;
    if(code == 0x00070004)
    {
      ip1 = icq_PacketRead32(p);
      ip2 = icq_PacketRead32(p);
      icq_PacketRead8(p);
      port1 = icq_PacketRead16(p);
      fontsize = icq_PacketRead32(p);
      fontstyle = icq_PacketRead32(p);
      font = icq_PacketReadString(p);
      encoding = icq_PacketRead16(p);
    }
    else
    {
      ip1 = icq_PacketRead32(p);
      ip2 = icq_PacketRead32(p);
      port1 = icq_PacketRead16(p);
      icq_PacketRead8(p);
      fontsize = icq_PacketRead32(p);
      fontstyle = icq_PacketRead32(p);
      font = icq_PacketReadString(p);
      encoding = icq_PacketRead16(p);
    }
    if(font)
      icq_TCPChatUpdateFont(pchat, font, encoding, fontstyle, fontsize);
    icq_ChatSessionSetStatus(pchat, CHAT_STATUS_READY);
    plink->mode|=TCP_LINK_MODE_RAW;
  }
  else
    if(remote_uin>0xffffff00)
    {
      remote_uin=icq_PacketRead32(p);
      user = icq_PacketReadString(p);
      icq_PacketRead16(p); /* Unknown */;
      fg = icq_PacketRead32(p);
      bg = icq_PacketRead32(p);
      icq_TCPChatUpdateColors(pchat, fg, bg);

      presponse=icq_TCPCreateChatInfo2Packet(plink, plink->icqlink->icq_Nick,
        0x00ffffff, 0x00000000);
      icq_PacketSend(presponse, plink->socket);
      icq_PacketDelete(presponse);
      icq_ChatSessionSetStatus(pchat, CHAT_STATUS_WAIT_FONT);
    }
    else
    {
      user = icq_PacketReadString(p);
      fg = icq_PacketRead32(p);
      bg = icq_PacketRead32(p);
      icq_TCPChatUpdateColors(pchat, fg, bg);
      font = (char *)NULL;
      encoding = 0;
      fontstyle = 0;
      fontsize = 0;
      if((code = icq_PacketRead32(p)) == 0x00070004)
      {
        icq_PacketRead32(p);
        ip1 = icq_PacketRead32(p);
        ip2 = icq_PacketRead32(p);
        icq_PacketRead8(p);
        port1 = icq_PacketRead16(p);
        fontsize = icq_PacketRead32(p);
        fontstyle = icq_PacketRead32(p);
        font = icq_PacketReadString(p);
        encoding = icq_PacketRead16(p);
      }
      else if(code == 0x00000006)
      {
        icq_PacketRead32(p);
        ip1 = icq_PacketRead32(p);
        ip2 = icq_PacketRead32(p);
        port1 = icq_PacketRead16(p);
        icq_PacketRead8(p);
        fontsize = icq_PacketRead32(p);
        fontstyle = icq_PacketRead32(p);
        font = icq_PacketReadString(p);
        encoding = icq_PacketRead16(p);
      }
      if(font)
        icq_TCPChatUpdateFont(pchat, font, encoding, fontstyle, fontsize);
      presponse=icq_TCPCreateChatFontInfoPacket(plink);
      icq_PacketSend(presponse, plink->socket);
      icq_PacketDelete(presponse);
      /* notify app that chat connection has been established */
      icq_ChatSessionSetStatus(pchat, CHAT_STATUS_READY);
      plink->mode|=TCP_LINK_MODE_RAW;
    }
}


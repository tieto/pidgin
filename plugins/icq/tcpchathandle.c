/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: tcpchathandle.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.8  2000/07/24 03:10:08  bills
added support for real nickname during TCP transactions like file and
chat, instead of using Bill all the time (hmm, where'd I get that from? :)

Revision 1.7  2000/07/09 22:19:35  bills
added new *Close functions, use *Close functions instead of *Delete
where correct, and misc cleanup

Revision 1.6  2000/05/04 15:57:20  bills
Reworked file transfer notification, small bugfixes, and cleanups.

Revision 1.5  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.4  2000/04/05 14:37:02  denis
Applied patch from "Guillaume R." <grs@mail.com> for basic Win32
compatibility.

Revision 1.3  2000/02/07 02:54:45  bills
warning cleanups.

Revision 1.2  2000/02/07 02:43:37  bills
new code for special chat functions (background, fonts, etc)

Revision 1.1  1999/09/29 19:47:21  bills
reworked chat/file handling.  fixed chat. (it's been broke since I put
non-blocking connects in)

*/

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdlib.h>

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"

#include "stdpackets.h"
#include "tcplink.h"
#include "chatsession.h"

void icq_HandleChatAck(icq_TCPLink *plink, icq_Packet *p, int port)
{
  icq_TCPLink *pchatlink;
  icq_ChatSession *pchat;
  icq_Packet *p2;

  if(plink->icqlink->icq_RequestNotify)
    (*plink->icqlink->icq_RequestNotify)(plink->icqlink, p->id, ICQ_NOTIFY_ACK, 0, 0);
  pchatlink=icq_TCPLinkNew(plink->icqlink);
  pchatlink->type=TCP_LINK_CHAT;
  pchatlink->id=p->id;
  
  pchat=icq_ChatSessionNew(plink->icqlink);
  pchat->id=p->id;
  pchat->remote_uin=plink->remote_uin;
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

void icq_TCPOnChatReqReceived(ICQLINK *link, DWORD uin, const char *message, DWORD id)
{
#ifdef TCP_PACKET_TRACE
  printf("chat request packet received from %lu { sequence=%lx, message=%s }\n",
     uin, id, message);
#endif /* TCP_PACKET_TRACE */

  if(link->icq_RecvChatReq) {

    /* use the current system time for time received */
    time_t t=time(0);
    struct tm *ptime=localtime(&t);

    (*link->icq_RecvChatReq)(link, uin, ptime->tm_hour, ptime->tm_min,
       ptime->tm_mday, ptime->tm_mon+1, ptime->tm_year+1900, message, id);

    /* don't send an acknowledgement to the remote client!
     * GUI is responsible for sending acknowledgement once user accepts
     * or denies using icq_TCPSendChatAck */
  }
}

void icq_TCPChatUpdateFont(icq_TCPLink *plink, const char *font, WORD encoding, DWORD style, DWORD size)
{
  int packet_len, fontlen;
  char *buffer;
  if(!plink->icqlink->icq_RequestNotify)
    return;
  buffer = malloc(packet_len = (2 + (fontlen = strlen(font) + 1)) + 2 + 1 + (4+1)*2);
  buffer[0] = '\x11';
  *((DWORD *)&buffer[1]) = style;
  buffer[5] = '\x12';
  *((DWORD *)&buffer[6]) = size;
  buffer[10] = '\x10';
  *((WORD *)&buffer[11]) = fontlen;
  strcpy(&buffer[13], font);
  icq_RusConv("wk", &buffer[13]);
  *((WORD *)&buffer[13 + fontlen]) = encoding;
  if(plink->icqlink->icq_RequestNotify)
    (*plink->icqlink->icq_RequestNotify)(plink->icqlink, plink->id, ICQ_NOTIFY_CHATDATA, packet_len, buffer);
  free(buffer);
}

void icq_TCPChatUpdateColors(icq_TCPLink *plink, DWORD foreground, DWORD background)
{
  char buffer[10];

  if(!plink->icqlink->icq_RequestNotify)
    return;  
  buffer[0] = '\x00';
  *((DWORD *)&buffer[1]) = foreground;
  buffer[5] = '\x01';
  *((DWORD *)&buffer[6]) = background;
  if(plink->icqlink->icq_RequestNotify)
    (*plink->icqlink->icq_RequestNotify)(plink->icqlink, plink->id, ICQ_NOTIFY_CHATDATA,
                                         sizeof(buffer), buffer);
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
      icq_TCPChatUpdateFont(plink, font, encoding, fontstyle, fontsize);
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
      icq_TCPChatUpdateColors(plink, fg, bg);

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
      icq_TCPChatUpdateColors(plink, fg, bg);
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
        icq_TCPChatUpdateFont(plink, font, encoding, fontstyle, fontsize);
      presponse=icq_TCPCreateChatFontInfoPacket(plink);
      icq_PacketSend(presponse, plink->socket);
      icq_PacketDelete(presponse);
      /* notify app that chat connection has been established */
      icq_ChatSessionSetStatus(pchat, CHAT_STATUS_READY);
      plink->mode|=TCP_LINK_MODE_RAW;
    }
}


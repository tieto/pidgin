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

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "icqevent.h"
#include "icqpacket.h"
#include "tcplink.h"
#include "chatsession.h"
#include "filesession.h"

#include "eventhandle.h"

void icq_TCPProcessPacket2(icq_Packet *p, icq_TCPLink *tcplink)
{
  icq_MessageEvent *pevent=(icq_MessageEvent *)icq_ParsePacket(p);
  icq_Event *pbase=(icq_Event *)pevent;

  icq_Link *icqlink=tcplink->icqlink;

  if (pbase->uin != tcplink->remote_uin)
  {
    /* TODO: spoofed packet! */
  }

  pbase->handleEvent(pbase, icqlink);

  /* notify library client than the ack was received from remote client */
  if (pbase->subtype==ICQ_EVENT_ACK)
  {
    icq_FmtLog(tcplink->icqlink, ICQ_LOG_MESSAGE, "received ack %d\n", p->id);
    if(icqlink->icq_RequestNotify)
    {
      (*icqlink->icq_RequestNotify)(icqlink, pbase->id,
        ICQ_NOTIFY_ACK, pevent->status, (void *)pevent->message);
      (*icqlink->icq_RequestNotify)(icqlink, pbase->id,
        ICQ_NOTIFY_SUCCESS, 0, 0);
    }
  }    
}

void icq_HandleMessageEvent(icq_Event *pbase, icq_Link *icqlink)
{
  icq_MessageEvent *pevent=(icq_MessageEvent *)pbase;
  struct tm *ptime=localtime(&(pbase->time));

  if (pbase->subtype==ICQ_EVENT_MESSAGE && icqlink->icq_RecvMessage)
  {
    (*icqlink->icq_RecvMessage)(icqlink, pbase->uin, ptime->tm_hour,
      ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1,
      ptime->tm_year+1900, pevent->message);
    /* TODO: send ack */
  }

}

void icq_HandleURLEvent(icq_Event *pbase, icq_Link *icqlink) 
{
  icq_URLEvent *pevent=(icq_URLEvent *)pbase;
  struct tm *ptime=localtime(&(pbase->time));

  if (pbase->subtype==ICQ_EVENT_MESSAGE && icqlink->icq_RecvURL)
  {
    (*icqlink->icq_RecvURL)(icqlink, pbase->uin, ptime->tm_hour,
      ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1,
      ptime->tm_year+1900, pevent->url, pevent->message);
    /* TODO: send ack */
  }
}

void icq_HandleChatRequestEvent(icq_Event *pbase, icq_Link *icqlink)
{
  icq_ChatRequestEvent *pevent=(icq_ChatRequestEvent *)pbase;
  icq_MessageEvent *pmsgevent=(icq_MessageEvent *)pmsgevent;

  struct tm *ptime=localtime(&(pbase->time));

  switch(pbase->subtype)
  {
    case ICQ_EVENT_MESSAGE:
      if (icqlink->icq_RecvChatReq)
        (*icqlink->icq_RecvChatReq)(icqlink, pbase->uin,
          ptime->tm_hour, ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1,
          ptime->tm_year+1900, pmsgevent->message, pbase->id);
      /* don't send an ack to the remote client!  library client is
       * responsible for sending the ack once the user accepts
       * or denies the request */
      break;
    case ICQ_EVENT_ACK:
      icq_HandleChatRequestAck(pbase, icqlink);
      break;
    case ICQ_EVENT_CANCEL:
      /* TODO */
      break;
    default:
      /* TODO */
      break;
  }
}

void icq_HandleChatRequestAck(icq_Event *pbase, icq_Link *icqlink)
{
  icq_ChatRequestEvent *pevent=(icq_ChatRequestEvent *)pbase;
  icq_TCPLink *pchatlink;
  icq_ChatSession *pchat;
  icq_Packet *p2;

  /* once a chat request acknowledgement has been received, the remote
   * client opens up a listening port for us.  we need to connect to
   * this port and all chat session communication takes place over
   * this new tcp link */
  pchatlink=icq_TCPLinkNew(icqlink);
  pchatlink->type=TCP_LINK_CHAT;
  pchatlink->id=pbase->id;

  /* create a new chat session to manage the communication, and link
   * it to the tcp link */
  pchat=icq_ChatSessionNew(icqlink);
  pchat->id=pbase->id;
  pchat->remote_uin=pbase->uin;
  pchatlink->session=pchat;

  icq_ChatSessionSetStatus(pchat, CHAT_STATUS_CONNECTING);

  /* initiate the connection to the remote client's chat session 
   * port, which was specified in the ack event they sent */
  icq_TCPLinkConnect(pchatlink, pbase->uin, pevent->port);

  /* send off chat init event */
  p2=icq_TCPCreateChatInfoPacket(pchatlink, icqlink->icq_Nick, 0x00ffffff,
    0x00000000);
  icq_TCPLinkSend(pchatlink, p2);  
}


void icq_HandleFileRequestEvent(icq_Event *pbase, icq_Link *icqlink)
{
  icq_FileRequestEvent *pevent=(icq_FileRequestEvent *)pbase;
  icq_MessageEvent *pmsgevent=(icq_MessageEvent *)pmsgevent;
  struct tm *ptime=localtime(&(pbase->time));

  switch(pbase->subtype)
  {
    case ICQ_EVENT_MESSAGE:
      if (icqlink->icq_RecvFileReq)
        (*icqlink->icq_RecvFileReq)(icqlink, pbase->uin,
          ptime->tm_hour, ptime->tm_min, ptime->tm_mday, ptime->tm_mon+1,
          ptime->tm_year+1900, pmsgevent->message, pevent->filename,
          pevent->filesize, pbase->id);
      /* don't send an ack to the remote client!  library client is
       * responsible for sending the ack once the user accepts
       * or denies the request */
      break;
    case ICQ_EVENT_ACK:
      icq_HandleFileRequestAck(pbase, icqlink);
      break;
    case ICQ_EVENT_CANCEL:
      break;
    default:
      /* TODO */
      break;
  }
}

void icq_HandleFileRequestAck(icq_Event *pbase, icq_Link *icqlink)
{
  icq_FileRequestEvent *pevent=(icq_FileRequestEvent *)pbase;
  icq_TCPLink *pfilelink;
  icq_FileSession *pfile;
  icq_Packet *p2;

  /* once a file request acknowledgement has been received, the remote
   * client opens up a listening port for us.  we need to connect to
   * this port and all file transfer communication takes place over
   * this new tcp link */
  pfilelink=icq_TCPLinkNew(icqlink);
  pfilelink->type=TCP_LINK_FILE;

  /* a file session was created when the request was initially sent,
   * but it wasn't attached to a tcp link because one did not exist. 
   * find the file sesion now and link it to the new tcp link */
  pfile=icq_FindFileSession(icqlink, pbase->uin, 
    pbase->id); /* TODO: make sure find session succeeded */
  pfile->tcplink=pfilelink;
  pfilelink->id=pfile->id;
  pfilelink->session=pfile;

  /* notify the library client of the created file session */
  if (icqlink->icq_RequestNotify)
    (*icqlink->icq_RequestNotify)(icqlink, pfile->id,
      ICQ_NOTIFY_FILESESSION, sizeof(icq_FileSession), pfile);
  icq_FileSessionSetStatus(pfile, FILE_STATUS_CONNECTING);

  /* initiate the connection to the remote client's file session 
   * port, which was specified in the ack event they sent */
  icq_TCPLinkConnect(pfilelink, pbase->uin, pevent->port);

  /* send off the file transfer init event */
  /* TODO: convert file packets to events */
  p2=icq_TCPCreateFile00Packet( pfile->total_files,
    pfile->total_bytes, pfile->current_speed, icqlink->icq_Nick);
  icq_TCPLinkSend(pfilelink, p2); 
}


/*
icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown message type %d!\n", type);
icq_FmtLog(plink->icqlink, ICQ_LOG_WARNING, "unknown packet command %d!\n",
 command); 

 TODO: conversion
    strncpy(data,message,512) ;
    icq_RusConv("wk",data) ;


 TODO: ack code

    if(plink)
    {
      pack=icq_TCPCreateMessageAck(plink,0);
      icq_PacketAppend32(pack, id);
      icq_PacketSend(pack, plink->socket);
      icq_PacketDelete(pack);
    }
*/

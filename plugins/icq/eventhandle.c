/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* 
 * $Id: eventhandle.c 1442 2001-01-28 01:52:27Z warmenhoven $
 *
 * $Log$
 * Revision 1.2  2001/01/28 01:52:27  warmenhoven
 * icqlib 1.1.5
 *
 * Revision 1.3  2000/12/19 06:00:07  bills
 * moved members from ICQLINK to ICQLINK_private struct
 *
 * Revision 1.1  2000/06/15 18:50:03  bills
 * committed for safekeeping - this code will soon replace tcphandle.c 
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

void icq_TCPProcessPacket2(icq_Packet *p, icq_TCPLink *plink)
{
  icq_MessageEvent *pevent=(icq_MessageEvent *)icq_ParsePacket(p);
  icq_Event *pbase=(icq_Event *)pevent;

  ICQLINK *icqlink=plink->icqlink;

  if (pbase->uin != plink->remote_uin)
  {
    /* TODO: spoofed packet! */
  }

  pbase->handleEvent(pbase, icqlink);

  /* notify library client than the ack was received from remote client */
  if (pbase->subtype==ICQ_EVENT_ACK)
  {
    icq_FmtLog(plink->icqlink, ICQ_LOG_MESSAGE, "received ack %d\n", p->id);
    if(icqlink->icq_RequestNotify)
    {
      (*icqlink->icq_RequestNotify)(icqlink, pbase->id,
        ICQ_NOTIFY_ACK, pevent->status, (void *)pevent->message);
      (*icqlink->icq_RequestNotify)(icqlink, pbase->id,
        ICQ_NOTIFY_SUCCESS, 0, 0);
    }
  }    
}

void icq_HandleMessageEvent(icq_Event *pbase, ICQLINK *icqlink)
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

void icq_HandleURLEvent(icq_Event *pbase, ICQLINK *icqlink) 
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

void icq_HandleChatRequestEvent(icq_Event *pbase, ICQLINK *icqlink)
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

void icq_HandleChatRequestAck(icq_Event *pbase, ICQLINK *icqlink)
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


void icq_HandleFileRequestEvent(icq_Event *pbase, ICQLINK *icqlink)
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

void icq_HandleFileRequestAck(icq_Event *pbase, ICQLINK *icqlink)
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

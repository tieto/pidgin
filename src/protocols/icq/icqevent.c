/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: icqevent.c 2509 2001-10-13 00:06:18Z warmenhoven $
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

#include "icqlib.h" /* for icqbyteorder.h ?! */
#include "icqbyteorder.h"
#include "icqevent.h"

#ifdef EVENT_DEBUG
#include <string.h>
#include <stdio.h>
#endif

#define new_event(x, y)   y * x = ( y * )malloc(sizeof(y))

/* generic Event - 'header' for each tcp packet */

void icq_EventInit(icq_Event *p, int type, int subtype, unsigned long uin, 
  int version)
{
  if (!p)
    return;

  p->uin=uin;
  p->version=version;
  p->type=type;
  p->subtype=subtype;
  p->direction=ICQ_EVENT_OUTGOING;
}

icq_Packet *icq_EventCreatePacket(icq_Event *pbase)
{
  icq_Packet *p=icq_PacketNew();

  /* create header for tcp packet */
  icq_PacketAppend32(p, pbase->uin);
  icq_PacketAppend16(p, pbase->version);
  icq_PacketAppend16(p, pbase->subtype);
  icq_PacketAppend16(p, 0x0000);
  icq_PacketAppend32(p, pbase->uin);
  icq_PacketAppend32(p, pbase->type);

  return p;
}

void icq_EventParsePacket(icq_Event *pevent, icq_Packet *p)
{
  /* parse header of tcp packet */
  icq_PacketBegin(p);
  (void)icq_PacketRead32(p);             /* uin */
  pevent->version=icq_PacketRead16(p);   /* max supported tcp version */
  pevent->subtype=icq_PacketRead16(p);   /* event subtype */
  (void)icq_PacketRead16(p);             /* 0x0000 */
  pevent->uin=icq_PacketRead32(p);       /* uin */
  pevent->type=icq_PacketRead16(p);      /* event type */
}

/* Message Event - extends generic Event */

icq_MessageEvent *icq_CreateMessageEvent(int subtype, unsigned long uin, 
  const char *message) 
{
  new_event(p, icq_MessageEvent);
  icq_Event *pbase=(icq_Event *)p;
  icq_MessageEventInit(p, ICQ_TCP_MSG_MSG, subtype, uin,
    ICQ_TCP_MSG_REAL, message);

  pbase->createPacket=icq_MessageCreatePacket;

#ifdef EVENT_DEBUG
  pbase->eventName=icq_MessageEventName;
  pbase->eventDump=icq_MessageEventDump;
#endif

  return p;
}

void icq_MessageEventInit(icq_MessageEvent *p, int type, int subtype, 
  unsigned long uin, int msgtype, const char *message)
{
  icq_EventInit((icq_Event *)p, type, subtype, uin, ICQ_TCP_VER);
  p->type=msgtype;
  p->message=(char *)message;
  p->status=0; /* FIXME */
}

icq_Packet *icq_MessageCreatePacket(icq_Event *pbase, icq_TCPLink *plink)
{
  icq_MessageEvent *pevent=(icq_MessageEvent *)pbase;

  /* create header */
  icq_Packet *p=icq_EventCreatePacket(pbase);

  /* append data specific to message event */
  icq_PacketAppendString(p, (char*)pevent->message);
  icq_PacketAppend32(p, plink->socket_address.sin_addr.s_addr);
  /* FIXME: should be RealIp */
  icq_PacketAppend32(p, htonl(plink->icqlink->icq_OurIP));
  icq_PacketAppend32(p, plink->socket_address.sin_port);
  icq_PacketAppend8(p, 0x04);
  icq_PacketAppend16(p, pevent->status);
  icq_PacketAppend16(p, pevent->type);

  return p;
}

void icq_MessageParsePacket(icq_Event *pbase, icq_Packet *p)
{
  icq_MessageEvent *pevent=(icq_MessageEvent *)pbase;

  /* parse message event data from packet */
  pevent->message=(char *)icq_PacketReadString(p);    /* message text */
  (void)icq_PacketRead32(p);                  /* remote ip */
  (void)icq_PacketRead32(p);                  /* remote real ip */
  (void)icq_PacketRead32(p);                  /* remote message port */
  (void)icq_PacketRead8(p);                   /* tcp flag */
  pevent->status=icq_PacketRead16(p);         /* remote user status */
  pevent->type=icq_PacketRead16(p);           /* message type */
}

#ifdef EVENT_DEBUG
const char *icq_MessageEventName(icq_Event *p)
{
  if (p->type==ICQ_EVENT_MESSAGE)
    return "message";
  else if (p->type==ICQ_EVENT_ACK)
    return "message ack";

  return "message cancel";
}

const char *icq_MessageEventDump(icq_Event *p)
{
  static char buf[255];
  icq_MessageEvent *pevent=(icq_MessageEvent *)p;

  sprintf(buf, ", type=%x, message=\"%10s...\", status=%x",
    pevent->type, pevent->message, pevent->status);

  return buf;
}
#endif

/* URL Event - extends message Event */

icq_URLEvent *icq_CreateURLEvent(int subtype, unsigned long uin,
  const char *message, const char *url)
{
  char *str=(char *)malloc(strlen(message)+strlen(url)+2);
  icq_Event *pbase;
  icq_URLEvent *p;

  strcpy((char*)str, message);
  *(str+strlen(message))=0xFE;
  strcpy((char*)(str+strlen(message)+1), url);

  /* TODO: make sure create message event copies message */
  pbase=(icq_Event *)icq_CreateMessageEvent(subtype, uin, str);
  p=(icq_URLEvent *)pbase;

  free(str);

  *(p->message+strlen(message))=0x00;
  p->url=p->message+strlen(message)+1;

  pbase->createPacket=icq_URLCreatePacket;

#ifdef EVENT_DEBUG
  pbase->eventName=icq_URLEventName;
  pbase->eventDump=icq_URLEventDump;
#endif

  return p;

}

icq_Packet *icq_URLCreatePacket(icq_Event *pbase, icq_TCPLink *plink)
{
  icq_URLEvent *pevent=(icq_URLEvent *)pbase;
  icq_Packet *p;

  /* hack message string to include url */
  *(pevent->message+strlen(pevent->message))=0xFE;

  /* create packet */
  p=icq_MessageCreatePacket(pbase, plink);

  /* hack message string to seperate url */
  *(pevent->message+strlen(pevent->message))=0x00;

  return p;
}

void icq_URLParsePacket(icq_Event *pbase, icq_Packet *p)
{
  char *pfe;
  icq_URLEvent *pevent=(icq_URLEvent *)pbase;

  /* TODO: make sure messageparsepacket allocates message string
   *       and add a delete event function */
  icq_MessageParsePacket(pbase, p);

  /* hack message string to seperate url */
  pfe=strchr(pevent->message, '\xFE');
  *pfe=0;
  
  /* set url */
  pevent->url=pfe+1;

}

#ifdef EVENT_DEBUG
const char *icq_URLEventName(icq_Event *p)
{
  if (p->type==ICQ_EVENT_MESSAGE)
    return "url";
  else if (p->type==ICQ_EVENT_ACK)
    return "url ack";

  return "url cancel";
}

const char *icq_URLEventDump(icq_Event *p)
{
  static char buf[255];
  icq_MessageEvent *pevent=(icq_MessageEvent *)p;

  sprintf(buf, ", type=%x, message=\"%10s...\", url=\"%10s...\" status=%x",
    pevent->type, pevent->message, pevent->url, pevent->status);

  return buf;
}
#endif


/* Chat Request Event - extends Message Event */

icq_ChatRequestEvent *icq_ChatRequestEventNew(int subtype,
  unsigned long uin, const char *message, int port)
{
  new_event(p, icq_ChatRequestEvent);
  icq_Event *pbase=(icq_Event *)p;
  icq_MessageEventInit((icq_MessageEvent *)p, ICQ_TCP_MSG_CHAT, subtype,
    uin, ICQ_TCP_MSG_REAL, message);
  p->port=port;

  pbase->createPacket=icq_ChatRequestCreatePacket;

#ifdef EVENT_DEBUG
  pbase->eventName=icq_ChatRequestEventName;
  pbase->eventDump=icq_ChatRequestEventDump;
#endif

  return p;
}

icq_Packet *icq_ChatRequestCreatePacket(icq_Event *pbase, 
  icq_TCPLink *plink)
{
  icq_ChatRequestEvent *pevent=(icq_ChatRequestEvent *)pbase;

  /* create header and message data */
  icq_Packet *p=icq_MessageCreatePacket(pbase, plink);

  /* append data specific to chat event */
  icq_PacketAppendString(p, 0);
  icq_PacketAppend32(p, htonl(pevent->port));
  icq_PacketAppend32(p, htoicql(pevent->port));  

  return p;
}

void icq_ChatParsePacket(icq_Event *pbase, icq_Packet *p)
{
  icq_ChatRequestEvent *pevent=(icq_ChatRequestEvent *)pbase;
  int port;

  /* parse header and message event data */
  icq_MessageParsePacket(pbase, p);

  /* parse chat event data */
  (void)icq_PacketReadString(p);      /* null string */
  port=icq_PacketRead32(p);           /* chat listen port, network order */
  (void)icq_PacketRead32(p);          /* chat listen port, intel order */

  pevent->port=ntohl(port);
}

#ifdef EVENT_DEBUG
const char *icq_ChatRequestEventName(icq_Event *p)
{
  if (p->type==ICQ_EVENT_MESSAGE)
    return "chat request";
  else if (p->type==ICQ_EVENT_ACK) {
    icq_MessageEvent *pevent=(icq_MessageEvent *)p;
    if (pevent->status==ICQ_TCP_STATUS_REFUSE)
      return "chat request refuse";
    else
      return "chat request ack";
  } else if (p->type==ICQ_EVENT_CANCEL) 
    return "chat request cancel";

  return "unknown chat request";
}

const char *icq_ChatRequestEventDump(icq_Event *p)
{
  static char buf[255];
  static char buf2[255];
  icq_ChatRequestEvent *pevent=(icq_ChatRequestEvent *)p;

  strcpy(buf, icq_MessageEventDump(p));
  sprintf(buf2, ", port=%d", pevent->port);
  strcat(buf, buf2);

  return buf;
}
#endif

/* File Request Event - builds on Message Event */

icq_FileRequestEvent *icq_FileRequestEventNew(int subtype,
  unsigned long uin,  const char *message, const char *filename,
  unsigned long filesize)
{
  new_event(p, icq_FileRequestEvent);
  icq_Event *pbase=(icq_Event *)p;
  icq_MessageEventInit((icq_MessageEvent *)p, ICQ_TCP_MSG_FILE, subtype,
    uin, ICQ_TCP_MSG_REAL, message);
  p->filename=filename;
  p->filesize=filesize;

  pbase->createPacket=icq_FileRequestCreatePacket;

#ifdef EVENT_DEBUG
  pbase->eventName=icq_FileRequestEventName;
  pbase->eventDump=icq_FileRequestEventDump;
#endif

  return p;
}

icq_Packet *icq_FileRequestCreatePacket(icq_Event *pbase, 
  icq_TCPLink *plink)
{
  icq_FileRequestEvent *pevent=(icq_FileRequestEvent *)pbase;

  /* create header and message data */
  icq_Packet *p=icq_MessageCreatePacket(pbase, plink);

  /* append file event data */
  icq_PacketAppend32(p, htonl(pevent->port));
  icq_PacketAppendString(p, pevent->filename);
  icq_PacketAppend32(p, pevent->filesize);
  icq_PacketAppend32(p, htoicql(pevent->port));

  return p;
}

void icq_FileParsePacket(icq_Event *pbase, icq_Packet *p)
{
  icq_FileRequestEvent *pevent=(icq_FileRequestEvent *)pbase;

  /* parse header and message data */
  icq_MessageParsePacket(pbase, p);

  /* parse file event data */
  pevent->port=ntohl(icq_PacketRead32(p));  /* file listen port, network */
  pevent->filename=icq_PacketReadString(p); /* filename text */
  pevent->filesize=icq_PacketRead32(p);     /* total size */
  (void)icq_PacketRead32(p);                /* file listen port, intel */
}

#ifdef EVENT_DEBUG
const char *icq_FileRequestEventName(icq_Event *p)
{
  if (p->type==ICQ_EVENT_MESSAGE)
    return "file request";
  else if (p->type==ICQ_EVENT_ACK) {
    icq_MessageEvent *pevent=(icq_MessageEvent *)p;
    if (pevent->status==ICQ_TCP_STATUS_REFUSE)
      return "file request refuse";
    else
      return "file request ack";
  } else if (p->type==ICQ_EVENT_CANCEL) 
    return "file request cancel";

  return "unknown file request";
}

const char *icq_FileRequestEventDump(icq_Event *p)
{
  static char buf[255];
  static char buf2[255];
  icq_FileRequestEvent *pevent=(icq_FileRequestEvent *)p;

  strcpy(buf, icq_MessageEventDump(p));
  sprintf(buf2, ", port=%d, filename=\"%s\", filesize=%ld", pevent->port,
    pevent->filename, pevent->filesize);
  strcat(buf, buf2);

  return buf;
}
#endif

/* main packet parser */

icq_Event *icq_ParsePacket(icq_Packet *p)
{
  /* FIXME */
  icq_Event *pevent=(icq_Event *)malloc(sizeof(icq_FileRequestEvent));
  pevent->direction=ICQ_EVENT_INCOMING;
  pevent->time=time(0);

  icq_EventParsePacket(pevent, p);

  switch(pevent->type) {

    case ICQ_TCP_MSG_MSG:
      icq_MessageParsePacket(pevent, p);
      break;
    case ICQ_TCP_MSG_URL:
      icq_URLParsePacket(pevent, p);
      break;
    case ICQ_TCP_MSG_CHAT:
      icq_ChatParsePacket(pevent, p);
      break;
    case ICQ_TCP_MSG_FILE:
      icq_FileParsePacket(pevent, p);
      break;
    default:
      /* FIXME: log */
      free(pevent);
      pevent=0;
      break;
  }

  /* FIXME: ensure no bytes are remaining */

  return pevent;
}

#ifdef EVENT_DEBUG
const char *icq_EventDump(icq_Event *pevent)
{
  static char buf[255];

  sprintf("%s event sent to uin %ld { %s }", (pevent->eventName)(pevent),
    pevent->uin, (pevent->eventDump)(pevent) );

  return buf;
}
#endif

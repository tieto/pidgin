/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: icqevent.h 1442 2001-01-28 01:52:27Z warmenhoven $
 *
 * $Log$
 * Revision 1.2  2001/01/28 01:52:27  warmenhoven
 * icqlib 1.1.5
 *
 * Revision 1.2  2000/06/15 18:54:09  bills
 * added time attribute and handleEvent function pointer to icq_Event,
 * renamed icq_ChatEvent to icq_ChatRequestEvent, renamed icq_FileEvent to
 * icq_FileRequestEvent, added icq_URLEvent and related methods
 *
 * Revision 1.1  2000/06/15 15:27:39  bills
 * committed for safekeeping - this code will soon replace stdpackets.c and
 * greatly simplify tcphandle.c, as well as reducing code duplication in many
 * places.  it provides a much more flexible framework for managing events
 * and parsing and creating packets
 *
*/

#ifndef _ICQEVENT_H
#define _ICQEVENT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>

#include "icqpacket.h"
#include "tcplink.h"
#include "stdpackets.h"

#define EVENT_DEBUG

#define ICQ_EVENT_MESSAGE                ICQ_TCP_MESSAGE
#define ICQ_EVENT_ACK                    ICQ_TCP_ACK
#define ICQ_EVENT_CANCEL                 ICQ_TCP_CANCEL

#define ICQ_EVENT_INCOMING               1
#define ICQ_EVENT_OUTGOING               2

typedef struct icq_Event_s {

  unsigned long version;
  unsigned long id;
  unsigned long uin;

  int type;         /* chat, file, message, url */
  int subtype;      /* message, ack, or cancel */

  int direction;  
  time_t time;

  icq_Packet *(*createPacket)(struct icq_Event_s *, icq_TCPLink *);
  void (*handleEvent)(struct icq_Event_s *, ICQLINK *);

#ifdef EVENT_DEBUG
  const char *(*eventName)(struct icq_Event_s *);
  const char *(*eventDump)(struct icq_Event_s *);
#endif

} icq_Event;

typedef struct icq_MessageEvent_s {

  icq_Event event;

  char *message;  /* must be non-const for url hack */
  char *url;      /* hack so we can use same structure for url */
  int status;
  int type;

} icq_MessageEvent;

typedef struct icq_MessageEvent_s icq_URLEvent;

typedef struct icq_ChatRequestEvent_s {

  icq_MessageEvent message_event;

  int port;

} icq_ChatRequestEvent;

typedef struct icq_FileRequestEvent_s {

  icq_MessageEvent message_event;

  const char *filename;
  unsigned long filesize;
  int port;

} icq_FileRequestEvent;

/* generic event functions */
void icq_EventInit(icq_Event *p, int type, int subtype, unsigned long uin,
  int version);
icq_Packet *icq_EventCreatePacket(icq_Event *pbase);
void icq_EventParsePacket(icq_Event *pevent, icq_Packet *p);

/* message event functions */
icq_MessageEvent *icq_CreateMessageEvent(int subtype, unsigned long uin, 
  const char *message);
void icq_MessageEventInit(icq_MessageEvent *p, int type, int subtype, 
  unsigned long uin, int msgtype, const char *message);
icq_Packet *icq_MessageCreatePacket(icq_Event *pbase, icq_TCPLink *plink);
void icq_MessageParsePacket(icq_Event *pbase, icq_Packet *p);

/* url event functions */
icq_URLEvent *icq_CreateURLEvent(int subtype, unsigned long uin, 
  const char *message, const char *url);
icq_Packet *icq_URLCreatePacket(icq_Event *pbase, icq_TCPLink *plink);
void icq_URLParsePacket(icq_Event *pbase, icq_Packet *p);

/* chat request event functions */
icq_ChatRequestEvent *icq_ChatRequestEventNew(int subtype, 
  unsigned long uin, const char *message, int port);
icq_Packet *icq_ChatRequestCreatePacket(icq_Event *pbase,
  icq_TCPLink *plink);
void icq_ChatParsePacket(icq_Event *pbase, icq_Packet *p);

/* file request event functions */
icq_FileRequestEvent *icq_FileRequestEventNew(int subtype,
  unsigned long uin, const char *message, const char *filename,
  unsigned long filesize);
icq_Packet *icq_FileRequestCreatePacket(icq_Event *pbase,
  icq_TCPLink *plink);
void icq_FileParsePacket(icq_Event *pbase, icq_Packet *p);

/* main packet parser */
icq_Event *icq_ParsePacket(icq_Packet *p);

#ifdef EVENT_DEBUG
const char *icq_MessageEventName(icq_Event *p);
const char *icq_MessageEventDump(icq_Event *p);
const char *icq_URLEventName(icq_Event *p);
const char *icq_URLEventDump(icq_Event *p);
const char *icq_ChatRequestEventName(icq_Event *p);
const char *icq_ChatRequestEventDump(icq_Event *p);
const char *icq_FileRequestEventName(icq_Event *p);
const char *icq_FileRequestEventDump(icq_Event *p);
const char *icq_EventDump(icq_Event *p);
#endif

#endif /* _ICQEVENT_H */

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: eventhandle.h 1319 2000-12-19 10:08:29Z warmenhoven $
 *
 * $Log$
 * Revision 1.1  2000/12/19 10:08:29  warmenhoven
 * Yay, new icqlib
 *
 * Revision 1.1  2000/06/15 18:50:03  bills
 * committed for safekeeping - this code will soon replace tcphandle.c
 *
*/

#ifndef _EVENTHANDLE_H
#define _EVENTHANDLE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void icq_HandleMessageEvent(icq_Event *pevent, ICQLINK *icqlink);
void icq_HandleURLEvent(icq_Event *pevent, ICQLINK *icqlink);
void icq_HandleChatRequestEvent(icq_Event *base, ICQLINK *icqlink);
void icq_HandleFileRequestEvent(icq_Event *base, ICQLINK *icqlink);

void icq_HandleChatRequestAck(icq_Event *pevent, ICQLINK *icqlink);
void icq_HandleFileRequestAck(icq_Event *pevent, ICQLINK *icqlink);

#endif /* _EVENTHANDLE_H */

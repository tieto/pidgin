/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: eventhandle.h 1987 2001-06-09 14:46:51Z warmenhoven $
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

#ifndef _EVENTHANDLE_H
#define _EVENTHANDLE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void icq_HandleMessageEvent(icq_Event *pevent, icq_Link *icqlink);
void icq_HandleURLEvent(icq_Event *pevent, icq_Link *icqlink);
void icq_HandleChatRequestEvent(icq_Event *base, icq_Link *icqlink);
void icq_HandleFileRequestEvent(icq_Event *base, icq_Link *icqlink);

void icq_HandleChatRequestAck(icq_Event *pevent, icq_Link *icqlink);
void icq_HandleFileRequestAck(icq_Event *pevent, icq_Link *icqlink);

#endif /* _EVENTHANDLE_H */

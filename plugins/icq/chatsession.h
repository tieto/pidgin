/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: chatsession.h 1987 2001-06-09 14:46:51Z warmenhoven $
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

#ifndef _CHAT_SESSION_H
#define _CHAT_SESSION_H

#include "icq.h"
#include "icqtypes.h"

/* chat session states:

   accepting chat request 
	 
   1. remote user initiates chat by sending chat request to message listen 
	    port
	 2. local user accepts chat, ack packet sent back to remote user and
	    chat listen port opened
			* chat session created on local side with ID of ack packet
			  LISTENING
			* remote chat session created with ID of ack packet
			  CONNECTING
	 3. remote client connects to local chat listen port, sends hello and
	    sends info packet with name and colors
			* local chat session associated with new icq_TCPLink according to uin
	 4.	local client sends back big info packet with name, colors, and font
		  
	 5. remote client sends font packet, connection is considered established
	 
	 sending chat request
	 
	 1. local user initiates chat by sending chat request to remote message
	    listen port
	 2. remote user accepts chat, ack packet received from remote client and
	    remote client opens chat listen port
	 3. local client connects to remote chat listen port, sends hello and
	    sends info packet with name and colors
	 4. remote client sends back big info packet with name, colors, and font
	 5. local client sends font packet, connection is considered established

   1. icq_RecvChatRequest - provides session ID (same as packet sequence)
	 2. icq_SendChatAck - pass session ID
	     ICQ_NOTIFY_CONNECTED
			 ICQ_NOTIFY_SENT
			 ICQ_NOTIFY_CHAT, CHAT_STATUS_LISTENING
   3. ICQ_NOTIFY_CHAT, CHAT_STATUS_WAIT_NAME
	 4. ICQ_NOTIFY_CHAT, CHAT_STATUS_WAIT_FONT
	 5. ICQ_NOTIFY_CHAT, CHAT_STATUS_CONNECTED
	    ICQ_NOTIFY_CHATDATA, ....
			ICQ_NOTIFY_SUCCESS
*/

icq_ChatSession *icq_ChatSessionNew(icq_Link *);
void icq_ChatSessionDelete(void *);
void icq_ChatSessionSetStatus(icq_ChatSession *, int);
icq_ChatSession *icq_FindChatSession(icq_Link *, DWORD);

#endif

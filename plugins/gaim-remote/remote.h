/*
 * Remote control plugin for Gaim
 *
 * Copyright (C) 2003 Christian Hammond.
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _GAIM_REMOTE_H_
#define _GAIM_REMOTE_H_

#include <gaim-remote/remote-socket.h>


/* this is the basis of the CUI protocol. */
#define CUI_TYPE_META		1
#define CUI_TYPE_PLUGIN		2
#define CUI_TYPE_USER		3
#define CUI_TYPE_CONN		4
#define CUI_TYPE_BUDDY		5	/* BUDDY_LIST, i.e., both groups and buddies */
#define CUI_TYPE_MESSAGE	6
#define CUI_TYPE_CHAT		7
#define CUI_TYPE_REMOTE     8
					/* This is used to send commands to other UI's, 
					 * like "Open new conversation" or "send IM".
					 * Even though there's much redundancy with the
					 * other CUI_TYPES, we're better keeping this stuff
					 * separate because it's intended use is so different */

#define CUI_META_LIST		1
					/* 1 is always list; this is ignored by the core.
					   If we move to TCP this can be a keepalive */
#define CUI_META_QUIT		2
#define CUI_META_DETACH		3
					/* you don't need to send this, you can just close
					   the socket. the core will understand. */
#define CUI_META_PING       4
#define CUI_META_ACK        5

#define CUI_PLUGIN_LIST		1
#define CUI_PLUGIN_LOAD		2
#define CUI_PLUGIN_UNLOAD	3

#define CUI_USER_LIST		1
#define CUI_USER_ADD		2
#define CUI_USER_REMOVE		3
#define CUI_USER_MODIFY		4	/* this handles moving them in the list too */
#define CUI_USER_SIGNON		5
#define CUI_USER_AWAY		6
#define CUI_USER_BACK		7
#define CUI_USER_LOGOUT		8

#define CUI_CONN_LIST		1
#define CUI_CONN_PROGRESS	2
#define CUI_CONN_ONLINE		3
#define CUI_CONN_OFFLINE	4	/* this may send a "reason" for why it was killed */

#define CUI_BUDDY_LIST		1
#define CUI_BUDDY_STATE		2
					/* notifies the UI of state changes; UI can use it to
					   request the current status from the core */
#define CUI_BUDDY_ADD		3
#define CUI_BUDDY_REMOVE	4
#define CUI_BUDDY_MODIFY	5

#define CUI_MESSAGE_LIST	1	/* no idea */
#define CUI_MESSAGE_SEND	2
#define CUI_MESSAGE_RECV	3

#define CUI_CHAT_LIST		1
#define CUI_CHAT_HISTORY	2	/* is this necessary? should we have one for IMs? */
#define CUI_CHAT_JOIN		3	/* handles other people joining/parting too */
#define CUI_CHAT_PART		4
#define CUI_CHAT_SEND		5
#define CUI_CHAT_RECV		6

#define CUI_REMOTE_CONNECTIONS  2       /* Get a list of gaim_connections */
#define CUI_REMOTE_URI          3       /* Have the core handle aim:// URI's */
#define CUI_REMOTE_BLIST        4       /* Return a copy of the buddy list */
#define CUI_REMOTE_STATE        5       /* Given a buddy, return his presence. */
#define CUI_REMOTE_NEW_CONVO    6       /* Must give a user, can give an optional message */
#define CUI_REMOTE_SEND         7       /* Sends a message, a 'quiet' flag determines whether
					 * a convo window is displayed or not. */
#define CUI_REMOTE_ADD_BUDDY    8       /* Adds buddy to list */
#define CUI_REMOTE_REMOVE_BUDDY 9       /* Removes buddy from list */
#define CUI_REMOTE_JOIN_CHAT    10       /* Joins a chat. */
                              /* What else?? */

#endif /* _GAIM_REMOTE_H_ */

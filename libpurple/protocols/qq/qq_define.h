/**
 * @file qq_define.h
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef _QQ_HEADER_INFO_H_
#define _QQ_HEADER_INFO_H_

#include <glib.h>

#define QQ_UDP_HEADER_LENGTH    7
#define QQ_TCP_HEADER_LENGTH    9

#define QQ_PACKET_TAG			0x02	/* all QQ text packets starts with it */
#define QQ_PACKET_TAIL			0x03	/* all QQ text packets end with it */

#define QQ_CLIENT_0D55 0x0d55	/* QQ2005 used by openq before */
#define QQ_CLIENT_111D 0x111D	/* QQ2007 */
#define QQ_CLIENT_115B 0x115B	/* QQ2008 He Sui*/

const gchar *qq_get_ver_desc(gint source);

/* list of known QQ commands */
enum {
	QQ_CMD_LOGOUT = 0x0001,				/* log out */
	QQ_CMD_KEEP_ALIVE = 0x0002,			/* get onlines from tencent */
	QQ_CMD_UPDATE_INFO = 0x0004,			/* update information */
	QQ_CMD_SEARCH_USER = 0x0005,			/* search for user */
	QQ_CMD_GET_BUDDY_INFO = 0x0006,			/* get user information */
	QQ_CMD_ADD_BUDDY_NO_AUTH = 0x0009,		/* add buddy without auth */
	QQ_CMD_REMOVE_BUDDY = 0x000a,			/* delete a buddy  */
	QQ_CMD_ADD_BUDDY_AUTH = 0x000b,			/* buddy authentication */
	QQ_CMD_CHANGE_STATUS = 0x000d,		/* change my online status */
	QQ_CMD_ACK_SYS_MSG = 0x0012,			/* ack system message */
	QQ_CMD_SEND_IM = 0x0016,			/* send message */
	QQ_CMD_RECV_IM = 0x0017,			/* receive message */
	QQ_CMD_REMOVE_ME = 0x001c,			/* remove self */
	QQ_CMD_REQUEST_KEY = 0x001d,			/* request key for file transfer */
	QQ_CMD_CELL_PHONE_1 = 0x0021,			/* cell phone 1 */
	QQ_CMD_LOGIN = 0x0022,				/* login */
	QQ_CMD_GET_BUDDIES_LIST = 0x0026,		/* get buddies list */
	QQ_CMD_GET_BUDDIES_ONLINE = 0x0027,		/* get online buddies list */
	QQ_CMD_CELL_PHONE_2 = 0x0029,			/* cell phone 2 */
	QQ_CMD_ROOM = 0x0030,			/* room command */
	QQ_CMD_GET_BUDDIES_AND_ROOMS = 0x0058,
	QQ_CMD_GET_LEVEL = 0x005C,			/* get level for one or more buddies */
	QQ_CMD_TOKEN  = 0x0062, 		/* get login token */
	QQ_CMD_RECV_MSG_SYS = 0x0080,			/* receive a system message */
	QQ_CMD_BUDDY_CHANGE_STATUS = 0x0081,	/* buddy change status */
	/* for QQ2007*/
	QQ_CMD_GET_SERVER = 0x0091,					/* select login server */
	QQ_CMD_TOKEN_EX = 0x00BA,						/* get LOGIN token */
	QQ_CMD_CHECK_PWD = 0x00DD,				/* Password verify */
	QQ_CMD_AUTH_CODE = 0x00AE,				/* the request verification of information */
	QQ_CMD_ADD_BUDDY_NO_AUTH_EX = 0x00A7,			/* add friend without auth */
	QQ_CMD_ADD_BUDDY_AUTH_EX = 0x00A8, 				/* add buddy with auth */
	QQ_CMD_BUDDY_CHECK_CODE =  0x00B5,
	QQ_CMD_BUDDY_QUESTION =  0x00B7
};

const gchar *qq_get_cmd_desc(gint type);

enum {
	QQ_ROOM_CMD_CREATE = 0x01,
	QQ_ROOM_CMD_MEMBER_OPT = 0x02,
	QQ_ROOM_CMD_CHANGE_INFO = 0x03,
	QQ_ROOM_CMD_GET_INFO = 0x04,
	QQ_ROOM_CMD_ACTIVATE = 0x05,
	QQ_ROOM_CMD_SEARCH = 0x06,
	QQ_ROOM_CMD_JOIN = 0x07,
	QQ_ROOM_CMD_AUTH = 0x08,
	QQ_ROOM_CMD_QUIT = 0x09,
	QQ_ROOM_CMD_SEND_IM = 0x0a,
	QQ_ROOM_CMD_GET_ONLINES = 0x0b,
	QQ_ROOM_CMD_GET_BUDDIES = 0x0c,

	QQ_ROOM_CMD_CHANGE_CARD = 0x0E,
	QQ_ROOM_CMD_GET_REALNAMES = 0x0F,
	QQ_ROOM_CMD_GET_CARD = 0x10,
	QQ_ROOM_CMD_SEND_IM_EX = 0x1A,
	QQ_ROOM_CMD_ADMIN = 0x1B,
	QQ_ROOM_CMD_TRANSFER = 0x1C,
	QQ_ROOM_CMD_TEMP_CREATE = 0x30,
	QQ_ROOM_CMD_TEMP_CHANGE_MEMBER = 0x31,
	QQ_ROOM_CMD_TEMP_QUIT = 0x32,
	QQ_ROOM_CMD_TEMP_GET_INFO = 0x33,
	QQ_ROOM_CMD_TEMP_SEND_IM = 0x35,
	QQ_ROOM_CMD_TEMP_GET_MEMBERS = 0x37
};

const gchar *qq_get_room_cmd_desc(gint room_cmd);

enum {
	QQ_SERVER_BUDDY_ADDED = 1,
	QQ_SERVER_BUDDY_ADD_REQUEST = 2,
	QQ_SERVER_BUDDY_ADDED_ME = 3,
	QQ_SERVER_BUDDY_REJECTED_ME = 4,
	QQ_SERVER_NOTICE= 6,
	QQ_SERVER_NEW_CLIENT = 9,
	QQ_SERVER_BUDDY_ADDING_EX = 40,
	QQ_SERVER_BUDDY_ADD_REQUEST_EX = 41,
	QQ_SERVER_BUDDY_ADDED_ANSWER = 42,
	QQ_SERVER_BUDDY_ADDED_EX = 43
};

enum {
	QQ_BUDDY_OFFLINE = 0x00,
	QQ_BUDDY_ONLINE_NORMAL = 10,
	QQ_BUDDY_CHANGE_TO_OFFLINE = 20,
	QQ_BUDDY_ONLINE_AWAY = 30,
	QQ_BUDDY_ONLINE_INVISIBLE = 40,
	QQ_BUDDY_ONLINE_BUSY = 50
};

gboolean is_online(guint8 status);

#endif

/**
 * @file qq_define.c
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

#include "internal.h"

#include "qq_define.h"

#define QQ_CLIENT_062E 0x062e	/* GB QQ2000c build 0630 */
#define QQ_CLIENT_072E 0x072e	/* EN QQ2000c build 0305 */
#define QQ_CLIENT_0801 0x0801	/* EN QQ2000c build 0630 */
#define QQ_CLIENT_0A1D 0x0a1d	/* GB QQ2003c build 0808 */
#define QQ_CLIENT_0B07 0x0b07	/* GB QQ2003c build 0925 */
#define QQ_CLIENT_0B2F 0x0b2f	/* GB QQ2003iii build 0117 */
#define QQ_CLIENT_0B35 0x0b35	/* GB QQ2003iii build 0304 (offical release) */
#define QQ_CLIENT_0B37 0x0b37	/* GB QQ2003iii build 0304 (April 05 updates) */
#define QQ_CLIENT_0E1B 0x0e1b	/* QQ2005 ? */
#define QQ_CLIENT_0E35 0x0e35	/* EN QQ2005 V05.0.200.020 */
#define QQ_CLIENT_0F15 0x0f15	/* QQ2006 Spring Festival build */
#define QQ_CLIENT_0F5F 0x0f5f	/* QQ2006 final build */

#define QQ_CLIENT_0C0B 0x0C0B	/* QQ2004 */
#define QQ_CLIENT_0C0D 0x0C0D	/* QQ2004 preview*/
#define QQ_CLIENT_0C21 0x0C21	/* QQ2004 */
#define QQ_CLIENT_0C49 0x0C49	/* QQ2004II */
#define QQ_CLIENT_0D05 0x0D05	/* QQ2005 beta1 */
#define QQ_CLIENT_0D51 0x0D51	/* QQ2005 beta2 */
#define QQ_CLIENT_0D61 0x0D61	/* QQ2005 */
#define QQ_CLIENT_05A5 0x05A5	/* ? */
#define QQ_CLIENT_05F1 0x0F15	/* QQ2006 Spring Festival */
#define QQ_CLIENT_0F4B 0x0F4B	/* QQ2006 Beta 3  */

#define QQ_CLIENT_1105 0x1105	/* QQ2007 beta4*/
#define QQ_CLIENT_1203 0x1203	/* QQ2008 */
#define QQ_CLIENT_1205 0x1205	/* QQ2008 Qi Fu */
#define QQ_CLIENT_120B 0x120B	/* QQ2008 July 8.0.978.400 */
#define QQ_CLIENT_1412 0x1412	/* QQMac 1.0 preview1 build 670 */
#define QQ_CLIENT_1441 0x1441	/* QQ2009 preview2 */

#define QQ_SERVER_0100 0x0100	/* server */


/* given source tag, return its description accordingly */
const gchar *qq_get_ver_desc(gint source)
{
	switch (source) {
	case QQ_CLIENT_062E:
		return "GB QQ2000c build 0630";
	case QQ_CLIENT_072E:
		return "En QQ2000c build 0305";
	case QQ_CLIENT_0801:
		return "En QQ2000c build 0630";
	case QQ_CLIENT_0A1D:
		return "GB QQ2003ii build 0808";
	case QQ_CLIENT_0B07:
		return "GB QQ2003ii build 0925";
	case QQ_CLIENT_0B2F:
		return "GB QQ2003iii build 0117";
	case QQ_CLIENT_0B35:
		return "GB QQ2003iii build 0304";
	case QQ_CLIENT_0B37:
		return "GB QQ2003iii build 0304 (April 5 update)";
	case QQ_CLIENT_0C0B:
		return "QQ2004";
	case QQ_CLIENT_0C0D:
		return "QQ2004 preview";
	case QQ_CLIENT_0C21:
		return "QQ2004";
	case QQ_CLIENT_0C49:
		return "QQ2004II";
	case QQ_CLIENT_0D05:
		return "QQ2005 beta1";
	case QQ_CLIENT_0D51:
		return "QQ2005 beta2";
	case QQ_CLIENT_0D55:
	case QQ_CLIENT_0D61:
		return "QQ2005";
	case QQ_CLIENT_0E1B:
		return "QQ2005 or QQ2006";
	case QQ_CLIENT_0E35:
		return "En QQ2005 V05.0.200.020";
	case QQ_CLIENT_0F15:
		return "QQ2006 Spring Festival";
	case QQ_CLIENT_0F4B:
		return "QQ2006 beta3";
	case QQ_CLIENT_0F5F:
		return "QQ2006 final build";
	case QQ_CLIENT_1105:
		return "QQ2007 beta4";
	case QQ_CLIENT_111D:
		return "QQ2007";
	case QQ_CLIENT_115B:
	case QQ_CLIENT_1203:
	case QQ_CLIENT_1205:
	case QQ_CLIENT_120B:
		return "QQ2008";
	case QQ_CLIENT_1412:
		return "QQMac 1.0 preview1 build 670";
	case QQ_CLIENT_1441:
		return "QQ2009 preview2";
	case QQ_CLIENT_1663:
		return "QQ2009";
	case QQ_SERVER_0100:
		return "QQ Server 0100";
	default:
		return "Unknown Version";
	}
}

/* given command alias, return the command name accordingly */
const gchar *qq_get_cmd_desc(gint cmd)
{
	switch (cmd) {
	case QQ_CMD_LOGOUT:
		return "QQ_CMD_LOGOUT";
	case QQ_CMD_KEEP_ALIVE:
		return "CMD_KEEP_ALIVE";
	case QQ_CMD_UPDATE_INFO:
		return "CMD_UPDATE_INFO";
	case QQ_CMD_SEARCH_USER:
		return "CMD_SEARCH_USER";
	case QQ_CMD_GET_BUDDY_INFO:
		return "CMD_GET_BUDDY_INFO";
	case QQ_CMD_ADD_BUDDY_NO_AUTH:
		return "CMD_ADD_BUDDY_NO_AUTH";
	case QQ_CMD_REMOVE_BUDDY:
		return "CMD_REMOVE_BUDDY";
	case QQ_CMD_ADD_BUDDY_AUTH:
		return "CMD_ADD_BUDDY_AUTH";
	case QQ_CMD_CHANGE_STATUS:
		return "CMD_CHANGE_STATUS";
	case QQ_CMD_ACK_SYS_MSG:
		return "CMD_ACK_SYS_MSG";
	case QQ_CMD_SEND_IM:
		return "CMD_SEND_IM";
	case QQ_CMD_RECV_IM:
		return "CMD_RECV_IM";
	case QQ_CMD_REMOVE_ME:
		return "CMD_REMOVE_ME";
	case QQ_CMD_LOGIN:
		return "CMD_LOGIN";
	case QQ_CMD_GET_BUDDIES_LIST:
		return "CMD_GET_BUDDIES_LIST";
	case QQ_CMD_GET_BUDDIES_ONLINE:
		return "CMD_GET_BUDDIES_ONLINE";
	case QQ_CMD_ROOM:
		return "CMD_ROOM";
	case QQ_CMD_GET_BUDDIES_AND_ROOMS:
		return "CMD_GET_BUDDIES_AND_ROOMS";
	case QQ_CMD_GET_LEVEL:
		return "CMD_GET_LEVEL";
	case QQ_CMD_TOKEN:
		return "CMD_TOKEN";
	case QQ_CMD_RECV_MSG_SYS:
		return "CMD_RECV_MSG_SYS";
	case QQ_CMD_BUDDY_CHANGE_STATUS:
		return "CMD_BUDDY_CHANGE_STATUS";
	case QQ_CMD_GET_SERVER:
		return "CMD_GET_SERVER";
	case QQ_CMD_TOKEN_EX:
		return "CMD_TOKEN_EX";
	case QQ_CMD_CHECK_PWD:
		return "CMD_CHECK_PWD";
	case QQ_CMD_AUTH_CODE:
		return "CMD_AUTH_CODE";
	case QQ_CMD_ADD_BUDDY_NO_AUTH_EX:
		return "CMD_ADD_BUDDY_NO_AUTH_EX";
	case QQ_CMD_ADD_BUDDY_AUTH_EX:
		return "CMD_BUDDY_ADD_AUTH_EX";
	case QQ_CMD_BUDDY_CHECK_CODE:
		return "CMD_BUDDY_CHECK_CODE";
	case QQ_CMD_BUDDY_QUESTION:
		return "CMD_BUDDY_QUESTION";
	case QQ_CMD_BUDDY_MEMO:
		return "CMD_BUDDY_MEMO";
	default:
		return "CMD_UNKNOW";
	}
}

const gchar *qq_get_room_cmd_desc(gint room_cmd)
{
	switch (room_cmd) {
	case QQ_ROOM_CMD_CREATE:
		return "ROOM_CMD_CREATE";
	case QQ_ROOM_CMD_MEMBER_OPT:
		return "ROOM_CMD_MEMBER_OPT";
	case QQ_ROOM_CMD_CHANGE_INFO:
		return "ROOM_CMD_CHANGE_INFO";
	case QQ_ROOM_CMD_GET_INFO:
		return "ROOM_CMD_GET_INFO";
	case QQ_ROOM_CMD_ACTIVATE:
		return "ROOM_CMD_ACTIVATE";
	case QQ_ROOM_CMD_SEARCH:
		return "ROOM_CMD_SEARCH";
	case QQ_ROOM_CMD_JOIN:
		return "ROOM_CMD_JOIN";
	case QQ_ROOM_CMD_AUTH:
		return "ROOM_CMD_AUTH";
	case QQ_ROOM_CMD_QUIT:
		return "ROOM_CMD_QUIT";
	case QQ_ROOM_CMD_SEND_IM:
		return "ROOM_CMD_SEND_IM";
	case QQ_ROOM_CMD_GET_ONLINES:
		return "ROOM_CMD_GET_ONLINES";
	case QQ_ROOM_CMD_GET_BUDDIES:
		return "ROOM_CMD_GET_BUDDIES";
	case QQ_ROOM_CMD_CHANGE_CARD:
		return "ROOM_CMD_CHANGE_CARD";
	case QQ_ROOM_CMD_GET_REALNAMES:
		return "ROOM_CMD_GET_REALNAMES";
	case QQ_ROOM_CMD_GET_CARD:
		return "ROOM_CMD_GET_CARD";
	case QQ_ROOM_CMD_SEND_IM_EX:
		return "ROOM_CMD_SEND_IM_EX";
	case QQ_ROOM_CMD_ADMIN:
		return "ROOM_CMD_ADMIN";
	case QQ_ROOM_CMD_TRANSFER:
		return "ROOM_CMD_TRANSFER";
	case QQ_ROOM_CMD_TEMP_CREATE:
		return "ROOM_CMD_TEMP_CREATE";
	case QQ_ROOM_CMD_TEMP_CHANGE_MEMBER:
		return "ROOM_CMD_TEMP_CHANGE_MEMBER";
	case QQ_ROOM_CMD_TEMP_QUIT:
		return "ROOM_CMD_TEMP_QUIT";
	case QQ_ROOM_CMD_TEMP_GET_INFO:
		return "ROOM_CMD_TEMP_GET_INFO";
	case QQ_ROOM_CMD_TEMP_SEND_IM:
		return "ROOM_CMD_TEMP_SEND_IM";
	case QQ_ROOM_CMD_TEMP_GET_MEMBERS:
		return "ROOM_CMD_TEMP_GET_MEMBERS";
	default:
		return "ROOM_CMD_UNKNOW";
	}
}

/* check if status means online or offline */
gboolean is_online(guint8 status)
{
	switch(status) {
		case QQ_BUDDY_ONLINE_NORMAL:
		case QQ_BUDDY_ONLINE_AWAY:
		case QQ_BUDDY_ONLINE_INVISIBLE:
		case QQ_BUDDY_ONLINE_BUSY:
			return TRUE;
		case QQ_BUDDY_CHANGE_TO_OFFLINE:
			return FALSE;
	}
	return FALSE;
}

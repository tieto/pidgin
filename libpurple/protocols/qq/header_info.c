/**
 * @file header_info.c
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "header_info.h"

#define QQ_CLIENT_062E 0x062e	/* GB QQ2000c build 0630 */
#define QQ_CLIENT_072E 0x072e	/* EN QQ2000c build 0305 */
#define QQ_CLIENT_0801 0x0801	/* EN QQ2000c build 0630 */
#define QQ_CLIENT_0A1D 0x0a1d	/* GB QQ2003c build 0808 */
#define QQ_CLIENT_0B07 0x0b07	/* GB QQ2003c build 0925 */
#define QQ_CLIENT_0B2F 0x0b2f	/* GB QQ2003iii build 0117 */
#define QQ_CLIENT_0B35 0x0b35	/* GB QQ2003iii build 0304 (offical release) */
#define QQ_CLIENT_0B37 0x0b37	/* GB QQ2003iii build 0304 (April 05 updates) */
#define QQ_CLIENT_0E1B 0x0e1b	/* QQ2005? QQ2006? */
#define QQ_CLIENT_0F15 0x0f15	/* QQ2006 Spring Festival build */
#define QQ_CLIENT_0F5F 0x0f5f	/* QQ2006 final build */

#define QQ_SERVER_0100 0x0100	/* server */

/* given command alias, return the command name accordingly */
const gchar *qq_get_cmd_desc(gint type)
{
	switch (type) {
	case QQ_CMD_LOGOUT:
		return "QQ_CMD_LOGOUT";
	case QQ_CMD_KEEP_ALIVE:
		return "QQ_CMD_KEEP_ALIVE";
	case QQ_CMD_UPDATE_INFO:
		return "QQ_CMD_UPDATE_INFO";
	case QQ_CMD_SEARCH_USER:
		return "QQ_CMD_SEARCH_USER";
	case QQ_CMD_GET_USER_INFO:
		return "QQ_CMD_GET_USER_INFO";
	case QQ_CMD_ADD_FRIEND_WO_AUTH:
		return "QQ_CMD_ADD_FRIEND_WO_AUTH";
	case QQ_CMD_DEL_FRIEND:
		return "QQ_CMD_DEL_FRIEND";
	case QQ_CMD_BUDDY_AUTH:
		return "QQ_CMD_BUDDY_AUTH";
	case QQ_CMD_CHANGE_ONLINE_STATUS:
		return "QQ_CMD_CHANGE_ONLINE_STATUS";
	case QQ_CMD_ACK_SYS_MSG:
		return "QQ_CMD_ACK_SYS_MSG";
	case QQ_CMD_SEND_IM:
		return "QQ_CMD_SEND_IM";
	case QQ_CMD_RECV_IM:
		return "QQ_CMD_RECV_IM";
	case QQ_CMD_REMOVE_SELF:
		return "QQ_CMD_REMOVE_SELF";
	case QQ_CMD_LOGIN:
		return "QQ_CMD_LOGIN";
	case QQ_CMD_GET_FRIENDS_LIST:
		return "QQ_CMD_GET_FRIENDS_LIST";
	case QQ_CMD_GET_FRIENDS_ONLINE:
		return "QQ_CMD_GET_FRIENDS_ONLINE";
	case QQ_CMD_GROUP_CMD:
		return "QQ_CMD_GROUP_CMD";
	case QQ_CMD_GET_ALL_LIST_WITH_GROUP:
		return "QQ_CMD_GET_ALL_LIST_WITH_GROUP";
	case QQ_CMD_GET_LEVEL:
		return "QQ_CMD_GET_LEVEL";
	case QQ_CMD_REQUEST_LOGIN_TOKEN:
		return "QQ_CMD_REQUEST_LOGIN_TOKEN";
	case QQ_CMD_RECV_MSG_SYS:
		return "QQ_CMD_RECV_MSG_SYS";
	case QQ_CMD_RECV_MSG_FRIEND_CHANGE_STATUS:
		return "QQ_CMD_RECV_MSG_FRIEND_CHANGE_STATUS";
	default:
		return "UNKNOWN_TYPE";
	}
}

/* given source tag, return its description accordingly */
const gchar *qq_get_source_str(gint source)
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
	case QQ_CLIENT_0E1B:
		return "QQ2005 or QQ2006";
	case QQ_CLIENT_0F15:
		return "QQ2006 Spring Festival build";
	case QQ_CLIENT_0F5F:
		return "QQ2006 final build";
	case QQ_SERVER_0100:
		return "QQ Server 0100";
	default:
		return "QQ unknown version";
	}
}

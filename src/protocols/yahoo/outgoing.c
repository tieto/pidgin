/*
 * libyay
 *
 * Copyright (C) 2001 Eric Warmenhoven <warmenhoven@yahoo.com>
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
 *
 */

#include "internal.h"
#include <string.h>

int yahoo_write(struct yahoo_session *session, struct yahoo_conn *conn, void *buf, int len)
{
	if (!session || !conn)
		return 0;

	if (!g_list_find(session->connlist, conn))
		return 0;

	if (send(conn->socket, buf, len, 0) != len) {
		int type = conn->type;
		yahoo_close(session, conn);
		YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "error sending");
		if (type == YAHOO_CONN_TYPE_DUMB)
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
		return 0;
	}

	return len;
}

int yahoo_write_cmd(struct yahoo_session *session, struct yahoo_conn *conn,
		int service, const char *active_id, void *buf, guint msgtype)
{
	struct yahoo_packet *pkt;
	int ret;

	if (!session || !conn)
		return 0;

	if (!(pkt = g_new0(struct yahoo_packet, 1)))
		return 0;

	strcpy(pkt->version, "YPNS2.0");
	yahoo_storeint(pkt->len, sizeof(struct yahoo_packet));
	yahoo_storeint(pkt->service, service);

	yahoo_storeint(pkt->magic_id, conn->magic_id);
	yahoo_storeint(pkt->msgtype, msgtype);

	strcpy(pkt->nick1, session->login);
	strcpy(pkt->nick2, active_id);

	strncpy(pkt->content, buf, 1024);

	ret = yahoo_write(session, conn, pkt, sizeof(struct yahoo_packet));
	g_free(pkt);
	return ret;
}

int yahoo_activate_id(struct yahoo_session *session, char *id)
{
	struct yahoo_conn *conn;

	if (!session || !id)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_IDACT, id, id, 0);
}

int yahoo_deactivate_id(struct yahoo_session *session, char *id)
{
	struct yahoo_conn *conn;

	if (!session || !id)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_IDDEACT, id, id, 0);
}

int yahoo_send_message(struct yahoo_session *session, const char *active_id,
		const char *user, const char *msg)
{
	struct yahoo_conn *conn;
	char *buf;
	int ret;

	if (!session || !user || !msg)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	if (!(buf = g_strconcat(user, ",", msg, NULL)))
		return 0;

	ret = yahoo_write_cmd(session, conn, YAHOO_SERVICE_MESSAGE,
			active_id ? active_id : session->name, buf, 0);
	g_free(buf);

	return ret;
}

int yahoo_send_message_offline(struct yahoo_session *session, const char *active_id,
		const char *user, const char *msg)
{
	struct yahoo_conn *conn;
	char *buf;
	int ret;

	if (!session || !user || !msg)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	if (!(buf = g_strconcat(user, ",", msg, NULL)))
		return 0;

	ret = yahoo_write_cmd(session, conn, YAHOO_SERVICE_MESSAGE,
			active_id ? active_id : session->name, buf, YAHOO_MESSAGE_SEND);
	g_free(buf);

	return ret;
}

int yahoo_logoff(struct yahoo_session *session)
{
	struct yahoo_conn *conn;

	if (!session)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_LOGOFF, session->name, session->name, 0);
}

int yahoo_ping(struct yahoo_session *session)
{
	struct yahoo_conn *conn;

	if (!session)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_PING, session->name, "", 0);
}

int yahoo_away(struct yahoo_session *session, enum yahoo_status status, char *msg)
{
	struct yahoo_conn *conn;
	char buf[1024];

	if (!session)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	g_snprintf(buf, sizeof(buf), "%d%c%s", status, 1, msg ? msg : "---");

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_ISAWAY, session->name, buf, 0);
}

int yahoo_back(struct yahoo_session *session, enum yahoo_status status, char *msg)
{
	struct yahoo_conn *conn;
	char buf[1024];

	if (!session)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	g_snprintf(buf, sizeof(buf), "%d%c%s", status, 1, msg ? msg : "---");

	return yahoo_write_cmd(session, conn, YAHOO_SERVICE_ISBACK, session->name, buf, 0);
}

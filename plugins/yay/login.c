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

int yahoo_send_login(struct yahoo_session *session, const char *name, const char *password)
{
	char *buf = g_malloc(1024 + strlen(name) + strlen(password));
	char *a, *b;
	int at;
	struct yahoo_conn *conn;

	if (!buf)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_AUTH)))
		return 0;

	if (!(a = yahoo_urlencode(name)))
		return 0;
	if (!(b = yahoo_urlencode(password))) {
		g_free(a);
		return 0;
	}

	at = g_snprintf(buf, 1024 + strlen(name) + strlen(password),
			"GET %s%s/config/ncclogin?login=%s&passwd=%s&n=1 HTTP/1.0\r\n"
			"User-Agent: " YAHOO_USER_AGENT "\r\n"
			"Host: %s\r\n\r\n",
			session->proxy_type ? "http://" : "",
			session->proxy_type ? session->auth_host : "",
			a, b, session->auth_host);

	g_free(a);
	g_free(b);

	if (yahoo_write(session, conn, buf, at) != at) {
		g_free(buf);
		return 0;
	}

	g_free(buf);
	session->name = g_strdup(name);
	return at;
}

int yahoo_finish_logon(struct yahoo_session *session, enum yahoo_status status)
{
	char *buf = NULL, *tmp = NULL;
	int ret;
	struct yahoo_conn *conn;

	if (!session || !session->login_cookie)
		return 0;

	if (!(conn = yahoo_getconn_type(session, YAHOO_CONN_TYPE_MAIN)))
		return 0;

	if (session->identities) {
		if (!(tmp = g_strjoinv(",", session->identities)))
			return 0;
	} else
		tmp = "";

	if (!(buf = g_strconcat(session->login_cookie, "\001", session->name, tmp, NULL))) {
		g_free(tmp);
		return 0;
	}

	ret = yahoo_write_cmd(session, conn, YAHOO_SERVICE_LOGON, session->name, buf, status);
	g_free(buf);
	if (session->identities)
		g_free(tmp);

	return ret;
}

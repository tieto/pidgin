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

int yahoo_add_buddy(struct yahoo_session *session, const char *active_id,
		const char *group, const char *buddy, const char *message)
{
	struct yahoo_conn *conn;
	char *send;
	char *grp, *bdy, *id, *msg = "", *usr;

	if (!session || !active_id || !group || !buddy)
		return 0;

	if (!(grp = yahoo_urlencode(group)))
		return 0;
	if (!(bdy = yahoo_urlencode(buddy))) {
		g_free(grp);
		return 0;
	}
	if (!(id = yahoo_urlencode(active_id))) {
		g_free(grp);
		g_free(bdy);
		return 0;
	}
	if (!(usr = yahoo_urlencode(session->name))) {
		g_free(grp);
		g_free(bdy);
		g_free(id);
		return 0;
	}
	if (message && strlen(message) && !(msg = yahoo_urlencode(message))) {
		g_free(grp);
		g_free(bdy);
		g_free(id);
		g_free(usr);
		return 0;
	}

	send = g_strconcat("GET /config/set_buddygrp?.bg=", grp,
			"&.src=bl&.cmd=a&.bdl=", bdy,
			"&.id=", id,
			"&.l=", usr,
			"&.amsg=", msg,
			" HTTP/1.0\r\n",
			"User-Agent: " YAHOO_USER_AGENT "\r\n"
			"Host: " YAHOO_DATA_HOST "\r\n"
			"Cookie: ", session->cookie,
			"\r\n\r\n", NULL);
	g_free(grp);
	g_free(bdy);
	g_free(id);
	g_free(usr);
	if (message && strlen(message))
		g_free(msg);

	if (!send)
		return 0;

	if (!(conn = yahoo_new_conn(session, YAHOO_CONN_TYPE_DUMB, NULL, 0)))
		return 0;

	conn->txqueue = send;

	return 1;
}

int yahoo_remove_buddy(struct yahoo_session *session, const char *active_id,
		const char *group, const char *buddy, const char *message)
{
	struct yahoo_conn *conn;
	char *send;
	char *grp, *bdy, *id, *msg = "", *usr;

	if (!session || !active_id || !group || !buddy)
		return 0;

	if (!(grp = yahoo_urlencode(group)))
		return 0;
	if (!(bdy = yahoo_urlencode(buddy))) {
		g_free(grp);
		return 0;
	}
	if (!(id = yahoo_urlencode(active_id))) {
		g_free(grp);
		g_free(bdy);
		return 0;
	}
	if (!(usr = yahoo_urlencode(session->name))) {
		g_free(grp);
		g_free(bdy);
		g_free(id);
		return 0;
	}
	if (message && strlen(message) && !(msg = yahoo_urlencode(message))) {
		g_free(grp);
		g_free(bdy);
		g_free(id);
		g_free(usr);
		return 0;
	}

	send = g_strconcat("GET /config/set_buddygrp?.bg=", grp,
			"&.src=bl&.cmd=d&.bdl=", bdy,
			"&.id=", id,
			"&.l=", usr,
			"&.amsg=", msg,
			" HTTP/1.0\r\n",
			"User-Agent: " YAHOO_USER_AGENT "\r\n"
			"Host: " YAHOO_DATA_HOST "\r\n"
			"Cookie: ", session->cookie,
			"\r\n\r\n", NULL);
	g_free(grp);
	g_free(bdy);
	g_free(id);
	g_free(usr);
	if (message && strlen(message))
		g_free(msg);

	if (!send)
		return 0;

	if (!(conn = yahoo_new_conn(session, YAHOO_CONN_TYPE_DUMB, NULL, 0)))
		return 0;

	conn->txqueue = send;

	return 1;
}

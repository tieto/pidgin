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
#include <stdlib.h>
#include <ctype.h>

static int yahoo_parse_config(struct yahoo_session *session, struct yahoo_conn *conn, char *buf)
{
	char **str_array = g_strsplit(buf, "\n", 1024);
	char **it;
	int state = 0;

	for (it = str_array; *it; it++) {
		if (!strncmp(*it, "ERROR", strlen("ERROR"))) {
			yahoo_close(session, conn);
			CALLBACK(session, YAHOO_HANDLE_BADPASSWORD);
			return 1;
		} else if (!strncmp(*it, "Set-Cookie: ", strlen("Set-Cookie: "))) {
			char **sa;
			char **m;

			char *end = strchr(*it, ';');
			if (session->cookie)
				g_free(session->cookie);
			session->cookie = g_strndup(*it + strlen("Set-Cookie: "),
					end - *it - strlen("Set-Cookie: "));
			YAHOO_PRINT(session, YAHOO_LOG_DEBUG, session->cookie);
			if (!session->cookie) {
				yahoo_close(session, conn);
				YAHOO_PRINT(session, YAHOO_LOG_ERROR, "did not get cookie");
				CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
				return 1;
			}

			sa = g_strsplit(session->cookie, "&", 8);
			for (m = sa; *m; m++) {
				if (!strncmp(*m, "n=", 2)) {
					if (session->login_cookie)
						g_free(session->login_cookie);
					session->login_cookie = g_strdup(*m + 2);
					YAHOO_PRINT(session, YAHOO_LOG_DEBUG, session->login_cookie);
				}
			}
			g_strfreev(sa);
		} else if (!strncmp(*it, "BEGIN BUDDYLIST", strlen("BEGIN BUDDYLIST"))) {
			state = 1;
		} else if (!strncmp(*it, "END BUDDYLIST", strlen("END BUDDYLIST"))) {
			state = 0;
		} else if (!strncmp(*it, "BEGIN IGNORELIST", strlen("BEGIN IGNORELIST"))) {
			state = 2;
		} else if (!strncmp(*it, "END IGNORELIST", strlen("END IGNORELIST"))) {
			state = 0;
		} else if (!strncmp(*it, "BEGIN IDENTITIES", strlen("BEGIN IDENTITIES"))) {
			state = 3;
		} else if (!strncmp(*it, "END IDENTITIES", strlen("END IDENTITIES"))) {
			state = 0;
		} else if (!strncmp(*it, "Mail=", strlen("Mail="))) {
			session->mail = atoi(*it + strlen("Mail="));
		} else if (!strncmp(*it, "Login=", strlen("Login="))) {
			if (session->login)
				g_free(session->login);
			session->login = g_strdup(*it + strlen("Login="));
		} else {
			if (state == 1) {
				struct yahoo_group *grp = g_new0(struct yahoo_group, 1);
				char *end = strchr(*it, ':');
				grp->name = g_strndup(*it, end - *it);
				end++;
				grp->buddies = g_strsplit(end, ",", 1024);
				session->groups = g_list_append(session->groups, grp);
			} else if (state == 2) {
				session->ignored = g_list_append(session->ignored, g_strdup(*it));
			} else if (state == 3) {
				session->identities = g_strsplit(*it, ",", 6);
			}
		}
	}

	g_strfreev(str_array);
	yahoo_close(session, conn);
	CALLBACK(session, YAHOO_HANDLE_LOGINCOOKIE);
	return 0;
}

static void yahoo_parse_status(struct yahoo_session *sess, struct yahoo_packet *pkt)
{
	char *tmp = pkt->content;
	int count = 0;
	char **strs;
	int i;

	YAHOO_PRINT(sess, YAHOO_LOG_DEBUG, pkt->content);

	if (strstr(pkt->content, "was not AWAY"))
		return;

	while (*tmp && isdigit((int)*tmp))
		count = count * 10 + *tmp++ - '0';
	if (*tmp == ',')
		tmp++;
	count = count ? count : 1;

	if (count > 1)
		strs = g_strsplit(tmp, "),", count);
	else
		strs = g_strsplit(tmp, ")", count);

	for (i = 0; i < count && strs[i]; i++) {
		char **vals;
		char *end, *who;
		char **it;
		int c, j;

		who = strs[i];
		end = strchr(who, '(');
		*end++ = '\0';

		vals = g_strsplit(end, ",", 1024);

		for (it = vals, c = 0; *it; it++, c++);
		if (c > 6)
			end = g_strdup(vals[1]);
		for (j = 2; j < c - 5; j++) {
			char *x = end;
			end = g_strconcat(end, ",", vals[j], NULL);
			g_free(x);
		}

		CALLBACK(sess, YAHOO_HANDLE_STATUS, who, atoi(vals[0]), end,
				atoi(vals[c - 3]), atoi(vals[c - 2]), atoi(vals[c - 1]));

		if (c > 6)
			g_free(end);
		g_strfreev(vals);
	}

	g_strfreev(strs);
}

static void yahoo_parse_message(struct yahoo_session *sess, struct yahoo_packet *pkt)
{
	char buf[256];
	int type = yahoo_makeint(pkt->msgtype);
	char **str_array;
	switch(type) {
	case YAHOO_MESSAGE_NORMAL:
		str_array = g_strsplit(pkt->content, ",,", 2);
		CALLBACK(sess, YAHOO_HANDLE_MESSAGE, pkt->nick2, str_array[0], str_array[1]);
		g_strfreev(str_array);
		break;
	default:
		g_snprintf(buf, sizeof(buf), "unhandled message type %d: %s", type, pkt->content);
		YAHOO_PRINT(sess, YAHOO_LOG_WARNING, buf);
		break;
	}
}

static void yahoo_parse_packet(struct yahoo_session *sess,
		struct yahoo_conn *conn, struct yahoo_packet *pkt)
{
	char buf[256];
	int service = yahoo_makeint(pkt->service);
	conn->magic_id = yahoo_makeint(pkt->magic_id);
	g_snprintf(buf, sizeof(buf), "Service %d (msgtype %d)", service, yahoo_makeint(pkt->msgtype));
	YAHOO_PRINT(sess, YAHOO_LOG_DEBUG, buf);
	switch(service) {
	case YAHOO_SERVICE_LOGON:
		if (yahoo_makeint(pkt->msgtype) == 0)
			CALLBACK(sess, YAHOO_HANDLE_ONLINE);
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
		yahoo_parse_status(sess, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		if (yahoo_makeint(pkt->msgtype) == 3)
			yahoo_parse_message(sess, pkt);
		else
			yahoo_parse_status(sess, pkt);
		break;
	case YAHOO_SERVICE_IDACT:
		CALLBACK(sess, YAHOO_HANDLE_ACTIVATE, pkt->content);
		break;
	case YAHOO_SERVICE_MESSAGE:
		yahoo_parse_message(sess, pkt);
		break;
	case YAHOO_SERVICE_NEWMAIL:
		CALLBACK(sess, YAHOO_HANDLE_NEWMAIL, strlen(pkt->content) ? atoi(pkt->content) : 0);
		break;
	default:
		g_snprintf(buf, sizeof(buf), "unhandled service type %d: %s", service, pkt->content);
		YAHOO_PRINT(sess, YAHOO_LOG_WARNING, buf);
		break;
	}
}

void yahoo_socket_handler(struct yahoo_session *session, int socket, int type)
{
	int pos = 0;
	struct yahoo_conn *conn;

	if (!session)
		return;

	if (!(conn = yahoo_find_conn(session, socket)))
		return;

	if (type == YAHOO_SOCKET_WRITE) {
		int error = ETIMEDOUT, len = sizeof(error);

		if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
			error = errno;
		if (error) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "unable to connect");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
			return;
		}

		fcntl(socket, F_SETFL, 0);

		YAHOO_PRINT(session, YAHOO_LOG_NOTICE, "connected");

		if (yahoo_socket_notify)
			(*yahoo_socket_notify)(session, socket, YAHOO_SOCKET_WRITE, FALSE);
		if (yahoo_socket_notify)
			(*yahoo_socket_notify)(session, socket, YAHOO_SOCKET_READ, TRUE);

		if (conn->type == YAHOO_CONN_TYPE_AUTH) {
			CALLBACK(session, YAHOO_HANDLE_AUTHCONNECT);
		} else if (conn->type == YAHOO_CONN_TYPE_MAIN) {
			CALLBACK(session, YAHOO_HANDLE_MAINCONNECT);
		} else if (conn->type == YAHOO_CONN_TYPE_DUMB) {
			YAHOO_PRINT(session, YAHOO_LOG_DEBUG, "sending to buddy list host");
			yahoo_write(session, conn, conn->txqueue, strlen(conn->txqueue));
			g_free(conn->txqueue);
			conn->txqueue = NULL;
		}

		return;
	}

	if (conn->type == YAHOO_CONN_TYPE_AUTH) {
		char *buf = g_malloc0(5000);
		while (read(socket, &buf[pos++], 1) == 1);
		if (pos == 1) {
			g_free(buf);
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not read auth response");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
			return;
		}
		YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
		if (yahoo_parse_config(session, conn, buf)) {
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not parse auth response");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
		}
		g_free(buf);
	} else if (conn->type == YAHOO_CONN_TYPE_MAIN) {
		struct yahoo_packet pkt;
		int len;

		if ((read(socket, &pkt, 8) != 8) || strcmp(pkt.version, "YHOO1.0")) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "invalid version type");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
			return;
		}

		if (read(socket, &pkt.len, 4) != 4) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not read length");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
			return;
		}
		len = yahoo_makeint(pkt.len);

		if (read(socket, &pkt.service, len - 12) != len - 12) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not read data");
			CALLBACK(session, YAHOO_HANDLE_DISCONNECT);
			return;
		}
		yahoo_parse_packet(session, conn, &pkt);
	} else if (conn->type == YAHOO_CONN_TYPE_DUMB) {
		char *buf = g_malloc0(5000);
		while (read(socket, &buf[pos++], 1) == 1);
		if (pos == 1) {
			g_free(buf);
			YAHOO_PRINT(session, YAHOO_LOG_WARNING, "error reading from listserv");
			return;
		}
		YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
		YAHOO_PRINT(session, YAHOO_LOG_NOTICE, "closing buddy list host connnection");
		yahoo_close(session, conn);
	}
}

void yahoo_add_handler(struct yahoo_session *session, int type, yahoo_callback function)
{
	if (!session)
		return;

	session->callbacks[type].function = function;
}

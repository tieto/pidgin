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

static void yahoo_parse_config(struct yahoo_session *session, struct yahoo_conn *conn, char *buf)
{
	char **str_array = g_strsplit(buf, "\n", 1024);
	char **it;
	int state = 0;

	for (it = str_array; *it; it++) {
		if (!**it) continue;
		if (!strncmp(*it, "ERROR", strlen("ERROR"))) {
			yahoo_close(session, conn);
			if (session->callbacks[YAHOO_HANDLE_BADPASSWORD].function)
				(*session->callbacks[YAHOO_HANDLE_BADPASSWORD].function)(session);
			return;
		} else if (!strncmp(*it, "Set-Cookie: Y=", strlen("Set-Cookie: Y="))) {
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
				if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
					(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
				return;
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
				if (end) {
					grp->name = g_strndup(*it, end - *it);
					end++;
					grp->buddies = g_strsplit(end, ",", 1024);
					session->groups = g_list_append(session->groups, grp);
				}
			} else if (state == 2) {
				session->ignored = g_list_append(session->ignored, g_strdup(*it));
			} else if (state == 3) {
				session->identities = g_strsplit(*it, ",", 6);
			}
		}
	}

	g_strfreev(str_array);
	yahoo_close(session, conn);
	if (session->callbacks[YAHOO_HANDLE_LOGINCOOKIE].function)
		(*session->callbacks[YAHOO_HANDLE_LOGINCOOKIE].function)(session);
}

static void yahoo_parse_status(struct yahoo_session *sess, struct yahoo_packet *pkt)
{
	/* OK, I'm going to comment it this time. We either get:
	 * gtkobject(99,(test)\001,6634CD3,0,1,0,0)
	 * or
	 * 2,gtkobject(0,6634CD3,0,1,0,0),warmenhoven(0,6C8C0C48,0,1,0,0)
	 *
	 * in the first case, we only get one person, and we get a bunch of fields.
	 * in the second case, the number is how many people we got, and then the
	 * names followed by a bunch of fields, separated by commas.
	 *
	 * the fields are: (status, [optional: custom message \001,] session id, ?, pager, chat, game)
	 *
	 * the custom message may contain any characters (none are escaped) so we can't split on
	 * anything in particular. this is what we fucked up with the first time
	 */
	char *tmp = pkt->content;
	int count = 0;
	int i;

	if (strstr(pkt->content, "was not AWAY"))
		return;

	/* count is the first number, if we got it. */
	while (*tmp && isdigit((int)*tmp))
		count = count * 10 + *tmp++ - '0';
	count = count ? count : 1;

	for (i = 0; i < count && *tmp; i++) {
		int status, in_pager, in_chat, in_game;
		char *p, *who, *msg = NULL;

		if (*tmp == ',')
			tmp++;

		/* who is the person we're getting an update for. there will always be a paren
		 * after this so we should be OK. if we don't get a paren, we'll just break. */
		who = tmp;
		if ((tmp = strchr(who, '(')) == NULL)
			break;
		*tmp++ = '\0';

		/* tmp now points to the start of the fields. we'll get the status first. it
		 * should always be followed by a comma, and if it isn't, we'll just break. */
		if ((p = strchr(tmp, ',')) == NULL)
			break;
		*p++ = '\0';
		status = atoi(tmp);
		tmp = p;

		if (status == YAHOO_STATUS_CUSTOM) {
			/* tmp now points to the away message. it should end with "\001,". */
			msg = tmp;
			if ((tmp = strstr(msg, "\001,")) == NULL)
				break;
			*tmp = '\0';
			tmp += 2;
		}

		/* tmp now points to the session id. we don't need it. */
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		tmp++;

		/* tmp is at the unknown value. we don't need it. */
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		tmp++;

		/* tmp is at the in_pager value */
		if ((p = strchr(tmp, ',')) == NULL)
			break;
		*p++ = '\0';
		in_pager = atoi(tmp);
		tmp = p;

		/* tmp is at the in_chat value */
		if ((p = strchr(tmp, ',')) == NULL)
			break;
		*p++ = '\0';
		in_chat = atoi(tmp);
		tmp = p;

		/* tmp is at the in_game value. this is the last value, so it should end with
		 * a parenthesis. */
		if ((p = strchr(tmp, ')')) == NULL)
			break;
		*p++ = '\0';
		in_game = atoi(tmp);
		tmp = p;

		if (sess->callbacks[YAHOO_HANDLE_STATUS].function)
			(*sess->callbacks[YAHOO_HANDLE_STATUS].function)(sess, who, status, msg,
									 in_pager, in_chat, in_game);
	}
}

static void yahoo_parse_message(struct yahoo_session *sess, struct yahoo_packet *pkt)
{
	char buf[256];
	int type = yahoo_makeint(pkt->msgtype);
	char *str_array[4];
	char *tmp;

	switch(type) {
	case YAHOO_MESSAGE_NORMAL:
		str_array[0] = pkt->content;
		if ((tmp = strchr(str_array[0], ',')) == NULL)
			break;
		*tmp++ = '\0';
		str_array[1] = tmp;
		if ((tmp = strchr(str_array[1], ',')) == NULL)
			break;
		*tmp++ = '\0';
		str_array[2] = tmp;
		if (sess->callbacks[YAHOO_HANDLE_MESSAGE].function)
			(*sess->callbacks[YAHOO_HANDLE_MESSAGE].function)(sess, pkt->nick2,
									  str_array[0],
									  atol(str_array[1]),
									  str_array[2]);
		break;
	case YAHOO_MESSAGE_BOUNCE:
		if (sess->callbacks[YAHOO_HANDLE_BOUNCE].function)
			(*sess->callbacks[YAHOO_HANDLE_BOUNCE].function)(sess);
		break;
	case YAHOO_MESSAGE_OFFLINE:
		tmp = pkt->content;
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		++tmp;
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		str_array[0] = ++tmp;
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		*tmp++ = '\0';
		str_array[1] = tmp;
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		*tmp++ = '\0';
		str_array[2] = tmp;
		if ((tmp = strchr(tmp, ',')) == NULL)
			break;
		*tmp++ = '\0';
		str_array[3] = tmp;
		if (sess->callbacks[YAHOO_HANDLE_MESSAGE].function)
			(*sess->callbacks[YAHOO_HANDLE_MESSAGE].function)(sess, str_array[0],
									  str_array[1],
									  atol(str_array[2]),
									  str_array[3]);
		break;
	default:
		g_snprintf(buf, sizeof(buf), "unhandled message type %d", type);
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
	g_snprintf(buf, sizeof(buf), "Service %d (msgtype %d): %s", service,
			yahoo_makeint(pkt->msgtype), pkt->content);
	YAHOO_PRINT(sess, YAHOO_LOG_DEBUG, buf);
	switch(service) {
	case YAHOO_SERVICE_LOGON:
		if (yahoo_makeint(pkt->msgtype) == 0)
			if (sess->callbacks[YAHOO_HANDLE_ONLINE].function)
				(*sess->callbacks[YAHOO_HANDLE_ONLINE].function)(sess);
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
		yahoo_parse_status(sess, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		if (yahoo_makeint(pkt->msgtype) == 3) {
			char **str_array = g_strsplit(pkt->content, ",,", 2);
			if (sess->callbacks[YAHOO_HANDLE_BUDDYADDED].function)
				(*sess->callbacks[YAHOO_HANDLE_BUDDYADDED].function)(sess,
										     pkt->nick2,
										     str_array[0],
										     str_array[1]);
			g_strfreev(str_array);
		} else
			yahoo_parse_status(sess, pkt);
		break;
	case YAHOO_SERVICE_IDACT:
		if (sess->callbacks[YAHOO_HANDLE_ACTIVATE].function)
			(*sess->callbacks[YAHOO_HANDLE_ACTIVATE].function)(sess);
		break;
	case YAHOO_SERVICE_MESSAGE:
		yahoo_parse_message(sess, pkt);
		break;
	case YAHOO_SERVICE_NEWMAIL:
		if (sess->callbacks[YAHOO_HANDLE_NEWMAIL].function)
			(*sess->callbacks[YAHOO_HANDLE_NEWMAIL].function)(sess, strlen(pkt->content) ?
									  atoi(pkt->content) : 0);
		break;
	default:
		g_snprintf(buf, sizeof(buf), "unhandled service type %d", service);
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
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}

		fcntl(socket, F_SETFL, 0);

		YAHOO_PRINT(session, YAHOO_LOG_NOTICE, "connected");

		conn->connected = TRUE;
		if (yahoo_socket_notify)
			(*yahoo_socket_notify)(session, socket, YAHOO_SOCKET_WRITE, FALSE);
		if (yahoo_socket_notify)
			(*yahoo_socket_notify)(session, socket, YAHOO_SOCKET_READ, TRUE);

		if (conn->type == YAHOO_CONN_TYPE_AUTH) {
			if (session->callbacks[YAHOO_HANDLE_AUTHCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_AUTHCONNECT].function)(session);
		} else if (conn->type == YAHOO_CONN_TYPE_MAIN) {
			if (session->callbacks[YAHOO_HANDLE_MAINCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_MAINCONNECT].function)(session);
		} else if (conn->type == YAHOO_CONN_TYPE_DUMB) {
			YAHOO_PRINT(session, YAHOO_LOG_DEBUG, "sending to buddy list host");
			yahoo_write(session, conn, conn->txqueue, strlen(conn->txqueue));
			g_free(conn->txqueue);
			conn->txqueue = NULL;
		} else if (conn->type == YAHOO_CONN_TYPE_PROXY) {
			char buf[1024];
			g_snprintf(buf, sizeof(buf), "CONNECT %s:%d HTTP/1.1\r\n\r\n",
					session->pager_host, session->pager_port);
			YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
			yahoo_write(session, conn, buf, strlen(buf));
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
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}
		YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
		yahoo_parse_config(session, conn, buf);
		g_free(buf);
	} else if (conn->type == YAHOO_CONN_TYPE_MAIN) {
		struct yahoo_packet pkt;
		int len;

		if ((read(socket, &pkt, 8) != 8) || strcmp(pkt.version, "YHOO1.0")) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "invalid version type");
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}

		if (read(socket, &pkt.len, 4) != 4) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not read length");
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}
		len = yahoo_makeint(pkt.len);

		if (read(socket, &pkt.service, len - 12) != len - 12) {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_CRITICAL, "could not read data");
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}
		yahoo_parse_packet(session, conn, &pkt);
	} else if (conn->type == YAHOO_CONN_TYPE_DUMB) {
		char *buf = g_malloc0(5000);
		while (read(socket, &buf[pos++], 1) == 1);
		if (pos == 1) {
			g_free(buf);
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_WARNING, "error reading from listserv");
			return;
		}
		YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
		YAHOO_PRINT(session, YAHOO_LOG_NOTICE, "closing buddy list host connnection");
		yahoo_close(session, conn);
		g_free(buf);
	} else if (conn->type == YAHOO_CONN_TYPE_PROXY) {
		char *buf = g_malloc0(5000);
		int nlc = 0;
		while ((nlc != 2) && (read(socket, &buf[pos++], 1) == 1)) {
			if (buf[pos-1] == '\n')
				nlc++;
			else if (buf[pos-1] != '\r')
				nlc = 0;
		}
		if (pos == 1) {
			g_free(buf);
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_ERROR, "error reading from proxy");
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
			return;
		}
		YAHOO_PRINT(session, YAHOO_LOG_DEBUG, buf);
		if (!strncasecmp(buf, HTTP_GOODSTRING1, strlen(HTTP_GOODSTRING1)) ||
		    !strncasecmp(buf, HTTP_GOODSTRING2, strlen(HTTP_GOODSTRING2))) {
			conn->type = YAHOO_CONN_TYPE_MAIN;
			YAHOO_PRINT(session, YAHOO_LOG_NOTICE, "proxy connected successfully");
			if (session->callbacks[YAHOO_HANDLE_MAINCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_MAINCONNECT].function)(session);
		} else {
			yahoo_close(session, conn);
			YAHOO_PRINT(session, YAHOO_LOG_ERROR, "proxy could not connect");
			if (session->callbacks[YAHOO_HANDLE_DISCONNECT].function)
				(*session->callbacks[YAHOO_HANDLE_DISCONNECT].function)(session);
		}
		g_free(buf);
	}
}

void yahoo_add_handler(struct yahoo_session *session, int type, yahoo_callback function)
{
	if (!session)
		return;

	session->callbacks[type].function = function;
}

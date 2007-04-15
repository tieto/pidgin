/**
 * @file directconn.c MSN direct connection functions
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
#include "msn.h"
#include "directconn.h"

#include "slp.h"
#include "slpmsg.h"

/**************************************************************************
 * Directconn Specific
 **************************************************************************/

void
msn_directconn_send_handshake(MsnDirectConn *directconn)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;

	g_return_if_fail(directconn != NULL);

	slplink = directconn->slplink;

	slpmsg = msn_slpmsg_new(slplink);
	slpmsg->flags = 0x100;

	if (directconn->nonce != NULL)
	{
		guint32 t1;
		guint16 t2;
		guint16 t3;
		guint16 t4;
		guint64 t5;

		sscanf (directconn->nonce, "%08X-%04hX-%04hX-%04hX-%012" G_GINT64_MODIFIER "X", &t1, &t2, &t3, &t4, &t5);

		t1 = GUINT32_TO_LE(t1);
		t2 = GUINT16_TO_LE(t2);
		t3 = GUINT16_TO_LE(t3);
		t4 = GUINT16_TO_BE(t4);
		t5 = GUINT64_TO_BE(t5);

		slpmsg->ack_id     = t1;
		slpmsg->ack_sub_id = t2 | (t3 << 16);
		slpmsg->ack_size   = t4 | t5;
	}

	g_free(directconn->nonce);

	msn_slplink_send_slpmsg(slplink, slpmsg);

	directconn->acked =TRUE;
}

/**************************************************************************
 * Connection Functions
 **************************************************************************/

#if 0
static int
create_listener(int port)
{
	int fd;
	const int on = 1;

#if 0
	struct addrinfo hints;
	struct addrinfo *c, *res;
	char port_str[5];

	snprintf(port_str, sizeof(port_str), "%d", port);

	memset(&hints, 0, sizeof(hints));

	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(NULL, port_str, &hints, &res) != 0)
	{
		gaim_debug_error("msn", "Could not get address info: %s.\n",
						 port_str);
		return -1;
	}

	for (c = res; c != NULL; c = c->ai_next)
	{
		fd = socket(c->ai_family, c->ai_socktype, c->ai_protocol);

		if (fd < 0)
			continue;

		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		if (bind(fd, c->ai_addr, c->ai_addrlen) == 0)
			break;

		close(fd);
	}

	if (c == NULL)
	{
		gaim_debug_error("msn", "Could not find socket: %s.\n", port_str);
		return -1;
	}

	freeaddrinfo(res);
#else
	struct sockaddr_in sockin;

	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0)
		return -1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0)
	{
		close(fd);
		return -1;
	}

	memset(&sockin, 0, sizeof(struct sockaddr_in));
	sockin.sin_family = AF_INET;
	sockin.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *)&sockin, sizeof(struct sockaddr_in)) != 0)
	{
		close(fd);
		return -1;
	}
#endif

	if (listen (fd, 4) != 0)
	{
		close (fd);
		return -1;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	return fd;
}
#endif

static size_t
msn_directconn_write(MsnDirectConn *directconn,
					 const char *data, size_t len)
{
	char *buffer, *tmp;
	size_t buf_size;
	size_t ret;
	guint32 sent_len;

	g_return_val_if_fail(directconn != NULL, 0);

	buf_size = len + 4;
	buffer = tmp = g_malloc(buf_size);

	sent_len = GUINT32_TO_LE(len);

	memcpy(tmp, &sent_len, 4);
	tmp += 4;
	memcpy(tmp, data, len);
	tmp += len;

	ret = write(directconn->fd, buffer, buf_size);

#ifdef DEBUG_DC
	char *str;
	str = g_strdup_printf("%s/msntest/w%.4d.bin", g_get_home_dir(), directconn->c);

	FILE *tf = g_fopen(str, "w");
	fwrite(buffer, 1, buf_size, tf);
	fclose(tf);

	g_free(str);
#endif

	g_free(buffer);

#if 0
	/* Let's write the length of the data. */
	ret = write(directconn->fd, &len, sizeof(len));

	/* Let's write the data. */
	ret = write(directconn->fd, data, len);

	char *str;
	str = g_strdup_printf("/home/revo/msntest/w%.4d.bin", directconn->c);

	FILE *tf = g_fopen(str, "w");
	fwrite(&len, 1, sizeof(len), tf);
	fwrite(data, 1, len, tf);
	fclose(tf);

	g_free(str);
#endif

	directconn->c++;

	return ret;
}

#if 0
void
msn_directconn_parse_nonce(MsnDirectConn *directconn, const char *nonce)
{
	guint32 t1;
	guint16 t2;
	guint16 t3;
	guint16 t4;
	guint64 t5;

	g_return_if_fail(directconn != NULL);
	g_return_if_fail(nonce      != NULL);

	sscanf (nonce, "%08X-%04hX-%04hX-%04hX-%012llX", &t1, &t2, &t3, &t4, &t5);

	t1 = GUINT32_TO_LE(t1);
	t2 = GUINT16_TO_LE(t2);
	t3 = GUINT16_TO_LE(t3);
	t4 = GUINT16_TO_BE(t4);
	t5 = GUINT64_TO_BE(t5);

	directconn->slpheader = g_new0(MsnSlpHeader, 1);

	directconn->slpheader->ack_id     = t1;
	directconn->slpheader->ack_sub_id = t2 | (t3 << 16);
	directconn->slpheader->ack_size   = t4 | t5;
}
#endif

void
msn_directconn_send_msg(MsnDirectConn *directconn, MsnMessage *msg)
{
	char *body;
	size_t body_len;

	body = msn_message_gen_slp_body(msg, &body_len);

	msn_directconn_write(directconn, body, body_len);
}

static void
msn_directconn_process_msg(MsnDirectConn *directconn, MsnMessage *msg)
{
	gaim_debug_info("msn", "directconn: process_msg\n");

	msn_slplink_process_msg(directconn->slplink, msg);
}

static void
read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnDirectConn* directconn;
	char *body;
	size_t len, body_len;

	gaim_debug_info("msn", "read_cb: %d, %d\n", source, cond);

	directconn = data;

	/* Let's read the length of the data. */
	len = read(directconn->fd, &body_len, sizeof(body_len));

	if (len <= 0)
	{
		/* ERROR */
		gaim_debug_error("msn", "error reading\n");

		msn_directconn_destroy(directconn);

		return;
	}

	body_len = GUINT32_FROM_LE(body_len);

	gaim_debug_info("msn", "body_len=%d\n", body_len);

	if (body_len <= 0)
	{
		/* ERROR */
		gaim_debug_error("msn", "error reading\n");

		msn_directconn_destroy(directconn);

		return;
	}

	body = g_try_malloc(body_len);

	if (body != NULL)
	{
		/* Let's read the data. */
		len = read(directconn->fd, body, body_len);

		gaim_debug_info("msn", "len=%d\n", len);
	}
	else
	{
		gaim_debug_error("msn", "Failed to allocate memory for read\n");
		len = 0;
	}

	if (len > 0)
	{
		MsnMessage *msg;

#ifdef DEBUG_DC
		str = g_strdup_printf("/home/revo/msntest/r%.4d.bin", directconn->c);

		FILE *tf = g_fopen(str, "w");
		fwrite(body, 1, len, tf);
		fclose(tf);

		g_free(str);
#endif

		directconn->c++;

		msg = msn_message_new_msnslp();
		msn_message_parse_slp_body(msg, body, body_len);

		msn_directconn_process_msg(directconn, msg);
	}
	else
	{
		/* ERROR */
		gaim_debug_error("msn", "error reading\n");

		msn_directconn_destroy(directconn);
	}
}

static void
connect_cb(gpointer data, gint source, const gchar *error_message)
{
	MsnDirectConn* directconn;
	int fd;

	gaim_debug_misc("msn", "directconn: connect_cb: %d\n", source);

	directconn = data;
	directconn->connect_data = NULL;

	if (TRUE)
	{
		fd = source;
	}
	else
	{
		struct sockaddr_in client_addr;
		unsigned int client;
		fd = accept (source, (struct sockaddr *)&client_addr, &client);
	}

	directconn->fd = fd;

	if (fd > 0)
	{
		directconn->inpa = gaim_input_add(fd, GAIM_INPUT_READ, read_cb,
										  directconn);

		if (TRUE)
		{
			/* Send foo. */
			msn_directconn_write(directconn, "foo", strlen("foo") + 1);

			/* Send Handshake */
			msn_directconn_send_handshake(directconn);
		}
		else
		{
		}
	}
	else
	{
		/* ERROR */
		gaim_debug_error("msn", "could not add input\n");

		if (directconn->inpa)
			gaim_input_remove(directconn->inpa);

		close(directconn->fd);
	}
}

gboolean
msn_directconn_connect(MsnDirectConn *directconn, const char *host, int port)
{
	MsnSession *session;

	g_return_val_if_fail(directconn != NULL, FALSE);
	g_return_val_if_fail(host       != NULL, TRUE);
	g_return_val_if_fail(port        > 0,    FALSE);

	session = directconn->slplink->session;

#if 0
	if (session->http_method)
	{
		servconn->http_data->gateway_host = g_strdup(host);
	}
#endif

	directconn->connect_data = gaim_proxy_connect(NULL, session->account,
			host, port, connect_cb, directconn);

	if (directconn->connect_data != NULL)
	{
		return TRUE;
	}
	else
		return FALSE;
}

#if 0
void
msn_directconn_listen(MsnDirectConn *directconn)
{
	int port;
	int fd;

	port = 7000;

	for (fd = -1; fd < 0;)
		fd = create_listener(++port);

	directconn->fd = fd;

	directconn->inpa = gaim_input_add(fd, GAIM_INPUT_READ, connect_cb,
		directconn);

	directconn->port = port;
	directconn->c = 0;
}
#endif

MsnDirectConn*
msn_directconn_new(MsnSlpLink *slplink)
{
	MsnDirectConn *directconn;

	directconn = g_new0(MsnDirectConn, 1);

	directconn->slplink = slplink;

	if (slplink->directconn != NULL)
		gaim_debug_info("msn", "got_transresp: LEAK\n");

	slplink->directconn = directconn;

	return directconn;
}

void
msn_directconn_destroy(MsnDirectConn *directconn)
{
	if (directconn->connect_data != NULL)
		gaim_proxy_connect_cancel(directconn->connect_data);

	if (directconn->inpa != 0)
		gaim_input_remove(directconn->inpa);

	if (directconn->fd >= 0)
		close(directconn->fd);

	if (directconn->nonce != NULL)
		g_free(directconn->nonce);

	directconn->slplink->directconn = NULL;

	g_free(directconn);
}

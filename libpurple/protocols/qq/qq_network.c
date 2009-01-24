/**
 * @file qq_network.c
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

#include "cipher.h"
#include "debug.h"
#include "internal.h"

#include "buddy_info.h"
#include "group_info.h"
#include "group_internal.h"
#include "qq_crypt.h"
#include "qq_define.h"
#include "qq_base.h"
#include "buddy_list.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_trans.h"
#include "utils.h"
#include "qq_process.h"

#define QQ_DEFAULT_PORT					8000

/* set QQ_CONNECT_MAX to 1, when test reconnecting */
#define QQ_CONNECT_MAX						3
#define QQ_CONNECT_INTERVAL			2
#define QQ_CONNECT_CHECK					5
#define QQ_KEEP_ALIVE_INTERVAL		60
#define QQ_TRANS_INTERVAL				10

gboolean connect_to_server(PurpleConnection *gc, gchar *server, gint port);

static qq_connection *connection_find(qq_data *qd, int fd) {
	qq_connection *ret = NULL;
	GSList *entry = qd->openconns;
	while(entry) {
		ret = entry->data;
		if(ret->fd == fd) return ret;
		entry = entry->next;
	}
	return NULL;
}

static qq_connection *connection_create(qq_data *qd, int fd) {
	qq_connection *ret = g_new0(qq_connection, 1);
	ret->fd = fd;
	qd->openconns = g_slist_append(qd->openconns, ret);
	return ret;
}

static void connection_remove(qq_data *qd, int fd) {
	qq_connection *conn = connection_find(qd, fd);
	qd->openconns = g_slist_remove(qd->openconns, conn);

	g_return_if_fail( conn != NULL );

	purple_debug_info("QQ", "Close socket %d\n", conn->fd);
	if(conn->input_handler > 0)	purple_input_remove(conn->input_handler);
	if(conn->can_write_handler > 0)	purple_input_remove(conn->can_write_handler);

	if (conn->fd >= 0)	close(conn->fd);
	if(conn->tcp_txbuf != NULL) 	purple_circ_buffer_destroy(conn->tcp_txbuf);
	if (conn->tcp_rxqueue != NULL)	g_free(conn->tcp_rxqueue);

	g_free(conn);
}

static void connection_free_all(qq_data *qd) {
	qq_connection *ret = NULL;
	GSList *entry = qd->openconns;
	while(entry) {
		ret = entry->data;
		connection_remove(qd, ret->fd);
		entry = qd->openconns;
	}
}
static gboolean set_new_server(qq_data *qd)
{
	gint count;
	gint index;
	GList *it = NULL;

 	g_return_val_if_fail(qd != NULL, FALSE);

	if (qd->servers == NULL) {
		purple_debug_info("QQ", "Server list is NULL\n");
		return FALSE;
	}

	/* remove server used before */
	if (qd->curr_server != NULL) {
		purple_debug_info("QQ",
			"Remove current [%s] from server list\n", qd->curr_server);
   		qd->servers = g_list_remove(qd->servers, qd->curr_server);
   		qd->curr_server = NULL;
    }

	count = g_list_length(qd->servers);
	purple_debug_info("QQ", "Server list has %d\n", count);
	if (count <= 0) {
		/* no server left, disconnect when result is false */
		qd->servers = NULL;
		return FALSE;
	}

	/* get new server */
	index  = rand() % count;
	it = g_list_nth(qd->servers, index);
	qd->curr_server = it->data;		/* do not free server_name */
	if (qd->curr_server == NULL || strlen(qd->curr_server) <= 0 ) {
		purple_debug_info("QQ", "Server name at %d is empty\n", index);
		return FALSE;
	}

	purple_debug_info("QQ", "set new server to %s\n", qd->curr_server);
	return TRUE;
}

static gint packet_get_header(guint8 *header_tag,  guint16 *source_tag,
	guint16 *cmd, guint16 *seq, guint8 *buf)
{
	gint bytes = 0;
	bytes += qq_get8(header_tag, buf + bytes);
	bytes += qq_get16(source_tag, buf + bytes);
	bytes += qq_get16(cmd, buf + bytes);
	bytes += qq_get16(seq, buf + bytes);
	return bytes;
}

static gboolean connect_check(gpointer data)
{
	PurpleConnection *gc = (PurpleConnection *) data;
	qq_data *qd;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, FALSE);
	qd = (qq_data *) gc->proto_data;

	if (qd->connect_watcher > 0) {
		purple_timeout_remove(qd->connect_watcher);
		qd->connect_watcher = 0;
	}

	if (qd->fd >= 0 && qd->ld.token != NULL && qd->ld.token_len > 0) {
		purple_debug_info("QQ", "Connect ok\n");
		return FALSE;
	}

	qd->connect_watcher = purple_timeout_add_seconds(0, qq_connect_later, gc);
	return FALSE;
}

/* Warning: qq_connect_later destory all connection
 *  Any function should be care of use qq_data after call this function
 *  Please conside tcp_pending and udp_pending */
gboolean qq_connect_later(gpointer data)
{
	PurpleConnection *gc;
	char *tmp_server;
	int port;
	gchar **segments;
	qq_data *qd;

	gc = (PurpleConnection *) data;
	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, FALSE);
	qd = (qq_data *) gc->proto_data;
	tmp_server = NULL;

	if (qd->check_watcher > 0) {
		purple_timeout_remove(qd->check_watcher);
		qd->check_watcher = 0;
	}
	qq_disconnect(gc);

	if (qd->redirect_ip.s_addr != 0) {
		/* redirect to new server */
		tmp_server = g_strdup_printf("%s:%d", inet_ntoa(qd->redirect_ip), qd->redirect_port);
		qd->servers = g_list_append(qd->servers, tmp_server);

		qd->curr_server = tmp_server;
		tmp_server = NULL;

		qd->redirect_ip.s_addr = 0;
		qd->redirect_port = 0;
		qd->connect_retry = QQ_CONNECT_MAX;
	}

	if (qd->curr_server == NULL || strlen (qd->curr_server) == 0 || qd->connect_retry <= 0) {
		if ( set_new_server(qd) != TRUE) {
			purple_connection_error_reason(gc,
					PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Unable to connect."));
			return FALSE;
		}
		qd->connect_retry = QQ_CONNECT_MAX;
	}

	segments = g_strsplit(qd->curr_server, ":", 0);
	tmp_server = g_strdup(segments[0]);
	if (NULL != segments[1]) {
		port = atoi(segments[1]);
		if (port <= 0) {
			purple_debug_info("QQ", "Port not define in %s, use default.\n", qd->curr_server);
			port = QQ_DEFAULT_PORT;
		}
	} else {
		purple_debug_info("QQ", "Error splitting server string: %s, setting port to default.\n", qd->curr_server);
		port = QQ_DEFAULT_PORT;
	}

	g_strfreev(segments);

	qd->connect_retry--;
	if ( !connect_to_server(gc, tmp_server, port) ) {
			purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to connect."));
	}

	g_free(tmp_server);
	tmp_server = NULL;

	qd->check_watcher = purple_timeout_add_seconds(QQ_CONNECT_CHECK, connect_check, gc);
	return FALSE;	/* timeout callback stops */
}

static void redirect_server(PurpleConnection *gc)
{
	qq_data *qd;
	qd = (qq_data *) gc->proto_data;

	if (qd->check_watcher > 0) {
			purple_timeout_remove(qd->check_watcher);
			qd->check_watcher = 0;
	}
	if (qd->connect_watcher > 0)	purple_timeout_remove(qd->connect_watcher);
	qd->connect_watcher = purple_timeout_add_seconds(QQ_CONNECT_INTERVAL, qq_connect_later, gc);
}

/* process the incoming packet from qq_pending */
static gboolean packet_process(PurpleConnection *gc, guint8 *buf, gint buf_len)
{
	qq_data *qd;
	gint bytes, bytes_not_read;

	guint8 header_tag;
	guint16 source_tag;
	guint16 cmd;
	guint16 seq;		/* May be ack_seq or send_seq, depends on cmd */
	guint8 room_cmd;
	guint32 room_id;
	gint update_class;
	guint32 ship32;
	int ret;

	qq_transaction *trans;

	g_return_val_if_fail(buf != NULL && buf_len > 0, TRUE);

	qd = (qq_data *) gc->proto_data;

	qd->net_stat.rcved++;
	if (qd->net_stat.rcved <= 0)	memset(&(qd->net_stat), 0, sizeof(qd->net_stat));

	/* Len, header and tail tag have been checked before */
	bytes = 0;
	bytes += packet_get_header(&header_tag, &source_tag, &cmd, &seq, buf + bytes);

#if 1
		purple_debug_info("QQ", "==> [%05d] %s 0x%04X, source tag 0x%04X len %d\n",
				seq, qq_get_cmd_desc(cmd), cmd, source_tag, buf_len);
#endif
	/* this is the length of all the encrypted data (also remove tail tag) */
	bytes_not_read = buf_len - bytes - 1;

	/* ack packet, we need to update send tranactions */
	/* we do not check duplication for server ack */
	trans = qq_trans_find_rcved(gc, cmd, seq);
	if (trans == NULL) {
		/* new server command */
		if ( !qd->is_login ) {
			qq_trans_add_remain(gc, cmd, seq, buf + bytes, bytes_not_read);
		} else {
			qq_trans_add_server_cmd(gc, cmd, seq, buf + bytes, bytes_not_read);
			qq_proc_server_cmd(gc, cmd, seq, buf + bytes, bytes_not_read);
		}
		return TRUE;
	}

	if (qq_trans_is_dup(trans)) {
		qd->net_stat.rcved_dup++;
		purple_debug_info("QQ", "dup [%05d] %s, discard...\n", seq, qq_get_cmd_desc(cmd));
		return TRUE;
	}

	update_class = qq_trans_get_class(trans);
	ship32 = qq_trans_get_ship(trans);
	if (update_class != 0 || ship32 != 0) {
		purple_debug_info("QQ", "Update class %d, ship32 %d\n", update_class, ship32);
	}

	switch (cmd) {
		case QQ_CMD_TOKEN:
		case QQ_CMD_GET_SERVER:
		case QQ_CMD_TOKEN_EX:
		case QQ_CMD_CHECK_PWD:
		case QQ_CMD_LOGIN:
			ret = qq_proc_login_cmds(gc, cmd, seq, buf + bytes, bytes_not_read, update_class, ship32);
			if (ret != QQ_LOGIN_REPLY_OK) {
				if (ret == QQ_LOGIN_REPLY_REDIRECT) {
					redirect_server(gc);
				}
				return FALSE;	/* do nothing after this function and return now */
			}
			break;
		case QQ_CMD_ROOM:
			room_cmd = qq_trans_get_room_cmd(trans);
			room_id = qq_trans_get_room_id(trans);
			qq_proc_room_cmds(gc, seq, room_cmd, room_id, buf + bytes, bytes_not_read, update_class, ship32);
			break;
		default:
			qq_proc_client_cmds(gc, cmd, seq, buf + bytes, bytes_not_read, update_class, ship32);
			break;
	}

	return TRUE;
}

static void tcp_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = (PurpleConnection *) data;
	qq_data *qd;
	qq_connection *conn;
	guint8 buf[1024];		/* set to 16 when test  tcp_rxqueue */
	gint buf_len;
	gint bytes;

	guint8 *pkt;
	guint16 pkt_len;

	gchar *error_msg;
	guint8 *jump;
	gint jump_len;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	if(cond != PURPLE_INPUT_READ) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Socket error"));
		return;
	}

	conn = connection_find(qd, source);
	g_return_if_fail(conn != NULL);

	/* test code, not using tcp_rxqueue
	memset(pkt,0, sizeof(pkt));
	buf_len = read(qd->fd, pkt, sizeof(pkt));
	if (buf_len > 2) {
		packet_process(gc, pkt + 2, buf_len - 2);
	}
	return;
	*/

	buf_len = read(source, buf, sizeof(buf));
	if (buf_len < 0) {
		if (errno == EAGAIN)
			/* No worries */
			return;

		error_msg = g_strdup_printf(_("Lost connection with server:\n%s"), g_strerror(errno));
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				error_msg);
		g_free(error_msg);
		return;
	} else if (buf_len == 0) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server closed the connection."));
		return;
	}

	/* keep alive will be sent in 30 seconds since last_receive
	 *  QQ need a keep alive packet in every 60 seconds
	 gc->last_received = time(NULL);
	*/
	/* purple_debug_info("TCP_PENDING", "Read %d bytes, rxlen is %d\n", buf_len, conn->tcp_rxlen); */
	conn->tcp_rxqueue = g_realloc(conn->tcp_rxqueue, buf_len + conn->tcp_rxlen);
	memcpy(conn->tcp_rxqueue + conn->tcp_rxlen, buf, buf_len);
	conn->tcp_rxlen += buf_len;

	pkt = g_newa(guint8, MAX_PACKET_SIZE);
	while (PURPLE_CONNECTION_IS_VALID(gc)) {
		if (qd->openconns == NULL) {
			break;
		}
		if (conn->tcp_rxqueue == NULL) {
			conn->tcp_rxlen = 0;
			break;
		}
		if (conn->tcp_rxlen < QQ_TCP_HEADER_LENGTH) {
			break;
		}

		bytes = 0;
		bytes += qq_get16(&pkt_len, conn->tcp_rxqueue + bytes);
		if (conn->tcp_rxlen < pkt_len) {
			break;
		}

		/* purple_debug_info("TCP_PENDING", "Packet len=%d, rxlen=%d\n", pkt_len, conn->tcp_rxlen); */
		if ( pkt_len < QQ_TCP_HEADER_LENGTH
		    || *(conn->tcp_rxqueue + bytes) != QQ_PACKET_TAG
			|| *(conn->tcp_rxqueue + pkt_len - 1) != QQ_PACKET_TAIL) {
			/* HEY! This isn't even a QQ. What are you trying to pull? */
			purple_debug_warning("TCP_PENDING", "Packet error, no header or tail tag\n");

			jump = memchr(conn->tcp_rxqueue + 1, QQ_PACKET_TAIL, conn->tcp_rxlen - 1);
			if ( !jump ) {
				purple_debug_warning("TCP_PENDING", "Failed to find next tail, clear receive buffer\n");
				g_free(conn->tcp_rxqueue);
				conn->tcp_rxqueue = NULL;
				conn->tcp_rxlen = 0;
				return;
			}

			/* jump and over QQ_PACKET_TAIL */
			jump_len = (jump - conn->tcp_rxqueue) + 1;
			purple_debug_warning("TCP_PENDING", "Find next tail at %d, jump %d\n", jump_len, jump_len + 1);
			g_memmove(conn->tcp_rxqueue, jump, conn->tcp_rxlen - jump_len);
			conn->tcp_rxlen -= jump_len;
			continue;
		}

		memset(pkt, 0, MAX_PACKET_SIZE);
		g_memmove(pkt, conn->tcp_rxqueue + bytes, pkt_len - bytes);

		/* jump to next packet */
		conn->tcp_rxlen -= pkt_len;
		if (conn->tcp_rxlen) {
			/* purple_debug_info("TCP_PENDING", "shrink tcp_rxqueue to %d\n", conn->tcp_rxlen);	*/
			jump = g_memdup(conn->tcp_rxqueue + pkt_len, conn->tcp_rxlen);
			g_free(conn->tcp_rxqueue);
			conn->tcp_rxqueue = jump;
		} else {
			/* purple_debug_info("TCP_PENDING", "free tcp_rxqueue\n"); */
			g_free(conn->tcp_rxqueue);
			conn->tcp_rxqueue = NULL;
		}

		/* packet_process may call disconnect and destory data like conn
		 * do not call packet_process before jump,
		 * break if packet_process return FALSE */
		if (packet_process(gc, pkt, pkt_len - bytes) == FALSE) {
			purple_debug_info("TCP_PENDING", "Connection has been destory\n");
			break;
		}
	}
}

static void udp_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = NULL;
	qq_data *qd;
	guint8 *buf;
	gint buf_len;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	if(cond != PURPLE_INPUT_READ) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Socket error"));
		return;
	}

	buf = g_newa(guint8, MAX_PACKET_SIZE);

	/* here we have UDP proxy suppport */
	buf_len = read(source, buf, MAX_PACKET_SIZE);
	if (buf_len <= 0) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to read from socket"));
		return;
	}

	/* keep alive will be sent in 30 seconds since last_receive
	 *  QQ need a keep alive packet in every 60 seconds
	 gc->last_received = time(NULL);
	*/

	if (buf_len < QQ_UDP_HEADER_LENGTH) {
		if (buf[0] != QQ_PACKET_TAG || buf[buf_len - 1] != QQ_PACKET_TAIL) {
			qq_hex_dump(PURPLE_DEBUG_ERROR, "UDP_PENDING",
					buf, buf_len,
					"Received packet is too short, or no header and tail tag");
			return;
		}
	}

	/* packet_process may call disconnect and destory data like conn
	 * do not call packet_process before jump,
	 * break if packet_process return FALSE */
	packet_process(gc, buf, buf_len);
}

static gint udp_send_out(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	gint ret;

	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

#if 0
	purple_debug_info("UDP_SEND_OUT", "Send %d bytes to socket %d\n", data_len, qd->fd);
#endif

	errno = 0;
	ret = send(qd->fd, data, data_len, 0);
	if (ret < 0 && errno == EAGAIN) {
		return ret;
	}

	if (ret < 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_debug_error("UDP_SEND_OUT", "Send failed: %d, %s\n", errno, g_strerror(errno));
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				g_strerror(errno));
	}
	return ret;
}

static void tcp_can_write(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = (PurpleConnection *) data;
	qq_data *qd;
	qq_connection *conn;
	int ret, writelen;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	conn = connection_find(qd, source);
	g_return_if_fail(conn != NULL);

	writelen = purple_circ_buffer_get_max_read(conn->tcp_txbuf);
	if (writelen == 0) {
		purple_input_remove(conn->can_write_handler);
		conn->can_write_handler = 0;
		return;
	}

	ret = write(source, conn->tcp_txbuf->outptr, writelen);
	purple_debug_info("TCP_CAN_WRITE", "total %d bytes is sent %d\n", writelen, ret);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret < 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        _("Write Error"));
		return;
	}

	purple_circ_buffer_mark_read(conn->tcp_txbuf, ret);
}

static gint tcp_send_out(PurpleConnection *gc, guint8 *data, gint data_len)
{
	qq_data *qd;
	qq_connection *conn;
	gint ret;

	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

	conn = connection_find(qd, qd->fd);
	g_return_val_if_fail(conn, -1);

#if 0
	purple_debug_info("TCP_SEND_OUT", "Send %d bytes to socket %d\n", data_len, qd->fd);
#endif

	if (conn->can_write_handler == 0) {
		ret = write(qd->fd, data, data_len);
	} else {
		ret = -1;
		errno = EAGAIN;
	}

	/*
	purple_debug_info("TCP_SEND_OUT",
		"Socket %d, total %d bytes is sent %d\n", qd->fd, data_len, ret);
	*/
	if (ret < 0 && errno == EAGAIN) {
		/* socket is busy, send later */
		purple_debug_info("TCP_SEND_OUT", "Socket is busy and send later\n");
		ret = 0;
	} else if (ret <= 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_debug_error("TCP_SEND_OUT",
			"Send to socket %d failed: %d, %s\n", qd->fd, errno, g_strerror(errno));
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				g_strerror(errno));
		return ret;
	}

	if (ret < data_len) {
		purple_debug_info("TCP_SEND_OUT", "Add %d bytes to buffer\n", data_len - ret);
		if (conn->can_write_handler == 0) {
			conn->can_write_handler = purple_input_add(qd->fd, PURPLE_INPUT_WRITE, tcp_can_write, gc);
		}
		if (conn->tcp_txbuf == NULL) {
			conn->tcp_txbuf = purple_circ_buffer_new(4096);
		}
		purple_circ_buffer_append(conn->tcp_txbuf, data + ret, data_len - ret);
	}
	return ret;
}

static gboolean network_timeout(gpointer data)
{
	PurpleConnection *gc = (PurpleConnection *) data;
	qq_data *qd;
	gboolean is_lost_conn;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, TRUE);
	qd = (qq_data *) gc->proto_data;

	is_lost_conn = qq_trans_scan(gc);
	if (is_lost_conn) {
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Connection lost"));
		return TRUE;
	}

	if ( !qd->is_login ) {
		return TRUE;
	}

	qd->itv_count.keep_alive--;
	if (qd->itv_count.keep_alive <= 0) {
		qd->itv_count.keep_alive = qd->itv_config.keep_alive;
		if (qd->client_version >= 2008) {
			qq_request_keep_alive_2008(gc);
		} else if (qd->client_version >= 2007) {
			qq_request_keep_alive_2007(gc);
		} else {
			qq_request_keep_alive(gc);
		}
		return TRUE;
	}

	if (qd->itv_config.update <= 0) {
		return TRUE;
	}

	qd->itv_count.update--;
	if (qd->itv_count.update <= 0) {
		qd->itv_count.update = qd->itv_config.update;
		qq_update_online(gc, 0);
		return TRUE;
	}

	return TRUE;		/* if return FALSE, timeout callback stops */
}

static void set_all_keys(PurpleConnection *gc)
{
	qq_data *qd;
	const gchar *passwd;
	guint8 *dest;
	int dest_len = QQ_KEY_LENGTH;
#ifndef DEBUG
	int bytes;
#endif
	/* _qq_show_socket("Got login socket", source); */

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	/* QQ use random seq, to minimize duplicated packets */
	srand(time(NULL));
	qd->send_seq = rand() & 0xffff;

	qd->is_login = FALSE;
	qd->uid = strtoul(purple_account_get_username(purple_connection_get_account(gc)), NULL, 10);

#ifdef DEBUG
	memset(qd->ld.random_key, 0x01, sizeof(qd->ld.random_key));
#else
	for (bytes = 0; bytes < sizeof(qd->ld.random_key); bytes++)	{
		qd->ld.random_key[bytes] = (guint8) (rand() & 0xff);
	}
#endif

	/* now generate md5 processed passwd */
	passwd = purple_account_get_password(purple_connection_get_account(gc));

	/* use twice-md5 of user password as session key since QQ 2003iii */
	dest = qd->ld.pwd_md5;
	qq_get_md5(dest, dest_len, (guint8 *)passwd, strlen(passwd));

	dest = qd->ld.pwd_twice_md5;
	qq_get_md5(dest, dest_len, qd->ld.pwd_md5, dest_len);
}

/* the callback function after socket is built
 * we setup the qq protocol related configuration here */
static void connect_cb(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc;
	qq_data *qd;
	PurpleAccount *account ;
	qq_connection *conn;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;
	account = purple_connection_get_account(gc);

	/* conn_data will be destoryed */
	qd->conn_data = NULL;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		purple_debug_info("QQ_CONN", "Invalid connection\n");
		close(source);
		return;
	}

	if (source < 0) {	/* socket returns -1 */
		purple_debug_info("QQ_CONN",
				"Could not establish a connection with the server:\n%s\n",
				error_message);
		if (qd->connect_watcher > 0)	purple_timeout_remove(qd->connect_watcher);
		qd->connect_watcher = purple_timeout_add_seconds(QQ_CONNECT_INTERVAL, qq_connect_later, gc);
		return;
	}

	/* _qq_show_socket("Got login socket", source); */
	qd->fd = source;
	conn = connection_create(qd, source);
	if (qd->use_tcp) {
		conn->input_handler = purple_input_add(source, PURPLE_INPUT_READ, tcp_pending, gc);
	} else {
		conn->input_handler = purple_input_add(source, PURPLE_INPUT_READ, udp_pending, gc);
	}

	g_return_if_fail(qd->network_watcher == 0);
	qd->network_watcher = purple_timeout_add_seconds(qd->itv_config.resend, network_timeout, gc);

	set_all_keys( gc );

	if (qd->client_version >= 2007) {
		purple_connection_update_progress(gc, _("Getting server"), 2, QQ_CONNECT_STEPS);
		qq_request_get_server(gc);
		return;
	}

	purple_connection_update_progress(gc, _("Requesting token"), 2, QQ_CONNECT_STEPS);
	qq_request_token(gc);
}

#ifndef purple_proxy_connect_udp
static void udp_can_write(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc;
	qq_data *qd;
	socklen_t len;
	int error=0, ret;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;


	purple_debug_info("proxy", "Connected.\n");

	/*
	 * getsockopt after a non-blocking connect returns -1 if something is
	 * really messed up (bad descriptor, usually). Otherwise, it returns 0 and
	 * error holds what connect would have returned if it blocked until now.
	 * Thus, error == 0 is success, error == EINPROGRESS means "try again",
	 * and anything else is a real error.
	 *
	 * (error == EINPROGRESS can happen after a select because the kernel can
	 * be overly optimistic sometimes. select is just a hint that you might be
	 * able to do something.)
	 */
	len = sizeof(error);
	ret = getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0 && error == EINPROGRESS)
		return; /* we'll be called again later */

	purple_input_remove(qd->udp_can_write_handler);
	qd->udp_can_write_handler = 0;
	if (ret < 0 || error != 0) {
		if(ret != 0)
			error = errno;

		close(source);

		purple_debug_error("proxy", "getsockopt SO_ERROR check: %s\n", g_strerror(error));

		connect_cb(gc, -1, _("Unable to connect"));
		return;
	}

	connect_cb(gc, source, NULL);
}

static void udp_host_resolved(GSList *hosts, gpointer data, const char *error_message) {
	PurpleConnection *gc;
	qq_data *qd;
	struct sockaddr server_addr;
	int addr_size;
	gint fd = -1;
	int flags;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;

	/* udp_query_data must be set as NULL.
	 * Otherwise purple_dnsquery_destroy in qq_disconnect cause glib double free error */
	qd->udp_query_data = NULL;

	if (!hosts || !hosts->data) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Couldn't resolve host"));
		return;
	}

	addr_size = GPOINTER_TO_INT(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	memcpy(&server_addr, hosts->data, addr_size);
	g_free(hosts->data);

	hosts = g_slist_remove(hosts, hosts->data);
	while(hosts) {
		hosts = g_slist_remove(hosts, hosts->data);
		g_free(hosts->data);
		hosts = g_slist_remove(hosts, hosts->data);
	}

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		purple_debug_error("QQ",
				"Unable to create socket: %s\n", g_strerror(errno));
		return;
	}

	/* we use non-blocking mode to speed up connection */
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#ifndef _WIN32
	fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

	/* From Unix-socket-FAQ: http://www.faqs.org/faqs/unix-faq/socket/
	 *
	 * If a UDP socket is unconnected, which is the normal state after a
	 * bind() call, then send() or write() are not allowed, since no
	 * destination is available; only sendto() can be used to send data.
	 *
	 * Calling connect() on the socket simply records the specified address
	 * and port number as being the desired communications partner. That
	 * means that send() or write() are now allowed; they use the destination
	 * address and port given on the connect call as the destination of packets.
	 */
	if (connect(fd, &server_addr, addr_size) >= 0) {
		purple_debug_info("QQ", "Connected.\n");
		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		connect_cb(gc, fd, NULL);
		return;
	}

	/* [EINPROGRESS]
	 *    The socket is marked as non-blocking and the connection cannot be
	 *    completed immediately. It is possible to select for completion by
	 *    selecting the socket for writing.
	 * [EINTR]
	 *    A signal interrupted the call.
	 *    The connection is established asynchronously.
	 */
	if ((errno == EINPROGRESS) || (errno == EINTR)) {
			purple_debug_warning( "QQ", "Connect in asynchronous mode.\n");
			qd->udp_can_write_handler = purple_input_add(fd, PURPLE_INPUT_WRITE, udp_can_write, gc);
			return;
		}

	purple_debug_error("QQ", "Connection failed: %s\n", g_strerror(errno));
	close(fd);
}
#endif

gboolean connect_to_server(PurpleConnection *gc, gchar *server, gint port)
{
	PurpleAccount *account ;
	qq_data *qd;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, FALSE);
	account = purple_connection_get_account(gc);
	qd = (qq_data *) gc->proto_data;

	if (server == NULL || strlen(server) == 0 || port == 0) {
		purple_connection_error_reason(gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Invalid server or port"));
		return FALSE;
	}

	purple_connection_update_progress(gc, _("Connecting to server"), 1, QQ_CONNECT_STEPS);

	purple_debug_info("QQ", "Connect to %s:%d\n", server, port);

	if (qd->conn_data != NULL) {
		purple_proxy_connect_cancel(qd->conn_data);
		qd->conn_data = NULL;
	}

#ifdef purple_proxy_connect_udp
	if (qd->use_tcp) {
		qd->conn_data = purple_proxy_connect(gc, account, server, port, connect_cb, gc);
	} else {
		qd->conn_data = purple_proxy_connect_udp(gc, account, server, port, connect_cb, gc);
	}
	if ( qd->conn_data == NULL ) {
		purple_debug_error("QQ", "Couldn't create socket");
		return FALSE;
	}
#else
	/* QQ connection via UDP/TCP.
	* Now use Purple proxy function to provide TCP proxy support,
	* and qq_udp_proxy.c to add UDP proxy support (thanks henry) */
	if(qd->use_tcp) {
		qd->conn_data = purple_proxy_connect(gc, account, server, port, connect_cb, gc);
		if ( qd->conn_data == NULL ) {
			purple_debug_error("QQ", "Unable to connect.");
			return FALSE;
		}
		return TRUE;
	}

	purple_debug_info("QQ", "UDP Connect to %s:%d\n", server, port);
	qd->udp_query_data = purple_dnsquery_a(server, port, udp_host_resolved, gc);
	if ( qd->udp_query_data == NULL ) {
		purple_debug_error("QQ", "Could not resolve hostname");
		return FALSE;
	}
#endif
	return TRUE;
}

/* clean up qq_data structure and all its components
 * always used before a redirectly connection */
void qq_disconnect(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	purple_debug_info("QQ", "Disconnecting...\n");

	if (qd->network_watcher > 0) {
		purple_debug_info("QQ", "Remove network watcher\n");
		purple_timeout_remove(qd->network_watcher);
		qd->network_watcher = 0;
	}

	/* finish  all I/O */
	if (qd->fd >= 0 && qd->is_login) {
		qq_request_logout(gc);
	}

	/* not connected */
	if (qd->conn_data != NULL) {
		purple_debug_info("QQ", "Connect cancel\n");
		purple_proxy_connect_cancel(qd->conn_data);
		qd->conn_data = NULL;
	}
#ifndef purple_proxy_connect_udp
	if (qd->udp_can_write_handler) {
		purple_input_remove(qd->udp_can_write_handler);
		qd->udp_can_write_handler = 0;
	}
	if (qd->udp_query_data != NULL) {
		purple_debug_info("QQ", "destroy udp_query_data\n");
		purple_dnsquery_destroy(qd->udp_query_data);
		qd->udp_query_data = NULL;
	}
#endif
	connection_free_all(qd);
	qd->fd = -1;

	qq_trans_remove_all(gc);

	memset(qd->ld.random_key, 0, sizeof(qd->ld.random_key));
	memset(qd->ld.pwd_md5, 0, sizeof(qd->ld.pwd_md5));
	memset(qd->ld.pwd_twice_md5, 0, sizeof(qd->ld.pwd_twice_md5));
	memset(qd->ld.login_key, 0, sizeof(qd->ld.login_key));
	memset(qd->session_key, 0, sizeof(qd->session_key));
	memset(qd->session_md5, 0, sizeof(qd->session_md5));

	qd->my_local_ip.s_addr = 0;
	qd->my_local_port = 0;
	qd->my_ip.s_addr = 0;
	qd->my_port = 0;

	qq_room_data_free_all(gc);
	qq_buddy_data_free_all(gc);
}

static gint packet_encap(qq_data *qd, guint8 *buf, gint maxlen, guint16 cmd, guint16 seq,
	guint8 *data, gint data_len)
{
	gint bytes = 0;
	g_return_val_if_fail(qd != NULL && buf != NULL && maxlen > 0, -1);
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	/* QQ TCP packet has two bytes in the begining defines packet length
	 * so leave room here to store packet size */
	if (qd->use_tcp) {
		bytes += qq_put16(buf + bytes, 0x0000);
	}
	/* now comes the normal QQ packet as UDP */
	bytes += qq_put8(buf + bytes, QQ_PACKET_TAG);
	bytes += qq_put16(buf + bytes, qd->client_tag);
	bytes += qq_put16(buf + bytes, cmd);

	bytes += qq_put16(buf + bytes, seq);

	bytes += qq_put32(buf + bytes, qd->uid);
	bytes += qq_putdata(buf + bytes, data, data_len);
	bytes += qq_put8(buf + bytes, QQ_PACKET_TAIL);

	/* set TCP packet length at begin of the packet */
	if (qd->use_tcp) {
		qq_put16(buf, bytes);
	}

	return bytes;
}

/* data has been encrypted before */
static gint packet_send_out(PurpleConnection *gc, guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint8 *buf;
	gint buf_len;
	gint bytes_sent;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *)gc->proto_data;
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	buf_len = packet_encap(qd, buf, MAX_PACKET_SIZE, cmd, seq, data, data_len);
	if (buf_len <= 0) {
		return -1;
	}

	qd->net_stat.sent++;
	if (qd->use_tcp) {
		bytes_sent = tcp_send_out(gc, buf, buf_len);
	} else {
		bytes_sent = udp_send_out(gc, buf, buf_len);
	}

	return bytes_sent;
}

gint qq_send_cmd_encrypted(PurpleConnection *gc, guint16 cmd, guint16 seq,
	guint8 *encrypted, gint encrypted_len, gboolean is_save2trans)
{
	gint sent_len;

#if 1
		/* qq_show_packet("qq_send_cmd_encrypted", data, data_len); */
		purple_debug_info("QQ", "<== [%05d] %s(0x%04X), datalen %d\n",
				seq, qq_get_cmd_desc(cmd), cmd, encrypted_len);
#endif

	sent_len = packet_send_out(gc, cmd, seq, encrypted, encrypted_len);
	if (is_save2trans)  {
		qq_trans_add_client_cmd(gc, cmd, seq, encrypted, encrypted_len, 0, 0);
	}
	return sent_len;
}

/* Encrypt data with session_key, and send packet out */
static gint send_cmd_detail(PurpleConnection *gc, guint16 cmd, guint16 seq,
	guint8 *data, gint data_len, gboolean is_save2trans, gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint8 *encrypted;
	gint encrypted_len;
	gint bytes_sent;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *)gc->proto_data;
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	/* at most 16 bytes more */
	encrypted = g_newa(guint8, data_len + 16);
	encrypted_len = qq_encrypt(encrypted, data, data_len, qd->session_key);
	if (encrypted_len < 16) {
		purple_debug_error("QQ_ENCRYPT", "Error len %d: [%05d] 0x%04X %s\n",
				encrypted_len, seq, cmd, qq_get_cmd_desc(cmd));
		return -1;
	}

	bytes_sent = packet_send_out(gc, cmd, seq, encrypted, encrypted_len);

	if (is_save2trans)  {
		qq_trans_add_client_cmd(gc, cmd, seq, encrypted, encrypted_len,
				update_class, ship32);
	}
	return bytes_sent;
}

gint qq_send_cmd_mess(PurpleConnection *gc, guint16 cmd, guint8 *data, gint data_len,
		gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint16 seq;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	seq = ++qd->send_seq;
#if 1
		purple_debug_info("QQ", "<== [%05d] %s(0x%04X), datalen %d\n",
				seq, qq_get_cmd_desc(cmd), cmd, data_len);
#endif
	return send_cmd_detail(gc, cmd, seq, data, data_len, TRUE, update_class, ship32);
}

/* set seq and is_save2trans, then call send_cmd_detail */
gint qq_send_cmd(PurpleConnection *gc, guint16 cmd, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint16 seq;
	gboolean is_save2trans;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	if (cmd != QQ_CMD_LOGOUT) {
		seq = ++qd->send_seq;
		is_save2trans = TRUE;
	} else {
		seq = 0xFFFF;
		is_save2trans = FALSE;
	}
#if 1
		purple_debug_info("QQ", "<== [%05d] %s(0x%04X), datalen %d\n",
				seq, qq_get_cmd_desc(cmd), cmd, data_len);
#endif
	return send_cmd_detail(gc, cmd, seq, data, data_len, is_save2trans, 0, 0);
}

/* set seq and is_save2trans, then call send_cmd_detail */
gint qq_send_server_reply(PurpleConnection *gc, guint16 cmd, guint16 seq, guint8 *data, gint data_len)
{
	qq_data *qd;
	guint8 *encrypted;
	gint encrypted_len;
	gint bytes_sent;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *)gc->proto_data;
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

#if 1
		purple_debug_info("QQ", "<== [SRV-%05d] %s(0x%04X), datalen %d\n",
				seq, qq_get_cmd_desc(cmd), cmd, data_len);
#endif
	/* at most 16 bytes more */
	encrypted = g_newa(guint8, data_len + 16);
	encrypted_len = qq_encrypt(encrypted, data, data_len, qd->session_key);
	if (encrypted_len < 16) {
		purple_debug_error("QQ_ENCRYPT", "Error len %d: [%05d] 0x%04X %s\n",
				encrypted_len, seq, cmd, qq_get_cmd_desc(cmd));
		return -1;
	}

	bytes_sent = packet_send_out(gc, cmd, seq, encrypted, encrypted_len);
	qq_trans_add_server_reply(gc, cmd, seq, encrypted, encrypted_len);

	return bytes_sent;
}

static gint send_room_cmd(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len, gint update_class, guint32 ship32)
{
	qq_data *qd;
	guint8 *buf;
	gint buf_len;
	guint8 *encrypted;
	gint encrypted_len;
	gint bytes_sent;
	guint16 seq;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);

	/* encap room_cmd and room id to buf*/
	buf_len = 0;
	buf_len += qq_put8(buf + buf_len, room_cmd);
	if (room_id != 0) {
		/* id 0 is for QQ Demo Group, now they are closed*/
		buf_len += qq_put32(buf + buf_len, room_id);
	}
	if (data != NULL && data_len > 0) {
		buf_len += qq_putdata(buf + buf_len, data, data_len);
	}

	qd->send_seq++;
	seq = qd->send_seq;

	/* Encrypt to encrypted with session_key */
	/* at most 16 bytes more */
	encrypted = g_newa(guint8, buf_len + 16);
	encrypted_len = qq_encrypt(encrypted, buf, buf_len, qd->session_key);
	if (encrypted_len < 16) {
		purple_debug_error("QQ_ENCRYPT", "Error len %d: [%05d] %s (0x%02X)\n",
				encrypted_len, seq, qq_get_room_cmd_desc(room_cmd), room_cmd);
		return -1;
	}

	bytes_sent = packet_send_out(gc, QQ_CMD_ROOM, seq, encrypted, encrypted_len);
#if 1
		/* qq_show_packet("send_room_cmd", buf, buf_len); */
		purple_debug_info("QQ",
				"<== [%05d] %s (0x%02X) to room %d, datalen %d\n",
				seq, qq_get_room_cmd_desc(room_cmd), room_cmd, room_id, buf_len);
#endif

	qq_trans_add_room_cmd(gc, seq, room_cmd, room_id, encrypted, encrypted_len,
			update_class, ship32);
	return bytes_sent;
}

gint qq_send_room_cmd_mess(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len, gint update_class, guint32 ship32)
{
	g_return_val_if_fail(room_cmd > 0, -1);
	return send_room_cmd(gc, room_cmd, room_id, data, data_len, update_class, ship32);
}

gint qq_send_room_cmd(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len)
{
	g_return_val_if_fail(room_cmd > 0 && room_id > 0, -1);
	return send_room_cmd(gc, room_cmd, room_id, data, data_len, 0, 0);
}

gint qq_send_room_cmd_noid(PurpleConnection *gc, guint8 room_cmd,
		guint8 *data, gint data_len)
{
	g_return_val_if_fail(room_cmd > 0, -1);
	return send_room_cmd(gc, room_cmd, 0, data, data_len, 0, 0);
}

gint qq_send_room_cmd_only(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	g_return_val_if_fail(room_cmd > 0 && room_id > 0, -1);
	return send_room_cmd(gc, room_cmd, room_id, NULL, 0, 0, 0);
}

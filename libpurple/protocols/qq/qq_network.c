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

#ifdef _WIN32
#define random rand
#define srandom srand
#endif

#include "buddy_info.h"
#include "group_info.h"
#include "group_free.h"
#include "qq_crypt.h"
#include "header_info.h"
#include "qq_base.h"
#include "buddy_list.h"
#include "packet_parse.h"
#include "qq_network.h"
#include "qq_trans.h"
#include "utils.h"
#include "qq_process.h"

/* set QQ_RECONNECT_MAX to 1, when test reconnecting */
#define QQ_RECONNECT_MAX					4
#define QQ_RECONNECT_INTERVAL		5000
#define QQ_KEEP_ALIVE_INTERVAL		60000
#define QQ_TRANS_INTERVAL				10000

static gboolean set_new_server(qq_data *qd)
{
	gint count;
	gint index;
	GList *it = NULL;
	
 	g_return_val_if_fail(qd != NULL, FALSE);

	if (qd->servers == NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Server list is NULL\n");
		return FALSE;
	}

	if (qd->real_hostname) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "free real_hostname\n");
		g_free(qd->real_hostname);
		qd->real_hostname = NULL;
	}

	/* remove server used before */
	if (qd->server_name != NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
			"Remove previous server [%s]\n", qd->server_name);
   		qd->servers = g_list_remove(qd->servers, qd->server_name);
   		qd->server_name = NULL;
    }
	
	count = g_list_length(qd->servers);
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Server list has %d\n", count);
	if (count <= 0) {
		/* no server left, disconnect when result is false */
		qd->servers = NULL;
		return FALSE;
	}
	
	/* get new server */
	index  = random() % count;
	it = g_list_nth(qd->servers, index);
    qd->server_name = it->data;		/* do not free server_name */
    if (qd->server_name == NULL || strlen(qd->server_name) <= 0 ) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Server name at %d is empty\n", index);
		return FALSE;
	}

	qd->real_hostname = g_strdup(qd->server_name);
	qd->real_port = qd->user_port;
	
 	qd->reconnect_times = QQ_RECONNECT_MAX;

	purple_debug(PURPLE_DEBUG_INFO, "QQ",
		"set new server to %s:%d\n", qd->real_hostname, qd->real_port);
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

static gboolean reconnect_later_cb(gpointer data)
{
	PurpleConnection *gc;
	qq_data *qd;

	gc = (PurpleConnection *) data;
	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, FALSE);
	qd = (qq_data *) gc->proto_data;

	qd->reconnect_timeout = 0;

	qq_connect(gc->account);
	return FALSE;	/* timeout callback stops */
}

static void reconnect_later(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	qd->reconnect_times--;
	if (qd->reconnect_times < 0) {
		if ( set_new_server(qd) != TRUE) {
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Failed to connect server"));
			return;
		}
	}

	purple_debug(PURPLE_DEBUG_INFO, "QQ",
		"Reconnect to server %s:%d next retries %d in %d ms\n",
		qd->real_hostname, qd->real_port,
		qd->reconnect_times, QQ_RECONNECT_INTERVAL);

	qd->reconnect_timeout = purple_timeout_add(QQ_RECONNECT_INTERVAL,
		reconnect_later_cb, gc);
}

/* process the incoming packet from qq_pending */
static void packet_process(PurpleConnection *gc, guint8 *buf, gint buf_len)
{
	qq_data *qd;
	gint bytes, bytes_not_read;

	gboolean prev_login_status;
	
	guint8 header_tag;
	guint16 source_tag;
	guint16 cmd;
	guint16 seq;		/* May be ack_seq or send_seq, depends on cmd */
	
	guint8 room_cmd;
	guint32 room_id;

	qq_transaction *trans;

	g_return_if_fail(buf != NULL && buf_len > 0);

	qd = (qq_data *) gc->proto_data;

	prev_login_status = qd->logged_in;

	/* Len, header and tail tag have been checked before */
	bytes = 0;
	bytes += packet_get_header(&header_tag, &source_tag, &cmd, &seq, buf + bytes);

#if 1
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"==> [%05d] 0x%04X %s, from (0x%04X %s) len %d\n",
				seq, cmd, qq_get_cmd_desc(cmd), source_tag, qq_get_ver_desc(source_tag), buf_len);
#endif	
	bytes_not_read = buf_len - bytes - 1;

	/* ack packet, we need to update send tranactions */
	/* we do not check duplication for server ack */
	trans = qq_trans_find_rcved(qd, cmd, seq);
	if (trans == NULL) {
		/* new server command */
		qq_trans_add_server_cmd(qd, cmd, seq, buf + bytes, bytes_not_read);
		if ( qd->logged_in ) {
			qq_proc_cmd_server(gc, cmd, seq, buf + bytes, bytes_not_read);
		}
		return;
	}

	if (qq_trans_is_dup(trans)) {
		purple_debug(PURPLE_DEBUG_WARNING,
				"QQ", "dup [%05d] %s, discard...\n", seq, qq_get_cmd_desc(cmd));
		return;
	}

	if (qq_trans_is_server(trans)) {
		if ( qd->logged_in ) {
			qq_proc_cmd_server(gc, cmd, seq, buf + bytes, bytes_not_read);
		}
		return;
	}

	/* this is the length of all the encrypted data (also remove tail tag */
	if (cmd == QQ_CMD_ROOM) {
		room_cmd = qq_trans_get_room_cmd(trans);
		room_id = qq_trans_get_room_id(trans);
#if 1
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"%s (0x%02X ) for room %d, len %d\n",
				qq_get_room_cmd_desc(room_cmd), room_cmd, room_id, buf_len);
#endif	
		qq_proc_room_cmd_reply(gc, seq, room_cmd, room_id, buf + bytes, bytes_not_read);
	} else {
		qq_proc_cmd_reply(gc, cmd, seq, buf + bytes, bytes_not_read);
	}
	
	/* check is redirect or not, and do it now */
	if (qd->is_redirect) {
	 	/* free resource except real_hostname and port */
		qq_disconnect(gc);
	 	qd->reconnect_times = QQ_RECONNECT_MAX;
		reconnect_later(gc);
		return;
	}

	if (prev_login_status != qd->logged_in && qd->logged_in == TRUE) {
		/* logged_in, but we have packets before login */
		qq_trans_process_before_login(qd);
	}
}

static void tcp_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc;
	qq_data *qd;
	guint8 buf[1024];		/* set to 16 when test  tcp_rxqueue */
	gint buf_len;
	gint bytes;
	
	guint8 *pkt;
	guint16 pkt_len;
	
	gchar *error_msg;
	guint8 *jump;
	gint jump_len;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	if(cond != PURPLE_INPUT_READ) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Socket error"));
		return;
	}

	qd = (qq_data *) gc->proto_data;
	
	/* test code, not using tcp_rxqueue
	memset(pkt,0, sizeof(pkt));
	buf_len = read(qd->fd, pkt, sizeof(pkt));
	if (buf_len > 2) {
		packet_process(gc, pkt + 2, buf_len - 2);
	}
	return;
	*/
	
	buf_len = read(qd->fd, buf, sizeof(buf));
	if (buf_len < 0) {
		if (errno == EAGAIN)
			/* No worries */
			return;

		error_msg = g_strdup_printf(_("Lost connection with server:\n%d, %s"), errno, g_strerror(errno));
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_msg);
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
	/*
	purple_debug(PURPLE_DEBUG_INFO, "TCP_PENDING",
			   "Read %d bytes from socket, rxlen is %d\n", buf_len, qd->tcp_rxlen);
	*/
	qd->tcp_rxqueue = g_realloc(qd->tcp_rxqueue, buf_len + qd->tcp_rxlen);
	memcpy(qd->tcp_rxqueue + qd->tcp_rxlen, buf, buf_len);
	qd->tcp_rxlen += buf_len;
	
	pkt = g_newa(guint8, MAX_PACKET_SIZE);
	while (1) {
		if (qd->tcp_rxlen < QQ_TCP_HEADER_LENGTH) {
			break;
		}
		
		bytes = 0;
		bytes += qq_get16(&pkt_len, qd->tcp_rxqueue + bytes);
		if (qd->tcp_rxlen < pkt_len) {
			break;
		}

		/* 
		purple_debug(PURPLE_DEBUG_INFO, "TCP_PENDING",
				   "Packet len is %d bytes, rxlen is %d\n", pkt_len, qd->tcp_rxlen);
		*/
		if ( pkt_len < QQ_TCP_HEADER_LENGTH
		    || *(qd->tcp_rxqueue + bytes) != QQ_PACKET_TAG
			|| *(qd->tcp_rxqueue + pkt_len - 1) != QQ_PACKET_TAIL) {
			/* HEY! This isn't even a QQ. What are you trying to pull? */

			purple_debug(PURPLE_DEBUG_ERROR, "TCP_PENDING",
				 "Packet error, failed to check header and tail tag\n");

			jump = memchr(qd->tcp_rxqueue + 1, QQ_PACKET_TAIL, qd->tcp_rxlen - 1);
			if ( !jump ) {
				purple_debug(PURPLE_DEBUG_INFO, "TCP_PENDING",
				 	"Failed to find next QQ_PACKET_TAIL, clear receive buffer\n");
				g_free(qd->tcp_rxqueue);
				qd->tcp_rxqueue = NULL;
				qd->tcp_rxlen = 0;
				return;
			}

			/* jump and over QQ_PACKET_TAIL */
			jump_len = (jump - qd->tcp_rxqueue) + 1;
			purple_debug(PURPLE_DEBUG_INFO, "TCP_PENDING",
				"Find next QQ_PACKET_TAIL at %d, jump %d bytes\n", jump_len, jump_len + 1);
			g_memmove(qd->tcp_rxqueue, jump, qd->tcp_rxlen - jump_len);
			qd->tcp_rxlen -= jump_len;
			continue;
		}

		memset(pkt, 0, MAX_PACKET_SIZE);
		g_memmove(pkt, qd->tcp_rxqueue + bytes, pkt_len - bytes);
		
		/* jump to next packet */
		qd->tcp_rxlen -= pkt_len;
		if (qd->tcp_rxlen) {
			/*
			purple_debug(PURPLE_DEBUG_ERROR, "TCP_PENDING", "shrink tcp_rxqueue to %d\n", qd->tcp_rxlen);		
			*/
			jump = g_memdup(qd->tcp_rxqueue + pkt_len, qd->tcp_rxlen);
			g_free(qd->tcp_rxqueue);
			qd->tcp_rxqueue = jump;
		} else {
			/* purple_debug(PURPLE_DEBUG_ERROR, "TCP_PENDING", "free tcp_rxqueue\n"); */
			g_free(qd->tcp_rxqueue);
			qd->tcp_rxqueue = NULL;
		}

		if (pkt == NULL) {
			continue;
		}
		/* do not call packet_process before jump 
		 * packet_process may call disconnect and destory tcp_rxqueue */
		packet_process(gc, pkt, pkt_len - bytes);
	}
}

static void udp_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc;
	qq_data *qd;
	guint8 *buf;
	gint buf_len;

	gc = (PurpleConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	if(cond != PURPLE_INPUT_READ) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Socket error"));
		return;
	}

	qd = (qq_data *) gc->proto_data;
	g_return_if_fail(qd->fd >= 0);
	
	buf = g_newa(guint8, MAX_PACKET_SIZE);

	/* here we have UDP proxy suppport */
	buf_len = read(qd->fd, buf, MAX_PACKET_SIZE);
	if (buf_len <= 0) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
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
	
	packet_process(gc, buf, buf_len);
}

static gint udp_send_out(qq_data *qd, guint8 *data, gint data_len)
{
	gint ret;

	g_return_val_if_fail(qd != NULL && qd->fd >= 0 && data != NULL && data_len > 0, -1);

	/*
	purple_debug(PURPLE_DEBUG_INFO, "UDP_SEND_OUT", "Send %d bytes to socket %d\n", data_len, qd->fd);
	*/
	
	errno = 0;
	ret = send(qd->fd, data, data_len, 0);
	if (ret < 0 && errno == EAGAIN) {
		return ret;
	}
	
	if (ret < 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_debug(PURPLE_DEBUG_ERROR, "UDP_SEND_OUT", "Send failed: %d, %s\n", errno, g_strerror(errno));
		purple_connection_error_reason(qd->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, g_strerror(errno));
	}
	return ret;
}

static void tcp_can_write(gpointer data, gint source, PurpleInputCondition cond)
{
	qq_data *qd = data;
	int ret, writelen;

	writelen = purple_circ_buffer_get_max_read(qd->tcp_txbuf);
	if (writelen == 0) {
		purple_input_remove(qd->tx_handler);
		qd->tx_handler = 0;
		return;
	}

	ret = write(qd->fd, qd->tcp_txbuf->outptr, writelen);
	purple_debug(PURPLE_DEBUG_ERROR, "TCP_CAN_WRITE",
		"total %d bytes is sent %d\n", writelen, ret);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret < 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_connection_error_reason(qd->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                               _("Write Error"));
		return;
	}

	purple_circ_buffer_mark_read(qd->tcp_txbuf, ret);
}

static gint tcp_send_out(qq_data *qd, guint8 *data, gint data_len)
{
	gint ret;

	g_return_val_if_fail(qd != NULL && qd->fd >= 0 && data != NULL && data_len > 0, -1);

	/*
	purple_debug(PURPLE_DEBUG_INFO, "TCP_SEND_OUT", "Send %d bytes to socket %d\n", data_len, qd->fd);
	 */

	if (qd->tx_handler == 0) {
		ret = write(qd->fd, data, data_len);
	} else {
		ret = -1;
		errno = EAGAIN;
	}

	/*
	purple_debug(PURPLE_DEBUG_INFO, "TCP_SEND_OUT",
		"Socket %d, total %d bytes is sent %d\n", qd->fd, data_len, ret);
	*/
	if (ret < 0 && errno == EAGAIN) {
		/* socket is busy, send later */
		purple_debug(PURPLE_DEBUG_INFO, "TCP_SEND_OUT", "Socket is busy and send later\n");
		ret = 0;
	} else if (ret <= 0) {
		/* TODO: what to do here - do we really have to disconnect? */
		purple_debug(PURPLE_DEBUG_ERROR, "TCP_SEND_OUT",
			"Send to socket %d failed: %d, %s\n", qd->fd, errno, g_strerror(errno));
		purple_connection_error_reason(qd->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, g_strerror(errno));
		return ret;
	}

	if (ret < data_len) {
		purple_debug(PURPLE_DEBUG_INFO, "TCP_SEND_OUT",
			"Add %d bytes to buffer\n", data_len - ret);
		if (qd->tx_handler == 0) {
			qd->tx_handler = purple_input_add(qd->fd, PURPLE_INPUT_WRITE, tcp_can_write, qd);
		}
		purple_circ_buffer_append(qd->tcp_txbuf, data + ret, data_len - ret);
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

	is_lost_conn = qq_trans_scan(qd);
	if (is_lost_conn) {
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Connection lost"));
		return TRUE;
	}

	if ( !qd->logged_in ) {
		return TRUE;
	}
	
	qd->itv_count.keep_alive--;
	if (qd->itv_count.keep_alive <= 0) {
		qd->itv_count.keep_alive = qd->itv_config.keep_alive;
		qq_send_packet_keep_alive(gc);
		return TRUE;
	}

	if (qd->itv_config.update <= 0) {
		return TRUE;
	}

	qd->itv_count.update--;
	if (qd->itv_count.update <= 0) {
		qd->itv_count.update = qd->itv_config.update;
		qq_send_packet_get_buddies_online(gc, 0);

		qq_send_cmd_group_all_get_online_members(gc);
		return TRUE;
	}

	return TRUE;		/* if return FALSE, timeout callback stops */
}

/* the callback function after socket is built
 * we setup the qq protocol related configuration here */
static void qq_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	qq_data *qd;
	PurpleConnection *gc;
	gchar *conn_msg;
	const gchar *passwd;
	PurpleAccount *account ;

	gc = (PurpleConnection *) data;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ_CONN", "Invalid connection\n");
		close(source);
		return;
	}

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;
	account = purple_connection_get_account(gc);

	/* Connect is now complete; clear the PurpleProxyConnectData */
	qd->connect_data = NULL;

	if (source < 0) {	/* socket returns -1 */
		purple_debug(PURPLE_DEBUG_INFO, "QQ_CONN", "Invalid connection, source is < 0\n");
		qq_disconnect(gc);
		reconnect_later(gc);
		return;
	}

	/* _qq_show_socket("Got login socket", source); */

	/* QQ use random seq, to minimize duplicated packets */
	srandom(time(NULL));
	qd->send_seq = random() & 0x0000ffff;
	qd->fd = source;
	qd->logged_in = FALSE;
	qd->channel = 1;
	qd->uid = strtol(purple_account_get_username(purple_connection_get_account(gc)), NULL, 10);

	/* now generate md5 processed passwd */
	passwd = purple_account_get_password(purple_connection_get_account(gc));

	/* use twice-md5 of user password as session key since QQ 2003iii */
	qq_get_md5(qd->password_twice_md5, sizeof(qd->password_twice_md5),
		(guint8 *)passwd, strlen(passwd));
	qq_get_md5(qd->password_twice_md5, sizeof(qd->password_twice_md5),
		qd->password_twice_md5, sizeof(qd->password_twice_md5));

	g_return_if_fail(qd->network_timeout == 0);
	qd->itv_config.resend = purple_account_get_int(account, "resend_interval", 10);
	if (qd->itv_config.resend <= 0) qd->itv_config.resend = 10;

	qd->itv_config.keep_alive = purple_account_get_int(account, "keep_alive_interval", 60);
	if (qd->itv_config.keep_alive < 30) qd->itv_config.keep_alive = 30;
	qd->itv_config.keep_alive /= qd->itv_config.resend;
	qd->itv_count.keep_alive = qd->itv_config.keep_alive;

	qd->itv_config.update = purple_account_get_int(account, "update_interval", 300);
	if (qd->itv_config.update > 0) {
		if (qd->itv_config.update < qd->itv_config.keep_alive) {
			qd->itv_config.update = qd->itv_config.keep_alive;
		}
		qd->itv_config.update /= qd->itv_config.resend;
		qd->itv_count.update = qd->itv_config.update;
	} else {
		qd->itv_config.update = 0;
	}

	qd->network_timeout = purple_timeout_add(qd->itv_config.resend *1000, network_timeout, gc);
	
	if (qd->use_tcp)
		gc->inpa = purple_input_add(qd->fd, PURPLE_INPUT_READ, tcp_pending, gc);
	else
		gc->inpa = purple_input_add(qd->fd, PURPLE_INPUT_READ, udp_pending, gc);

	/* Update the login progress status display */
	conn_msg = g_strdup_printf("Login as %d", qd->uid);
	purple_connection_update_progress(gc, conn_msg, QQ_CONNECT_STEPS - 1, QQ_CONNECT_STEPS);
	g_free(conn_msg);

	qq_send_packet_token(gc);
}

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
		
	purple_input_remove(qd->tx_handler);
	qd->tx_handler = 0;
	if (ret < 0 || error != 0) {
		if(ret != 0) 
			error = errno;

		close(source);

		purple_debug_error("proxy", "getsockopt SO_ERROR check: %s\n", g_strerror(error));

		qq_connect_cb(gc, -1, _("Unable to connect"));
		return;
	}

	qq_connect_cb(gc, source, NULL);
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
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
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
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Connected.\n");
		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		qq_connect_cb(gc, fd, NULL);
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
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Connect in asynchronous mode.\n");
			qd->tx_handler = purple_input_add(fd, PURPLE_INPUT_WRITE, udp_can_write, gc);
			return;
		}

	purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Connection failed: %s\n", g_strerror(errno));
	close(fd);
}

/* establish a generic QQ connection 
 * TCP/UDP, and direct/redirected */
void qq_connect(PurpleAccount *account)
{
	PurpleConnection *gc;
	qq_data *qd;
	gchar *conn_msg;

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	qd = (qq_data *) gc->proto_data;


	/* test set_new_server
	while (set_new_server(qd)) {
   		purple_debug(PURPLE_DEBUG_INFO, "QQ_TEST",
   			"New server %s:%d  Real server %s:%d\n",
   			qd->server_name, qd->user_port, qd->real_hostname, qd->real_port);
	}
	purple_debug(PURPLE_DEBUG_INFO, "QQ_TEST", "qd->servers %lu\n",
 			qd->servers);
 	exit(1);
	*/
	if (qd->server_name == NULL) {
		/* must be first call this function */
		if ( set_new_server(qd) != TRUE) {
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
					_("Failed to connect server"));
			return;
		}
	}

	if (qd->real_hostname == NULL || qd->real_port == 0) {
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("hostname is NULL or port is 0"));
		return;
	}

	conn_msg = g_strdup_printf( _("Connecting server %s, retries %d"),
		qd->real_hostname, qd->reconnect_times);
	purple_connection_update_progress(gc, conn_msg, 1, QQ_CONNECT_STEPS);
	g_free(conn_msg);

	if (qd->is_redirect) {
   		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Redirect to %s:%d\n",
   			qd->real_hostname, qd->real_port);
   	}
	qd->is_redirect = FALSE;

	qd->fd = -1;
	qd->tx_handler = 0;
	
	/* QQ connection via UDP/TCP. 
	* Now use Purple proxy function to provide TCP proxy support,
	* and qq_udp_proxy.c to add UDP proxy support (thanks henry) */
	if(qd->use_tcp) {
   		purple_debug(PURPLE_DEBUG_INFO, "QQ", "TCP Connect to %s:%d\n",
   			qd->real_hostname, qd->real_port);

		/* TODO: is there a good default grow size? */
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Create tcp_txbuf\n");
		qd->tcp_txbuf = purple_circ_buffer_new(0);

		qd->connect_data = purple_proxy_connect(NULL, account,
				qd->real_hostname, qd->real_port, qq_connect_cb, gc);
		if (qd->connect_data == NULL) {
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Unable to connect."));
		}
		return;
	}
	
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "UDP Connect to %s:%d\n",
		qd->real_hostname, qd->real_port);

	g_return_if_fail(qd->udp_query_data == NULL);
	qd->udp_query_data = purple_dnsquery_a(qd->real_hostname, qd->real_port,
		udp_host_resolved, gc);
	if (qd->udp_query_data == NULL) {
		purple_connection_error_reason(qd->gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Could not resolve hostname"));
	}
}

/* clean up qq_data structure and all its components
 * always used before a redirectly connection */
void qq_disconnect(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Disconnecting ...\n");

	if (qd->network_timeout > 0) {
		purple_timeout_remove(qd->network_timeout);
		qd->network_timeout = 0;
	}

	/* finish  all I/O */
	if (qd->fd >= 0 && qd->logged_in) {
		qq_send_packet_logout(gc);
	}

	if (gc->inpa > 0) {
		purple_input_remove(gc->inpa);
		gc->inpa = 0;
	}

	if (qd->fd >= 0) {
		close(qd->fd);
		qd->fd = -1;
	}

	if (qd->reconnect_timeout > 0) {
		purple_timeout_remove(qd->reconnect_timeout);
		qd->reconnect_timeout = 0;
	}

	if (qd->connect_data != NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Cancel connect_data\n");
		purple_proxy_connect_cancel(qd->connect_data);
	}
	
	if(qd->tcp_txbuf != NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "destroy tcp_txbuf\n");
		purple_circ_buffer_destroy(qd->tcp_txbuf);
		qd->tcp_txbuf = NULL;
	}
	
	if (qd->tx_handler) {
		purple_input_remove(qd->tx_handler);
		qd->tx_handler = 0;
	}
	if (qd->tcp_rxqueue != NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "destroy tcp_rxqueue\n");
		g_free(qd->tcp_rxqueue);
		qd->tcp_rxqueue = NULL;
		qd->tcp_rxlen = 0;
	}
	
	if (qd->udp_query_data != NULL) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "destroy udp_query_data\n");
		purple_dnsquery_destroy(qd->udp_query_data);
		qd->udp_query_data = NULL;
	}

	qq_trans_remove_all(qd);
	
	if (qd->token) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "free token\n");
		g_free(qd->token);
		qd->token = NULL;
		qd->token_len = 0;
	}
	memset(qd->inikey, 0, sizeof(qd->inikey));
	memset(qd->password_twice_md5, 0, sizeof(qd->password_twice_md5));
	memset(qd->session_key, 0, sizeof(qd->session_key));
	memset(qd->session_md5, 0, sizeof(qd->session_md5));

	qd->my_ip.s_addr = 0;

	qq_group_free_all(qd);
	qq_add_buddy_request_free(qd);
	qq_info_query_free(qd);
	qq_buddies_list_free(gc->account, qd);
}

static gint encap(qq_data *qd, guint8 *buf, gint maxlen, guint16 cmd, guint16 seq, 
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
	bytes += qq_put16(buf + bytes, QQ_CLIENT);
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
gint qq_send_data(qq_data *qd, guint16 cmd, guint16 seq, gboolean need_ack,
	guint8 *data, gint data_len)
{
	guint8 *buf;
	gint buf_len;
	gint bytes_sent;

	g_return_val_if_fail(qd != NULL, -1);
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	buf = g_newa(guint8, MAX_PACKET_SIZE);
	memset(buf, 0, MAX_PACKET_SIZE);
	buf_len = encap(qd, buf, MAX_PACKET_SIZE, cmd, seq, data, data_len);
	if (buf_len <= 0) {
		return -1;
	}

	if (qd->use_tcp) {
		bytes_sent = tcp_send_out(qd, buf, buf_len);
	} else {
		bytes_sent = udp_send_out(qd, buf, buf_len);
	}

	if (need_ack)  {
		qq_trans_add_client_cmd(qd, cmd, seq, data, data_len);
	}
	
#if 1
		/* qq_show_packet("QQ_SEND_DATA", buf, buf_len); */
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"<== [%05d], 0x%04X %s, total %d bytes is sent %d\n", 
				seq, cmd, qq_get_cmd_desc(cmd), buf_len, bytes_sent);
#endif
	return bytes_sent;
}

/* Encrypt data with session_key, then call qq_send_data */
gint qq_send_cmd_detail(qq_data *qd, guint16 cmd, guint16 seq, gboolean need_ack,
	guint8 *data, gint data_len)
{
	guint8 *encrypted_data;
	gint encrypted_len;

	g_return_val_if_fail(qd != NULL, -1);
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	/* at most 16 bytes more */
	encrypted_data = g_newa(guint8, data_len + 16);
#if 0
	purple_debug(PURPLE_DEBUG_INFO, "QQ_ENCRYPT",
			"Before %d: [%05d] 0x%04X %s\n",
			data_len, seq, cmd, qq_get_cmd_desc(cmd));
#endif
	encrypted_len = qq_encrypt(encrypted_data, data, data_len, qd->session_key);
	if (encrypted_len < 16) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ_ENCRYPT",
				"Error len %d: [%05d] 0x%04X %s\n",
				encrypted_len, seq, cmd, qq_get_cmd_desc(cmd));
		return -1;
	}

#if 0
	purple_debug(PURPLE_DEBUG_INFO, "QQ_ENCRYPT",
			"After %d: [%05d] 0x%04X %s\n",
			encrypted_len, seq, cmd, qq_get_cmd_desc(cmd));
#endif
	return qq_send_data(qd, cmd, seq, need_ack, encrypted_data, encrypted_len);
}

/* set seq and need_ack, then call qq_send_cmd_detail */
gint qq_send_cmd(qq_data *qd, guint16 cmd, guint8 *data, gint data_len)
{
	g_return_val_if_fail(qd != NULL, -1);
	g_return_val_if_fail(data != NULL && data_len > 0, -1);

	qd->send_seq++;
	return qq_send_cmd_detail(qd, cmd, qd->send_seq, TRUE, data, data_len);
}

gint qq_send_room_cmd_noid(PurpleConnection *gc, guint8 room_cmd, 
		guint8 *data, gint data_len)
{
	return qq_send_room_cmd(gc, room_cmd, 0, data, data_len);
}

gint qq_send_room_cmd_only(PurpleConnection *gc, guint8 room_cmd, guint32 room_id)
{
	g_return_val_if_fail(room_cmd > 0 && room_id > 0, -1);
	return qq_send_room_cmd(gc, room_cmd, room_id, NULL, 0);
}

gint qq_send_room_cmd(PurpleConnection *gc, guint8 room_cmd, guint32 room_id,
		guint8 *data, gint data_len)
{
	qq_data *qd;

	guint8 *buf;
	gint buf_len;
	guint8 *encrypted_data;
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
		/* id 0 is for QQ Demo Group, now there are not existed*/
		buf_len += qq_put32(buf + buf_len, room_id);
	}
	if (data != NULL && data_len > 0) {
		buf_len += qq_putdata(buf + buf_len, data, data_len);
	}
	qd->send_seq++;
	seq = qd->send_seq;

	/* Encrypt to encrypted_data with session_key */
	/* at most 16 bytes more */
	encrypted_data = g_newa(guint8, buf_len + 16);
	encrypted_len = qq_encrypt(encrypted_data, buf, buf_len, qd->session_key);
	if (encrypted_len < 16) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ_ENCRYPT",
				"Error len %d: [%05d] QQ_CMD_ROOM.(0x%02X %s)\n",
				encrypted_len, seq, room_cmd, qq_get_room_cmd_desc(room_cmd));
		return -1;
	}

	/* Encap header to buf */
	buf_len = encap(qd, buf, MAX_PACKET_SIZE, QQ_CMD_ROOM, seq, encrypted_data, encrypted_len);
	if (buf_len <= 0) {
		return -1;
	}

	if (qd->use_tcp) {
		bytes_sent = tcp_send_out(qd, buf, buf_len);
	} else {
		bytes_sent = udp_send_out(qd, buf, buf_len);
	}

	qq_trans_add_room_cmd(qd, seq, room_cmd, room_id, buf, buf_len);
	
#if 1
		/* qq_show_packet("QQ_SEND_DATA", buf, buf_len); */
		purple_debug(PURPLE_DEBUG_INFO, "QQ",
				"<== [%05d], QQ_CMD_ROOM.(0x%02X %s) to room %d, total %d bytes is sent %d\n", 
				seq, room_cmd, qq_get_room_cmd_desc(room_cmd), room_id,
				buf_len, bytes_sent);
#endif
	return bytes_sent;
}

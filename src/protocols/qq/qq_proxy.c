/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
 *                    Henry Ou  <henry@linux.net>
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

#include "cipher.h"
#include "debug.h"
#include "internal.h"

#ifdef _WIN32
#define random rand
#define srandom srand
#endif

#include "packet_parse.h"
#include "buddy_info.h"
#include "buddy_opt.h"
#include "char_conv.h"
#include "group_free.h"
#include "login_logout.h"
#include "qq_proxy.h"
#include "recv_core.h"
#include "send_core.h"
#include "sendqueue.h"
#include "udp_proxy_s5.h"
#include "utils.h"

/* These functions are used only in development phase
 *
static void _qq_show_socket(gchar *desc, gint fd) {
	struct sockaddr_in sin;
	gint len = sizeof(sin);
	getsockname(fd, (struct sockaddr *)&sin, &len);
	gaim_debug(GAIM_DEBUG_INFO, desc, "%s:%d\n",
            inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
}
*/

void _qq_show_packet(gchar *desc, gchar *buf, gint len)
{
	char buf1[4096], buf2[10];
	int i;
	buf1[0] = 0;
	for (i = 0; i < len; i++) {
		sprintf(buf2, " %02x(%d)", buf[i] & 0xff, buf[i] & 0xff);
		strcat(buf1, buf2);
	}
	strcat(buf1, "\n");
	gaim_debug(GAIM_DEBUG_INFO, desc, buf1);
}

/* QQ 2003iii uses double MD5 for the pwkey to get the session key */
static guint8 *_gen_pwkey(const gchar *pwd)
{
        GaimCipher *cipher;
        GaimCipherContext *context;

	gchar pwkey_tmp[QQ_KEY_LENGTH];

	cipher = gaim_ciphers_find_cipher("md5");
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, pwd, strlen(pwd));
	gaim_cipher_context_digest(context, sizeof(pwkey_tmp), pwkey_tmp, NULL);
	gaim_cipher_context_destroy(context);
	context = gaim_cipher_context_new(cipher, NULL);
	gaim_cipher_context_append(context, pwkey_tmp, QQ_KEY_LENGTH);
	gaim_cipher_context_digest(context, sizeof(pwkey_tmp), pwkey_tmp, NULL);
	gaim_cipher_context_destroy(context);

	return g_memdup(pwkey_tmp, QQ_KEY_LENGTH);
}

gint _qq_fill_host(struct sockaddr_in *addr, const gchar *host, guint16 port)
{
	if (!inet_aton(host, &(addr->sin_addr))) {
		struct hostent *hp;
		if (!(hp = gethostbyname(host))) {
			return -1;
		}
		memset(addr, 0, sizeof(struct sockaddr_in));
		memcpy(&(addr->sin_addr.s_addr), hp->h_addr, hp->h_length);
		addr->sin_family = hp->h_addrtype;
	} else {
		addr->sin_family = AF_INET;
	}

	addr->sin_port = htons(port);
	return 0;
}

/* set up any finalizing start-up stuff */
static void _qq_start_services(GaimConnection *gc)
{
	/* start watching for IMs about to be sent */
	/*
	gaim_signal_connect(gaim_conversations_get_handle(),
			"sending-im-msg", gc,
			GAIM_CALLBACK(qq_sending_im_msg_cb), NULL);
			*/
}

/* the callback function after socket is built
 * we setup the qq protocol related configuration here */
static void _qq_got_login(gpointer data, gint source, GaimInputCondition cond)
{
	qq_data *qd;
	GaimConnection *gc;
	gchar *buf;
	const gchar *passwd;

	gc = (GaimConnection *) data;
	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	if (g_list_find(gaim_connections_get_all(), gc) == NULL) {
		close(source);
		return;
	}

	if (source < 0) {	/* socket returns -1 */
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}

	qd = (qq_data *) gc->proto_data;

	/* QQ use random seq, to minimize duplicated packets */
	srandom(time(NULL));
	qd->send_seq = random() & 0x0000ffff;
	qd->fd = source;
	qd->logged_in = FALSE;
	qd->channel = 1;
	qd->uid = strtol(gaim_account_get_username(gaim_connection_get_account(gc)), NULL, 10);
	qd->before_login_packets = g_queue_new();

	/* now generate md5 processed passwd */
	passwd = gaim_account_get_password(gaim_connection_get_account(gc));
	qd->pwkey = _gen_pwkey(passwd);

	qd->sendqueue_timeout = gaim_timeout_add(QQ_SENDQUEUE_TIMEOUT, qq_sendqueue_timeout_callback, gc);
	gc->inpa = gaim_input_add(qd->fd, GAIM_INPUT_READ, qq_input_pending, gc);

	/* Update the login progress status display */
	buf = g_strdup_printf("Login as %d", qd->uid);
	gaim_connection_update_progress(gc, buf, 1, QQ_CONNECT_STEPS);
	g_free(buf);

	_qq_start_services(gc);

	qq_send_packet_request_login_token(gc);
}

/* clean up qq_data structure and all its components
 * always used before a redirectly connection */
static void _qq_common_clean(GaimConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	/* finish  all I/O */
	if (qd->fd >= 0 && qd->logged_in)
		qq_send_packet_logout(gc);
	close(qd->fd);

	if (qd->sendqueue_timeout > 0) {
		gaim_timeout_remove(qd->sendqueue_timeout);
		qd->sendqueue_timeout = 0;
	}

	if (gc->inpa > 0) {
		gaim_input_remove(gc->inpa);
		gc->inpa = 0;
	}

	qq_b4_packets_free(qd);
	qq_sendqueue_free(qd);
	qq_group_packets_free(qd);
	qq_group_free_all(qd);
	qq_add_buddy_request_free(qd);
	qq_info_query_free(qd);
	qq_buddies_list_free(gc->account, qd);
}

static gint _qq_proxy_none(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	gint fd = -1;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Using UDP without proxy\n");
	fd = socket(PF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ Redirect", "Unable to create socket: %s\n", strerror(errno));
		return -1;
	}

	/* we use non-blocking mode to speed up connection */
	fcntl(fd, F_SETFL, O_NONBLOCK);

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
	if (connect(fd, addr, addrlen) < 0) {
		/* [EINPROGRESS]
		 *    The socket is marked as non-blocking and the connection cannot be 
		 *    completed immediately. It is possible to select for completion by 
		 *    selecting the socket for writing.
		 * [EINTR]
		 *    A signal interrupted the call. 
		 *    The connection is established asynchronously.
		 */
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Connect in asynchronous mode.\n");
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "QQ", "Faiil connection: %d\n", strerror(errno));
			close(fd);
			return -1;
		}		/* if errno */
	} else {		/* connect returns 0 */
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "Connected.\n");
		fcntl(fd, F_SETFL, 0);
		phb->func(phb->data, fd, GAIM_INPUT_READ);
	}

	return fd;
}

/* returns the socket handler, or -1 if there is any error */
static gint _qq_udp_proxy_connect(GaimAccount *account,
			   const gchar *server,
			   guint16 port, void callback(gpointer, gint, GaimInputCondition), GaimConnection *gc)
{
	struct sockaddr_in sin;
	struct PHB *phb;
	GaimProxyInfo *info;
	qq_data *qd;

	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);
	qd = (qq_data *) gc->proto_data;

	info = gaim_account_get_proxy_info(account);

	phb = g_new0(struct PHB, 1);
	phb->host = g_strdup(server);
	phb->port = port;
	phb->account = account;
	phb->gpi = info;
	phb->func = callback;
	phb->data = gc;

	if (_qq_fill_host(&sin, server, port) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ",
			   "gethostbyname(\"%s\", %d) failed: %s\n", server, port, hstrerror(h_errno));
		return -1;
	}

	if (info == NULL) {
		qd->proxy_type = GAIM_PROXY_NONE;
		return _qq_proxy_none(phb, (struct sockaddr *) &sin, sizeof(sin));
	}

	qd->proxy_type = info->type;
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Choosing proxy type %d\n", info->type);

	switch (info->type) {
	case GAIM_PROXY_NONE:
		return _qq_proxy_none(phb, (struct sockaddr *) &sin, sizeof(sin));
	case GAIM_PROXY_SOCKS5:
		/* as the destination is always QQ server during the session, 
		 * we can set dest_sin here, instead of _qq_s5_canread_again */
		_qq_fill_host(&qd->dest_sin, phb->host, phb->port);
		_qq_fill_host(&sin, phb->gpi->host, phb->gpi->port);
		return qq_proxy_socks5(phb, (struct sockaddr *) &sin, sizeof(sin));
	default:
		return _qq_proxy_none(phb, (struct sockaddr *) &sin, sizeof(sin));
	}

	return -1;
}

/* QQ connection via UDP/TCP. 
 * I use GAIM proxy function to provide TCP proxy support,
 * and qq_udp_proxy.c to add UDP proxy support (thanks henry)
 *  return the socket handle, -1 means fail */
static gint _proxy_connect_full (GaimAccount *account, const gchar *host, guint16 port, 
		GaimInputFunction func, gpointer data, gboolean use_tcp)
{
	GaimConnection *gc;
	qq_data *qd;

	gc = gaim_account_get_connection(account);
	qd = (qq_data *) gc->proto_data;
	qd->server_ip = g_strdup(host);
	qd->server_port = port;

	return use_tcp ? gaim_proxy_connect(account, host, port, func, data) :	/* TCP mode */
	    _qq_udp_proxy_connect(account, host, port, func, data);	/* UDP mode */
}

/* establish a generic QQ connection 
 * TCP/UDP, and direct/redirected
 * return the socket handler, or -1 if there is any error */
gint qq_connect(GaimAccount *account, const gchar *host, guint16 port, gboolean use_tcp, gboolean is_redirect)
{
	GaimConnection *gc;

	g_return_val_if_fail(host != NULL, -1);
	g_return_val_if_fail(port > 0, -1);

	gc = gaim_account_get_connection(account);
	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);

	if (is_redirect)
		_qq_common_clean(gc);

	return _proxy_connect_full(account, host, port, _qq_got_login, gc, use_tcp);
}

/* clean up the given QQ connection and free all resources */
void qq_disconnect(GaimConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL);

	_qq_common_clean(gc);

	qd = gc->proto_data;
	g_free(qd->inikey);
	g_free(qd->pwkey);
	g_free(qd->session_key);
	g_free(qd->my_ip);
	g_free(qd);

	gc->proto_data = NULL;
}

/* send packet with proxy support */
gint qq_proxy_write(qq_data *qd, guint8 *data, gint len)
{
	guint8 *buf;
	gint ret;

	g_return_val_if_fail(qd != NULL && qd->fd >= 0 && data != NULL && len > 0, -1);

	/* TCP sock5 may be processed twice
	 * so we need to check qd->use_tcp as well */
	if ((!qd->use_tcp) && qd->proxy_type == GAIM_PROXY_SOCKS5) {	/* UDP sock5 */
		buf = g_newa(guint8, len + 10);
		buf[0] = 0x00;
		buf[1] = 0x00;	/* reserved */
		buf[2] = 0x00;	/* frag */
		buf[3] = 0x01;	/* type */
		g_memmove(buf + 4, &(qd->dest_sin.sin_addr.s_addr), 4);
		g_memmove(buf + 8, &(qd->dest_sin.sin_port), 2);
		g_memmove(buf + 10, data, len);
		ret = send(qd->fd, buf, len + 10, 0);
	} else {
		ret = send(qd->fd, data, len, 0);
	}

	return ret;
}

/* read packet input with proxy support */
gint qq_proxy_read(qq_data *qd, guint8 *data, gint len)
{
	guint8 *buf;
	gint bytes;
	buf = g_newa(guint8, MAX_PACKET_SIZE + 10);

	g_return_val_if_fail(qd != NULL && data != NULL && len > 0, -1);
	g_return_val_if_fail(qd->fd > 0, -1);

	bytes = read(qd->fd, buf, len + 10);
	if (bytes < 0)
		return -1;

	if ((!qd->use_tcp) && qd->proxy_type == GAIM_PROXY_SOCKS5) {	/* UDP sock5 */
		if (bytes < 10)
			return -1;
		bytes -= 10;
		g_memmove(data, buf + 10, bytes);	/* cut off the header */
	} else {
		g_memmove(data, buf, bytes);
	}

	return bytes;
}

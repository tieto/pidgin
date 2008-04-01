/**
 * @file qq_proxy.c
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

/* These functions are used only in development phase */
/*
static void _qq_show_socket(gchar *desc, gint fd) {
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(fd, (struct sockaddr *)&sin, &len);
	purple_debug(PURPLE_DEBUG_INFO, desc, "%s:%d\n",
            inet_ntoa(sin.sin_addr), g_ntohs(sin.sin_port));
}
*/

void _qq_show_packet(const gchar *desc, const guint8 *buf, gint len)
{
	char buf1[8*len+2], buf2[10];
	int i;
	buf1[0] = 0;
	for (i = 0; i < len; i++) {
		sprintf(buf2, " %02x(%d)", buf[i] & 0xff, buf[i] & 0xff);
		strcat(buf1, buf2);
	}
	strcat(buf1, "\n");
	purple_debug(PURPLE_DEBUG_INFO, desc, "%s", buf1);
}

/* QQ 2003iii uses double MD5 for the pwkey to get the session key */
static guint8 *_gen_pwkey(const gchar *pwd)
{
        PurpleCipher *cipher;
        PurpleCipherContext *context;

	guchar pwkey_tmp[QQ_KEY_LENGTH];

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, (guchar *) pwd, strlen(pwd));
	purple_cipher_context_digest(context, sizeof(pwkey_tmp), pwkey_tmp, NULL);
	purple_cipher_context_destroy(context);
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, pwkey_tmp, QQ_KEY_LENGTH);
	purple_cipher_context_digest(context, sizeof(pwkey_tmp), pwkey_tmp, NULL);
	purple_cipher_context_destroy(context);

	return g_memdup(pwkey_tmp, QQ_KEY_LENGTH);
}

static gboolean _qq_fill_host(GSList *hosts, struct sockaddr_in *addr, gint *addr_size)
{
	if (!hosts || !hosts->data)
		return FALSE;

	*addr_size = GPOINTER_TO_INT(hosts->data);

	hosts = g_slist_remove(hosts, hosts->data);
	memcpy(addr, hosts->data, *addr_size);
	g_free(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	while(hosts) {
		hosts = g_slist_remove(hosts, hosts->data);
		g_free(hosts->data);
		hosts = g_slist_remove(hosts, hosts->data);
	}

	return TRUE;
}

/* set up any finalizing start-up stuff */
static void _qq_start_services(PurpleConnection *gc)
{
	/* start watching for IMs about to be sent */
	/*
	purple_signal_connect(purple_conversations_get_handle(),
			"sending-im-msg", gc,
			PURPLE_CALLBACK(qq_sending_im_msg_cb), NULL);
			*/
}

/* the callback function after socket is built
 * we setup the qq protocol related configuration here */
static void _qq_got_login(gpointer data, gint source, const gchar *error_message)
{
	qq_data *qd;
	PurpleConnection *gc;
	gchar *buf;
	const gchar *passwd;

	gc = (PurpleConnection *) data;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		close(source);
		return;
	}

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);

	if (source < 0) {	/* socket returns -1 */
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_message);
		return;
	}

	qd = (qq_data *) gc->proto_data;

	/*
	_qq_show_socket("Got login socket", source);
	*/

	/* QQ use random seq, to minimize duplicated packets */
	srandom(time(NULL));
	qd->send_seq = random() & 0x0000ffff;
	qd->fd = source;
	qd->logged_in = FALSE;
	qd->channel = 1;
	qd->uid = strtol(purple_account_get_username(purple_connection_get_account(gc)), NULL, 10);

	/* now generate md5 processed passwd */
	passwd = purple_account_get_password(purple_connection_get_account(gc));
	qd->pwkey = _gen_pwkey(passwd);

	qd->sendqueue_timeout = purple_timeout_add(QQ_SENDQUEUE_TIMEOUT, qq_sendqueue_timeout_callback, gc);
	gc->inpa = purple_input_add(qd->fd, PURPLE_INPUT_READ, qq_input_pending, gc);

	/* Update the login progress status display */
	buf = g_strdup_printf("Login as %d", qd->uid);
	purple_connection_update_progress(gc, buf, 1, QQ_CONNECT_STEPS);
	g_free(buf);

	_qq_start_services(gc);

	qq_send_packet_request_login_token(gc);
}

/* clean up qq_data structure and all its components
 * always used before a redirectly connection */
static void _qq_common_clean(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL && gc->proto_data != NULL);
	qd = (qq_data *) gc->proto_data;

	/* finish  all I/O */
	if (qd->fd >= 0 && qd->logged_in)
		qq_send_packet_logout(gc);
	close(qd->fd);

	if (qd->sendqueue_timeout > 0) {
		purple_timeout_remove(qd->sendqueue_timeout);
		qd->sendqueue_timeout = 0;
	}

	if (gc->inpa > 0) {
		purple_input_remove(gc->inpa);
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

static void no_one_calls(gpointer data, gint source, PurpleInputCondition cond)
{
        struct PHB *phb = data;
	socklen_t len;
	int error=0, ret;

	purple_debug_info("proxy", "Connected.\n");

	len = sizeof(error);

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
	ret = getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len);
	if (ret == 0 && error == EINPROGRESS)
		return; /* we'll be called again later */
	if (ret < 0 || error != 0) {
		if(ret!=0) 
			error = errno;
		close(source);
		purple_input_remove(phb->inpa);

		purple_debug_error("proxy", "getsockopt SO_ERROR check: %s\n", g_strerror(error));

		phb->func(phb->data, -1, _("Unable to connect"));
		return;
	}

	purple_input_remove(phb->inpa);

	if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

		phb->func(phb->data, source, NULL);
	}

	g_free(phb->host);
	g_free(phb);
}

/* returns -1 if fails, otherwise returns the file handle */
static gint _qq_proxy_none(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	gint fd = -1;
	int flags;

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Using UDP without proxy\n");
	fd = socket(PF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ Redirect", 
			"Unable to create socket: %s\n", g_strerror(errno));
		return -1;
	}

	/* we use non-blocking mode to speed up connection */
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

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
			purple_debug_warning("QQ", "Connect in asynchronous mode.\n");
			phb->inpa = purple_input_add(fd, PURPLE_INPUT_WRITE, no_one_calls, phb);
		} else {
			purple_debug_error("QQ", "Connection failed: %s\n", g_strerror(errno));
			close(fd);
			return -1;
		}		/* if errno */
	} else {		/* connect returns 0 */
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "Connected.\n");
		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		phb->func(phb->data, fd, NULL);
	}

	return fd;
}

static void _qq_proxy_resolved(GSList *hosts, gpointer data, const char *error_message)
{
	struct PHB *phb = (struct PHB *) data;
	struct sockaddr_in addr;
	gint addr_size, ret = -1;

	if(_qq_fill_host(hosts, &addr, &addr_size))
		ret = qq_proxy_socks5(phb, (struct sockaddr *) &addr, addr_size);

	if (ret < 0) {
		phb->func(phb->data, -1, _("Unable to connect"));
		g_free(phb->host);
		g_free(phb);
	}
}

static void _qq_server_resolved(GSList *hosts, gpointer data, const char *error_message)
{
	struct PHB *phb = (struct PHB *) data;
	PurpleConnection *gc = (PurpleConnection *) phb->data;
	qq_data *qd = (qq_data *) gc->proto_data;
	struct sockaddr_in addr;
	gint addr_size, ret = -1;

	if(_qq_fill_host(hosts, &addr, &addr_size)) {
		switch (purple_proxy_info_get_type(phb->gpi)) {
			case PURPLE_PROXY_NONE:
				ret = _qq_proxy_none(phb, (struct sockaddr *) &addr, addr_size);
				break;
			case PURPLE_PROXY_SOCKS5:
				ret = 0;
				if (purple_proxy_info_get_host(phb->gpi) == NULL || 
						purple_proxy_info_get_port(phb->gpi) == 0) {
					purple_debug(PURPLE_DEBUG_ERROR, "QQ", 
							"Use of socks5 proxy selected but host or port info doesn't exist.\n");
					ret = -1;
				} else {
					/* as the destination is always QQ server during the session, 
				 	* we can set dest_sin here, instead of _qq_s5_canread_again */
					memcpy(&qd->dest_sin, &addr, addr_size);
					if (purple_dnsquery_a(purple_proxy_info_get_host(phb->gpi),
							purple_proxy_info_get_port(phb->gpi),
							_qq_proxy_resolved, phb) == NULL)
						ret = -1;
				}
				break;
			default:
				purple_debug(PURPLE_DEBUG_WARNING, "QQ", 
						"Proxy type %i is unsupported, not using a proxy.\n",
						purple_proxy_info_get_type(phb->gpi));
				ret = _qq_proxy_none(phb, (struct sockaddr *) &addr, addr_size);
		}
	}

	if (ret < 0) {
		phb->func(gc, -1, _("Unable to connect"));
		g_free(phb->host);
		g_free(phb);
	}
}

/* returns -1 if dns lookup fails, otherwise returns 0 */
static gint _qq_udp_proxy_connect(PurpleAccount *account,
			   const gchar *server, guint16 port, 
			   void callback(gpointer, gint, const gchar *error_message), 
			   PurpleConnection *gc)
{
	PurpleProxyInfo *info;
	struct PHB *phb;
	qq_data *qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail(gc != NULL && qd != NULL, -1);

	info = purple_proxy_get_setup(account);

	phb = g_new0(struct PHB, 1);
	phb->host = g_strdup(server);
	phb->port = port;
	phb->account = account;
	phb->gpi = info;
	phb->func = callback;
	phb->data = gc;
	qd->proxy_type = purple_proxy_info_get_type(phb->gpi);

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Choosing proxy type %d\n", 
			purple_proxy_info_get_type(phb->gpi));

	if (purple_dnsquery_a(server, port, _qq_server_resolved, phb) == NULL) {
		phb->func(gc, -1, _("Unable to connect"));
		g_free(phb->host);
		g_free(phb);
		return -1;
	} else {
		return 0;
	}
}

/* QQ connection via UDP/TCP. 
 * I use Purple proxy function to provide TCP proxy support,
 * and qq_udp_proxy.c to add UDP proxy support (thanks henry) */
static gint _proxy_connect_full (PurpleAccount *account, const gchar *host, guint16 port, 
		PurpleProxyConnectFunction func, gpointer data, gboolean use_tcp)
{
	PurpleConnection *gc;
	qq_data *qd;

	gc = purple_account_get_connection(account);
	qd = (qq_data *) gc->proto_data;
	qd->server_ip = g_strdup(host);
	qd->server_port = port;

	if(use_tcp)
		return (purple_proxy_connect(NULL, account, host, port, func, data) == NULL);
	else
		return _qq_udp_proxy_connect(account, host, port, func, data);
}

/* establish a generic QQ connection 
 * TCP/UDP, and direct/redirected */
gint qq_connect(PurpleAccount *account, const gchar *host, guint16 port, 
		gboolean use_tcp, gboolean is_redirect)
{
	PurpleConnection *gc;
	qq_data *qd;

	g_return_val_if_fail(host != NULL, -1);
	g_return_val_if_fail(port > 0, -1);

	gc = purple_account_get_connection(account);
	g_return_val_if_fail(gc != NULL && gc->proto_data != NULL, -1);

	if (is_redirect)
		_qq_common_clean(gc);

	qd = (qq_data *) gc->proto_data;
	qd->before_login_packets = g_queue_new();

	return _proxy_connect_full(account, host, port, _qq_got_login, gc, use_tcp);
}

/* clean up the given QQ connection and free all resources */
void qq_disconnect(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL);

	_qq_common_clean(gc);

	qd = gc->proto_data;
	g_free(qd->inikey);
	g_free(qd->pwkey);
	g_free(qd->session_key);
	g_free(qd->session_md5);
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
	if ((!qd->use_tcp) && qd->proxy_type == PURPLE_PROXY_SOCKS5) {	/* UDP sock5 */
		buf = g_newa(guint8, len + 10);
		buf[0] = 0x00;
		buf[1] = 0x00;	/* reserved */
		buf[2] = 0x00;	/* frag */
		buf[3] = 0x01;	/* type */
		g_memmove(buf + 4, &(qd->dest_sin.sin_addr.s_addr), 4);
		g_memmove(buf + 8, &(qd->dest_sin.sin_port), 2);
		g_memmove(buf + 10, data, len);
		errno = 0;
		ret = send(qd->fd, buf, len + 10, 0);
	} else {
		errno = 0;
		ret = send(qd->fd, data, len, 0);
	}
	if (ret == -1)
		purple_connection_error_reason(qd->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, g_strerror(errno));

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

	if ((!qd->use_tcp) && qd->proxy_type == PURPLE_PROXY_SOCKS5) {	/* UDP sock5 */
		if (bytes < 10)
			return -1;
		bytes -= 10;
		g_memmove(data, buf + 10, bytes);	/* cut off the header */
	} else {
		g_memmove(data, buf, bytes);
	}

	return bytes;
}

/*
 * @file yahoo_filexfer.c Yahoo Filetransfer
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

#include "internal.h"
#include "dnsquery.h"

#include "prpl.h"
#include "util.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "proxy.h"
#include "ft.h"
#include "libymsg.h"
#include "yahoo_packet.h"
#include "yahoo_filexfer.h"
#include "yahoo_doodle.h"
#include "yahoo_friend.h"

struct yahoo_xfer_data {
	gchar *host;
	gchar *path;
	int port;
	PurpleConnection *gc;
	long expires;
	gboolean started;
	guchar *txbuf;
	gsize txbuflen;
	gsize txbuf_written;
	guint tx_handler;
	gchar *rxqueue;
	guint rxlen;
	gchar *xfer_peer_idstring;
	gchar *xfer_idstring_for_relay;
	int version; /* 0 for old, 15 for Y7(YMSG 15) */
	int info_val_249;

	enum {
		STARTED = 0,
		HEAD_REQUESTED,
		HEAD_REPLY_RECEIVED,
		TRANSFER_PHASE,
		ACCEPTED,
		P2P_HEAD_REQUESTED,
		P2P_HEAD_REPLIED,
		P2P_GET_REQUESTED
	} status_15;

	/* contains all filenames, in case of multiple transfers, with the first
	 * one in the list being the current file's name (ymsg15) */
	GSList *filename_list;
	GSList *size_list; /* corresponds to filename_list, with size as **STRING** */
	gboolean firstoflist;
	gchar *xfer_url;	/* url of the file, used when we are p2p server */
	int yahoo_local_p2p_ft_server_fd;
	int yahoo_local_p2p_ft_server_port;
	int yahoo_p2p_ft_server_watcher;
	int input_event;
};

static void yahoo_xfer_data_free(struct yahoo_xfer_data *xd)
{
	PurpleConnection *gc;
	YahooData *yd;
	PurpleXfer *xfer;
	GSList *l;

	gc = xd->gc;
	yd = purple_connection_get_protocol_data(gc);

	/* remove entry from map */
	if(xd->xfer_peer_idstring) {
		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xd->xfer_peer_idstring);
		if(xfer)
			g_hash_table_remove(yd->xfer_peer_idstring_map, xd->xfer_peer_idstring);
	}

	/* empty file & filesize list */
	for (l = xd->filename_list; l; l = l->next) {
		g_free(l->data);
		l->data=NULL;
	}
	for (l = xd->size_list; l; l = l->next) {
		g_free(l->data);
		l->data=NULL;
	}
	g_slist_free(xd->filename_list);
	g_slist_free(xd->size_list);

	g_free(xd->host);
	g_free(xd->path);
	g_free(xd->txbuf);
	g_free(xd->xfer_peer_idstring);
	g_free(xd->xfer_idstring_for_relay);
	if (xd->tx_handler)
		purple_input_remove(xd->tx_handler);
	g_free(xd);
}

static void yahoo_receivefile_send_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	gssize remaining, written;

	xfer = data;
	xd = purple_xfer_get_protocol_data(xfer);

	remaining = xd->txbuflen - xd->txbuf_written;
	written = purple_xfer_write(xfer, xd->txbuf + xd->txbuf_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		purple_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if (written < remaining) {
		xd->txbuf_written += written;
		return;
	}

	purple_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	g_free(xd->txbuf);
	xd->txbuf = NULL;
	xd->txbuflen = 0;

	purple_xfer_start(xfer, source, NULL, 0);

}

static void yahoo_receivefile_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;

	purple_debug_info("yahoo", "in yahoo_receivefile_connected\n");

	if (!(xfer = data))
		return;
	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;
	if ((source < 0) || (xd->path == NULL) || (xd->host == NULL)) {
		purple_xfer_error(PURPLE_XFER_RECEIVE, purple_xfer_get_account(xfer),
				purple_xfer_get_remote_user(xfer), _("Unable to connect."));
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_xfer_set_fd(xfer, source);

	/* The first time we get here, assemble the tx buffer */
	if (xd->txbuflen == 0) {
		gchar *header = g_strdup_printf("GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n",
			      xd->path, xd->host);
		xd->txbuf = (guchar*) header;
		xd->txbuflen = strlen(header);
		xd->txbuf_written = 0;
	}

	if (!xd->tx_handler)
	{
		xd->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE,
			yahoo_receivefile_send_cb, xfer);
		yahoo_receivefile_send_cb(xfer, source, PURPLE_INPUT_WRITE);
	}
}

static void yahoo_sendfile_send_cb(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	gssize written, remaining;

	xfer = data;
	xd = purple_xfer_get_protocol_data(xfer);

	remaining = xd->txbuflen - xd->txbuf_written;
	written = purple_xfer_write(xfer, xd->txbuf + xd->txbuf_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		purple_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if (written < remaining) {
		xd->txbuf_written += written;
		return;
	}

	purple_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	g_free(xd->txbuf);
	xd->txbuf = NULL;
	xd->txbuflen = 0;

	purple_xfer_start(xfer, source, NULL, 0);
}

static void yahoo_sendfile_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	struct yahoo_packet *pkt;
	gchar *size, *filename, *encoded_filename, *header;
	guchar *pkt_buf;
	const char *host;
	int port;
	size_t content_length, header_len, pkt_buf_len;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;

	purple_debug_info("yahoo", "in yahoo_sendfile_connected\n");

	if (!(xfer = data))
		return;
	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;

	if (source < 0) {
		purple_xfer_error(PURPLE_XFER_RECEIVE, purple_xfer_get_account(xfer),
				purple_xfer_get_remote_user(xfer), _("Unable to connect."));
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_xfer_set_fd(xfer, source);

	/* Assemble the tx buffer */
	gc = xd->gc;
	account = purple_connection_get_account(gc);
	yd = purple_connection_get_protocol_data(gc);

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANSFER,
		YAHOO_STATUS_AVAILABLE, yd->session_id);

	size = g_strdup_printf("%" G_GOFFSET_FORMAT, purple_xfer_get_size(xfer));
	filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
	encoded_filename = yahoo_string_encode(gc, filename, NULL);

	yahoo_packet_hash(pkt, "sssss", 0, purple_connection_get_display_name(gc),
	  5, purple_xfer_get_remote_user(xfer), 14, "", 27, encoded_filename, 28, size);
	g_free(size);
	g_free(encoded_filename);
	g_free(filename);

	content_length = YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt);

	pkt_buf_len = yahoo_packet_build(pkt, 4, FALSE, yd->jp, &pkt_buf);
	yahoo_packet_free(pkt);

	host = purple_account_get_string(account, "xfer_host", YAHOO_XFER_HOST);
	port = purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT);
	header = g_strdup_printf(
		"POST http://%s:%d/notifyft HTTP/1.0\r\n"
		"Content-length: %" G_GOFFSET_FORMAT "\r\n"
		"Host: %s:%d\r\n"
		"Cookie: Y=%s; T=%s\r\n"
		"\r\n",
		host, port, content_length + 4 + purple_xfer_get_size(xfer),
		host, port, yd->cookie_y, yd->cookie_t);

	header_len = strlen(header);

	xd->txbuflen = header_len + pkt_buf_len + 4;
	xd->txbuf = g_malloc(xd->txbuflen);

	memcpy(xd->txbuf, header, header_len);
	g_free(header);
	memcpy(xd->txbuf + header_len, pkt_buf, pkt_buf_len);
	g_free(pkt_buf);
	memcpy(xd->txbuf + header_len + pkt_buf_len, "29\xc0\x80", 4);

	xd->txbuf_written = 0;

	if (xd->tx_handler == 0)
	{
		xd->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE,
										yahoo_sendfile_send_cb, xfer);
		yahoo_sendfile_send_cb(xfer, source, PURPLE_INPUT_WRITE);
	}
}

static void yahoo_xfer_init(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;

	xfer_data = purple_xfer_get_protocol_data(xfer);
	gc = xfer_data->gc;
	yd = purple_connection_get_protocol_data(gc);
	account = purple_connection_get_account(gc);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND) {
		if (yd->jp) {
			if (purple_proxy_connect(gc, account, purple_account_get_string(account, "xferjp_host",  YAHOOJP_XFER_HOST),
			                       purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
			                       yahoo_sendfile_connected, xfer) == NULL)
			{
				purple_notify_error(gc, NULL, _("File Transfer Failed"),
				                _("Unable to establish file descriptor."));
				purple_xfer_cancel_remote(xfer);
			}
		} else {
			if (purple_proxy_connect(gc, account, purple_account_get_string(account, "xfer_host",  YAHOO_XFER_HOST),
			                       purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
			                       yahoo_sendfile_connected, xfer) == NULL)
			{
				purple_notify_error(gc, NULL, _("File Transfer Failed"),
				                _("Unable to establish file descriptor."));
				purple_xfer_cancel_remote(xfer);
			}
		}
	} else {
		purple_xfer_set_fd(xfer, -1);
		if (purple_proxy_connect(gc, account, xfer_data->host, xfer_data->port,
		                              yahoo_receivefile_connected, xfer) == NULL) {
			purple_notify_error(gc, NULL, _("File Transfer Failed"),
			             _("Unable to establish file descriptor."));
			purple_xfer_cancel_remote(xfer);
		}
	}
}

static void yahoo_xfer_init_15(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;
	struct yahoo_packet *pkt;

	xfer_data = purple_xfer_get_protocol_data(xfer);
	gc = xfer_data->gc;
	yd = purple_connection_get_protocol_data(gc);
	account = purple_connection_get_account(gc);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)	{
		gchar *filename;
		filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
		pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
							   YAHOO_STATUS_AVAILABLE,
							   yd->session_id);
		yahoo_packet_hash(pkt, "sssiiiisiii",
			1, purple_normalize(account, purple_account_get_username(account)),
			5, purple_xfer_get_remote_user(xfer),
			265, xfer_data->xfer_peer_idstring,
			222, 1,
			266, 1,
			302, 268,
			300, 268,
			27,  filename,
			28,  (int)purple_xfer_get_size(xfer),
			301, 268,
			303, 268);
		g_free(filename);
	} else {
		if(xfer_data->firstoflist == TRUE) {
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
				YAHOO_STATUS_AVAILABLE, yd->session_id);

			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 3);
		} else {
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_ACC_15,
				YAHOO_STATUS_AVAILABLE, yd->session_id);

			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				271, 1);
		}
	}
	yahoo_packet_send_and_free(pkt, yd);
}

static void yahoo_xfer_start(PurpleXfer *xfer)
{
	/* We don't need to do anything here, do we? */
}

static goffset calculate_length(const gchar *l, size_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit(l[i]))
			continue;
		return g_ascii_strtoll(l + i, NULL, 10);
	}
	return 0;
}

static gssize yahoo_xfer_read(guchar **buffer, PurpleXfer *xfer)
{
	gchar buf[4096];
	gssize len;
	gchar *start = NULL;
	gchar *length;
	gchar *end;
	goffset filelen;
	struct yahoo_xfer_data *xd = purple_xfer_get_protocol_data(xfer);

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_RECEIVE) {
		return 0;
	}

	len = read(purple_xfer_get_fd(xfer), buf, sizeof(buf));

	if (len <= 0) {
		if ((purple_xfer_get_size(xfer) > 0) &&
		    (purple_xfer_get_bytes_sent(xfer) >= purple_xfer_get_size(xfer))) {
			purple_xfer_set_completed(xfer, TRUE);
			return 0;
		} else
			return -1;
	}

	if (!xd->started) {
		xd->rxqueue = g_realloc(xd->rxqueue, len + xd->rxlen);
		memcpy(xd->rxqueue + xd->rxlen, buf, len);
		xd->rxlen += len;

		length = g_strstr_len(xd->rxqueue, len, "Content-length:");
		/* some proxies re-write this header, changing the capitalization :(
		 * technically that's allowed since headers are case-insensitive
		 * [RFC 2616, section 4.2] */
		if (length == NULL)
			length = g_strstr_len(xd->rxqueue, len, "Content-Length:");
		if (length) {
			end = g_strstr_len(length, length - xd->rxqueue, "\r\n");
			if (!end)
				return 0;
			if ((filelen = calculate_length(length, len - (length - xd->rxqueue))))
				purple_xfer_set_size(xfer, filelen);
		}
		start = g_strstr_len(xd->rxqueue, len, "\r\n\r\n");
		if (start)
			start += 4;
		if (!start || start > (xd->rxqueue + len))
			return 0;
		xd->started = TRUE;

		len -= (start - xd->rxqueue);

		*buffer = g_malloc(len);
		memcpy(*buffer, start, len);
		g_free(xd->rxqueue);
		xd->rxqueue = NULL;
		xd->rxlen = 0;
	} else {
		*buffer = g_malloc(len);
		memcpy(*buffer, buf, len);
	}

	return len;
}

static gssize yahoo_xfer_write(const guchar *buffer, size_t size, PurpleXfer *xfer)
{
	gssize len;
	struct yahoo_xfer_data *xd = purple_xfer_get_protocol_data(xfer);

	if (!xd)
		return -1;

	if (purple_xfer_get_type(xfer) != PURPLE_XFER_SEND) {
		return -1;
	}

	len = write(purple_xfer_get_fd(xfer), buffer, size);

	if (len == -1) {
		if (purple_xfer_get_bytes_sent(xfer) >= purple_xfer_get_size(xfer))
			purple_xfer_set_completed(xfer, TRUE);
		if ((errno != EAGAIN) && (errno != EINTR))
			return -1;
		return 0;
	}

	return len;
}

static void yahoo_xfer_cancel_send(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = purple_xfer_get_protocol_data(xfer);

	if(purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL && xfer_data->version == 15)
	{
		PurpleConnection *gc;
		PurpleAccount *account;
		YahooData *yd;
		struct yahoo_packet *pkt;

		gc = xfer_data->gc;
		yd = purple_connection_get_protocol_data(gc);
		account = purple_connection_get_account(gc);
		if(xfer_data->xfer_idstring_for_relay) /* hack to see if file trans acc/info packet has been received */
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15,
								   YAHOO_STATUS_DISCONNECTED,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				66, -1);
		}
		else
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_AVAILABLE,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 2);
		}
		yahoo_packet_send_and_free(pkt, yd);
	}


	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	purple_xfer_set_protocol_data(xfer, NULL);
}

static void yahoo_xfer_cancel_recv(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = purple_xfer_get_protocol_data(xfer);

	if(purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL && xfer_data->version == 15)
	{

		PurpleConnection *gc;
		PurpleAccount *account;
		YahooData *yd;
		struct yahoo_packet *pkt;

		gc = xfer_data->gc;
		yd = purple_connection_get_protocol_data(gc);
		account = purple_connection_get_account(gc);
		if(!xfer_data->xfer_idstring_for_relay) /* hack to see if file trans acc/info packet has been received */
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_AVAILABLE,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 4);
		}
		else
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_DISCONNECTED,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				66, -1);
		}
		yahoo_packet_send_and_free(pkt, yd);
	}

	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	purple_xfer_set_protocol_data(xfer, NULL);
}

/* Send HTTP OK after receiving file */
static void yahoo_p2p_ft_server_send_OK(PurpleXfer *xfer)
{
	char *tx = NULL;
	int written;
	int fd = purple_xfer_get_fd(xfer);

	tx = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nContent-Type: application/octet-stream\r\nConnection: close\r\n\r\n";
	written = write(fd, tx, strlen(tx));

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0)
		purple_debug_info("yahoo", "p2p filetransfer: Unable to write HTTP OK");

	/* close connection */
	close(fd);
	purple_xfer_set_fd(xfer, -1);
}

static void yahoo_xfer_end(PurpleXfer *xfer_old)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleXfer *xfer = NULL;
	PurpleConnection *gc;
	YahooData *yd;

	xfer_data = purple_xfer_get_protocol_data(xfer_old);
	if(xfer_data && xfer_data->version == 15
	   && purple_xfer_get_type(xfer_old) == PURPLE_XFER_RECEIVE
	   && xfer_data->filename_list) {

		/* Send HTTP OK in case of p2p transfer, when we act as server */
		if((xfer_data->xfer_url != NULL) && (purple_xfer_get_fd(xfer_old) >=0) && (purple_xfer_get_status(xfer_old) == PURPLE_XFER_STATUS_DONE))
			yahoo_p2p_ft_server_send_OK(xfer_old);

		/* removing top of filename & size list completely */
		g_free( xfer_data->filename_list->data );
		g_free( xfer_data->size_list->data );

		xfer_data->filename_list->data = NULL;
		xfer_data->size_list->data = NULL;

		xfer_data->filename_list = g_slist_delete_link(xfer_data->filename_list, xfer_data->filename_list);
		xfer_data->size_list = g_slist_delete_link(xfer_data->size_list, xfer_data->size_list);

		/* if there are still more files */
		if(xfer_data->filename_list)
		{
			gchar* filename;
			goffset filesize;

			filename = xfer_data->filename_list->data;
			filesize = g_ascii_strtoll( xfer_data->size_list->data, NULL, 10 );

			gc = xfer_data->gc;
			yd = purple_connection_get_protocol_data(gc);

			/* setting up xfer_data for next file's tranfer */
			g_free(xfer_data->host);
			g_free(xfer_data->path);
			g_free(xfer_data->txbuf);
			g_free(xfer_data->rxqueue);
			g_free(xfer_data->xfer_idstring_for_relay);
			if (xfer_data->tx_handler)
				purple_input_remove(xfer_data->tx_handler);
			xfer_data->host = NULL;
			xfer_data->host = NULL;
			xfer_data->port = 0;
			xfer_data->expires = 0;
			xfer_data->started = FALSE;
			xfer_data->txbuf = NULL;
			xfer_data->txbuflen = 0;
			xfer_data->txbuf_written = 0;
			xfer_data->tx_handler = 0;
			xfer_data->rxqueue = NULL;
			xfer_data->rxlen = 0;
			xfer_data->xfer_idstring_for_relay = NULL;
			xfer_data->info_val_249 = 0;
			xfer_data->status_15 = STARTED;
			xfer_data->firstoflist = FALSE;

			/* Dereference xfer_data from old xfer */
			purple_xfer_set_protocol_data(xfer_old, NULL);

			/* Build the file transfer handle. */
			xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_RECEIVE, purple_xfer_get_remote_user(xfer_old));


			if (xfer) {
				/* Set the info about the incoming file. */
				char *utf8_filename = yahoo_string_decode(gc, filename, TRUE);
				purple_xfer_set_filename(xfer, utf8_filename);
				g_free(utf8_filename);
				purple_xfer_set_size(xfer, filesize);

				purple_xfer_set_protocol_data(xfer, xfer_data);

				/* Setup our I/O op functions */
				purple_xfer_set_init_fnc(xfer,        yahoo_xfer_init_15);
				purple_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
				purple_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
				purple_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
				purple_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
				purple_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
				purple_xfer_set_write_fnc(xfer,       yahoo_xfer_write);
				purple_xfer_set_request_denied_fnc(xfer,yahoo_xfer_cancel_recv);

				/* update map to current xfer */
				g_hash_table_remove(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring);
				g_hash_table_insert(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring, xfer);

				/* Now perform the request */
				purple_xfer_request(xfer);
			}
			return;
		}
	}
	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	purple_xfer_set_protocol_data(xfer_old, NULL);
}

void yahoo_process_p2pfilexfer(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

	char *me      = NULL;
	char *from    = NULL;
	char *service = NULL;
	char *message = NULL;
	char *command = NULL;
	char *imv     = NULL;
	char *unknown = NULL;

	/* Get all the necessary values from this new packet */
	while(l != NULL)
	{
		struct yahoo_pair *pair = l->data;

		switch(pair->key) {
		case 5:         /* Get who the packet is for */
			me = pair->value;
			break;
		case 4:         /* Get who the packet is from */
			from = pair->value;
			break;
		case 49:        /* Get the type of service */
			service = pair->value;
			break;
		case 14:        /* Get the 'message' of the packet */
			message = pair->value;
			break;
		case 13:        /* Get the command associated with this packet */
			command = pair->value;
			break;
		case 63:        /* IMVironment name and version */
			imv = pair->value;
			break;
		case 64:        /* Not sure, but it does vary with initialization of Doodle */
			unknown = pair->value; /* So, I'll keep it (for a little while atleast) */
			break;
		}

		l = l->next;
	}

	/* If this packet is an IMVIRONMENT, handle it accordingly */
	if(service != NULL && imv != NULL && !strcmp(service, "IMVIRONMENT"))
	{
		/* Check for a Doodle packet and handle it accordingly */
		if(strstr(imv, "doodle;") != NULL)
			yahoo_doodle_process(gc, me, from, command, message, imv);

		/* If an IMVIRONMENT packet comes without a specific imviroment name */
		if(!strcmp(imv, ";0"))
		{
			/* It is unfortunately time to close all IMVironments with the remote client */
			yahoo_doodle_command_got_shutdown(gc, from);
		}
	}
}

void yahoo_process_filetransfer(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *from = NULL;
	char *to = NULL;
	char *msg = NULL;
	char *url = NULL;
	char *imv = NULL;
	long expires = 0;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	char *service = NULL;
	char *filename = NULL;
	goffset filesize = G_GOFFSET_CONSTANT(0);
	GSList *l;

	yd = purple_connection_get_protocol_data(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			from = pair->value;
			break;
		case 5:
			to = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		case 20:
			url = pair->value;
			break;
		case 38:
			expires = strtol(pair->value, NULL, 10);
			break;
		case 27:
			filename = pair->value;
			break;
		case 28:
			filesize = g_ascii_strtoll(pair->value, NULL, 10);
			break;
		case 49:
			service = pair->value;
			break;
		case 63:
			imv = pair->value;
			break;
		}
	}

	/*
	 * The remote user has changed their IMVironment.  We
	 * record it for later use.
	 */
	if (from && imv && service && (strcmp("IMVIRONMENT", service) == 0)) {
		g_hash_table_replace(yd->imvironments, g_strdup(from), g_strdup(imv));
		return;
	}

	if (pkt->service == YAHOO_SERVICE_P2PFILEXFER) {
		if (service && (strcmp("FILEXFER", service) != 0)) {
			purple_debug_misc("yahoo", "unhandled service 0x%02x\n", pkt->service);
			return;
		}
	}

	if (msg) {
		char *tmp;
		tmp = strchr(msg, '\006');
		if (tmp)
			*tmp = '\0';
	}

	if (!url || !from)
		return;

	/* Setup the Yahoo-specific file transfer data */
	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->gc = gc;
	if (!purple_url_parse(url, &(xfer_data->host), &(xfer_data->port), &(xfer_data->path), NULL, NULL)) {
		g_free(xfer_data);
		return;
	}

	purple_debug_misc("yahoo_filexfer", "Host is %s, port is %d, path is %s, and the full url was %s.\n",
	                xfer_data->host, xfer_data->port, xfer_data->path, url);

	/* Build the file transfer handle. */
	xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_RECEIVE, from);
	if (xfer == NULL) {
		g_free(xfer_data);
		g_return_if_reached();
	}

	purple_xfer_set_protocol_data(xfer, xfer_data);

	/* Set the info about the incoming file. */
	if (filename) {
		char *utf8_filename = yahoo_string_decode(gc, filename, TRUE);
		purple_xfer_set_filename(xfer, utf8_filename);
		g_free(utf8_filename);
	} else {
		gchar *start, *end;
		start = g_strrstr(xfer_data->path, "/");
		if (start)
			start++;
		end = g_strrstr(xfer_data->path, "?");
		if (start && *start && end) {
			char *utf8_filename;
			filename = g_strndup(start, end - start);
			utf8_filename = yahoo_string_decode(gc, filename, TRUE);
			g_free(filename);
			purple_xfer_set_filename(xfer, utf8_filename);
			g_free(utf8_filename);
			filename = NULL;
		}
	}

	purple_xfer_set_size(xfer, filesize);

	/* Setup our I/O op functions */
	purple_xfer_set_init_fnc(xfer,        yahoo_xfer_init);
	purple_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
	purple_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
	purple_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	purple_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	purple_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
	purple_xfer_set_write_fnc(xfer,       yahoo_xfer_write);

	/* Now perform the request */
	purple_xfer_request(xfer);
}

PurpleXfer *yahoo_new_xfer(PurpleConnection *gc, const char *who)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xfer_data;

	g_return_val_if_fail(who != NULL, NULL);

	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->gc = gc;

	/* Build the file transfer handle. */
	xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_SEND, who);
	if (xfer == NULL)
	{
		g_free(xfer_data);
		g_return_val_if_reached(NULL);
	}

	purple_xfer_set_protocol_data(xfer, xfer_data);

	/* Setup our I/O op functions */
	purple_xfer_set_init_fnc(xfer,        yahoo_xfer_init);
	purple_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
	purple_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
	purple_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	purple_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	purple_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
	purple_xfer_set_write_fnc(xfer,       yahoo_xfer_write);

	return xfer;
}

static gchar* yahoo_xfer_new_xfer_id(void)
{
	gchar *ans;
	int i,j;
	ans = g_strnfill(24, ' ');
	ans[23] = '$';
	ans[22] = '$';
	for(i = 0; i < 22; i++)
	{
		j = g_random_int_range (0,61);
		if(j < 26)
			ans[i] = j + 'a';
		else if(j < 52)
			ans[i] = j - 26 + 'A';
		else
			ans[i] = j - 52 + '0';
	}
	return ans;
}

static void yahoo_xfer_dns_connected_15(GSList *hosts, gpointer data, const char *error_message)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	struct sockaddr_in *addr;
	struct yahoo_packet *pkt;
	unsigned long actaddr;
	unsigned char a,b,c,d;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;
	gchar *url;
	gchar *filename;

	if (!(xfer = data))
		return;
	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;
	gc = xd->gc;
	account = purple_connection_get_account(gc);
	yd = purple_connection_get_protocol_data(gc);

	if(!hosts)
	{
		purple_debug_error("yahoo", "Unable to find an IP address for relay.msg.yahoo.com\n");
		purple_xfer_cancel_remote(xfer);
		return;
	}

	/* Discard the length... */
	hosts = g_slist_remove(hosts, hosts->data);
	if(!hosts)
	{
		purple_debug_error("yahoo", "Unable to find an IP address for relay.msg.yahoo.com\n");
		purple_xfer_cancel_remote(xfer);
		return;
	}

	/* TODO:actually, u must try with addr no.1 , if its not working addr no.2 ..... */
	addr = hosts->data;
	actaddr = addr->sin_addr.s_addr;
	d = actaddr & 0xff;
	actaddr >>= 8;
	c = actaddr & 0xff;
	actaddr >>= 8;
	b = actaddr & 0xff;
	actaddr >>= 8;
	a = actaddr & 0xff;
	if(yd->jp)
		xd->port = YAHOOJP_XFER_RELAY_PORT;
	else
		xd->port = YAHOO_XFER_RELAY_PORT;

	url = g_strdup_printf("%u.%u.%u.%u", d, c, b, a);

	/* Free the address... */
	g_free(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	addr = NULL;
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_remove(hosts, hosts->data);
		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_remove(hosts, hosts->data);
	}

	if (!purple_url_parse(url, &(xd->host), &(xd->port), &(xd->path), NULL, NULL)) {
		purple_xfer_cancel_remote(xfer);
		g_free(url);
		return;
	}
	g_free(url);

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
	filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));

	yahoo_packet_hash(pkt, "ssssis",
		1, purple_normalize(account, purple_account_get_username(account)),
		5, purple_xfer_get_remote_user(xfer),
		265, xd->xfer_peer_idstring,
		27,  filename,
		249, 3,
		250, xd->host);

	g_free(filename);
	yahoo_packet_send_and_free(pkt, yd);
}

gboolean yahoo_can_receive_file(PurpleConnection *gc, const char *who)
{
	if (!who || yahoo_get_federation_from_name(who) != YAHOO_FEDERATION_NONE)
		return FALSE;
	return TRUE;
}

void yahoo_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	struct yahoo_xfer_data *xfer_data;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleXfer *xfer = yahoo_new_xfer(gc, who);

	g_return_if_fail(xfer != NULL);

	/* if we don't have a p2p connection, try establishing it now */
	if( !g_hash_table_lookup(yd->peers, who) )
		yahoo_send_p2p_pkt(gc, who, 0);

	xfer_data = purple_xfer_get_protocol_data(xfer);
	xfer_data->status_15 = STARTED;
	purple_xfer_set_init_fnc(xfer, yahoo_xfer_init_15);
	xfer_data->version = 15;
	xfer_data->xfer_peer_idstring = yahoo_xfer_new_xfer_id();
	g_hash_table_insert(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring, xfer);

	/* Now perform the request */
	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

static void yahoo_p2p_ft_server_listen_cb(int listenfd, gpointer data);	/* using this in yahoo_xfer_send_cb_15 */
static void yahoo_xfer_connected_15(gpointer data, gint source, const gchar *error_message);/* using this in recv_cb */

static void yahoo_xfer_recv_cb_15(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	int did;
	guchar buf[1000];
	PurpleAccount *account;
	PurpleConnection *gc;

	xfer = data;
	xd = purple_xfer_get_protocol_data(xfer);
	account = purple_connection_get_account(xd->gc);
	gc = xd->gc;

	while((did = read(source, buf, sizeof(buf))) > 0)
	{
		/* TODO: Convert to circ buffer, this all is pretty horrible */
		xd->txbuf = g_realloc(xd->txbuf, xd->txbuflen + did);
		g_memmove(xd->txbuf + xd->txbuflen, buf, did);
		xd->txbuflen += did;
	}

	if (did < 0 && errno == EAGAIN)
		return;
	else if (did < 0) {
		purple_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	xd->txbuflen = 0;

	if(xd->status_15 == HEAD_REQUESTED) {
		xd->status_15 = HEAD_REPLY_RECEIVED;
		close(source);/* Is this required? */
		g_free(xd->txbuf);
		xd->txbuf = NULL;
		if (purple_proxy_connect(gc, account, xd->host, xd->port, yahoo_xfer_connected_15, xfer) == NULL)
		{
			purple_notify_error(gc, NULL, _("File Transfer Failed"),
				_("Unable to establish file descriptor."));
			purple_xfer_cancel_remote(xfer);
		}
	} else {
		purple_debug_error("yahoo","Unrecognized yahoo file transfer mode and stage (ymsg15):%d,%d\n",
						   purple_xfer_get_type(xfer),
						   xd->status_15);
		return;
	}
}

static void yahoo_xfer_send_cb_15(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	int remaining, written;

	xfer = data;
	xd = purple_xfer_get_protocol_data(xfer);
	remaining = xd->txbuflen - xd->txbuf_written;
	written = write(source, xd->txbuf + xd->txbuf_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		purple_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if (written < remaining) {
		xd->txbuf_written += written;
		return;
	}

	purple_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	g_free(xd->txbuf);
	xd->txbuf = NULL;
	xd->txbuflen = 0;
	xd->txbuf_written = 0;

	if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE && xd->status_15 == STARTED)
	{
		xd->status_15 = HEAD_REQUESTED;
		xd->tx_handler = purple_input_add(source, PURPLE_INPUT_READ, yahoo_xfer_recv_cb_15, xfer);
		yahoo_xfer_recv_cb_15(xfer, source, PURPLE_INPUT_READ);
	}
	else if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE && xd->status_15 == HEAD_REPLY_RECEIVED)
	{
		xd->status_15 = TRANSFER_PHASE;
		purple_xfer_set_fd(xfer, source);
		purple_xfer_start(xfer, source, NULL, 0);
	}
	else if(purple_xfer_get_type(xfer) == PURPLE_XFER_SEND && (xd->status_15 == ACCEPTED || xd->status_15 == P2P_GET_REQUESTED) )
	{
		xd->status_15 = TRANSFER_PHASE;
		purple_xfer_set_fd(xfer, source);
		/* Remove Read event */
		purple_input_remove(xd->input_event);
		xd->input_event = 0;
		purple_xfer_start(xfer, source, NULL, 0);
	}
	else if(purple_xfer_get_type(xfer) == PURPLE_XFER_SEND && xd->status_15 == P2P_HEAD_REQUESTED)
	{
		xd->status_15 = P2P_HEAD_REPLIED;
		/* Remove Read event and close descriptor */
		purple_input_remove(xd->input_event);
		xd->input_event = 0;
		close(source);
		purple_xfer_set_fd(xfer, -1);
		/* start local server, listen for connections */
		purple_network_listen(xd->yahoo_local_p2p_ft_server_port, AF_UNSPEC, SOCK_STREAM, TRUE, yahoo_p2p_ft_server_listen_cb, xfer);
	}
	else
	{
		purple_debug_error("yahoo", "Unrecognized yahoo file transfer mode and stage (ymsg15):%d,%d\n", purple_xfer_get_type(xfer), xd->status_15);
		return;
	}
}

static void yahoo_xfer_connected_15(gpointer data, gint source, const gchar *error_message)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	PurpleAccount *account;
	PurpleConnection *gc;

	if (!(xfer = data))
		return;
	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;
	gc = xd->gc;
	account = purple_connection_get_account(gc);
	if ((source < 0) || (xd->path == NULL) || (xd->host == NULL)) {
		purple_xfer_error(PURPLE_XFER_RECEIVE, purple_xfer_get_account(xfer),
			purple_xfer_get_remote_user(xfer), _("Unable to connect."));
		purple_xfer_cancel_remote(xfer);
		return;
	}
	/* The first time we get here, assemble the tx buffer */
	if (xd->txbuflen == 0)
	{
		gchar* cookies;
		gchar* initial_buffer;
		YahooData *yd = purple_connection_get_protocol_data(gc);

		/* cookies = yahoo_get_cookies(gc);
		 * This doesn't seem to be working. The function is returning NULL, which yahoo servers don't like
		 * For now let us not use this function */

		cookies = g_strdup_printf("Y=%s; T=%s", yd->cookie_y, yd->cookie_t);

		if(purple_xfer_get_type(xfer) == PURPLE_XFER_SEND && xd->status_15 == ACCEPTED)
		{
			if(xd->info_val_249 == 2)
				{
				/* sending file via p2p, we are connected as client */
				initial_buffer = g_strdup_printf("POST /%s HTTP/1.1\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Content-Length: %" G_GOFFSET_FORMAT "\r\n"
						"Cache-Control: no-cache\r\n\r\n",
										xd->path,
										xd->host,
										purple_xfer_get_size(xfer));	/* to do, add Referer */
				}
			else
				{
				/* sending file via relaying */
				initial_buffer = g_strdup_printf("POST /relay?token=%s&sender=%s&recver=%s HTTP/1.1\r\n"
						"Cookie:%s\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Content-Length: %" G_GOFFSET_FORMAT "\r\n"
						"Cache-Control: no-cache\r\n\r\n",
										purple_url_encode(xd->xfer_idstring_for_relay),
										purple_normalize(account, purple_account_get_username(account)),
										purple_xfer_get_remote_user(xfer),
										cookies,
										xd->host,
										purple_xfer_get_size(xfer));	/* to do, add Referer */
				}
		}
		else if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE && xd->status_15 == STARTED)
		{
			if(xd->info_val_249 == 1)
				{
				/* receiving file via p2p, connected as client */
				initial_buffer = g_strdup_printf("HEAD /%s HTTP/1.1\r\n"
						"Accept: */*\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Content-Length: 0\r\n"
						"Cache-Control: no-cache\r\n\r\n",
						xd->path,xd->host);
			}
			else
				{
				/* receiving file via relaying */
				initial_buffer = g_strdup_printf("HEAD /relay?token=%s&sender=%s&recver=%s HTTP/1.1\r\n"
						"Accept: */*\r\n"
						"Cookie: %s\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Content-Length: 0\r\n"
						"Cache-Control: no-cache\r\n\r\n",
										purple_url_encode(xd->xfer_idstring_for_relay),
										purple_normalize(account, purple_account_get_username(account)),
										purple_xfer_get_remote_user(xfer),
										cookies,
										xd->host);
			}
		}
		else if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE && xd->status_15 == HEAD_REPLY_RECEIVED)
		{
			if(xd->info_val_249 == 1)
				{
				/* receiving file via p2p, connected as client */
				initial_buffer = g_strdup_printf("GET /%s HTTP/1.1\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Connection: Keep-Alive\r\n\r\n",
						xd->path, xd->host);
			}
			else
				{
				/* receiving file via relaying */
				initial_buffer = g_strdup_printf("GET /relay?token=%s&sender=%s&recver=%s HTTP/1.1\r\n"
						"Cookie: %s\r\n"
						"User-Agent: " YAHOO_CLIENT_USERAGENT "\r\n"
						"Host: %s\r\n"
						"Connection: Keep-Alive\r\n\r\n",
										purple_url_encode(xd->xfer_idstring_for_relay),
										purple_normalize(account, purple_account_get_username(account)),
										purple_xfer_get_remote_user(xfer),
										cookies,
										xd->host);
			}
		}
		else
		{
			purple_debug_error("yahoo", "Unrecognized yahoo file transfer mode and stage (ymsg15):%d,%d\n", purple_xfer_get_type(xfer), xd->status_15);
			g_free(cookies);
			return;
		}
		xd->txbuf = (guchar*) initial_buffer;
		xd->txbuflen = strlen(initial_buffer);
		xd->txbuf_written = 0;
		g_free(cookies);
	}

	if (!xd->tx_handler)
	{
		xd->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE,
			yahoo_xfer_send_cb_15, xfer);
		yahoo_xfer_send_cb_15(xfer, source, PURPLE_INPUT_WRITE);
	}
}

static void yahoo_p2p_ft_POST_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;

	xfer = data;
	if (!(xd = purple_xfer_get_protocol_data(xfer))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_input_remove(xd->input_event);
	xd->status_15 = TRANSFER_PHASE;
	purple_xfer_set_fd(xfer, source);
	purple_xfer_start(xfer, source, NULL, 0);
}

static void yahoo_p2p_ft_HEAD_GET_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	guchar buf[1024];
	int len;
	char *url_head;
	char *url_get;
	time_t unix_time;
	char *time_str;

	xfer = data;
	if (!(xd = purple_xfer_get_protocol_data(xfer))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	len = read(source, buf, sizeof(buf));
	if ((len < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
		return ; /* No Worries*/
	else if (len <= 0)	{
		purple_debug_warning("yahoo","p2p-ft: Error in connection, or host disconnected\n");
		purple_input_remove(xd->input_event);
		purple_xfer_cancel_remote(xfer);
		return;
	}

	url_head = g_strdup_printf("HEAD %s", xd->xfer_url);
	url_get = g_strdup_printf("GET %s", xd->xfer_url);

	if( strncmp(url_head, (char *)buf, strlen(url_head)) == 0 )
		xd->status_15 = P2P_HEAD_REQUESTED;
	else if( strncmp(url_get, (char *)buf, strlen(url_get)) == 0 )
		xd->status_15 = P2P_GET_REQUESTED;
	else	{
		purple_debug_warning("yahoo","p2p-ft: Wrong HEAD/GET request from peer, disconnecting host\n");
		purple_input_remove(xd->input_event);
		purple_xfer_cancel_remote(xfer);
		g_free(url_head);
		return;
	}

	unix_time = time(NULL);
	time_str = ctime(&unix_time);
	time_str[strlen(time_str) - 1] = '\0';

	if (xd->txbuflen == 0)	{
		gchar *initial_buffer = g_strdup_printf("HTTP/1.0 200 OK\r\n"
		                            "Date: %s GMT\r\n"
		                            "Server: Y!/1.0\r\n"
		                            "MIME-version: 1.0\r\n"
		                            "Last-modified: %s GMT\r\n"
		                            "Content-length: %" G_GOFFSET_FORMAT "\r\n\r\n",
		                            time_str, time_str, purple_xfer_get_size(xfer));
		xd->txbuf = (guchar *)initial_buffer;
		xd->txbuflen = strlen(initial_buffer);
		xd->txbuf_written = 0;
	}

	if (!xd->tx_handler)	{
		xd->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE, yahoo_xfer_send_cb_15, xfer);
		yahoo_xfer_send_cb_15(xfer, source, PURPLE_INPUT_WRITE);
	}

	g_free(url_head);
	g_free(url_get);
}

static void yahoo_p2p_ft_server_send_connected_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	int acceptfd;
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;

	xfer = data;
	if (!(xd = purple_xfer_get_protocol_data(xfer))) {
		purple_xfer_cancel_remote(xfer);
		return;
	}

	acceptfd = accept(source, NULL, 0);
	if(acceptfd == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	else if(acceptfd == -1) {
		purple_debug_warning("yahoo","yahoo_p2p_server_send_connected_cb: accept: %s\n", g_strerror(errno));
		purple_xfer_cancel_remote(xfer);
		/* remove watcher and close p2p ft server */
		purple_input_remove(xd->yahoo_p2p_ft_server_watcher);
		close(xd->yahoo_local_p2p_ft_server_fd);
		return;
	}

	/* remove watcher and close p2p ft server */
	purple_input_remove(xd->yahoo_p2p_ft_server_watcher);
	close(xd->yahoo_local_p2p_ft_server_fd);

	/* Add an Input Read event to the file descriptor */
	purple_xfer_set_fd(xfer, acceptfd);
	if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE)
		xd->input_event = purple_input_add(acceptfd, PURPLE_INPUT_READ, yahoo_p2p_ft_POST_cb, data);
	else
		xd->input_event = purple_input_add(acceptfd, PURPLE_INPUT_READ, yahoo_p2p_ft_HEAD_GET_cb, data);
}

static void yahoo_p2p_ft_server_listen_cb(int listenfd, gpointer data)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	struct yahoo_packet *pkt;
	PurpleAccount *account;
	YahooData *yd;
	gchar *filename;
	const char *local_ip;
	gchar *url_to_send = NULL;
	char *filename_without_spaces = NULL;

	xfer = data;
	if (!(xd = purple_xfer_get_protocol_data(xfer)) || (listenfd == -1))	{
		purple_debug_warning("yahoo","p2p: error starting server for p2p file transfer\n");
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if( (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) || (xd->status_15 != P2P_HEAD_REPLIED) )	{
		yd = purple_connection_get_protocol_data(xd->gc);
		account = purple_connection_get_account(xd->gc);
		local_ip = purple_network_get_my_ip(listenfd);
		xd->yahoo_local_p2p_ft_server_port = purple_network_get_port_from_fd(listenfd);

		filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
		filename_without_spaces = g_strdup(filename);
		purple_util_chrreplace(filename_without_spaces, ' ', '+');
		xd->xfer_url = g_strdup_printf("/Messenger.%s.%d000%s?AppID=Messenger&UserID=%s&K=lc9lu2u89gz1llmplwksajkjx", purple_xfer_get_remote_user(xfer), (int)time(NULL), filename_without_spaces, purple_xfer_get_remote_user(xfer));
		url_to_send = g_strdup_printf("http://%s:%d%s", local_ip, xd->yahoo_local_p2p_ft_server_port, xd->xfer_url);

		if(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) {
			xd->info_val_249 = 2;	/* 249=2: we are p2p server, and receiving file */
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_ACC_15,
				YAHOO_STATUS_AVAILABLE, yd->session_id);
			yahoo_packet_hash(pkt, "ssssis",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xd->xfer_peer_idstring,
				27, purple_xfer_get_filename(xfer),
				249, 2,
				250, url_to_send);
		}
		else	{
			xd->info_val_249 = 1;	/* 249=1: we are p2p server, and sending file */
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
			yahoo_packet_hash(pkt, "ssssis",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xd->xfer_peer_idstring,
				27,  filename,
				249, 1,
				250, url_to_send);
		}

		yahoo_packet_send_and_free(pkt, yd);

		g_free(filename);
		g_free(url_to_send);
		g_free(filename_without_spaces);
	}

	/* Add an Input Read event to the file descriptor */
	xd->yahoo_local_p2p_ft_server_fd = listenfd;
	xd->yahoo_p2p_ft_server_watcher = purple_input_add(listenfd, PURPLE_INPUT_READ, yahoo_p2p_ft_server_send_connected_cb, data);
}

/* send (p2p) file transfer information */
static void yahoo_p2p_client_send_ft_info(PurpleConnection *gc, PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xd;
	struct yahoo_packet *pkt;
	PurpleAccount *account;
	YahooData *yd;
	gchar *filename;
	struct yahoo_p2p_data *p2p_data;

	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;

	account = purple_connection_get_account(gc);
	yd = purple_connection_get_protocol_data(gc);

	p2p_data = g_hash_table_lookup(yd->peers, purple_xfer_get_remote_user(xfer));
	if( p2p_data->connection_type == YAHOO_P2P_WE_ARE_SERVER )
		if(purple_network_listen_range(0, 0, AF_UNSPEC, SOCK_STREAM, TRUE, yahoo_p2p_ft_server_listen_cb, xfer))
			return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
	filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));

	yahoo_packet_hash(pkt, "ssssi",
		1, purple_normalize(account, purple_account_get_username(account)),
		5, purple_xfer_get_remote_user(xfer),
		265, xd->xfer_peer_idstring,
		27,  filename,
		249, 2);	/* 249=2: we are p2p client */
	xd->info_val_249 = 2;
	yahoo_packet_send_and_free(pkt, yd);

	g_free(filename);
}

void yahoo_process_filetrans_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *from = NULL;
	char *to = NULL;
	char *imv = NULL;
	long val_222 = 0L;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	char *service = NULL;
	char *filename = NULL;
	char *xfer_peer_idstring = NULL;
	char *utf8_filename;
	goffset filesize = G_GOFFSET_CONSTANT(0);
	GSList *l;
	GSList *filename_list = NULL;
	GSList *size_list = NULL;
	int nooffiles = 0;

	yd = purple_connection_get_protocol_data(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			from = pair->value;
			break;
		case 5:
			to = pair->value;
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 27:
			filename_list = g_slist_prepend(filename_list, g_strdup(pair->value));
			nooffiles++;
			break;
		case 28:
			size_list = g_slist_prepend(size_list, g_strdup(pair->value));
			break;
		case 222:
			val_222 = atol(pair->value);
			/* 1=send, 2=cancel, 3=accept, 4=reject */
			break;

		/* check for p2p and imviron .... not sure it comes by this service packet. Since it was bundled with filexfer in old ymsg version, still keeping it. */
		case 49:
			service = pair->value;
			break;
		case 63:
			imv = pair->value;
			break;
		/* end check */

		}
	}
	if(!xfer_peer_idstring)
		return;

	if(val_222 == 2 || val_222 == 4)
	{
		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map,
								   xfer_peer_idstring);
		if(!xfer) return;
		purple_xfer_cancel_remote(xfer);
		return;
	}
	if(val_222 == 3)
	{
		PurpleAccount *account;
		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map,
								   xfer_peer_idstring);
		if(!xfer)
			return;
		/*
		*	In the file trans info packet that we must reply with, we are
		*	supposed to mention the ip address...
		*	purple connect does not give me a way of finding the ip address...
		*	so, purple dnsquery is used... but retries, trying with next ip
		*	address etc. is not implemented..TODO
		*/

		/* To send through p2p */
		if( g_hash_table_lookup(yd->peers, from) )	{
			/* send p2p file transfer information */
			yahoo_p2p_client_send_ft_info(gc, xfer);
			return;
		}

		account = purple_connection_get_account(gc);
		if (yd->jp)
		{
			purple_dnsquery_a(account, YAHOOJP_XFER_RELAY_HOST,
							YAHOOJP_XFER_RELAY_PORT,
							yahoo_xfer_dns_connected_15, xfer);
		}
		else
		{
			purple_dnsquery_a(account, YAHOO_XFER_RELAY_HOST,
							YAHOO_XFER_RELAY_PORT,
							yahoo_xfer_dns_connected_15, xfer);
		}
		return;
	}

	/* processing for p2p and imviron .... not sure it comes by this service packet. Since it was bundled with filexfer in old ymsg version, still keeping it. */
	/*
	* The remote user has changed their IMVironment.  We
	* record it for later use.
	*/
	if (from && imv && service && (strcmp("IMVIRONMENT", service) == 0)) {
		g_hash_table_replace(yd->imvironments, g_strdup(from), g_strdup(imv));
		return;
	}

	if (pkt->service == YAHOO_SERVICE_P2PFILEXFER) {
		if (service && (strcmp("FILEXFER", service) != 0)) {
			purple_debug_misc("yahoo", "unhandled service 0x%02x\n", pkt->service);
			return;
		}
	}
	/* end processing */

	if(!filename_list)
		return;
	/* have to change list into order in which client at other end sends */
	filename_list = g_slist_reverse(filename_list);
	size_list = g_slist_reverse(size_list);
	filename = filename_list->data;
	filesize = g_ascii_strtoll(size_list->data, NULL, 10);

	if(!from) return;
	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->version = 15;
	xfer_data->firstoflist = TRUE;
	xfer_data->gc = gc;
	xfer_data->xfer_peer_idstring = g_strdup(xfer_peer_idstring);
	xfer_data->filename_list = filename_list;
	xfer_data->size_list = size_list;

	/* Build the file transfer handle. */
	xfer = purple_xfer_new(purple_connection_get_account(gc), PURPLE_XFER_RECEIVE, from);
	if (xfer == NULL)
	{
		g_free(xfer_data);
		g_return_if_reached();
	}

	/* Set the info about the incoming file. */
	utf8_filename = yahoo_string_decode(gc, filename, TRUE);
	purple_xfer_set_filename(xfer, utf8_filename);
	g_free(utf8_filename);
	purple_xfer_set_size(xfer, filesize);

	purple_xfer_set_protocol_data(xfer, xfer_data);

	/* Setup our I/O op functions */
	purple_xfer_set_init_fnc(xfer,        yahoo_xfer_init_15);
	purple_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
	purple_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
	purple_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	purple_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	purple_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
	purple_xfer_set_write_fnc(xfer,       yahoo_xfer_write);
	purple_xfer_set_request_denied_fnc(xfer,yahoo_xfer_cancel_recv);

	g_hash_table_insert(yd->xfer_peer_idstring_map,
						xfer_data->xfer_peer_idstring,
						xfer);

	if(nooffiles > 1) {
		gchar* message;
		message = g_strdup_printf(_("%s is trying to send you a group of %d files.\n"), purple_xfer_get_remote_user(xfer), nooffiles);
		purple_xfer_conversation_write(xfer, message, FALSE);
		g_free(message);
	}
	/* Now perform the request */
	purple_xfer_request(xfer);
}

void yahoo_process_filetrans_info_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *from = NULL;
	char *to = NULL;
	char *url = NULL;
	long val_249 = 0;
	long val_66 = 0;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	char *filename = NULL;
	char *xfer_peer_idstring = NULL;
	char *xfer_idstring_for_relay = NULL;
	GSList *l;
	struct yahoo_packet *pkt_to_send;
	struct yahoo_p2p_data *p2p_data;

	yd = purple_connection_get_protocol_data(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			from = pair->value;
			break;
		case 5:
			to = pair->value;
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 27:
			filename = pair->value;
			break;
		case 66:
			val_66 = strtol(pair->value, NULL, 10);
			break;
		case 249:
			val_249 = strtol(pair->value, NULL, 10);
			/* 249 has value 1 or 2 when doing p2p transfer and value 3 when relaying through yahoo server */
			break;
		case 250:
			url = pair->value;
			break;
		case 251:
			xfer_idstring_for_relay = pair->value;
			break;
		}
	}

	if(!xfer_peer_idstring)
		return;

	xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xfer_peer_idstring);

	if(!xfer) return;

	if(val_66==-1)
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data = purple_xfer_get_protocol_data(xfer);

	xfer_data->info_val_249 = val_249;
	xfer_data->xfer_idstring_for_relay = g_strdup(xfer_idstring_for_relay);
	if(val_249 == 1 || val_249 == 3) {
		PurpleAccount *account;
		if (!purple_url_parse(url, &(xfer_data->host), &(xfer_data->port), &(xfer_data->path), NULL, NULL)) {
			purple_xfer_cancel_remote(xfer);
			return;
		}

		account = purple_connection_get_account(xfer_data->gc);

		pkt_to_send = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_ACC_15,
			YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash(pkt_to_send, "ssssis",
			1, purple_normalize(account, purple_account_get_username(account)),
			5, purple_xfer_get_remote_user(xfer),
			265, xfer_data->xfer_peer_idstring,
			27, purple_xfer_get_filename(xfer),
			249, xfer_data->info_val_249,
			251, xfer_data->xfer_idstring_for_relay);

		yahoo_packet_send_and_free(pkt_to_send, yd);

		if (purple_proxy_connect(gc, account, xfer_data->host, xfer_data->port,
			yahoo_xfer_connected_15, xfer) == NULL) {
			purple_notify_error(gc, NULL, _("File Transfer Failed"),
				_("Unable to establish file descriptor."));
			purple_xfer_cancel_remote(xfer);
		}
	}
	else if(val_249 == 2)	{
		p2p_data = g_hash_table_lookup(yd->peers, purple_xfer_get_remote_user(xfer));
		if( !( p2p_data && (p2p_data->connection_type == YAHOO_P2P_WE_ARE_SERVER) ) )	{
			purple_xfer_cancel_remote(xfer);
			return;
		}
		if(!purple_network_listen_range(0, 0, AF_UNSPEC, SOCK_STREAM, TRUE, yahoo_p2p_ft_server_listen_cb, xfer)) {
			purple_xfer_cancel_remote(xfer);
			return;
		}
	}
}

/* TODO: Check filename etc. No probs till some hacker comes in the way */
void yahoo_process_filetrans_acc_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	gchar *xfer_peer_idstring = NULL;
	gchar *xfer_idstring_for_relay = NULL;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	GSList *l;
	PurpleAccount *account;
	long val_66 = 0;
	gchar *url = NULL;
	int val_249 = 0;

	yd = purple_connection_get_protocol_data(gc);
	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 251:
			xfer_idstring_for_relay = pair->value;
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 66:
			val_66 = atol(pair->value);
			break;
		case 249:
			val_249 = atol(pair->value);
			break;
		case 250:
			url = pair->value;	/* we get a p2p url here when sending file, connected as client */
			break;
		}
	}

	xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xfer_peer_idstring);
	if(!xfer) return;

	if(val_66 == -1 || ( (!(xfer_idstring_for_relay)) && (val_249 != 2) ))
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if( (val_249 == 2) && (!(url)) )
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data = purple_xfer_get_protocol_data(xfer);
	if(url)
		purple_url_parse(url, &(xfer_data->host), &(xfer_data->port), &(xfer_data->path), NULL, NULL);

	xfer_data->xfer_idstring_for_relay = g_strdup(xfer_idstring_for_relay);
	xfer_data->status_15 = ACCEPTED;
	account = purple_connection_get_account(gc);

	if (purple_proxy_connect(gc, account, xfer_data->host, xfer_data->port,
		yahoo_xfer_connected_15, xfer) == NULL)
	{
		purple_notify_error(gc, NULL, _("File Transfer Failed"),_("Unable to connect"));
			purple_xfer_cancel_remote(xfer);
	}
}

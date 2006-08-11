/*
 * @file yahoo_filexfer.c Yahoo Filetransfer
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

#include "prpl.h"
#include "internal.h"
#include "util.h"
#include "debug.h"
#include "notify.h"
#include "proxy.h"
#include "ft.h"
#include "yahoo.h"
#include "yahoo_packet.h"
#include "yahoo_filexfer.h"
#include "yahoo_doodle.h"

struct yahoo_xfer_data {
	gchar *host;
	gchar *path;
	int port;
	GaimConnection *gc;
	long expires;
	gboolean started;
	gchar *txbuf;
	gsize txbuflen;
	gsize txbuf_written;
	guint tx_handler;
	gchar *rxqueue;
	guint rxlen;
};

static void yahoo_xfer_data_free(struct yahoo_xfer_data *xd)
{
	g_free(xd->host);
	g_free(xd->path);
	g_free(xd->txbuf);
	if (xd->tx_handler)
		gaim_input_remove(xd->tx_handler);
	g_free(xd);
}

static void yahoo_receivefile_send_cb(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXfer *xfer;
	struct yahoo_xfer_data *xd;
	int remaining, written;

	xfer = data;
	xd = xfer->data;

	remaining = xd->txbuflen - xd->txbuf_written;
	written = write(xfer->fd, xd->txbuf + xd->txbuf_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		gaim_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	if (written < remaining) {
		xd->txbuf_written += written;
		return;
	}

	gaim_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	g_free(xd->txbuf);
	xd->txbuf = NULL;
	xd->txbuflen = 0;

	gaim_xfer_start(xfer, source, NULL, 0);

}

static void yahoo_receivefile_connected(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXfer *xfer;
	struct yahoo_xfer_data *xd;

	gaim_debug(GAIM_DEBUG_INFO, "yahoo",
			   "AAA - in yahoo_receivefile_connected\n");
	if (!(xfer = data))
		return;
	if (!(xd = xfer->data))
		return;
	if ((source < 0) || (xd->path == NULL) || (xd->host == NULL)) {
		gaim_xfer_error(GAIM_XFER_RECEIVE, gaim_xfer_get_account(xfer),
				xfer->who, _("Unable to connect."));
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	xfer->fd = source;

	/* The first time we get here, assemble the tx buffer */
	if (xd->txbuflen == 0) {
		xd->txbuf = g_strdup_printf("GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n",
			      xd->path, xd->host);
		xd->txbuflen = strlen(xd->txbuf);
		xd->txbuf_written = 0;
	}

	if (!xd->tx_handler)
	{
		xd->tx_handler = gaim_input_add(source, GAIM_INPUT_WRITE,
			yahoo_receivefile_send_cb, xfer);
		yahoo_receivefile_send_cb(xfer, source, GAIM_INPUT_WRITE);
	}
}

static void yahoo_sendfile_send_cb(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXfer *xfer;
	struct yahoo_xfer_data *xd;
	int written, remaining;

	xfer = data;
	xd = xfer->data;

	remaining = xd->txbuflen - xd->txbuf_written;
	written = write(xfer->fd, xd->txbuf + xd->txbuf_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		gaim_debug_error("yahoo", "Unable to write in order to start ft errno = %d\n", errno);
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	if (written < remaining) {
		xd->txbuf_written += written;
		return;
	}

	gaim_input_remove(xd->tx_handler);
	xd->tx_handler = 0;
	g_free(xd->txbuf);
	xd->txbuf = NULL;
	xd->txbuflen = 0;

	gaim_xfer_start(xfer, source, NULL, 0);
}

static void yahoo_sendfile_connected(gpointer data, gint source, GaimInputCondition condition)
{
	GaimXfer *xfer;
	struct yahoo_xfer_data *xd;
	struct yahoo_packet *pkt;
	gchar *size, *filename, *encoded_filename, *header;
	guchar *pkt_buf;
	const char *host;
	int port;
	size_t content_length, header_len, pkt_buf_len;
	GaimConnection *gc;
	GaimAccount *account;
	struct yahoo_data *yd;

	gaim_debug(GAIM_DEBUG_INFO, "yahoo",
			   "AAA - in yahoo_sendfile_connected\n");
	if (!(xfer = data))
		return;
	if (!(xd = xfer->data))
		return;

	if (source < 0) {
		gaim_xfer_error(GAIM_XFER_RECEIVE, gaim_xfer_get_account(xfer),
				xfer->who, _("Unable to connect."));
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	xfer->fd = source;

	/* Assemble the tx buffer */
	gc = xd->gc;
	account = gaim_connection_get_account(gc);
	yd = gc->proto_data;

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANSFER,
		YAHOO_STATUS_AVAILABLE, yd->session_id);

	size = g_strdup_printf("%" G_GSIZE_FORMAT, gaim_xfer_get_size(xfer));
	filename = g_path_get_basename(gaim_xfer_get_local_filename(xfer));
	encoded_filename = yahoo_string_encode(gc, filename, NULL);

	yahoo_packet_hash(pkt, "sssss", 0, gaim_connection_get_display_name(gc),
	  5, xfer->who, 14, "", 27, encoded_filename, 28, size);
	g_free(size);
	g_free(encoded_filename);
	g_free(filename);

	content_length = YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt);

	pkt_buf_len = yahoo_packet_build(pkt, 8, FALSE, &pkt_buf);
	yahoo_packet_free(pkt);

	host = gaim_account_get_string(account, "xfer_host", YAHOO_XFER_HOST);
	port = gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT);
	header = g_strdup_printf(
		"POST http://%s:%d/notifyft HTTP/1.0\r\n"
		"Content-length: %" G_GSIZE_FORMAT "\r\n"
		"Host: %s:%d\r\n"
		"Cookie: Y=%s; T=%s\r\n"
		"\r\n",
		host, port, content_length + 4 + gaim_xfer_get_size(xfer),
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
		xd->tx_handler = gaim_input_add(source, GAIM_INPUT_WRITE,
										yahoo_sendfile_send_cb, xfer);
		yahoo_sendfile_send_cb(xfer, source, GAIM_INPUT_WRITE);
	}
}

static void yahoo_xfer_init(GaimXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;
	GaimConnection *gc;
	GaimAccount *account;
	struct yahoo_data *yd;

	xfer_data = xfer->data;
	gc = xfer_data->gc;
	yd = gc->proto_data;
	account = gaim_connection_get_account(gc);

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) {
		if (yd->jp) {
			if (gaim_proxy_connect(account, gaim_account_get_string(account, "xferjp_host",  YAHOOJP_XFER_HOST),
			                       gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
			                       yahoo_sendfile_connected, xfer) == -1)
			{
				gaim_notify_error(gc, NULL, _("File Transfer Failed"),
				                _("Unable to establish file descriptor."));
				gaim_xfer_cancel_remote(xfer);
			}
		} else {
			if (gaim_proxy_connect(account, gaim_account_get_string(account, "xfer_host",  YAHOO_XFER_HOST),
			                       gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
			                       yahoo_sendfile_connected, xfer) == -1)
			{
				gaim_notify_error(gc, NULL, _("File Transfer Failed"),
				                _("Unable to establish file descriptor."));
				gaim_xfer_cancel_remote(xfer);
			}
		}
	} else {
		xfer->fd = gaim_proxy_connect(account, xfer_data->host, xfer_data->port,
		                              yahoo_receivefile_connected, xfer);
		if (xfer->fd == -1) {
			gaim_notify_error(gc, NULL, _("File Transfer Failed"),
			             _("Unable to establish file descriptor."));
			gaim_xfer_cancel_remote(xfer);
		}
	}
}

static void yahoo_xfer_start(GaimXfer *xfer)
{
	/* We don't need to do anything here, do we? */
}

static void yahoo_xfer_end(GaimXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = xfer->data;

	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	xfer->data = NULL;

}

static guint calculate_length(const gchar *l, size_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (!g_ascii_isdigit(l[i]))
			continue;
		return strtol(l + i, NULL, 10);
	}
	return 0;
}

static gssize yahoo_xfer_read(guchar **buffer, GaimXfer *xfer)
{
	gchar buf[4096];
	gssize len;
	gchar *start = NULL;
	gchar *length;
	gchar *end;
	int filelen;
	struct yahoo_xfer_data *xd = xfer->data;

	if (gaim_xfer_get_type(xfer) != GAIM_XFER_RECEIVE) {
		return 0;
	}

	len = read(xfer->fd, buf, sizeof(buf));

	if (len <= 0) {
		if ((gaim_xfer_get_size(xfer) > 0) &&
		    (gaim_xfer_get_bytes_sent(xfer) >= gaim_xfer_get_size(xfer))) {
			gaim_xfer_set_completed(xfer, TRUE);
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
				gaim_xfer_set_size(xfer, filelen);
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

static gssize yahoo_xfer_write(const guchar *buffer, size_t size, GaimXfer *xfer)
{
	gssize len;
	struct yahoo_xfer_data *xd = xfer->data;

	if (!xd)
		return -1;

	if (gaim_xfer_get_type(xfer) != GAIM_XFER_SEND) {
		return -1;
	}

	len = write(xfer->fd, buffer, size);

	if (len == -1) {
		if (gaim_xfer_get_bytes_sent(xfer) >= gaim_xfer_get_size(xfer))
			gaim_xfer_set_completed(xfer, TRUE);
		if ((errno != EAGAIN) && (errno != EINTR))
			return -1;
		return 0;
	}

	if ((gaim_xfer_get_bytes_sent(xfer) + len) >= gaim_xfer_get_size(xfer))
		gaim_xfer_set_completed(xfer, TRUE);

	return len;
}

static void yahoo_xfer_cancel_send(GaimXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = xfer->data;

	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	xfer->data = NULL;
}

static void yahoo_xfer_cancel_recv(GaimXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = xfer->data;

	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	xfer->data = NULL;
}

void yahoo_process_p2pfilexfer(GaimConnection *gc, struct yahoo_packet *pkt)
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

		if(pair->key == 5)         /* Get who the packet is for */
			me = pair->value;

		if(pair->key == 4)         /* Get who the packet is from */
			from = pair->value;

		if(pair->key == 49)        /* Get the type of service */
			service = pair->value;

		if(pair->key == 14)        /* Get the 'message' of the packet */
			message = pair->value;

		if(pair->key == 13)        /* Get the command associated with this packet */
			command = pair->value;

		if(pair->key == 63)        /* IMVironment name and version */
			imv = pair->value;

		if(pair->key == 64)        /* Not sure, but it does vary with initialization of Doodle */
			unknown = pair->value; /* So, I'll keep it (for a little while atleast) */

		l = l->next;
	}

	/* If this packet is an IMVIRONMENT, handle it accordingly */
	if(service != NULL && imv != NULL && !strcmp(service, "IMVIRONMENT"))
	{
		/* Check for a Doodle packet and handle it accordingly */
		if(!strcmp(imv, "doodle;11"))
			yahoo_doodle_process(gc, me, from, command, message);

		/* If an IMVIRONMENT packet comes without a specific imviroment name */
		if(!strcmp(imv, ";0"))
		{
			/* It is unfortunately time to close all IMVironments with the remote client */
			yahoo_doodle_command_got_shutdown(gc, from);
		}
	}
}

void yahoo_process_filetransfer(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *from = NULL;
	char *to = NULL;
	char *msg = NULL;
	char *url = NULL;
	char *imv = NULL;
	long expires = 0;
	GaimXfer *xfer;
	struct yahoo_data *yd;
	struct yahoo_xfer_data *xfer_data;
	char *service = NULL;
	char *filename = NULL;
	unsigned long filesize = 0L;
	GSList *l;

	yd = gc->proto_data;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 4)
			from = pair->value;
		if (pair->key == 5)
			to = pair->value;
		if (pair->key == 14)
			msg = pair->value;
		if (pair->key == 20)
			url = pair->value;
		if (pair->key == 38)
			expires = strtol(pair->value, NULL, 10);
		if (pair->key == 27)
			filename = pair->value;
		if (pair->key == 28)
			filesize = atol(pair->value);
		if (pair->key == 49)
			service = pair->value;
		if (pair->key == 63)
			imv = pair->value;
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
			gaim_debug_misc("yahoo", "unhandled service 0x%02x\n", pkt->service);
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
	if (!gaim_url_parse(url, &(xfer_data->host), &(xfer_data->port), &(xfer_data->path), NULL, NULL)) {
		g_free(xfer_data);
		return;
	}

	gaim_debug_misc("yahoo_filexfer", "Host is %s, port is %d, path is %s, and the full url was %s.\n",
	                xfer_data->host, xfer_data->port, xfer_data->path, url);

	/* Build the file transfer handle. */
	xfer = gaim_xfer_new(gc->account, GAIM_XFER_RECEIVE, from);
	xfer->data = xfer_data;

	/* Set the info about the incoming file. */
	if (filename) {
		char *utf8_filename = yahoo_string_decode(gc, filename, TRUE);
		gaim_xfer_set_filename(xfer, utf8_filename);
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
			gaim_xfer_set_filename(xfer, utf8_filename);
			g_free(utf8_filename);
			filename = NULL;
		}
	}

	gaim_xfer_set_size(xfer, filesize);

	/* Setup our I/O op functions */
	gaim_xfer_set_init_fnc(xfer,        yahoo_xfer_init);
	gaim_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
	gaim_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
	gaim_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	gaim_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	gaim_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
	gaim_xfer_set_write_fnc(xfer,       yahoo_xfer_write);

	/* Now perform the request */
	gaim_xfer_request(xfer);
}

GaimXfer *yahoo_new_xfer(GaimConnection *gc, const char *who)
{
	GaimXfer *xfer;
	struct yahoo_xfer_data *xfer_data;
	
	g_return_val_if_fail(who != NULL, NULL);
	
	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->gc = gc;
	
	/* Build the file transfer handle. */
	xfer = gaim_xfer_new(gc->account, GAIM_XFER_SEND, who);
	xfer->data = xfer_data;
	
	/* Setup our I/O op functions */
	gaim_xfer_set_init_fnc(xfer,        yahoo_xfer_init);
	gaim_xfer_set_start_fnc(xfer,       yahoo_xfer_start);
	gaim_xfer_set_end_fnc(xfer,         yahoo_xfer_end);
	gaim_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	gaim_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	gaim_xfer_set_read_fnc(xfer,        yahoo_xfer_read);
	gaim_xfer_set_write_fnc(xfer,       yahoo_xfer_write);

	return xfer;
}

void yahoo_send_file(GaimConnection *gc, const char *who, const char *file)
{
	GaimXfer *xfer = yahoo_new_xfer(gc, who);

	g_return_if_fail(xfer != NULL);

	/* Now perform the request */
	if (file)
		gaim_xfer_request_accepted(xfer, file);
	else
		gaim_xfer_request(xfer);
}

/*
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
 *
 */

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "debug.h"
#include "prpl.h"
#include "proxy.h"
#include "util.h"

#include "yahoo.h"
#include "yahoo_packet.h"
#include "yahoo_friend.h"
#include "yahoo_picture.h"


struct yahoo_fetch_picture_data {
	GaimConnection *gc;
	char *who;
	int checksum;
};

static void
yahoo_fetch_picture_cb(GaimUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *pic_data, size_t len, const gchar *error_message)
{
	struct yahoo_fetch_picture_data *d;
	struct yahoo_data *yd;
	GaimBuddy *b;

	d = user_data;
	yd = d->gc->proto_data;
	yd->url_datas = g_slist_remove(yd->url_datas, url_data);

	if (error_message != NULL) {
		gaim_debug_error("yahoo", "Fetching buddy icon failed: %s\n", error_message);
	} else if (len == 0) {
		gaim_debug_error("yahoo", "Fetched an icon with length 0.  Strange.\n");
	} else {
		gaim_buddy_icons_set_for_user(gaim_connection_get_account(d->gc), d->who, (void *)pic_data, len);
		b = gaim_find_buddy(gaim_connection_get_account(d->gc), d->who);
		if (b)
			gaim_blist_node_set_int((GaimBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY, d->checksum);
	}

	g_free(d->who);
	g_free(d);
}

void yahoo_process_picture(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd;
	GSList *l = pkt->hash;
	char *who = NULL, *us = NULL;
	gboolean got_icon_info = FALSE, send_icon_info = FALSE;
	char *url = NULL;
	int checksum = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1:
		case 4:
			who = pair->value;
			break;
		case 5:
			us = pair->value;
			break;
		case 13: {
				int tmp;
				tmp = strtol(pair->value, NULL, 10);
				if (tmp == 1) {
					send_icon_info = TRUE;
				} else if (tmp == 2) {
					got_icon_info = TRUE;
				}
				break;
			}
		case 20:
			url = pair->value;
			break;
		case 192:
			checksum = strtol(pair->value, NULL, 10);
			break;
		}

		l = l->next;
	}

	/* Yahoo IM 6 spits out 0.png as the URL if the buddy icon is not set */
	if (who && got_icon_info && url && !strncasecmp(url, "http://", 7)) {
		/* TODO: make this work p2p, try p2p before the url */
		GaimUtilFetchUrlData *url_data;
		struct yahoo_fetch_picture_data *data;
		GaimBuddy *b = gaim_find_buddy(gc->account, who);
		if (b && (checksum == gaim_blist_node_get_int((GaimBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY)))
			return;

		data = g_new0(struct yahoo_fetch_picture_data, 1);
		data->gc = gc;
		data->who = g_strdup(who);
		data->checksum = checksum;
		url_data = gaim_util_fetch_url(url, FALSE,
				"Mozilla/4.0 (compatible; MSIE 5.0)", FALSE,
				yahoo_fetch_picture_cb, data);
		if (url_data != NULL) {
			yd = gc->proto_data;
			yd->url_datas = g_slist_prepend(yd->url_datas, url_data);
		} else {
			g_free(data->who);
			g_free(data);
		}
	} else if (who && send_icon_info) {
		yahoo_send_picture_info(gc, who);
	}
}

void yahoo_process_picture_update(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	int icon = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			who = pair->value;
			break;
		case 5:
			/* us */
			break;
		case 206:
			icon = strtol(pair->value, NULL, 10);
			break;
		}
		l = l->next;
	}

	if (who) {
		if (icon == 2)
			yahoo_send_picture_request(gc, who);
		else if ((icon == 0) || (icon == 1)) {
			GaimBuddy *b = gaim_find_buddy(gc->account, who);
			YahooFriend *f;
			gaim_buddy_icons_set_for_user(gc->account, who, NULL, 0);
			if (b)
				gaim_blist_node_remove_setting((GaimBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
			if ((f = yahoo_friend_find(gc, who)))
				yahoo_friend_set_buddy_icon_need_request(f, TRUE);
			gaim_debug_misc("yahoo", "Setting user %s's icon to NULL.\n", who);
		}
	}
}

void yahoo_process_picture_checksum(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	int checksum = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			who = pair->value;
			break;
		case 5:
			/* us */
			break;
		case 192:
			checksum = strtol(pair->value, NULL, 10);
			break;
		}
		l = l->next;
	}

	if (who) {
		GaimBuddy *b = gaim_find_buddy(gc->account, who);
		if (b && (checksum != gaim_blist_node_get_int((GaimBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY)))
			yahoo_send_picture_request(gc, who);
	}
}

void yahoo_process_picture_upload(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	char *url = NULL;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 5:
			/* us */
			break;
		case 27:
			/* filename on our computer. */
			break;
		case 20: /* url at yahoo */
			url = pair->value;
		case 38: /* timestamp */
			break;
		}
		l = l->next;
	}

	if (url) {
		if (yd->picture_url)
			g_free(yd->picture_url);
		yd->picture_url = g_strdup(url);
		gaim_account_set_string(account, YAHOO_PICURL_SETTING, url);
		gaim_account_set_int(account, YAHOO_PICCKSUM_SETTING, yd->picture_checksum);
		yahoo_send_picture_update(gc, 2);
		yahoo_send_picture_checksum(gc);
	}
}

void yahoo_process_avatar_update(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	int avatar = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			who = pair->value;
			break;
		case 5:
			/* us */
			break;
		case 206:
			/*
			 * 0 - No icon or avatar
			 * 1 - Using an avatar
			 * 2 - Using an icon
			 */
			avatar = strtol(pair->value, NULL, 10);
			break;
		}
		l = l->next;
	}

	if (who) {
		if (avatar == 2)
			yahoo_send_picture_request(gc, who);
		else if ((avatar == 0) || (avatar == 1)) {
			GaimBuddy *b = gaim_find_buddy(gc->account, who);
			YahooFriend *f;
			gaim_buddy_icons_set_for_user(gc->account, who, NULL, 0);
			if (b)
				gaim_blist_node_remove_setting((GaimBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
			if ((f = yahoo_friend_find(gc, who)))
				yahoo_friend_set_buddy_icon_need_request(f, TRUE);
			gaim_debug_misc("yahoo", "Setting user %s's icon to NULL.\n", who);
		}
	}
}

void yahoo_send_picture_info(GaimConnection *gc, const char *who)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->picture_url) {
		gaim_debug_warning("yahoo", "Attempted to send picture info without a picture\n");
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sssssi", 1, gaim_connection_get_display_name(gc),
	                  4, gaim_connection_get_display_name(gc), 5, who,
	                  13, "2", 20, yd->picture_url, 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_request(GaimConnection *gc, const char *who)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_str(pkt, 4, gaim_connection_get_display_name(gc)); /* me */
	yahoo_packet_hash_str(pkt, 5, who); /* the other guy */
	yahoo_packet_hash_str(pkt, 13, "1"); /* 1 = request, 2 = reply */
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_checksum(GaimConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_CHECKSUM, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssi", 1, gaim_connection_get_display_name(gc),
			  212, "1", 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_update_to_user(GaimConnection *gc, const char *who, int type)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_UPDATE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssi", 1, gaim_connection_get_display_name(gc), 5, who, 206, type);
	yahoo_packet_send_and_free(pkt, yd);
}

struct yspufe {
	GaimConnection *gc;
	int type;
};

static void yahoo_send_picture_update_foreach(gpointer key, gpointer value, gpointer data)
{
	char *who = key;
	YahooFriend *f = value;
	struct yspufe *d = data;

	if (f->status != YAHOO_STATUS_OFFLINE)
		yahoo_send_picture_update_to_user(d->gc, who, d->type);
}

void yahoo_send_picture_update(GaimConnection *gc, int type)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yspufe data;

	data.gc = gc;
	data.type = type;

	g_hash_table_foreach(yd->friends, yahoo_send_picture_update_foreach, &data);
}

void yahoo_buddy_icon_upload_data_free(struct yahoo_buddy_icon_upload_data *d)
{
	gaim_debug_misc("yahoo", "In yahoo_buddy_icon_upload_data_free()\n");

	if (d->str)
		g_string_free(d->str, TRUE);
	g_free(d->filename);
	if (d->watcher)
		gaim_input_remove(d->watcher);
	if (d->fd != -1)
		close(d->fd);
	g_free(d);
}

/* we couldn't care less about the server's response, but yahoo gets grumpy if we close before it sends it */
static void yahoo_buddy_icon_upload_reading(gpointer data, gint source, GaimInputCondition condition)
{
	struct yahoo_buddy_icon_upload_data *d = data;
	GaimConnection *gc = d->gc;
	char buf[1024];
	int ret;

	if (!GAIM_CONNECTION_IS_VALID(gc)) {
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}

	ret = read(d->fd, buf, sizeof(buf));

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0)
		yahoo_buddy_icon_upload_data_free(d);
}

static void yahoo_buddy_icon_upload_pending(gpointer data, gint source, GaimInputCondition condition)
{
	struct yahoo_buddy_icon_upload_data *d = data;
	GaimConnection *gc = d->gc;
	ssize_t wrote;

	if (!GAIM_CONNECTION_IS_VALID(gc)) {
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}

	wrote = write(d->fd, d->str->str + d->pos, d->str->len - d->pos);
	if (wrote < 0 && errno == EAGAIN)
		return;
	if (wrote <= 0) {
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}
	d->pos += wrote;
	if (d->pos >= d->str->len) {
		gaim_debug_misc("yahoo", "Finished uploading buddy icon.\n");
		gaim_input_remove(d->watcher);
		d->watcher = gaim_input_add(d->fd, GAIM_INPUT_READ, yahoo_buddy_icon_upload_reading, d);
	}
}

static void yahoo_buddy_icon_upload_connected(gpointer data, gint source, const gchar *error_message)
{
	struct yahoo_buddy_icon_upload_data *d = data;
	struct yahoo_packet *pkt;
	gchar *size, *header;
	guchar *pkt_buf;
	const char *host;
	int port;
	size_t content_length, pkt_buf_len;
	GaimConnection *gc;
	GaimAccount *account;
	struct yahoo_data *yd;

	g_return_if_fail(d != NULL);

	gc = d->gc;
	account = gaim_connection_get_account(gc);
	yd = gc->proto_data;

	/* Buddy icon connect is now complete; clear the GaimProxyConnectData */
	yd->buddy_icon_connect_data = NULL;

	if (source < 0) {
		gaim_debug_error("yahoo", "Buddy icon upload failed, no file desc.\n");
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}

	pkt = yahoo_packet_new(0xc2, YAHOO_STATUS_AVAILABLE, yd->session_id);

	size = g_strdup_printf("%" G_GSIZE_FORMAT, d->str->len);
	/* 1 = me, 38 = expire time(?), 0 = me, 28 = size, 27 = filename, 14 = NULL, 29 = data */
	yahoo_packet_hash_str(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 38, "604800"); /* time til expire */
	gaim_account_set_int(account, YAHOO_PICEXPIRE_SETTING, time(NULL) + 604800);
	yahoo_packet_hash_str(pkt, 0, gaim_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 28, size);
	g_free(size);
	yahoo_packet_hash_str(pkt, 27, d->filename);
	yahoo_packet_hash_str(pkt, 14, "");

	content_length = YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt);

	host = gaim_account_get_string(account, "xfer_host", YAHOO_XFER_HOST);
	port = gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT);
	header = g_strdup_printf(
		"POST http://%s:%d/notifyft HTTP/1.0\r\n"
		"Content-length: %" G_GSIZE_FORMAT "\r\n"
		"Host: %s:%d\r\n"
		"Cookie: Y=%s; T=%s\r\n"
		"\r\n",
		host, port, content_length + 4 + d->str->len,
		host, port, yd->cookie_y, yd->cookie_t);

	/* There's no magic here, we just need to prepend in reverse order */
	g_string_prepend(d->str, "29\xc0\x80");

	pkt_buf_len = yahoo_packet_build(pkt, 8, FALSE, yd->jp, &pkt_buf);
	yahoo_packet_free(pkt);
	g_string_prepend_len(d->str, (char *)pkt_buf, pkt_buf_len);
	g_free(pkt_buf);

	g_string_prepend(d->str, header);
	g_free(header);

	d->fd = source;
	d->watcher = gaim_input_add(d->fd, GAIM_INPUT_WRITE, yahoo_buddy_icon_upload_pending, d);

	yahoo_buddy_icon_upload_pending(d, d->fd, GAIM_INPUT_WRITE);
}

void yahoo_buddy_icon_upload(GaimConnection *gc, struct yahoo_buddy_icon_upload_data *d)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	GaimProxyConnectData *connect_data = NULL;

	g_return_if_fail(d != NULL);

	if (yd->buddy_icon_connect_data) {
		/* Cancel any in-progress buddy icon upload */
		gaim_proxy_connect_cancel(yd->buddy_icon_connect_data);
		yd->buddy_icon_connect_data = NULL;
	}

	if (yd->jp) {
		if ((connect_data = gaim_proxy_connect(NULL, account, gaim_account_get_string(account, "xferjp_host",  YAHOOJP_XFER_HOST),
											   gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
											   yahoo_buddy_icon_upload_connected, d)) == NULL)
		{
			gaim_debug_error("yahoo", "Uploading our buddy icon failed to connect.\n");
			yahoo_buddy_icon_upload_data_free(d);
		}
	} else {
		if ((connect_data = gaim_proxy_connect(NULL, account, gaim_account_get_string(account, "xfer_host",  YAHOO_XFER_HOST),
											   gaim_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
											   yahoo_buddy_icon_upload_connected, d)) == NULL)
		{
			gaim_debug_error("yahoo", "Uploading our buddy icon failed to connect.\n");
			yahoo_buddy_icon_upload_data_free(d);
		}
	}

	if (connect_data) {
		yd->buddy_icon_connect_data = connect_data;
	}
}

void yahoo_set_buddy_icon(GaimConnection *gc, const char *iconfile)
{
	struct yahoo_data *yd = gc->proto_data;
	GaimAccount *account = gc->account;
	FILE *file;
	struct stat st;

	if (iconfile == NULL) {
		if (yd->picture_url)
			g_free(yd->picture_url);
		yd->picture_url = NULL;

		gaim_account_set_string(account, YAHOO_PICURL_SETTING, NULL);
		gaim_account_set_int(account, YAHOO_PICCKSUM_SETTING, 0);
		gaim_account_set_int(account, YAHOO_PICEXPIRE_SETTING, 0);
		if (yd->logged_in)
			yahoo_send_picture_update(gc, 0);
		/* TODO: check if we're connected and tell everyone we ain't not one no more */
	} else if (!g_stat(iconfile, &st)) {
		file = g_fopen(iconfile, "rb");
		if (file) {
			GString *s = g_string_sized_new(st.st_size);
			size_t len;
			struct yahoo_buddy_icon_upload_data *d;
			int oldcksum = gaim_account_get_int(account, YAHOO_PICCKSUM_SETTING, 0);
			int expire = gaim_account_get_int(account, YAHOO_PICEXPIRE_SETTING, 0);
			const char *oldurl = gaim_account_get_string(account, YAHOO_PICURL_SETTING, NULL);

			g_string_set_size(s, st.st_size);
			len = fread(s->str, 1, st.st_size, file);
			fclose(file);
			g_string_set_size(s, len);
			yd->picture_checksum = g_string_hash(s);

			if ((yd->picture_checksum == oldcksum) && (expire > (time(NULL) + 60*60*24)) &&
			    oldcksum && expire && oldurl) {
				gaim_debug_misc("yahoo", "buddy icon is up to date. Not reuploading.\n");
				g_string_free(s, TRUE);
				if (yd->picture_url)
					g_free(yd->picture_url);
				yd->picture_url = g_strdup(oldurl);
				return;
			}

			d = g_new0(struct yahoo_buddy_icon_upload_data, 1);
			d->gc = gc;
			d->str = s;
			d->fd = -1;
			d->filename = g_strdup(iconfile);

			if (!yd->logged_in) {
				yd->picture_upload_todo = d;
				return;
			}

			yahoo_buddy_icon_upload(gc, d);
		} else
			gaim_debug_error("yahoo",
				   "Can't open buddy icon file!\n");
	} else
		gaim_debug_error("yahoo",
			   "Can't stat buddy icon file!\n");
}

/*
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
	PurpleConnection *gc;
	char *who;
	int checksum;
};

static void
yahoo_fetch_picture_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *pic_data, size_t len, const gchar *error_message)
{
	struct yahoo_fetch_picture_data *d;
	struct yahoo_data *yd;
	PurpleBuddy *b;

	d = user_data;
	yd = d->gc->proto_data;
	yd->url_datas = g_slist_remove(yd->url_datas, url_data);

	if (error_message != NULL) {
		purple_debug_error("yahoo", "Fetching buddy icon failed: %s\n", error_message);
	} else if (len == 0) {
		purple_debug_error("yahoo", "Fetched an icon with length 0.  Strange.\n");
	} else {
		purple_buddy_icons_set_for_user(purple_connection_get_account(d->gc), d->who, (void *)pic_data, len);
		b = purple_find_buddy(purple_connection_get_account(d->gc), d->who);
		if (b)
			purple_blist_node_set_int((PurpleBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY, d->checksum);
	}

	g_free(d->who);
	g_free(d);
}

void yahoo_process_picture(PurpleConnection *gc, struct yahoo_packet *pkt)
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
		PurpleUtilFetchUrlData *url_data;
		struct yahoo_fetch_picture_data *data;
		PurpleBuddy *b = purple_find_buddy(gc->account, who);
		if (b && (checksum == purple_blist_node_get_int((PurpleBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY)))
			return;

		data = g_new0(struct yahoo_fetch_picture_data, 1);
		data->gc = gc;
		data->who = g_strdup(who);
		data->checksum = checksum;
		url_data = purple_util_fetch_url(url, FALSE,
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

void yahoo_process_picture_update(PurpleConnection *gc, struct yahoo_packet *pkt)
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
			PurpleBuddy *b = purple_find_buddy(gc->account, who);
			YahooFriend *f;
			purple_buddy_icons_set_for_user(gc->account, who, NULL, 0);
			if (b)
				purple_blist_node_remove_setting((PurpleBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
			if ((f = yahoo_friend_find(gc, who)))
				yahoo_friend_set_buddy_icon_need_request(f, TRUE);
			purple_debug_misc("yahoo", "Setting user %s's icon to NULL.\n", who);
		}
	}
}

void yahoo_process_picture_checksum(PurpleConnection *gc, struct yahoo_packet *pkt)
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
		PurpleBuddy *b = purple_find_buddy(gc->account, who);
		if (b && (checksum != purple_blist_node_get_int((PurpleBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY)))
			yahoo_send_picture_request(gc, who);
	}
}

void yahoo_process_picture_upload(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
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
		purple_account_set_string(account, YAHOO_PICURL_SETTING, url);
		purple_account_set_int(account, YAHOO_PICCKSUM_SETTING, yd->picture_checksum);
		yahoo_send_picture_update(gc, 2);
		yahoo_send_picture_checksum(gc);
	}
}

void yahoo_process_avatar_update(PurpleConnection *gc, struct yahoo_packet *pkt)
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
			PurpleBuddy *b = purple_find_buddy(gc->account, who);
			YahooFriend *f;
			purple_buddy_icons_set_for_user(gc->account, who, NULL, 0);
			if (b)
				purple_blist_node_remove_setting((PurpleBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
			if ((f = yahoo_friend_find(gc, who)))
				yahoo_friend_set_buddy_icon_need_request(f, TRUE);
			purple_debug_misc("yahoo", "Setting user %s's icon to NULL.\n", who);
		}
	}
}

void yahoo_send_picture_info(PurpleConnection *gc, const char *who)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->picture_url) {
		purple_debug_warning("yahoo", "Attempted to send picture info without a picture\n");
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sssssi", 1, purple_connection_get_display_name(gc),
	                  4, purple_connection_get_display_name(gc), 5, who,
	                  13, "2", 20, yd->picture_url, 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_request(PurpleConnection *gc, const char *who)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_str(pkt, 4, purple_connection_get_display_name(gc)); /* me */
	yahoo_packet_hash_str(pkt, 5, who); /* the other guy */
	yahoo_packet_hash_str(pkt, 13, "1"); /* 1 = request, 2 = reply */
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_checksum(PurpleConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_CHECKSUM, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssi", 1, purple_connection_get_display_name(gc),
			  212, "1", 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_update_to_user(PurpleConnection *gc, const char *who, int type)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_UPDATE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssi", 1, purple_connection_get_display_name(gc), 5, who, 206, type);
	yahoo_packet_send_and_free(pkt, yd);
}

struct yspufe {
	PurpleConnection *gc;
	int type;
};

static void yahoo_send_picture_update_foreach(gpointer key, gpointer value, gpointer data)
{
	const char *who = key;
	YahooFriend *f = value;
	struct yspufe *d = data;

	if (f->status != YAHOO_STATUS_OFFLINE)
		yahoo_send_picture_update_to_user(d->gc, who, d->type);
}

void yahoo_send_picture_update(PurpleConnection *gc, int type)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yspufe data;

	data.gc = gc;
	data.type = type;

	g_hash_table_foreach(yd->friends, yahoo_send_picture_update_foreach, &data);
}

void yahoo_buddy_icon_upload_data_free(struct yahoo_buddy_icon_upload_data *d)
{
	purple_debug_misc("yahoo", "In yahoo_buddy_icon_upload_data_free()\n");

	if (d->str)
		g_string_free(d->str, TRUE);
	g_free(d->filename);
	if (d->watcher)
		purple_input_remove(d->watcher);
	if (d->fd != -1)
		close(d->fd);
	g_free(d);
}

/* we couldn't care less about the server's response, but yahoo gets grumpy if we close before it sends it */
static void yahoo_buddy_icon_upload_reading(gpointer data, gint source, PurpleInputCondition condition)
{
	struct yahoo_buddy_icon_upload_data *d = data;
	PurpleConnection *gc = d->gc;
	char buf[1024];
	int ret;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}

	ret = read(d->fd, buf, sizeof(buf));

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0)
		yahoo_buddy_icon_upload_data_free(d);
}

static void yahoo_buddy_icon_upload_pending(gpointer data, gint source, PurpleInputCondition condition)
{
	struct yahoo_buddy_icon_upload_data *d = data;
	PurpleConnection *gc = d->gc;
	ssize_t wrote;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
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
		purple_debug_misc("yahoo", "Finished uploading buddy icon.\n");
		purple_input_remove(d->watcher);
		d->watcher = purple_input_add(d->fd, PURPLE_INPUT_READ, yahoo_buddy_icon_upload_reading, d);
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
	PurpleConnection *gc;
	PurpleAccount *account;
	struct yahoo_data *yd;

	gc = d->gc;
	account = purple_connection_get_account(gc);
	yd = gc->proto_data;

	/* Buddy icon connect is now complete; clear the PurpleProxyConnectData */
	yd->buddy_icon_connect_data = NULL;

	if (source < 0) {
		purple_debug_error("yahoo", "Buddy icon upload failed: %s\n", error_message);
		yahoo_buddy_icon_upload_data_free(d);
		return;
	}

	pkt = yahoo_packet_new(0xc2, YAHOO_STATUS_AVAILABLE, yd->session_id);

	size = g_strdup_printf("%" G_GSIZE_FORMAT, d->str->len);
	/* 1 = me, 38 = expire time(?), 0 = me, 28 = size, 27 = filename, 14 = NULL, 29 = data */
	yahoo_packet_hash_str(pkt, 1, purple_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 38, "604800"); /* time til expire */
	purple_account_set_int(account, YAHOO_PICEXPIRE_SETTING, time(NULL) + 604800);
	yahoo_packet_hash_str(pkt, 0, purple_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 28, size);
	g_free(size);
	yahoo_packet_hash_str(pkt, 27, d->filename);
	yahoo_packet_hash_str(pkt, 14, "");

	content_length = YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt);

	host = purple_account_get_string(account, "xfer_host", YAHOO_XFER_HOST);
	port = purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT);
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
	d->watcher = purple_input_add(d->fd, PURPLE_INPUT_WRITE, yahoo_buddy_icon_upload_pending, d);

	yahoo_buddy_icon_upload_pending(d, d->fd, PURPLE_INPUT_WRITE);
}

void yahoo_buddy_icon_upload(PurpleConnection *gc, struct yahoo_buddy_icon_upload_data *d)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;

	if (yd->buddy_icon_connect_data != NULL) {
		/* Cancel any in-progress buddy icon upload */
		purple_proxy_connect_cancel(yd->buddy_icon_connect_data);
		yd->buddy_icon_connect_data = NULL;
	}

	yd->buddy_icon_connect_data = purple_proxy_connect(NULL, account,
			yd->jp ? purple_account_get_string(account, "xferjp_host",  YAHOOJP_XFER_HOST)
			       : purple_account_get_string(account, "xfer_host",  YAHOO_XFER_HOST),
			purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT),
			yahoo_buddy_icon_upload_connected, d);

	if (yd->buddy_icon_connect_data == NULL)
	{
		purple_debug_error("yahoo", "Uploading our buddy icon failed to connect.\n");
		yahoo_buddy_icon_upload_data_free(d);
	}
}

void yahoo_set_buddy_icon(PurpleConnection *gc, const char *iconfile)
{
	struct yahoo_data *yd = gc->proto_data;
	PurpleAccount *account = gc->account;
	gchar *icondata;
	gsize len;
	GError *error = NULL;

	if (iconfile == NULL) {
		g_free(yd->picture_url);
		yd->picture_url = NULL;

		purple_account_set_string(account, YAHOO_PICURL_SETTING, NULL);
		purple_account_set_int(account, YAHOO_PICCKSUM_SETTING, 0);
		purple_account_set_int(account, YAHOO_PICEXPIRE_SETTING, 0);
		if (yd->logged_in)
			/* Tell everyone we ain't got one no more */
			yahoo_send_picture_update(gc, 0);

	} else if (g_file_get_contents(iconfile, &icondata, &len, &error)) {
		GString *s = g_string_new_len(icondata, len);
		struct yahoo_buddy_icon_upload_data *d;
		int oldcksum = purple_account_get_int(account, YAHOO_PICCKSUM_SETTING, 0);
		int expire = purple_account_get_int(account, YAHOO_PICEXPIRE_SETTING, 0);
		const char *oldurl = purple_account_get_string(account, YAHOO_PICURL_SETTING, NULL);

		g_free(icondata);
		yd->picture_checksum = g_string_hash(s);

		if ((yd->picture_checksum == oldcksum) &&
			(expire > (time(NULL) + 60*60*24)) && oldurl)
		{
			purple_debug_misc("yahoo", "buddy icon is up to date. Not reuploading.\n");
			g_string_free(s, TRUE);
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

	} else {
		purple_debug_error("yahoo",
				"Could not read buddy icon file '%s': %s\n",
				iconfile, error->message);
		g_error_free(error);
	}
}

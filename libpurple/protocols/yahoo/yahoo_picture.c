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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "buddylist.h"
#include "debug.h"
#include "http.h"
#include "prpl.h"
#include "proxy.h"
#include "util.h"

#include "libymsg.h"
#include "yahoo_packet.h"
#include "yahoo_friend.h"
#include "yahoo_picture.h"


struct yahoo_fetch_picture_data {
	PurpleConnection *gc;
	char *who;
	int checksum;
};

static void
yahoo_fetch_picture_cb(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _data)
{
	struct yahoo_fetch_picture_data *d = _data;

	if (!purple_http_response_is_successfull(response)) {
		purple_debug_error("yahoo", "Fetching buddy icon failed: %s\n",
			purple_http_response_get_error(response));
	} else {
		char *checksum = g_strdup_printf("%i", d->checksum);
		const gchar *pic_data;
		size_t pic_len;

		pic_data = purple_http_response_get_data(response, &pic_len);

		purple_buddy_icons_set_for_user(
			purple_connection_get_account(d->gc), d->who,
			g_memdup(pic_data, pic_len), pic_len, checksum);
		g_free(checksum);
	}

	g_free(d->who);
	g_free(d);
}

void yahoo_process_picture(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	YahooData *yd;
	GSList *l = pkt->hash;
	char *who = NULL;
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
		case 5: /* us */
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

	if (!who)
		return;

	if (!purple_account_privacy_check(purple_connection_get_account(gc), who)) {
		purple_debug_info("yahoo", "Picture packet from %s dropped.\n", who);
		return;
	}

	/* Yahoo IM 6 spits out 0.png as the URL if the buddy icon is not set */
	if (who && got_icon_info && url && !g_ascii_strncasecmp(url, "http://", 7)) {
		/* TODO: make this work p2p, try p2p before the url */
		struct yahoo_fetch_picture_data *data;

		data = g_new0(struct yahoo_fetch_picture_data, 1);
		data->gc = gc;
		data->who = g_strdup(who);
		data->checksum = checksum;
		yd = purple_connection_get_protocol_data(gc);
		purple_http_connection_set_add(yd->http_reqs, purple_http_get(
			gc, yahoo_fetch_picture_cb, data, url));
	} else if (who && send_icon_info) {
		yahoo_send_picture_info(gc, who);
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
		PurpleBuddy *b = purple_blist_find_buddy(purple_connection_get_account(gc), who);
		const char *locksum = NULL;

		/* FIXME: Cleanup this strtol() stuff if possible. */
		if (b) {
			locksum = purple_buddy_icons_get_checksum_for_user(b);
			if (!locksum || (checksum != strtol(locksum, NULL, 10)))
				yahoo_send_picture_request(gc, who);
		}
	}
}

void yahoo_process_picture_upload(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);
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
		g_free(yd->picture_url);
		yd->picture_url = g_strdup(url);
		purple_account_set_string(account, YAHOO_PICURL_SETTING, url);
		purple_account_set_int(account, YAHOO_PICCKSUM_SETTING, yd->picture_checksum);
		yahoo_send_picture_checksum(gc);
		yahoo_send_picture_update(gc, 2);
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
		case 206:   /* Older versions. Still needed? */
		case 213:   /* Newer versions */
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
			YahooFriend *f;
			purple_buddy_icons_set_for_user(purple_connection_get_account(gc), who, NULL, 0, NULL);
			if ((f = yahoo_friend_find(gc, who)))
				yahoo_friend_set_buddy_icon_need_request(f, TRUE);
			purple_debug_misc("yahoo", "Setting user %s's icon to NULL.\n", who);
		}
	}
}

void yahoo_send_picture_info(PurpleConnection *gc, const char *who)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	if (!yd->picture_url) {
		purple_debug_warning("yahoo", "Attempted to send picture info without a picture\n");
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssssi", 1, purple_connection_get_display_name(gc),
	                  5, who,
	                  13, "2", 20, yd->picture_url, 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_request(PurpleConnection *gc, const char *who)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash_str(pkt, 1, purple_connection_get_display_name(gc)); /* me */
	yahoo_packet_hash_str(pkt, 5, who); /* the other guy */
	yahoo_packet_hash_str(pkt, 13, "1"); /* 1 = request, 2 = reply */
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_checksum(PurpleConnection *gc)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_CHECKSUM, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "ssi", 1, purple_connection_get_display_name(gc),
			  212, "1", 192, yd->picture_checksum);
	yahoo_packet_send_and_free(pkt, yd);
}

void yahoo_send_picture_update_to_user(PurpleConnection *gc, const char *who, int type)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AVATAR_UPDATE, YAHOO_STATUS_AVAILABLE, yd->session_id);
	yahoo_packet_hash(pkt, "si", 3, who, 213, type);
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
	YahooData *yd = purple_connection_get_protocol_data(gc);
	struct yspufe data;

	data.gc = gc;
	data.type = type;

	g_hash_table_foreach(yd->friends, yahoo_send_picture_update_foreach, &data);
}

void yahoo_buddy_icon_upload_data_free(struct yahoo_buddy_icon_upload_data *d)
{
	purple_debug_misc("yahoo", "In yahoo_buddy_icon_upload_data_free()\n");

	if (d->picture_data)
		g_string_free(d->picture_data, TRUE);
	g_free(d->filename);
	g_free(d);
}

static void
yahoo_buddy_icon_upload_done(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, gpointer _d)
{
	struct yahoo_buddy_icon_upload_data *d = _d;
	PurpleConnection *gc = d->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);

	yd->picture_upload_hc = NULL;

	if (!purple_http_response_is_successfull(response))
		purple_debug_info("yahoo", "Error uploading buddy icon.\n");
	else
		purple_debug_misc("yahoo", "Finished uploading buddy icon.\n");

	yahoo_buddy_icon_upload_data_free(d);
}

static void
yahoo_buddy_icon_build_packet(struct yahoo_buddy_icon_upload_data *d)
{
	struct yahoo_packet *pkt;
	PurpleConnection *gc = d->gc;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	gchar *len_str;
	guchar *pkt_buf;
	gsize pkt_buf_len;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PICTURE_UPLOAD,
		YAHOO_STATUS_AVAILABLE, yd->session_id);

	len_str = g_strdup_printf("%" G_GSIZE_FORMAT, d->picture_data->len);
	/* 1 = me, 38 = expire time(?), 0 = me, 28 = size, 27 = filename,
	 * 14 = NULL, 29 = data
	 */
	yahoo_packet_hash_str(pkt, 1, purple_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 38, "604800"); /* time til expire */
	purple_account_set_int(purple_connection_get_account(gc),
		YAHOO_PICEXPIRE_SETTING, time(NULL) + 604800);
	yahoo_packet_hash_str(pkt, 0, purple_connection_get_display_name(gc));
	yahoo_packet_hash_str(pkt, 28, len_str);
	g_free(len_str);
	yahoo_packet_hash_str(pkt, 27, d->filename);
	yahoo_packet_hash_str(pkt, 14, "");
	/* 4 padding for the 29 key name */
	pkt_buf_len = yahoo_packet_build(pkt, 4, FALSE, yd->jp, &pkt_buf);
	yahoo_packet_free(pkt);

	/* There's no magic here, we just need to prepend in reverse order */
	g_string_prepend(d->picture_data, "29\xc0\x80");

	g_string_prepend_len(d->picture_data, (char *)pkt_buf, pkt_buf_len);
	g_free(pkt_buf);
}

void
yahoo_buddy_icon_upload(PurpleConnection *gc,
	struct yahoo_buddy_icon_upload_data *d)
{
	PurpleHttpRequest *req;
	PurpleHttpCookieJar *cjar;
	PurpleAccount *account = purple_connection_get_account(gc);
	YahooData *yd = purple_connection_get_protocol_data(gc);

	/* Cancel any in-progress buddy icon upload */
	purple_http_conn_cancel(yd->picture_upload_hc);

	req = purple_http_request_new(NULL);
	purple_http_request_set_url_printf(req, "http://%s:%d/notifyft",
		purple_account_get_string(account, "xfer_host", yd->jp ?
			YAHOOJP_XFER_HOST : YAHOO_XFER_HOST),
		purple_account_get_int(account, "xfer_port", YAHOO_XFER_PORT));
	purple_http_request_set_method(req, "POST");
	cjar = purple_http_request_get_cookie_jar(req);
	purple_http_cookie_jar_set(cjar, "T", yd->cookie_t);
	purple_http_cookie_jar_set(cjar, "Y", yd->cookie_y);
	purple_http_request_header_set(req, "Cache-Control", "no-cache");
	purple_http_request_header_set(req, "User-Agent",
		YAHOO_CLIENT_USERAGENT);

	yahoo_buddy_icon_build_packet(d);
	purple_http_request_set_contents(req, d->picture_data->str,
		d->picture_data->len);
	g_string_free(d->picture_data, TRUE);
	d->picture_data = NULL;

	yd->picture_upload_hc = purple_http_request(gc, req,
		yahoo_buddy_icon_upload_done, d);
}

static int yahoo_buddy_icon_calculate_checksum(const guchar *data, gsize len)
{
	/* This code is borrowed from Kopete, which seems to be managing to calculate
	   checksums in such a manner that Yahoo!'s servers are happy */

	const guchar *p = data;
	int checksum = 0, g, i = len;

	while(i--) {
		checksum = (checksum << 4) + *p++;

		if((g = (checksum & 0xf0000000)) != 0)
			checksum ^= g >> 23;

		checksum &= ~g;
	}

	purple_debug_misc("yahoo", "Calculated buddy icon checksum: %d\n", checksum);

	return checksum;
}

void yahoo_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);

	if (img == NULL) {
		g_free(yd->picture_url);
		yd->picture_url = NULL;

		/* TODO: don't we have to clear it on the server too?! */

		purple_account_set_string(account, YAHOO_PICURL_SETTING, NULL);
		purple_account_set_int(account, YAHOO_PICCKSUM_SETTING, 0);
		purple_account_set_int(account, YAHOO_PICEXPIRE_SETTING, 0);
		if (yd->logged_in)
			/* Tell everyone we ain't got one no more */
			yahoo_send_picture_update(gc, 0);

	} else {
		gconstpointer data = purple_imgstore_get_data(img);
		size_t len = purple_imgstore_get_size(img);
		GString *s = g_string_new_len(data, len);
		struct yahoo_buddy_icon_upload_data *d;
		int oldcksum = purple_account_get_int(account, YAHOO_PICCKSUM_SETTING, 0);
		int expire = purple_account_get_int(account, YAHOO_PICEXPIRE_SETTING, 0);
		const char *oldurl = purple_account_get_string(account, YAHOO_PICURL_SETTING, NULL);

		yd->picture_checksum = yahoo_buddy_icon_calculate_checksum(data, len);

		if ((yd->picture_checksum == oldcksum) &&
			(expire > (time(NULL) + 60*60*24)) && oldurl)
		{
			purple_debug_misc("yahoo", "buddy icon is up to date. Not reuploading.\n");
			g_string_free(s, TRUE);
			g_free(yd->picture_url);
			yd->picture_url = g_strdup(oldurl);
			return;
		}

		/* We use this solely for sending a filename to the server */
		d = g_new0(struct yahoo_buddy_icon_upload_data, 1);
		d->gc = gc;
		d->picture_data = s;
		d->filename = g_strdup(purple_imgstore_get_filename(img));

		if (!yd->logged_in) {
			yd->picture_upload_todo = d;
			return;
		}

		yahoo_buddy_icon_upload(gc, d);

	}
}

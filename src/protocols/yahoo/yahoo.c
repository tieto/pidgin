/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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
#include "multi.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "html.h"

#include "yahoo.h"
#include "yahoochat.h"
#include "md5.h"

/* XXX */
#include "gtkinternal.h"
#include "gaim.h"
#include "ui.h"

extern char *yahoo_crypt(const char *, const char *);

/* #define YAHOO_DEBUG */

#define USEROPT_MAIL 0

#define YAHOO_PAGER_HOST "scs.yahoo.com"
#define YAHOO_PAGER_PORT 5050
#define YAHOO_PROFILE_URL "http://profiles.yahoo.com/"

#define YAHOO_PROTO_VER 0x0900

#define YAHOO_PACKET_HDRLEN (4 + 2 + 2 + 2 + 2 + 4 + 4)

static void yahoo_add_buddy(GaimConnection *gc, const char *who, GaimGroup *);

static struct yahoo_friend *yahoo_friend_new()
{
	struct yahoo_friend *ret;

	ret = g_new0(struct yahoo_friend, 1);
	ret->status = YAHOO_STATUS_OFFLINE;

	return ret;
}

static void yahoo_friend_free(gpointer p)
{
	struct yahoo_friend *f = p;
	if (f->msg)
		g_free(f->msg);
	if (f->game)
		g_free(f->game);
	g_free(f);
}

struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, enum yahoo_status status, int id)
{
	struct yahoo_packet *pkt = g_new0(struct yahoo_packet, 1);

	pkt->service = service;
	pkt->status = status;
	pkt->id = id;

	return pkt;
}

void yahoo_packet_hash(struct yahoo_packet *pkt, int key, const char *value)
{
	struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);
	pair->key = key;
	pair->value = g_strdup(value);
	pkt->hash = g_slist_append(pkt->hash, pair);
}

static int yahoo_packet_length(struct yahoo_packet *pkt)
{
	GSList *l;

	int len = 0;

	l = pkt->hash;
	while (l) {
		struct yahoo_pair *pair = l->data;
		int tmp = pair->key;
		do {
			tmp /= 10;
			len++;
		} while (tmp);
		len += 2;
		len += strlen(pair->value);
		len += 2;
		l = l->next;
	}

	return len;
}

/* sometimes i wish prpls could #include things from other prpls. then i could just
 * use the routines from libfaim and not have to admit to knowing how they work. */
#define yahoo_put16(buf, data) ( \
		(*(buf) = (u_char)((data)>>8)&0xff), \
		(*((buf)+1) = (u_char)(data)&0xff),  \
		2)
#define yahoo_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define yahoo_put32(buf, data) ( \
		(*((buf)) = (u_char)((data)>>24)&0xff), \
		(*((buf)+1) = (u_char)((data)>>16)&0xff), \
		(*((buf)+2) = (u_char)((data)>>8)&0xff), \
		(*((buf)+3) = (u_char)(data)&0xff), \
		4)
#define yahoo_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))

static void yahoo_packet_read(struct yahoo_packet *pkt, guchar *data, int len)
{
	int pos = 0;

	while (pos + 1 < len) {
		char key[64], *value = NULL, *esc;
		int accept;
		int x;

		struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);

		x = 0;
		while (pos + 1 < len) {
			if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
				break;
			key[x++] = data[pos++];
		}
		key[x] = 0;
		pos += 2;
		pair->key = strtol(key, NULL, 10);
		accept = x; /* if x is 0 there was no key, so don't accept it */

		if (len - pos + 1 <= 0) {
			/* Truncated. Garbage or something. */
			accept = 0;
		}

		if (accept) {
			value = g_malloc(len - pos + 1);
			x = 0;
			while (pos + 1 < len) {
				if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
					break;
				value[x++] = data[pos++];
			}
			value[x] = 0;
			pair->value = g_strdup(value);
			g_free(value);
			pkt->hash = g_slist_append(pkt->hash, pair);
			esc = g_strescape(pair->value, NULL);
			gaim_debug(GAIM_DEBUG_MISC, "yahoo",
					   "Key: %d  \tValue: %s\n", pair->key, esc);
			g_free(esc);
		} else {
			g_free(pair);
		}
		pos += 2;

		/* Skip over garbage we've noticed in the mail notifications */
		if (data[0] == '9' && data[pos] == 0x01)
			pos++;
	}
}

static void yahoo_packet_write(struct yahoo_packet *pkt, guchar *data)
{
	GSList *l = pkt->hash;
	int pos = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;
		guchar buf[100];

		g_snprintf(buf, sizeof(buf), "%d", pair->key);
		strcpy(data + pos, buf);
		pos += strlen(buf);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		strcpy(data + pos, pair->value);
		pos += strlen(pair->value);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		l = l->next;
	}
}

static void yahoo_packet_dump(guchar *data, int len)
{
#ifdef YAHOO_DEBUG
	int i;

	gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");

	for (i = 0; i + 1 < len; i += 2) {
		if ((i % 16 == 0) && i) {
			gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
			gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");
		}

		gaim_debug(GAIM_DEBUG_MISC, NULL, "%02x%02x ", data[i], data[i + 1]);
	}
	if (i < len)
		gaim_debug(GAIM_DEBUG_MISC, NULL, "%02x", data[i]);

	gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
	gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");

	for (i = 0; i < len; i++) {
		if ((i % 16 == 0) && i) {
			gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
			gaim_debug(GAIM_DEBUG_MISC, "yahoo", "");
		}

		if (g_ascii_isprint(data[i]))
			gaim_debug(GAIM_DEBUG_MISC, NULL, "%c ", data[i]);
		else
			gaim_debug(GAIM_DEBUG_MISC, NULL, ". ");
	}

	gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
#endif
}

int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	int pktlen = yahoo_packet_length(pkt);
	int len = YAHOO_PACKET_HDRLEN + pktlen;
	int ret;

	guchar *data;
	int pos = 0;

	if (yd->fd < 0)
		return -1;

	data = g_malloc0(len + 1);

	memcpy(data + pos, "YMSG", 4); pos += 4;
	pos += yahoo_put16(data + pos, YAHOO_PROTO_VER);
	pos += yahoo_put16(data + pos, 0x0000);
	pos += yahoo_put16(data + pos, pktlen);
	pos += yahoo_put16(data + pos, pkt->service);
	pos += yahoo_put32(data + pos, pkt->status);
	pos += yahoo_put32(data + pos, pkt->id);

	yahoo_packet_write(pkt, data + pos);

	yahoo_packet_dump(data, len);
	ret = write(yd->fd, data, len);
	g_free(data);

	return ret;
}

void yahoo_packet_free(struct yahoo_packet *pkt)
{
	while (pkt->hash) {
		struct yahoo_pair *pair = pkt->hash->data;
		g_free(pair->value);
		g_free(pair);
		pkt->hash = g_slist_remove(pkt->hash, pair);
	}
	g_free(pkt);
}

static void yahoo_update_status(GaimConnection *gc, const char *name, struct yahoo_friend *f)
{
	if (!gc || !name || !f || !gaim_find_buddy(gaim_connection_get_account(gc), name))
		return;

	serv_got_update(gc, name, 1, 0, 0, f->idle, f->away ? UC_UNAVAILABLE : 0);
}

static void yahoo_process_status(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	struct yahoo_friend *f = NULL;
	char *name = NULL;


	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 0: /* we won't actually do anything with this */
			break;
		case 1: /* we don't get the full buddy list here. */
			if (!yd->logged_in) {
				gaim_connection_set_state(gc, GAIM_CONNECTED);
				serv_finish_login(gc);
				gaim_connection_set_display_name(gc, pair->value);
				yd->logged_in = TRUE;

				/* this requests the list. i have a feeling that this is very evil
				 *
				 * scs.yahoo.com sends you the list before this packet without  it being
				 * requested
				 *
				 * do_import(gc, NULL);
				 * newpkt = yahoo_packet_new(YAHOO_SERVICE_LIST, YAHOO_STATUS_OFFLINE, 0);
				 * yahoo_send_packet(yd, newpkt);
				 * yahoo_packet_free(newpkt);
				 */

				}
			break;
		case 8: /* how many online buddies we have */
			break;
		case 7: /* the current buddy */
			name = pair->value;
			f = g_hash_table_lookup(yd->friends, name);
			if (!f) {
				f = yahoo_friend_new();
				g_hash_table_insert(yd->friends, g_strdup(name), f);
			}
			break;
		case 10: /* state */
			if (!f)
				break;

			f->status = strtol(pair->value, NULL, 10);
			if ((f->status >= YAHOO_STATUS_BRB) && (f->status <= YAHOO_STATUS_STEPPEDOUT))
				f->away = 1;
			else
				f->away = 0;
			if (f->status == YAHOO_STATUS_IDLE)
				f->idle = time(NULL);
			if (f->status != YAHOO_STATUS_CUSTOM) {
				g_free(f->msg);
				f->msg = NULL;
			}
			if (f->status == YAHOO_STATUS_AVAILABLE)
				f->idle = 0;
			break;
		case 19: /* custom message */
			if (f) {
				if (f->msg)
					g_free(f->msg);
				f->msg = g_strdup(pair->value);
			}
			break;
		case 11: /* this is the buddy's session id */
			break;
		case 17: /* in chat? */
			break;
		case 47: /* is custom status away or not? 2=idle*/
			if (!f)
				break;
			f->away = strtol(pair->value, NULL, 10);
			if (f->away == 2)
				f->idle = time(NULL);
			break;
		case 138: /* either we're not idle, or we are but won't say how long */
			if (!f)
				break;

			if (f->idle)
				f->idle = -1;
			break;
		case 137: /* usually idle time in seconds, sometimes login time */
			if (!f)
				break;

			if (f->status != YAHOO_STATUS_AVAILABLE)
				f->idle = time(NULL) - strtol(pair->value, NULL, 10);
			break;
		case 13: /* bitmask, bit 0 = pager, bit 1 = chat, bit 2 = game */
			if (strtol(pair->value, NULL, 10) == 0) {
				if (f)
					f->status = YAHOO_STATUS_OFFLINE;
				serv_got_update(gc, name, 0, 0, 0, 0, 0);
				break;
			}

			if (f)
				yahoo_update_status(gc, name, f);
			break;
		case 60: /* SMS */
			if (f) {
				f->sms = strtol(pair->value, NULL, 10);
				yahoo_update_status(gc, name, f);
			}
			break;
		case 16: /* Custom error message */
			gaim_notify_error(gc, NULL, pair->value, NULL);
			break;
		default:
			gaim_debug(GAIM_DEBUG_ERROR, "yahoo",
					   "Unknown status key %d\n", pair->key);
			break;
		}

		l = l->next;
	}
}

static void yahoo_process_list(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	gboolean export = FALSE;
	gboolean got_serv_list = FALSE;
	GaimBuddy *b;
	GaimGroup *g;
	struct yahoo_friend *f = NULL;
	struct yahoo_data *yd = gc->proto_data;

	char **lines;
	char **split;
	char **buddies;
	char **tmp, **bud;

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		switch (pair->key) {
		case 87:
			if (!yd->tmp_serv_blist)
				yd->tmp_serv_blist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_blist, pair->value);
			break;
		case 88:
			if (!yd->tmp_serv_ilist)
				yd->tmp_serv_ilist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_ilist, pair->value);
			break;
		}
	}

	if (pkt->status != 0)
		return;

	if (yd->tmp_serv_blist) {
		lines = g_strsplit(yd->tmp_serv_blist->str, "\n", -1);
		for (tmp = lines; *tmp; tmp++) {
			split = g_strsplit(*tmp, ":", 2);
			if (!split)
				continue;
			if (!split[0] || !split[1]) {
				g_strfreev(split);
				continue;
			}
			buddies = g_strsplit(split[1], ",", -1);
			for (bud = buddies; bud && *bud; bud++) {
				if (!(f = g_hash_table_lookup(yd->friends, *bud))) {
					f = yahoo_friend_new(*bud);
					g_hash_table_insert(yd->friends, g_strdup(*bud), f);
				}
				if (!(b = gaim_find_buddy(gc->account,  *bud))) {
					if (!(g = gaim_find_group(split[0]))) {
						g = gaim_group_new(split[0]);
						gaim_blist_add_group(g, NULL);
					}
					b = gaim_buddy_new(gc->account, *bud, NULL);
					gaim_blist_add_buddy(b, NULL, g, NULL);
					export = TRUE;
				} /*else { do something here is gaim and yahoo don't agree on the buddies group
					GaimGroup *tmpgp;

					tmpgp = gaim_find_buddys_group(b);
					if (!tmpgp || !gaim_utf8_strcasecmp(tmpgp->name, split[0])) {
						GaimGroup *ng;
						if (!(ng = gaim_find_group(split[0])) {
							g = gaim_group_new(split[0]);
							gaim_blist_add_group(g, NULL);
						} */

			}
			g_strfreev(buddies);
			g_strfreev(split);
		}
		g_strfreev(lines);

		g_string_free(yd->tmp_serv_blist, TRUE);
		yd->tmp_serv_blist = NULL;
	}


	if (yd->tmp_serv_ilist) {
		buddies = g_strsplit(yd->tmp_serv_ilist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			/* The server is already ignoring the user */
			got_serv_list = TRUE;
			gaim_privacy_deny_add(gc->account, *bud, 1);
		}
		g_strfreev(buddies);

		g_string_free(yd->tmp_serv_ilist, TRUE);
		yd->tmp_serv_ilist = NULL;
	}

	if (got_serv_list) {
		gc->account->perm_deny = 4;
		serv_set_permit_deny(gc);
	}
	if (export)
		gaim_blist_save();
}

static void yahoo_process_notify(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	char *stat = NULL;
	char *game = NULL;
	struct yahoo_friend *f = NULL;
	GSList *l = pkt->hash;
	struct yahoo_data *yd = (struct yahoo_data*) gc->proto_data;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4)
			from = pair->value;
		if (pair->key == 49)
			msg = pair->value;
		if (pair->key == 13)
			stat = pair->value;
		if (pair->key == 14)
			game = pair->value;
		l = l->next;
	}

	if (!from || !msg)
		return;

	if (!g_ascii_strncasecmp(msg, "TYPING", strlen("TYPING"))) {
		if (*stat == '1')
			serv_got_typing(gc, from, 0, GAIM_TYPING);
		else
			serv_got_typing_stopped(gc, from);
	} else if (!g_ascii_strncasecmp(msg, "GAME", strlen("GAME"))) {
		GaimBuddy *bud = gaim_find_buddy(gc->account, from);

		if (!bud) {
			gaim_debug(GAIM_DEBUG_WARNING, "yahoo",
					   "%s is playing a game, and doesn't want "
					   "you to know.\n", from);
		}

		f = g_hash_table_lookup(yd->friends, from);
		if (!f)
			return; /* if they're not on the list, don't bother */

		if (f->game) {
			g_free(f->game);
			f->game = NULL;
		}

		if (*stat == '1') {
			f->game = g_strdup(game);
			if (bud)
				yahoo_update_status(gc, from, f);
		}
	}
}

static void yahoo_process_message(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	time_t tm = time(NULL);
	GSList *l = pkt->hash;

	if (pkt->status <= 1 || pkt->status == 5) {
		while (l) {
			struct yahoo_pair *pair = l->data;
			if (pair->key == 4)
				from = pair->value;
			if (pair->key == 15)
				tm = strtol(pair->value, NULL, 10);
			if (pair->key == 14) {
				char *m;

				msg = pair->value;

				strip_linefeed(msg);
				m = yahoo_codes_to_html(msg);
				serv_got_im(gc, from, m, 0, tm, -1);
				g_free(m);

				tm = time(NULL);
			}
			l = l->next;
		}
	} else if (pkt->status == 2) {
		gaim_notify_error(gc, NULL,
						  _("Your Yahoo! message did not get sent."), NULL);
	}
}

static void yahoo_buddy_added_us(GaimConnection *gc, struct yahoo_packet *pkt) {
	char *id = NULL;
	char *who = NULL;
	char *msg = NULL;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1:
			id = pair->value;
			break;
		case 3:
			who = pair->value;
			break;
		case 15: /* time, for when they add us and we're offline */
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (id)
		show_got_added(gc, id, who, NULL, msg);
}

static void yahoo_buddy_denied_our_add(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *msg = NULL;
	GSList *l = pkt->hash;
	GString *buf = NULL;
	struct yahoo_data *yd = gc->proto_data;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 3:
			who = pair->value;
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (who) {
		buf = g_string_sized_new(0);
		if (!msg)
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list."), who);
		else
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list for the following reason: %s."), who, msg);
		gaim_notify_info(gc, NULL, buf->str, NULL);
		g_string_free(buf, TRUE);
		g_hash_table_remove(yd->friends, who);
		serv_got_update(gc, who, 0, 0, 0, 0, 0);
	}
}

static void yahoo_process_contact(GaimConnection *gc, struct yahoo_packet *pkt)
{


	switch (pkt->status) {
	case 1:
		yahoo_process_status(gc, pkt);
		return;
	case 3:
		yahoo_buddy_added_us(gc, pkt);
		break;
	case 7:
		yahoo_buddy_denied_our_add(gc, pkt);
		break;
	default:
		break;
	}
}

static void yahoo_process_mail(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	char *who = NULL;
	char *email = NULL;
	char *subj = NULL;
	int count = 0;
	GSList *l = pkt->hash;

	if (!gaim_account_get_check_mail(account))
		return;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 9)
			count = strtol(pair->value, NULL, 10);
		else if (pair->key == 43)
			who = pair->value;
		else if (pair->key == 42)
			email = pair->value;
		else if (pair->key == 18)
			subj = pair->value;
		l = l->next;
	}

	if (who && subj && email && *email) {
		char *from = g_strdup_printf("%s (%s)", who, email);

		gaim_notify_email(gc, subj, from, gaim_account_get_username(account),
						  "http://mail.yahoo.com/", NULL, NULL);

		g_free(from);
	} else if (count > 0) {
		const char *to = gaim_account_get_username(account);
		const char *url = "http://mail.yahoo.com/";

		gaim_notify_emails(gc, count, FALSE, NULL, NULL, &to, &url,
						   NULL, NULL);
	}
}
/* This is the y64 alphabet... it's like base64, but has a . and a _ */
char base64digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

/* This is taken from Sylpheed by Hiroyuki Yamamoto.  We have our own tobase64 function
 * in util.c, but it has a bug I don't feel like finding right now ;) */
void to_y64(unsigned char *out, const unsigned char *in, int inlen)
     /* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */
{
	for (; inlen >= 3; inlen -= 3)
		{
			*out++ = base64digits[in[0] >> 2];
			*out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
			*out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
			*out++ = base64digits[in[2] & 0x3f];
			in += 3;
		}
	if (inlen > 0)
		{
			unsigned char fragment;

			*out++ = base64digits[in[0] >> 2];
			fragment = (in[0] << 4) & 0x30;
			if (inlen > 1)
				fragment |= in[1] >> 4;
			*out++ = base64digits[fragment];
			*out++ = (inlen < 2) ? '-' : base64digits[(in[1] << 2) & 0x3c];
			*out++ = '-';
		}
	*out = '\0';
}

static void yahoo_process_auth(GaimConnection *gc, struct yahoo_packet *pkt)
{
	char *seed = NULL;
	char *sn   = NULL;
	GSList *l = pkt->hash;
	struct yahoo_data *yd = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 94)
			seed = pair->value;
		if (pair->key == 1)
			sn = pair->value;
		l = l->next;
	}
	
	if (seed) {
		struct yahoo_packet *pack;
		const char *name = normalize(gaim_account_get_username(account));
		const char *pass = gaim_account_get_password(account);

		/* So, Yahoo has stopped supporting its older clients in India, and undoubtedly
		 * will soon do so in the rest of the world.
		 *
		 * The new clients use this authentication method.  I warn you in advance, it's
		 * bizzare, convoluted, inordinately complicated.  It's also no more secure than
		 * crypt() was.  The only purpose this scheme could serve is to prevent third
		 * part clients from connecting to their servers.
		 *
		 * Sorry, Yahoo.
		 */
		
		md5_byte_t result[16];
		md5_state_t ctx;
		
		char *crypt_result;
		char password_hash[25];
		char crypt_hash[25];
		char *hash_string_p = g_malloc(50 + strlen(sn));
		char *hash_string_c = g_malloc(50 + strlen(sn));
		
		char checksum;
		
		int sv;
		
		char result6[25];
		char result96[25];

		sv = seed[15];
		sv = sv % 8;

		md5_init(&ctx);
		md5_append(&ctx, pass, strlen(pass));
		md5_finish(&ctx, result);
		to_y64(password_hash, result, 16);
		
		md5_init(&ctx);
		crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");  
		md5_append(&ctx, crypt_result, strlen(crypt_result));
		md5_finish(&ctx, result);
		to_y64(crypt_hash, result, 16);

		switch (sv) {
		case 1:
		case 6:
			checksum = seed[seed[9] % 16];
			g_snprintf(hash_string_p, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, name, seed, password_hash);
                        g_snprintf(hash_string_c, strlen(sn) + 50,
				   "%c%s%s%s", checksum, name, seed, crypt_hash);
			break;
		case 2:
		case 7:
			checksum = seed[seed[15] % 16];
			g_snprintf(hash_string_p, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, seed, password_hash, name);
                        g_snprintf(hash_string_c, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, seed, crypt_hash, name);
			break;
		case 3:
			checksum = seed[seed[1] % 16];
			g_snprintf(hash_string_p, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, name, password_hash, seed);
                        g_snprintf(hash_string_c, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, name, crypt_hash, seed);
			break;
		case 4:
			checksum = seed[seed[3] % 16];
			g_snprintf(hash_string_p, strlen(sn) + 50,
				   "%c%s%s%s", checksum, password_hash, seed, name);
			g_snprintf(hash_string_c, strlen(sn) + 50,
				   "%c%s%s%s", checksum, crypt_hash, seed, name);
			break;
		case 0:
		case 5:
			checksum = seed[seed[7] % 16];
			g_snprintf(hash_string_p, strlen(sn) + 50,
                                   "%c%s%s%s", checksum, password_hash, name, seed);
                        g_snprintf(hash_string_c, strlen(sn) + 50,
				   "%c%s%s%s", checksum, crypt_hash, name, seed);
			break;
		}
		
		md5_init(&ctx);  
		md5_append(&ctx, hash_string_p, strlen(hash_string_p));
		md5_finish(&ctx, result);
		to_y64(result6, result, 16);

		md5_init(&ctx);  
		md5_append(&ctx, hash_string_c, strlen(hash_string_c));
		md5_finish(&ctx, result);
		to_y64(result96, result, 16);

		pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
		yahoo_packet_hash(pack, 0, name);
		yahoo_packet_hash(pack, 6, result6);
		yahoo_packet_hash(pack, 96, result96);
		yahoo_packet_hash(pack, 1, name);
		
		yahoo_send_packet(yd, pack);
		
		g_free(hash_string_p);
		g_free(hash_string_c);

		yahoo_packet_free(pack);
	}
}

static void ignore_buddy(GaimBuddy *b) {
	GaimGroup *g;
	GaimConversation *c;
	GaimAccount *account;
	gchar *name;

	if (!b)
		return;

	g = gaim_find_buddys_group(b);
	name = g_strdup(b->name);
	account = b->account;

	gaim_debug(GAIM_DEBUG_INFO, "blist",
		"Removing '%s' from buddy list.\n", b->name);
	serv_remove_buddy(account->gc, name, g->name);
	gaim_blist_remove_buddy(b);

	serv_add_deny(account->gc, name);
	gaim_blist_save();

	c = gaim_find_conversation_with_account(name, account);

	if (c != NULL)
		gaim_conversation_update(c, GAIM_CONV_UPDATE_REMOVE);

	g_free(name);
}

static void keep_buddy(GaimBuddy *b) {
	gaim_privacy_deny_remove(b->account, b->name, 1);
}

static void yahoo_process_ignore(GaimConnection *gc, struct yahoo_packet *pkt) {
	GaimBuddy *b;
	GSList *l;
	gchar *who = NULL;
	gchar *sn = NULL;
	gchar buf[BUF_LONG];
	gint ignore = 0;
	gint status = 0;

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;
		switch (pair->key) {
		case 0:
			who = pair->value;
			break;
		case 1:
			sn = pair->value;
			break;
		case 13:
			ignore = strtol(pair->value, NULL, 10);
			break;
		case 66:
			status = strtol(pair->value, NULL, 10);
			break;
		default:
			break;
		}
	}

	switch (status) {
	case 12:
		b = gaim_find_buddy(gc->account, who);
		g_snprintf(buf, sizeof(buf), _("You have tried to ignore %s, but the "
					"user is on your buddy list.  Clicking \"Yes\" "
					"will remove and ignore the buddy."), who);
		gaim_request_yes_no(gc, NULL, _("Ignore buddy?"), buf, 0, b,
						G_CALLBACK(ignore_buddy),
						G_CALLBACK(keep_buddy));
		break;
	case 2:
	case 3:
	case 0:
	default:
		break;
	}
}

static void yahoo_process_authresp(GaimConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	int err = 0;
	char *msg;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 66)
			err = strtol(pair->value, NULL, 10);

		l = l->next;
	}

	switch (err) {
	case 3:
		msg = _("Invalid username.");
		break;
	case 13:
		msg = _("Incorrect password.");
		break;
	default:
		msg = _("Unknown error.");
	}

	gaim_connection_error(gc, msg);
}

static void yahoo_packet_process(GaimConnection *gc, struct yahoo_packet *pkt)
{
	switch (pkt->service) {
	case YAHOO_SERVICE_LOGON:
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
	case YAHOO_SERVICE_GAMELOGON:
	case YAHOO_SERVICE_GAMELOGOFF:
	case YAHOO_SERVICE_CHATLOGON:
	case YAHOO_SERVICE_CHATLOGOFF:
		yahoo_process_status(gc, pkt);
		break;
	case YAHOO_SERVICE_NOTIFY:
		yahoo_process_notify(gc, pkt);
		break;
	case YAHOO_SERVICE_MESSAGE:
	case YAHOO_SERVICE_GAMEMSG:
	case YAHOO_SERVICE_CHATMSG:
		yahoo_process_message(gc, pkt);
		break;
	case YAHOO_SERVICE_NEWMAIL:
		yahoo_process_mail(gc, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		yahoo_process_contact(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTHRESP:
		yahoo_process_authresp(gc, pkt);
		break;
	case YAHOO_SERVICE_LIST:
		yahoo_process_list(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(gc, pkt);
		break;
	case YAHOO_SERVICE_IGNORECONTACT:
		yahoo_process_ignore(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFINVITE:
	case YAHOO_SERVICE_CONFADDINVITE:
		yahoo_process_conference_invite(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFDECLINE:
		yahoo_process_conference_decline(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGON:
		yahoo_process_conference_logon(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFLOGOFF:
		yahoo_process_conference_logoff(gc, pkt);
		break;
	case YAHOO_SERVICE_CONFMSG:
		yahoo_process_conference_message(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATONLINE:
		yahoo_process_chat_online(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLOGOUT:
		yahoo_process_chat_logout(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATGOTO:
		yahoo_process_chat_goto(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATJOIN:
		yahoo_process_chat_join(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATLEAVE: /* XXX is this right? */
	case YAHOO_SERVICE_CHATEXIT:
		yahoo_process_chat_exit(gc, pkt);
		break;
	case YAHOO_SERVICE_CHATINVITE: /* XXX never seen this one, might not do it right */
	case YAHOO_SERVICE_CHATADDINVITE:
		yahoo_process_chat_addinvite(gc, pkt);
		break;
	case YAHOO_SERVICE_COMMENT:
		yahoo_process_chat_message(gc, pkt);
		break;
	default:
		gaim_debug(GAIM_DEBUG_ERROR, "yahoo",
				   "Unhandled service 0x%02x\n", pkt->service);
		break;
	}
}

static void yahoo_pending(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd = gc->proto_data;
	char buf[1024];
	int len;

	len = read(yd->fd, buf, sizeof(buf));

	if (len <= 0) {
		gaim_connection_error(gc, _("Unable to read"));
		return;
	}

	yd->rxqueue = g_realloc(yd->rxqueue, len + yd->rxlen);
	memcpy(yd->rxqueue + yd->rxlen, buf, len);
	yd->rxlen += len;

	while (1) {
		struct yahoo_packet *pkt;
		int pos = 0;
		int pktlen;

		if (yd->rxlen < YAHOO_PACKET_HDRLEN)
			return;

		pos += 4; /* YMSG */
		pos += 2;
		pos += 2;

		pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
		gaim_debug(GAIM_DEBUG_MISC, "yahoo",
				   "%d bytes to read, rxlen is %d\n", pktlen, yd->rxlen);

		if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen))
			return;

		yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

		pkt = yahoo_packet_new(0, 0, 0);

		pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
		pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
		gaim_debug(GAIM_DEBUG_MISC, "yahoo",
				   "Yahoo Service: 0x%02x Status: %d\n",
				   pkt->service, pkt->status);
		pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

		yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

		yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
		if (yd->rxlen) {
			char *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN + pktlen, yd->rxlen);
			g_free(yd->rxqueue);
			yd->rxqueue = tmp;
		} else {
			g_free(yd->rxqueue);
			yd->rxqueue = NULL;
		}

		yahoo_packet_process(gc, pkt);

		yahoo_packet_free(pkt);
	}
}

static void yahoo_got_connected(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		gaim_connection_error(gc, _("Unable to connect"));
		return;
	}

	yd = gc->proto_data;
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, normalize(gaim_account_get_username(gaim_connection_get_account(gc))));
	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	gc->inpa = gaim_input_add(yd->fd, GAIM_INPUT_READ, yahoo_pending, gc);
}

static void yahoo_login(GaimAccount *account) {
	GaimConnection *gc = gaim_account_get_connection(account);
	struct yahoo_data *yd = gc->proto_data = g_new0(struct yahoo_data, 1);

	gc->flags |= GAIM_CONNECTION_HTML | GAIM_CONNECTION_NO_BGCOLOR;

	gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	yd->fd = -1;
	yd->friends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, yahoo_friend_free);
	yd->confs = NULL;
	yd->conf_id = 2;

	if (gaim_proxy_connect(account, gaim_account_get_string(account, "server",  YAHOO_PAGER_HOST),
			  gaim_account_get_int(account, "port", YAHOO_PAGER_PORT),
			  yahoo_got_connected, gc) != 0) {
		gaim_connection_error(gc, _("Connection problem"));
		return;
	}

}

static void yahoo_close(GaimConnection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;

	g_hash_table_destroy(yd->friends);
	g_slist_free(yd->confs);
	if (yd->chat_name)
		g_free(yd->chat_name);

	if (yd->fd >= 0)
		close(yd->fd);

	if (yd->rxqueue)
		g_free(yd->rxqueue);
	yd->rxlen = 0;
	if (gc->inpa)
		gaim_input_remove(gc->inpa);
	g_free(yd);
}

static const char *yahoo_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "yahoo";
}

static void yahoo_list_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne)
{
	int i = 0;
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	GaimAccount *account;
	GaimConnection *gc;
	struct yahoo_data *yd;
	struct yahoo_friend *f;

	if (!b || !(account = b->account) || !(gc = gaim_account_get_connection(account)) ||
	  				     !(yd = gc->proto_data))
		return;

	f = g_hash_table_lookup(yd->friends, b->name);
	if (!f) {
		*se = "notauthorized";
		return;
	}

	if (b->present == GAIM_BUDDY_OFFLINE) {
		*se = "offline";
		return;
	} else {
		if (f->away)
			emblems[i++] = "away";
		if (f->sms)
			emblems[i++] = "wireless";
		if (f->game)
			emblems[i++] = "game";
	}
	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

static char *yahoo_get_status_string(enum yahoo_status a)
{
	switch (a) {
	case YAHOO_STATUS_BRB:
		return _("Be Right Back");
	case YAHOO_STATUS_BUSY:
		return _("Busy");
	case YAHOO_STATUS_NOTATHOME:
		return _("Not At Home");
	case YAHOO_STATUS_NOTATDESK:
		return _("Not At Desk");
	case YAHOO_STATUS_NOTINOFFICE:
		return _("Not In Office");
	case YAHOO_STATUS_ONPHONE:
		return _("On The Phone");
	case YAHOO_STATUS_ONVACATION:
		return _("On Vacation");
	case YAHOO_STATUS_OUTTOLUNCH:
		return _("Out To Lunch");
	case YAHOO_STATUS_STEPPEDOUT:
		return _("Stepped Out");
	case YAHOO_STATUS_INVISIBLE:
		return _("Invisible");
	case YAHOO_STATUS_IDLE:
		return _("Idle");
	case YAHOO_STATUS_OFFLINE:
		return _("Offline");
	default:
		return _("Online");
	}
}

static void yahoo_initiate_conference(GaimConnection *gc, const char *name)
{
	GHashTable *components;
	struct yahoo_data *yd;
	int id;

	yd = gc->proto_data;
	id = yd->conf_id;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"),
		g_strdup_printf("%s-%d", gaim_connection_get_display_name(gc), id));
	g_hash_table_replace(components, g_strdup("topic"), g_strdup("Join my conference..."));
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	yahoo_c_join(gc, components);
	g_hash_table_destroy(components);

	yahoo_c_invite(gc, id, "Join my conference...", name);
}

static void yahoo_game(GaimConnection *gc, const char *name) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	char *game = NULL;
	char *t;
	char url[256];
	struct yahoo_friend *f;

	f = g_hash_table_lookup(yd->friends, name);
	if (!f)
		return;

	game = f->game;
	if (!game)
		return;

	t = game = g_strdup(strstr(game, "ante?room="));
	while (*t != '\t')
		t++;
	*t = 0;
	g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game);
	gaim_notify_uri(gc, url);
	g_free(game);
}

static char *yahoo_status_text(GaimBuddy *b)
{
	struct yahoo_data *yd = (struct yahoo_data*)b->account->gc->proto_data;
	struct yahoo_friend *f = NULL;
	char *stripped = NULL;

	f = g_hash_table_lookup(yd->friends, b->name);
	if (!f)
		return g_strdup(_("Not on server list"));

	switch (f->status) {
	case YAHOO_STATUS_AVAILABLE:
		return NULL;
	case YAHOO_STATUS_IDLE:
		if (f->idle == -1)
			return g_strdup(yahoo_get_status_string(f->status));
		return NULL;
	case YAHOO_STATUS_CUSTOM:
		if (!f->msg)
			return NULL;
		stripped = strip_html(f->msg);
		if (stripped) {
 			char *ret = g_markup_escape_text(stripped, strlen(stripped));
 			g_free(stripped);
 			return ret;
 		}
		return NULL;
	default:
		return g_strdup(yahoo_get_status_string(f->status));
 	}

	return NULL;
}

static char *yahoo_tooltip_text(GaimBuddy *b)
{
	struct yahoo_data *yd = (struct yahoo_data*)b->account->gc->proto_data;
	struct yahoo_friend *f;
	char *escaped, *status, *ret;

	f = g_hash_table_lookup(yd->friends, b->name);
	if (!f)
		status = g_strdup(_("Not on server list"));
	else
		switch (f->status) {
		case YAHOO_STATUS_IDLE:
			if (f->idle == -1) {
				status = g_strdup(yahoo_get_status_string(f->status));
				break;
			}
			return NULL;
		case YAHOO_STATUS_CUSTOM:
			if (!f->msg)
				return NULL;
			status = strip_html(f->msg);
			break;
		default:
			status = g_strdup(yahoo_get_status_string(f->status));
			break;
		}

	escaped = g_markup_escape_text(status, strlen(status));
	ret = g_strdup_printf(_("<b>Status:</b> %s"), escaped);
	g_free(status);
	g_free(escaped);

	return ret;
}

static GList *yahoo_buddy_menu(GaimConnection *gc, const char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	static char buf2[1024];
	struct yahoo_friend *f;

	f = g_hash_table_lookup(yd->friends, who);

	if (!f) {
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Add Buddy");
		pbm->callback = yahoo_add_buddy;
		pbm->gc = gc;
		m = g_list_append(m, pbm);

		return m;
	}

	if (f->status == YAHOO_STATUS_OFFLINE)
		return NULL;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Join in Chat");
	pbm->callback = yahoo_chat_goto;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Initiate Conference");
	pbm->callback = yahoo_initiate_conference;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	if (f->game) {
		char *game = f->game;
		char *room;
		char *t;

		if (!game)
			return m;

		pbm = g_new0(struct proto_buddy_menu, 1);
		if (!(room = strstr(game, "&follow="))) /* skip ahead to the url */
			return m;
		while (*room && *room != '\t')          /* skip to the tab */
			room++;
		t = room++;                             /* room as now at the name */
		while (*t != '\n')
			t++;                            /* replace the \n with a space */
		*t = ' ';
		g_snprintf(buf2, sizeof buf2, "%s", room);
		pbm->label = buf2;
		pbm->callback = yahoo_game;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
	}

	return m;
}

static void yahoo_act_id(GaimConnection *gc, const char *entry)
{
	struct yahoo_data *yd = gc->proto_data;

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_IDACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 3, entry);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	gaim_connection_set_display_name(gc, entry);
}

static void yahoo_show_act_id(GaimConnection *gc)
{
	gaim_request_input(gc, NULL, _("Active which ID?"), NULL,
					   gaim_connection_get_display_name(gc), FALSE, FALSE,
					   _("OK"), G_CALLBACK(yahoo_act_id),
					   _("Cancel"), NULL, gc);
}

static GList *yahoo_actions(GaimConnection *gc) {
	GList *m = NULL;
	struct proto_actions_menu *pam;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Activate ID");
	pam->callback = yahoo_show_act_id;
	pam->gc = gc;
	m = g_list_append(m, pam);

	return m;
}

static int yahoo_send_im(GaimConnection *gc, const char *who, const char *what, int len, GaimImFlags flags)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, 0);
	char *msg = yahoo_html_to_codes(what);

	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 14, msg);
	yahoo_packet_hash(pkt, 97, "1");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	g_free(msg);

	return 1;
}

int yahoo_send_typing(GaimConnection *gc, const char *who, int typ)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, 0);
	yahoo_packet_hash(pkt, 49, "TYPING");
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 14, " ");
	yahoo_packet_hash(pkt, 13, typ == GAIM_TYPING ? "1" : "0");
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 1002, "1");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);

	return 0;
}

static void yahoo_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	int service;
	char s[4];

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (msg) {
		yd->current_status = YAHOO_STATUS_CUSTOM;
		gc->away = g_strdup(msg);
	} else if (state) {
		gc->away = g_strdup("");
		if (!strcmp(state, _("Available"))) {
			yd->current_status = YAHOO_STATUS_AVAILABLE;
			g_free(gc->away);
			gc->away = NULL;
		} else if (!strcmp(state, _("Be Right Back"))) {
			yd->current_status = YAHOO_STATUS_BRB;
		} else if (!strcmp(state, _("Busy"))) {
			yd->current_status = YAHOO_STATUS_BUSY;
		} else if (!strcmp(state, _("Not At Home"))) {
			yd->current_status = YAHOO_STATUS_NOTATHOME;
		} else if (!strcmp(state, _("Not At Desk"))) {
			yd->current_status = YAHOO_STATUS_NOTATDESK;
		} else if (!strcmp(state, _("Not In Office"))) {
			yd->current_status = YAHOO_STATUS_NOTINOFFICE;
		} else if (!strcmp(state, _("On The Phone"))) {
			yd->current_status = YAHOO_STATUS_ONPHONE;
		} else if (!strcmp(state, _("On Vacation"))) {
			yd->current_status = YAHOO_STATUS_ONVACATION;
		} else if (!strcmp(state, _("Out To Lunch"))) {
			yd->current_status = YAHOO_STATUS_OUTTOLUNCH;
		} else if (!strcmp(state, _("Stepped Out"))) {
			yd->current_status = YAHOO_STATUS_STEPPEDOUT;
		} else if (!strcmp(state, _("Invisible"))) {
			yd->current_status = YAHOO_STATUS_INVISIBLE;
		} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
			if (gc->is_idle) {
				yd->current_status = YAHOO_STATUS_IDLE;
			} else {
				yd->current_status = YAHOO_STATUS_AVAILABLE;
			}
			g_free(gc->away);
			gc->away = NULL;
		}
	} else if (gc->is_idle) {
		yd->current_status = YAHOO_STATUS_IDLE;
	} else {
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	}

	if (yd->current_status == YAHOO_STATUS_AVAILABLE)
		service = YAHOO_SERVICE_ISBACK;
	else
		service = YAHOO_SERVICE_ISAWAY;
	pkt = yahoo_packet_new(service, yd->current_status, 0);
	g_snprintf(s, sizeof(s), "%d", yd->current_status);
	yahoo_packet_hash(pkt, 10, s);
	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		yahoo_packet_hash(pkt, 19, msg);
		if (gc->is_idle)
			yahoo_packet_hash(pkt, 47, "2");
		else
			yahoo_packet_hash(pkt, 47, "1");
	}

	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_set_idle(GaimConnection *gc, int idle)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = NULL;

	if (idle && yd->current_status == YAHOO_STATUS_AVAILABLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_IDLE, 0);
		yd->current_status = YAHOO_STATUS_IDLE;
	} else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
		yd->current_status = YAHOO_STATUS_AVAILABLE;
	} else if (idle && gc->away && yd->current_status == YAHOO_STATUS_CUSTOM) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_IDLE, 0);
	} else if (!idle && gc->away && yd->current_status == YAHOO_STATUS_CUSTOM) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_ISAWAY, YAHOO_STATUS_AVAILABLE, 0);
	}

	if (pkt) {
		char buf[4];
		g_snprintf(buf, sizeof(buf), "%d", yd->current_status);
		yahoo_packet_hash(pkt, 10, buf);
		if (gc->away && yd->current_status == YAHOO_STATUS_CUSTOM) {
			yahoo_packet_hash(pkt, 19, gc->away);
			if (idle)
				yahoo_packet_hash(pkt, 47, "2");
			else
				yahoo_packet_hash(pkt, 47, "1"); /* fixme when available messages are possible */
		}
		yahoo_send_packet(yd, pkt);
		yahoo_packet_free(pkt);
	}
}

static GList *yahoo_away_states(GaimConnection *gc)
{
	GList *m = NULL;

	m = g_list_append(m, _("Available"));
	m = g_list_append(m, _("Be Right Back"));
	m = g_list_append(m, _("Busy"));
	m = g_list_append(m, _("Not At Home"));
	m = g_list_append(m, _("Not At Desk"));
	m = g_list_append(m, _("Not In Office"));
	m = g_list_append(m, _("On The Phone"));
	m = g_list_append(m, _("On Vacation"));
	m = g_list_append(m, _("Out To Lunch"));
	m = g_list_append(m, _("Stepped Out"));
	m = g_list_append(m, _("Invisible"));
	m = g_list_append(m, GAIM_AWAY_CUSTOM);

	return m;
}

static void yahoo_keepalive(GaimConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	if (!yd->chat_online)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATPING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 109, gaim_connection_get_display_name(gc));
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_add_buddy(GaimConnection *gc, const char *who, GaimGroup *foo)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	GaimGroup *g;
	char *group = NULL;

	if (!yd->logged_in)
		return;

	g = gaim_find_buddys_group(gaim_find_buddy(gc->account, who));
	if (g)
		group = g->name;
	else
		group = "Buddies";

	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, group);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_remove_buddy(GaimConnection *gc, const char *who, const char *group)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_friend *f;

	if (!(f = g_hash_table_lookup(yd->friends, who)))
		return;

	g_hash_table_remove(yd->friends, who);

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, group);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_add_deny(GaimConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (gc->account->perm_deny != 4)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 13, "1");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_rem_deny(GaimConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 13, "2");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_set_permit_deny(GaimConnection *gc) {
	GaimAccount *acct;
	GSList *deny;

	acct = gc->account;

	switch (acct->perm_deny) {
		case 1:
		case 3:
		case 5:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;
		case 4:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_add_deny(gc, deny->data);
			break;
		case 2:
		default:
			break;
	}
}

static gboolean yahoo_unload_plugin(GaimPlugin *plugin)
{
	yahoo_dest_colorht();
	return TRUE;
}

static void yahoo_got_info(gpointer data, char *url_text, unsigned long len)
{
	char *stripped,*p;
	char buf[1024];

	/* we failed to grab the profile URL */
	if (!url_text) {
		g_show_info_text(NULL, NULL, 2,
				_("<html><body><b>Error retrieving profile</b></body></html>"), NULL);
		return;
	}

	/* we don't yet support the multiple link level of the warning page for
	 * 'adult' profiles, not to mention the fact that yahoo wants you to be
	 * logged in (on the website) to be able to view an 'adult' profile.  for
	 * now, just tell them that we can't help them, and provide a link to the
	 * profile if they want to do the web browser thing.
	 */
	p = strstr(url_text, "Adult Profiles Warning Message");
	if (p) {
		strcpy(buf, _("<b>Sorry, profiles marked as containing adult content are not supported at this time.</b><br><br>\n"));
		info_extract_field(url_text, buf, ".idname=", 0, "%26", 0, NULL,
				_("If you wish to view this profile, you will need to visit this link in your web browser"),
				1, YAHOO_PROFILE_URL);
		strcat(buf, "</body></html>\n");
		g_show_info_text(NULL, NULL, 2, buf, NULL);
		return;
	}

	/* at the moment we don't support profile pages with languages other than
	 * english. the problem is, that every user may choose his/her own profile
	 * language. this language has nothing to do with the preferences of the
	 * user which looks at the profile 
	 */
	p = strstr(url_text, "Last Updated:");
	if (!p) {
		strcpy(buf, _("<b>Sorry, non-English profiles are not supported at this time.</b><br><br>\n"));
		info_extract_field(url_text, buf, "<title>", 0, "'s Yahoo! Profile", 0, NULL,
				_("If you wish to view this profile, you will need to visit this link in your web browser"),
				1, YAHOO_PROFILE_URL);
		strcat(buf, "</body></html>\n");
		g_show_info_text(NULL, NULL, 2, buf, NULL);
		return;
	}

	/* strip_html() doesn't strip out character entities like &nbsp; and &#183;
	*/
	while ((p = strstr(url_text, "&nbsp;")) != NULL) {
		memmove(p, p + 6, strlen(p + 6));
		url_text[strlen(url_text) - 6] = '\0';
	}
	while ((p = strstr(url_text, "&#183;")) != NULL) {
		memmove(p, p + 6, strlen(p + 6));
		url_text[strlen(url_text) - 6] = '\0';
	}

	/* nuke the nasty \r's */
	while ((p = strchr(url_text, '\r')) != NULL) {
		memmove(p, p + 1, strlen(p + 1));
		url_text[strlen(url_text) - 1] = '\0';
	}

	/* nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = strip_html(url_text);

	/* gonna re-use the memory we've already got for url_text */
	strcpy(url_text, "<html><body>\n");

	/* extract their Yahoo! ID and put it in */
	info_extract_field(stripped, url_text, "Yahoo! ID:", 2, "\n", 0,
			NULL, _("Yahoo! ID"), 0, NULL);

	/* extract their Email address and put it in */
	info_extract_field(stripped, url_text, "My Email", 5, "\n", 0,
			"Private", _("Email"), 0, NULL);

	/* extract the Nickname if it exists */
	info_extract_field(stripped, url_text, "Nickname:", 1, "\n", '\n',
			NULL, _("Nickname"), 0, NULL);

	/* extract their RealName and put it in */
	info_extract_field(stripped, url_text, "RealName:", 1, "\n", '\n',
			NULL, _("Realname"), 0, NULL);

	/* extract their Location and put it in */
	info_extract_field(stripped, url_text, "Location:", 2, "\n", '\n',
			NULL, _("Location"), 0, NULL);

	/* extract their Age and put it in */
	info_extract_field(stripped, url_text, "Age:", 3, "\n", '\n',
			NULL, _("Age"), 0, NULL);

	/* extract their MaritalStatus and put it in */
	info_extract_field(stripped, url_text, "MaritalStatus:", 3, "\n", '\n',
			"No Answer", _("Marital Status"), 0, NULL);

	/* extract their Gender and put it in */
	info_extract_field(stripped, url_text, "Gender:", 3, "\n", '\n',
			"No Answer", _("Gender"), 0, NULL);

	/* extract their Occupation and put it in */
	info_extract_field(stripped, url_text, "Occupation:", 2, "\n", '\n',
			NULL, _("Occupation"), 0, NULL);

	/* Hobbies, Latest News, and Favorite Quote are a bit different, since the
	 * values can contain embedded newlines... but any or all of them can also
	 * not appear.  The way we delimit them is to successively look for the next
	 * one that _could_ appear, and if all else fails, we end the section by
	 * looking for the 'Links' heading, which is the next thing to follow this
	 * bunch.
	 */
	if (!info_extract_field(stripped, url_text, "Hobbies:", 1, "Latest News",
				'\n', NULL, _("Hobbies"), 0, NULL))
		if (!info_extract_field(stripped, url_text, "Hobbies:", 1, "Favorite Quote",
					'\n', NULL, _("Hobbies"), 0, NULL))
			info_extract_field(stripped, url_text, "Hobbies:", 1, "Links",
					'\n', NULL, _("Hobbies"), 0, NULL);
	if (!info_extract_field(stripped, url_text, "Latest News:", 1, "Favorite Quote",
				'\n', NULL, _("Latest News"), 0, NULL))
		info_extract_field(stripped, url_text, "Latest News:", 1, "Links",
				'\n', NULL, _("Latest News"), 0, NULL);
	info_extract_field(stripped, url_text, "Favorite Quote:", 0, "Links",
			'\n', NULL, _("Favorite Quote"), 0, NULL);

	/* Home Page will either be "No home page specified",
	 * or "Home Page: " and a link. */
	p = strstr(stripped, "No home page specified");
	if (!p)
		info_extract_field(stripped, url_text, "Home Page:", 1, " ", 0, NULL,
				_("Home Page"), 1, NULL);

	/* Cool Link {1,2,3} is also different.  If "No cool link specified" exists,
	 * then we have none.  If we have one however, we'll need to check and see if
	 * we have a second one.  If we have a second one, we have to check to see if
	 * we have a third one.
	 */
	p = strstr(stripped,"No cool link specified");
	if (!p)
		if (info_extract_field(stripped, url_text, "Cool Link 1:", 1, " ", 0, NULL,
					_("Cool Link 1"), 1, NULL))
			if (info_extract_field(stripped, url_text, "Cool Link 2:", 1, " ", 0, NULL,
						_("Cool Link 2"), 1, NULL))
				info_extract_field(stripped, url_text, "Cool Link 3:", 1, " ", 0, NULL,
						_("Cool Link 3"), 1, NULL);

	/* see if Member Since is there, and if so, extract it. */
	info_extract_field(stripped, url_text, "Member Since:", 1, "Last Updated:",
			'\n', NULL, _("Member Since"), 0, NULL);

	/* extract the Last Updated date and put it in */
	info_extract_field(stripped, url_text, "Last Updated:", 1, "\n", '\n', NULL,
			_("Last Updated"), 0, NULL);

	/* finish off the html */
	strcat(url_text, "</body></html>\n");
	g_free(stripped);

	/* show it to the user */
	g_show_info_text(NULL, NULL, 2, url_text, NULL);
}

static void yahoo_get_info(GaimConnection *gc, const char *name)
{
	/* struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data; */
	char url[256];
	g_snprintf(url, sizeof url, "%s%s", YAHOO_PROFILE_URL, name);
	grab_url(url, FALSE, yahoo_got_info, NULL, NULL, 0);
}

static void yahoo_change_buddys_group(GaimConnection *gc, const char *who,
				   const char *old_group, const char *new_group)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	/* Step 0:  If they aren't on the server list anyway,
	 *          don't bother letting the server know.
	 */
	if (!g_hash_table_lookup(yd->friends, who))
		return;

	/* Step 1:  Add buddy to new group. */
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, new_group);
	yahoo_packet_hash(pkt, 14, "");
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);

	/* Step 2:  Remove buddy from old group */
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, old_group);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static void yahoo_rename_group(GaimConnection *gc, const char *old_group,
                                                 const char *new_group, GList *whocares)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;

	pkt = yahoo_packet_new(YAHOO_SERVICE_GROUPRENAME, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 65, old_group);
	yahoo_packet_hash(pkt, 67, new_group);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_YAHOO,
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_CHAT_TOPIC,
	NULL, /* user_splits */
	NULL, /* protocol_options */
	yahoo_list_icon,
	yahoo_list_emblems,
	yahoo_status_text,
	yahoo_tooltip_text,
	yahoo_away_states,
	yahoo_actions,
	yahoo_buddy_menu,
	yahoo_c_info,
	yahoo_login,
	yahoo_close,
	yahoo_send_im,
	NULL, /* set info */
	yahoo_send_typing,
	yahoo_get_info,
	yahoo_set_away,
	NULL, /* get_away */
	NULL, /* set_dir */
	NULL, /* get_dir */
	NULL, /* dir_search */
	yahoo_set_idle,
	NULL, /* change_passwd*/
	yahoo_add_buddy,
	NULL, /* add_buddies */
	yahoo_remove_buddy,
	NULL, /*remove_buddies */
	NULL, /* add_permit */
	yahoo_add_deny,
	NULL, /* rem_permit */
	yahoo_rem_deny,
	yahoo_set_permit_deny,
	NULL, /* warn */
	yahoo_c_join,
	yahoo_c_invite,
	yahoo_c_leave,
	NULL, /* chat whisper */
	yahoo_c_send,
	yahoo_keepalive,
	NULL, /* register_user */
	NULL, /* get_cb_info */
	NULL, /* get_cb_away */
	NULL, /* alias_buddy */
	yahoo_change_buddys_group,
	yahoo_rename_group,
	NULL, /* buddy_free */
	NULL, /* convo_closed */
	NULL, /* normalize */
	NULL /* set_buddy_icon */
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-yahoo",                                     /**< id             */
	"Yahoo",	                                      /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Yahoo Protocol Plugin"),
	                                                  /**  description    */
	N_("Yahoo Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	yahoo_unload_plugin,                              /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Pager host"), "server",
											YAHOO_PAGER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("Pager port"), "port",
										 YAHOO_PAGER_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);
	my_protocol = plugin;

	yahoo_init_colorht();
}

GAIM_INIT_PLUGIN(yahoo, init_plugin, info);

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
#include "debug.h"
#include "multi.h"
#include "notify.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"

#include "md5.h"

/* XXX */
#include "gaim.h"
#include "ui.h"

extern char *yahoo_crypt(const char *, const char *);

/* #define YAHOO_DEBUG */

#define USEROPT_MAIL 0

#define YAHOO_PAGER_HOST "scs.yahoo.com"
#define YAHOO_PAGER_PORT 5050

#define YAHOO_PROTO_VER 0x0900

enum yahoo_service { /* these are easier to see in hex */
	YAHOO_SERVICE_LOGON = 1,
	YAHOO_SERVICE_LOGOFF,
	YAHOO_SERVICE_ISAWAY,
	YAHOO_SERVICE_ISBACK,
	YAHOO_SERVICE_IDLE, /* 5 (placemarker) */
	YAHOO_SERVICE_MESSAGE,
	YAHOO_SERVICE_IDACT,
	YAHOO_SERVICE_IDDEACT,
	YAHOO_SERVICE_MAILSTAT,
	YAHOO_SERVICE_USERSTAT, /* 0xa */
	YAHOO_SERVICE_NEWMAIL,
	YAHOO_SERVICE_CHATINVITE,
	YAHOO_SERVICE_CALENDAR,
	YAHOO_SERVICE_NEWPERSONALMAIL,
	YAHOO_SERVICE_NEWCONTACT,
	YAHOO_SERVICE_ADDIDENT, /* 0x10 */
	YAHOO_SERVICE_ADDIGNORE,
	YAHOO_SERVICE_PING,
	YAHOO_SERVICE_GROUPRENAME,
	YAHOO_SERVICE_SYSMESSAGE = 0x14,
	YAHOO_SERVICE_PASSTHROUGH2 = 0x16,
	YAHOO_SERVICE_CONFINVITE = 0x18,
	YAHOO_SERVICE_CONFLOGON,
	YAHOO_SERVICE_CONFDECLINE,
	YAHOO_SERVICE_CONFLOGOFF,
	YAHOO_SERVICE_CONFADDINVITE,
	YAHOO_SERVICE_CONFMSG,
	YAHOO_SERVICE_CHATLOGON,
	YAHOO_SERVICE_CHATLOGOFF,
	YAHOO_SERVICE_CHATMSG = 0x20,
	YAHOO_SERVICE_GAMELOGON = 0x28,
	YAHOO_SERVICE_GAMELOGOFF,
	YAHOO_SERVICE_GAMEMSG = 0x2a,
	YAHOO_SERVICE_FILETRANSFER = 0x46,
	YAHOO_SERVICE_NOTIFY = 0x4B,
	YAHOO_SERVICE_AUTHRESP = 0x54,
	YAHOO_SERVICE_LIST = 0x55,
	YAHOO_SERVICE_AUTH = 0x57,
	YAHOO_SERVICE_ADDBUDDY = 0x83,
	YAHOO_SERVICE_REMBUDDY = 0x84
};

enum yahoo_status {
	YAHOO_STATUS_AVAILABLE = 0,
	YAHOO_STATUS_BRB,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_NOTATDESK,
	YAHOO_STATUS_NOTINOFFICE,
	YAHOO_STATUS_ONPHONE,
	YAHOO_STATUS_ONVACATION,
	YAHOO_STATUS_OUTTOLUNCH,
	YAHOO_STATUS_STEPPEDOUT,
	YAHOO_STATUS_INVISIBLE = 12,
	YAHOO_STATUS_CUSTOM = 99,
	YAHOO_STATUS_IDLE = 999,
	YAHOO_STATUS_OFFLINE = 0x5a55aa56, /* don't ask */
	YAHOO_STATUS_TYPING = 0x16
};
#define YAHOO_STATUS_GAME 0x2 /* Games don't fit into the regular status model */

struct yahoo_data {
	int fd;
	guchar *rxqueue;
	int rxlen;
	GHashTable *hash;
	GHashTable *games;
	int current_status;
	gboolean logged_in;
};

struct yahoo_pair {
	int key;
	char *value;
};

struct yahoo_packet {
	guint16 service;
	guint32 status;
	guint32 id;
	GSList *hash;
};

#define YAHOO_PACKET_HDRLEN (4 + 2 + 2 + 2 + 2 + 4 + 4)

static struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, enum yahoo_status status, int id)
{
	struct yahoo_packet *pkt = g_new0(struct yahoo_packet, 1);

	pkt->service = service;
	pkt->status = status;
	pkt->id = id;

	return pkt;
}

static void yahoo_packet_hash(struct yahoo_packet *pkt, int key, const char *value)
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
		char key[64], *value = NULL;
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
			gaim_debug(GAIM_DEBUG_MISC, "yahoo",
					   "Key: %d  \tValue: %s\n", pair->key, pair->value);
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

		if (isprint(data[i]))
			gaim_debug(GAIM_DEBUG_MISC, NULL, "%c ", data[i]);
		else
			gaim_debug(GAIM_DEBUG_MISC, NULL, ". ");
	}

	gaim_debug(GAIM_DEBUG_MISC, NULL, "\n");
#endif
}

static int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt)
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

static void yahoo_packet_free(struct yahoo_packet *pkt)
{
	while (pkt->hash) {
		struct yahoo_pair *pair = pkt->hash->data;
		g_free(pair->value);
		g_free(pair);
		pkt->hash = g_slist_remove(pkt->hash, pair);
	}
	g_free(pkt);
}

static void yahoo_process_status(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	char *name = NULL;
	int state = 0;
	int gamestate = 0;
	char *msg = NULL;
	
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
			break;
		case 10: /* state */
			state = strtol(pair->value, NULL, 10);
			break;
		case 19: /* custom message */
			msg = pair->value;
			break;
		case 11: /* i didn't know what this was in the old protocol either */
			break;
		case 17: /* in chat? */
			break;
		case 13: /* in pager? */
			if (pkt->service == YAHOO_SERVICE_LOGOFF ||
			    strtol(pair->value, NULL, 10) == 0) {
				serv_got_update(gc, name, 0, 0, 0, 0, 0);
				break;
			}
			if (g_hash_table_lookup(yd->games, name))
				gamestate = YAHOO_STATUS_GAME;
			if (state == YAHOO_STATUS_CUSTOM) {
				gpointer val = g_hash_table_lookup(yd->hash, name);
				if (val) {
					g_free(val);
					g_hash_table_insert(yd->hash, name,
							msg ? g_strdup(msg) : g_malloc0(1));
				} else
					g_hash_table_insert(yd->hash, g_strdup(name),
							msg ? g_strdup(msg) : g_malloc0(1));
			}
			if (state == YAHOO_STATUS_AVAILABLE)
				serv_got_update(gc, name, 1, 0, 0, 0, gamestate);
			else if (state == YAHOO_STATUS_IDLE)
				serv_got_update(gc, name, 1, 0, 0, -1, (state << 2) | UC_UNAVAILABLE | gamestate);
			else
				serv_got_update(gc, name, 1, 0, 0, 0, (state << 2) | UC_UNAVAILABLE | gamestate);
			break;
		case 60: /* no clue */
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
	struct buddy *b;
	struct group *g;

	while (l) {
		char **lines;
		char **split;
		char **buddies;
		char **tmp, **bud;

		struct yahoo_pair *pair = l->data;
		l = l->next;

		if (pair->key != 87)
			continue;

		lines = g_strsplit(pair->value, "\n", -1);
		for (tmp = lines; *tmp; tmp++) {
			split = g_strsplit(*tmp, ":", 2);
			if (!split)
				continue;
			if (!split[0] || !split[1]) {
				g_strfreev(split);
				continue;
			}
			buddies = g_strsplit(split[1], ",", -1);
			for (bud = buddies; bud && *bud; bud++)
				if (!(b = gaim_find_buddy(gc->account,  *bud))) {
					if (!(g = gaim_find_group(split[0]))) {
						g = gaim_group_new(split[0]);
						gaim_blist_add_group(g, NULL);
					}
					b = gaim_buddy_new(gc->account, *bud, NULL);
					gaim_blist_add_buddy(b, g, NULL);
					export = TRUE;
				}
			g_strfreev(buddies);
			g_strfreev(split);
		}
		g_strfreev(lines);
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

	if (!msg)
		return;
	
	if (!g_ascii_strncasecmp(msg, "TYPING", strlen("TYPING"))) {
		if (*stat == '1')
			serv_got_typing(gc, from, 0, GAIM_TYPING);
		else
			serv_got_typing_stopped(gc, from);
	} else if (!g_ascii_strncasecmp(msg, "GAME", strlen("GAME"))) {
		struct buddy *bud = gaim_find_buddy(gc->account, from);
		void *free1=NULL, *free2=NULL;
		if (!bud) {
			gaim_debug(GAIM_DEBUG_WARNING, "yahoo",
					   "%s is playing a game, and doesn't want "
					   "you to know.\n", from);
		}

		if (*stat == '1') {
			if (g_hash_table_lookup_extended (yd->games, from, free1, free2)) {
				g_free(free1);
				g_free(free2);
			}
			g_hash_table_insert (yd->games, g_strdup(from), g_strdup(game));
			if (bud)
				serv_got_update(gc, from, 1, 0, 0, 0, bud->uc | YAHOO_STATUS_GAME);
		} else {
			if (g_hash_table_lookup_extended (yd->games, from, free1, free2)) {
				g_free(free1);
				g_free(free2);
				g_hash_table_remove (yd->games, from);
			}
			if (bud)
				serv_got_update(gc, from, 1, 0, 0, 0, bud->uc & ~YAHOO_STATUS_GAME);
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
				int i, j;

				msg = pair->value;

				strip_linefeed(msg);
				m = msg;
				for (i = 0, j = 0; m[i]; i++) {
					if (m[i] == 033) {
						while (m[i] && (m[i] != 'm'))
							i++;
						if (!m[i])
							i--;
						continue;
					}
					msg[j++] = m[i];
				}
				msg[j] = 0;
				serv_got_im(gc, from, msg, 0, tm, -1);

				tm = time(NULL);
			}
			l = l->next;
		}
	} else if (pkt->status == 2) {
		gaim_notify_error(gc, NULL,
						  _("Your Yahoo! message did not get sent."), NULL);
	}
}


static void yahoo_process_contact(GaimConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	char *id = NULL;
	char *who = NULL;
	char *msg = NULL;
	char *name = NULL;
	int state = YAHOO_STATUS_AVAILABLE;
	int online = FALSE;

	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 1)
			id = pair->value;
		else if (pair->key == 3)
			who = pair->value;
		else if (pair->key == 14)
			msg = pair->value;
		else if (pair->key == 7) 
			name = pair->value;
		else if (pair->key == 10)
			state = strtol(pair->value, NULL, 10);
		else if (pair->key == 13)
			online = strtol(pair->value, NULL, 10);
		l = l->next;
	}

	if (id)
		show_got_added(gc, id, who, NULL, msg);
	if (name) {
		if (state == YAHOO_STATUS_AVAILABLE)
			serv_got_update(gc, name, 1, 0, 0, 0, 0);
		else if (state == YAHOO_STATUS_IDLE)
			serv_got_update(gc, name, 1, 0, 0, -1, (state << 2));
		else
			serv_got_update(gc, name, 1, 0, 0, 0, (state << 2) | UC_UNAVAILABLE);
		if (state == YAHOO_STATUS_CUSTOM) {
			gpointer val = g_hash_table_lookup(yd->hash, name);
			if (val) {
				g_free(val);
				g_hash_table_insert(yd->hash, name,
						msg ? g_strdup(msg) : g_malloc0(1));
			} else
				g_hash_table_insert(yd->hash, g_strdup(name),
						msg ? g_strdup(msg) : g_malloc0(1));
		}
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
		GaimAccount *account = gaim_connection_get_account(gc);
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

		pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP, YAHOO_STATUS_AVAILABLE, 0);
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

static void yahoo_packet_process(GaimConnection *gc, struct yahoo_packet *pkt)
{
	switch (pkt->service)
	{
	case YAHOO_SERVICE_LOGON:
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
	case YAHOO_SERVICE_GAMELOGON:
	case YAHOO_SERVICE_GAMELOGOFF:
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
	case YAHOO_SERVICE_LIST:
		yahoo_process_list(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(gc, pkt);
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
		gaim_connection_error(gc, "Unable to read");
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
		gaim_connection_error(gc, "Unable to connect");
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

	gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	yd->fd = -1;
	yd->hash = g_hash_table_new(g_str_hash, g_str_equal);
	yd->games = g_hash_table_new(g_str_hash, g_str_equal);

	if (gaim_proxy_connect(account, gaim_account_get_string(account, "server",  YAHOO_PAGER_HOST),
			  gaim_account_get_int(account, "port", YAHOO_PAGER_PORT),
			  yahoo_got_connected, gc) != 0) {
		gaim_connection_error(gc, "Connection problem");
		return;
	}

}

static gboolean yahoo_destroy_hash(gpointer key, gpointer val, gpointer data)
{
	g_free(key);
	g_free(val);
	return TRUE;
}

static void yahoo_close(GaimConnection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	g_hash_table_foreach_remove(yd->hash, yahoo_destroy_hash, NULL);
	g_hash_table_destroy(yd->hash);
	g_hash_table_foreach_remove(yd->games, yahoo_destroy_hash, NULL);
	g_hash_table_destroy(yd->games);
	if (yd->fd >= 0)
		close(yd->fd);

	if (yd->rxqueue)
		g_free(yd->rxqueue);
	yd->rxlen = 0;
	if (gc->inpa)
		gaim_input_remove(gc->inpa);
	g_free(yd);
}

static const char *yahoo_list_icon(GaimAccount *a, struct buddy *b)
{
	return "yahoo";
}

static void yahoo_list_emblems(struct buddy *b, char **se, char **sw, char **nw, char **ne)
{
	int i = 0;
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	if (b->present == GAIM_BUDDY_OFFLINE) {
		*se = "offline";
		return;
	} else {
		if (b->uc & UC_UNAVAILABLE)
			emblems[i++] = "away";
		if (b->uc & YAHOO_STATUS_GAME)
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
	default:
		return _("Online");
	}
}

static void yahoo_game(GaimConnection *gc, const char *name) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	char *game = g_hash_table_lookup(yd->games, name);
	char *t;
	char url[256];

	if (!game)
		return;
	t = game = g_strdup(strstr(game, "ante?room="));
	while (*t != '\t')
		t++;
	*t = 0;
	g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game);
	open_url(NULL, url);
	g_free(game);
}

static char *yahoo_status_text(struct buddy *b)
{
	struct yahoo_data *yd = (struct yahoo_data*)b->account->gc->proto_data;
	if (b->uc & UC_UNAVAILABLE) {
		if ((b->uc >> 2) != YAHOO_STATUS_CUSTOM)
			return g_strdup(yahoo_get_status_string(b->uc >> 2));
		else {
			char *stripped = strip_html(g_hash_table_lookup(yd->hash, b->name));
			if(stripped) {
				char *ret = g_markup_escape_text(stripped, strlen(stripped));
				g_free(stripped);
				return ret;
			}
		}
	}
	return NULL;
}

static char *yahoo_tooltip_text(struct buddy *b)
{
	struct yahoo_data *yd = (struct yahoo_data*)b->account->gc->proto_data;
	if (b->uc & UC_UNAVAILABLE) {
		char *status;
		char *ret;
		if ((b->uc >> 2) != YAHOO_STATUS_CUSTOM)
			status = g_strdup(yahoo_get_status_string(b->uc >> 2));
		else
			status = strip_html(g_hash_table_lookup(yd->hash, b->name));
		if(status) {
			char *escaped = g_markup_escape_text(status, strlen(status));
			ret = g_strdup_printf(_("<b>Status:</b> %s"), escaped);
			g_free(status);
			g_free(escaped);
			return ret;
		}
	}
	return NULL;
}

static GList *yahoo_buddy_menu(GaimConnection *gc, const char *who)
{
	GList *m = NULL;
	struct proto_buddy_menu *pbm;
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct buddy *b = gaim_find_buddy(gc->account, who); /* this should never be null. if it is,
						  segfault and get the bug report. */
	static char buf2[1024];

	if (b->uc | YAHOO_STATUS_GAME) {
		char *game = g_hash_table_lookup(yd->games, b->name);
		char *room;
		if (!game)
			return m;
		if (game) {
			char *t;
			pbm = g_new0(struct proto_buddy_menu, 1);
			if (!(room = strstr(game, "&follow="))) /* skip ahead to the url */
				return NULL;
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

static int yahoo_send_im(GaimConnection *gc, const char *who, const char *what, int len, int flags)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, 0);
	char *msg = g_strdup(what);

	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 14, msg);
	yahoo_packet_hash(pkt, 97, "1");

	yahoo_send_packet(yd, pkt);

	yahoo_packet_free(pkt);
	
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
	if (yd->current_status == YAHOO_STATUS_CUSTOM)
		yahoo_packet_hash(pkt, 19, msg);

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
	}

	if (pkt) {
		char buf[4];
		g_snprintf(buf, sizeof(buf), "%d", yd->current_status);
		yahoo_packet_hash(pkt, 10, buf);
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
}

static void yahoo_add_buddy(GaimConnection *gc, const char *who)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	struct group *g;
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

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, 1, gaim_connection_get_display_name(gc));
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, group);
	yahoo_send_packet(yd, pkt);
	yahoo_packet_free(pkt);
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PROTO_YAHOO,
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_USE_POINTSIZE,
	NULL,
	NULL,
	yahoo_list_icon,
	yahoo_list_emblems,
	yahoo_status_text,
	yahoo_tooltip_text,
	yahoo_away_states,
	yahoo_actions,
	yahoo_buddy_menu,
	NULL,
	yahoo_login,
	yahoo_close,
	yahoo_send_im,
	NULL,
	yahoo_send_typing,
	NULL,
	yahoo_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
	yahoo_set_idle,
	NULL,
	yahoo_add_buddy,
	NULL,
	yahoo_remove_buddy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	yahoo_keepalive,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
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
	WEBSITE,                                          /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
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
}

GAIM_INIT_PLUGIN(yahoo, init_plugin, info);

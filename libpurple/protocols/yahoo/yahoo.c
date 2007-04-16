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
#include "cipher.h"
#include "cmds.h"
#include "core.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "proxy.h"
#include "request.h"
#include "server.h"
#include "util.h"
#include "version.h"

#include "yahoo.h"
#include "yahoochat.h"
#include "yahoo_auth.h"
#include "yahoo_crypt.h"
#include "yahoo_doodle.h"
#include "yahoo_filexfer.h"
#include "yahoo_friend.h"
#include "yahoo_packet.h"
#include "yahoo_picture.h"
#include "ycht.h"

/* #define YAHOO_DEBUG */

/* #define TRY_WEBMESSENGER_LOGIN 0 */

static void yahoo_add_buddy(PurpleConnection *gc, PurpleBuddy *, PurpleGroup *);
#ifdef TRY_WEBMESSENGER_LOGIN
static void yahoo_login_page_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, size_t len, const gchar *error_message);
#endif
static void yahoo_set_status(PurpleAccount *account, PurpleStatus *status);

static void
yahoo_add_permit(PurpleConnection *gc, const char *who)
{
	purple_debug_info("yahoo",
			"Permitting ID %s local contact rights for account %s\n", who, gc->account);
	purple_privacy_permit_add(gc->account,who,TRUE);
}

static void
yahoo_rem_permit(PurpleConnection *gc, const char *who)
{
	purple_debug_info("yahoo",
			"Denying ID %s local contact rights for account %s\n", who, gc->account);
	purple_privacy_permit_remove(gc->account,who,TRUE);
}

gboolean yahoo_privacy_check(PurpleConnection *gc, const char *who)
{
	/* returns TRUE if allowed through, FALSE otherwise */
	gboolean permitted;

	permitted = purple_privacy_check(gc->account, who);

	/* print some debug info */
	if (!permitted) {
		char *deb = NULL;
		switch (gc->account->perm_deny)
		{
			case PURPLE_PRIVACY_DENY_ALL:
				deb = "PURPLE_PRIVACY_DENY_ALL";
			break;
			case PURPLE_PRIVACY_DENY_USERS:
				deb = "PURPLE_PRIVACY_DENY_USERS";
			break;
			case PURPLE_PRIVACY_ALLOW_BUDDYLIST:
				deb = "PURPLE_PRIVACY_ALLOW_BUDDYLIST";
			break;
		}
		if(deb)
			purple_debug_info("yahoo",
				"%s blocked data received from %s (%s)\n",
				gc->account->username,who, deb);
	} else if (gc->account->perm_deny == PURPLE_PRIVACY_ALLOW_USERS) {
		purple_debug_info("yahoo",
			"%s allowed data received from %s (PURPLE_PRIVACY_ALLOW_USERS)\n",
			gc->account->username,who);
	}

	return permitted;
}

static void yahoo_update_status(PurpleConnection *gc, const char *name, YahooFriend *f)
{
	char *status = NULL;

	if (!gc || !name || !f || !purple_find_buddy(purple_connection_get_account(gc), name))
		return;

	if (f->status == YAHOO_STATUS_OFFLINE)
	{
		return;
	}

	switch (f->status) {
	case YAHOO_STATUS_AVAILABLE:
		status = YAHOO_STATUS_TYPE_AVAILABLE;
		break;
	case YAHOO_STATUS_BRB:
		status = YAHOO_STATUS_TYPE_BRB;
		break;
	case YAHOO_STATUS_BUSY:
		status = YAHOO_STATUS_TYPE_BUSY;
		break;
	case YAHOO_STATUS_NOTATHOME:
		status = YAHOO_STATUS_TYPE_NOTATHOME;
		break;
	case YAHOO_STATUS_NOTATDESK:
		status = YAHOO_STATUS_TYPE_NOTATDESK;
		break;
	case YAHOO_STATUS_NOTINOFFICE:
		status = YAHOO_STATUS_TYPE_NOTINOFFICE;
		break;
	case YAHOO_STATUS_ONPHONE:
		status = YAHOO_STATUS_TYPE_ONPHONE;
		break;
	case YAHOO_STATUS_ONVACATION:
		status = YAHOO_STATUS_TYPE_ONVACATION;
		break;
	case YAHOO_STATUS_OUTTOLUNCH:
		status = YAHOO_STATUS_TYPE_OUTTOLUNCH;
		break;
	case YAHOO_STATUS_STEPPEDOUT:
		status = YAHOO_STATUS_TYPE_STEPPEDOUT;
		break;
	case YAHOO_STATUS_INVISIBLE: /* this should never happen? */
		status = YAHOO_STATUS_TYPE_INVISIBLE;
		break;
	case YAHOO_STATUS_CUSTOM:
	case YAHOO_STATUS_IDLE:
		if (!f->away)
			status = YAHOO_STATUS_TYPE_AVAILABLE;
		else
			status = YAHOO_STATUS_TYPE_AWAY;
		break;
	default:
		purple_debug_warning("yahoo", "Warning, unknown status %d\n", f->status);
		break;
	}

	if (status) {
		if (f->status == YAHOO_STATUS_CUSTOM)
			purple_prpl_got_user_status(purple_connection_get_account(gc), name, status, "message",
			                          yahoo_friend_get_status_message(f), NULL);
		else
			purple_prpl_got_user_status(purple_connection_get_account(gc), name, status, NULL);
	}

	if (f->idle != 0)
		purple_prpl_got_user_idle(purple_connection_get_account(gc), name, TRUE, f->idle);
	else
		purple_prpl_got_user_idle(purple_connection_get_account(gc), name, FALSE, 0);

	if (f->sms)
		purple_prpl_got_user_status(purple_connection_get_account(gc), name, YAHOO_STATUS_TYPE_MOBILE, NULL);
	else
		purple_prpl_got_user_status_deactive(purple_connection_get_account(gc), name, YAHOO_STATUS_TYPE_MOBILE);
}

static void yahoo_process_status(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	YahooFriend *f = NULL;
	char *name = NULL;

	if (pkt->service == YAHOO_SERVICE_LOGOFF && pkt->status == -1) {
		gc->wants_to_die = TRUE;
		purple_connection_error(gc, _("You have signed on from another location."));
		return;
	}

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 0: /* we won't actually do anything with this */
			break;
		case 1: /* we don't get the full buddy list here. */
			if (!yd->logged_in) {
				purple_connection_set_display_name(gc, pair->value);
				purple_connection_set_state(gc, PURPLE_CONNECTED);
				yd->logged_in = TRUE;
				if (yd->picture_upload_todo) {
					yahoo_buddy_icon_upload(gc, yd->picture_upload_todo);
					yd->picture_upload_todo = NULL;
				}
				yahoo_set_status(account, purple_account_get_active_status(account));

				/* this requests the list. i have a feeling that this is very evil
				 *
				 * scs.yahoo.com sends you the list before this packet without  it being
				 * requested
				 *
				 * do_import(gc, NULL);
				 * newpkt = yahoo_packet_new(YAHOO_SERVICE_LIST, YAHOO_STATUS_OFFLINE, 0);
				 * yahoo_packet_send_and_free(newpkt, yd);
				 */

				}
			break;
		case 8: /* how many online buddies we have */
			break;
		case 7: /* the current buddy */
			if (name && f) /* update the previous buddy before changing the variables */
				yahoo_update_status(gc, name, f);
			name = pair->value;
			if (name && g_utf8_validate(name, -1, NULL))
				f = yahoo_friend_find_or_new(gc, name);
			else {
				f = NULL;
				name = NULL;
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

			if (f->status == YAHOO_STATUS_IDLE) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
					f->idle = time(NULL);
			} else
				f->idle = 0;

			if (f->status != YAHOO_STATUS_CUSTOM)
				yahoo_friend_set_status_message(f, NULL);

			f->sms = 0;
			break;
		case 19: /* custom message */
			if (f)
				yahoo_friend_set_status_message(f, yahoo_string_decode(gc, pair->value, FALSE));
			break;
		case 11: /* this is the buddy's session id */
			break;
		case 17: /* in chat? */
			break;
		case 47: /* is custom status away or not? 2=idle*/
			if (!f)
				break;

			/* I have no idea what it means when this is
			 * set when someone's available, but it doesn't
			 * mean idle. */
			if (f->status == YAHOO_STATUS_AVAILABLE)
				break;

			f->away = strtol(pair->value, NULL, 10);
			if (f->away == 2) {
				/* Idle may have already been set in a more precise way in case 137 */
				if (f->idle == 0)
					f->idle = time(NULL);
			}

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
				if (name) {
					purple_prpl_got_user_status(account, name, "offline", NULL);
			                purple_prpl_got_user_status_deactive(account, name, YAHOO_STATUS_TYPE_MOBILE);
				}
				break;
			}
			break;
		case 60: /* SMS */
			if (f) {
				f->sms = strtol(pair->value, NULL, 10);
				yahoo_update_status(gc, name, f);
			}
			break;
		case 197: /* Avatars */
		{
			guchar *decoded;
			char *tmp;
			gsize len;

			if (pair->value) {
				decoded = purple_base64_decode(pair->value, &len);
				if (len) {
					tmp = purple_str_binary_to_ascii(decoded, len);
					purple_debug_info("yahoo", "Got key 197, value = %s\n", tmp);
					g_free(tmp);
				}
				g_free(decoded);
			}
			break;
		}
		case 192: /* Pictures, aka Buddy Icons, checksum */
		{
			int cksum = strtol(pair->value, NULL, 10);
			PurpleBuddy *b;

			if (!name)
				break;

			b = purple_find_buddy(gc->account, name);

			if (!cksum || (cksum == -1)) {
				if (f)
					yahoo_friend_set_buddy_icon_need_request(f, TRUE);
				purple_buddy_icons_set_for_user(gc->account, name, NULL, 0);
				if (b)
					purple_blist_node_remove_setting((PurpleBlistNode *)b, YAHOO_ICON_CHECKSUM_KEY);
				break;
			}

			if (!f)
				break;

			yahoo_friend_set_buddy_icon_need_request(f, FALSE);
			if (b && cksum != purple_blist_node_get_int((PurpleBlistNode*)b, YAHOO_ICON_CHECKSUM_KEY))
				yahoo_send_picture_request(gc, name);

			break;
		}
		case 16: /* Custom error message */
			{
				char *tmp = yahoo_string_decode(gc, pair->value, TRUE);
				purple_notify_error(gc, NULL, tmp, NULL);
				g_free(tmp);
			}
			break;
		default:
			purple_debug(PURPLE_DEBUG_ERROR, "yahoo",
					   "Unknown status key %d\n", pair->key);
			break;
		}

		l = l->next;
	}

	if (name && f) /* update the last buddy */
		yahoo_update_status(gc, name, f);
}

static void yahoo_do_group_check(PurpleAccount *account, GHashTable *ht, const char *name, const char *group)
{
	PurpleBuddy *b;
	PurpleGroup *g;
	GSList *list, *i;
	gboolean onlist = 0;
	char *oname = NULL;
	char **oname_p = &oname;
	GSList **list_p = &list;

	if (!g_hash_table_lookup_extended(ht, purple_normalize(account, name), (gpointer *) oname_p, (gpointer *) list_p))
		list = purple_find_buddies(account, name);
	else
		g_hash_table_steal(ht, name);

	for (i = list; i; i = i->next) {
		b = i->data;
		g = purple_buddy_get_group(b);
		if (!purple_utf8_strcasecmp(group, g->name)) {
			purple_debug(PURPLE_DEBUG_MISC, "yahoo",
				"Oh good, %s is in the right group (%s).\n", name, group);
			list = g_slist_delete_link(list, i);
			onlist = 1;
			break;
		}
	}

	if (!onlist) {
		purple_debug(PURPLE_DEBUG_MISC, "yahoo",
			"Uhoh, %s isn't on the list (or not in this group), adding him to group %s.\n", name, group);
		if (!(g = purple_find_group(group))) {
			g = purple_group_new(group);
			purple_blist_add_group(g, NULL);
		}
		b = purple_buddy_new(account, name, NULL);
		purple_blist_add_buddy(b, NULL, g, NULL);
	}

	if (list) {
		if (!oname)
			oname = g_strdup(purple_normalize(account, name));
		g_hash_table_insert(ht, oname, list);
	} else if (oname)
		g_free(oname);
}

static void yahoo_do_group_cleanup(gpointer key, gpointer value, gpointer user_data)
{
	char *name = key;
	GSList *list = value, *i;
	PurpleBuddy *b;
	PurpleGroup *g;

	for (i = list; i; i = i->next) {
		b = i->data;
		g = purple_buddy_get_group(b);
		purple_debug(PURPLE_DEBUG_MISC, "yahoo", "Deleting Buddy %s from group %s.\n", name, g->name);
		purple_blist_remove_buddy(b);
	}
}

static char *_getcookie(char *rawcookie)
{
	char *cookie = NULL;
	char *tmpcookie;
	char *cookieend;

	if (strlen(rawcookie) < 2)
		return NULL;
	tmpcookie = g_strdup(rawcookie+2);
	cookieend = strchr(tmpcookie, ';');

	if (cookieend)
		*cookieend = '\0';

	cookie = g_strdup(tmpcookie);
	g_free(tmpcookie);

	return cookie;
}

static void yahoo_process_cookie(struct yahoo_data *yd, char *c)
{
	if (c[0] == 'Y') {
		if (yd->cookie_y)
			g_free(yd->cookie_y);
		yd->cookie_y = _getcookie(c);
	} else if (c[0] == 'T') {
		if (yd->cookie_t)
			g_free(yd->cookie_t);
		yd->cookie_t = _getcookie(c);
	}
}

static void yahoo_process_list_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

	PurpleAccount *account = purple_connection_get_account(gc);
	GHashTable *ht;
	char *grp = NULL;
	char *norm_bud = NULL;
	YahooFriend *f = NULL; /* It's your friends. They're going to want you to share your StarBursts. */
	                       /* But what if you had no friends? */
	PurpleBuddy *b;
	PurpleGroup *g;


	ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		switch (pair->key) {
		case 302: 
			/* This is always 318 before a group, 319 before the first s/n in a group, 320 before any ignored s/n.
			 * It is not sent for s/n's in a group after the first.
			 * All ignored s/n's are listed last, so when we see a 320 we clear the group and begin marking the
			 * s/n's as ignored.  It is always followed by an identical 300 key.
			 */
			if (pair->value && !strcmp(pair->value, "320")) {
				/* No longer in any group; this indicates the start of the ignore list. */
				g_free(grp);
				grp = NULL;
			}

			break;
		case 301: /* This is 319 before all s/n's in a group after the first. It is followed by an identical 300. */
			break;
		case 300: /* This is 318 before a group, 319 before any s/n in a group, and 320 before any ignored s/n. */
			break;
		case 65: /* This is the group */
			g_free(grp);
			grp = yahoo_string_decode(gc, pair->value, FALSE);
			break;
		case 7: /* buddy's s/n */
			g_free(norm_bud);
			norm_bud = g_strdup(purple_normalize(account, pair->value));

			if (grp) {
				/* This buddy is in a group */
				f = yahoo_friend_find_or_new(gc, norm_bud);
				if (!(b = purple_find_buddy(account, norm_bud))) {
					if (!(g = purple_find_group(grp))) {
						g = purple_group_new(grp);
						purple_blist_add_group(g, NULL);
					}
					b = purple_buddy_new(account, norm_bud, NULL);
					purple_blist_add_buddy(b, NULL, g, NULL);
				}
				yahoo_do_group_check(account, ht, norm_bud, grp);

			} else {
				/* This buddy is on the ignore list (and therefore in no group) */
				purple_privacy_deny_add(account, norm_bud, 1);				
			}
			break;
		case 241: /* another protocol user */
			if (f) {
				f->protocol = strtol(pair->value, NULL, 10);
				purple_debug_info("yahoo", "Setting protocol to %d\n", f->protocol);
			}
			break;
		/* case 242: */ /* this seems related to 241 */
			/* break; */
		}
	}

	g_hash_table_foreach(ht, yahoo_do_group_cleanup, NULL);
	g_hash_table_destroy(ht);
	g_free(grp);
	g_free(norm_bud);
}

static void yahoo_process_list(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	gboolean export = FALSE;
	gboolean got_serv_list = FALSE;
	PurpleBuddy *b;
	PurpleGroup *g;
	YahooFriend *f = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	GHashTable *ht;

	char **lines;
	char **split;
	char **buddies;
	char **tmp, **bud, *norm_bud;
	char *grp = NULL;

	if (pkt->id)
		yd->session_id = pkt->id;

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
		case 59: /* cookies, yum */
			yahoo_process_cookie(yd, pair->value);
			break;
		case YAHOO_SERVICE_PRESENCE_PERM:
			if (!yd->tmp_serv_plist)
				yd->tmp_serv_plist = g_string_new(pair->value);
			else
				g_string_append(yd->tmp_serv_plist, pair->value);
			break;
		}
	}

	if (pkt->status != 0)
		return;

	if (yd->tmp_serv_blist) {
		ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_slist_free);

		lines = g_strsplit(yd->tmp_serv_blist->str, "\n", -1);
		for (tmp = lines; *tmp; tmp++) {
			split = g_strsplit(*tmp, ":", 2);
			if (!split)
				continue;
			if (!split[0] || !split[1]) {
				g_strfreev(split);
				continue;
			}
			grp = yahoo_string_decode(gc, split[0], FALSE);
			buddies = g_strsplit(split[1], ",", -1);
			for (bud = buddies; bud && *bud; bud++) {
				norm_bud = g_strdup(purple_normalize(account, *bud));
				f = yahoo_friend_find_or_new(gc, norm_bud);

				if (!(b = purple_find_buddy(account, norm_bud))) {
					if (!(g = purple_find_group(grp))) {
						g = purple_group_new(grp);
						purple_blist_add_group(g, NULL);
					}
					b = purple_buddy_new(account, norm_bud, NULL);
					purple_blist_add_buddy(b, NULL, g, NULL);
					export = TRUE;
				}

				yahoo_do_group_check(account, ht, norm_bud, grp);
				g_free(norm_bud);
			}
			g_strfreev(buddies);
			g_strfreev(split);
			g_free(grp);
		}
		g_strfreev(lines);

		g_string_free(yd->tmp_serv_blist, TRUE);
		yd->tmp_serv_blist = NULL;
		g_hash_table_foreach(ht, yahoo_do_group_cleanup, NULL);
		g_hash_table_destroy(ht);
	}

	if (yd->tmp_serv_ilist) {
		buddies = g_strsplit(yd->tmp_serv_ilist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			/* The server is already ignoring the user */
			got_serv_list = TRUE;
			purple_privacy_deny_add(gc->account, *bud, 1);
		}
		g_strfreev(buddies);

		g_string_free(yd->tmp_serv_ilist, TRUE);
		yd->tmp_serv_ilist = NULL;
	}

	if (got_serv_list &&
		((gc->account->perm_deny != PURPLE_PRIVACY_ALLOW_BUDDYLIST) &&
		(gc->account->perm_deny != PURPLE_PRIVACY_DENY_ALL) &&
		(gc->account->perm_deny != PURPLE_PRIVACY_ALLOW_USERS)))
	{
		gc->account->perm_deny = PURPLE_PRIVACY_DENY_USERS;
		purple_debug_info("yahoo", "%s privacy defaulting to PURPLE_PRIVACY_DENY_USERS.\n",
		      gc->account->username);
	}

	if (yd->tmp_serv_plist) {
		buddies = g_strsplit(yd->tmp_serv_plist->str, ",", -1);
		for (bud = buddies; bud && *bud; bud++) {
			f = yahoo_friend_find(gc, *bud);
			if (f) {
				purple_debug_info("yahoo", "%s setting presence for %s to PERM_OFFLINE\n",
								 gc->account->username, *bud);
				f->presence = YAHOO_PRESENCE_PERM_OFFLINE;
			}
		}
		g_strfreev(buddies);
		g_string_free(yd->tmp_serv_plist, TRUE);
		yd->tmp_serv_plist = NULL;

	}
}

static void yahoo_process_notify(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	char *stat = NULL;
	char *game = NULL;
	YahooFriend *f = NULL;
	GSList *l = pkt->hash;

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

	if (!g_ascii_strncasecmp(msg, "TYPING", strlen("TYPING"))
		&& (yahoo_privacy_check(gc, from)))  {
		if (*stat == '1')
			serv_got_typing(gc, from, 0, PURPLE_TYPING);
		else
			serv_got_typing_stopped(gc, from);
	} else if (!g_ascii_strncasecmp(msg, "GAME", strlen("GAME"))) {
		PurpleBuddy *bud = purple_find_buddy(gc->account, from);

		if (!bud) {
			purple_debug(PURPLE_DEBUG_WARNING, "yahoo",
					   "%s is playing a game, and doesn't want "
					   "you to know.\n", from);
		}

		f = yahoo_friend_find(gc, from);
		if (!f)
			return; /* if they're not on the list, don't bother */

		yahoo_friend_set_game(f, NULL);

		if (*stat == '1') {
			yahoo_friend_set_game(f, game);
			if (bud)
				yahoo_update_status(gc, from, f);
		}
	}
}


struct _yahoo_im {
	char *from;
	int time;
	int utf8;
	int buddy_icon;
	char *msg;
};

static void yahoo_process_message(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	struct yahoo_data *yd = gc->proto_data;
	GSList *l = pkt->hash;
	GSList *list = NULL;
	struct _yahoo_im *im = NULL;

	const char *imv = NULL;

	if (pkt->status <= 1 || pkt->status == 5) {
		while (l != NULL) {
			struct yahoo_pair *pair = l->data;
			if (pair->key == 4) {
				im = g_new0(struct _yahoo_im, 1);
				list = g_slist_append(list, im);
				im->from = pair->value;
				im->time = time(NULL);
			}
			if (pair->key == 97)
				if (im)
					im->utf8 = strtol(pair->value, NULL, 10);
			if (pair->key == 15)
				if (im)
					im->time = strtol(pair->value, NULL, 10);
			if (pair->key == 206)
				if (im)
					im->buddy_icon = strtol(pair->value, NULL, 10);
			if (pair->key == 14) {
				if (im)
					im->msg = pair->value;
			}
			/* IMV key */
			if (pair->key == 63)
			{
				imv = pair->value;
			}
			l = l->next;
		}
	} else if (pkt->status == 2) {
		purple_notify_error(gc, NULL,
		                  _("Your Yahoo! message did not get sent."), NULL);
	}

	/** TODO: It seems that this check should be per IM, not global */
	/* Check for the Doodle IMV */
	if (im != NULL && imv!= NULL && im->from != NULL)
	{
		g_hash_table_replace(yd->imvironments, g_strdup(im->from), g_strdup(imv));

		if (strcmp(imv, "doodle;11") == 0)
		{
			PurpleWhiteboard *wb;

			if (!yahoo_privacy_check(gc, im->from)) {
				purple_debug_info("yahoo", "Doodle request from %s dropped.\n", im->from);
				return;
			}

			wb = purple_whiteboard_get_session(gc->account, im->from);

			/* If a Doodle session doesn't exist between this user */
			if(wb == NULL)
			{
				wb = purple_whiteboard_create(gc->account, im->from, DOODLE_STATE_REQUESTED);

				yahoo_doodle_command_send_request(gc, im->from);
				yahoo_doodle_command_send_ready(gc, im->from);
			}
		}
	}

	for (l = list; l; l = l->next) {
		YahooFriend *f;
		char *m, *m2;
		im = l->data;

		if (!im->from || !im->msg) {
			g_free(im);
			continue;
		}

		if (!yahoo_privacy_check(gc, im->from)) {
			purple_debug_info("yahoo", "Message from %s dropped.\n", im->from);
			return;
		}

		m = yahoo_string_decode(gc, im->msg, im->utf8);
		/* This may actually not be necessary, but it appears
		 * that at least at one point some clients were sending
		 * "\r\n" as line delimiters, so we want to avoid double
		 * lines. */
		m2 = purple_strreplace(m, "\r\n", "\n");
		g_free(m);
		m = m2;
		purple_util_chrreplace(m, '\r', '\n');

		if (!strcmp(m, "<ding>")) {
			PurpleBuddy *buddy;
			PurpleAccount *account;
			PurpleConversation *c;
			char *username, *str;
			
			account = purple_connection_get_account(gc);
			c = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, im->from);
			
			if ((buddy = purple_find_buddy(account, im->from)) != NULL)
				username = g_markup_escape_text(purple_buddy_get_alias(buddy), -1);
			else
				username = g_markup_escape_text(im->from, -1);
			
			str = g_strdup_printf(_("%s just sent you a Buzz!"), username);
			
			purple_conversation_write(c, NULL, str, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NOTIFY, im->time);

			g_free(username);
			g_free(str);
			g_free(m);
			g_free(im);
			continue;
		}

		m2 = yahoo_codes_to_html(m);
		g_free(m);
		serv_got_im(gc, im->from, m2, 0, im->time);
		g_free(m2);

		if ((f = yahoo_friend_find(gc, im->from)) && im->buddy_icon == 2) {
			if (yahoo_friend_get_buddy_icon_need_request(f)) {
				yahoo_send_picture_request(gc, im->from);
				yahoo_friend_set_buddy_icon_need_request(f, FALSE);
			}
		}

		g_free(im);
	}
	g_slist_free(list);
}

static void yahoo_process_sysmessage(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *prim, *me = NULL, *msg = NULL, *escmsg = NULL;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 5)
			me = pair->value;
		if (pair->key == 14)
			msg = pair->value;

		l = l->next;
	}

	if (!msg || !g_utf8_validate(msg, -1, NULL))
		return;

	/* TODO: Does this really need to be escaped?  It seems like it doesn't. */
	escmsg = g_markup_escape_text(msg, -1);

	prim = g_strdup_printf(_("Yahoo! system message for %s:"),
	                       me?me:purple_connection_get_display_name(gc));
	purple_notify_info(NULL, NULL, prim, escmsg);
	g_free(prim);
	g_free(escmsg);
}

struct yahoo_add_request {
	PurpleConnection *gc;
	char *id;
	char *who;
	char *msg;
};

static void
yahoo_buddy_add_authorize_cb(struct yahoo_add_request *add_req) {
	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req->msg);
	g_free(add_req);
}

static void
yahoo_buddy_add_deny_cb(struct yahoo_add_request *add_req, const char *msg) {
	struct yahoo_packet *pkt;
	char *encoded_msg = NULL;
	struct yahoo_data *yd = add_req->gc->proto_data;

	if (msg)
		encoded_msg = yahoo_string_encode(add_req->gc, msg, NULL);

	pkt = yahoo_packet_new(YAHOO_SERVICE_REJECTCONTACT,
			YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, "sss",
			1, purple_normalize(add_req->gc->account,
				purple_account_get_username(
					purple_connection_get_account(
						add_req->gc))),
			7, add_req->who,
			14, encoded_msg ? encoded_msg : "");

	yahoo_packet_send_and_free(pkt, yd);

	g_free(encoded_msg);

	g_free(add_req->id);
	g_free(add_req->who);
	g_free(add_req->msg);
	g_free(add_req);
}

static void
yahoo_buddy_add_deny_noreason_cb(struct yahoo_add_request *add_req, const char*msg)
{
	yahoo_buddy_add_deny_cb(add_req, NULL);
}

static void
yahoo_buddy_add_deny_reason_cb(struct yahoo_add_request *add_req) {
	purple_request_input(add_req->gc, NULL, _("Authorization denied message:"),
			NULL, _("No reason given."), TRUE, FALSE, NULL, 
			_("OK"), G_CALLBACK(yahoo_buddy_add_deny_cb),
			_("Cancel"), G_CALLBACK(yahoo_buddy_add_deny_noreason_cb), add_req);
}

static void yahoo_buddy_added_us(PurpleConnection *gc, struct yahoo_packet *pkt) {
	struct yahoo_add_request *add_req;
	char *msg = NULL;
	GSList *l = pkt->hash;

	add_req = g_new0(struct yahoo_add_request, 1);
	add_req->gc = gc;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 1:
			add_req->id = g_strdup(pair->value);
			break;
		case 3:
			add_req->who = g_strdup(pair->value);
			break;
		case 15: /* time, for when they add us and we're offline */
			break;
		case 14:
			msg = pair->value;
			break;
		}
		l = l->next;
	}

	if (add_req->id) {
		if (msg)
			add_req->msg = yahoo_string_decode(gc, msg, FALSE);

		/* DONE! this is almost exactly the same as what MSN does,
		 * this should probably be moved to the core.
		 */
		 purple_account_request_authorization(purple_connection_get_account(gc), add_req->who, add_req->id,
                                                    NULL, add_req->msg, purple_find_buddy(purple_connection_get_account(gc),add_req->who) != NULL,
						    G_CALLBACK(yahoo_buddy_add_authorize_cb), 
						    G_CALLBACK(yahoo_buddy_add_deny_reason_cb),
                                                    add_req);
	} else {
		g_free(add_req->id);
		g_free(add_req->who);
		/*g_free(add_req->msg);*/
		g_free(add_req);
	}
}

static void yahoo_buddy_denied_our_add(PurpleConnection *gc, struct yahoo_packet *pkt)
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
		char *msg2;
		buf = g_string_sized_new(0);
		if (!msg) {
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list."), who);
		} else {
			msg2 = yahoo_string_decode(gc, msg, FALSE);
			g_string_printf(buf, _("%s has (retroactively) denied your request to add them to your list for the following reason: %s."), who, msg2);
			g_free(msg2);
		}
		purple_notify_info(gc, NULL, _("Add buddy rejected"), buf->str);
		g_string_free(buf, TRUE);
		g_hash_table_remove(yd->friends, who);
		purple_prpl_got_user_status(purple_connection_get_account(gc), who, "offline", NULL); /* FIXME: make this set not on list status instead */
		/* TODO: Shouldn't we remove the buddy from our local list? */
	}
}

static void yahoo_process_contact(PurpleConnection *gc, struct yahoo_packet *pkt)
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

#define OUT_CHARSET "utf-8"

static char *yahoo_decode(const char *text)
{
	char *converted = NULL;
	char *n, *new;
	const char *end, *p;
	int i, k;

	n = new = g_malloc(strlen (text) + 1);
	end = text + strlen(text);

	for (p = text; p < end; p++, n++) {
		if (*p == '\\') {
			if (p[1] >= '0' && p[1] <= '7') {
				p += 1;
				for (i = 0, k = 0; k < 3; k += 1) {
					char c = p[k];
					if (c < '0' || c > '7') break;
					i *= 8;
					i += c - '0';
				}
				*n = i;
				p += k - 1;
			} else { /* bug 959248 */
				/* If we see a \ not followed by an octal number,
				 * it means that it is actually a \\ with one \
				 * already eaten by some unknown function.
				 * This is arguably broken.
				 *
				 * I think wing is wrong here, there is no function
				 * called that I see that could have done it. I guess
				 * it is just really sending single \'s. That's yahoo
				 * for you.
				 */
				*n = *p;
			}
		}
		else
			*n = *p;
	}

	*n = '\0';

	if (strstr(text, "\033$B"))
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-2022-jp", NULL, NULL, NULL);
	if (!converted)
		converted = g_convert(new, n - new, OUT_CHARSET, "iso-8859-1", NULL, NULL, NULL);
	g_free(new);

	return converted;
}

static void yahoo_process_mail(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	char *who = NULL;
	char *email = NULL;
	char *subj = NULL;
	char *yahoo_mail_url = (yd->jp? YAHOOJP_MAIL_URL: YAHOO_MAIL_URL);
	int count = 0;
	GSList *l = pkt->hash;

	if (!purple_account_get_check_mail(account))
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
		char *dec_who = yahoo_decode(who);
		char *dec_subj = yahoo_decode(subj);
		char *from = g_strdup_printf("%s (%s)", dec_who, email);

		purple_notify_email(gc, dec_subj, from, purple_account_get_username(account),
						  yahoo_mail_url, NULL, NULL);

		g_free(dec_who);
		g_free(dec_subj);
		g_free(from);
	} else if (count > 0) {
		const char *to = purple_account_get_username(account);
		const char *url = yahoo_mail_url;

		purple_notify_emails(gc, count, FALSE, NULL, NULL, &to, &url,
						   NULL, NULL);
	}
}
/* This is the y64 alphabet... it's like base64, but has a . and a _ */
static const char base64digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

/* This is taken from Sylpheed by Hiroyuki Yamamoto.  We have our own tobase64 function
 * in util.c, but it has a bug I don't feel like finding right now ;) */
static void to_y64(char *out, const unsigned char *in, gsize inlen)
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

static void yahoo_process_auth_old(PurpleConnection *gc, const char *seed)
{
	struct yahoo_packet *pack;
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *name = purple_normalize(account, purple_account_get_username(account));
	const char *pass = purple_connection_get_password(gc);
	struct yahoo_data *yd = gc->proto_data;

	/* So, Yahoo has stopped supporting its older clients in India, and undoubtedly
	 * will soon do so in the rest of the world.
	 *
	 * The new clients use this authentication method.  I warn you in advance, it's
	 * bizarre, convoluted, inordinately complicated.  It's also no more secure than
	 * crypt() was.  The only purpose this scheme could serve is to prevent third
	 * party clients from connecting to their servers.
	 *
	 * Sorry, Yahoo.
	 */

	PurpleCipher *cipher;
	PurpleCipherContext *context;
	guchar digest[16];

	char *crypt_result;
	char password_hash[25];
	char crypt_hash[25];
	char *hash_string_p = g_malloc(50 + strlen(name));
	char *hash_string_c = g_malloc(50 + strlen(name));

	char checksum;

	int sv;

	char result6[25];
	char result96[25];

	sv = seed[15];
	sv = sv % 8;

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (const guchar *)pass, strlen(pass));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);

	to_y64(password_hash, digest, 16);

	crypt_result = yahoo_crypt(pass, "$1$_2S43d5f$");

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)crypt_result, strlen(crypt_result));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	to_y64(crypt_hash, digest, 16);

	switch (sv) {
	case 1:
	case 6:
		checksum = seed[seed[9] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, seed, password_hash);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, seed, crypt_hash);
		break;
	case 2:
	case 7:
		checksum = seed[seed[15] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, seed, password_hash, name);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, seed, crypt_hash, name);
		break;
	case 3:
		checksum = seed[seed[1] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, password_hash, seed);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, name, crypt_hash, seed);
		break;
	case 4:
		checksum = seed[seed[3] % 16];
		g_snprintf(hash_string_p, strlen(name) + 50,
			   "%c%s%s%s", checksum, password_hash, seed, name);
		g_snprintf(hash_string_c, strlen(name) + 50,
			   "%c%s%s%s", checksum, crypt_hash, seed, name);
		break;
	case 0:
	case 5:
		checksum = seed[seed[7] % 16];
			g_snprintf(hash_string_p, strlen(name) + 50,
                                   "%c%s%s%s", checksum, password_hash, name, seed);
                        g_snprintf(hash_string_c, strlen(name) + 50,
				   "%c%s%s%s", checksum, crypt_hash, name, seed);
			break;
	}

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)hash_string_p, strlen(hash_string_p));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	to_y64(result6, digest, 16);

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)hash_string_c, strlen(hash_string_c));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	purple_cipher_context_destroy(context);
	to_y64(result96, digest, 16);

	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pack, "ssss", 0, name, 6, result6, 96, result96, 1, name);
	yahoo_packet_send_and_free(pack, yd);

	g_free(hash_string_p);
	g_free(hash_string_c);
}

/* I'm dishing out some uber-mad props to Cerulean Studios for cracking this
 * and sending the fix!  Thanks guys. */

static void yahoo_process_auth_new(PurpleConnection *gc, const char *seed)
{
	struct yahoo_packet *pack = NULL;
	PurpleAccount *account = purple_connection_get_account(gc);
	const char *name = purple_normalize(account, purple_account_get_username(account));
	const char *pass = purple_connection_get_password(gc);
	char *enc_pass;
	struct yahoo_data *yd = gc->proto_data;

	PurpleCipher			*md5_cipher;
	PurpleCipherContext	*md5_ctx;
	guchar				md5_digest[16];

	PurpleCipher			*sha1_cipher;
	PurpleCipherContext	*sha1_ctx1;
	PurpleCipherContext	*sha1_ctx2;

	char				*alphabet1			= "FBZDWAGHrJTLMNOPpRSKUVEXYChImkwQ";
	char				*alphabet2			= "F0E1D2C3B4A59687abcdefghijklmnop";

	char				*challenge_lookup	= "qzec2tb3um1olpar8whx4dfgijknsvy5";
	char				*operand_lookup		= "+|&%/*^-";
	char				*delimit_lookup		= ",;";

	char				*password_hash		= (char *)g_malloc(25);
	char				*crypt_hash		= (char *)g_malloc(25);
	char				*crypt_result		= NULL;

	unsigned char		pass_hash_xor1[64];
	unsigned char		pass_hash_xor2[64];
	unsigned char		crypt_hash_xor1[64];
	unsigned char		crypt_hash_xor2[64];
	char				resp_6[100];
	char				resp_96[100];

	unsigned char		digest1[20];
	unsigned char		digest2[20];
	unsigned char		comparison_src[20];
	unsigned char		magic_key_char[4];
	const char			*magic_ptr;

	unsigned int		magic[64];
	unsigned int		magic_work = 0;
	unsigned int		magic_4 = 0;

	int					x;
	int					y;
	int					cnt = 0;
	int					magic_cnt = 0;
	int					magic_len;

	memset(password_hash, 0, 25);
	memset(crypt_hash, 0, 25);
	memset(&pass_hash_xor1, 0, 64);
	memset(&pass_hash_xor2, 0, 64);
	memset(&crypt_hash_xor1, 0, 64);
	memset(&crypt_hash_xor2, 0, 64);
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);
	memset(&magic, 0, 64);
	memset(&resp_6, 0, 100);
	memset(&resp_96, 0, 100);
	memset(&magic_key_char, 0, 4);
	memset(&comparison_src, 0, 20);

	md5_cipher = purple_ciphers_find_cipher("md5");
	md5_ctx = purple_cipher_context_new(md5_cipher, NULL);

	sha1_cipher = purple_ciphers_find_cipher("sha1");
	sha1_ctx1 = purple_cipher_context_new(sha1_cipher, NULL);
	sha1_ctx2 = purple_cipher_context_new(sha1_cipher, NULL);

	/*
	 * Magic: Phase 1.  Generate what seems to be a 30 byte value (could change if base64
	 * ends up differently?  I don't remember and I'm tired, so use a 64 byte buffer.
	 */

	magic_ptr = seed;

	while (*magic_ptr != (int)NULL) {
		char   *loc;

		/* Ignore parentheses. */

		if (*magic_ptr == '(' || *magic_ptr == ')') {
			magic_ptr++;
			continue;
		}

		/* Characters and digits verify against the challenge lookup. */

		if (isalpha(*magic_ptr) || isdigit(*magic_ptr)) {
			loc = strchr(challenge_lookup, *magic_ptr);
			if (!loc) {
			  /* SME XXX Error - disconnect here */
			}

			/* Get offset into lookup table and shl 3. */

			magic_work = loc - challenge_lookup;
			magic_work <<= 3;

			magic_ptr++;
			continue;
		} else {
			unsigned int	local_store;

			loc = strchr(operand_lookup, *magic_ptr);
			if (!loc) {
				/* SME XXX Disconnect */
			}

			local_store = loc - operand_lookup;

			/* Oops; how did this happen? */

			if (magic_cnt >= 64)
				break;

			magic[magic_cnt++] = magic_work | local_store;
			magic_ptr++;
			continue;
		}
			}

	magic_len = magic_cnt;
	magic_cnt = 0;

	/* Magic: Phase 2.  Take generated magic value and sprinkle fairy
	 * dust on the values.
	 */

	for (magic_cnt = magic_len-2; magic_cnt >= 0; magic_cnt--) {
		unsigned char	byte1;
		unsigned char	byte2;

		/* Bad.  Abort. */

		if ((magic_cnt + 1 > magic_len) || (magic_cnt > magic_len))
			break;

		byte1 = magic[magic_cnt];
		byte2 = magic[magic_cnt+1];

		byte1 *= 0xcd;
		byte1 ^= byte2;

		magic[magic_cnt+1] = byte1;
			}

	/*
	 * Magic: Phase 3.  This computes 20 bytes.  The first 4 bytes are used as our magic
	 * key (and may be changed later); the next 16 bytes are an MD5 sum of the magic key
	 * plus 3 bytes.  The 3 bytes are found by looping, and they represent the offsets
	 * into particular functions we'll later call to potentially alter the magic key.
	 *
	 * %-)
	 */

	magic_cnt = 1;
	x = 0;

	do {
		unsigned int	bl = 0;
		unsigned int	cl = magic[magic_cnt++];

		if (magic_cnt >= magic_len)
			break;

		if (cl > 0x7F) {
			if (cl < 0xe0)
				bl = cl = (cl & 0x1f) << 6;
			else {
				bl = magic[magic_cnt++];
				cl = (cl & 0x0f) << 6;
				bl = ((bl & 0x3f) + cl) << 6;
			}

			cl = magic[magic_cnt++];
			bl = (cl & 0x3f) + bl;
		} else
			bl = cl;

		comparison_src[x++] = (bl & 0xff00) >> 8;
		comparison_src[x++] = bl & 0xff;
	} while (x < 20);

	/* First four bytes are magic key. */
	memcpy(&magic_key_char[0], comparison_src, 4);
	magic_4 = magic_key_char[0] | (magic_key_char[1]<<8) | (magic_key_char[2]<<16) | (magic_key_char[3]<<24);

	/*
	 * Magic: Phase 4.  Determine what function to use later by getting outside/inside
	 * loop values until we match our previous buffer.
	 */
	for (x = 0; x < 65535; x++) {
		int			leave = 0;

		for (y = 0; y < 5; y++) {
			unsigned char	test[3];

			/* Calculate buffer. */
			test[0] = x;
			test[1] = x >> 8;
			test[2] = y;

			purple_cipher_context_reset(md5_ctx, NULL);
			purple_cipher_context_append(md5_ctx, magic_key_char, 4);
			purple_cipher_context_append(md5_ctx, test, 3);
			purple_cipher_context_digest(md5_ctx, sizeof(md5_digest),
									   md5_digest, NULL);

			if (!memcmp(md5_digest, comparison_src+4, 16)) {
				leave = 1;
				break;
			}
		}

		if (leave == 1)
			break;
	}

	/* If y != 0, we need some help. */
	if (y != 0) {
		unsigned int	updated_key;

		/* Update magic stuff.
		 * Call it twice because Yahoo's encryption is super bad ass.
		 */
		updated_key = yahoo_auth_finalCountdown(magic_4, 0x60, y, x);
		updated_key = yahoo_auth_finalCountdown(updated_key, 0x60, y, x);

		magic_key_char[0] = updated_key & 0xff;
		magic_key_char[1] = (updated_key >> 8) & 0xff;
		magic_key_char[2] = (updated_key >> 16) & 0xff;
		magic_key_char[3] = (updated_key >> 24) & 0xff;
	}

	enc_pass = yahoo_string_encode(gc, pass, NULL);

	/* Get password and crypt hashes as per usual. */
	purple_cipher_context_reset(md5_ctx, NULL);
	purple_cipher_context_append(md5_ctx, (const guchar *)enc_pass, strlen(enc_pass));
	purple_cipher_context_digest(md5_ctx, sizeof(md5_digest),
							   md5_digest, NULL);
	to_y64(password_hash, md5_digest, 16);

	crypt_result = yahoo_crypt(enc_pass, "$1$_2S43d5f$");

	g_free(enc_pass);
	enc_pass = NULL;

	purple_cipher_context_reset(md5_ctx, NULL);
	purple_cipher_context_append(md5_ctx, (const guchar *)crypt_result, strlen(crypt_result));
	purple_cipher_context_digest(md5_ctx, sizeof(md5_digest),
							   md5_digest, NULL);
	to_y64(crypt_hash, md5_digest, 16);

	/* Our first authentication response is based off of the password hash. */
	for (x = 0; x < (int)strlen(password_hash); x++)
		pass_hash_xor1[cnt++] = password_hash[x] ^ 0x36;

	if (cnt < 64)
		memset(&(pass_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;

	for (x = 0; x < (int)strlen(password_hash); x++)
		pass_hash_xor2[cnt++] = password_hash[x] ^ 0x5c;

	if (cnt < 64)
		memset(&(pass_hash_xor2[cnt]), 0x5c, 64-cnt);

	/*
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */

	purple_cipher_context_append(sha1_ctx1, pass_hash_xor1, 64);
	if (y >= 3)
		purple_cipher_context_set_option(sha1_ctx1, "sizeLo", GINT_TO_POINTER(0x1ff));
	purple_cipher_context_append(sha1_ctx1, magic_key_char, 4);
	purple_cipher_context_digest(sha1_ctx1, sizeof(digest1), digest1, NULL);

	/*
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */

	purple_cipher_context_append(sha1_ctx2, pass_hash_xor2, 64);
	purple_cipher_context_append(sha1_ctx2, digest1, 20);
	purple_cipher_context_digest(sha1_ctx2, sizeof(digest2), digest2, NULL);

	/*
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */

	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;

		char			byte[6];

		memset(&byte, 0, 6);

		/* First two bytes of digest stuffed together. */

		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];

		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_6, byte);
		strcat(resp_6, "=");

		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);

		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_6, byte);

		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_6, byte);
	}

	/* Our second authentication response is based off of the crypto hash. */

	cnt = 0;
	memset(&digest1, 0, 20);
	memset(&digest2, 0, 20);

	for (x = 0; x < (int)strlen(crypt_hash); x++)
		crypt_hash_xor1[cnt++] = crypt_hash[x] ^ 0x36;

	if (cnt < 64)
		memset(&(crypt_hash_xor1[cnt]), 0x36, 64-cnt);

	cnt = 0;

	for (x = 0; x < (int)strlen(crypt_hash); x++)
		crypt_hash_xor2[cnt++] = crypt_hash[x] ^ 0x5c;

	if (cnt < 64)
		memset(&(crypt_hash_xor2[cnt]), 0x5c, 64-cnt);

	purple_cipher_context_reset(sha1_ctx1, NULL);
	purple_cipher_context_reset(sha1_ctx2, NULL);

	/*
	 * The first context gets the password hash XORed with 0x36 plus a magic value
	 * which we previously extrapolated from our challenge.
	 */

	purple_cipher_context_append(sha1_ctx1, crypt_hash_xor1, 64);
	if (y >= 3) {
		purple_cipher_context_set_option(sha1_ctx1, "sizeLo",
									   GINT_TO_POINTER(0x1ff));
	}
	purple_cipher_context_append(sha1_ctx1, magic_key_char, 4);
	purple_cipher_context_digest(sha1_ctx1, sizeof(digest1), digest1, NULL);

	/*
	 * The second context gets the password hash XORed with 0x5c plus the SHA-1 digest
	 * of the first context.
	 */

	purple_cipher_context_append(sha1_ctx2, crypt_hash_xor2, 64);
	purple_cipher_context_append(sha1_ctx2, digest1, 20);
	purple_cipher_context_digest(sha1_ctx2, sizeof(digest2), digest2, NULL);

	/*
	 * Now that we have digest2, use it to fetch characters from an alphabet to construct
	 * our first authentication response.
	 */

	for (x = 0; x < 20; x += 2) {
		unsigned int	val = 0;
		unsigned int	lookup = 0;

		char			byte[6];

		memset(&byte, 0, 6);

		/* First two bytes of digest stuffed together. */

		val = digest2[x];
		val <<= 8;
		val += digest2[x+1];

		lookup = (val >> 0x0b);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet1))
			break;
		sprintf(byte, "%c", alphabet1[lookup]);
		strcat(resp_96, byte);
		strcat(resp_96, "=");

		lookup = (val >> 0x06);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);

		lookup = (val >> 0x01);
		lookup &= 0x1f;
		if (lookup >= strlen(alphabet2))
			break;
		sprintf(byte, "%c", alphabet2[lookup]);
		strcat(resp_96, byte);

		lookup = (val & 0x01);
		if (lookup >= strlen(delimit_lookup))
			break;
		sprintf(byte, "%c", delimit_lookup[lookup]);
		strcat(resp_96, byte);
	}
	purple_debug_info("yahoo", "yahoo status: %d\n", yd->current_status);
	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP,	yd->current_status, 0);
	yahoo_packet_hash(pack, "sssss", 0, name, 6, resp_6, 96, resp_96, 1,
	                  name, 135, "6,0,0,1710");
	if (yd->picture_checksum)
		yahoo_packet_hash_int(pack, 192, yd->picture_checksum);

	yahoo_packet_send_and_free(pack, yd);

	purple_cipher_context_destroy(md5_ctx);
	purple_cipher_context_destroy(sha1_ctx1);
	purple_cipher_context_destroy(sha1_ctx2);

	g_free(password_hash);
	g_free(crypt_hash);
}

static void yahoo_process_auth(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *seed = NULL;
	char *sn   = NULL;
	GSList *l = pkt->hash;
	int m = 0;
	gchar *buf;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 94)
			seed = pair->value;
		if (pair->key == 1)
			sn = pair->value;
		if (pair->key == 13)
			m = atoi(pair->value);
		l = l->next;
	}

	if (seed) {
		switch (m) {
		case 0:
			yahoo_process_auth_old(gc, seed);
			break;
		case 1:
		case 2: /* This case seems to work, could probably use testing */
			yahoo_process_auth_new(gc, seed);
			break;
		default:
			buf = g_strdup_printf(_("The Yahoo server has requested the use of an unrecognized "
						"authentication method.  You will probably not be able "
						"to successfully sign on to Yahoo.  Check %s for updates."), PURPLE_WEBSITE);
			purple_notify_error(gc, "", _("Failed Yahoo! Authentication"),
					  buf);
			g_free(buf);
			yahoo_process_auth_new(gc, seed); /* Can't hurt to try it anyway. */
		}
	}
}

static void ignore_buddy(PurpleBuddy *buddy) {
	PurpleGroup *group;
	PurpleAccount *account;
	gchar *name;

	if (!buddy)
		return;

	group = purple_buddy_get_group(buddy);
	name = g_strdup(buddy->name);
	account = buddy->account;

	purple_debug(PURPLE_DEBUG_INFO, "blist",
		"Removing '%s' from buddy list.\n", buddy->name);
	purple_account_remove_buddy(account, buddy, group);
	purple_blist_remove_buddy(buddy);

	serv_add_deny(account->gc, name);

	g_free(name);
}

static void keep_buddy(PurpleBuddy *b) {
	purple_privacy_deny_remove(b->account, b->name, 1);
}

static void yahoo_process_ignore(PurpleConnection *gc, struct yahoo_packet *pkt) {
	PurpleBuddy *b;
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
		b = purple_find_buddy(gc->account, who);
		g_snprintf(buf, sizeof(buf), _("You have tried to ignore %s, but the "
					"user is on your buddy list.  Clicking \"Yes\" "
					"will remove and ignore the buddy."), who);
		purple_request_yes_no(gc, NULL, _("Ignore buddy?"), buf, 0, b,
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

static void yahoo_process_authresp(PurpleConnection *gc, struct yahoo_packet *pkt)
{
#ifdef TRY_WEBMESSENGER_LOGIN
	struct yahoo_data *yd = gc->proto_data;
#endif
	GSList *l = pkt->hash;
	int err = 0;
	char *msg;
	char *url = NULL;
	char *fullmsg;

	while (l) {
		struct yahoo_pair *pair = l->data;

		if (pair->key == 66)
			err = strtol(pair->value, NULL, 10);
		if (pair->key == 20)
			url = pair->value;

		l = l->next;
	}

	switch (err) {
	case 3:
		msg = g_strdup(_("Invalid screen name."));
		break;
	case 13:
#ifdef TRY_WEBMESSENGER_LOGIN
		if (!yd->wm) {
			PurpleUtilFetchUrlData *url_data;
			yd->wm = TRUE;
			if (yd->fd >= 0)
				close(yd->fd);
			if (gc->inpa)
				purple_input_remove(gc->inpa);
			url_data = purple_util_fetch_url(WEBMESSENGER_URL, TRUE,
					"Purple/" VERSION, FALSE, yahoo_login_page_cb, gc);
			if (url_data != NULL)
				yd->url_datas = g_slist_prepend(yd->url_datas, url_data);
			return;
		}
#endif
		msg = g_strdup(_("Incorrect password."));
		break;
	case 14:
		msg = g_strdup(_("Your account is locked, please log in to the Yahoo! website."));
		break;
	default:
		msg = g_strdup_printf(_("Unknown error number %d. Logging into the Yahoo! website may fix this."), err);
	}

	if (url)
		fullmsg = g_strdup_printf("%s\n%s", msg, url);
	else
		fullmsg = g_strdup(msg);

	gc->wants_to_die = TRUE;
	purple_connection_error(gc, fullmsg);
	g_free(msg);
	g_free(fullmsg);
}

static void yahoo_process_addbuddy(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	int err = 0;
	char *who = NULL;
	char *group = NULL;
	char *decoded_group;
	char *buf;
	YahooFriend *f;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 66:
			err = strtol(pair->value, NULL, 10);
			break;
		case 7:
			who = pair->value;
			break;
		case 65:
			group = pair->value;
			break;
		}

		l = l->next;
	}

	if (!who)
		return;
	if (!group)
		group = "";

	if (!err || (err == 2)) { /* 0 = ok, 2 = already on serv list */
		f = yahoo_friend_find_or_new(gc, who);
		yahoo_update_status(gc, who, f);
		return;
	}

	decoded_group = yahoo_string_decode(gc, group, FALSE);
	buf = g_strdup_printf(_("Could not add buddy %s to group %s to the server list on account %s."),
				who, decoded_group, purple_connection_get_display_name(gc));
	if (!purple_conv_present_error(who, purple_connection_get_account(gc), buf))
		purple_notify_error(gc, NULL, _("Could not add buddy to server list"), buf);
	g_free(buf);
	g_free(decoded_group);
}

static void yahoo_process_p2p(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *who = NULL;
	char *base64 = NULL;
	guchar *decoded;
	gsize len;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 5:
			/* our identity */
			break;
		case 4:
			who = pair->value;
			break;
		case 1:
			/* who again, the master identity this time? */
			break;
		case 12:
			base64 = pair->value;
			/* so, this is an ip address. in base64. decoded it's in ascii.
			   after strtol, it's in reversed byte order. Who thought this up?*/
			break;
		/*
			TODO: figure these out
			yahoo: Key: 61          Value: 0
			yahoo: Key: 2   Value:
			yahoo: Key: 13          Value: 0
			yahoo: Key: 49          Value: PEERTOPEER
			yahoo: Key: 140         Value: 1
			yahoo: Key: 11          Value: -1786225828
		*/

		}

		l = l->next;
	}

	if (base64) {
		guint32 ip;
		char *tmp2;
		YahooFriend *f;

		decoded = purple_base64_decode(base64, &len);
		if (len) {
			char *tmp = purple_str_binary_to_ascii(decoded, len);
			purple_debug_info("yahoo", "Got P2P service packet (from server): who = %s, ip = %s\n", who, tmp);
			g_free(tmp);
		}

		tmp2 = g_strndup((const gchar *)decoded, len); /* so its \0 terminated...*/
		ip = strtol(tmp2, NULL, 10);
		g_free(tmp2);
		g_free(decoded);
		tmp2 = g_strdup_printf("%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff,
		                       (ip >> 24) & 0xff);
		f = yahoo_friend_find(gc, who);
		if (f)
			yahoo_friend_set_ip(f, tmp2);
		g_free(tmp2);
	}
}

static void yahoo_process_audible(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *who = NULL, *msg = NULL, *id = NULL;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			who = pair->value;
			break;
		case 5:
			/* us */
			break;
		case 230:
			/* the audible, in foo.locale.bar.baz format
			   eg: base.tw.smiley.smiley43 */
			id = pair->value;
			break;
		case 231:
			/* the text of the audible */
			msg = pair->value;
			break;
		case 232:
			/* weird number (md5 hash?), like 8ebab9094156135f5dcbaccbeee662a5c5fd1420 */
			break;
		}

		l = l->next;
	}

	if (!msg)
		msg = id;
	if (!who || !msg)
		return;
	if (!g_utf8_validate(msg, -1, NULL)) {
		purple_debug_misc("yahoo", "Warning, nonutf8 audible, ignoring!\n");
		return;
	}
	if (!yahoo_privacy_check(gc, who)) {
		purple_debug_misc("yahoo", "Audible message from %s for %s dropped!\n",
		      gc->account->username, who);
		return;
	}
	if (id) {
		/* "http://us.dl1.yimg.com/download.yahoo.com/dl/aud/"+locale+"/"+id+".swf" */
		char **audible_locale = g_strsplit(id, ".", 0);
		char *buf = g_strdup_printf(_("[ Audible %s/%s/%s.swf ] %s"), YAHOO_AUDIBLE_URL, audible_locale[1], id, msg);
		g_strfreev(audible_locale);

		serv_got_im(gc, who, buf, 0, time(NULL));
		g_free(buf);
	} else
		serv_got_im(gc, who, msg, 0, time(NULL));
}

static void yahoo_packet_process(PurpleConnection *gc, struct yahoo_packet *pkt)
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
	case YAHOO_SERVICE_Y6_STATUS_UPDATE:
	case YAHOO_SERVICE_STATUS_15:
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
	case YAHOO_SERVICE_SYSMESSAGE:
		yahoo_process_sysmessage(gc, pkt);
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
	case YAHOO_SERVICE_LIST_15:
		yahoo_process_list_15(gc, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(gc, pkt);
		break;
	case YAHOO_SERVICE_ADDBUDDY:
		yahoo_process_addbuddy(gc, pkt);
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
	case YAHOO_SERVICE_PRESENCE_PERM:
	case YAHOO_SERVICE_PRESENCE_SESSION:
		yahoo_process_presence(gc, pkt);
		break;
	case YAHOO_SERVICE_P2PFILEXFER:
		/* This case had no break and continued; thus keeping it this way.*/
		yahoo_process_p2pfilexfer(gc, pkt);
	case YAHOO_SERVICE_FILETRANSFER:
		yahoo_process_filetransfer(gc, pkt);
		break;
	case YAHOO_SERVICE_PEERTOPEER:
		yahoo_process_p2p(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE:
		yahoo_process_picture(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPDATE:
		yahoo_process_picture_update(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_CHECKSUM:
		yahoo_process_picture_checksum(gc, pkt);
		break;
	case YAHOO_SERVICE_PICTURE_UPLOAD:
		yahoo_process_picture_upload(gc, pkt);
		break;
	case YAHOO_SERVICE_AVATAR_UPDATE:
		yahoo_process_avatar_update(gc, pkt);
		break;
	case YAHOO_SERVICE_AUDIBLE:
		yahoo_process_audible(gc, pkt);
		break;
	default:
		purple_debug(PURPLE_DEBUG_ERROR, "yahoo",
				   "Unhandled service 0x%02x\n", pkt->service);
		break;
	}
}

static void yahoo_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	struct yahoo_data *yd = gc->proto_data;
	char buf[1024];
	int len;

	len = read(yd->fd, buf, sizeof(buf));

	if (len < 0) {
		gchar *tmp;

		if (errno == EAGAIN)
			/* No worries */
			return;

		tmp = g_strdup_printf(_("Lost connection with server:\n%s"),
				strerror(errno));
		purple_connection_error(gc, tmp);
		g_free(tmp);
		return;
	} else if (len == 0) {
		purple_connection_error(gc, _("Server closed the connection."));
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

		if (strncmp((char *)yd->rxqueue, "YMSG", MIN(4, yd->rxlen)) != 0) {
			/* HEY! This isn't even a YMSG packet. What
			 * are you trying to pull? */
			guchar *start;

			purple_debug_warning("yahoo", "Error in YMSG stream, got something not a YMSG packet!");

			start = memchr(yd->rxqueue + 1, 'Y', yd->rxlen - 1);
			if (start) {
				g_memmove(yd->rxqueue, start, yd->rxlen - (start - yd->rxqueue));
				yd->rxlen -= start - yd->rxqueue;
				continue;
			} else {
				g_free(yd->rxqueue);
				yd->rxqueue = NULL;
				yd->rxlen = 0;
				return;
			}
		}

		pos += 4; /* YMSG */
		pos += 2;
		pos += 2;

		pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
		purple_debug(PURPLE_DEBUG_MISC, "yahoo",
				   "%d bytes to read, rxlen is %d\n", pktlen, yd->rxlen);

		if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen))
			return;

		yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

		pkt = yahoo_packet_new(0, 0, 0);

		pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
		pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
		purple_debug(PURPLE_DEBUG_MISC, "yahoo",
				   "Yahoo Service: 0x%02x Status: %d\n",
				   pkt->service, pkt->status);
		pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

		yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

		yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
		if (yd->rxlen) {
			guchar *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN + pktlen, yd->rxlen);
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

static void yahoo_got_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc = data;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		purple_connection_error(gc, _("Unable to connect."));
		return;
	}

	yd = gc->proto_data;
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, yd->current_status, 0);

	yahoo_packet_hash_str(pkt, 1, purple_normalize(gc->account, purple_account_get_username(purple_connection_get_account(gc))));
	yahoo_packet_send_and_free(pkt, yd);

	gc->inpa = purple_input_add(yd->fd, PURPLE_INPUT_READ, yahoo_pending, gc);
}

#ifdef TRY_WEBMESSENGER_LOGIN
static void yahoo_got_web_connected(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc = data;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;

	if (!PURPLE_CONNECTION_IS_VALID(gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		purple_connection_error(gc, _("Unable to connect."));
		return;
	}

	yd = gc->proto_data;
	yd->fd = source;

	pkt = yahoo_packet_new(YAHOO_SERVICE_WEBLOGIN, YAHOO_STATUS_WEBLOGIN, 0);

	yahoo_packet_hash(pkt, "sss", 0,
	                  purple_normalize(gc->account, purple_account_get_username(purple_connection_get_account(gc))),
	                  1, purple_normalize(gc->account, purple_account_get_username(purple_connection_get_account(gc))),
	                  6, yd->auth);
	yahoo_packet_send_and_free(pkt, yd);

	g_free(yd->auth);
	gc->inpa = purple_input_add(yd->fd, PURPLE_INPUT_READ, yahoo_pending, gc);
}

static void yahoo_web_pending(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc = data;
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	char bufread[2048], *i = bufread, *buf = bufread;
	int len;
	GString *s;

	len = read(source, bufread, sizeof(bufread) - 1);

	if (len < 0) {
		gchar *tmp;

		if (errno == EAGAIN)
			/* No worries */
			return;

		tmp = g_strdup_printf(_("Lost connection with server:\n%s"),
				strerror(errno));
		purple_connection_error(gc, tmp);
		g_free(tmp);
		return;
	} else if (len == 0) {
		purple_connection_error(gc, _("Server closed the connection."));
		return;
	}

	if (yd->rxlen > 0 || !g_strstr_len(buf, len, "\r\n\r\n")) {
		yd->rxqueue = g_realloc(yd->rxqueue, yd->rxlen + len + 1);
		memcpy(yd->rxqueue + yd->rxlen, buf, len);
		yd->rxlen += len;
		i = buf = (char *)yd->rxqueue;
		len = yd->rxlen;
	}
	buf[len] = '\0';

	if ((strncmp(buf, "HTTP/1.0 302", strlen("HTTP/1.0 302")) &&
			  strncmp(buf, "HTTP/1.1 302", strlen("HTTP/1.1 302")))) {
		purple_connection_error(gc, _("Received unexpected HTTP response from server."));
		return;
	}

	s = g_string_sized_new(len);

	while ((i = strstr(i, "Set-Cookie: "))) {
		i += strlen("Set-Cookie: ");
		for (;*i != ';' && *i != '\0'; i++)
			g_string_append_c(s, *i);

		g_string_append(s, "; ");
	}

	yd->auth = g_string_free(s, FALSE);
	purple_input_remove(gc->inpa);
	close(source);
	g_free(yd->rxqueue);
	yd->rxqueue = NULL;
	yd->rxlen = 0;
	/* Now we have our cookies to login with.  I'll go get the milk. */
	if (purple_proxy_connect(gc, account, "wcs2.msg.dcn.yahoo.com",
			       purple_account_get_int(account, "port", YAHOO_PAGER_PORT),
			       yahoo_got_web_connected, gc) == NULL) {
		purple_connection_error(gc, _("Connection problem"));
		return;
	}
}

static void yahoo_got_cookies_send_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleConnection *gc;
	struct yahoo_data *yd;
	int written, remaining;

	gc = data;
	yd = gc->proto_data;

	remaining = strlen(yd->auth) - yd->auth_written;
	written = write(source, yd->auth + yd->auth_written, remaining);

	if (written < 0 && errno == EAGAIN)
		written = 0;
	else if (written <= 0) {
		g_free(yd->auth);
		yd->auth = NULL;
		if (gc->inpa)
			purple_input_remove(gc->inpa);
		gc->inpa = 0;
		purple_connection_error(gc, _("Unable to connect."));
		return;
	}

	if (written < remaining) {
		yd->auth_written += written;
		return;
	}

	g_free(yd->auth);
	yd->auth = NULL;
	yd->auth_written = 0;
	purple_input_remove(gc->inpa);
	gc->inpa = purple_input_add(source, PURPLE_INPUT_READ, yahoo_web_pending, gc);
}

static void yahoo_got_cookies(gpointer data, gint source, const gchar *error_message)
{
	PurpleConnection *gc = data;

	if (source < 0) {
		purple_connection_error(gc, _("Unable to connect."));
		return;
	}

	if (gc->inpa == 0)
	{
		gc->inpa = purple_input_add(source, PURPLE_INPUT_WRITE,
			yahoo_got_cookies_send_cb, gc);
		yahoo_got_cookies_send_cb(gc, source, PURPLE_INPUT_WRITE);
	}
}

static void yahoo_login_page_hash_iter(const char *key, const char *val, GString *url)
{
	if (!strcmp(key, "passwd"))
		return;
	g_string_append_c(url, '&');
	g_string_append(url, key);
	g_string_append_c(url, '=');
	if (!strcmp(key, ".save") || !strcmp(key, ".js"))
		g_string_append_c(url, '1');
	else if (!strcmp(key, ".challenge"))
		g_string_append(url, val);
	else
		g_string_append(url, purple_url_encode(val));
}

static GHashTable *yahoo_login_page_hash(const char *buf, size_t len)
{
	GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	const char *c = buf;
	char *d;
	char name[64], value[64];
	int count;
	int input_len = strlen("<input ");
	int name_len = strlen("name=\"");
	int value_len = strlen("value=\"");
	while ((len > ((c - buf) + input_len))
			&& (c = strstr(c, "<input "))) {
		if (!(c = g_strstr_len(c, len - (c - buf), "name=\"")))
			continue;
		c += name_len;
		count = sizeof(name)-1;
		for (d = name; (len > ((c - buf) + 1)) && *c!='"'
				&& count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		count = sizeof(value)-1;
		if (!(d = g_strstr_len(c, len - (c - buf), "value=\"")))
			continue;
		d += value_len;
		if (strchr(c, '>') < d)
			break;
		for (c = d, d = value; (len > ((c - buf) + 1))
				&& *c!='"' && count; c++, d++, count--)
			*d = *c;
		*d = '\0';
		g_hash_table_insert(hash, g_strdup(name), g_strdup(value));
	}
	return hash;
}

static void
yahoo_login_page_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *url_text, size_t len, const gchar *error_message)
{
	PurpleConnection *gc = (PurpleConnection *)user_data;
	PurpleAccount *account = purple_connection_get_account(gc);
	struct yahoo_data *yd = gc->proto_data;
	const char *sn = purple_account_get_username(account);
	const char *pass = purple_connection_get_password(gc);
	GHashTable *hash = yahoo_login_page_hash(url_text, len);
	GString *url = g_string_new("GET http://login.yahoo.com/config/login?login=");
	char md5[33], *hashp = md5, *chal;
	int i;
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	guchar digest[16];

	yd->url_datas = g_slist_remove(yd->url_datas, url_data);

	if (error_message != NULL)
	{
		/* TODO: Include error_message in the message below */
		purple_connection_error(gc, _("Unable to connect."));
		return;
	}

	url = g_string_append(url, sn);
	url = g_string_append(url, "&passwd=");

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);

	purple_cipher_context_append(context, (const guchar *)pass, strlen(pass));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}

	chal = g_strconcat(md5, g_hash_table_lookup(hash, ".challenge"), NULL);
	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, (const guchar *)chal, strlen(chal));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}
	/*
	 * I dunno why this is here and commented out.. but in case it's needed
	 * I updated it..

	purple_cipher_context_reset(context, NULL);
	purple_cipher_context_append(context, md5, strlen(md5));
	purple_cipher_context_digest(context, sizeof(digest), digest, NULL);
	hashp = md5;
	for (i = 0; i < 16; ++i) {
		g_snprintf(hashp, 3, "%02x", digest[i]);
		hashp += 2;
	}
	*/
	g_free(chal);

	url = g_string_append(url, md5);
	g_hash_table_foreach(hash, (GHFunc)yahoo_login_page_hash_iter, url);

	url = g_string_append(url, "&.hash=1&.md5=1 HTTP/1.1\r\n"
			      "Host: login.yahoo.com\r\n\r\n");
	g_hash_table_destroy(hash);
	yd->auth = g_string_free(url, FALSE);
	if (purple_proxy_connect(gc, account, "login.yahoo.com", 80, yahoo_got_cookies, gc) == NULL) {
		purple_connection_error(gc, _("Connection problem"));
		return;
	}

	purple_cipher_context_destroy(context);
}
#endif

static void yahoo_server_check(PurpleAccount *account)
{
	const char *server;

	server = purple_account_get_string(account, "server", YAHOO_PAGER_HOST);

	if (strcmp(server, "scs.yahoo.com") == 0)
		purple_account_set_string(account, "server", YAHOO_PAGER_HOST);
}

static void yahoo_picture_check(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	char *buddyicon;

	buddyicon = purple_buddy_icons_get_full_path(purple_account_get_buddy_icon(account));
	yahoo_set_buddy_icon(gc, buddyicon);
	g_free(buddyicon);
}

static int get_yahoo_status_from_purple_status(PurpleStatus *status)
{
	PurplePresence *presence;
	const char *status_id;
	const char *msg;

	presence = purple_status_get_presence(status);
	status_id = purple_status_get_id(status);
	msg = purple_status_get_attr_string(status, "message");

	if (!strcmp(status_id, YAHOO_STATUS_TYPE_AVAILABLE)) {
		if ((msg != NULL) && (*msg != '\0'))
			return YAHOO_STATUS_CUSTOM;
		else
			return YAHOO_STATUS_AVAILABLE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_BRB)) {
		return YAHOO_STATUS_BRB;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_BUSY)) {
		return YAHOO_STATUS_BUSY;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTATHOME)) {
		return YAHOO_STATUS_NOTATHOME;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTATDESK)) {
		return YAHOO_STATUS_NOTATDESK;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_NOTINOFFICE)) {
		return YAHOO_STATUS_NOTINOFFICE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_ONPHONE)) {
		return YAHOO_STATUS_ONPHONE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_ONVACATION)) {
		return YAHOO_STATUS_ONVACATION;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_OUTTOLUNCH)) {
		return YAHOO_STATUS_OUTTOLUNCH;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_STEPPEDOUT)) {
		return YAHOO_STATUS_STEPPEDOUT;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_INVISIBLE)) {
		return YAHOO_STATUS_INVISIBLE;
	} else if (!strcmp(status_id, YAHOO_STATUS_TYPE_AWAY)) {
		return YAHOO_STATUS_CUSTOM;
	} else if (purple_presence_is_idle(presence)) {
		return YAHOO_STATUS_IDLE;
	} else {
		purple_debug_error("yahoo", "Unexpected PurpleStatus!\n");
		return YAHOO_STATUS_AVAILABLE;
	}
}

static void yahoo_login(PurpleAccount *account) {
	PurpleConnection *gc = purple_account_get_connection(account);
	struct yahoo_data *yd = gc->proto_data = g_new0(struct yahoo_data, 1);
	PurpleStatus *status = purple_account_get_active_status(account);
	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_NO_URLDESC;

	purple_connection_update_progress(gc, _("Connecting"), 1, 2);

	purple_connection_set_display_name(gc, purple_account_get_username(account));

	yd->fd = -1;
	yd->txhandler = -1;
	/* TODO: Is there a good grow size for the buffer? */
	yd->txbuf = purple_circ_buffer_new(0);
	yd->friends = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, yahoo_friend_free);
	yd->imvironments = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	yd->confs = NULL;
	yd->conf_id = 2;

	yd->current_status = get_yahoo_status_from_purple_status(status);

	yahoo_server_check(account);
	yahoo_picture_check(account);

	if (purple_account_get_bool(account, "yahoojp", FALSE)) {
		yd->jp = TRUE;
		if (purple_proxy_connect(gc, account,
		                       purple_account_get_string(account, "serverjp",  YAHOOJP_PAGER_HOST),
		                       purple_account_get_int(account, "port", YAHOO_PAGER_PORT),
		                       yahoo_got_connected, gc) == NULL)
		{
			purple_connection_error(gc, _("Connection problem"));
			return;
		}
	} else {
		yd->jp = FALSE;
		if (purple_proxy_connect(gc, account,
		                       purple_account_get_string(account, "server",  YAHOO_PAGER_HOST),
		                       purple_account_get_int(account, "port", YAHOO_PAGER_PORT),
		                       yahoo_got_connected, gc) == NULL)
		{
			purple_connection_error(gc, _("Connection problem"));
			return;
		}
	}
}

static void yahoo_close(PurpleConnection *gc) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	GSList *l;

	if (gc->inpa)
		purple_input_remove(gc->inpa);

	while (yd->url_datas) {
		purple_util_fetch_url_cancel(yd->url_datas->data);
		yd->url_datas = g_slist_delete_link(yd->url_datas, yd->url_datas);
	}

	for (l = yd->confs; l; l = l->next) {
		PurpleConversation *conv = l->data;

		yahoo_conf_leave(yd, purple_conversation_get_name(conv),
		                 purple_connection_get_display_name(gc),
				 purple_conv_chat_get_users(PURPLE_CONV_CHAT(conv)));
	}
	g_slist_free(yd->confs);

	yd->chat_online = 0;
	if (yd->in_chat)
		yahoo_c_leave(gc, 1); /* 1 = YAHOO_CHAT_ID */

	g_hash_table_destroy(yd->friends);
	g_hash_table_destroy(yd->imvironments);
	g_free(yd->chat_name);

	g_free(yd->cookie_y);
	g_free(yd->cookie_t);

	if (yd->txhandler)
		purple_input_remove(yd->txhandler);

	purple_circ_buffer_destroy(yd->txbuf);

	if (yd->fd >= 0)
		close(yd->fd);

	g_free(yd->rxqueue);
	yd->rxlen = 0;
	g_free(yd->picture_url);

	if (yd->buddy_icon_connect_data)
		purple_proxy_connect_cancel(yd->buddy_icon_connect_data);
	if (yd->picture_upload_todo)
		yahoo_buddy_icon_upload_data_free(yd->picture_upload_todo);
	if (yd->ycht)
		ycht_connection_close(yd->ycht);

	g_free(yd);
	gc->proto_data = NULL;
}

static const char *yahoo_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "yahoo";
}

static const char *yahoo_list_emblem(PurpleBuddy *b)
{
	PurpleAccount *account;
	PurpleConnection *gc;
	struct yahoo_data *yd;
	YahooFriend *f;
	PurplePresence *presence;

	if (!b || !(account = b->account) || !(gc = purple_account_get_connection(account)) ||
					     !(yd = gc->proto_data))
		return NULL;

	f = yahoo_friend_find(gc, b->name);
	if (!f) {
		return "not-authorized";
	}

	presence = purple_buddy_get_presence(b);

	if (purple_presence_is_online(presence)) {
		if (yahoo_friend_get_game(f))
			return "game";
		if (f->protocol == 2)
			return "msn";
	}
	return NULL;
}

static const char *yahoo_get_status_string(enum yahoo_status a)
{
	switch (a) {
	case YAHOO_STATUS_BRB:
		return _("Be Right Back");
	case YAHOO_STATUS_BUSY:
		return _("Busy");
	case YAHOO_STATUS_NOTATHOME:
		return _("Not at Home");
	case YAHOO_STATUS_NOTATDESK:
		return _("Not at Desk");
	case YAHOO_STATUS_NOTINOFFICE:
		return _("Not in Office");
	case YAHOO_STATUS_ONPHONE:
		return _("On the Phone");
	case YAHOO_STATUS_ONVACATION:
		return _("On Vacation");
	case YAHOO_STATUS_OUTTOLUNCH:
		return _("Out to Lunch");
	case YAHOO_STATUS_STEPPEDOUT:
		return _("Stepped Out");
	case YAHOO_STATUS_INVISIBLE:
		return _("Invisible");
	case YAHOO_STATUS_IDLE:
		return _("Idle");
	case YAHOO_STATUS_OFFLINE:
		return _("Offline");
	default:
		return _("Available");
	}
}

static void yahoo_initiate_conference(PurpleBlistNode *node, gpointer data) {

	PurpleBuddy *buddy;
	PurpleConnection *gc;

	GHashTable *components;
	struct yahoo_data *yd;
	int id;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	yd = gc->proto_data;
	id = yd->conf_id;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(components, g_strdup("room"),
		g_strdup_printf("%s-%d", purple_connection_get_display_name(gc), id));
	g_hash_table_replace(components, g_strdup("topic"), g_strdup("Join my conference..."));
	g_hash_table_replace(components, g_strdup("type"), g_strdup("Conference"));
	yahoo_c_join(gc, components);
	g_hash_table_destroy(components);

	yahoo_c_invite(gc, id, "Join my conference...", buddy->name);
}

static void yahoo_presence_settings(PurpleBlistNode *node, gpointer data) {
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	int presence_val = GPOINTER_TO_INT(data);

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);

	yahoo_friend_update_presence(gc, buddy->name, presence_val);
}

static void yahoo_game(PurpleBlistNode *node, gpointer data) {

	PurpleBuddy *buddy;
	PurpleConnection *gc;

	struct yahoo_data *yd;
	const char *game;
	char *game2;
	char *t;
	char url[256];
	YahooFriend *f;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	yd = (struct yahoo_data *) gc->proto_data;

	f = yahoo_friend_find(gc, buddy->name);
	if (!f)
		return;

	game = yahoo_friend_get_game(f);
	if (!game)
		return;

	t = game2 = g_strdup(strstr(game, "ante?room="));
	while (*t && *t != '\t')
		t++;
	*t = 0;
	g_snprintf(url, sizeof url, "http://games.yahoo.com/games/%s", game2);
	purple_notify_uri(gc, url);
	g_free(game2);
}

static char *yahoo_status_text(PurpleBuddy *b)
{
	YahooFriend *f = NULL;
	const char *msg;
	char *msg2;

	f = yahoo_friend_find(b->account->gc, b->name);
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
		if (!(msg = yahoo_friend_get_status_message(f)))
			return NULL;
		msg2 = g_markup_escape_text(msg, strlen(msg));
		purple_util_chrreplace(msg2, '\n', ' ');
		return msg2;

	default:
		return g_strdup(yahoo_get_status_string(f->status));
	}
}

void yahoo_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	YahooFriend *f;
	char *escaped;
	char *status = NULL;
	const char *presence = NULL;

	f = yahoo_friend_find(b->account->gc, b->name);
	if (!f)
		status = g_strdup_printf("\n%s", _("Not on server list"));
	else {
		switch (f->status) {
		case YAHOO_STATUS_CUSTOM:
			if (!yahoo_friend_get_status_message(f))
				return;
			status = g_strdup(yahoo_friend_get_status_message(f));
			break;
		case YAHOO_STATUS_OFFLINE:
			break;
		default:
			status = g_strdup(yahoo_get_status_string(f->status));
			break;
		}

		switch (f->presence) {
			case YAHOO_PRESENCE_ONLINE:
				presence = _("Appear Online");
				break;
			case YAHOO_PRESENCE_PERM_OFFLINE:
				presence = _("Appear Permanently Offline");
				break;
			case YAHOO_PRESENCE_DEFAULT:
				break;
			default:
				purple_debug_error("yahoo", "Unknown presence in yahoo_tooltip_text\n");
				break;
		}
	}

	if (status != NULL) {
		escaped = g_markup_escape_text(status, strlen(status));
		purple_notify_user_info_add_pair(user_info, _("Status"), escaped);
		g_free(status);
		g_free(escaped);
	}

	if (presence != NULL)
		purple_notify_user_info_add_pair(user_info, _("Presence"), presence);
}

static void yahoo_addbuddyfrommenu_cb(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);

	yahoo_add_buddy(gc, buddy, NULL);
}


static void yahoo_chat_goto_menu(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);

	yahoo_chat_goto(gc, buddy->name);
}

static GList *build_presence_submenu(YahooFriend *f, PurpleConnection *gc) {
	GList *m = NULL;
	PurpleMenuAction *act;
	struct yahoo_data *yd = (struct yahoo_data *) gc->proto_data;

	if (yd->current_status == YAHOO_STATUS_INVISIBLE) {
		if (f->presence != YAHOO_PRESENCE_ONLINE) {
			act = purple_menu_action_new(_("Appear Online"),
			                           PURPLE_CALLBACK(yahoo_presence_settings),
			                           GINT_TO_POINTER(YAHOO_PRESENCE_ONLINE),
			                           NULL);
			m = g_list_append(m, act);
		} else if (f->presence != YAHOO_PRESENCE_DEFAULT) {
			act = purple_menu_action_new(_("Appear Offline"),
			                           PURPLE_CALLBACK(yahoo_presence_settings),
			                           GINT_TO_POINTER(YAHOO_PRESENCE_DEFAULT),
			                           NULL);
			m = g_list_append(m, act);
		}
	}

	if (f->presence == YAHOO_PRESENCE_PERM_OFFLINE) {
		act = purple_menu_action_new(_("Don't Appear Permanently Offline"),
		                           PURPLE_CALLBACK(yahoo_presence_settings),
		                           GINT_TO_POINTER(YAHOO_PRESENCE_DEFAULT),
		                           NULL);
		m = g_list_append(m, act);
	} else {
		act = purple_menu_action_new(_("Appear Permanently Offline"),
		                           PURPLE_CALLBACK(yahoo_presence_settings),
		                           GINT_TO_POINTER(YAHOO_PRESENCE_PERM_OFFLINE),
		                           NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static void yahoo_doodle_blist_node(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *b = (PurpleBuddy *)node;
	PurpleConnection *gc = b->account->gc;

	yahoo_doodle_initiate(gc, b->name);
}

static GList *yahoo_buddy_menu(PurpleBuddy *buddy)
{
	GList *m = NULL;
	PurpleMenuAction *act;

	PurpleConnection *gc = purple_account_get_connection(buddy->account);
	struct yahoo_data *yd = gc->proto_data;
	static char buf2[1024];
	YahooFriend *f;

	f = yahoo_friend_find(gc, buddy->name);

	if (!f && !yd->wm) {
		act = purple_menu_action_new(_("Add Buddy"),
		                           PURPLE_CALLBACK(yahoo_addbuddyfrommenu_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);

		return m;

	}

	if (f && f->status != YAHOO_STATUS_OFFLINE) {
		if (!yd->wm) {
			act = purple_menu_action_new(_("Join in Chat"),
			                           PURPLE_CALLBACK(yahoo_chat_goto_menu),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}

		act = purple_menu_action_new(_("Initiate Conference"),
		                           PURPLE_CALLBACK(yahoo_initiate_conference),
		                           NULL, NULL);
		m = g_list_append(m, act);

		if (yahoo_friend_get_game(f)) {
			const char *game = yahoo_friend_get_game(f);
			char *room;
			char *t;

			if ((room = strstr(game, "&follow="))) {/* skip ahead to the url */
				while (*room && *room != '\t')          /* skip to the tab */
					room++;
				t = room++;                             /* room as now at the name */
				while (*t != '\n')
					t++;                            /* replace the \n with a space */
				*t = ' ';
				g_snprintf(buf2, sizeof buf2, "%s", room);

				act = purple_menu_action_new(buf2,
				                           PURPLE_CALLBACK(yahoo_game),
				                           NULL, NULL);
				m = g_list_append(m, act);
			}
		}
	}

	if (f) {
		act = purple_menu_action_new(_("Presence Settings"), NULL, NULL,
		                           build_presence_submenu(f, gc));
		m = g_list_append(m, act);
	}

	if (f) {
		act = purple_menu_action_new(_("Start Doodling"),
		                           PURPLE_CALLBACK(yahoo_doodle_blist_node),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *yahoo_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		return yahoo_buddy_menu((PurpleBuddy *) node);
	} else {
		return NULL;
	}
}

static void yahoo_act_id(PurpleConnection *gc, const char *entry)
{
	struct yahoo_data *yd = gc->proto_data;

	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_IDACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_str(pkt, 3, entry);
	yahoo_packet_send_and_free(pkt, yd);

	purple_connection_set_display_name(gc, entry);
}

static void yahoo_show_act_id(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	purple_request_input(gc, NULL, _("Active which ID?"), NULL,
					   purple_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(yahoo_act_id),
					   _("Cancel"), NULL, gc);
}

static void yahoo_show_chat_goto(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	purple_request_input(gc, NULL, _("Join who in chat?"), NULL,
					   "", FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(yahoo_chat_goto),
					   _("Cancel"), NULL, gc);
}

static GList *yahoo_actions(PurplePlugin *plugin, gpointer context) {
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Activate ID..."),
			yahoo_show_act_id);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Join User in Chat..."),
			yahoo_show_chat_goto);
	m = g_list_append(m, act);

	return m;
}

static int yahoo_send_im(PurpleConnection *gc, const char *who, const char *what, PurpleMessageFlags flags)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, 0);
	char *msg = yahoo_html_to_codes(what);
	char *msg2;
	gboolean utf8 = TRUE;
	PurpleWhiteboard *wb;
	int ret = 1;
	YahooFriend *f = NULL;

	msg2 = yahoo_string_encode(gc, msg, &utf8);

	yahoo_packet_hash(pkt, "ss", 1, purple_connection_get_display_name(gc), 5, who);
	if ((f = yahoo_friend_find(gc, who)) && f->protocol)
		yahoo_packet_hash_int(pkt, 241, f->protocol);

	if (utf8)
		yahoo_packet_hash_str(pkt, 97, "1");
	yahoo_packet_hash_str(pkt, 14, msg2);

	/*
	 * IMVironment.
	 *
	 * If this message is to a user who is also Doodling with the local user,
	 * format the chat packet with the correct IMV information (thanks Yahoo!)
	 *
	 * Otherwise attempt to use the same IMVironment as the remote user,
	 * just so that we don't inadvertantly reset their IMVironment back
	 * to nothing.
	 *
	 * If they have no set an IMVironment, then use the default.
	 */
	wb = purple_whiteboard_get_session(gc->account, who);
	if (wb)
		yahoo_packet_hash_str(pkt, 63, "doodle;11");
	else
	{
		const char *imv;
		imv = g_hash_table_lookup(yd->imvironments, who);
		if (imv != NULL)
			yahoo_packet_hash_str(pkt, 63, imv);
		else
			yahoo_packet_hash_str(pkt, 63, ";0");
	}

	yahoo_packet_hash_str(pkt,   64, "0"); /* no idea */
	yahoo_packet_hash_str(pkt, 1002, "1"); /* no idea, Yahoo 6 or later only it seems */
	if (!yd->picture_url)
		yahoo_packet_hash_str(pkt, 206, "0"); /* 0 = no picture, 2 = picture, maybe 1 = avatar? */
	else
		yahoo_packet_hash_str(pkt, 206, "2");

	/* We may need to not send any packets over 2000 bytes, but I'm not sure yet. */
	if ((YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt)) <= 2000)
		yahoo_packet_send(pkt, yd);
	else
		ret = -E2BIG;

	yahoo_packet_free(pkt);

	g_free(msg);
	g_free(msg2);

	return ret;
}

static unsigned int yahoo_send_typing(PurpleConnection *gc, const char *who, PurpleTypingState state)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, 0);
	yahoo_packet_hash(pkt, "ssssss", 49, "TYPING", 1, purple_connection_get_display_name(gc),
	                  14, " ", 13, state == PURPLE_TYPING ? "1" : "0",
	                  5, who, 1002, "1");

	yahoo_packet_send_and_free(pkt, yd);

	return 0;
}

static void yahoo_session_presence_remove(gpointer key, gpointer value, gpointer data)
{
	YahooFriend *f = value;
	if (f && f->presence == YAHOO_PRESENCE_ONLINE)
		f->presence = YAHOO_PRESENCE_DEFAULT;
}

static void yahoo_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	PurplePresence *presence;
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;
	int old_status;
	const char *msg = NULL;
	char *tmp = NULL;
	char *conv_msg = NULL;

	if (!purple_status_is_active(status))
		return;

	gc = purple_account_get_connection(account);
	presence = purple_status_get_presence(status);
	yd = (struct yahoo_data *)gc->proto_data;
	old_status = yd->current_status;

	yd->current_status = get_yahoo_status_from_purple_status(status);

	if (yd->current_status == YAHOO_STATUS_CUSTOM)
	{
		msg = purple_status_get_attr_string(status, "message");

		if (purple_status_is_available(status)) {
			tmp = yahoo_string_encode(gc, msg, NULL);
			conv_msg = purple_markup_strip_html(tmp);
			g_free(tmp);
		} else {
			if ((msg == NULL) || (*msg == '\0'))
				msg = _("Away");
			tmp = yahoo_string_encode(gc, msg, NULL);
			conv_msg = purple_markup_strip_html(tmp);
			g_free(tmp);
		}
	}

	if (yd->current_status == YAHOO_STATUS_INVISIBLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_VISIBLE_TOGGLE, YAHOO_STATUS_AVAILABLE, 0);
		yahoo_packet_hash_str(pkt, 13, "2");
		yahoo_packet_send_and_free(pkt, yd);

		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_STATUS_UPDATE, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_int(pkt, 10, yd->current_status);

	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		yahoo_packet_hash_str(pkt, 19, conv_msg);
	} else {
		yahoo_packet_hash_str(pkt, 19, "");
	}

	g_free(conv_msg);

	if (purple_presence_is_idle(presence))
		yahoo_packet_hash_str(pkt, 47, "2");
	else if (!purple_status_is_available(status))
		yahoo_packet_hash_str(pkt, 47, "1");

	yahoo_packet_send_and_free(pkt, yd);

	if (old_status == YAHOO_STATUS_INVISIBLE) {
		pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_VISIBLE_TOGGLE, YAHOO_STATUS_AVAILABLE, 0);
		yahoo_packet_hash_str(pkt, 13, "1");
		yahoo_packet_send_and_free(pkt, yd);

		/* Any per-session presence settings are removed */
		g_hash_table_foreach(yd->friends, yahoo_session_presence_remove, NULL);

	}
}

static void yahoo_set_idle(PurpleConnection *gc, int idle)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = NULL;
	char *msg = NULL, *msg2 = NULL;
	PurpleStatus *status = NULL;

	if (idle && yd->current_status != YAHOO_STATUS_CUSTOM)
		yd->current_status = YAHOO_STATUS_IDLE;
	else if (!idle && yd->current_status == YAHOO_STATUS_IDLE) {
		status = purple_presence_get_active_status(purple_account_get_presence(purple_connection_get_account(gc)));
		yd->current_status = get_yahoo_status_from_purple_status(status);
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_Y6_STATUS_UPDATE, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash_int(pkt, 10, yd->current_status);
	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		const char *tmp;
		if (status == NULL)
			status = purple_presence_get_active_status(purple_account_get_presence(purple_connection_get_account(gc)));
		tmp = purple_status_get_attr_string(status, "message");
		if (tmp != NULL) {
			msg = yahoo_string_encode(gc, tmp, NULL);
			msg2 = purple_markup_strip_html(msg);
			yahoo_packet_hash_str(pkt, 19, msg2);
		} else {
			/* get_yahoo_status_from_purple_status() returns YAHOO_STATUS_CUSTOM for
			 * the generic away state (YAHOO_STATUS_TYPE_AWAY) with no message */
			yahoo_packet_hash_str(pkt, 19, _("Away"));
		}
	} else {
		yahoo_packet_hash_str(pkt, 19, "");
	}

	if (idle)
		yahoo_packet_hash_str(pkt, 47, "2");
	else if (!purple_presence_is_available(purple_account_get_presence(purple_connection_get_account(gc))))
		yahoo_packet_hash_str(pkt, 47, "1");

	yahoo_packet_send_and_free(pkt, yd);

	g_free(msg);
	g_free(msg2);
}

static GList *yahoo_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, YAHOO_STATUS_TYPE_AVAILABLE,
	                                       NULL, TRUE, TRUE, FALSE,
	                                       "message", _("Message"),
	                                       purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_AWAY,
	                                       NULL, TRUE, TRUE, FALSE,
	                                       "message", _("Message"),
	                                       purple_value_new(PURPLE_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_BRB, _("Be Right Back"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_UNAVAILABLE, YAHOO_STATUS_TYPE_BUSY, _("Busy"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTATHOME, _("Not at Home"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTATDESK, _("Not at Desk"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_NOTINOFFICE, _("Not in Office"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_UNAVAILABLE, YAHOO_STATUS_TYPE_ONPHONE, _("On the Phone"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_EXTENDED_AWAY, YAHOO_STATUS_TYPE_ONVACATION, _("On Vacation"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_OUTTOLUNCH, _("Out to Lunch"), TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_AWAY, YAHOO_STATUS_TYPE_STEPPEDOUT, _("Stepped Out"), TRUE);
	types = g_list_append(types, type);


	type = purple_status_type_new(PURPLE_STATUS_INVISIBLE, YAHOO_STATUS_TYPE_INVISIBLE, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_OFFLINE, YAHOO_STATUS_TYPE_OFFLINE, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new_full(PURPLE_STATUS_MOBILE, YAHOO_STATUS_TYPE_MOBILE, NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, type);

	return types;
}

static void yahoo_keepalive(PurpleConnection *gc)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_send_and_free(pkt, yd);

	if (!yd->chat_online)
		return;

	if (yd->wm) {
		ycht_chat_send_keepalive(yd->ycht);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_CHATPING, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash_str(pkt, 109, purple_connection_get_display_name(gc));
	yahoo_packet_send_and_free(pkt, yd);
}

/* XXX - What's the deal with PurpleGroup *foo? */
static void yahoo_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *foo)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;
	PurpleGroup *g;
	char *group = NULL;
	char *group2 = NULL;

	if (!yd->logged_in)
		return;

	if (!yahoo_privacy_check(gc, purple_buddy_get_name(buddy)))
		return;

	if (foo)
		group = foo->name;
	if (!group) {
		g = purple_buddy_get_group(buddy);
		if (g)
			group = g->name;
		else
			group = "Buddies";
	}

	group2 = yahoo_string_encode(gc, group, NULL);
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssss", 1, purple_connection_get_display_name(gc),
	                  7, buddy->name, 65, group2, 14, "");
	yahoo_packet_send_and_free(pkt, yd);
	g_free(group2);
}

static void yahoo_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
        struct yahoo_packet *pkt;
	GSList *buddies, *l;
	PurpleGroup *g;
	gboolean remove = TRUE;
	char *cg;

	if (!(yahoo_friend_find(gc, buddy->name)))
		return;

	buddies = purple_find_buddies(purple_connection_get_account(gc), buddy->name);
	for (l = buddies; l; l = l->next) {
		g = purple_buddy_get_group(l->data);
		if (purple_utf8_strcasecmp(group->name, g->name)) {
			remove = FALSE;
			break;
		}
	}

	g_slist_free(buddies);

	if (remove)
		g_hash_table_remove(yd->friends, buddy->name);

	cg = yahoo_string_encode(gc, group->name, NULL);
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc),
	                  7, buddy->name, 65, cg);
	yahoo_packet_send_and_free(pkt, yd);
	g_free(cg);
}

static void yahoo_add_deny(PurpleConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;
	/* It seems to work better without this */

	/* if (gc->account->perm_deny != 4)
		return; */

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc),
	                  7, who, 13, "1");
	yahoo_packet_send_and_free(pkt, yd);
}

static void yahoo_rem_deny(PurpleConnection *gc, const char *who) {
	struct yahoo_data *yd = (struct yahoo_data *)gc->proto_data;
	struct yahoo_packet *pkt;

	if (!yd->logged_in)
		return;

	if (!who || who[0] == '\0')
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_IGNORECONTACT, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc), 7, who, 13, "2");
	yahoo_packet_send_and_free(pkt, yd);
}

static void yahoo_set_permit_deny(PurpleConnection *gc) {
	PurpleAccount *acct;
	GSList *deny;

	acct = gc->account;

	switch (acct->perm_deny) {
		/* privacy 1 */
		case PURPLE_PRIVACY_ALLOW_ALL:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;
		/* privacy 3 */
		case PURPLE_PRIVACY_ALLOW_USERS:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_rem_deny(gc, deny->data);
			break;
		/* privacy 5 */
		case PURPLE_PRIVACY_ALLOW_BUDDYLIST:
		/* privacy 4 */
		case PURPLE_PRIVACY_DENY_USERS:
			for (deny = acct->deny;deny;deny = deny->next)
				yahoo_add_deny(gc, deny->data);
			break;
		/* privacy 2 */
		case PURPLE_PRIVACY_DENY_ALL:
		default:
			break;
	}
}

static gboolean yahoo_unload_plugin(PurplePlugin *plugin)
{
	yahoo_dest_colorht();

	return TRUE;
}

static void yahoo_change_buddys_group(PurpleConnection *gc, const char *who,
				   const char *old_group, const char *new_group)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *gpn, *gpo;

	/* Step 0:  If they aren't on the server list anyway,
	 *          don't bother letting the server know.
	 */
	if (!yahoo_friend_find(gc, who))
		return;

	/* If old and new are the same, we would probably
	 * end up deleting the buddy, which would be bad.
	 * This might happen because of the charset conversation.
	 */
	gpn = yahoo_string_encode(gc, new_group, NULL);
	gpo = yahoo_string_encode(gc, old_group, NULL);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	/* Step 1:  Add buddy to new group. */
	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "ssss", 1, purple_connection_get_display_name(gc),
	                  7, who, 65, gpn, 14, "");
	yahoo_packet_send_and_free(pkt, yd);

	/* Step 2:  Remove buddy from old group */
	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc), 7, who, 65, gpo);
	yahoo_packet_send_and_free(pkt, yd);
	g_free(gpn);
	g_free(gpo);
}

static void yahoo_rename_group(PurpleConnection *gc, const char *old_name,
							   PurpleGroup *group, GList *moved_buddies)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt;
	char *gpn, *gpo;

	gpn = yahoo_string_encode(gc, group->name, NULL);
	gpo = yahoo_string_encode(gc, old_name, NULL);
	if (!strcmp(gpn, gpo)) {
		g_free(gpn);
		g_free(gpo);
		return;
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_GROUPRENAME, YAHOO_STATUS_AVAILABLE, 0);
	yahoo_packet_hash(pkt, "sss", 1, purple_connection_get_display_name(gc),
	                  65, gpo, 67, gpn);
	yahoo_packet_send_and_free(pkt, yd);
	g_free(gpn);
	g_free(gpo);
}

/********************************* Commands **********************************/

static PurpleCmdRet
yahoopurple_cmd_buzz(PurpleConversation *c, const gchar *cmd, gchar **args, gchar **error, void *data) {

	PurpleAccount *account = purple_conversation_get_account(c);
	const char *username = purple_account_get_username(account);

	if (*args && args[0])
		return PURPLE_CMD_RET_FAILED;

	purple_debug(PURPLE_DEBUG_INFO, "yahoo",
	           "Sending <ding> on account %s to buddy %s.\n", username, c->name);
	purple_conv_im_send(PURPLE_CONV_IM(c), "<ding>");
	purple_conversation_write(c, NULL, _("You have just sent a Buzz!"), PURPLE_MESSAGE_SYSTEM, time(NULL));
	return PURPLE_CMD_RET_OK;
}

static PurplePlugin *my_protocol = NULL;

static PurpleCmdRet
yahoopurple_cmd_chat_join(PurpleConversation *conv, const char *cmd,
                        char **args, char **error, void *data)
{
	GHashTable *comp;
	PurpleConnection *gc;
	struct yahoo_data *yd;
	int id;

	if (!args || !args[0])
		return PURPLE_CMD_RET_FAILED;

	gc = purple_conversation_get_gc(conv);
	yd = gc->proto_data;
	id = yd->conf_id;
	purple_debug(PURPLE_DEBUG_INFO, "yahoo",
	           "Trying to join %s \n", args[0]);

	comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_replace(comp, g_strdup("room"),
	g_strdup_printf("%s", g_ascii_strdown(args[0], strlen(args[0]))));
	g_hash_table_replace(comp, g_strdup("type"), g_strdup("Chat"));

	yahoo_c_join(gc, comp);

	g_hash_table_destroy(comp);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet
yahoopurple_cmd_chat_list(PurpleConversation *conv, const char *cmd,
                        char **args, char **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	if (*args && args[0])
		return PURPLE_CMD_RET_FAILED;
	purple_roomlist_show_with_account(account);
	return PURPLE_CMD_RET_OK;
}

static gboolean yahoo_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

/************************** Plugin Initialization ****************************/
static void
yahoopurple_register_commands(void)
{
	purple_cmd_register("join", "s", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_chat_join,
	                  _("join &lt;room&gt;:  Join a chat room on the Yahoo network"), NULL);
	purple_cmd_register("list", "", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT |
	                  PURPLE_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_chat_list,
	                  _("list: List rooms on the Yahoo network"), NULL);
	purple_cmd_register("buzz", "", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoopurple_cmd_buzz,
	                  _("buzz: Buzz a user to get their attention"), NULL);
	purple_cmd_register("doodle", "", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY,
	                  "prpl-yahoo", yahoo_doodle_purple_cmd_start,
	                 _("doodle: Request user to start a Doodle session"), NULL);
}

static PurpleAccount *find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

/* This may not be the best way to do this, but we find the first key w/o a value
 * and assume it is the screenname */
static void yahoo_find_uri_novalue_param(gpointer key, gpointer value, gpointer user_data)
{
	char **retval = user_data;

	if (value == NULL && *retval == NULL) {
		*retval = key;
	}
}

static gboolean yahoo_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	char *acct_id = g_hash_table_lookup(params, "account");
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "ymsgr"))
		return FALSE;

	acct = find_acct(purple_plugin_get_id(my_protocol), acct_id);

	if (!acct)
		return FALSE;

	/* ymsgr:SendIM?screename&m=The+Message */
	if (!g_ascii_strcasecmp(cmd, "SendIM")) {
		char *sname = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &sname);
		if (sname) {
			char *message = g_hash_table_lookup(params, "m");

			PurpleConversation *conv = purple_find_conversation_with_account(
				PURPLE_CONV_TYPE_IM, sname, acct);
			if (conv == NULL)
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, acct, sname);
			purple_conversation_present(conv);

			if (message) {
				/* Spaces are encoded as '+' */
				g_strdelimit(message, "+", ' ');
				purple_conv_im_send(PURPLE_CONV_IM(conv), message);
			}
		}
		/*else
			**If pidgindialogs_im() was in the core, we could use it here.
			 * It is all purple_request_* based, but I'm not sure it really belongs in the core
			pidgindialogs_im();*/

		return TRUE;
	}
	/* ymsgr:Chat?roomname */
	else if (!g_ascii_strcasecmp(cmd, "Chat")) {
		char *rname = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &rname);
		if (rname) {
			/* This is somewhat hacky, but the params aren't useful after this command */
			g_hash_table_insert(params, g_strdup("room"), g_strdup(rname));
			g_hash_table_insert(params, g_strdup("type"), g_strdup("Chat"));
			serv_join_chat(purple_account_get_connection(acct), params);
		}
		/*else
			** Same as above (except that this would have to be re-written using purple_request_*)
			pidgin_blist_joinchat_show(); */

		return TRUE;
	}
	/* ymsgr:AddFriend?name */
	else if (!g_ascii_strcasecmp(cmd, "AddFriend")) {
		char *name = NULL;
		g_hash_table_foreach(params, yahoo_find_uri_novalue_param, &name);
		purple_blist_request_add_buddy(acct, name, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}

static PurpleWhiteboardPrplOps yahoo_whiteboard_prpl_ops =
{
	yahoo_doodle_start,
	yahoo_doodle_end,
	yahoo_doodle_get_dimensions,
	NULL,
	yahoo_doodle_get_brush,
	yahoo_doodle_set_brush,
	yahoo_doodle_send_draw_list,
	yahoo_doodle_clear
};

static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_CHAT_TOPIC,
	NULL, /* user_splits */
	NULL, /* protocol_options */
	{"png,gif,jpeg", 96, 96, 96, 96, 0, PURPLE_ICON_SCALE_SEND},
	yahoo_list_icon,
	yahoo_list_emblem,
	yahoo_status_text,
	yahoo_tooltip_text,
	yahoo_status_types,
	yahoo_blist_node_menu,
	yahoo_c_info,
	yahoo_c_info_defaults,
	yahoo_login,
	yahoo_close,
	yahoo_send_im,
	NULL, /* set info */
	yahoo_send_typing,
	yahoo_get_info,
	yahoo_set_status,
	yahoo_set_idle,
	NULL, /* change_passwd*/
	yahoo_add_buddy,
	NULL, /* add_buddies */
	yahoo_remove_buddy,
	NULL, /*remove_buddies */
	yahoo_add_permit,
	yahoo_add_deny,
	yahoo_rem_permit,
	yahoo_rem_deny,
	yahoo_set_permit_deny,
	yahoo_c_join,
	NULL, /* reject chat invite */
	yahoo_get_chat_name,
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
	purple_normalize_nocase, /* normalize */
	yahoo_set_buddy_icon,
	NULL, /* void (*remove_group)(PurpleConnection *gc, const char *group);*/
	NULL, /* char *(*get_cb_real_name)(PurpleConnection *gc, int id, const char *who); */
	NULL, /* set_chat_topic */
	NULL, /* find_blist_chat */
	yahoo_roomlist_get_list,
	yahoo_roomlist_cancel,
	yahoo_roomlist_expand_category,
	NULL, /* can_receive_file */
	yahoo_send_file,
	yahoo_new_xfer,
	yahoo_offline_message, /* offline_message */
	&yahoo_whiteboard_prpl_ops,
	NULL, /* send_raw */
	NULL, /* roomlist_room_serialize */
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */
	"prpl-yahoo",                                     /**< id             */
	"Yahoo",	                                      /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Yahoo Protocol Plugin"),
	                                                  /**  description    */
	N_("Yahoo Protocol Plugin"),
	NULL,                                             /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */
	NULL,                                             /**< load           */
	yahoo_unload_plugin,                              /**< unload         */
	NULL,                                             /**< destroy        */
	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	yahoo_actions
};

static void
init_plugin(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

	option = purple_account_option_bool_new(_("Yahoo Japan"), "yahoojp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Pager server"), "server", YAHOO_PAGER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Japan Pager server"), "serverjp", YAHOOJP_PAGER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Pager port"), "port", YAHOO_PAGER_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("File transfer server"), "xfer_host", YAHOO_XFER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Japan file transfer server"), "xferjp_host", YAHOOJP_XFER_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("File transfer port"), "xfer_port", YAHOO_XFER_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Chat room locale"), "room_list_locale", YAHOO_ROOMLIST_LOCALE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Ignore conference and chatroom invitations"), "ignore_invites", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Encoding"), "local_charset", "ISO-8859-1");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);


#if 0
	option = purple_account_option_string_new(_("Chat room list URL"), "room_list", YAHOO_ROOMLIST_URL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Yahoo Chat server"), "ycht-server", YAHOO_YCHT_HOST);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Yahoo Chat port"), "ycht-port", YAHOO_YCHT_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif

	my_protocol = plugin;
	yahoopurple_register_commands();
	yahoo_init_colorht();

	purple_signal_connect(purple_get_core(), "uri-handler", plugin,
		PURPLE_CALLBACK(yahoo_uri_handler), NULL);
}

PURPLE_INIT_PLUGIN(yahoo, init_plugin, info);

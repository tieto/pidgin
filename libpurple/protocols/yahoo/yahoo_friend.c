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
#include "prpl.h"
#include "util.h"
#include "debug.h"

#include "yahoo_friend.h"

static YahooFriend *yahoo_friend_new(void)
{
	YahooFriend *ret;

	ret = g_new0(YahooFriend, 1);
	ret->status = YAHOO_STATUS_OFFLINE;
	ret->presence = YAHOO_PRESENCE_DEFAULT;

	return ret;
}

YahooFriend *yahoo_friend_find(PurpleConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	const char *norm;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(gc->proto_data != NULL, NULL);

	yd = gc->proto_data;
	norm = purple_normalize(purple_connection_get_account(gc), name);

	return g_hash_table_lookup(yd->friends, norm);
}

YahooFriend *yahoo_friend_find_or_new(PurpleConnection *gc, const char *name)
{
	YahooFriend *f;
	struct yahoo_data *yd;
	const char *norm;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(gc->proto_data != NULL, NULL);

	yd = gc->proto_data;
	norm = purple_normalize(purple_connection_get_account(gc), name);

	f = g_hash_table_lookup(yd->friends, norm);
	if (!f) {
		f = yahoo_friend_new();
		g_hash_table_insert(yd->friends, g_strdup(norm), f);
	}

	return f;
}

void yahoo_friend_set_ip(YahooFriend *f, const char *ip)
{
	g_free(f->ip);
	f->ip = g_strdup(ip);
}

const char *yahoo_friend_get_ip(YahooFriend *f)
{
	return f->ip;
}

void yahoo_friend_set_game(YahooFriend *f, const char *game)
{
	g_free(f->game);

	if (game)
		f->game = g_strdup(game);
	else
		f->game = NULL;
}

const char *yahoo_friend_get_game(YahooFriend *f)
{
	return f->game;
}

void yahoo_friend_set_status_message(YahooFriend *f, char *msg)
{
	g_free(f->msg);

	f->msg = msg;
}

const char *yahoo_friend_get_status_message(YahooFriend *f)
{
	return f->msg;
}

void yahoo_friend_set_buddy_icon_need_request(YahooFriend *f, gboolean needs)
{
	f->bicon_sent_request = !needs;
}

gboolean yahoo_friend_get_buddy_icon_need_request(YahooFriend *f)
{
	return !f->bicon_sent_request;
}

void yahoo_friend_set_alias_id(YahooFriend *f, const char *alias_id)
{
	g_free(f->alias_id);
	f->alias_id = g_strdup(alias_id);
}

const char *yahoo_friend_get_alias_id(YahooFriend *f)
{
	return f->alias_id;
}

void yahoo_friend_free(gpointer p)
{
	YahooFriend *f = p;
	g_free(f->msg);
	g_free(f->game);
	g_free(f->ip);
	g_free(f->alias_id);
	g_free(f);
}

void yahoo_process_presence(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	YahooFriend *f;
	char *who = NULL;
	int value = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
			case 7:
				who = pair->value;
				break;
			case 31:
				value = strtol(pair->value, NULL, 10);
				break;
		}

		l = l->next;
	}

	if (value != 1 && value != 2) {
		purple_debug_error("yahoo", "Received unknown value for presence key: %d\n", value);
		return;
	}

	g_return_if_fail(who != NULL);

	f = yahoo_friend_find(gc, who);
	if (!f)
		return;

	if (pkt->service == YAHOO_SERVICE_PRESENCE_PERM) {
		purple_debug_info("yahoo", "Setting permanent presence for %s to %d.\n", who, (value == 1));
		/* If setting from perm offline to online when in invisible status,
		 * this has already been taken care of (when the temp status changed) */
		if (value == 2 && f->presence == YAHOO_PRESENCE_ONLINE) {
		} else {
			if (value == 1) /* Setting Perm offline */
				f->presence = YAHOO_PRESENCE_PERM_OFFLINE;
			else
				f->presence = YAHOO_PRESENCE_DEFAULT;
		}
	} else {
		purple_debug_info("yahoo", "Setting session presence for %s to %d.\n", who, (value == 1));
		if (value == 1)
			f->presence = YAHOO_PRESENCE_ONLINE;
		else
			f->presence = YAHOO_PRESENCE_DEFAULT;
	}
}

void yahoo_friend_update_presence(PurpleConnection *gc, const char *name,
		YahooPresenceVisibility presence)
{
	struct yahoo_data *yd = gc->proto_data;
	struct yahoo_packet *pkt = NULL;
	YahooFriend *f;
	const char *thirtyone, *thirteen;
	int service = -1;

	if (!yd->logged_in)
		return;

	f = yahoo_friend_find(gc, name);
	if (!f)
		return;

	/* No need to change the value if it is already correct */
	if (f->presence == presence) {
		purple_debug_info("yahoo", "Not setting presence because there are no changes.\n");
		return;
	}

	if (presence == YAHOO_PRESENCE_PERM_OFFLINE) {
		service = YAHOO_SERVICE_PRESENCE_PERM;
		thirtyone = "1";
		thirteen = "2";
	} else if (presence == YAHOO_PRESENCE_DEFAULT) {
		if (f->presence == YAHOO_PRESENCE_PERM_OFFLINE) {
			service = YAHOO_SERVICE_PRESENCE_PERM;
			thirtyone = "2";
			thirteen = "2";
		} else if (yd->current_status == YAHOO_STATUS_INVISIBLE) {
			service = YAHOO_SERVICE_PRESENCE_SESSION;
			thirtyone = "2";
			thirteen = "1";
		}
	} else if (presence == YAHOO_PRESENCE_ONLINE) {
		if (f->presence == YAHOO_PRESENCE_PERM_OFFLINE) {
			pkt = yahoo_packet_new(YAHOO_SERVICE_PRESENCE_PERM,
					YAHOO_STATUS_AVAILABLE, yd->session_id);
			yahoo_packet_hash(pkt, "ssssssss",
					1, purple_connection_get_display_name(gc),
					31, "2", 13, "2",
					302, "319", 300, "319",
					7, name,
					301, "319", 303, "319");
			yahoo_packet_send_and_free(pkt, yd);
		}

		service = YAHOO_SERVICE_PRESENCE_SESSION;
		thirtyone = "1";
		thirteen = "1";
	}

	if (service > 0) {
		pkt = yahoo_packet_new(service,
				YAHOO_STATUS_AVAILABLE, yd->session_id);

		yahoo_packet_hash(pkt, "ssssssss",
				1, purple_connection_get_display_name(gc),
				31, thirtyone, 13, thirteen,
				302, "319", 300, "319",
				7, name,
				301, "319", 303, "319");

		yahoo_packet_send_and_free(pkt, yd);
	}
}

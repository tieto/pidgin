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

#include "prpl.h"
#include "util.h"

#include "yahoo_friend.h"

YahooFriend *yahoo_friend_new(void)
{
	YahooFriend *ret;

	ret = g_new0(YahooFriend, 1);
	ret->status = YAHOO_STATUS_OFFLINE;

	return ret;
}

YahooFriend *yahoo_friend_find(GaimConnection *gc, const char *name)
{
	struct yahoo_data *yd;
	const char *norm;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(gc->proto_data != NULL, NULL);

	yd = gc->proto_data;
	norm = gaim_normalize(gaim_connection_get_account(gc), name);

	return g_hash_table_lookup(yd->friends, norm);
}

YahooFriend *yahoo_friend_find_or_new(GaimConnection *gc, const char *name)
{
	YahooFriend *f;
	struct yahoo_data *yd;
	const char *norm;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(gc->proto_data != NULL, NULL);

	yd = gc->proto_data;
	norm = gaim_normalize(gaim_connection_get_account(gc), name);

	f = g_hash_table_lookup(yd->friends, norm);
	if (!f) {
		f = yahoo_friend_new();
		g_hash_table_insert(yd->friends, g_strdup(norm), f);
	}

	return f;
}

void yahoo_friend_set_ip(YahooFriend *f, const char *ip)
{
	if (f->ip)
		g_free(f->ip);
	f->ip = g_strdup(ip);
}

const char *yahoo_friend_get_ip(YahooFriend *f)
{
	return f->ip;
}

void yahoo_friend_free(gpointer p)
{
	YahooFriend *f = p;
	if (f->msg)
		g_free(f->msg);
	if (f->game)
		g_free(f->game);
	if (f->ip)
		g_free(f->ip);
	g_free(f);
}

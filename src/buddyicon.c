/**
 * @file icon.c Buddy Icon API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "internal.h"
#include "buddyicon.h"
#include "conversation.h"

static GHashTable *account_cache = NULL;

GaimBuddyIcon *
gaim_buddy_icon_new(GaimAccount *account, const char *username,
					void *icon_data, size_t icon_len)
{
	GaimBuddyIcon *icon;

	g_return_val_if_fail(account   != NULL, NULL);
	g_return_val_if_fail(username  != NULL, NULL);
	g_return_val_if_fail(icon_data != NULL, NULL);
	g_return_val_if_fail(icon_len  > 0,    NULL);

	icon = gaim_buddy_icons_find(account, username);

	if (icon == NULL)
	{
		GHashTable *icon_cache;

		icon = g_new0(GaimBuddyIcon, 1);

		gaim_buddy_icon_set_account(icon,  account);
		gaim_buddy_icon_set_username(icon, username);

		icon_cache = g_hash_table_lookup(account_cache, account);

		if (icon_cache == NULL)
		{
			icon_cache = g_hash_table_new(g_str_hash, g_str_equal);

			g_hash_table_insert(account_cache, account, icon_cache);
		}

		g_hash_table_insert(icon_cache,
							(char *)gaim_buddy_icon_get_username(icon), icon);
	}

	gaim_buddy_icon_set_data(icon, icon_data, icon_len);

	gaim_buddy_icon_ref(icon);

	return icon;
}

void
gaim_buddy_icon_destroy(GaimBuddyIcon *icon)
{
	GHashTable *icon_cache;

	g_return_if_fail(icon != NULL);

	if (icon->ref_count > 0)
	{
		gaim_buddy_icon_unref(icon);

		return;
	}

	icon_cache = g_hash_table_lookup(account_cache,
									 gaim_buddy_icon_get_account(icon));

	if (icon_cache != NULL)
		g_hash_table_remove(icon_cache, gaim_buddy_icon_get_username(icon));

	if (icon->username != NULL)
		g_free(icon->username);

	if (icon->data != NULL)
		g_free(icon->data);

	g_free(icon);
}

GaimBuddyIcon *
gaim_buddy_icon_ref(GaimBuddyIcon *icon)
{
	g_return_val_if_fail(icon != NULL, NULL);

	icon->ref_count++;

	return icon;
}

GaimBuddyIcon *
gaim_buddy_icon_unref(GaimBuddyIcon *icon)
{
	g_return_val_if_fail(icon != NULL, NULL);

	if (icon->ref_count <= 0)
		return NULL;

	icon->ref_count--;

	if (icon->ref_count == 0)
	{
		gaim_buddy_icon_destroy(icon);

		return NULL;
	}

	return icon;
}

void
gaim_buddy_icon_update(GaimBuddyIcon *icon)
{
	GaimConversation *conv;
	GaimAccount *account;
	const char *username;
	GSList *sl;

	g_return_if_fail(icon != NULL);

	account  = gaim_buddy_icon_get_account(icon);
	username = gaim_buddy_icon_get_username(icon);

	for (sl = gaim_find_buddies(account, username); sl != NULL; sl = sl->next)
	{
		GaimBuddy *buddy = (GaimBuddy *)sl->data;

		gaim_buddy_set_icon(buddy, icon);
	}

	conv = gaim_find_conversation_with_account(username, account);

	if (conv != NULL && gaim_conversation_get_type(conv) == GAIM_CONV_IM)
		gaim_im_set_icon(GAIM_IM(conv), icon);
}

void
gaim_buddy_icon_set_account(GaimBuddyIcon *icon, GaimAccount *account)
{
	g_return_if_fail(icon    != NULL);
	g_return_if_fail(account != NULL);

	icon->account = account;
}

void
gaim_buddy_icon_set_username(GaimBuddyIcon *icon, const char *username)
{
	g_return_if_fail(icon     != NULL);
	g_return_if_fail(username != NULL);

	if (icon->username != NULL)
		g_free(icon->username);

	icon->username = g_strdup(username);
}

void
gaim_buddy_icon_set_data(GaimBuddyIcon *icon, void *data, size_t len)
{
	g_return_if_fail(icon != NULL);

	if (icon->data != NULL)
		g_free(icon->data);

	if (data != NULL && len > 0)
	{
		icon->data = g_memdup(data, len);
		icon->len  = len;
	}
	else
	{
		icon->data = NULL;
		icon->len  = 0;
	}

	gaim_buddy_icon_update(icon);
}

GaimAccount *
gaim_buddy_icon_get_account(const GaimBuddyIcon *icon)
{
	g_return_val_if_fail(icon != NULL, NULL);

	return icon->account;
}

const char *
gaim_buddy_icon_get_username(const GaimBuddyIcon *icon)
{
	g_return_val_if_fail(icon != NULL, NULL);

	return icon->username;
}

const void *
gaim_buddy_icon_get_data(const GaimBuddyIcon *icon, size_t *len)
{
	g_return_val_if_fail(icon != NULL, NULL);

	if (len != NULL)
		*len = icon->len;

	return icon->data;
}

void
gaim_buddy_icons_set_for_user(GaimAccount *account, const char *username,
							  void *icon_data, size_t icon_len)
{
	g_return_if_fail(account  != NULL);
	g_return_if_fail(username != NULL);

	gaim_buddy_icon_new(account, username, icon_data, icon_len);
}

GaimBuddyIcon *
gaim_buddy_icons_find(const GaimAccount *account, const char *username)
{
	GHashTable *icon_cache;

	g_return_val_if_fail(account  != NULL, NULL);
	g_return_val_if_fail(username != NULL, NULL);

	icon_cache = g_hash_table_lookup(account_cache, account);

	if (icon_cache == NULL)
		return NULL;

	return g_hash_table_lookup(icon_cache, username);
}

void *
gaim_buddy_icons_get_handle()
{
	static int handle;

	return &handle;
}

void
gaim_buddy_icons_init()
{
	account_cache = g_hash_table_new_full(
		g_direct_hash, g_direct_equal,
		NULL, (GFreeFunc)g_hash_table_destroy);
}

void
gaim_buddy_icons_uninit()
{
	g_hash_table_destroy(account_cache);
}

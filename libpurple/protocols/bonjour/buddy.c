/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <stdlib.h>

#include "buddy.h"
#include "account.h"
#include "blist.h"
#include "bonjour.h"
#include "debug.h"

/**
 * Creates a new buddy.
 */
BonjourBuddy *
bonjour_buddy_new(const gchar *name, PurpleAccount* account)
{
	BonjourBuddy *buddy = g_new0(BonjourBuddy, 1);

	buddy->account = account;
	buddy->name = g_strdup(name);

	return buddy;
}

void
set_bonjour_buddy_value(BonjourBuddy* buddy, bonjour_buddy_member member, const char* value, uint32_t len)
{
	gchar **key = NULL;
	switch (member)
	{
		case E_BUDDY_FIRST:
			key = &buddy->first;
			break;
		case E_BUDDY_LAST:
			key = &buddy->last;
			break;
		case E_BUDDY_STATUS:
			key = &buddy->status;
			break;
		case E_BUDDY_EMAIL:
			key = &buddy->email;
			break;
		case E_BUDDY_PHSH:
			key = &buddy->phsh;
			break;
		case E_BUDDY_JID:
			key = &buddy->jid;
			break;
		case E_BUDDY_AIM:
			key = &buddy->AIM;
			break;
		case E_BUDDY_VC:
			key = &buddy->vc;
			break;
		case E_BUDDY_MSG:
			key = &buddy->msg;
			break;
	}

	g_free(*key);
	*key = NULL;
	*key = g_strndup(value, len);
}

/**
 * Check if all the compulsory buddy data is present.
 */
gboolean
bonjour_buddy_check(BonjourBuddy *buddy)
{
	if (buddy->account == NULL)
		return FALSE;

	if (buddy->name == NULL)
		return FALSE;

	return TRUE;
}

/**
 * If the buddy does not yet exist, then create it and add it to
 * our buddy list.  In either case we set the correct status for
 * the buddy.
 */
void
bonjour_buddy_add_to_purple(BonjourBuddy *bonjour_buddy)
{
	PurpleBuddy *buddy;
	PurpleGroup *group;
	const char *status_id, *first, *last;
	gchar *alias;

	/* Translate between the Bonjour status and the Purple status */
	if (g_ascii_strcasecmp("dnd", bonjour_buddy->status) == 0)
		status_id = BONJOUR_STATUS_ID_AWAY;
	else
		status_id = BONJOUR_STATUS_ID_AVAILABLE;

	/*
	 * TODO: Figure out the idle time by getting the "away"
	 * field from the DNS SD.
	 */

	/* Create the alias for the buddy using the first and the last name */
	first = bonjour_buddy->first;
	last = bonjour_buddy->last;
	alias = g_strdup_printf("%s%s%s",
							(first && *first ? first : ""),
							(first && *first && last && *last ? " " : ""),
							(last && *last ? last : ""));

	/* Make sure the Bonjour group exists in our buddy list */
	group = purple_find_group(BONJOUR_GROUP_NAME); /* Use the buddy's domain, instead? */
	if (group == NULL)
	{
		group = purple_group_new(BONJOUR_GROUP_NAME);
		purple_blist_add_group(group, NULL);
	}

	/* Make sure the buddy exists in our buddy list */
	buddy = purple_find_buddy(bonjour_buddy->account, bonjour_buddy->name);

	if (buddy == NULL)
	{
		buddy = purple_buddy_new(bonjour_buddy->account, bonjour_buddy->name, alias);
		buddy->proto_data = bonjour_buddy;
		purple_blist_node_set_flags((PurpleBlistNode *)buddy, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
		purple_blist_add_buddy(buddy, NULL, group, NULL);
	}

	/* Set the user's status */
	if (bonjour_buddy->msg != NULL)
		purple_prpl_got_user_status(bonjour_buddy->account, buddy->name, status_id,
								  "message", bonjour_buddy->msg,
								  NULL);
	else
		purple_prpl_got_user_status(bonjour_buddy->account, buddy->name, status_id,
								  NULL);
	purple_prpl_got_user_idle(bonjour_buddy->account, buddy->name, FALSE, 0);

	g_free(alias);
}

/**
 * Deletes a buddy from memory.
 */
void
bonjour_buddy_delete(BonjourBuddy *buddy)
{
	g_free(buddy->name);
	g_free(buddy->first);
	g_free(buddy->phsh);
	g_free(buddy->status);
	g_free(buddy->email);
	g_free(buddy->last);
	g_free(buddy->jid);
	g_free(buddy->AIM);
	g_free(buddy->vc);
	g_free(buddy->ip);
	g_free(buddy->msg);

	if (buddy->conversation != NULL)
	{
		g_free(buddy->conversation->buddy_name);
		g_free(buddy->conversation);
	}

#ifdef USE_BONJOUR_APPLE
	if (buddy->txt_query != NULL)
	{
		purple_input_remove(buddy->txt_query_fd);
		DNSServiceRefDeallocate(buddy->txt_query);
	}
#endif

	g_free(buddy);
}

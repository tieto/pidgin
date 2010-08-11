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
bonjour_buddy_new(const gchar *name, const gchar *first, gint port_p2pj,
	const gchar *phsh, const gchar *status, const gchar *email,
	const gchar *last, const gchar *jid, const gchar *AIM,
	const gchar *vc, const gchar *ip, const gchar *msg)
{
	BonjourBuddy *buddy = malloc(sizeof(BonjourBuddy));

	buddy->name = g_strdup(name);
	buddy->first = g_strdup(first);
	buddy->port_p2pj = port_p2pj;
	buddy->phsh = g_strdup(phsh);
	buddy->status = g_strdup(status);
	buddy->email = g_strdup(email);
	buddy->last = g_strdup(last);
	buddy->jid = g_strdup(jid);
	buddy->AIM = g_strdup(AIM);
	buddy->vc = g_strdup(vc);
	buddy->ip = g_strdup(ip);
	buddy->msg = g_strdup(msg);
	buddy->conversation = NULL;

	return buddy;
}

/**
 * Check if all the compulsory buddy data is present.
 */
gboolean
bonjour_buddy_check(BonjourBuddy *buddy)
{
	if (buddy->name == NULL) {
		return FALSE;
	}

	if (buddy->first == NULL) {
		return FALSE;
	}

	if (buddy->last == NULL) {
		return FALSE;
	}

	if (buddy->status == NULL) {
		return FALSE;
	}

	return TRUE;
}

/**
 * If the buddy does not yet exist, then create it and add it to
 * our buddy list.  In either case we set the correct status for
 * the buddy.
 */
void
bonjour_buddy_add_to_purple(PurpleAccount *account, BonjourBuddy *bonjour_buddy)
{
	PurpleBuddy *buddy;
	PurpleGroup *group;
	const char *status_id, *first, *last;
	char *alias;

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
	buddy = purple_find_buddy(account, bonjour_buddy->name);
	if (buddy == NULL)
	{
		buddy = purple_buddy_new(account, bonjour_buddy->name, alias);
		buddy->proto_data = bonjour_buddy;
		purple_blist_node_set_flags((PurpleBlistNode *)buddy, PURPLE_BLIST_NODE_FLAG_NO_SAVE);
		purple_blist_add_buddy(buddy, NULL, group, NULL);
	}

	/* Set the user's status */
	if (bonjour_buddy->msg != NULL)
		purple_prpl_got_user_status(account, buddy->name, status_id,
								  "message", bonjour_buddy->msg,
								  NULL);
	else
		purple_prpl_got_user_status(account, buddy->name, status_id,
								  NULL);
	purple_prpl_got_user_idle(account, buddy->name, FALSE, 0);

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

	free(buddy);
}

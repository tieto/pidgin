/**
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "facebook_roster.h"

#include "roster.h"

#include "internal.h"

void
jabber_facebook_roster_cleanup(JabberStream *js, xmlnode *query)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	xmlnode *item;
	GSList *local_buddies;
	GHashTable *remove_buddies;
	GHashTableIter it;
	PurpleBuddy *buddy;
	const gchar *jid;

	if (js->facebook_roster_cleanup_performed)
		return;
	js->facebook_roster_cleanup_performed = TRUE;

	/* mark all local buddies as "to be removed" */
	remove_buddies = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, NULL);
	local_buddies = purple_find_buddies(account, NULL);
	for (; local_buddies; local_buddies = g_slist_delete_link(
		local_buddies, local_buddies))
	{
		buddy = local_buddies->data;

		g_hash_table_insert(remove_buddies, g_strdup(jabber_normalize(
			account, purple_buddy_get_name(buddy))), buddy);
	}

	/* un-mark all remote buddies */
	for (item = xmlnode_get_child(query, "item"); item;
		item = xmlnode_get_next_twin(item))
	{
		jid = xmlnode_get_attrib(item, "jid");

		g_hash_table_remove(remove_buddies,
			jabber_normalize(account, jid));
	}

	/* remove all not-remote buddies */
	g_hash_table_iter_init(&it, remove_buddies);
	while (g_hash_table_iter_next(&it, (gpointer*)&jid, (gpointer*)&buddy)) {
		const gchar *alias = purple_buddy_get_local_buddy_alias(buddy);
		item = xmlnode_new_child(query, "item");
		xmlnode_set_namespace(item, xmlnode_get_namespace(query));
		xmlnode_set_attrib(item, "jid", jid);
		xmlnode_set_attrib(item, "subscription", "remove");
		if (alias)
			xmlnode_set_attrib(item, "name", alias);
	}

	g_hash_table_destroy(remove_buddies);
}

gboolean
jabber_facebook_roster_incoming(JabberStream *js, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	const gchar *jid, *subscription;
	gchar *jid_norm;
	PurpleBuddy *buddy;
	PurpleGroup *buddy_group;
	xmlnode *group;
	const gchar *alias;

	subscription = xmlnode_get_attrib(item, "subscription");

	/* Skip entries added with jabber_facebook_roster_cleanup */
	if (g_strcmp0(subscription, "remove") == 0)
		return TRUE;

	jid = xmlnode_get_attrib(item, "jid");
	jid_norm = g_strdup(jabber_normalize(account, jid));
	buddy = purple_find_buddy(account, jid);
	g_free(jid_norm);

	/* Facebook forces "Facebook Friends" group */
	while ((group = xmlnode_get_child(item, "group")) != NULL)
		xmlnode_free(group);
	group = xmlnode_new_child(item, "group");
	xmlnode_set_namespace(group, xmlnode_get_namespace(item));

	/* We don't have that buddy on the list, add him to the default group */
	if (!buddy) {
		xmlnode_insert_data(group,
			JABBER_ROSTER_DEFAULT_GROUP, -1);
		return TRUE;
	}

	/* Facebook forces buddy real name as alias */
	alias = purple_buddy_get_local_buddy_alias(buddy);
	if (alias)
		xmlnode_set_attrib(item, "name", alias);

	/* Add buddy to his group */
	buddy_group = purple_buddy_get_group(buddy);
	if (g_strcmp0(purple_group_get_name(buddy_group), _("Buddies")) == 0)
		buddy_group = NULL;
	if (buddy_group) {
		xmlnode_insert_data(group,
			purple_group_get_name(buddy_group), -1);
	} else {
		xmlnode_insert_data(group,
			JABBER_ROSTER_DEFAULT_GROUP, -1);
	}

	return TRUE;
}

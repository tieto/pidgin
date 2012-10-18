/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Rewritten from scratch during Google Summer of Code 2012
 * by Tomek Wasilczyk (http://www.wasilczyk.pl).
 *
 * Previously implemented by:
 *  - Arkadiusz Miskiewicz <misiek@pld.org.pl> - first implementation (2001);
 *  - Bartosz Oler <bartosz@bzimage.us> - reimplemented during GSoC 2005;
 *  - Krzysztof Klinikowski <grommasher@gmail.com> - some parts (2009-2011).
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
 */

#include "roster.h"

#include "gg.h"
#include "xml.h"
#include "utils.h"
#include "purplew.h"

#include <debug.h>
#include <glibcompat.h>

#define GGP_ROSTER_SYNC_SETT "gg-synchronized"
#define GGP_ROSTER_DEBUG 0
#define GGP_ROSTER_GROUPID_DEFAULT "00000000-0000-0000-0000-000000000000"
#define GGP_ROSTER_GROUPID_BOTS "0b345af6-0001-0000-0000-000000000004"

// TODO: ignored contacts synchronization (?)

typedef struct
{
	int version;
	
	xmlnode *xml;
	
	xmlnode *groups_node, *contacts_node;
	
	/**
	 * Key: (uin_t) user identifier
	 * Value: (xmlnode*) xml node for contact
	 */
	GHashTable *contact_nodes;
	
	/**
	 * Key: (gchar*) group id
	 * Value: (xmlnode*) xml node for group
	 */
	GHashTable *group_nodes;
	
	/**
	 * Key: (gchar*) group name
	 * Value: (gchar*) group id
	 */
	GHashTable *group_ids;
	
	/**
	 * Key: (gchar*) group id
	 * Value: (gchar*) group name
	 */
	GHashTable *group_names;

	gchar *bots_group_id;

	gboolean needs_update;
} ggp_roster_content;

typedef struct
{
	enum
	{
		GGP_ROSTER_CHANGE_CONTACT_UPDATE,
		GGP_ROSTER_CHANGE_CONTACT_REMOVE,
		GGP_ROSTER_CHANGE_GROUP_RENAME,
	} type;
	union
	{
		uin_t uin;
		struct
		{
			gchar *old_name;
			gchar *new_name;
		} group_rename;
	} data;
} ggp_roster_change;

static inline ggp_roster_session_data *
ggp_roster_get_rdata(PurpleConnection *gc);
static void ggp_roster_content_free(ggp_roster_content *content);
static void ggp_roster_change_free(gpointer change);
static int ggp_roster_get_version(PurpleConnection *gc);
static gboolean ggp_roster_timer_cb(gpointer _gc);
#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(ggp_roster_content *content);
#endif

// synchronization control
static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy);
static void ggp_roster_set_synchronized(PurpleConnection *gc,
	PurpleBuddy *buddy, gboolean synchronized);

// buddy list import
static gboolean ggp_roster_reply_list_read_group(xmlnode *node,
	ggp_roster_content *content);
static gboolean ggp_roster_reply_list_read_buddy(PurpleConnection *gc,
	xmlnode *node, ggp_roster_content *content, GHashTable *remove_buddies);
static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version,
	const char *reply);

// buddy list export
static const gchar * ggp_roster_send_update_group_add(
	ggp_roster_content *content, PurpleGroup *group);
static gboolean ggp_roster_send_update_contact_update(PurpleConnection *gc,
	ggp_roster_change *change);
static gboolean ggp_roster_send_update_contact_remove(PurpleConnection *gc,
	ggp_roster_change *change);
static gboolean ggp_roster_send_update_group_rename(PurpleConnection *gc,
	ggp_roster_change *change);
static void ggp_roster_send_update(PurpleConnection *gc);
static void ggp_roster_reply_ack(PurpleConnection *gc, uint32_t version);
static void ggp_roster_reply_reject(PurpleConnection *gc, uint32_t version);

/******************************************************************************/

static inline ggp_roster_session_data *
ggp_roster_get_rdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return &accdata->roster_data;
} 

static void ggp_roster_content_free(ggp_roster_content *content)
{
	if (content == NULL)
		return;
	if (content->xml)
		xmlnode_free(content->xml);
	if (content->contact_nodes)
		g_hash_table_destroy(content->contact_nodes);
	if (content->group_nodes)
		g_hash_table_destroy(content->group_nodes);
	if (content->group_ids)
		g_hash_table_destroy(content->group_ids);
	if (content->group_names)
		g_hash_table_destroy(content->group_names);
	if (content->bots_group_id)
		g_free(content->bots_group_id);
	g_free(content);
}

static void ggp_roster_change_free(gpointer _change)
{
	ggp_roster_change *change = _change;
	
	if (change->type == GGP_ROSTER_CHANGE_GROUP_RENAME)
	{
		g_free(change->data.group_rename.old_name);
		g_free(change->data.group_rename.new_name);
	}
	
	g_free(change);
}

static int ggp_roster_get_version(PurpleConnection *gc)
{
	ggp_roster_content *content = ggp_roster_get_rdata(gc)->content;
	if (content == NULL)
		return 0;
	return content->version;
}

static gboolean ggp_roster_timer_cb(gpointer _gc)
{
	PurpleConnection *gc = _gc;
	
	g_return_val_if_fail(PURPLE_CONNECTION_IS_VALID(gc), FALSE);
	
	ggp_roster_send_update(gc);
	
	return TRUE;
}

#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(ggp_roster_content *content)
{
	char *str;
	int len;
	
	g_return_if_fail(content != NULL);
	g_return_if_fail(content->xml != NULL);
	
	str = xmlnode_to_formatted_str(content->xml, &len);
	purple_debug_misc("gg", "ggp_roster_dump: [%s]\n", str);
	g_free(str);
}
#endif

/*******************************************************************************
 * Setup.
 ******************************************************************************/

gboolean ggp_roster_enabled(void)
{
	static gboolean checked = FALSE;
	static gboolean enabled;
	
	if (!checked)
	{
		enabled = gg_libgadu_check_feature(
			GG_LIBGADU_FEATURE_USERLIST100);
		checked = TRUE;
	}
	return enabled;
}

void ggp_roster_setup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	rdata->content = NULL;
	rdata->sent_updates = NULL;
	rdata->pending_updates = NULL;
	rdata->timer = 0;
	rdata->is_updating = FALSE;
	
	if (ggp_roster_enabled())
		rdata->timer = purple_timeout_add_seconds(2,
			ggp_roster_timer_cb, gc);
}

void ggp_roster_cleanup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	if (rdata->timer)
		purple_timeout_remove(rdata->timer);
	ggp_roster_content_free(rdata->content);
	g_list_free_full(rdata->sent_updates, ggp_roster_change_free);
	g_list_free_full(rdata->pending_updates, ggp_roster_change_free);
}

/*******************************************************************************
 * Synchronization control.
 ******************************************************************************/

static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy)
{
	gboolean ret = purple_blist_node_get_bool(PURPLE_BLIST_NODE(buddy),
		GGP_ROSTER_SYNC_SETT);
	return ret;
}

static void ggp_roster_set_synchronized(PurpleConnection *gc,
	PurpleBuddy *buddy, gboolean synchronized)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
	ggp_roster_change *change;
	
	purple_blist_node_set_bool(PURPLE_BLIST_NODE(buddy),
		GGP_ROSTER_SYNC_SETT, synchronized);
	if (!synchronized)
	{
		change = g_new0(ggp_roster_change, 1);
		change->type = GGP_ROSTER_CHANGE_CONTACT_UPDATE;
		change->data.uin = uin;
		rdata->pending_updates =
			g_list_append(rdata->pending_updates, change);
	}
}

void ggp_roster_request_update(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	int local_version = ggp_roster_get_version(gc);
	
	if (!ggp_roster_enabled())
	{
		purple_debug_warning("gg", "ggp_roster_request_update: "
			"feature disabled\n");
		return;
	}
	
	purple_debug_info("gg", "ggp_roster_request_update: local=%u\n",
		local_version);
	
	gg_userlist100_request(accdata->session, GG_USERLIST100_GET,
		local_version, GG_USERLIST100_FORMAT_TYPE_GG100, NULL);
}

/*******************************************************************************
 * Libgadu callbacks.
 ******************************************************************************/

void ggp_roster_reply(PurpleConnection *gc,
	struct gg_event_userlist100_reply *reply)
{
	if (GG_USERLIST100_FORMAT_TYPE_GG100 != reply->format_type)
	{
		purple_debug_warning("gg", "ggp_roster_reply: "
			"unsupported format type (%x)\n", reply->format_type);
		return;
	}
	
	if (reply->type == GG_USERLIST100_REPLY_LIST)
		ggp_roster_reply_list(gc, reply->version, reply->reply);
	else if (reply->type == 0x01) // list up to date (TODO: push to libgadu)
		purple_debug_info("gg", "ggp_roster_reply: list up to date\n");
	else if (reply->type == GG_USERLIST100_REPLY_ACK)
		ggp_roster_reply_ack(gc, reply->version);
	else if (reply->type == GG_USERLIST100_REPLY_REJECT)
		ggp_roster_reply_reject(gc, reply->version);
	else
		purple_debug_error("gg", "ggp_roster_reply: "
			"unsupported reply (%x)\n", reply->type);
}

void ggp_roster_version(PurpleConnection *gc,
	struct gg_event_userlist100_version *version)
{
	int local_version = ggp_roster_get_version(gc);
	int remote_version = version->version;

	purple_debug_info("gg", "ggp_roster_version: local=%u, remote=%u\n",
		local_version, remote_version);
	
	if (local_version < remote_version)
		ggp_roster_request_update(gc);
}

/*******************************************************************************
 * Libpurple callbacks.
 ******************************************************************************/

void ggp_roster_alias_buddy(PurpleConnection *gc, const char *who,
	const char *alias)
{
	PurpleBuddy *buddy;
	
	g_return_if_fail(who != NULL);
	
	if (!ggp_roster_enabled())
		return;
	
	purple_debug_misc("gg", "ggp_roster_alias_buddy(\"%s\", \"%s\")\n",
		who, alias);
	
	buddy = purple_find_buddy(purple_connection_get_account(gc), who);
	g_return_if_fail(buddy != NULL);
	
	ggp_roster_set_synchronized(gc, buddy, FALSE);
}

void ggp_roster_group_buddy(PurpleConnection *gc, const char *who,
	const char *old_group, const char *new_group)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_change *change = g_new0(ggp_roster_change, 1);

	if (!ggp_roster_enabled())
		return;
	if (rdata->is_updating)
		return;
	
	purple_debug_misc("gg", "ggp_roster_group_buddy: "
		"who=\"%s\", group=\"%s\" -> \"%s\")\n",
		who, old_group, new_group);
	
	// purple_find_buddy(..., who) is not accessible at this moment
	change->type = GGP_ROSTER_CHANGE_CONTACT_UPDATE;
	change->data.uin = ggp_str_to_uin(who);
	rdata->pending_updates = g_list_append(rdata->pending_updates, change);
}

void ggp_roster_rename_group(PurpleConnection *gc, const char *old_name,
	PurpleGroup *group, GList *moved_buddies)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_change *change = g_new0(ggp_roster_change, 1);
	
	if (!ggp_roster_enabled())
		return;
	
	change->type = GGP_ROSTER_CHANGE_GROUP_RENAME;
	change->data.group_rename.old_name = g_strdup(old_name);
	change->data.group_rename.new_name =
		g_strdup(purple_group_get_name(group));
	rdata->pending_updates = g_list_append(rdata->pending_updates, change);
}

void ggp_roster_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
	PurpleGroup *group, const char *message)
{
	g_return_if_fail(gc != NULL);
	g_return_if_fail(buddy != NULL);

	if (!ggp_roster_enabled())
		return;
	
	ggp_roster_set_synchronized(gc, buddy, FALSE);
}

void ggp_roster_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
	PurpleGroup *group)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_change *change = g_new0(ggp_roster_change, 1);
	
	if (!ggp_roster_enabled())
		return;
	
	change->type = GGP_ROSTER_CHANGE_CONTACT_REMOVE;
	change->data.uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
	rdata->pending_updates = g_list_append(rdata->pending_updates, change);
}

/*******************************************************************************
 * Buddy list import.
 ******************************************************************************/

static gboolean ggp_roster_reply_list_read_group(xmlnode *node,
	ggp_roster_content *content)
{
	char *name, *id;
	gboolean removable;
	gboolean succ = TRUE, is_bot, is_default;
	
	succ &= ggp_xml_get_string(node, "Id", &id);
	succ &= ggp_xml_get_string(node, "Name", &name);
	succ &= ggp_xml_get_bool(node, "IsRemovable", &removable);
	
	if (!succ)
	{
		g_free(id);
		g_free(name);
		g_return_val_if_reached(FALSE);
	}
	
	is_bot = (strcmp(id, GGP_ROSTER_GROUPID_BOTS) == 0 ||
		g_strcmp0(name, "Pomocnicy") == 0);
	is_default = (strcmp(id, GGP_ROSTER_GROUPID_DEFAULT) == 0 ||
		g_strcmp0(name, GGP_PURPLEW_GROUP_DEFAULT) == 0 ||
		g_strcmp0(name, "[default]") == 0);
	
	if (!content->bots_group_id && is_bot)
		content->bots_group_id = g_strdup(id);
	
	if (!removable || is_bot || is_default)
	{
		g_free(id);
		g_free(name);
		return TRUE;
	}
	
	g_hash_table_insert(content->group_nodes, g_strdup(id), node);
	g_hash_table_insert(content->group_ids, g_strdup(name), g_strdup(id));
	g_hash_table_insert(content->group_names, id, name);
	
	return TRUE;
}

static gboolean ggp_roster_reply_list_read_buddy(PurpleConnection *gc,
	xmlnode *node, ggp_roster_content *content, GHashTable *remove_buddies)
{
	gchar *alias, *group_name = NULL;
	uin_t uin;
	gboolean succ = TRUE;
	xmlnode *group_list, *group_elem;
	PurpleBuddy *buddy = NULL;
	PurpleGroup *group = NULL;
	PurpleGroup *currentGroup;
	gboolean alias_changed;
	PurpleAccount *account = purple_connection_get_account(gc);
	
	succ &= ggp_xml_get_string(node, "ShowName", &alias);
	succ &= ggp_xml_get_uint(node, "GGNumber", &uin);
	
	group_list = xmlnode_get_child(node, "Groups");
	succ &= (group_list != NULL);
	
	if (!succ)
	{
		g_free(alias);
		g_return_val_if_reached(FALSE);
	}
	
	g_hash_table_insert(content->contact_nodes, GINT_TO_POINTER(uin), node);
	
	// check, if alias is set
	if (strlen(alias) == 0 ||
		strcmp(alias, ggp_uin_to_str(uin)) == 0)
	{
		g_free(alias);
		alias = NULL;
	}
	
	// getting (eventually creating) group
	group_elem = xmlnode_get_child(group_list, "GroupId");
	while (group_elem != NULL)
	{
		gchar *id;
		gboolean isbot;
		
		if (!ggp_xml_get_string(group_elem, NULL, &id))
			continue;
		isbot = (0 == g_strcmp0(id, content->bots_group_id));
		group_name = g_hash_table_lookup(content->group_names, id);
		g_free(id);
		
		// we don't want to import bots;
		// they are inserted to roster by default
		if (isbot)
		{
			g_free(alias);
			return TRUE;
		}
		
		if (group_name != NULL)
			break;
		
		group_elem = xmlnode_get_next_twin(group_elem);
	}
	if (group_name)
	{
		group = purple_find_group(group_name);
		if (!group)
		{
			group = purple_group_new(group_name);
			purple_blist_add_group(group, NULL);
		}
	}
	
	// add buddy, if doesn't exists
	buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
	g_hash_table_remove(remove_buddies, GINT_TO_POINTER(uin));
	if (!buddy)
	{
		purple_debug_info("gg", "ggp_roster_reply_list_read_buddy: "
			"adding %u (%s) to buddy list\n", uin, alias);
		buddy = purple_buddy_new(account, ggp_uin_to_str(uin), alias);
		purple_blist_add_buddy(buddy, NULL, group, NULL);
		ggp_roster_set_synchronized(gc, buddy, TRUE);
		
		g_free(alias);
		return TRUE;
	}
	
	// buddy exists, but is not synchronized - local list has priority
	if (!ggp_roster_is_synchronized(buddy))
	{
		purple_debug_misc("gg", "ggp_roster_reply_list_read_buddy: "
			"ignoring not synchronized %u (%s)\n",
			uin, purple_buddy_get_name(buddy));
		g_free(alias);
		return TRUE;
	}
	
	currentGroup = ggp_purplew_buddy_get_group_only(buddy);
	alias_changed =
		(0 != g_strcmp0(alias, purple_buddy_get_alias_only(buddy)));
	
	if (currentGroup == group && !alias_changed)
	{
		g_free(alias);
		return TRUE;
	}
	
	purple_debug_misc("gg", "ggp_roster_reply_list_read_buddy: "
		"updating %u (%s) - alias=\"%s\"->\"%s\", group=%p->%p (%s)\n",
		uin, purple_buddy_get_name(buddy),
		purple_buddy_get_alias(buddy), alias,
		currentGroup, group, group_name);
	if (alias_changed)
		purple_blist_alias_buddy(buddy, alias);
	if (currentGroup != group)
		purple_blist_add_buddy(buddy, NULL, group, NULL);
	
	g_free(alias);
	return TRUE;
}

static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version,
	const char *data)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	xmlnode *xml, *xml_it;
	PurpleAccount *account;
	GSList *local_buddies;
	GHashTable *remove_buddies;
	GList *update_buddies = NULL, *local_groups, *it, *table_values;
	ggp_roster_content *content;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(data != NULL);

	account = purple_connection_get_account(gc);
	
	purple_debug_info("gg", "ggp_roster_reply_list: got list, version=%u\n",
		version);

	xml = xmlnode_from_str(data, -1);
	if (xml == NULL)
	{
		purple_debug_warning("gg", "ggp_roster_reply_list: "
			"invalid xml\n");
		return;
	}

	ggp_roster_content_free(rdata->content);
	rdata->content = NULL;
	rdata->is_updating = TRUE;
	content = g_new0(ggp_roster_content, 1);
	content->version = version;
	content->xml = xml;
	content->contact_nodes = g_hash_table_new(NULL, NULL);
	content->group_nodes = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, NULL);
	content->group_ids = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, g_free);
	content->group_names = g_hash_table_new_full(
		g_str_hash, g_str_equal, g_free, g_free);

#if GGP_ROSTER_DEBUG
	ggp_roster_dump(content);
#endif

	// reading groups
	content->groups_node = xmlnode_get_child(xml, "Groups");
	if (content->groups_node == NULL)
	{
		ggp_roster_content_free(content);
		g_return_if_reached();
	}
	xml_it = xmlnode_get_child(content->groups_node, "Group");
	while (xml_it != NULL)
	{
		if (!ggp_roster_reply_list_read_group(xml_it, content))
		{
			ggp_roster_content_free(content);
			g_return_if_reached();
		}
		
		xml_it = xmlnode_get_next_twin(xml_it);
	}
	
	// dumping current group list
	local_groups = ggp_purplew_account_get_groups(account, TRUE);
	
	// dumping current buddy list
	// we will:
	// - remove synchronized ones, if not found in list at server
	// - upload not synchronized ones
	local_buddies = purple_find_buddies(account, NULL);
	remove_buddies = g_hash_table_new(g_direct_hash, g_direct_equal);
	while (local_buddies)
	{
		PurpleBuddy *buddy = local_buddies->data;
		uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
		local_buddies =
			g_slist_delete_link(local_buddies, local_buddies);

		if (!uin)
			continue;

		if (ggp_roster_is_synchronized(buddy))
			g_hash_table_insert(remove_buddies,
				GINT_TO_POINTER(uin), buddy);
		else
			update_buddies = g_list_append(update_buddies, buddy);
	}
	
	// reading buddies
	content->contacts_node = xmlnode_get_child(xml, "Contacts");
	if (content->contacts_node == NULL)
	{
		g_hash_table_destroy(remove_buddies);
		g_list_free(update_buddies);
		ggp_roster_content_free(content);
		g_return_if_reached();
	}
	xml_it = xmlnode_get_child(content->contacts_node, "Contact");
	while (xml_it != NULL)
	{
		if (!ggp_roster_reply_list_read_buddy(gc, xml_it, content,
			remove_buddies))
		{
			g_hash_table_destroy(remove_buddies);
			g_list_free(update_buddies);
			ggp_roster_content_free(content);
			g_return_if_reached();
		}
	
		xml_it = xmlnode_get_next_twin(xml_it);
	}
	
	// removing buddies, which are not present in roster
	table_values = g_hash_table_get_values(remove_buddies);
	it = g_list_first(table_values);
	while (it)
	{
		PurpleBuddy *buddy = it->data;
		it = g_list_next(it);
		if (!ggp_roster_is_synchronized(buddy))
			continue;
		purple_debug_info("gg", "ggp_roster_reply_list: "
			"removing %s from buddy list\n",
			purple_buddy_get_name(buddy));
		purple_blist_remove_buddy(buddy);
	}
	g_list_free(table_values);
	g_hash_table_destroy(remove_buddies);
	
	// remove groups, which are empty, but had contacts before
	// synchronization
	it = g_list_first(local_groups);
	while (it)
	{
		PurpleGroup *group = it->data;
		it = g_list_next(it);
		if (purple_blist_get_group_size(group, TRUE) != 0)
			continue;
		purple_debug_info("gg", "ggp_roster_reply_list: "
			"removing group %s\n", purple_group_get_name(group));
		purple_blist_remove_group(group);
	}
	g_list_free(local_groups);
	
	// adding not synchronized buddies
	it = g_list_first(update_buddies);
	while (it)
	{
		PurpleBuddy *buddy = it->data;
		uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
		ggp_roster_change *change;
		
		it = g_list_next(it);
		g_assert(uin > 0);
		
		purple_debug_misc("gg", "ggp_roster_reply_list: "
			"adding change of %u for roster\n", uin);
		change = g_new0(ggp_roster_change, 1);
		change->type = GGP_ROSTER_CHANGE_CONTACT_UPDATE;
		change->data.uin = uin;
		rdata->pending_updates =
			g_list_append(rdata->pending_updates, change);
	}
	g_list_free(update_buddies);

	rdata->content = content;
	rdata->is_updating = FALSE;
	purple_debug_info("gg", "ggp_roster_reply_list: "
		"import done, version=%u\n", version);
}

/*******************************************************************************
 * Buddy list export.
 ******************************************************************************/

static const gchar * ggp_roster_send_update_group_add(
	ggp_roster_content *content, PurpleGroup *group)
{
	gchar *id_dyn;
	const char *id_existing, *group_name;
	static gchar id[40];
	xmlnode *group_node;
	gboolean succ = TRUE;

	if (group)
	{
		group_name = purple_group_get_name(group);
		id_existing =
			g_hash_table_lookup(content->group_ids, group_name);
	}
	else
		id_existing = GGP_ROSTER_GROUPID_DEFAULT;
	if (id_existing)
		return id_existing;

	purple_debug_info("gg", "ggp_roster_send_update_group_add: adding %s\n",
		purple_group_get_name(group));

	id_dyn = purple_uuid_random();
	g_snprintf(id, sizeof(id), "%s", id_dyn);
	g_free(id_dyn);
	
	group_node = xmlnode_new_child(content->groups_node, "Group");
	succ &= ggp_xml_set_string(group_node, "Id", id);
	succ &= ggp_xml_set_string(group_node, "Name", group_name);
	succ &= ggp_xml_set_string(group_node, "IsExpanded", "true");
	succ &= ggp_xml_set_string(group_node, "IsRemovable", "true");
	content->needs_update = TRUE;
	
	g_hash_table_insert(content->group_ids, g_strdup(group_name),
		g_strdup(id));
	g_hash_table_insert(content->group_nodes, g_strdup(id), group_node);
	
	g_return_val_if_fail(succ, NULL);

	return id;
}

static gboolean ggp_roster_send_update_contact_update(PurpleConnection *gc,
	ggp_roster_change *change)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_content *content = ggp_roster_get_rdata(gc)->content;
	uin_t uin = change->data.uin;
	PurpleBuddy *buddy;
	xmlnode *buddy_node, *contact_groups;
	gboolean succ = TRUE;
	const char *group_id;
	
	g_return_val_if_fail(change->type == GGP_ROSTER_CHANGE_CONTACT_UPDATE,
		FALSE);
	
	buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
	if (!buddy)
		return TRUE;
	buddy_node = g_hash_table_lookup(content->contact_nodes,
		GINT_TO_POINTER(uin));

	group_id = ggp_roster_send_update_group_add(content,
		ggp_purplew_buddy_get_group_only(buddy));

	if (buddy_node)
	{ // update existing
		purple_debug_misc("gg", "ggp_roster_send_update_contact_update:"
			" updating %u...\n", uin);
		
		succ &= ggp_xml_set_string(buddy_node, "ShowName",
			purple_buddy_get_alias(buddy));
		
		contact_groups = xmlnode_get_child(buddy_node, "Groups");
		g_assert(contact_groups);
		ggp_xmlnode_remove_children(contact_groups);
		succ &= ggp_xml_set_string(contact_groups, "GroupId", group_id);
	
		g_return_val_if_fail(succ, FALSE);
		
		return TRUE;
	}
	
	// add new
	purple_debug_misc("gg", "ggp_roster_send_update_contact_update: "
		"adding %u...\n", uin);
	buddy_node = xmlnode_new_child(content->contacts_node, "Contact");
	succ &= ggp_xml_set_string(buddy_node, "Guid", purple_uuid_random());
	succ &= ggp_xml_set_uint(buddy_node, "GGNumber", uin);
	succ &= ggp_xml_set_string(buddy_node, "ShowName",
		purple_buddy_get_alias(buddy));
	
	contact_groups = xmlnode_new_child(buddy_node, "Groups");
	g_assert(contact_groups);
	succ &= ggp_xml_set_string(contact_groups, "GroupId", group_id);
	
	xmlnode_new_child(buddy_node, "Avatars");
	succ &= ggp_xml_set_bool(buddy_node, "FlagBuddy", TRUE);
	succ &= ggp_xml_set_bool(buddy_node, "FlagNormal", TRUE);
	succ &= ggp_xml_set_bool(buddy_node, "FlagFriend", TRUE);
	
	// we don't use Guid, so update is not needed
	//content->needs_update = TRUE;
	
	g_hash_table_insert(content->contact_nodes, GINT_TO_POINTER(uin),
		buddy_node);
	
	g_return_val_if_fail(succ, FALSE);
	
	return TRUE;
}

static gboolean ggp_roster_send_update_contact_remove(PurpleConnection *gc,
	ggp_roster_change *change)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_content *content = ggp_roster_get_rdata(gc)->content;
	uin_t uin = change->data.uin;
	PurpleBuddy *buddy;
	xmlnode *buddy_node;

	g_return_val_if_fail(change->type == GGP_ROSTER_CHANGE_CONTACT_REMOVE,
		FALSE);
	
	buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
	if (buddy)
	{
		purple_debug_info("gg", "ggp_roster_send_update_contact_remove:"
			" contact %u re-added\n", uin);
		return TRUE;
	}

	buddy_node = g_hash_table_lookup(content->contact_nodes,
		GINT_TO_POINTER(uin));
	if (!buddy_node) // already removed
		return TRUE;
	
	purple_debug_info("gg", "ggp_roster_send_update_contact_remove: "
		"removing %u\n", uin);
	xmlnode_free(buddy_node);
	g_hash_table_remove(content->contact_nodes, GINT_TO_POINTER(uin));
	
	return TRUE;
}

static gboolean ggp_roster_send_update_group_rename(PurpleConnection *gc,
	ggp_roster_change *change)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_content *content = ggp_roster_get_rdata(gc)->content;
	const char *old_name = change->data.group_rename.old_name;
	const char *new_name = change->data.group_rename.new_name;
	xmlnode *group_node;
	const char *group_id;

	g_return_val_if_fail(change->type == GGP_ROSTER_CHANGE_GROUP_RENAME,
		FALSE);
	
	purple_debug_misc("gg", "ggp_roster_send_update_group_rename: "
		"\"%s\"->\"%s\"\n", old_name, new_name);

	// moving to or from default group instead of renaming it
	if (0 == g_strcmp0(old_name, GGP_PURPLEW_GROUP_DEFAULT) ||
		0 == g_strcmp0(new_name, GGP_PURPLEW_GROUP_DEFAULT))
	{
		PurpleGroup *group;
		GList *group_buddies;
		group = purple_find_group(new_name);
		if (!group)
			return TRUE;
		purple_debug_info("gg", "ggp_roster_send_update_group_rename: "
			"invalidating buddies in default group\n");
		group_buddies = ggp_purplew_group_get_buddies(group, account);
		while (group_buddies)
		{
			ggp_roster_set_synchronized(gc, group_buddies->data,
				FALSE);
			group_buddies = g_list_remove_link(group_buddies,
				group_buddies);
		}
		return TRUE;
	}
	group_id = g_hash_table_lookup(content->group_ids, old_name);
	if (!group_id)
	{
		purple_debug_info("gg", "ggp_roster_send_update_group_rename: "
			"%s is not present at roster\n", old_name);
		return TRUE;
	}
	
	group_node = g_hash_table_lookup(content->group_nodes, group_id);
	if (!group_node)
	{
		purple_debug_error("gg", "ggp_roster_send_update_group_rename: "
			"node for %s not found, id=%s\n", old_name, group_id);
		g_hash_table_remove(content->group_ids, old_name);
		return TRUE;
	}
	
	g_hash_table_remove(content->group_ids, old_name);
	g_hash_table_insert(content->group_ids, g_strdup(new_name),
		g_strdup(group_id));
	g_hash_table_insert(content->group_nodes, g_strdup(group_id),
		group_node);
	return ggp_xml_set_string(group_node, "Name", new_name);
}

static void ggp_roster_send_update(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_content *content = rdata->content;
	GList *updates_it;
	gchar *str;
	int len;
	
	// an update is running now
	if (rdata->sent_updates)
		return;

	// no pending updates found
	if (!rdata->pending_updates)
		return;
	
	// not initialized
	if (!content)
		return;
	
	purple_debug_info("gg", "ggp_roster_send_update: "
		"pending updates found\n");
	
	rdata->sent_updates = rdata->pending_updates;
	rdata->pending_updates = NULL;
	
	updates_it = g_list_first(rdata->sent_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		gboolean succ = FALSE;
		updates_it = g_list_next(updates_it);
		
		if (change->type == GGP_ROSTER_CHANGE_CONTACT_UPDATE)
			succ = ggp_roster_send_update_contact_update(gc,
				change);
		else if (change->type == GGP_ROSTER_CHANGE_CONTACT_REMOVE)
			succ = ggp_roster_send_update_contact_remove(gc,
				change);
		else if (change->type == GGP_ROSTER_CHANGE_GROUP_RENAME)
			succ = ggp_roster_send_update_group_rename(gc, change);
		else
			purple_debug_fatal("gg", "ggp_roster_send_update: "
				"not handled\n");
		g_return_if_fail(succ);
	}
	
#if GGP_ROSTER_DEBUG
	ggp_roster_dump(content);
#endif
	
	str = xmlnode_to_str(content->xml, &len);
	gg_userlist100_request(accdata->session, GG_USERLIST100_PUT,
		content->version, GG_USERLIST100_FORMAT_TYPE_GG100, str);
	g_free(str);
}

static void ggp_roster_reply_ack(PurpleConnection *gc, uint32_t version)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	ggp_roster_content *content = rdata->content;
	GList *updates_it;

	purple_debug_info("gg", "ggp_roster_reply_ack: version=%u\n", version);
	
	// set synchronization flag for all buddies, that were updated at roster
	updates_it = g_list_first(rdata->sent_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		PurpleBuddy *buddy;
		updates_it = g_list_next(updates_it);
		
		if (change->type != GGP_ROSTER_CHANGE_CONTACT_UPDATE)
			continue;
		
		buddy = purple_find_buddy(account,
			ggp_uin_to_str(change->data.uin));
		if (buddy)
			ggp_roster_set_synchronized(gc, buddy, TRUE);
	}
	
	// we need to remove "synchronized" flag for all contacts, that have
	// beed modified between roster update start and now
	updates_it = g_list_first(rdata->pending_updates);
	while (updates_it)
	{
		ggp_roster_change *change = updates_it->data;
		PurpleBuddy *buddy;
		updates_it = g_list_next(updates_it);
		
		if (change->type != GGP_ROSTER_CHANGE_CONTACT_UPDATE)
			continue;
		
		buddy = purple_find_buddy(account,
			ggp_uin_to_str(change->data.uin));
		if (buddy && ggp_roster_is_synchronized(buddy))
			ggp_roster_set_synchronized(gc, buddy, FALSE);
	}
	
	g_list_free_full(rdata->sent_updates, ggp_roster_change_free);
	rdata->sent_updates = NULL;
	
	// bump roster version or update it, if needed
	g_return_if_fail(content != NULL);
	if (content->needs_update)
	{
		ggp_roster_content_free(rdata->content);
		rdata->content = NULL;
		// we have to wait for gg_event_userlist100_version
		//ggp_roster_request_update(gc);
	}
	else
		content->version = version;
}

static void ggp_roster_reply_reject(PurpleConnection *gc, uint32_t version)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	purple_debug_info("gg", "ggp_roster_reply_reject: version=%u\n",
		version);
	
	g_return_if_fail(rdata->sent_updates);
	
	rdata->pending_updates = g_list_concat(rdata->pending_updates,
		rdata->sent_updates);
	rdata->sent_updates = NULL;
	
	ggp_roster_content_free(rdata->content);
	rdata->content = NULL;
	ggp_roster_request_update(gc);
}

/******************************************************************************/


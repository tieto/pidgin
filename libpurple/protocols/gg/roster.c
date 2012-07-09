#include "roster.h"

#include "gg.h"
#include "xml.h"
#include "utils.h"

#include <debug.h>

#define GGP_ROSTER_SYNC_SETT "gg-synchronized"
#define GGP_ROSTER_DEBUG 1

static void ggp_roster_set_synchronized(PurpleBuddy *buddy, gboolean synchronized);
static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy);

static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version, const char *reply);

#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(PurpleConnection *gc);
#endif

/********/

static inline ggp_roster_session_data *
ggp_roster_get_rdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return &accdata->roster_data;
} 

void ggp_roster_setup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	rdata->xml = NULL;
	rdata->version = 0;
}

void ggp_roster_cleanup(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);

	if (rdata->xml)
		xmlnode_free(rdata->xml);
}

static void ggp_roster_set_synchronized(PurpleBuddy *buddy, gboolean synchronized)
{
	purple_blist_node_set_bool((PurpleBlistNode*)buddy, GGP_ROSTER_SYNC_SETT, synchronized);
}

static gboolean ggp_roster_is_synchronized(PurpleBuddy *buddy)
{
	return purple_blist_node_get_bool((PurpleBlistNode*)buddy, GGP_ROSTER_SYNC_SETT);
}

void ggp_roster_update(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	
	purple_debug_info("gg", "ggp_roster_update\n");
	
	if (!gg_libgadu_check_feature(GG_LIBGADU_FEATURE_USERLIST100))
	{
		purple_debug_error("gg", "ggp_roster_update - feature disabled\n");
		return;
	}
	
	gg_userlist100_request(accdata->session, GG_USERLIST100_GET, 0, GG_USERLIST100_FORMAT_TYPE_GG100, NULL);
}

void ggp_roster_reply(PurpleConnection *gc, struct gg_event_userlist100_reply *reply)
{
	purple_debug_info("gg", "ggp_roster_reply [type=%d, version=%u, format_type=%d]\n",
		reply->type, reply->version, reply->format_type);
	
	if (reply->type != GG_USERLIST100_REPLY_LIST)
		return;
	
	if (GG_USERLIST100_FORMAT_TYPE_GG100 != reply->format_type)
	{
		purple_debug_warning("gg", "ggp_buddylist_load100_reply: unsupported format type (%u)\n", reply->format_type);
		return;
	}
	
	ggp_roster_reply_list(gc, reply->version, reply->reply);
}

static void ggp_roster_reply_list(PurpleConnection *gc, uint32_t version, const char *data)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	xmlnode *xml, *curr_list, *curr_elem;
	GHashTable *groups;
	PurpleAccount *account;
	GSList *buddies;
	GHashTable *remove_buddies;
	GList *remove_buddies_list, *remove_buddies_it;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(data != NULL);

	account = purple_connection_get_account(gc);

	xml = xmlnode_from_str(data, -1);
	if (xml == NULL)
	{
		purple_debug_warning("gg", "ggp_roster_reply_list: invalid xml\n");
		return;
	}

	if (rdata->xml)
		xmlnode_free(rdata->xml);
	rdata->xml = xml;

#if GGP_ROSTER_DEBUG
	ggp_roster_dump(gc);
#endif

	// reading groups

	curr_list = xmlnode_get_child(xml, "Groups");
	g_return_if_fail(curr_list != NULL);

	groups = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	curr_elem = xmlnode_get_child(curr_list, "Group");
	while (curr_elem != NULL)
	{
		char *name, *id;
		gboolean removable;
		gboolean succ = TRUE;
		
		succ &= ggp_xml_get_string(curr_elem, "Id", &id);
		succ &= ggp_xml_get_string(curr_elem, "Name", &name);
		succ &= ggp_xml_get_bool(curr_elem, "IsRemovable", &removable);
		
		if (!succ)
		{
			g_free(id);
			g_free(name);
			g_hash_table_destroy(groups);
			g_return_if_reached();
		}
		
		if (removable)
		{
			g_hash_table_insert(groups, id, name);
			purple_debug_misc("gg", "ggp_roster_reply_list: group %s [id=%s]\n", name, id);
		}
		else
		{
			g_free(id);
			g_free(name);
		}
		
		curr_elem = xmlnode_get_next_twin(curr_elem);
	}
	
	// dumping current buddy list
	// we will remove them, if not found in list at server
	
	buddies = purple_find_buddies(account, NULL);
	remove_buddies = g_hash_table_new(g_direct_hash, g_direct_equal);
	while (buddies)
	{
		PurpleBuddy *buddy = buddies->data;
		uin_t uin = ggp_str_to_uin(purple_buddy_get_name(buddy));
		
		if (uin && ggp_roster_is_synchronized(buddy))
			g_hash_table_insert(remove_buddies, GINT_TO_POINTER(uin), buddy);

		buddies = g_slist_delete_link(buddies, buddies);
	}
	
	// reading buddies
	
	curr_list = xmlnode_get_child(xml, "Contacts");
	if (curr_list == NULL)
	{
		g_hash_table_destroy(groups);
		g_hash_table_destroy(remove_buddies);
		g_return_if_reached();
	}
	
	curr_elem = xmlnode_get_child(curr_list, "Contact");
	while (curr_elem != NULL)
	{
		gchar *alias, *group_name;
		uin_t uin;
		gboolean isbot, isbuddy, normal, friend; //TODO: czy potrzebne?
		gboolean succ = TRUE;
		xmlnode *group_list, *group_elem;
		PurpleBuddy *buddy = NULL;
		PurpleGroup *group = NULL;
		
		succ &= ggp_xml_get_string(curr_elem, "ShowName", &alias);
		succ &= ggp_xml_get_uint(curr_elem, "GGNumber", &uin);
		
		succ &= ggp_xml_get_bool(curr_elem, "FlagBuddy", &isbuddy);
		succ &= ggp_xml_get_bool(curr_elem, "FlagNormal", &normal);
		succ &= ggp_xml_get_bool(curr_elem, "FlagFriend", &friend);
		
		group_list = xmlnode_get_child(curr_elem, "Groups");
		succ &= (group_list != NULL);
		
		if (!succ)
		{
			g_free(alias);
			g_hash_table_destroy(groups);
			g_hash_table_destroy(remove_buddies);
			g_return_if_reached();
		}
		
		// looking up for group name
		group_elem = xmlnode_get_child(group_list, "GroupId");
		while (group_elem != NULL)
		{
			gchar *id;
			if (!ggp_xml_get_string(group_elem, NULL, &id))
				continue;
			group_name = g_hash_table_lookup(groups, id);
			isbot = (strcmp(id, "0b345af6-0001-0000-0000-000000000004") == 0 ||
				g_strcmp0(group_name, "Pomocnicy") == 0);
			g_free(id);
			
			if (isbot)
				break;
			
			if (group_name != NULL)
				break;
			
			group_elem = xmlnode_get_next_twin(group_elem);
		}
		
		// we don't want to import bots;
		// they are inserted to roster by default
		if (isbot)
		{
			g_free(alias);
			curr_elem = xmlnode_get_next_twin(curr_elem);
			continue;
		}
		
		if (strlen(alias) == 0 ||
			strcmp(alias, ggp_uin_to_str(uin)) == 0)
		{
			g_free(alias);
			alias = NULL;
		}
		
		purple_debug_misc("gg", "ggp_roster_reply_list: user [uin=%u, alias=\"%s\", group=\"%s\", buddy=%d, normal=%d, friend=%d]\n",
			uin, alias, group_name, isbuddy, normal, friend);
		
		if (group_name)
		{
			group = purple_find_group(group_name);
			if (!group)
			{ // TODO: group rename
				group = purple_group_new(group_name);
				purple_blist_add_group(group, NULL);
			}
		}
		
		buddy = purple_find_buddy(account, ggp_uin_to_str(uin));
		g_hash_table_remove(remove_buddies, GINT_TO_POINTER(uin));
		if (buddy)
		{
			PurpleGroup *currentGroup;
			
			// local list has priority
			if (!ggp_roster_is_synchronized(buddy))
			{
				purple_debug_info("gg", "ggp_roster_reply_list: ignoring not synchronized %s\n", purple_buddy_get_name(buddy));
				g_free(alias);
				curr_elem = xmlnode_get_next_twin(curr_elem);
				continue;
			}
			
			purple_debug_misc("gg", "ggp_roster_reply_list: updating %s\n", purple_buddy_get_name(buddy));
			purple_blist_alias_buddy(buddy, alias);
			currentGroup = purple_buddy_get_group(buddy);
			if (currentGroup != group)
				purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		else
		{
			purple_debug_info("gg", "ggp_roster_reply_list: adding %s to buddy list\n", purple_buddy_get_name(buddy));
			buddy = purple_buddy_new(account, ggp_uin_to_str(uin), alias);
			purple_blist_add_buddy(buddy, NULL, group, NULL);
		}
		ggp_roster_set_synchronized(buddy, TRUE);
		
		g_free(alias);
		curr_elem = xmlnode_get_next_twin(curr_elem);
	}
	
	g_hash_table_destroy(groups);
	
	// removing buddies, which are not present in roster
	remove_buddies_list = g_hash_table_get_values(remove_buddies);
	remove_buddies_it = g_list_first(remove_buddies_list);
	while (remove_buddies_it)
	{
		PurpleBuddy *buddy = remove_buddies_it->data;
		purple_debug_info("gg", "ggp_roster_reply_list: removing %s from buddy list\n", purple_buddy_get_name(buddy));
		purple_blist_remove_buddy(buddy);
		remove_buddies_it = g_list_next(remove_buddies_it);
	}
	
	g_list_free(remove_buddies_list);
	g_hash_table_destroy(remove_buddies);
}

#if GGP_ROSTER_DEBUG
static void ggp_roster_dump(PurpleConnection *gc)
{
	ggp_roster_session_data *rdata = ggp_roster_get_rdata(gc);
	char *str;
	int len;
	
	g_return_if_fail(rdata->xml != NULL);
	
	str = xmlnode_to_formatted_str(rdata->xml, &len);
	purple_debug_misc("gg", "ggp_roster_reply_list: [%s]\n", str);
	g_free(str);
}
#endif

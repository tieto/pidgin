/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

#include "visibility.h"

struct visibility_cb_data
{
	guint16 list_type;
	gboolean add_to_list;
};

static void
visibility_cb(PurpleBlistNode *node, struct visibility_cb_data *data)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(node);
	const char* bname = purple_buddy_get_name(buddy);
	OscarData *od = purple_account_get_connection(purple_buddy_get_account(buddy))->proto_data;

	if (data->add_to_list) {
		aim_ssi_add_to_private_list(od, bname, data->list_type);
	} else {
		aim_ssi_del_from_private_list(od, bname, data->list_type);
	}

	g_free(data);
}

PurpleMenuAction *
create_visibility_menu_item(OscarData *od, const char *bname)
{
	PurpleAccount *account = purple_connection_get_account(od->gc);
	gboolean invisible = purple_account_is_status_active(account, OSCAR_STATUS_ID_INVISIBLE);
	guint16 list_type = invisible ? AIM_SSI_TYPE_PERMIT : AIM_SSI_TYPE_DENY;
	gboolean on_list = aim_ssi_itemlist_finditem(od->ssi.local, NULL, bname, list_type) != NULL;
	gchar *label;
	struct visibility_cb_data *data;
	PurpleMenuAction *result;

	data = g_new0(struct visibility_cb_data, 1);
	data->list_type = list_type;
	data->add_to_list = !on_list;

	label = g_strdup_printf("%s %s", on_list ? "Don't appear" : "Appear", invisible ? "Online" : "Offline");
	result = purple_menu_action_new(label, PURPLE_CALLBACK(visibility_cb), data, NULL);
	g_free(label);
	return result;
}

static void
show_private_list(PurplePluginAction *action, guint16 list_type, const gchar *list_description, const gchar *menu_action_name)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	OscarData *od = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	GSList *buddies, *filtered_buddies, *cur;
	gchar *text, *secondary;

	buddies = purple_find_buddies(account, NULL);
	filtered_buddies = NULL;
	for (cur = buddies; cur != NULL; cur = cur->next) {
		PurpleBuddy *buddy;
		const gchar *bname;

		buddy = cur->data;
		bname = purple_buddy_get_name(buddy);
		if (aim_ssi_itemlist_finditem(od->ssi.local, NULL, bname, list_type)) {
			filtered_buddies = g_slist_prepend(filtered_buddies, buddy);
		}
	}

	g_slist_free(buddies);

	filtered_buddies = g_slist_reverse(filtered_buddies);
	text = oscar_format_buddies(filtered_buddies, "you have no buddies on this list");
	g_slist_free(filtered_buddies);

	secondary = g_strdup_printf("You can add a buddy to this list "
					"by right-clicking on them and "
					"selecting \"%s\"", menu_action_name);
	purple_notify_formatted(gc, NULL, list_description, secondary, text, NULL, NULL);
	g_free(secondary);
	g_free(text);
}

void
oscar_show_visible_list(PurplePluginAction *action)
{
	show_private_list(action, AIM_SSI_TYPE_PERMIT, "These buddies will always see "
							"your status, even when you switch "
							"to \"Invisible\"", "Appear Online");
}

void
oscar_show_invisible_list(PurplePluginAction *action)
{
	show_private_list(action, AIM_SSI_TYPE_DENY, "These buddies will always see you as offline", "Appear Offline");
}
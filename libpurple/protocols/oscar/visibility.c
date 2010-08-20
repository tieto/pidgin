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
#include "request.h"

/* 4 separate strings are needed in order to ease translators' job */
#define APPEAR_ONLINE		N_("Appear Online")
#define DONT_APPEAR_ONLINE	N_("Don't Appear Online")
#define APPEAR_OFFLINE		N_("Appear Offline")
#define DONT_APPEAR_OFFLINE	N_("Don't Appear Offline")

static guint16
get_buddy_list_type(OscarData *od)
{
	PurpleAccount *account = purple_connection_get_account(od->gc);
	return purple_account_is_status_active(account, OSCAR_STATUS_ID_INVISIBLE) ? AIM_SSI_TYPE_PERMIT : AIM_SSI_TYPE_DENY;
}

static gboolean
is_buddy_on_list(OscarData *od, const char *bname)
{
	return aim_ssi_itemlist_finditem(od->ssi.local, NULL, bname, get_buddy_list_type(od)) != NULL;
}

static void
visibility_cb(PurpleBlistNode *node, gpointer whatever)
{
	PurpleBuddy *buddy = PURPLE_BUDDY(node);
	const char* bname = purple_buddy_get_name(buddy);
	OscarData *od = purple_connection_get_protocol_data(purple_account_get_connection(purple_buddy_get_account(buddy)));
	guint16 list_type = get_buddy_list_type(od);

	if (!is_buddy_on_list(od, bname)) {
		aim_ssi_add_to_private_list(od, bname, list_type);
	} else {
		aim_ssi_del_from_private_list(od, bname, list_type);
	}
}

PurpleMenuAction *
create_visibility_menu_item(OscarData *od, const char *bname)
{
	PurpleAccount *account = purple_connection_get_account(od->gc);
	gboolean invisible = purple_account_is_status_active(account, OSCAR_STATUS_ID_INVISIBLE);
	gboolean on_list = is_buddy_on_list(od, bname);
	const gchar *label;

	if (invisible) {
		label = on_list ? _(DONT_APPEAR_ONLINE) : _(APPEAR_ONLINE);
	} else {
		label = on_list ? _(DONT_APPEAR_OFFLINE) : _(APPEAR_OFFLINE);
	}
	return purple_menu_action_new(label, PURPLE_CALLBACK(visibility_cb), NULL, NULL);
}

typedef void (*ShowDialog)(PurplePluginAction *);

struct list_remove_data
{
	PurplePluginAction *action;
	ShowDialog show_dialog_again;
	OscarData *od;
	guint16 list_type;
	const gchar *list_name;
};

static void
list_remove_cb(struct list_remove_data *data, PurpleRequestFields *fields)
{
	ShowDialog show_dialog_again = data->show_dialog_again;
	PurplePluginAction *action = data->action;
	PurpleRequestField *field = purple_request_fields_get_field(fields, "list-items");
	GList *sels = purple_request_field_list_get_selected(field);
	for (; sels; sels = sels->next) {
		const gchar *name = sels->data;
		const gchar *bname = purple_request_field_list_get_data(field, name);

		purple_debug_info("oscar", "Removing %s from %s\n", bname, data->list_name);

		aim_ssi_del_from_private_list(data->od, bname, data->list_type);
	}

	g_free(data);
	show_dialog_again(action);
}

static void
list_close_cb(struct list_remove_data *data, gpointer ignored)
{
	g_free(data);
}

static void
show_private_list(PurplePluginAction *action,
		  guint16 list_type,
		  const gchar *list_name,
		  const gchar *list_description,
		  const gchar *menu_action_name,
		  ShowDialog caller)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	OscarData *od = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	GSList *buddies, *cur;
	gchar *desc;
	struct list_remove_data *data;

	PurpleRequestField *field;
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);

	purple_request_fields_add_group(fields, group);

	desc = g_strdup_printf(_("You can add a buddy to this list "
				 "by right-clicking on them and "
				 "selecting \"%s\""), menu_action_name);

	field = purple_request_field_list_new("list-items", desc);
	g_free(desc);

	purple_request_field_group_add_field(group, field);

	purple_request_field_list_set_multi_select(field, TRUE);
	purple_request_field_set_required(field, TRUE);

	buddies = purple_find_buddies(account, NULL);
	for (cur = buddies; cur != NULL; cur = cur->next) {
		PurpleBuddy *buddy;
		const gchar *bname;

		buddy = cur->data;
		bname = purple_buddy_get_name(buddy);
		if (aim_ssi_itemlist_finditem(od->ssi.local, NULL, bname, list_type)) {
			const gchar *alias = purple_buddy_get_alias_only(buddy);
			char *dname = alias ? g_strdup_printf("%s (%s)", bname, alias) : NULL;
			purple_request_field_list_add(field, dname ? dname : bname, (void *)bname);
			g_free(dname);
		}
	}

	g_slist_free(buddies);

	data = g_new0(struct list_remove_data, 1);
	data->action = action;
	data->show_dialog_again = caller;
	data->od = od;
	data->list_type = list_type;
	data->list_name = list_name;

	purple_request_fields(gc, list_name, list_description, NULL,
			      fields,
			      _("Close"), G_CALLBACK(list_close_cb),
			      _("Remove"), G_CALLBACK(list_remove_cb),
			      account, NULL, NULL,
			      data);
}

void
oscar_show_visible_list(PurplePluginAction *action)
{
	show_private_list(action,
			  AIM_SSI_TYPE_PERMIT,
			  _("Visible List"),
			  _("These buddies will see "
			    "your status when you switch "
			    "to \"Invisible\""),
			  _(APPEAR_ONLINE),
			  oscar_show_visible_list);
}

void
oscar_show_invisible_list(PurplePluginAction *action)
{
	show_private_list(action,
			  AIM_SSI_TYPE_DENY,
			  _("Invisible List"),
			  _("These buddies will always see you as offline"),
			  _(APPEAR_OFFLINE),
			  oscar_show_invisible_list);
}


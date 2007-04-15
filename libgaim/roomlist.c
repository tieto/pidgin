/**
 * @file roomlist.c Room List API
 * @ingroup core
 *
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
 */

#include <glib.h>

#include "account.h"
#include "connection.h"
#include "debug.h"
#include "roomlist.h"
#include "server.h"


static GaimRoomlistUiOps *ops = NULL;

/**************************************************************************/
/** @name Room List API                                                   */
/**************************************************************************/
/*@{*/

void gaim_roomlist_show_with_account(GaimAccount *account)
{
	if (ops && ops->show_with_account)
		ops->show_with_account(account);
}

GaimRoomlist *gaim_roomlist_new(GaimAccount *account)
{
	GaimRoomlist *list;

	g_return_val_if_fail(account != NULL, NULL);

	list = g_new0(GaimRoomlist, 1);
	list->account = account;
	list->rooms = NULL;
	list->fields = NULL;
	list->ref = 1;

	if (ops && ops->create)
		ops->create(list);

	return list;
}

void gaim_roomlist_ref(GaimRoomlist *list)
{
	g_return_if_fail(list != NULL);

	list->ref++;
	gaim_debug_misc("roomlist", "reffing list, ref count now %d\n", list->ref);
}

static void gaim_roomlist_room_destroy(GaimRoomlist *list, GaimRoomlistRoom *r)
{
	GList *l, *j;

	for (l = list->fields, j = r->fields; l && j; l = l->next, j = j->next) {
		GaimRoomlistField *f = l->data;
		if (f->type == GAIM_ROOMLIST_FIELD_STRING)
			g_free(j->data);
	}

	g_list_free(r->fields);
	g_free(r->name);
	g_free(r);
}

static void gaim_roomlist_field_destroy(GaimRoomlistField *f)
{
	g_free(f->label);
	g_free(f->name);
	g_free(f);
}

static void gaim_roomlist_destroy(GaimRoomlist *list)
{
	GList *l;

	gaim_debug_misc("roomlist", "destroying list %p\n", list);

	if (ops && ops->destroy)
		ops->destroy(list);

	for (l = list->rooms; l; l = l->next) {
		GaimRoomlistRoom *r = l->data;
		gaim_roomlist_room_destroy(list, r);
	}
	g_list_free(list->rooms);

	g_list_foreach(list->fields, (GFunc)gaim_roomlist_field_destroy, NULL);
	g_list_free(list->fields);

	g_free(list);
}

void gaim_roomlist_unref(GaimRoomlist *list)
{
	g_return_if_fail(list != NULL);
	g_return_if_fail(list->ref > 0);

	list->ref--;

	gaim_debug_misc("roomlist", "unreffing list, ref count now %d\n", list->ref);
	if (list->ref == 0)
		gaim_roomlist_destroy(list);
}

void gaim_roomlist_set_fields(GaimRoomlist *list, GList *fields)
{
	g_return_if_fail(list != NULL);

	list->fields = fields;

	if (ops && ops->set_fields)
		ops->set_fields(list, fields);
}

void gaim_roomlist_set_in_progress(GaimRoomlist *list, gboolean in_progress)
{
	g_return_if_fail(list != NULL);

	list->in_progress = in_progress;

	if (ops && ops->in_progress)
		ops->in_progress(list, in_progress);
}

gboolean gaim_roomlist_get_in_progress(GaimRoomlist *list)
{
	g_return_val_if_fail(list != NULL, FALSE);

	return list->in_progress;
}

void gaim_roomlist_room_add(GaimRoomlist *list, GaimRoomlistRoom *room)
{
	g_return_if_fail(list != NULL);
	g_return_if_fail(room != NULL);

	list->rooms = g_list_append(list->rooms, room);

	if (ops && ops->add_room)
		ops->add_room(list, room);
}

GaimRoomlist *gaim_roomlist_get_list(GaimConnection *gc)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	g_return_val_if_fail(gc != NULL, NULL);

	if (gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->roomlist_get_list)
		return prpl_info->roomlist_get_list(gc);
	return NULL;
}

void gaim_roomlist_cancel_get_list(GaimRoomlist *list)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc;

	g_return_if_fail(list != NULL);

	gc = gaim_account_get_connection(list->account);

	g_return_if_fail(gc != NULL);

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->roomlist_cancel)
		prpl_info->roomlist_cancel(list);
}

void gaim_roomlist_expand_category(GaimRoomlist *list, GaimRoomlistRoom *category)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConnection *gc;

	g_return_if_fail(list != NULL);
	g_return_if_fail(category != NULL);
	g_return_if_fail(category->type & GAIM_ROOMLIST_ROOMTYPE_CATEGORY);

	gc = gaim_account_get_connection(list->account);
	g_return_if_fail(gc != NULL);

	if (gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->roomlist_expand_category)
		prpl_info->roomlist_expand_category(list, category);
}

/*@}*/

/**************************************************************************/
/** @name Room API                                                        */
/**************************************************************************/
/*@{*/

GaimRoomlistRoom *gaim_roomlist_room_new(GaimRoomlistRoomType type, const gchar *name,
                                         GaimRoomlistRoom *parent)
{
	GaimRoomlistRoom *room;

	g_return_val_if_fail(name != NULL, NULL);

	room = g_new0(GaimRoomlistRoom, 1);
	room->type = type;
	room->name = g_strdup(name);
	room->parent = parent;

	return room;
}

void gaim_roomlist_room_add_field(GaimRoomlist *list, GaimRoomlistRoom *room, gconstpointer field)
{
	GaimRoomlistField *f;

	g_return_if_fail(list != NULL);
	g_return_if_fail(room != NULL);
	g_return_if_fail(list->fields != NULL);

	if (!room->fields)
		f = list->fields->data;
	else
		f = g_list_nth_data(list->fields, g_list_length(room->fields));

	g_return_if_fail(f != NULL);

	switch(f->type) {
		case GAIM_ROOMLIST_FIELD_STRING:
			room->fields = g_list_append(room->fields, g_strdup(field));
			break;
		case GAIM_ROOMLIST_FIELD_BOOL:
		case GAIM_ROOMLIST_FIELD_INT:
			room->fields = g_list_append(room->fields, GINT_TO_POINTER(field));
			break;
	}
}

void gaim_roomlist_room_join(GaimRoomlist *list, GaimRoomlistRoom *room)
{
	GHashTable *components;
	GList *l, *j;
	GaimConnection *gc;

	g_return_if_fail(list != NULL);
	g_return_if_fail(room != NULL);

	gc = gaim_account_get_connection(list->account);
	if (!gc)
		return;

	components = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_replace(components, "name", room->name);
	for (l = list->fields, j = room->fields; l && j; l = l->next, j = j->next) {
		GaimRoomlistField *f = l->data;

		g_hash_table_replace(components, f->name, j->data);
	}

	serv_join_chat(gc, components);

	g_hash_table_destroy(components);
}

/*@}*/

/**************************************************************************/
/** @name Room Field API                                                  */
/**************************************************************************/
/*@{*/

GaimRoomlistField *gaim_roomlist_field_new(GaimRoomlistFieldType type,
                                           const gchar *label, const gchar *name,
                                           gboolean hidden)
{
	GaimRoomlistField *f;

	g_return_val_if_fail(label != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	f = g_new0(GaimRoomlistField, 1);

	f->type = type;
	f->label = g_strdup(label);
	f->name = g_strdup(name);
	f->hidden = hidden;

	return f;
}

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/


void gaim_roomlist_set_ui_ops(GaimRoomlistUiOps *ui_ops)
{
	ops = ui_ops;
}

GaimRoomlistUiOps *gaim_roomlist_get_ui_ops(void)
{
	return ops;
}

/*@}*/

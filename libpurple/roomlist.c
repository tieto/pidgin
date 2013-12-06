/**
 * @file roomlist.c Room List API
 * @ingroup core
 */
/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "glibcompat.h"

#include "account.h"
#include "connection.h"
#include "debug.h"
#include "roomlist.h"
#include "server.h"

#define PURPLE_ROOMLIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_ROOMLIST, PurpleRoomlistPrivate))

/** @copydoc _PurpleRoomlistPrivate */
typedef struct _PurpleRoomlistPrivate  PurpleRoomlistPrivate;

/**
 * Private data for a room list.
 */
struct _PurpleRoomlistPrivate {
	PurpleAccount *account;  /**< The account this list belongs to. */
	GList *fields;           /**< The fields.                       */
	GList *rooms;            /**< The list of rooms.                */
	gboolean in_progress;    /**< The listing is in progress.       */

	/* TODO Remove this and use protocol-specific subclasses. */
	gpointer proto_data;     /** Protocol private data.             */
};

/**
 * Represents a room.
 */
struct _PurpleRoomlistRoom {
	PurpleRoomlistRoomType type; /**< The type of room. */
	gchar *name; /**< The name of the room. */
	GList *fields; /**< Other fields. */
	PurpleRoomlistRoom *parent; /**< The parent room, or NULL. */
	gboolean expanded_once; /**< A flag the UI uses to avoid multiple expand protocol cbs. */
};

/**
 * A field a room might have.
 */
struct _PurpleRoomlistField {
	PurpleRoomlistFieldType type; /**< The type of field. */
	gchar *label; /**< The i18n user displayed name of the field. */
	gchar *name; /**< The internal name of the field. */
	gboolean hidden; /**< Hidden? */
};

/* Room list property enums */
enum
{
	PROP_0,
	PROP_ACCOUNT,
	PROP_FIELDS,
	PROP_IN_PROGRESS,
	PROP_LAST
};

static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];
static PurpleRoomlistUiOps *ops = NULL;

static void purple_roomlist_field_free(PurpleRoomlistField *f);
static void purple_roomlist_room_destroy(PurpleRoomlist *list, PurpleRoomlistRoom *r);

/**************************************************************************/
/** @name Room List API                                                   */
/**************************************************************************/
/*@{*/

void purple_roomlist_show_with_account(PurpleAccount *account)
{
	if (ops && ops->show_with_account)
		ops->show_with_account(account);
}

PurpleAccount *purple_roomlist_get_account(PurpleRoomlist *list)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->account;
}

void purple_roomlist_set_fields(PurpleRoomlist *list, GList *fields)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_if_fail(priv != NULL);

	priv->fields = fields;

	if (ops && ops->set_fields)
		ops->set_fields(list, fields);

	g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_FIELDS]);
}

void purple_roomlist_set_in_progress(PurpleRoomlist *list, gboolean in_progress)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_if_fail(priv != NULL);

	priv->in_progress = in_progress;

	if (ops && ops->in_progress)
		ops->in_progress(list, in_progress);

	g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_IN_PROGRESS]);
}

gboolean purple_roomlist_get_in_progress(PurpleRoomlist *list)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, FALSE);

	return priv->in_progress;
}

void purple_roomlist_room_add(PurpleRoomlist *list, PurpleRoomlistRoom *room)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(room != NULL);

	priv->rooms = g_list_append(priv->rooms, room);

	if (ops && ops->add_room)
		ops->add_room(list, room);
}

PurpleRoomlist *purple_roomlist_get_list(PurpleConnection *gc)
{
	PurpleProtocol *protocol = NULL;

	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);
	g_return_val_if_fail(PURPLE_CONNECTION_IS_CONNECTED(gc), NULL);

	protocol = purple_connection_get_protocol(gc);

	if(protocol)
		return purple_protocol_roomlist_iface_get_list(protocol, gc);

	return NULL;
}

void purple_roomlist_cancel_get_list(PurpleRoomlist *list)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;

	g_return_if_fail(priv != NULL);

	gc = purple_account_get_connection(priv->account);

	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	if(gc)
		protocol = purple_connection_get_protocol(gc);

	if(protocol)
		purple_protocol_roomlist_iface_cancel(protocol, list);
}

void purple_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(category != NULL);
	g_return_if_fail(category->type & PURPLE_ROOMLIST_ROOMTYPE_CATEGORY);

	gc = purple_account_get_connection(priv->account);
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	if(gc)
		protocol = purple_connection_get_protocol(gc);

	if(protocol)
		purple_protocol_roomlist_iface_expand_category(protocol, list, category);
}

GList * purple_roomlist_get_fields(PurpleRoomlist *list)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->fields;
}

gpointer purple_roomlist_get_protocol_data(PurpleRoomlist *list)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->proto_data;
}

void purple_roomlist_set_protocol_data(PurpleRoomlist *list, gpointer proto_data)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	g_return_if_fail(priv != NULL);

	priv->proto_data = proto_data;
}

gpointer purple_roomlist_get_ui_data(PurpleRoomlist *list)
{
	g_return_val_if_fail(PURPLE_IS_ROOMLIST(list), NULL);

	return list->ui_data;
}

void purple_roomlist_set_ui_data(PurpleRoomlist *list, gpointer ui_data)
{
	g_return_if_fail(PURPLE_IS_ROOMLIST(list));

	list->ui_data = ui_data;
}

/*@}*/

/**************************************************************************/
/** @name Room List GObject code                                          */
/**************************************************************************/
/*@{*/

/* Set method for GObject properties */
static void
purple_roomlist_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleRoomlist *list = PURPLE_ROOMLIST(obj);
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);

	switch (param_id) {
		case PROP_ACCOUNT:
			priv->account = g_value_get_object(value);
			break;
		case PROP_FIELDS:
			purple_roomlist_set_fields(list, g_value_get_pointer(value));
			break;
		case PROP_IN_PROGRESS:
			purple_roomlist_set_in_progress(list, g_value_get_boolean(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_roomlist_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleRoomlist *list = PURPLE_ROOMLIST(obj);

	switch (param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_roomlist_get_account(list));
			break;
		case PROP_FIELDS:
			g_value_set_pointer(value, purple_roomlist_get_fields(list));
			break;
		case PROP_IN_PROGRESS:
			g_value_set_boolean(value, purple_roomlist_get_in_progress(list));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Called when done constructing */
static void
purple_roomlist_constructed(GObject *object)
{
	PurpleRoomlist *list = PURPLE_ROOMLIST(object);

	parent_class->constructed(object);

	if (ops && ops->create)
		ops->create(list);
}

/* GObject finalize function */
static void
purple_roomlist_finalize(GObject *object)
{
	PurpleRoomlist *list = PURPLE_ROOMLIST(object);
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	GList *l;

	purple_debug_misc("roomlist", "destroying list %p\n", list);

	if (ops && ops->destroy)
		ops->destroy(list);

	for (l = priv->rooms; l; l = l->next) {
		PurpleRoomlistRoom *r = l->data;
		purple_roomlist_room_destroy(list, r);
	}
	g_list_free(priv->rooms);

	g_list_foreach(priv->fields, (GFunc)purple_roomlist_field_free, NULL);
	g_list_free(priv->fields);

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_roomlist_class_init(PurpleRoomlistClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_roomlist_finalize;
	obj_class->constructed = purple_roomlist_constructed;

	/* Setup properties */
	obj_class->get_property = purple_roomlist_get_property;
	obj_class->set_property = purple_roomlist_set_property;

	g_type_class_add_private(klass, sizeof(PurpleRoomlistPrivate));

	properties[PROP_ACCOUNT] = g_param_spec_object("account", "Account",
				"The account for the room list.",
				PURPLE_TYPE_ACCOUNT,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_FIELDS] = g_param_spec_pointer("fields", "Fields",
				"The list of fields for a roomlist.",
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_IN_PROGRESS] = g_param_spec_boolean("in-progress",
				"In progress",
				"Whether the room list is being fetched.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_roomlist_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleRoomlistClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_roomlist_class_init,
			NULL,
			NULL,
			sizeof(PurpleRoomlist),
			0,
			NULL,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleRoomlist",
				&info, 0);
	}

	return type;
}

PurpleRoomlist *purple_roomlist_new(PurpleAccount *account)
{
	PurpleRoomlist *list;
	PurpleProtocol *protocol;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	protocol = purple_protocols_find(purple_account_get_protocol_id(account));

	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	if (PURPLE_PROTOCOL_IMPLEMENTS(protocol, FACTORY_IFACE, roomlist_new))
		list = purple_protocol_factory_iface_roomlist_new(protocol, account);
	else
		list = g_object_new(PURPLE_TYPE_ROOMLIST,
			"account", account,
			NULL
		);

	g_return_val_if_fail(list != NULL, NULL);

	return list;
}

/*@}*/

/**************************************************************************/
/** @name Room API                                                        */
/**************************************************************************/
/*@{*/

PurpleRoomlistRoom *purple_roomlist_room_new(PurpleRoomlistRoomType type, const gchar *name,
                                         PurpleRoomlistRoom *parent)
{
	PurpleRoomlistRoom *room;

	g_return_val_if_fail(name != NULL, NULL);

	room = g_new0(PurpleRoomlistRoom, 1);
	room->type = type;
	room->name = g_strdup(name);
	room->parent = parent;

	return room;
}

void purple_roomlist_room_add_field(PurpleRoomlist *list, PurpleRoomlistRoom *room, gconstpointer field)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	PurpleRoomlistField *f;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(room != NULL);
	g_return_if_fail(priv->fields != NULL);

	/* If this is the first call for this room, grab the first field in
	 * the Roomlist's fields.  Otherwise, grab the field that is one
	 * more than the number of fields already present for the room.
         * (This works because g_list_nth_data() is zero-indexed and
         * g_list_length() is one-indexed.) */
	if (!room->fields)
		f = priv->fields->data;
	else
		f = g_list_nth_data(priv->fields, g_list_length(room->fields));

	g_return_if_fail(f != NULL);

	switch(f->type) {
		case PURPLE_ROOMLIST_FIELD_STRING:
			room->fields = g_list_append(room->fields, g_strdup(field));
			break;
		case PURPLE_ROOMLIST_FIELD_BOOL:
		case PURPLE_ROOMLIST_FIELD_INT:
			room->fields = g_list_append(room->fields, GINT_TO_POINTER(field));
			break;
	}

	g_object_notify_by_pspec(G_OBJECT(list), properties[PROP_FIELDS]);
}

void purple_roomlist_room_join(PurpleRoomlist *list, PurpleRoomlistRoom *room)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	GHashTable *components;
	GList *l, *j;
	PurpleConnection *gc;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(room != NULL);

	gc = purple_account_get_connection(priv->account);
	if (!gc)
		return;

	components = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_replace(components, "name", room->name);
	for (l = priv->fields, j = room->fields; l && j; l = l->next, j = j->next) {
		PurpleRoomlistField *f = l->data;

		g_hash_table_replace(components, f->name, j->data);
	}

	serv_join_chat(gc, components);

	g_hash_table_destroy(components);
}

PurpleRoomlistRoomType purple_roomlist_room_get_room_type(PurpleRoomlistRoom *room)
{
	return room->type;
}

const char * purple_roomlist_room_get_name(PurpleRoomlistRoom *room)
{
	return room->name;
}

PurpleRoomlistRoom * purple_roomlist_room_get_parent(PurpleRoomlistRoom *room)
{
	return room->parent;
}

gboolean purple_roomlist_room_get_expanded_once(PurpleRoomlistRoom *room)
{
	g_return_val_if_fail(room != NULL, FALSE);

	return room->expanded_once;
}

void purple_roomlist_room_set_expanded_once(PurpleRoomlistRoom *room, gboolean expanded_once)
{
	g_return_if_fail(room != NULL);

	room->expanded_once = expanded_once;
}

GList *purple_roomlist_room_get_fields(PurpleRoomlistRoom *room)
{
	return room->fields;
}

static void purple_roomlist_room_destroy(PurpleRoomlist *list, PurpleRoomlistRoom *r)
{
	PurpleRoomlistPrivate *priv = PURPLE_ROOMLIST_GET_PRIVATE(list);
	GList *l, *j;

	for (l = priv->fields, j = r->fields; l && j; l = l->next, j = j->next) {
		PurpleRoomlistField *f = l->data;
		if (f->type == PURPLE_ROOMLIST_FIELD_STRING)
			g_free(j->data);
	}

	g_list_free(r->fields);
	g_free(r->name);
	g_free(r);
}

/*@}*/

/**************************************************************************/
/** @name Room GBoxed code                                                */
/**************************************************************************/
/*@{*/

static PurpleRoomlistRoom *purple_roomlist_room_copy(PurpleRoomlistRoom *r)
{
	g_return_val_if_fail(r != NULL, NULL);

	return purple_roomlist_room_new(r->type, r->name, r->parent);
}

static void purple_roomlist_room_free(PurpleRoomlistRoom *r)
{
	g_return_if_fail(r != NULL);

	g_list_free(r->fields);
	g_free(r->name);
	g_free(r);
}

GType purple_roomlist_room_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleRoomlistRoom",
				(GBoxedCopyFunc)purple_roomlist_room_copy,
				(GBoxedFreeFunc)purple_roomlist_room_free);
	}

	return type;
}

/*@}*/

/**************************************************************************/
/** @name Room Field API                                                  */
/**************************************************************************/
/*@{*/

PurpleRoomlistField *purple_roomlist_field_new(PurpleRoomlistFieldType type,
                                           const gchar *label, const gchar *name,
                                           gboolean hidden)
{
	PurpleRoomlistField *f;

	g_return_val_if_fail(label != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	f = g_new0(PurpleRoomlistField, 1);

	f->type = type;
	f->label = g_strdup(label);
	f->name = g_strdup(name);
	f->hidden = hidden;

	return f;
}

PurpleRoomlistFieldType purple_roomlist_field_get_field_type(PurpleRoomlistField *field)
{
	return field->type;
}

const char * purple_roomlist_field_get_label(PurpleRoomlistField *field)
{
	return field->label;
}

gboolean purple_roomlist_field_get_hidden(PurpleRoomlistField *field)
{
	return field->hidden;
}

/*@}*/

/**************************************************************************/
/** @name Room Field GBoxed code                                          */
/**************************************************************************/
/*@{*/

static PurpleRoomlistField *purple_roomlist_field_copy(PurpleRoomlistField *f)
{
	g_return_val_if_fail(f != NULL, NULL);

	return purple_roomlist_field_new(f->type, f->label, f->name, f->hidden);
}

static void purple_roomlist_field_free(PurpleRoomlistField *f)
{
	g_return_if_fail(f != NULL);

	g_free(f->label);
	g_free(f->name);
	g_free(f);
}

GType purple_roomlist_field_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleRoomlistField",
				(GBoxedCopyFunc)purple_roomlist_field_copy,
				(GBoxedFreeFunc)purple_roomlist_field_free);
	}

	return type;
}

/*@}*/

/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/


void purple_roomlist_set_ui_ops(PurpleRoomlistUiOps *ui_ops)
{
	ops = ui_ops;
}

PurpleRoomlistUiOps *purple_roomlist_get_ui_ops(void)
{
	return ops;
}

/*@}*/

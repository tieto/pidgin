/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "blistnodetypes.h"
#include "internal.h"

#define PURPLE_BLIST_NODE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BLIST_NODE, PurpleBListNodePrivate))

/** @copydoc _PurpleBListNodePrivate */
typedef struct _PurpleBListNodePrivate  PurpleBListNodePrivate;

/** Private data of a buddy list node */
struct _PurpleBListNodePrivate {
	GHashTable *settings;  /**< per-node settings                            */
	gboolean dont_save;    /**< node should not be saved with the buddy list */
};

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_DONT_SAVE,
	PROP_LAST
};

static GObjectClass *parent_class;

/**************************************************************************/
/* Buddy list node API                                                    */
/**************************************************************************/

static PurpleBListNode *get_next_node(PurpleBListNode *node, gboolean godeep)
{
	if (node == NULL)
		return NULL;

	if (godeep && node->child)
		return node->child;

	if (node->next)
		return node->next;

	return get_next_node(node->parent, FALSE);
}

PurpleBListNode *purple_blist_node_next(PurpleBListNode *node, gboolean offline)
{
	PurpleBListNode *ret = node;

	if (offline)
		return get_next_node(ret, TRUE);
	do
	{
		ret = get_next_node(ret, TRUE);
	} while (ret && PURPLE_IS_BUDDY(ret) &&
			!purple_account_is_connected(purple_buddy_get_account((PurpleBuddy *)ret)));

	return ret;
}

PurpleBListNode *purple_blist_node_get_parent(PurpleBListNode *node)
{
	return node ? node->parent : NULL;
}

PurpleBListNode *purple_blist_node_get_first_child(PurpleBListNode *node)
{
	return node ? node->child : NULL;
}

PurpleBListNode *purple_blist_node_get_sibling_next(PurpleBListNode *node)
{
	return node? node->next : NULL;
}

PurpleBListNode *purple_blist_node_get_sibling_prev(PurpleBListNode *node)
{
	return node? node->prev : NULL;
}

void *
purple_blist_node_get_ui_data(const PurpleBListNode *node)
{
	g_return_val_if_fail(node, NULL);

	return node->ui_data;
}

void
purple_blist_node_set_ui_data(PurpleBListNode *node, void *ui_data) {
	g_return_if_fail(node);

	node->ui_data = ui_data;
}

void purple_blist_node_remove_setting(PurpleBListNode *node, const char *key)
{
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	g_hash_table_remove(priv->settings, key);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

void
purple_blist_node_set_dont_save(PurpleBListNode *node, gboolean dont_save)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);

	priv->dont_save = dont_save;
}

gboolean
purple_blist_node_get_dont_save(PurpleBListNode *node)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->dont_save;
}

gboolean
purple_blist_node_has_setting(PurpleBListNode* node, const char *key)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->settings != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	/* Boxed type, so it won't ever be NULL, so no need for _extended */
	return (g_hash_table_lookup(priv->settings, key) != NULL);
}

void
purple_blist_node_set_bool(PurpleBListNode* node, const char *key, gboolean data)
{
	GValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_BOOLEAN);
	g_value_set_boolean(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

gboolean
purple_blist_node_get_bool(PurpleBListNode* node, const char *key)
{
	GValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->settings != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return FALSE;

	g_return_val_if_fail(G_VALUE_HOLDS_BOOLEAN(value), FALSE);

	return g_value_get_boolean(value);
}

void
purple_blist_node_set_int(PurpleBListNode* node, const char *key, int data)
{
	GValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_INT);
	g_value_set_int(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

int
purple_blist_node_get_int(PurpleBListNode* node, const char *key)
{
	GValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, 0);
	g_return_val_if_fail(priv->settings != NULL, 0);
	g_return_val_if_fail(key != NULL, 0);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return 0;

	g_return_val_if_fail(G_VALUE_HOLDS_INT(value), 0);

	return g_value_get_int(value);
}

void
purple_blist_node_set_string(PurpleBListNode* node, const char *key, const char *data)
{
	GValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_STRING);
	g_value_set_string(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

const char *
purple_blist_node_get_string(PurpleBListNode* node, const char *key)
{
	GValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(priv->settings != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return NULL;

	g_return_val_if_fail(G_VALUE_HOLDS_STRING(value), NULL);

	return g_value_get_string(value);
}

GList *
purple_blist_node_get_extended_menu(PurpleBListNode *n)
{
	GList *menu = NULL;

	g_return_val_if_fail(n != NULL, NULL);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-extended-menu",
			n, &menu);
	return menu;
}

static void
purple_blist_node_setting_free(gpointer data)
{
	GValue *value = (GValue *)data;

	g_value_unset(value);
	g_free(value);
}

/**************************************************************************
 * GObject code
 **************************************************************************/

/* GObject Property names */
#define PROP_DONT_SAVE_S  "dont-save"

/* Set method for GObject properties */
static void
purple_blist_node_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBListNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case PROP_DONT_SAVE:
			purple_blist_node_set_dont_save(node, g_value_get_boolean(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_blist_node_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleBListNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case PROP_DONT_SAVE:
			g_value_set_boolean(value, purple_blist_node_get_dont_save(node));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_blist_node_init(GTypeInstance *instance, gpointer klass)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(instance);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)purple_blist_node_setting_free);
}

/* GObject finalize function */
static void
purple_blist_node_finalize(GObject *object)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(object);

	g_hash_table_destroy(priv->settings);

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_blist_node_class_init(PurpleBListNodeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_blist_node_finalize;

	/* Setup properties */
	obj_class->get_property = purple_blist_node_get_property;
	obj_class->set_property = purple_blist_node_set_property;

	g_object_class_install_property(obj_class, PROP_DONT_SAVE,
			g_param_spec_boolean(PROP_DONT_SAVE_S, _("Do not save"),
				_("Whether node should not be saved with the buddy list."),
				FALSE, G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleBListNodePrivate));
}

GType
purple_blist_node_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleBListNodeClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_blist_node_class_init,
			NULL,
			NULL,
			sizeof(PurpleBListNode),
			0,
			(GInstanceInitFunc)purple_blist_node_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleBListNode",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

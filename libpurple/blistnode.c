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
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BLIST_NODE, PurpleBlistNodePrivate))

/** @copydoc _PurpleBlistNodePrivate */
typedef struct _PurpleBlistNodePrivate  PurpleBlistNodePrivate;

#define PURPLE_COUNTING_NODE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_COUNTING_NODE, PurpleCountingNodePrivate))

/** @copydoc _PurpleCountingNodePrivate */
typedef struct _PurpleCountingNodePrivate  PurpleCountingNodePrivate;

/** Private data of a buddy list node */
struct _PurpleBlistNodePrivate {
	GHashTable *settings;  /**< per-node settings                            */
	gboolean dont_save;    /**< node should not be saved with the buddy list */
};

/* Blist node property enums */
enum
{
	BLNODE_PROP_0,
	BLNODE_PROP_DONT_SAVE,
	BLNODE_PROP_LAST
};

/** Private data of a counting node */
struct _PurpleCountingNodePrivate {
	int totalsize;    /**< The number of children under this node            */
	int currentsize;  /**< The number of children under this node
	                       corresponding to online accounts                  */
	int onlinecount;  /**< The number of children under this contact who are
	                       currently online                                  */
};

/* Counting node property enums */
enum
{
	CNODE_PROP_0,
	CNODE_PROP_TOTAL_SIZE,
	CNODE_PROP_CURRENT_SIZE,
	CNODE_PROP_ONLINE_COUNT,
	CNODE_PROP_LAST
};

static GObjectClass *parent_class;

/**************************************************************************/
/* Buddy list node API                                                    */
/**************************************************************************/

static PurpleBlistNode *get_next_node(PurpleBlistNode *node, gboolean godeep)
{
	if (node == NULL)
		return NULL;

	if (godeep && node->child)
		return node->child;

	if (node->next)
		return node->next;

	return get_next_node(node->parent, FALSE);
}

PurpleBlistNode *purple_blist_node_next(PurpleBlistNode *node, gboolean offline)
{
	PurpleBlistNode *ret = node;

	if (offline)
		return get_next_node(ret, TRUE);
	do
	{
		ret = get_next_node(ret, TRUE);
	} while (ret && PURPLE_IS_BUDDY(ret) &&
			!purple_account_is_connected(purple_buddy_get_account((PurpleBuddy *)ret)));

	return ret;
}

PurpleBlistNode *purple_blist_node_get_parent(PurpleBlistNode *node)
{
	return node ? node->parent : NULL;
}

PurpleBlistNode *purple_blist_node_get_first_child(PurpleBlistNode *node)
{
	return node ? node->child : NULL;
}

PurpleBlistNode *purple_blist_node_get_sibling_next(PurpleBlistNode *node)
{
	return node? node->next : NULL;
}

PurpleBlistNode *purple_blist_node_get_sibling_prev(PurpleBlistNode *node)
{
	return node? node->prev : NULL;
}

void *
purple_blist_node_get_ui_data(const PurpleBlistNode *node)
{
	g_return_val_if_fail(node, NULL);

	return node->ui_data;
}

void
purple_blist_node_set_ui_data(PurpleBlistNode *node, void *ui_data) {
	g_return_if_fail(node);

	node->ui_data = ui_data;
}

void purple_blist_node_remove_setting(PurpleBlistNode *node, const char *key)
{
	PurpleBlistUiOps *ops;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	g_hash_table_remove(priv->settings, key);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

void
purple_blist_node_set_dont_save(PurpleBlistNode *node, gboolean dont_save)
{
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);

	priv->dont_save = dont_save;
}

gboolean
purple_blist_node_get_dont_save(PurpleBlistNode *node)
{
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, 0);

	return priv->dont_save;
}

GHashTable *
purple_blist_node_get_settings(PurpleBlistNode *node)
{
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->settings;
}

gboolean
purple_blist_node_has_setting(PurpleBlistNode* node, const char *key)
{
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->settings != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	/* Boxed type, so it won't ever be NULL, so no need for _extended */
	return (g_hash_table_lookup(priv->settings, key) != NULL);
}

void
purple_blist_node_set_bool(PurpleBlistNode* node, const char *key, gboolean data)
{
	GValue *value;
	PurpleBlistUiOps *ops;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_g_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

gboolean
purple_blist_node_get_bool(PurpleBlistNode* node, const char *key)
{
	GValue *value;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

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
purple_blist_node_set_int(PurpleBlistNode* node, const char *key, int data)
{
	GValue *value;
	PurpleBlistUiOps *ops;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_g_value_new(G_TYPE_INT);
	g_value_set_int(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

int
purple_blist_node_get_int(PurpleBlistNode* node, const char *key)
{
	GValue *value;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

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
purple_blist_node_set_string(PurpleBlistNode* node, const char *key, const char *data)
{
	GValue *value;
	PurpleBlistUiOps *ops;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_g_value_new(G_TYPE_STRING);
	g_value_set_string(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

const char *
purple_blist_node_get_string(PurpleBlistNode* node, const char *key)
{
	GValue *value;
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

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
purple_blist_node_get_extended_menu(PurpleBlistNode *n)
{
	GList *menu = NULL;

	g_return_val_if_fail(n != NULL, NULL);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-extended-menu",
			n, &menu);
	return menu;
}

/**************************************************************************
 * GObject code for PurpleBlistNode
 **************************************************************************/

/* GObject Property names */
#define BLNODE_PROP_DONT_SAVE_S  "dont-save"

/* Set method for GObject properties */
static void
purple_blist_node_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBlistNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case BLNODE_PROP_DONT_SAVE:
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
	PurpleBlistNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case BLNODE_PROP_DONT_SAVE:
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
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(instance);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)purple_g_value_free);
}

/* GObject finalize function */
static void
purple_blist_node_finalize(GObject *object)
{
	PurpleBlistNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(object);

	g_hash_table_destroy(priv->settings);

	parent_class->finalize(object);
}

/* Class initializer function */
static void
purple_blist_node_class_init(PurpleBlistNodeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_blist_node_finalize;

	/* Setup properties */
	obj_class->get_property = purple_blist_node_get_property;
	obj_class->set_property = purple_blist_node_set_property;

	g_object_class_install_property(obj_class, BLNODE_PROP_DONT_SAVE,
			g_param_spec_boolean(BLNODE_PROP_DONT_SAVE_S, _("Do not save"),
				_("Whether node should not be saved with the buddy list."),
				FALSE, G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleBlistNodePrivate));
}

GType
purple_blist_node_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleBlistNodeClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_blist_node_class_init,
			NULL,
			NULL,
			sizeof(PurpleBlistNode),
			0,
			(GInstanceInitFunc)purple_blist_node_init,
			NULL,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleBlistNode",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**************************************************************************/
/* Counting node API                                                      */
/**************************************************************************/

int
purple_counting_node_get_total_size(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->totalsize;
}

int
purple_counting_node_get_current_size(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->currentsize;
}

int
purple_counting_node_get_online_count(PurpleCountingNode *counter)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_val_if_fail(priv != NULL, -1);

	return priv->onlinecount;
}

void
purple_counting_node_change_total_size(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->totalsize += delta;
}

void
purple_counting_node_change_current_size(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->currentsize += delta;
}

void
purple_counting_node_change_online_count(PurpleCountingNode *counter, int delta)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->onlinecount += delta;
}

void
purple_counting_node_set_total_size(PurpleCountingNode *counter, int totalsize)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->totalsize = totalsize;
}

void
purple_counting_node_set_current_size(PurpleCountingNode *counter, int currentsize)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->currentsize = currentsize;
}

void
purple_counting_node_set_online_count(PurpleCountingNode *counter, int onlinecount)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(counter);

	g_return_if_fail(priv != NULL);

	priv->onlinecount = onlinecount;
}

/**************************************************************************
 * GObject code for PurpleCountingNode
 **************************************************************************/
 
/* GObject Property names */
#define CNODE_PROP_TOTAL_SIZE_S    "total-size"
#define CNODE_PROP_CURRENT_SIZE_S  "current-size"
#define CNODE_PROP_ONLINE_COUNT_S  "online-count"

/* Set method for GObject properties */
static void
purple_counting_node_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleCountingNode *node = PURPLE_COUNTING_NODE(obj);

	switch (param_id) {
		case CNODE_PROP_TOTAL_SIZE:
			purple_counting_node_set_total_size(node, g_value_get_int(value));
			break;
		case CNODE_PROP_CURRENT_SIZE:
			purple_counting_node_set_current_size(node, g_value_get_int(value));
			break;
		case CNODE_PROP_ONLINE_COUNT:
			purple_counting_node_set_online_count(node, g_value_get_int(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_counting_node_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleCountingNode *node = PURPLE_COUNTING_NODE(obj);

	switch (param_id) {
		case CNODE_PROP_TOTAL_SIZE:
			g_value_set_int(value, purple_counting_node_get_total_size(node));
			break;
		case CNODE_PROP_CURRENT_SIZE:
			g_value_set_int(value, purple_counting_node_get_current_size(node));
			break;
		case CNODE_PROP_ONLINE_COUNT:
			g_value_set_int(value, purple_counting_node_get_online_count(node));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_counting_node_init(GTypeInstance *instance, gpointer klass)
{
	PurpleCountingNodePrivate *priv = PURPLE_COUNTING_NODE_GET_PRIVATE(instance);

	priv->totalsize   = 0;
	priv->currentsize = 0;
	priv->onlinecount = 0;
}

/* Class initializer function */
static void
purple_counting_node_class_init(PurpleCountingNodeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	/* Setup properties */
	obj_class->get_property = purple_counting_node_get_property;
	obj_class->set_property = purple_counting_node_set_property;

	g_object_class_install_property(obj_class, CNODE_PROP_TOTAL_SIZE,
			g_param_spec_int(CNODE_PROP_TOTAL_SIZE_S, _("Total size"),
				_("The number of children under this node."),
				G_MININT, G_MAXINT, 0, G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CNODE_PROP_CURRENT_SIZE,
			g_param_spec_int(CNODE_PROP_CURRENT_SIZE_S, _("Current size"),
				_("The number of children with online accounts."),
				G_MININT, G_MAXINT, 0, G_PARAM_READWRITE)
			);

	g_object_class_install_property(obj_class, CNODE_PROP_ONLINE_COUNT,
			g_param_spec_int(CNODE_PROP_ONLINE_COUNT_S, _("Online count"),
				_("The number of children that are online."),
				G_MININT, G_MAXINT, 0, G_PARAM_READWRITE)
			);

	g_type_class_add_private(klass, sizeof(PurpleCountingNodePrivate));
}

GType
purple_counting_node_get_type(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleCountingNodeClass),
			NULL,
			NULL,
			(GClassInitFunc)purple_counting_node_class_init,
			NULL,
			NULL,
			sizeof(PurpleCountingNode),
			0,
			(GInstanceInitFunc)purple_counting_node_init,
			NULL,
		};

		type = g_type_register_static(PURPLE_TYPE_BLIST_NODE,
				"PurpleCountingNode",
				&info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

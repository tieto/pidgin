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
#include "blistnode.h"

#define PURPLE_BLIST_NODE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_BLIST_NODE, PurpleBListNodePrivate))

/** @copydoc _PurpleBListNodePrivate */
typedef struct _PurpleBListNodePrivate  PurpleBListNodePrivate;

/** Private data of a buddy list node */
struct _PurpleBListNodePrivate {
	GHashTable *settings;     /**< per-node settings                       */
	gboolean dont_save;       /**< node should not be saved with the buddy
	                               list                                    */
};

/**************************************************************************/
/* Buddy list node API                                                    */
/**************************************************************************/

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

/* TODO GObjectify */
static void
purple_blist_node_destroy(PurpleBListNode *node)
{
	PurpleBListUiOps *ui_ops;
	PurpleBListNode *child, *next_child;

	ui_ops = purple_blist_get_ui_ops();
	child = node->child;
	while (child) {
		next_child = child->next;
		purple_blist_node_destroy(child);
		child = next_child;
	}

	/* Allow the UI to free data */
	node->parent = NULL;
	node->child  = NULL;
	node->next   = NULL;
	node->prev   = NULL;
	if (ui_ops && ui_ops->remove)
		ui_ops->remove(purplebuddylist, node);

	if (PURPLE_IS_BUDDY(node))
		purple_buddy_destroy((PurpleBuddy*)node);
	else if (PURPLE_IS_CHAT(node))
		purple_chat_destroy((PurpleChat*)node);
	else if (PURPLE_IS_CONTACT(node))
		purple_contact_destroy((PurpleContact*)node);
	else if (PURPLE_IS_GROUP(node))
		purple_group_destroy((PurpleGroup*)node);
}

static void
purple_blist_node_setting_free(gpointer data)
{
	PurpleValue *value;

	value = (PurpleValue *)data;

	purple_value_destroy(value);
}

static void purple_blist_node_initialize_settings(PurpleBListNode *node)
{
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);

	if (priv->settings)
		return;

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)purple_blist_node_setting_free);
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
	PurpleValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_value_new(PURPLE_TYPE_BOOLEAN);
	purple_value_set_boolean(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

gboolean
purple_blist_node_get_bool(PurpleBListNode* node, const char *key)
{
	PurpleValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(priv->settings != NULL, FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return FALSE;

	g_return_val_if_fail(purple_value_get_type(value) == PURPLE_TYPE_BOOLEAN, FALSE);

	return purple_value_get_boolean(value);
}

void
purple_blist_node_set_int(PurpleBListNode* node, const char *key, int data)
{
	PurpleValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_value_new(PURPLE_TYPE_INT);
	purple_value_set_int(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

int
purple_blist_node_get_int(PurpleBListNode* node, const char *key)
{
	PurpleValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, 0);
	g_return_val_if_fail(priv->settings != NULL, 0);
	g_return_val_if_fail(key != NULL, 0);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return 0;

	g_return_val_if_fail(purple_value_get_type(value) == PURPLE_TYPE_INT, 0);

	return purple_value_get_int(value);
}

void
purple_blist_node_set_string(PurpleBListNode* node, const char *key, const char *data)
{
	PurpleValue *value;
	PurpleBListUiOps *ops;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_if_fail(priv != NULL);
	g_return_if_fail(priv->settings != NULL);
	g_return_if_fail(key != NULL);

	value = purple_value_new(PURPLE_TYPE_STRING);
	purple_value_set_string(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	ops = purple_blist_get_ui_ops();
	if (ops && ops->save_node)
		ops->save_node(node);
}

const char *
purple_blist_node_get_string(PurpleBListNode* node, const char *key)
{
	PurpleValue *value;
	PurpleBListNodePrivate *priv = PURPLE_BLIST_NODE_GET_PRIVATE(node);

	g_return_val_if_fail(priv != NULL, NULL);
	g_return_val_if_fail(priv->settings != NULL, NULL);
	g_return_val_if_fail(key != NULL, NULL);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return NULL;

	g_return_val_if_fail(purple_value_get_type(value) == PURPLE_TYPE_STRING, NULL);

	return purple_value_get_string(value);
}

GList *
purple_blist_node_get_extended_menu(PurpleBListNode *n)
{
	GList *menu = NULL;

	g_return_val_if_fail(n != NULL, NULL);

	purple_signal_emit(purple_blist_get_handle(),
			"blist-node-extended-menu",
			n, &menu);
	return menu;
}

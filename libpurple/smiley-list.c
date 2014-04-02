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

#include "smiley-list.h"

#include "dbus-maybe.h"
#include "debug.h"
#include "smiley-parser.h"
#include "trie.h"

#define PURPLE_SMILEY_LIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY_LIST, \
	PurpleSmileyListPrivate))

typedef struct {
	GList *smileys;
	GList *smileys_end;
	PurpleTrie *trie;
	GHashTable *unique_map;
} PurpleSmileyListPrivate;

enum
{
	PROP_0,
	PROP_LAST
};

static void
_list_append2(GList **head_p, GList **tail_p, gpointer data)
{
	GList *head = *head_p;
	GList *tail = *tail_p;
	GList *elem;

	g_return_if_fail((head == NULL) == (tail == NULL));
	g_return_if_fail((tail == NULL) || (tail->next == NULL));

	elem = g_list_alloc();
	elem->data = data;
	elem->prev = tail;
	elem->next = NULL;

	if (head) {
		tail->next = elem;
		*tail_p = elem;
	} else
		*head_p = *tail_p = elem;
}

static void
_list_remove_link2(GList **head_p, GList **tail_p, GList *link)
{
	GList *head = *head_p;
	GList *tail = *tail_p;

	g_return_if_fail(head != NULL);
	g_return_if_fail(tail != NULL);

	if (link == tail)
		*tail_p = tail->prev;
	*head_p = g_list_remove_link(head, link);
}

/*******************************************************************************
 * API implementation
 ******************************************************************************/

PurpleSmileyList *
purple_smiley_list_new(void)
{
	return g_object_new(PURPLE_TYPE_SMILEY_LIST, NULL);
}

gboolean
purple_smiley_list_add(PurpleSmileyList *list, PurpleSmiley *smiley)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);
	const gchar *smiley_path;
	gboolean succ;
	gchar *tmp = NULL;
	const gchar *shortcut;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(PURPLE_IS_SMILEY(smiley), FALSE);

	if (g_object_get_data(G_OBJECT(smiley), "purple-smiley-list") != NULL) {
		purple_debug_warning("smiley-list",
			"smiley is already associated with some list");
		return FALSE;
	}

	shortcut = purple_smiley_get_shortcut(smiley);
	if (purple_smiley_parse_escape())
		shortcut = tmp = g_markup_escape_text(shortcut, -1);
	succ = purple_trie_add(priv->trie, shortcut, smiley);
	g_free(tmp);
	if (!succ)
		return FALSE;

	g_object_ref(smiley);
	_list_append2(&priv->smileys, &priv->smileys_end, smiley);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", list);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem",
		priv->smileys_end);

	smiley_path = purple_smiley_get_path(smiley);
	if (g_hash_table_lookup(priv->unique_map, smiley_path) == NULL) {
		g_hash_table_insert(priv->unique_map,
			g_strdup(smiley_path), smiley);
	}

	return TRUE;
}

void
purple_smiley_list_remove(PurpleSmileyList *list, PurpleSmiley *smiley)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);
	GList *list_elem;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(PURPLE_IS_SMILEY(smiley));

	if (g_object_get_data(G_OBJECT(smiley), "purple-smiley-list") != list) {
		purple_debug_warning("smiley-list", "remove: invalid list");
		return;
	}

	list_elem = g_object_get_data(G_OBJECT(smiley),
		"purple-smiley-list-elem");

	purple_trie_remove(priv->trie, purple_smiley_get_shortcut(smiley));
	_list_remove_link2(&priv->smileys, &priv->smileys_end, list_elem);

	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", NULL);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem", NULL);
	g_object_unref(smiley);
}

PurpleSmiley *
purple_smiley_list_get_by_shortcut(PurpleSmileyList *list,
	const gchar *shortcut)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return g_hash_table_lookup(priv->unique_map, shortcut);
}

PurpleTrie *
purple_smiley_list_get_trie(PurpleSmileyList *list)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->trie;
}

GList *
purple_smiley_list_get_unique(PurpleSmileyList *list)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return g_hash_table_get_values(priv->unique_map);
}


/*******************************************************************************
 * Object stuff
 ******************************************************************************/

static void
purple_smiley_list_init(GTypeInstance *instance, gpointer klass)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(instance);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	priv->trie = purple_trie_new();
	priv->unique_map = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, NULL);

	PURPLE_DBUS_REGISTER_POINTER(sl, PurpleSmileyList);
}

static void
purple_smiley_list_finalize(GObject *obj)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(obj);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);
	GList *it;

	g_object_unref(priv->trie);
	g_hash_table_destroy(priv->unique_map);

	for (it = priv->smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", NULL);
		g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem", NULL);
		g_object_unref(smiley);
	}
	g_list_free(priv->smileys);

	PURPLE_DBUS_UNREGISTER_POINTER(sl);
}

static void
purple_smiley_list_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(object);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	(void)priv;

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_list_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(object);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	(void)priv;

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_smiley_list_class_init(PurpleSmileyListClass *klass)
{
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(PurpleSmileyListPrivate));

	gobj_class->get_property = purple_smiley_list_get_property;
	gobj_class->set_property = purple_smiley_list_set_property;
	gobj_class->finalize = purple_smiley_list_finalize;
}

GType
purple_smiley_list_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleSmileyListClass),
			.class_init = (GClassInitFunc)purple_smiley_list_class_init,
			.instance_size = sizeof(PurpleSmileyList),
			.instance_init = purple_smiley_list_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleSmileyList", &info, 0);
	}

	return type;
}

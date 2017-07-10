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

#include <string.h>

#define PURPLE_SMILEY_LIST_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_SMILEY_LIST, \
	PurpleSmileyListPrivate))

typedef struct {
	GList *smileys;
	GList *smileys_end;
	PurpleTrie *trie;
	GHashTable *path_map;
	GHashTable *shortcut_map;

	gboolean drop_failed_remotes;
} PurpleSmileyListPrivate;

enum
{
	PROP_0,
	PROP_DROP_FAILED_REMOTES,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

/*******************************************************************************
 * Object stuff
 ******************************************************************************/
G_DEFINE_TYPE_WITH_PRIVATE(PurpleSmileyList, purple_smiley_list, G_TYPE_OBJECT);

static void
purple_smiley_list_init(PurpleSmileyList *list) {
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	priv->trie = purple_trie_new();
	priv->path_map = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, NULL);
	priv->shortcut_map = g_hash_table_new_full(g_str_hash, g_str_equal,
		g_free, NULL);

	PURPLE_DBUS_REGISTER_POINTER(list, PurpleSmileyList);
}

static void
purple_smiley_list_finalize(GObject *obj) {
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(obj);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);
	GList *it;

	g_object_unref(priv->trie);
	g_hash_table_destroy(priv->path_map);
	g_hash_table_destroy(priv->shortcut_map);

	for (it = priv->smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", NULL);
		g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem", NULL);
		g_object_unref(smiley);
	}
	g_list_free(priv->smileys);

	PURPLE_DBUS_UNREGISTER_POINTER(sl);

	G_OBJECT_CLASS(purple_smiley_list_parent_class)->finalize(obj);
}

static void
purple_smiley_list_get_property(GObject *obj, guint param_id, GValue *value,
                                GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(obj);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	switch (param_id) {
		case PROP_DROP_FAILED_REMOTES:
			g_value_set_boolean(value, priv->drop_failed_remotes);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_smiley_list_set_property(GObject *obj, guint param_id,
                                const GValue *value, GParamSpec *pspec)
{
	PurpleSmileyList *sl = PURPLE_SMILEY_LIST(obj);
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(sl);

	switch (param_id) {
		case PROP_DROP_FAILED_REMOTES:
			priv->drop_failed_remotes = g_value_get_boolean(value);
			/* XXX: we could scan for remote smiley's on our list
			 * and change the setting, but we don't care that much.
			 */
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_smiley_list_class_init(PurpleSmileyListClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_smiley_list_get_property;
	obj_class->set_property = purple_smiley_list_set_property;
	obj_class->finalize = purple_smiley_list_finalize;

	properties[PROP_DROP_FAILED_REMOTES] = g_param_spec_boolean(
		"drop-failed-remotes", "Drop failed remote PurpleSmileys",
		"Watch added remote smileys and remove them from the list, "
		"if they change their state to failed", FALSE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
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
_list_delete_link2(GList **head_p, GList **tail_p, GList *link)
{
	GList *head = *head_p;
	GList *tail = *tail_p;

	g_return_if_fail(head != NULL);
	g_return_if_fail(tail != NULL);

	if (link == tail)
		*tail_p = tail->prev;
	*head_p = g_list_delete_link(head, link);
}

static const gchar *
smiley_get_uniqid(PurpleSmiley *smiley)
{
	return purple_image_get_path(PURPLE_IMAGE(smiley));
}

/*******************************************************************************
 * API implementation
 ******************************************************************************/
PurpleSmileyList *
purple_smiley_list_new(void) {
	return g_object_new(PURPLE_TYPE_SMILEY_LIST, NULL);
}

gboolean
purple_smiley_list_add(PurpleSmileyList *list, PurpleSmiley *smiley) {
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);
	const gchar *smiley_path;
	gboolean succ;
	gchar *shortcut_escaped;
	const gchar *shortcut;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(PURPLE_IS_SMILEY(smiley), FALSE);

	if (g_object_get_data(G_OBJECT(smiley), "purple-smiley-list") != NULL) {
		purple_debug_warning("smiley-list",
			"smiley is already associated with some list");
		return FALSE;
	}

	shortcut = purple_smiley_get_shortcut(smiley);

	if (g_hash_table_lookup(priv->shortcut_map, shortcut) != NULL)
		return FALSE;

	shortcut_escaped = g_markup_escape_text(shortcut, -1);
	succ = purple_trie_add(priv->trie, shortcut_escaped, smiley);

	/* A special-case for WebKit, which unescapes apos entity.
	 * Please, don't trust this hack - it may be removed in future releases.
	 */
	if (succ && strstr(shortcut_escaped, "&apos;") != NULL) {
		gchar *tmp = shortcut_escaped;
		shortcut_escaped = purple_strreplace(shortcut_escaped,
			"&apos;", "'");
		g_free(tmp);
		succ = purple_trie_add(priv->trie, shortcut_escaped, smiley);
	}

	g_free(shortcut_escaped);
	if (!succ)
		return FALSE;

	g_object_ref(smiley);
	_list_append2(&priv->smileys, &priv->smileys_end, smiley);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", list);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem",
		priv->smileys_end);

	g_hash_table_insert(priv->shortcut_map, g_strdup(shortcut), smiley);

	smiley_path = smiley_get_uniqid(smiley);

	/* TODO: add to the table, when the smiley sets the path */
	if (!smiley_path)
		return TRUE;

	if (g_hash_table_lookup(priv->path_map, smiley_path) == NULL) {
		g_hash_table_insert(priv->path_map,
			g_strdup(smiley_path), smiley);
	}

	return TRUE;
}

void
purple_smiley_list_remove(PurpleSmileyList *list, PurpleSmiley *smiley) {
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);
	GList *list_elem, *it;
	const gchar *shortcut, *path;
	gchar *shortcut_escaped;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(PURPLE_IS_SMILEY(smiley));

	if (g_object_get_data(G_OBJECT(smiley), "purple-smiley-list") != list) {
		purple_debug_warning("smiley-list", "remove: invalid list");
		return;
	}

	list_elem = g_object_get_data(G_OBJECT(smiley),
		"purple-smiley-list-elem");

	shortcut = purple_smiley_get_shortcut(smiley);
	path = smiley_get_uniqid(smiley);

	g_hash_table_remove(priv->shortcut_map, shortcut);
	if (path)
		g_hash_table_remove(priv->path_map, path);

	shortcut_escaped = g_markup_escape_text(shortcut, -1);
	purple_trie_remove(priv->trie, shortcut);
	g_free(shortcut_escaped);

	_list_delete_link2(&priv->smileys, &priv->smileys_end, list_elem);

	/* re-add entry to path_map if smiley was not unique */
	for (it = priv->smileys; it && path; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;

		if (g_strcmp0(smiley_get_uniqid(smiley), path) == 0) {
			g_hash_table_insert(priv->path_map,
				g_strdup(path), smiley);
			break;
		}
	}

	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list", NULL);
	g_object_set_data(G_OBJECT(smiley), "purple-smiley-list-elem", NULL);
	g_object_unref(smiley);
}

gboolean
purple_smiley_list_is_empty(const PurpleSmileyList *list) {
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, TRUE);

	return (priv->smileys == NULL);
}

PurpleSmiley *
purple_smiley_list_get_by_shortcut(PurpleSmileyList *list,
	                               const gchar *shortcut)
{
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return g_hash_table_lookup(priv->shortcut_map, shortcut);
}

PurpleTrie *
purple_smiley_list_get_trie(PurpleSmileyList *list) {
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->trie;
}

GList *
purple_smiley_list_get_unique(PurpleSmileyList *list) {
	GList *unique = NULL, *it;
	GHashTable *unique_map;
	PurpleSmileyListPrivate *priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	g_return_val_if_fail(priv != NULL, NULL);

	/* We could just return g_hash_table_get_values(priv->path_map) here,
	 * but it won't be in order. */

	unique_map = g_hash_table_new(g_str_hash, g_str_equal);

	for (it = priv->smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		const gchar *path = smiley_get_uniqid(smiley);

		if (g_hash_table_lookup(unique_map, path))
			continue;

		unique = g_list_prepend(unique, smiley);
		g_hash_table_insert(unique_map, (gpointer)path, smiley);
	}

	g_hash_table_destroy(unique_map);

	return g_list_reverse(unique);
}

GList *
purple_smiley_list_get_all(PurpleSmileyList *list) {
	PurpleSmileyListPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_SMILEY_LIST(list), NULL);

	priv = PURPLE_SMILEY_LIST_GET_PRIVATE(list);

	return g_hash_table_get_values(priv->shortcut_map);
}

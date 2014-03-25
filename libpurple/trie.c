/*
 * Purple
 *
 * Purple is the legal property of its developers, whose names are too
 * numerous to list here. Please refer to the COPYRIGHT file distributed
 * with this source distribution
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "trie.h"

#include "memorypool.h"

#define PURPLE_TRIE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_TRIE, PurpleTriePrivate))

typedef struct _PurpleTrieRecord PurpleTrieRecord;

typedef struct
{
	gboolean reset_on_match;

	PurpleMemoryPool *records_mempool;
	GSList *records;
} PurpleTriePrivate;

struct _PurpleTrieRecord
{
	const gchar *word;
	gpointer data;
};

/*******************************************************************************
 * Records
 ******************************************************************************/

void
purple_trie_add(PurpleTrie *trie, const gchar *word, gpointer data)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieRecord *rec;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(word != NULL);

	rec = g_new(PurpleTrieRecord, 1);
	rec->word = purple_memory_pool_strdup(priv->records_mempool, word);
	rec->data = data;

	priv->records = g_slist_prepend(priv->records, rec);
}

/*******************************************************************************
 * API implementation
 ******************************************************************************/

gboolean
purple_trie_get_reset_on_match(PurpleTrie *trie)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	g_return_val_if_fail(priv, FALSE);

	return priv->reset_on_match;
}

void
purple_trie_set_reset_on_match(PurpleTrie *trie, gboolean reset)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	g_return_if_fail(priv);

	priv->reset_on_match = reset;
}

/*******************************************************************************
 * Object stuff
 ******************************************************************************/

enum
{
	PROP_ZERO,
	PROP_RESET_ON_MATCH,
	PROP_LAST
};

static GObjectClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];

PurpleTrie *
purple_trie_new(void)
{
	return g_object_new(PURPLE_TYPE_TRIE, NULL);
}

static void
purple_trie_init(GTypeInstance *instance, gpointer klass)
{
	PurpleTrie *trie = PURPLE_TRIE(instance);
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	priv->records_mempool = purple_memory_pool_new();
}

static void
purple_trie_finalize(GObject *obj)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(obj);

	g_slist_free_full(priv->records, g_free);
	g_object_unref(priv->records_mempool);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_trie_get_property(GObject *obj, guint param_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleTrie *trie = PURPLE_TRIE(obj);
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	switch (param_id) {
		case PROP_RESET_ON_MATCH:
			g_value_set_boolean(value, priv->reset_on_match);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
	}
}

static void
purple_trie_set_property(GObject *obj, guint param_id,
	const GValue *value, GParamSpec *pspec)
{
	PurpleTrie *trie = PURPLE_TRIE(obj);
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	switch (param_id) {
		case PROP_RESET_ON_MATCH:
			priv->reset_on_match = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
	}
}

static void
purple_trie_class_init(PurpleTrieClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleTriePrivate));

	obj_class->finalize = purple_trie_finalize;
	obj_class->get_property = purple_trie_get_property;
	obj_class->set_property = purple_trie_set_property;

	properties[PROP_RESET_ON_MATCH] = g_param_spec_boolean("reset-on-match",
		"Reset on match", "Determines, if the search state machine "
		"should be reset to the initial state on every match. This "
		"ensures, that every match is distinct from each other.", TRUE,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

GType
purple_trie_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleTrieClass),
			.class_init = (GClassInitFunc)purple_trie_class_init,
			.instance_size = sizeof(PurpleTrie),
			.instance_init = purple_trie_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT,
			"PurpleTrie", &info, 0);
	}

	return type;
}

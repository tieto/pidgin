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

#define PURPLE_TRIE_STATES_POOL_BLOCK_SIZE 102400

typedef struct _PurpleTrieRecord PurpleTrieRecord;
typedef struct _PurpleTrieState PurpleTrieState;
typedef struct _PurpleTrieRecordList PurpleTrieRecordList;

typedef struct
{
	gboolean reset_on_match;

	PurpleMemoryPool *records_str_mempool;
	PurpleMemoryPool *records_obj_mempool;
	PurpleTrieRecordList *records;

	PurpleMemoryPool *states_mempool;
	PurpleTrieState *root_state;
} PurpleTriePrivate;

struct _PurpleTrieRecord
{
	const gchar *word;
	gpointer data;
};

struct _PurpleTrieRecordList
{
	PurpleTrieRecord *rec;
	PurpleTrieRecordList *next;
	PurpleTrieRecordList *prev;
};

struct _PurpleTrieState
{
	PurpleTrieState *parent;
	PurpleTrieState **children;

	PurpleTrieState *longest_suffix;

	PurpleTrieRecord *found_word;
};

/*******************************************************************************
 * Records list
 ******************************************************************************/

static PurpleTrieRecordList *
purple_record_list_new(PurpleMemoryPool *mpool, PurpleTrieRecord *rec)
{
	PurpleTrieRecordList *node;

	node = purple_memory_pool_alloc(mpool,
		sizeof(PurpleTrieRecordList), sizeof(gpointer));
	g_return_val_if_fail(node != NULL, NULL);

	node->rec = rec;

	return node;
}

static PurpleTrieRecordList *
purple_record_list_prepend(PurpleMemoryPool *mpool,
	PurpleTrieRecordList *old_head, PurpleTrieRecord *rec)
{
	PurpleTrieRecordList *new_head;

	new_head = purple_record_list_new(mpool, rec);
	g_return_val_if_fail(new_head != NULL, NULL);

	new_head->next = old_head;
	old_head->prev = new_head;
	new_head->prev = NULL;

	return new_head;
}

static PurpleTrieRecordList *
purple_record_list_copy(PurpleMemoryPool *mpool,
	const PurpleTrieRecordList *head)
{
	PurpleTrieRecordList *new_head = NULL, *new_tail = NULL;

	while (head) {
		PurpleTrieRecordList *node;

		node = purple_record_list_new(mpool, head->rec);
		g_return_val_if_fail(node != NULL, NULL); /* there is no leak */

		node->prev = new_tail;
		if (new_tail)
			new_tail->next = node;
		new_tail = node;
		if (!new_head)
			new_head = node;

		head = head->next;
	}

	return new_head;
}

static PurpleTrieRecordList *
purple_record_list_remove(PurpleTrieRecordList *head,
	PurpleTrieRecordList *node)
{
	g_return_val_if_fail(head != NULL, NULL);
	g_return_val_if_fail(node != NULL, head);
	g_return_val_if_fail(head->prev == NULL, NULL);

	if (head == node) {
		if (head->next != NULL)
			head->next->prev = NULL;
		return head->next;
	} else {
		g_return_val_if_fail(node->prev != NULL, NULL);
		node->prev->next = node->next;
		if (node->next != NULL)
			node->next->prev = node->prev;
		return head;
	}
}


/*******************************************************************************
 * States management
 ******************************************************************************/

static void
purple_trie_states_cleanup(PurpleTrie *trie)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	g_return_if_fail(priv != NULL);

	if (priv->root_state != NULL) {
		purple_memory_pool_cleanup(priv->states_mempool);
		priv->root_state = NULL;
	}
}

/* Allocates a state and binds it to the parent. */
static PurpleTrieState *
purple_trie_state_new(PurpleTrie *trie, PurpleTrieState *parent, guchar letter)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieState *state;

	g_return_val_if_fail(priv != NULL, NULL);

	state = purple_memory_pool_alloc0(priv->states_mempool,
		sizeof(PurpleTrieState), sizeof(gpointer));
	g_return_val_if_fail(state != NULL, NULL);

	if (parent == NULL)
		return state;

	state->parent = parent;
	if (parent->children == NULL) {
		parent->children = purple_memory_pool_alloc0(
			priv->states_mempool,
			/* PurpleTrieState *children[sizeof(guchar)] */
			sizeof(guchar) * sizeof(gpointer),
			sizeof(gpointer));
	}

	if (parent->children == NULL) {
		purple_memory_pool_free(priv->states_mempool, state);
		g_warn_if_reached();
		return NULL;
	}

	parent->children[letter] = state;

	return state;
}

static gboolean
purple_trie_states_build(PurpleTrie *trie)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieState *root;
	PurpleMemoryPool *reclist_mpool;
	PurpleTrieRecordList *reclist;

	g_return_val_if_fail(priv != NULL, FALSE);

	if (priv->root_state != NULL)
		return TRUE;

	priv->root_state = root = purple_trie_state_new(trie, NULL, '\0');
	g_return_val_if_fail(root != NULL, FALSE);

	reclist_mpool = purple_memory_pool_new();
	reclist = purple_record_list_copy(reclist_mpool, priv->records);

	/* XXX: todo */
	/* test */ purple_record_list_remove(reclist, reclist->next);

	g_object_unref(reclist_mpool);

	return TRUE;
}

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

	/* Every change in a trie invalidates longest_suffix map.
	 * These prefixes could be updated instead of cleaning the whole graph.
	 */
	purple_trie_states_cleanup(trie);

	rec = purple_memory_pool_alloc(priv->records_obj_mempool,
		sizeof(PurpleTrieRecord), sizeof(gpointer));
	rec->word = purple_memory_pool_strdup(priv->records_str_mempool, word);
	rec->data = data;

	priv->records = purple_record_list_prepend(priv->records_obj_mempool,
		priv->records, rec);

	/* XXX: it won't be called here (it's inefficient), but in search function */
	purple_trie_states_build(trie);
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

	priv->records_obj_mempool = purple_memory_pool_new();
	priv->records_str_mempool = purple_memory_pool_new();
	priv->states_mempool = purple_memory_pool_new_sized(
		PURPLE_TRIE_STATES_POOL_BLOCK_SIZE);
}

static void
purple_trie_finalize(GObject *obj)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(obj);

	g_object_unref(priv->records_obj_mempool);
	g_object_unref(priv->records_str_mempool);
	g_object_unref(priv->states_mempool);

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

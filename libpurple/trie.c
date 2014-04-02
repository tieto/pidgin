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

#include <string.h>

#include "debug.h"
#include "memorypool.h"

#define PURPLE_TRIE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_TRIE, PurpleTriePrivate))

/* A single internal (that don't have any children) consists
 * of 256 + 4 pointers. That's 1040 bytes on 32-bit machine or 2080 bytes
 * on 64-bit.
 *
 * Thus, in 10500-byte pool block we can hold about 5-10 internal states.
 * Threshold of 100 states means, we'd need 10-20 "small" blocks before
 * switching to ~1-2 large blocks.
 */
#define PURPLE_TRIE_LARGE_THRESHOLD 100
#define PURPLE_TRIE_STATES_SMALL_POOL_BLOCK_SIZE 10880
#define PURPLE_TRIE_STATES_LARGE_POOL_BLOCK_SIZE 102400

typedef struct _PurpleTrieRecord PurpleTrieRecord;
typedef struct _PurpleTrieState PurpleTrieState;
typedef struct _PurpleTrieRecordList PurpleTrieRecordList;

typedef struct
{
	gboolean reset_on_match;

	PurpleMemoryPool *records_str_mempool;
	PurpleMemoryPool *records_obj_mempool;
	PurpleTrieRecordList *records;
	GHashTable *records_map;
	gsize records_total_size;

	PurpleMemoryPool *states_mempool;
	PurpleTrieState *root_state;
} PurpleTriePrivate;

struct _PurpleTrieRecord
{
	gchar *word;
	guint word_len;
	gpointer data;
};

struct _PurpleTrieRecordList
{
	PurpleTrieRecord *rec;
	PurpleTrieRecordList *next;
	PurpleTrieRecordList *prev;

	gpointer extra_data;
};

struct _PurpleTrieState
{
	PurpleTrieState *parent;
	PurpleTrieState **children;

	PurpleTrieState *longest_suffix;

	PurpleTrieRecord *found_word;
};

typedef struct
{
	PurpleTrieState *state;

	PurpleTrieState *root_state;
	gboolean reset_on_match;

	PurpleTrieReplaceCb replace_cb;
	gpointer user_data;
} PurpleTrieMachine;

/* TODO: an option to make it eager or lazy (now, it's eager) */
enum
{
	PROP_ZERO,
	PROP_RESET_ON_MATCH,
	PROP_LAST
};

static GObjectClass *parent_class = NULL;
static GParamSpec *properties[PROP_LAST];


/*******************************************************************************
 * Records list
 ******************************************************************************/

static PurpleTrieRecordList *
purple_record_list_new(PurpleMemoryPool *mpool, PurpleTrieRecord *rec)
{
	PurpleTrieRecordList *node;

	node = purple_memory_pool_alloc0(mpool,
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
	if (old_head)
		old_head->prev = new_head;

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
purple_trie_state_new(PurpleTrie *trie, PurpleTrieState *parent, guchar character)
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
			/* PurpleTrieState *children[G_MAXUCHAR + 1] */
			256 * sizeof(gpointer),
			sizeof(gpointer));
	}

	if (parent->children == NULL) {
		purple_memory_pool_free(priv->states_mempool, state);
		g_warn_if_reached();
		return NULL;
	}

	parent->children[character] = state;

	return state;
}

#if 0
static gchar *
purple_trie_print(PurpleTrieState *state, int limit)
{
	GString *str = g_string_new(NULL);
	int i;

	if (limit < 0)
		return g_strdup("{ LIMIT }");

	if (state->found_word)
		g_string_append(str, "*");
	g_string_append(str, "{ ");
	for (i = 0; i < 256; i++) {
		gchar *chp;
		if (!state->children)
			continue;
		if (!state->children[i])
			continue;
		if (i == 0)
			g_string_append(str, "(null)->");
		else
			g_string_append_printf(str, "%c->", i);
		if (state->children[i] == state)
			g_string_append(str, "loop");
		else {
			chp = purple_trie_print(state->children[i], limit - 1);
			g_string_append(str, chp);
			g_string_append_c(str, ' ');
			g_free(chp);
		}
	}
	g_string_append(str, "}");

	return g_string_free(str, FALSE);
}
#endif

static gboolean
purple_trie_states_build(PurpleTrie *trie)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieState *root;
	PurpleMemoryPool *reclist_mpool;
	PurpleTrieRecordList *reclist, *it;
	gulong cur_len;

	g_return_val_if_fail(priv != NULL, FALSE);

	if (priv->root_state != NULL)
		return TRUE;

	if (priv->records_total_size < PURPLE_TRIE_LARGE_THRESHOLD) {
		purple_memory_pool_set_block_size(priv->states_mempool,
			PURPLE_TRIE_STATES_SMALL_POOL_BLOCK_SIZE);
	} else {
		purple_memory_pool_set_block_size(priv->states_mempool,
			PURPLE_TRIE_STATES_LARGE_POOL_BLOCK_SIZE);
	}

	priv->root_state = root = purple_trie_state_new(trie, NULL, '\0');
	g_return_val_if_fail(root != NULL, FALSE);
	g_assert(root->longest_suffix == NULL);

	/* reclist is a list of words not yet added to the trie. Shorter words
	 * are removed from the list, when they are fully added to the trie. */
	reclist_mpool = purple_memory_pool_new();
	reclist = purple_record_list_copy(reclist_mpool, priv->records);

	/* extra_data on every element of reclist will be a pointer to a trie
	 * node -- the prefix of the word with len of cur_len */
	for (it = reclist; it != NULL; it = it->next) {
		it->extra_data = root;
	}

	/* Iterate over indexes of words -- every loop iteration checks certain
	 * index of all remaining words. Loop finishes when there are no words
	 * longer than cur_len. */
	for (cur_len = 0; reclist != NULL; cur_len++) {
		for (it = reclist; it; it = it->next) {
			PurpleTrieRecord *rec = it->rec;
			guchar character = rec->word[cur_len];
			PurpleTrieState *prefix = it->extra_data;
			PurpleTrieState *lon_suf_parent;

			g_assert(character != '\0');

			if (prefix->children && prefix->children[character]) {
				/* Word's prefix is already in the trie, added
				 * by the other word. */
				prefix = prefix->children[character];
			} else {
				/* We need to create a new branch of trie. */
				prefix = purple_trie_state_new(trie, prefix,
					character);
				if (!prefix) {
					g_warn_if_reached();
					g_object_unref(reclist_mpool);
					return FALSE;
				}
			}
			it->extra_data = prefix;
			/* prefix is now of length increased by one character. */

			/* The whole word is now added to the trie. */
			if (rec->word[cur_len + 1] == '\0') {
				if (prefix->found_word == NULL)
					prefix->found_word = rec;
				else {
					purple_debug_warning("trie", "found "
						"a collision of \"%s\" words",
						rec->word);
				}

				/* "it" is not modified here, so it->next is
				 * still valid */
				reclist = purple_record_list_remove(reclist, it);
			}

			/* We need to fill the longest_suffix field -- a longest
			 * complete suffix of the prefix we created. We look for
			 * that suffix in any path starting in root and ending
			 * in the (cur_len - 1) level of trie. */
			if (prefix->longest_suffix != NULL)
				continue;
			lon_suf_parent = prefix->parent->longest_suffix;
			while (lon_suf_parent) {
				if (lon_suf_parent->children &&
					lon_suf_parent->children[character])
				{
					prefix->longest_suffix = lon_suf_parent->
						children[character];
					break;
				}
				lon_suf_parent = lon_suf_parent->longest_suffix;
			}
			if (prefix->longest_suffix == NULL)
				prefix->longest_suffix = root;
			if (prefix->found_word == NULL) {
				prefix->found_word =
					prefix->longest_suffix->found_word;
			}
		}
	}

	g_object_unref(reclist_mpool);

	return TRUE;
}

/*******************************************************************************
 * Searching
 ******************************************************************************/

static void
purple_trie_replace_advance(PurpleTrieMachine *m, const guchar character)
{
	/* change state after processing a character */
	while (TRUE) {
		/* Perfect fit - next character is the same, as the child of the
		 * prefix we reached so far. */
		if (m->state->children && m->state->children[character]) {
			m->state = m->state->children[character];
			break;
		}

		/* We reached root, that's a pity. */
		if (m->state == m->root_state)
			break;

		/* Let's try a bit shorter suffix. */
		m->state = m->state->longest_suffix;
	}
}

static gboolean
purple_trie_replace_do_replacement(PurpleTrieMachine *m, GString *out)
{
	gboolean was_replaced = FALSE;
	gsize str_old_len;

	/* if we reached a "found" state, let's process it */
	if (!m->state->found_word)
		return FALSE;

	/* let's get back to the beginning of the word */
	g_assert(out->len >= m->state->found_word->word_len - 1);
	str_old_len = out->len;
	out->len -= m->state->found_word->word_len - 1;

	was_replaced = m->replace_cb(out, m->state->found_word->word,
		m->state->found_word->data, m->user_data);

	/* output was untouched, revert to the previous position */
	if (!was_replaced)
		out->len = str_old_len;

	if (was_replaced || m->reset_on_match)
		m->state = m->root_state;

	return was_replaced;
}

gchar *
purple_trie_replace(PurpleTrie *trie, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieMachine machine;
	GString *out;
	gsize i;

	if (src == NULL)
		return NULL;

	g_return_val_if_fail(replace_cb != NULL, g_strdup(src));
	g_return_val_if_fail(priv != NULL, NULL);

	purple_trie_states_build(trie);

	machine.state = priv->root_state;
	machine.root_state = priv->root_state;
	machine.reset_on_match = priv->reset_on_match;
	machine.replace_cb = replace_cb;
	machine.user_data = user_data;

	out = g_string_new(NULL);
	i = 0;
	while (src[i] != '\0') {
		guchar character = src[i++];
		gboolean was_replaced;

		purple_trie_replace_advance(&machine, character);
		was_replaced = purple_trie_replace_do_replacement(&machine, out);

		/* We skipped a character without finding any records,
		 * let's just copy it to the output. */
		if (!was_replaced)
			g_string_append_c(out, character);
	}

	return g_string_free(out, FALSE);
}

gchar *
purple_trie_multi_replace(const GSList *tries, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data)
{
	guint tries_count, m_idx;
	PurpleTrieMachine *machines;
	GString *out;
	gsize i;

	if (src == NULL)
		return NULL;

	g_return_val_if_fail(replace_cb != NULL, g_strdup(src));

	tries_count = g_slist_length((GSList*)tries);
	if (tries_count == 0)
		return g_strdup(src);

	/* Initialize all machines. */
	machines = g_new(PurpleTrieMachine, tries_count);
	for (i = 0; i < tries_count; i++, tries = tries->next) {
		PurpleTrie *trie = tries->data;
		PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

		if (priv == NULL) {
			g_warn_if_reached();
			g_free(machines);
			return NULL;
		}

		purple_trie_states_build(trie);

		machines[i].state = priv->root_state;
		machines[i].root_state = priv->root_state;
		machines[i].reset_on_match = priv->reset_on_match;
		machines[i].replace_cb = replace_cb;
		machines[i].user_data = user_data;
	}

	out = g_string_new(NULL);
	i = 0;
	while (src[i] != '\0') {
		guchar character = src[i++];
		gboolean was_replaced = FALSE;

		/* Advance every machine and possibly perform a replacement. */
		for (m_idx = 0; m_idx < tries_count; m_idx++) {
			purple_trie_replace_advance(&machines[m_idx], character);
			if (was_replaced)
				continue;
			was_replaced = purple_trie_replace_do_replacement(
				&machines[m_idx], out);
		}

		/* We skipped a character without finding any records,
		 * let's just copy it to the output. */
		if (!was_replaced)
			g_string_append_c(out, character);

		/* If we replaced a word, reset _all_ machines */
		if (was_replaced) {
			for (m_idx = 0; m_idx < tries_count; m_idx++) {
				machines[m_idx].state =
					machines[m_idx].root_state;
			}
		}
	}

	g_free(machines);
	return g_string_free(out, FALSE);
}


/*******************************************************************************
 * Records
 ******************************************************************************/

gboolean
purple_trie_add(PurpleTrie *trie, const gchar *word, gpointer data)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieRecord *rec;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(word != NULL, FALSE);
	g_return_val_if_fail(word[0] != '\0', FALSE);

	if (g_hash_table_lookup(priv->records_map, word) != NULL) {
		purple_debug_warning("trie", "record exists: %s", word);
		return FALSE;
	}

	/* Every change in a trie invalidates longest_suffix map.
	 * These prefixes could be updated instead of cleaning the whole graph.
	 */
	purple_trie_states_cleanup(trie);

	rec = purple_memory_pool_alloc(priv->records_obj_mempool,
		sizeof(PurpleTrieRecord), sizeof(gpointer));
	rec->word = purple_memory_pool_strdup(priv->records_str_mempool, word);
	rec->word_len = strlen(word);
	g_assert(rec->word_len > 0);
	rec->data = data;

	priv->records_total_size += rec->word_len;
	priv->records = purple_record_list_prepend(priv->records_obj_mempool,
		priv->records, rec);
	g_hash_table_insert(priv->records_map, priv->records, rec->word);

	return TRUE;
}

void
purple_trie_remove(PurpleTrie *trie, const gchar *word)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);
	PurpleTrieRecordList *it;

	g_return_if_fail(priv != NULL);
	g_return_if_fail(word != NULL);
	g_return_if_fail(word[0] != '\0');

	it = g_hash_table_lookup(priv->records_map, word);
	if (it == NULL)
		return;

	/* see purple_trie_add */
	purple_trie_states_cleanup(trie);

	priv->records_total_size -= it->rec->word_len;
	priv->records = purple_record_list_remove(priv->records, it);
	g_hash_table_remove(priv->records_map, it->rec->word);

	purple_memory_pool_free(priv->records_str_mempool, it->rec->word);
	purple_memory_pool_free(priv->records_obj_mempool, it->rec);
	purple_memory_pool_free(priv->records_obj_mempool, it);
}

guint
purple_trie_get_size(PurpleTrie *trie)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(trie);

	g_return_val_if_fail(priv != NULL, 0);

	return g_hash_table_size(priv->records_map);
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
	g_object_notify_by_pspec(G_OBJECT(trie), properties[PROP_RESET_ON_MATCH]);
}

/*******************************************************************************
 * Object stuff
 ******************************************************************************/

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
	priv->states_mempool = purple_memory_pool_new();
	purple_memory_pool_set_block_size(priv->states_mempool,
		PURPLE_TRIE_STATES_SMALL_POOL_BLOCK_SIZE);

	priv->records_map = g_hash_table_new(g_str_hash, g_str_equal);
}

static void
purple_trie_finalize(GObject *obj)
{
	PurpleTriePrivate *priv = PURPLE_TRIE_GET_PRIVATE(obj);

	g_hash_table_destroy(priv->records_map);
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
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

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

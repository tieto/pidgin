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

#ifndef PURPLE_TRIE_H
#define PURPLE_TRIE_H

#include <glib-object.h>

#define PURPLE_TYPE_TRIE (purple_trie_get_type())
#define PURPLE_TRIE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_TRIE, PurpleTrie))
#define PURPLE_TRIE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_TRIE, PurpleTrieClass))
#define PURPLE_IS_TRIE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_TRIE))
#define PURPLE_IS_TRIE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_TRIE))
#define PURPLE_TRIE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_TRIE, PurpleTrieClass))

typedef struct _PurpleTrie PurpleTrie;
typedef struct _PurpleTrieClass PurpleTrieClass;

struct _PurpleTrie
{
	/*< private >*/
	GObject parent_instance;
};

struct _PurpleTrieClass
{
	/*< private >*/
	GObjectClass parent_class;

	void (*purple_reserved1)(void);
	void (*purple_reserved2)(void);
	void (*purple_reserved3)(void);
	void (*purple_reserved4)(void);
};

G_BEGIN_DECLS

GType
purple_trie_get_type(void);

/**
 * purple_trie_new:
 *
 * Creates a new trie.
 *
 * Returns: The new #PurpleTrie.
 */
PurpleTrie *
purple_trie_new(void);

/**
 * purple_trie_get_reset_on_match:
 * @trie: The trie.
 *
 * Checks, if the trie will reset its internal state after every match.
 *
 * Returns: %TRUE, if trie will reset, %FALSE otherwise.
 */
gboolean
purple_trie_get_reset_on_match(PurpleTrie *trie);

/**
 * purple_trie_set_reset_on_match:
 * @trie: The trie.
 * @reset: %TRUE, if trie should reset, %FALSE otherwise.
 *
 * Enables or disables a feature of resetting trie's state after every match.
 * When enabled, it will not search for overlapping matches.
 */
void
purple_trie_set_reset_on_match(PurpleTrie *trie, gboolean reset);

/**
 * purple_trie_add:
 * @trie: The trie.
 * @word: The word.
 * @data: The word-related data (may be %NULL).
 *
 * Adds a word to the trie.
 */
void
purple_trie_add(PurpleTrie *trie, const gchar *word, gpointer data);

G_END_DECLS

#endif /* PURPLE_MEMORY_POOL_H */

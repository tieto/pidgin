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

/**
 * PurpleTrieReplaceCb:
 * @out: Currently built output string, append replacement to it.
 * @word: Found word.
 * @word_data: A user data bound with this word, when added with
 *             purple_trie_add().
 * @user_data: A user supplied data passed when calling purple_trie_replace().
 *
 * A funtion called after every found matching substring to be replaced.
 *
 * Returns: %TRUE, if the word was replaced, %FALSE otherwise. In the second
 *          case you must not modify @out.
 */
typedef gboolean (*PurpleTrieReplaceCb)(GString *out, const gchar *word,
	gpointer word_data, gpointer user_data);

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
 *
 * Returns: %TRUE if succeeded, %FALSE otherwise (possibly on duplicate)
 */
gboolean
purple_trie_add(PurpleTrie *trie, const gchar *word, gpointer data);

/**
 * purple_trie_remove:
 * @trie: The trie.
 * @word: The word.
 *
 * Removes a word from the trie. Depending on used memory pool, this may not
 * free allocated memory (that will be freed when destroying the whole
 * collection), so use it wisely.
 */
void
purple_trie_remove(PurpleTrie *trie, const gchar *word);

guint
purple_trie_get_size(PurpleTrie *trie);

/**
 * purple_trie_replace:
 * @trie: The trie.
 * @src: The source string.
 * @replace_cb: The replacement function.
 * @user_data: Custom data to be passed to @replace_cb.
 *
 * Processes @src string and replaces all occuriences of words added to @trie.
 * It's O(strlen(src)), if replace_cb runs in O(strlen(word)).
 *
 * Returns: resulting string. Must be g_free'd when you are done using it.
 */
gchar *
purple_trie_replace(PurpleTrie *trie, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data);

/**
 * purple_trie_multi_replace:
 * @tries: The list of tries.
 * @src: The source string.
 * @replace_cb: The replacement function.
 * @user_data: Custom data to be passed to @replace_cb.
 *
 * Processes @src and replaces all occuriences of words added to tries in list
 * @tries. Entries added to tries on the beginning of the list have higher
 * priority, than ones added further.
 *
 * Differend GSList's can be combined to possess common parts, so you can create
 * a "tree of tries".
 *
 * Returns: resulting string. Must be g_free'd when you are done using it.
 */
gchar *
purple_trie_multi_replace(const GSList *tries, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data);

G_END_DECLS

#endif /* PURPLE_MEMORY_POOL_H */

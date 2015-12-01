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
/**
 * SECTION:trie
 * @include:trie.h
 * @section_id: libpurple-trie
 * @short_description: a structure for linear-time text searching
 * @title: Tries
 *
 * A #PurpleTrie is a structure for quick searching of multiple phrases within
 * a text. It's intended for repeated searches of the same set of patterns
 * within multiple source texts (or a single, big one).
 *
 * It's preparation time is <literal>O(p)</literal>, where <literal>p</literal>
 * is the total length of searched phrases. In current implementation, the
 * internal structure is invalidated after every modification of the
 * #PurpleTrie's contents, so it's not efficient to do alternating modifications
 * and searches. Search time does not depend on patterns being stored within
 * a trie and is always <literal>O(n)</literal>, where <literal>n</literal> is
 * the size of a text.
 *
 * Its main drawback is a significant memory usage - every internal trie node
 * needs about 1kB of memory on 32-bit machine and 2kB on 64-bit. Fortunately,
 * the trie grows slower when more words (with common prefixes) are added.
 * We could avoid invalidating the whole tree when altering it, but it would
 * require figuring out, how to update <literal>longest_suffix</literal> fields
 * in satisfying time.
 */

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

/**
 * PurpleTrie:
 *
 * The trie object instance.
 */
struct _PurpleTrie
{
	/*< private >*/
	GObject parent_instance;
};

/**
 * PurpleTrieClass:
 *
 * Base class for #PurpleTrie objects.
 */
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
 * @out: currently built output string, append replacement to it.
 * @word: found word.
 * @word_data: the user data bound with this word, when added with
 *             #purple_trie_add.
 * @user_data: the user supplied data passed when calling #purple_trie_replace.
 *
 * A funtion called on every matching substring to be replaced.
 *
 * If you decide to replace the word, append your text to @out and return %TRUE.
 * Otherwise, you must not touch @out. In both cases, you must not do any
 * operations on @out other than appending text to it.
 *
 * Returns: %TRUE if the word was replaced, %FALSE otherwise.
 */
typedef gboolean (*PurpleTrieReplaceCb)(GString *out, const gchar *word,
	gpointer word_data, gpointer user_data);

/**
 * PurpleTrieFindCb:
 * @word: found word.
 * @word_data: the user data bound with this word, when added with
 *             #purple_trie_add.
 * @user_data: the user data passed when calling #purple_trie_find.
 *
 * A function called on every matching substring.
 *
 * You can decide to count the match or not (for the total number of found
 * words, that is returned by #purple_trie_find). In both cases you can
 * obviously do some processing outside the #PurpleTrie object.
 *
 * If you decide to count the word and #PurpleTrie:reset-on-match property
 * is set, no overlapping words will be found - the processing will skip after
 * the end of this word.
 *
 * Returns: %TRUE if the word should be counter, %FALSE otherwise.
 */
typedef gboolean (*PurpleTrieFindCb)(const gchar *word, gpointer word_data,
	gpointer user_data);

G_BEGIN_DECLS

/**
 * purple_trie_get_type:
 *
 * Returns: the #GType for a #PurpleTrie.
 */
GType
purple_trie_get_type(void);

/**
 * purple_trie_new:
 *
 * Creates a new trie.
 *
 * Returns: the new #PurpleTrie.
 */
PurpleTrie *
purple_trie_new(void);

/**
 * purple_trie_get_reset_on_match:
 * @trie: the trie.
 *
 * Checks, if the trie will reset its internal state after every match.
 *
 * Returns: %TRUE, if trie will reset, %FALSE otherwise.
 */
gboolean
purple_trie_get_reset_on_match(PurpleTrie *trie);

/**
 * purple_trie_set_reset_on_match:
 * @trie: the trie.
 * @reset: %TRUE, if trie should reset, %FALSE otherwise.
 *
 * Enables or disables a feature of resetting trie's state after every match.
 * When enabled, it will not search for overlapping matches.
 *
 * It's well defined for #purple_trie_find, but not for replace operations.
 * Thus, for the latter, it's better to stay with this option enabled, because
 * its behavior may be changed in future.
 */
void
purple_trie_set_reset_on_match(PurpleTrie *trie, gboolean reset);

/**
 * purple_trie_add:
 * @trie: the trie.
 * @word: the word.
 * @data: the word-related data (may be %NULL).
 *
 * Adds a word to the trie. Current implementation doesn't allow for duplicates,
 * so please avoid adding those.
 *
 * Please note, that altering a trie invalidates its internal structure, so by
 * the occasion of next search, it will be rebuilt. It's done in
 * <literal>O(n)</literal>, where n is the total length of strings
 * in #PurpleTrie.
 *
 * Returns: %TRUE if succeeded, %FALSE otherwise.
 */
gboolean
purple_trie_add(PurpleTrie *trie, const gchar *word, gpointer data);

/**
 * purple_trie_remove:
 * @trie: the trie.
 * @word: the word.
 *
 * Removes a word from the trie. Depending on used memory pool, this may not
 * free allocated memory (that will be freed when destroying the whole
 * collection), so use it wisely. See #purple_memory_pool_free.
 *
 * Please note, that altering a trie invalidates its internal structure.
 * See #purple_trie_add.
 */
void
purple_trie_remove(PurpleTrie *trie, const gchar *word);

/**
 * purple_trie_get_size:
 * @trie: the trie.
 *
 * Returns the number of elements contained in the #PurpleTrie.
 *
 * Returns: the number of stored words in @trie.
 */
guint
purple_trie_get_size(PurpleTrie *trie);

/**
 * purple_trie_replace:
 * @trie: the trie.
 * @src: the source string.
 * @replace_cb: (scope call): the replacement function.
 * @user_data: custom data to be passed to @replace_cb.
 *
 * Processes @src string and replaces all occuriences of words added to @trie.
 * It's <literal>O(strlen(src))</literal>, if @replace_cb runs in
 * <literal>O(strlen(word))</literal> and #PurpleTrie:reset-on-match is set.
 *
 * Returns: resulting string. Must be #g_free'd when you are done using it.
 */
gchar *
purple_trie_replace(PurpleTrie *trie, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data);

/**
 * purple_trie_multi_replace:
 * @tries: (element-type PurpleTrie): the list of tries.
 * @src: the source string.
 * @replace_cb: (scope call): the replacement function.
 * @user_data: custom data to be passed to @replace_cb.
 *
 * Processes @src and replaces all occuriences of words added to tries in list
 * @tries. Entries added to tries on the beginning of the list have higher
 * priority, than ones added further.
 *
 * Different #GSList's can be combined to possess common parts, so you can create
 * a "tree of tries".
 *
 * Returns: resulting string. Must be #g_free'd when you are done using it.
 */
gchar *
purple_trie_multi_replace(const GSList *tries, const gchar *src,
	PurpleTrieReplaceCb replace_cb, gpointer user_data);

/**
 * purple_trie_find:
 * @trie: the trie.
 * @src: the source string.
 * @find_cb: (nullable) (scope call): the callback for the found entries (may be %NULL).
 * @user_data: custom data to be passed to @find_cb.
 *
 * Processes @src string and finds all occuriences of words added to @trie.
 * It's <literal>O(strlen(src))</literal>, if find_cb runs
 * in <literal>O(1)</literal>.
 *
 * The word is counted as found if it's found and the callback returns %TRUE.
 *
 * Returns: the number of found words.
 */
gulong
purple_trie_find(PurpleTrie *trie, const gchar *src,
	PurpleTrieFindCb find_cb, gpointer user_data);

/**
 * purple_trie_multi_find:
 * @tries: (element-type PurpleTrie): the list of tries.
 * @src: the source string.
 * @find_cb: (nullable) (scope call): the callback for the found entries (may be %NULL).
 * @user_data: custom data to be passed to @find_cb.
 *
 * Processes @src and replaces all occuriences of words added to tries in
 * list @tries. Entries added to tries on the beginning of the list have higher
 * priority, than ones added further.
 *
 * Different #GSList's can be combined to possess common parts, so you can create
 * a "tree of tries".
 *
 * Returns: the number of found words.
 */
gulong
purple_trie_multi_find(const GSList *tries, const gchar *src,
	PurpleTrieFindCb find_cb, gpointer user_data);

G_END_DECLS

#endif /* PURPLE_MEMORY_POOL_H */

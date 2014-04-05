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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "smiley-parser.h"

#include "smiley-custom.h"
#include "smiley-theme.h"

#define DISPLAY_OUR_CUSTOM_SMILEYS_FOR_INCOMING_MESSAGES 1

typedef struct
{
	PurpleConversation *conv;
	PurpleSmileyParseCb cb;
	gpointer ui_data;
} PurpleSmileyParseData;

static gboolean purple_smiley_parse_cb(GString *out, const gchar *word,
	gpointer _smiley, gpointer _parse_data)
{
	PurpleSmileyParseData *parse_data = _parse_data;
	PurpleSmiley *smiley = _smiley;

	parse_data->cb(out, smiley, parse_data->conv, parse_data->ui_data);

	return TRUE;
}

gchar *
purple_smiley_parse(PurpleConversation *conv, const gchar *html_message,
	gboolean use_remote_smileys, PurpleSmileyParseCb cb, gpointer ui_data)
{
	PurpleSmileyTheme *theme;
	PurpleSmileyList *theme_smileys = NULL, *remote_smileys = NULL;
	PurpleTrie *theme_trie = NULL, *custom_trie = NULL, *remote_trie = NULL;
	GSList *tries = NULL, tries_theme, tries_custom, tries_remote;
	PurpleSmileyParseData parse_data;

	if (html_message == NULL || html_message[0] == '\0')
		return g_strdup(html_message);

	/* get remote smileys */
	if (use_remote_smileys)
		remote_smileys = purple_conversation_get_remote_smileys(conv);
	if (remote_smileys)
		remote_trie = purple_smiley_list_get_trie(remote_smileys);
	if (remote_trie && purple_trie_get_size(remote_trie) == 0)
		remote_trie = NULL;

	/* get custom smileys */
	if (purple_conversation_get_features(conv) &
		PURPLE_CONNECTION_FLAG_ALLOW_CUSTOM_SMILEY)
	{
		custom_trie = purple_smiley_list_get_trie(
			purple_smiley_custom_get_list());
#if !DISPLAY_OUR_CUSTOM_SMILEYS_FOR_INCOMING_MESSAGES
		if (use_remote_smileys)
			custom_trie = NULL;
#endif
	}
	if (custom_trie && purple_trie_get_size(custom_trie) == 0)
		custom_trie = NULL;

	/* get theme smileys */
	theme = purple_smiley_theme_get_current();
	if (theme != NULL)
		theme_smileys = purple_smiley_theme_get_smileys(theme, ui_data);
	if (theme_smileys != NULL)
		theme_trie = purple_smiley_list_get_trie(theme_smileys);

	/* we have absolutely no smileys */
	if (theme_trie == NULL && custom_trie == NULL && remote_trie == NULL)
		return g_strdup(html_message);

	/* Create a tries list on the stack. */
	tries_theme.data = theme_trie;
	tries_custom.data = custom_trie;
	tries_remote.data = remote_trie;
	tries_theme.next = tries_custom.next = tries_remote.next = NULL;
	if (remote_trie != NULL)
		tries = &tries_remote;
	if (custom_trie != NULL) {
		if (tries)
			g_slist_last(tries)->next = &tries_custom;
		else
			tries = &tries_custom;
	}
	if (theme_trie != NULL) {
		if (tries)
			g_slist_last(tries)->next = &tries_theme;
		else
			tries = &tries_theme;
	}

	parse_data.conv = conv;
	parse_data.cb = cb;
	parse_data.ui_data = ui_data;

	/* TODO: don't replace text within tags, ie. <span style=":)"> */
	/* TODO: parse greedily (as much as possible) when PurpleTrie
	 * provides support for it. */
	return purple_trie_multi_replace(tries, html_message,
		purple_smiley_parse_cb, &parse_data);
}

static gboolean
smiley_find_cb(const gchar *word, gpointer _smiley, gpointer _found_smileys)
{
	PurpleSmiley *smiley = _smiley;
	GHashTable *found_smileys = _found_smileys;

	g_hash_table_insert(found_smileys, smiley, smiley);

	return TRUE;
}

GList *
purple_smiley_find(PurpleSmileyList *smileys, const gchar *message,
	gboolean is_html)
{
	PurpleTrie *trie;
	GHashTable *found_smileys;
	GList *found_list;
	gchar *escaped_message = NULL;

	if (message == NULL || message[0] == '\0')
		return NULL;

	if (smileys == NULL || purple_smiley_list_is_empty(smileys))
		return NULL;

	trie = purple_smiley_list_get_trie(smileys);
	g_return_val_if_fail(trie != NULL, NULL);

	if (!is_html)
		message = escaped_message = g_markup_escape_text(message, -1);

	found_smileys = g_hash_table_new(g_direct_hash, g_direct_equal);
	purple_trie_find(trie, message, smiley_find_cb, found_smileys);

	g_free(escaped_message);

	found_list = g_hash_table_get_values(found_smileys);
	g_hash_table_destroy(found_smileys);

	return found_list;
}

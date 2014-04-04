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

typedef struct
{
	PurpleSmileyParseCb cb;
	gpointer ui_data;
} PurpleSmileyParseData;

static gboolean escape_checked = FALSE;
static gboolean escape_value;

gboolean
purple_smiley_parse_escape(void)
{
	GHashTable *ui_info;

	if (escape_checked)
		return escape_value;

	ui_info = purple_core_get_ui_info();
	if (!ui_info)
		escape_value = FALSE;
	else {
		escape_value = GPOINTER_TO_INT(g_hash_table_lookup(ui_info,
			"smiley-parser-escape"));
	}

	escape_checked = TRUE;
	return escape_value;
}

static gboolean purple_smiley_parse_cb(GString *out, const gchar *word,
	gpointer _smiley, gpointer _parse_data)
{
	PurpleSmileyParseData *parse_data = _parse_data;
	PurpleSmiley *smiley = _smiley;

	parse_data->cb(out, smiley, parse_data->ui_data);

	return TRUE;
}

gchar *
purple_smiley_parse(PurpleConversation *conv, const gchar *message,
	PurpleSmileyParseCb cb, gpointer ui_data)
{
	PurpleSmileyTheme *theme;
	PurpleSmileyList *theme_smileys = NULL, *remote_smileys = NULL;
	PurpleTrie *theme_trie = NULL, *custom_trie, *remote_trie = NULL;
	GSList *tries = NULL, tries_theme, tries_custom, tries_remote;
	PurpleSmileyParseData parse_data;

	if (message == NULL || message[0] == '\0')
		return g_strdup(message);

	/* get remote smileys */
	remote_smileys = purple_conversation_get_remote_smileys(conv);
	if (remote_smileys)
		remote_trie = purple_smiley_list_get_trie(remote_smileys);
	if (remote_trie && purple_trie_get_size(remote_trie) == 0)
		remote_trie = NULL;

	/* get custom smileys */
	custom_trie = purple_smiley_list_get_trie(
		purple_smiley_custom_get_list());
	if (purple_trie_get_size(custom_trie) == 0)
		custom_trie = NULL;

	/* get theme smileys */
	theme = purple_smiley_theme_get_current();
	if (theme != NULL)
		theme_smileys = purple_smiley_theme_get_smileys(theme, ui_data);
	if (theme_smileys != NULL)
		theme_trie = purple_smiley_list_get_trie(theme_smileys);

	/* we have absolutely no smileys */
	if (theme_trie == NULL && custom_trie == NULL && remote_trie == NULL)
		return g_strdup(message);

	/* Create a tries list on the stack. */
	tries_theme.data = theme_trie;
	tries_custom.data = custom_trie;
	tries_remote.data = remote_trie;
	tries_theme.next = tries_custom.next = tries_remote.next = NULL;
	if (remote_trie != NULL)
		tries = &tries_remote;
	if (custom_trie != NULL) {
		if (tries)
			tries->next = &tries_custom;
		else
			tries = &tries_custom;
	}
	if (theme_trie != NULL) {
		if (tries)
			tries->next = &tries_theme;
		else
			tries = &tries_theme;
	}

	/* XXX: should we parse custom smileys,
	 * if protocol doesn't support it? */

	parse_data.cb = cb;
	parse_data.ui_data = ui_data;

	/* TODO: don't replace text within tags, ie. <span style=":)"> */
	/* TODO: parse greedily (as much as possible) when PurpleTrie
	 * provides support for it. */
	return purple_trie_multi_replace(tries, message,
		purple_smiley_parse_cb, &parse_data);
}

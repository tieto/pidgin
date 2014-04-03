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
	gpointer _smiley, gpointer _unused)
{
	PurpleSmiley *smiley = _smiley;

	g_string_append_printf(out, "<img alt=\"%s\" src=\"%s\" />",
		word, purple_smiley_get_path(smiley));

	return TRUE;
}

gchar *
purple_smiley_parse(const gchar *message, gpointer ui_data)
{
	PurpleSmileyTheme *theme;
	PurpleSmileyList *theme_smileys = NULL;
	PurpleTrie *theme_trie = NULL, *custom_trie;
	GSList *tries = NULL, tries_theme, tries_custom;

	if (message == NULL || message[0] == '\0')
		return g_strdup(message);

	custom_trie = purple_smiley_list_get_trie(
		purple_smiley_custom_get_list());
	if (purple_trie_get_size(custom_trie) == 0)
		custom_trie = NULL;

	theme = purple_smiley_theme_get_current();
	if (theme != NULL)
		theme_smileys = purple_smiley_theme_get_smileys(theme, ui_data);

	if (theme_smileys != NULL)
		theme_trie = purple_smiley_list_get_trie(theme_smileys);

	if (theme_trie == NULL && custom_trie == NULL)
		return g_strdup(message);

	/* Create a tries list on stack. */
	tries_theme.data = theme_trie;
	tries_custom.data = custom_trie;
	tries_theme.next = tries_custom.next = NULL;
	if (custom_trie != NULL)
		tries = &tries_custom;
	if (theme_trie != NULL) {
		if (tries)
			tries->next = &tries_theme;
		else
			tries = &tries_theme;
	}

	/* XXX: should we parse custom smileys,
	 * if protocol doesn't support it? */

	/* TODO: don't replace text within tags, ie. <span style=":)"> */
	/* TODO: parse greedily (as much as possible) when PurpleTrie
	 * provides support for it. */
	return purple_trie_multi_replace(tries, message,
		purple_smiley_parse_cb, NULL);
}

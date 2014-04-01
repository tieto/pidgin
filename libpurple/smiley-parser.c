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

#include "smiley-theme.h"

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
	PurpleSmileyList *theme_smileys;
	PurpleTrie *theme_trie;

	if (message == NULL || message[0] == '\0')
		return g_strdup(message);

	theme = purple_smiley_theme_get_current();
	if (theme == NULL)
		return g_strdup(message);

	theme_smileys = purple_smiley_theme_get_smileys(theme, ui_data);
	if (theme_smileys == NULL)
		return g_strdup(message);

	theme_trie = purple_smiley_list_get_trie(theme_smileys);
	g_return_val_if_fail(theme_trie != NULL, g_strdup(message));

	/* TODO: don't replace text within tags, ie. <span style=":)"> */
	return purple_trie_replace(theme_trie, message,
		purple_smiley_parse_cb, NULL);
}

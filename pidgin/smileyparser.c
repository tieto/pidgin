/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include <gtk/gtk.h>
#include <debug.h>
#include "smileyparser.h"
#include <smiley.h>
#include <string.h>
#include "gtkthemes.h"

static char *
get_fullpath(const char *filename)
{
	if (g_path_is_absolute(filename))
		return g_strdup(filename);
	else
		return g_build_path(g_get_current_dir(), filename, NULL);
}

static void
parse_for_shortcut_plaintext(const char *text, const char *shortcut, const char *file, GString *ret)
{
	const char *tmp = text;

	for (;*tmp;) {
		const char *end = strstr(tmp, shortcut);
		char *path;
		char *escaped_path;

		if (end == NULL) {
			g_string_append(ret, tmp);
			break;
		}
		path = get_fullpath(file);
		escaped_path = g_markup_escape_text(path, -1);

		g_string_append_len(ret, tmp, end-tmp);
		g_string_append_printf(ret,"<img alt='%s' src='%s' />",
					shortcut, escaped_path);
		g_free(path);
		g_free(escaped_path);
		g_assert(strlen(tmp) >= strlen(shortcut));
		tmp = end + strlen(shortcut);
	}
}

static char *
parse_for_shortcut(const char *markup, const char *shortcut, const char *file)
{
	GString* ret = g_string_new("");
	char *local_markup = g_strdup(markup);
	char *escaped_shortcut = g_markup_escape_text(shortcut, -1);

	char *temp = local_markup;

	for (;*temp;) {
		char *end = strchr(temp, '<');
		char *end_of_tag;

		if (!end) {
			parse_for_shortcut_plaintext(temp, escaped_shortcut, file, ret);
			break;
		}

		*end = 0;
		parse_for_shortcut_plaintext(temp, escaped_shortcut, file, ret);
		*end = '<';

		/* if this is well-formed, then there should be no '>' within
		 * the tag. TODO: handle a comment tag better :( */
		end_of_tag = strchr(end, '>');
		if (!end_of_tag) {
			g_string_append(ret, end);
			break;
		}

		g_string_append_len(ret, end, end_of_tag - end + 1);

		temp = end_of_tag + 1;
	}
	g_free(local_markup);
	g_free(escaped_shortcut);
	return g_string_free(ret, FALSE);
}

static char *
parse_for_purple_smiley(const char *markup, PurpleSmiley *smiley)
{
	char *file = purple_smiley_get_full_path(smiley);
	char *ret = parse_for_shortcut(markup, purple_smiley_get_shortcut(smiley), file);
	g_free(file);
	return ret;
}

static char *
parse_for_smiley_list(const char *markup, GHashTable *smileys)
{
	GHashTableIter iter;
	char *key, *value;
	char *ret = g_strdup(markup);

	g_hash_table_iter_init(&iter, smileys);
	while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value))
	{
		char *temp = parse_for_shortcut(ret, key, value);
		g_free(ret);
		ret = temp;
	}

	return ret;
}

char *
smiley_parse_markup(const char *markup, const char *proto_id)
{
	GList *smileys = purple_smileys_get_all();
	char *temp = g_strdup(markup), *temp2;
	struct smiley_list *list;
	const char *proto_name = "default";

	if (proto_id != NULL) {
		PurplePluginProtocolInfo *prpl_info;
		prpl_info = purple_find_protocol_info(proto_id);
		proto_name = prpl_info->name;
	}

	/* unnecessarily slow, but lets manage for now. */
	for (; smileys; smileys = g_list_next(smileys)) {
		temp2 = parse_for_purple_smiley(temp, PURPLE_SMILEY(smileys->data));
		g_free(temp);
		temp = temp2;
	}

	/* now for each theme smiley, observe that this does look nasty */
	if (!current_smiley_theme) {
		purple_debug_warning("smiley", "theme does not exist\n");
		return temp;
	}

	for (list = current_smiley_theme->list; list; list = list->next) {
		if (g_str_equal(list->sml, proto_name)) {
			temp2 = parse_for_smiley_list(temp, list->files);
			g_free(temp);
			temp = temp2;
		}
	}

	return temp;
}


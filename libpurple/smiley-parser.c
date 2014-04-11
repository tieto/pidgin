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
	union {
		struct {
			PurpleConversation *conv;
			PurpleSmileyParseCb cb;
			gpointer ui_data;
		} replace;
		struct {
			GHashTable *found_smileys;
		} find;
	} job;

	gboolean in_html_tag;
} PurpleSmileyParseData;

static PurpleTrie *html_sentry;

static inline void
purple_smiley_parse_check_html(const gchar *word,
	PurpleSmileyParseData *parse_data)
{
	if (G_LIKELY(word[0] == '<'))
		parse_data->in_html_tag = TRUE;
	else if (G_LIKELY(word[0] == '>'))
		parse_data->in_html_tag = FALSE;
	else
		g_return_if_reached();
	g_warn_if_fail(word[1] == '\0');
}

static gboolean
purple_smiley_parse_cb(GString *out, const gchar *word, gpointer _smiley,
	gpointer _parse_data)
{
	PurpleSmileyParseData *parse_data = _parse_data;
	PurpleSmiley *smiley = _smiley;

	/* a special-case for html_sentry */
	if (smiley == NULL) {
		purple_smiley_parse_check_html(word, parse_data);
		return FALSE;
	}

	if (parse_data->in_html_tag)
		return FALSE;

	return parse_data->job.replace.cb(out, smiley,
		parse_data->job.replace.conv, parse_data->job.replace.ui_data);
}

/* XXX: this shouldn't be a conv for incoming messages - see
 * PurpleConversationPrivate.remote_smileys.
 * For outgoing messages, we could pass conv in ui_data (or something).
 * Or maybe we should use two distinct functions?
 * To be reconsidered when we had PurpleDude-like objects.
 */
gchar *
purple_smiley_parser_smileify(PurpleConversation *conv, const gchar *html_message,
	gboolean use_remote_smileys, PurpleSmileyParseCb cb, gpointer ui_data)
{
	PurpleSmileyTheme *theme;
	PurpleSmileyList *theme_smileys = NULL, *remote_smileys = NULL;
	PurpleTrie *theme_trie = NULL, *custom_trie = NULL, *remote_trie = NULL;
	GSList *tries = NULL;
	GSList tries_sentry, tries_theme, tries_custom, tries_remote;
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
	tries_sentry.data = html_sentry;
	tries_theme.data = theme_trie;
	tries_custom.data = custom_trie;
	tries_remote.data = remote_trie;
	tries_sentry.next = NULL;
	tries_theme.next = tries_custom.next = tries_remote.next = NULL;
	tries = &tries_sentry;
	if (remote_trie != NULL)
		g_slist_last(tries)->next = &tries_remote;
	if (custom_trie != NULL)
		g_slist_last(tries)->next = &tries_custom;
	if (theme_trie != NULL)
		g_slist_last(tries)->next = &tries_theme;

	parse_data.job.replace.conv = conv;
	parse_data.job.replace.cb = cb;
	parse_data.job.replace.ui_data = ui_data;
	parse_data.in_html_tag = FALSE;

	/* TODO: parse greedily (as much as possible) when PurpleTrie
	 * provides support for it. */
	return purple_trie_multi_replace(tries, html_message,
		purple_smiley_parse_cb, &parse_data);
}

gchar *
purple_smiley_parser_replace(PurpleSmileyList *smileys,
	const gchar *html_message, PurpleSmileyParseCb cb, gpointer ui_data)
{
	PurpleTrie *smileys_trie = NULL;
	GSList trie_1, trie_2;
	PurpleSmileyParseData parse_data;

	if (html_message == NULL || html_message[0] == '\0')
		return g_strdup(html_message);

	if (smileys == NULL || purple_smiley_list_is_empty(smileys))
		return g_strdup(html_message);

	smileys_trie = purple_smiley_list_get_trie(smileys);
	g_return_val_if_fail(smileys_trie != NULL, NULL);

	trie_1.data = html_sentry;
	trie_2.data = smileys_trie;
	trie_1.next = &trie_2;
	trie_2.next = NULL;

	parse_data.job.replace.conv = NULL;
	parse_data.job.replace.cb = cb;
	parse_data.job.replace.ui_data = ui_data;
	parse_data.in_html_tag = FALSE;

	return purple_trie_multi_replace(&trie_1, html_message,
		purple_smiley_parse_cb, &parse_data);
}

static gboolean
smiley_find_cb(const gchar *word, gpointer _smiley, gpointer _parse_data)
{
	PurpleSmiley *smiley = _smiley;
	PurpleSmileyParseData *parse_data = _parse_data;

	/* a special-case for html_sentry */
	if (smiley == NULL) {
		purple_smiley_parse_check_html(word, parse_data);
		return FALSE;
	}

	if (parse_data->in_html_tag)
		return FALSE;

	g_hash_table_insert(parse_data->job.find.found_smileys, smiley, smiley);

	return TRUE;
}

GList *
purple_smiley_parser_find(PurpleSmileyList *smileys, const gchar *message,
	gboolean is_html)
{
	PurpleTrie *smileys_trie;
	GList *found_list;
	gchar *escaped_message = NULL;
	PurpleSmileyParseData parse_data;
	GSList trie_1, trie_2;

	if (message == NULL || message[0] == '\0')
		return NULL;

	if (smileys == NULL || purple_smiley_list_is_empty(smileys))
		return NULL;

	smileys_trie = purple_smiley_list_get_trie(smileys);
	g_return_val_if_fail(smileys_trie != NULL, NULL);

	if (!is_html)
		message = escaped_message = g_markup_escape_text(message, -1);

	parse_data.job.find.found_smileys =
		g_hash_table_new(g_direct_hash, g_direct_equal);
	parse_data.in_html_tag = FALSE;

	trie_1.data = html_sentry;
	trie_2.data = smileys_trie;
	trie_1.next = &trie_2;
	trie_2.next = NULL;
	purple_trie_multi_find(&trie_1, message, smiley_find_cb, &parse_data);

	g_free(escaped_message);

	found_list = g_hash_table_get_values(parse_data.job.find.found_smileys);
	g_hash_table_destroy(parse_data.job.find.found_smileys);

	return found_list;
}

void
_purple_smiley_parser_init(void)
{
	html_sentry = purple_trie_new();
	purple_trie_add(html_sentry, "<", NULL);
	purple_trie_add(html_sentry, ">", NULL);
}

void
_purple_smiley_parser_uninit(void)
{
	g_object_unref(html_sentry);
}

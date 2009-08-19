/**
 * @file gnttinyurl.c
 *
 * Copyright (C) 2009 Richard Nelson <wabz@whatsbeef.net>
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
 */


#include "internal.h"
#include <glib.h>

#define PLUGIN_STATIC_NAME	TinyURL
#define PREFS_BASE          "/plugins/gnt/tinyurl"
#define PREF_LENGTH  PREFS_BASE "/length"
#define PREF_URL  PREFS_BASE "/url"


#include <conversation.h>
#include <signals.h>

#include <glib.h>

#include <plugin.h>
#include <version.h>
#include <debug.h>
#include <notify.h>

#include <gntconv.h>

#include <gntplugin.h>
#include <gnttextview.h>

static int tag_num = 0;

typedef struct
{
	PurpleConversation *conv;
	gchar *tag;
	int num;
} CbInfo;

/* 3 functions from util.c */
static gboolean
badchar(char c)
{
	switch (c) {
	case ' ':
	case ',':
	case '\0':
	case '\n':
	case '\r':
	case '<':
	case '>':
	case '"':
	case '\'':
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
badentity(const char *c)
{
	if (!g_ascii_strncasecmp(c, "&lt;", 4) ||
		!g_ascii_strncasecmp(c, "&gt;", 4) ||
		!g_ascii_strncasecmp(c, "&quot;", 6)) {
		return TRUE;
	}
	return FALSE;
}

static GList *extract_urls(char *text) {
	const char *t, *c, *q = NULL;
	char *url_buf;
	GList *ret = NULL;
	gboolean inside_html = FALSE;
	int inside_paren = 0;
	c = text;
	while (*c) {
		if (*c == '(' && !inside_html) {
			inside_paren++;
			c++;
		}
		if (inside_html) {
			if (*c == '>') {
				inside_html = FALSE;
			} else if (!q && (*c == '\"' || *c == '\'')) {
				q = c;
			} else if(q) {
				if(*c == *q)
					q = NULL;
			}
		} else if (*c == '<') {
			inside_html = TRUE;
			if (!g_ascii_strncasecmp(c, "<A", 2)) {
				while (1) {
					if (*c == '>') {
						inside_html = FALSE;
						break;
					}
					c++;
					if (!(*c))
						break;
				}
			}
		} else if ((*c=='h') && (!g_ascii_strncasecmp(c, "http://", 7) ||
					(!g_ascii_strncasecmp(c, "https://", 8)))) {
			t = c;
			while (1) {
				if (badchar(*t) || badentity(t)) {

					if ((!g_ascii_strncasecmp(c, "http://", 7) && (t - c == 7)) ||
						(!g_ascii_strncasecmp(c, "https://", 8) && (t - c == 8))) {
						break;
					}

					if (*(t) == ',' && (*(t + 1) != ' ')) {
						t++;
						continue;
					}

					if (*(t - 1) == '.')
						t--;
					if ((*(t - 1) == ')' && (inside_paren > 0))) {
						t--;
					}

					url_buf = g_strndup(c, t - c);
					if (!g_list_find_custom(ret, url_buf, (GCompareFunc)strcmp)) {
						purple_debug_info("TinyURL", "Added URL %s\n", url_buf);
						ret = g_list_append(ret, g_strdup(url_buf));
					}
					c = t;
					break;
				}
				t++;

			}
		} else if (!g_ascii_strncasecmp(c, "www.", 4) && (c == text || badchar(c[-1]) || badentity(c-1))) {
			if (c[4] != '.') {
				t = c;
				while (1) {
					if (badchar(*t) || badentity(t)) {
						if (t - c == 4) {
							break;
						}

						if (*(t) == ',' && (*(t + 1) != ' ')) {
							t++;
							continue;
						}

						if (*(t - 1) == '.')
							t--;
						if ((*(t - 1) == ')' && (inside_paren > 0))) {
							t--;
						}
						url_buf = g_strndup(c, t - c);
						if (!g_list_find_custom(ret, url_buf, (GCompareFunc)strcmp)) {
							purple_debug_info("TinyURL", "Added URL %s\n", url_buf);
							ret = g_list_append(ret, url_buf);
						}
						c = t;
						break;
					}
					t++;
				}
			}
		}
		if (*c == ')' && !inside_html) {
			inside_paren--;
			c++;
		}
		if (*c == 0)
			break;
		c++;
	}
	return ret;
}

static void url_fetched(PurpleUtilFetchUrlData *url_data, gpointer cb_data,
				const gchar *url_text, gsize len, const gchar *error_message)
{
	CbInfo *data = (CbInfo *)cb_data;
	PurpleConversation *conv = data->conv;
	GList *convs = purple_get_conversations();
	/* ensure the conversation still exists */
	for (; convs; convs = convs->next) {
		if ((PurpleConversation *)(convs->data) == conv) {
			FinchConv *fconv = FINCH_CONV(conv);
			gchar *str = g_strdup_printf("[%d] %s", data->num, url_text);
			GntTextView *tv = GNT_TEXT_VIEW(fconv->tv);
			gnt_text_view_tag_change(tv, data->tag, str, FALSE);
			g_free(str);
			g_free(data->tag);
			return;
		}
	}
	g_free(data->tag);
	purple_debug_info("TinyURL", "Conversation no longer exists... :(\n");
}

static void free_urls(gpointer data, gpointer null)
{
	g_free(data);
}

static gboolean receiving_msg(PurpleAccount *account, char **sender, char **message,
				PurpleConversation *conv, PurpleMessageFlags *flags) {
	GString *t;
	GList *iter, *urls;
	int c = 0;

	if (!(*flags & PURPLE_MESSAGE_RECV) || *flags & PURPLE_MESSAGE_INVISIBLE)
		return FALSE;

	urls = purple_conversation_get_data(conv, "TinyURLs");
	if (urls != NULL) /* message was cancelled somewhere? Reset. */
		g_list_foreach(urls, free_urls, NULL);
	g_list_free(urls);
	urls = extract_urls(*message);
	if (!urls)
		return FALSE;

	t = g_string_new(*message);
	g_free(*message);
	for (iter = urls; iter; iter = iter->next) {
		if (g_utf8_strlen((char *)iter->data, -1) >= purple_prefs_get_int(PREF_LENGTH)) {
			int pos, x = 0;
			gchar *j, *s, *str, *orig;
			glong len = g_utf8_strlen(iter->data, -1);
			s = g_strdup(t->str);
			orig = s;
			str = g_strdup_printf("[%d]", ++c);
			while ((j = strstr(s, iter->data))) { /* replace all occurrences */
				pos = j - orig + (x++ * 3);
				s = j + len;
				t = g_string_insert(t, pos + len, str);
				if (*s == '\0') break;
			}
			g_free(orig);
			g_free(str);
			continue;
		} else {
			if (iter->prev) {
				iter = iter->prev;
				g_free(iter->next->data);
				urls = g_list_delete_link(urls, iter->next);
			} else {
				g_free(iter->data);
				g_list_free(urls);
				urls = NULL;
			}
		}
	}
	*message = t->str;
	g_string_free(t, FALSE);
	if (conv == NULL)
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, *sender);
	purple_conversation_set_data(conv, "TinyURLs", urls);
	return FALSE;
}

static void received_msg(PurpleAccount *account, char *sender, char *message,
				PurpleConversation *conv, PurpleMessageFlags flags) {
	int c;
	GList *urls, *iter;
	FinchConv *fconv = FINCH_CONV(conv);
	GntTextView *tv = GNT_TEXT_VIEW(fconv->tv);

	urls = purple_conversation_get_data(conv, "TinyURLs");
	if (!(flags & PURPLE_MESSAGE_RECV) || urls == NULL)
		return;

	for (iter = urls, c = 0; iter; iter = iter->next) {
		int i;
		CbInfo *cbdata;
		gchar *url, *str, *tmp;
		cbdata = g_new(CbInfo, 1);
		cbdata->num = ++c;
		cbdata->tag = g_strdup_printf("%s%d", "tiny_", tag_num++);
		cbdata->conv = conv;
		tmp = purple_unescape_html((char *)iter->data);
		if (g_ascii_strncasecmp(tmp, "http://", 7) && g_ascii_strncasecmp(tmp, "https://", 8)) {
			url = g_strdup_printf("%shttp%%3A%%2F%%2F%s", purple_prefs_get_string(PREF_URL), purple_url_encode(tmp));
		} else {
			url = g_strdup_printf("%s%s", purple_prefs_get_string(PREF_URL), purple_url_encode(tmp));
		}
		g_free(tmp);
		purple_util_fetch_url(url, TRUE, "finch", FALSE, url_fetched, cbdata);
		i = gnt_text_view_get_lines_below(tv);
		str = g_strdup_printf(_("\nFetching TinyURL..."));
		gnt_text_view_append_text_with_tag((tv), str, GNT_TEXT_FLAG_DIM, cbdata->tag);
		g_free(str);
		if (i == 0)
			gnt_text_view_scroll(tv, 0);
		g_free(iter->data);
		g_free(url);
	}
	g_list_free(urls);
	purple_conversation_set_data(conv, "TinyURLs", NULL);
}

static void
free_conv_urls(PurpleConversation *conv)
{
	GList *urls = purple_conversation_get_data(conv, "TinyURLs");
	if (urls)
		g_list_foreach(urls, free_urls, NULL);
	g_list_free(urls);
}

static gboolean
plugin_load(PurplePlugin *plugin) {
	purple_signal_connect(purple_conversations_get_handle(),
			"wrote-im-msg",
			plugin, PURPLE_CALLBACK(received_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"wrote-chat-msg",
			plugin, PURPLE_CALLBACK(received_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"receiving-im-msg",
			plugin, PURPLE_CALLBACK(receiving_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"receiving-chat-msg",
			plugin, PURPLE_CALLBACK(receiving_msg), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
			"deleting-conversation",
			plugin, PURPLE_CALLBACK(free_conv_urls), NULL);

	return TRUE;
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {

  PurplePluginPrefFrame *frame;
  PurplePluginPref *pref;

  frame = purple_plugin_pref_frame_new();

  pref = purple_plugin_pref_new_with_name(PREF_LENGTH);
  purple_plugin_pref_set_label(pref, _("Only create TinyURL for URLs"
				     " of this length or greater"));
  purple_plugin_pref_frame_add(frame, pref);
  pref = purple_plugin_pref_new_with_name(PREF_URL);
  purple_plugin_pref_set_label(pref, _("TinyURL (or other) address prefix"));
  purple_plugin_pref_frame_add(frame, pref);

  return frame;
}

static PurplePluginUiInfo prefs_info = {
  get_plugin_pref_frame,
  0,    /* page_num (Reserved) */
  NULL, /* frame (Reserved) */

  /* padding */
  NULL,
  NULL,
  NULL,
  NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	FINCH_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"TinyURL",
	N_("TinyURL"),
	DISPLAY_VERSION,
	N_("TinyURL plugin"),
	N_("When receiving a message with URL(s), use TinyURL for easier copying"),
	"Richard Nelson <wabz@whatsbeef.net>",
	PURPLE_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL,
	&prefs_info,            /**< prefs_info */
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin) {
  purple_prefs_add_none(PREFS_BASE);
  purple_prefs_add_int(PREF_LENGTH, 30);
  purple_prefs_add_string(PREF_URL, "http://tinyurl.com/api-create.php?url=");
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)

/*
 * Conversation Themes for Pidgin
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
 */

#include "gtkconv-theme.h"

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "xmlnode.h"

#include "pidgin.h"
#include "gtkconv.h"
#include "gtkwebview.h"

#include <stdlib.h>
#include <string.h>

/* GObject data keys - this will probably go away soon... */
#define MESSAGE_STYLE_KEY "message-style"

#define PIDGIN_CONV_THEME_GET_PRIVATE(Gobject) \
	(G_TYPE_INSTANCE_GET_PRIVATE((Gobject), PIDGIN_TYPE_CONV_THEME, PidginConvThemePrivate))

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct {
	/* current config options */
	char     *variant; /* allowed to be NULL if there are no variants */
	GList    *variants;

	/* Info.plist keys/values */
	GHashTable *info;

	/* caches */
	char    *template_html;
	char    *header_html;
	char    *footer_html;
	char    *incoming_content_html;
	char    *outgoing_content_html;
	char    *incoming_next_content_html;
	char    *outgoing_next_content_html;
	char    *status_html;
	char    *basestyle_css;
} PidginConvThemePrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;

/******************************************************************************
 * Enums
 *****************************************************************************/

enum {
	PROP_ZERO = 0,
	PROP_INFO,
};

/******************************************************************************
 * GObject Stuff
 *****************************************************************************/

static void
pidgin_conv_theme_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *psec)
{
	PidginConvTheme *theme = PIDGIN_CONV_THEME(obj);

	switch (param_id) {
		case PROP_INFO:
			g_value_set_boxed(value, (gpointer)pidgin_conversation_theme_get_info(theme));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
pidgin_conv_theme_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *psec)
{
	PidginConvTheme *theme = PIDGIN_CONV_THEME(obj);

	switch (param_id) {
		case PROP_INFO:
			pidgin_conversation_theme_set_info(theme, g_value_get_boxed(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
pidgin_conv_theme_init(GTypeInstance *instance,
		gpointer klass)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(instance);
}

static void
pidgin_conv_theme_finalize(GObject *obj)
{
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(obj);

	g_free(priv->template_html);
	g_free(priv->incoming_content_html);
	g_free(priv->outgoing_content_html);
	g_free(priv->outgoing_next_content_html);
	g_free(priv->status_html);
	g_free(priv->basestyle_css);

	if (priv->info)
		g_hash_table_destroy(priv->info);

	parent_class->finalize(obj);
}

static void
pidgin_conv_theme_class_init(PidginConvThemeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GParamSpec *pspec;

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PidginConvThemePrivate));

	obj_class->get_property = pidgin_conv_theme_get_property;
	obj_class->set_property = pidgin_conv_theme_set_property;
	obj_class->finalize = pidgin_conv_theme_finalize;

	/* INFO */
	pspec = g_param_spec_boxed("info", "Info",
			"The information about this theme",
			G_TYPE_HASH_TABLE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(obj_class, PROP_INFO, pspec);

}

GType
pidgin_conversation_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginConvThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_conv_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(PidginConvTheme),
			0, /* n_preallocs */
			pidgin_conv_theme_init, /* instance_init */
			NULL, /* value table */
		};
		type = g_type_register_static(PURPLE_TYPE_THEME,
				"PidginConvTheme", &info, 0);
	}
	return type;
}

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

static const GValue *
get_key(PidginConvThemePrivate *priv, const char *key, gboolean specific)
{
	GValue *val = NULL;

	/* Try variant-specific key */
	if (specific && priv->variant) {
		char *name = g_strdup_printf("%s:%s", key, priv->variant);
		val = g_hash_table_lookup(priv->info, name);
		g_free(name);
	}

	/* Try generic key */
	if (!val) {
		val = g_hash_table_lookup(priv->info, key);
	}

	return val;
}

static const char *
get_template_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->template_html)
		return priv->template_html;

	/* The template path can either come from the theme, or can
	 * be stock Template.html that comes with the plugin */
	file = g_build_filename(dir, "Contents", "Resources", "Template.html", NULL);

	if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
		g_free(file);
		file = g_build_filename(DATADIR, "pidgin", "webkit", "Template.html", NULL);
	}

	if (!g_file_get_contents(file, &priv->template_html, NULL, NULL)) {
		purple_debug_error("webkit", "Could not locate a Template.html (%s)\n", file);
		priv->template_html = g_strdup("");
	}
	g_free(file);

	return priv->template_html;
}

static const char *
get_status_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->status_html)
		return priv->status_html;

	file = g_build_filename(dir, "Contents", "Resources", "Status.html", NULL);
	if (!g_file_get_contents(file, &priv->status_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s could not find Resources/Status.html", dir);
		priv->status_html = g_strdup("");
	}
	g_free(file);

	return priv->status_html;
}

static const char *
get_basestyle_css(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->basestyle_css)
		return priv->basestyle_css;

	file = g_build_filename(dir, "Contents", "Resources", "main.css", NULL);
	if (!g_file_get_contents(file, &priv->basestyle_css, NULL, NULL))
		priv->basestyle_css = g_strdup("");
	g_free(file);

	return priv->basestyle_css;
}

static const char *
get_header_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->header_html)
		return priv->header_html;

	file = g_build_filename(dir, "Contents", "Resources", "Header.html", NULL);
	if (!g_file_get_contents(file, &priv->header_html, NULL, NULL))
		priv->header_html = g_strdup("");
	g_free(file);

	return priv->header_html;
}

static const char *
get_footer_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->footer_html)
		return priv->footer_html;

	file = g_build_filename(dir, "Contents", "Resources", "Footer.html", NULL);
	if (!g_file_get_contents(file, &priv->footer_html, NULL, NULL))
		priv->footer_html = g_strdup("");
	g_free(file);

	return priv->footer_html;
}

static const char *
get_incoming_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->incoming_content_html)
		return priv->incoming_content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Incoming", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_content_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s did not have a Incoming/Content.html\n", dir);
		priv->incoming_content_html = g_strdup("");
	}
	g_free(file);

	return priv->incoming_content_html;
}

static const char *
get_incoming_next_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->incoming_next_content_html)
		return priv->incoming_next_content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Incoming", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_next_content_html, NULL, NULL)) {
		priv->incoming_next_content_html = g_strdup(priv->incoming_content_html);
	}
	g_free(file);

	return priv->incoming_next_content_html;
}

static const char *
get_outgoing_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->outgoing_content_html)
		return priv->outgoing_content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Outgoing", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_content_html, NULL, NULL)) {
		priv->outgoing_content_html = g_strdup(priv->incoming_content_html);
	}
	g_free(file);

	return priv->outgoing_content_html;
}

static const char *
get_outgoing_next_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->outgoing_next_content_html)
		return priv->outgoing_next_content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Outgoing", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_next_content_html, NULL, NULL)) {
		priv->outgoing_next_content_html = g_strdup(priv->outgoing_content_html);
	}

	return priv->outgoing_next_content_html;
}

static char *
replace_header_tokens(const char *text, PurpleConversation *conv)
{
	GString *str = g_string_new(NULL);
	const char *cur = text;
	const char *prev = cur;

	if (text == NULL)
		return NULL;

	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		char *fin = NULL;

		if (!strncmp(cur, "%chatName%", strlen("%chatName%"))) {
			replace = conv->name;
		} else if (!strncmp(cur, "%sourceName%", strlen("%sourceName%"))) {
			replace = purple_account_get_alias(conv->account);
			if (replace == NULL)
				replace = purple_account_get_username(conv->account);
		} else if (!strncmp(cur, "%destinationName%", strlen("%destinationName%"))) {
			PurpleBuddy *buddy = purple_find_buddy(conv->account, conv->name);
			if (buddy) {
				replace = purple_buddy_get_alias(buddy);
			} else {
				replace = conv->name;
			}
		} else if (!strncmp(cur, "%incomingIconPath%", strlen("%incomingIconPath%"))) {
			PurpleBuddyIcon *icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
			replace = purple_buddy_icon_get_full_path(icon);
		} else if (!strncmp(cur, "%outgoingIconPath%", strlen("%outgoingIconPath%"))) {
		} else if (!strncmp(cur, "%timeOpened", strlen("%timeOpened"))) {
			char *format = NULL;
			if (*(cur + strlen("%timeOpened")) == '{') {
				const char *start = cur + strlen("%timeOpened") + 1;
				char *end = strstr(start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup(start, end - start);
				fin = end + 1;
			}
			replace = purple_utf8_strftime(format ? format : "%X", NULL);
			g_free(format);
		} else {
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len(str, prev, cur - prev);
		g_string_append(str, replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;
		} else {
			prev = cur = strchr(cur + 1, '%') + 1;
		}
	}

	/* And wrap it up */
	g_string_append(str, prev);
	return g_string_free(str, FALSE);
}

static char *
replace_template_tokens(PidginConvTheme *theme, const char *text, const char *header, const char *footer)
{
	PidginConvThemePrivate *priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	GString *str = g_string_new(NULL);
	const char *themedir;

	char **ms = g_strsplit(text, "%@", 6);
	char *base = NULL;
	char *csspath = pidgin_conversation_theme_get_css(theme);
	if (ms[0] == NULL || ms[1] == NULL || ms[2] == NULL || ms[3] == NULL || ms[4] == NULL || ms[5] == NULL) {
		g_strfreev(ms);
		g_string_free(str, TRUE);
		return NULL;
	}

	themedir = purple_theme_get_dir(PURPLE_THEME(theme));

	g_string_append(str, ms[0]);
	g_string_append(str, "file://");
	base = g_build_filename(themedir, "Contents", "Resources", "Template.html", NULL);
	g_string_append(str, base);
	g_free(base);

	g_string_append(str, ms[1]);

	g_string_append(str, get_basestyle_css(priv, themedir));

	g_string_append(str, ms[2]);

	g_string_append(str, "file://");
	g_string_append(str, csspath);

	g_string_append(str, ms[3]);
	if (header)
		g_string_append(str, header);
	g_string_append(str, ms[4]);
	if (footer)
		g_string_append(str, footer);
	g_string_append(str, ms[5]);

	g_strfreev(ms);
	g_free(csspath);
	return g_string_free(str, FALSE);
}

static void
set_theme_webkit_settings(WebKitWebView *webview, PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	WebKitWebSettings *settings;
	const GValue *val;

	g_object_get(G_OBJECT(webview), "settings", &settings, NULL);

	val = get_key(priv, "DefaultFontFamily", TRUE);
	if (val)
		g_object_set(G_OBJECT(settings), "default-font-family", g_value_get_string(val), NULL);

	val = get_key(priv, "DefaultFontSize", TRUE);
	if (val)
		g_object_set(G_OBJECT(settings), "default-font-size", GINT_TO_POINTER(g_value_get_int(val)), NULL);

	val = get_key(priv, "DefaultBackgroundIsTransparent", TRUE);
	if (val)
		/* this does not work :( */
		webkit_web_view_set_transparent(webview, g_value_get_boolean(val));
}

/*
 * The style specification says that if the conversation is a group
 * chat then the <div id="Chat"> element will be given a class
 * 'groupchat'. I can't add another '%@' in Template.html because
 * that breaks style-specific Template.html's. I have to either use libxml
 * or conveniently play with WebKit's javascript engine. The javascript
 * engine should work, but it's not an identical behavior.
 */
static void
webkit_set_groupchat(GtkWebView *webview)
{
	gtk_webview_safe_execute_script(webview, "document.getElementById('Chat').className = 'groupchat'");
}

static void
webkit_on_webview_destroy(GtkObject *object, gpointer data)
{
	g_object_unref(G_OBJECT(data));
	g_object_set_data(G_OBJECT(object), MESSAGE_STYLE_KEY, NULL);
}

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

const GHashTable *
pidgin_conversation_theme_get_info(const PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	return priv->info;
}

void
pidgin_conversation_theme_set_info(PidginConvTheme *theme, GHashTable *info)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	if (priv->info)
		g_hash_table_destroy(priv->info);

	priv->info = info;
}

void
pidgin_conversation_theme_add_variant(PidginConvTheme *theme, const char *variant)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	priv->variants = g_list_prepend(priv->variants, g_strdup(variant));
}

const char *
pidgin_conversation_theme_get_variant(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return g_strdup(priv->variant);
}

void
pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant)
{
	PidginConvThemePrivate *priv;
	const GValue *val;
	char *prefname;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	g_free(priv->variant);
	priv->variant = g_strdup(variant);

	val = get_key(priv, "CFBundleIdentifier", FALSE);
	prefname = g_strdup_printf(PIDGIN_PREFS_ROOT "/conversations/themes/%s/variant",
	                           g_value_get_string(val));
	purple_prefs_set_string(prefname, variant);
	g_free(prefname);
}

const GList *
pidgin_conversation_theme_get_variants(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return priv->variants;
}

PidginConvTheme *
pidgin_conversation_theme_copy(const PidginConvTheme *theme)
{
	PidginConvTheme *ret;
	PidginConvThemePrivate *old, *new;

	old = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	ret = g_object_new(PIDGIN_TYPE_CONV_THEME, "directory", purple_theme_get_dir(PURPLE_THEME(theme)), NULL);
	new = PIDGIN_CONV_THEME_GET_PRIVATE(ret);

	new->variant = g_strdup(old->variant);

	new->template_html = g_strdup(old->template_html);
	new->header_html = g_strdup(old->header_html);
	new->footer_html = g_strdup(old->footer_html);
	new->incoming_content_html = g_strdup(old->incoming_content_html);
	new->outgoing_content_html = g_strdup(old->outgoing_content_html);
	new->incoming_next_content_html = g_strdup(old->incoming_next_content_html);
	new->outgoing_next_content_html = g_strdup(old->outgoing_next_content_html);
	new->status_html = g_strdup(old->status_html);
	new->basestyle_css = g_strdup(old->basestyle_css);

	return ret;
}

char *
pidgin_conversation_theme_get_css(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	const char *dir;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	dir = purple_theme_get_dir(PURPLE_THEME(theme));
	if (!priv->variant) {
		return g_build_filename(dir, "Contents", "Resources", "main.css", NULL);
	} else {
		char *file = g_strdup_printf("%s.css", priv->variant);
		char *ret = g_build_filename(dir, "Contents", "Resources", "Variants",  file, NULL);
		g_free(file);
		return ret;
	}
}

/**
 * Called when either a new PurpleConversation is created
 * or when a PidginConversation changes its active PurpleConversation
 * This will not change the theme if the theme is already set.
 * (This is to prevent accidental theme changes if a new
 * PurpleConversation gets added.
 *
 * FIXME: it's not at all clear to me as to how
 * Adium themes handle the case when the PurpleConversation
 * changes.
 */
void
pidgin_conversation_theme_apply(PidginConvTheme *theme, PurpleConversation *conv)
{
	GtkWidget *webkit = PIDGIN_CONVERSATION(conv)->webview;
	const char *themedir;
	char *header, *footer;
	char *template;
	char *basedir;
	char *baseuri;
	PidginConvTheme *oldTheme;
	PidginConvTheme *copy;
	PidginConvThemePrivate *priv;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	oldTheme = g_object_get_data(G_OBJECT(webkit), MESSAGE_STYLE_KEY);
	if (oldTheme)
		return;

	g_assert(theme);

	themedir = purple_theme_get_dir(PURPLE_THEME(theme));

	header = replace_header_tokens(get_header_html(priv, themedir), conv);
	footer = replace_header_tokens(get_footer_html(priv, themedir), conv);
	template = replace_template_tokens(theme, get_template_html(priv, themedir), header, footer);

	g_assert(template);

	purple_debug_info("webkit", "template: %s\n", template);

	set_theme_webkit_settings(WEBKIT_WEB_VIEW(webkit), theme);
	basedir = g_build_filename(themedir, "Contents", "Resources", "Template.html", NULL);
	baseuri = g_strdup_printf("file://%s", basedir);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webkit), template, "text/html", "UTF-8", baseuri);

	copy = pidgin_conversation_theme_copy(theme);
	g_object_set_data(G_OBJECT(webkit), MESSAGE_STYLE_KEY, copy);

	g_object_unref(G_OBJECT(theme));
	/* I need to unref this style when the webkit object destroys */
	g_signal_connect(G_OBJECT(webkit), "destroy", G_CALLBACK(webkit_on_webview_destroy), copy);

	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
		webkit_set_groupchat(GTK_WEBVIEW(webkit));
	g_free(basedir);
	g_free(baseuri);
	g_free(header);
	g_free(footer);
	g_free(template);
}


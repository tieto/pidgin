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
	char    *topic_html;
	char    *status_html;
	char    *content_html;
	char    *incoming_content_html;
	char    *outgoing_content_html;
	char    *incoming_next_content_html;
	char    *outgoing_next_content_html;
	char    *incoming_context_html;
	char    *outgoing_context_html;
	char    *incoming_next_context_html;
	char    *outgoing_next_context_html;
	char    *basestyle_css;
} PidginConvThemePrivate;

/******************************************************************************
 * Enums
 *****************************************************************************/

enum {
	PROP_ZERO = 0,
	PROP_INFO,
	PROP_VARIANT,
	PROP_LAST
};

/******************************************************************************
 * Globals
 *****************************************************************************/

static GObjectClass *parent_class = NULL;
#if GLIB_CHECK_VERSION(2,26,0)
static GParamSpec *properties[PROP_LAST];
#endif

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

/* The template path can either come from the theme, or can
 * be stock Template.html that comes with Pidgin */
static char *
get_template_path(const char *dir)
{
	char *file;

	file = g_build_filename(dir, "Contents", "Resources", "Template.html", NULL);

	if (!g_file_test(file, G_FILE_TEST_EXISTS)) {
		g_free(file);
		file = g_build_filename(DATADIR, "pidgin", "theme", "conversation", "Template.html", NULL);
	}

	return file;
}

static const char *
get_template_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->template_html)
		return priv->template_html;

	file = get_template_path(dir);

	if (!g_file_get_contents(file, &priv->template_html, NULL, NULL)) {
		purple_debug_error("webkit", "Could not locate a Template.html (%s)\n", file);
		priv->template_html = g_strdup("");
	}
	g_free(file);

	return priv->template_html;
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
get_topic_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->topic_html)
		return priv->topic_html;

	file = g_build_filename(dir, "Contents", "Resources", "Topic.html", NULL);
	if (!g_file_get_contents(file, &priv->topic_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s could not find Resources/Topic.html\n", dir);
		priv->topic_html = g_strdup("");
	}
	g_free(file);

	return priv->topic_html;
}

static const char *
get_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->content_html)
		return priv->content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->content_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s did not have a Content.html\n", dir);
		priv->content_html = g_strdup("");
	}
	g_free(file);

	return priv->content_html;
}

static const char *
get_status_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->status_html)
		return priv->status_html;

	file = g_build_filename(dir, "Contents", "Resources", "Status.html", NULL);
	if (!g_file_get_contents(file, &priv->status_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s could not find Resources/Status.html\n", dir);
		priv->status_html = g_strdup(get_content_html(priv, dir));
	}
	g_free(file);

	return priv->status_html;
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
		priv->incoming_content_html = g_strdup(get_content_html(priv, dir));
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
		priv->incoming_next_content_html = g_strdup(get_incoming_content_html(priv, dir));
	}
	g_free(file);

	return priv->incoming_next_content_html;
}

static const char *
get_incoming_context_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->incoming_context_html)
		return priv->incoming_context_html;

	file = g_build_filename(dir, "Contents", "Resources", "Incoming", "Context.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_context_html, NULL, NULL)) {
		purple_debug_info("webkit", "%s did not have a Incoming/Context.html\n", dir);
		priv->incoming_context_html = g_strdup(get_incoming_content_html(priv, dir));
	}
	g_free(file);

	return priv->incoming_context_html;
}

static const char *
get_incoming_next_context_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->incoming_next_context_html)
		return priv->incoming_next_context_html;

	file = g_build_filename(dir, "Contents", "Resources", "Incoming", "NextContext.html", NULL);
	if (!g_file_get_contents(file, &priv->incoming_next_context_html, NULL, NULL)) {
		priv->incoming_next_context_html = g_strdup(get_incoming_context_html(priv, dir));
	}
	g_free(file);

	return priv->incoming_next_context_html;
}

static const char *
get_outgoing_content_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->outgoing_content_html)
		return priv->outgoing_content_html;

	file = g_build_filename(dir, "Contents", "Resources", "Outgoing", "Content.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_content_html, NULL, NULL)) {
		priv->outgoing_content_html = g_strdup(get_incoming_content_html(priv, dir));
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
		priv->outgoing_next_content_html = g_strdup(get_outgoing_content_html(priv, dir));
	}

	return priv->outgoing_next_content_html;
}

static const char *
get_outgoing_context_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->outgoing_context_html)
		return priv->outgoing_context_html;

	file = g_build_filename(dir, "Contents", "Resources", "Outgoing", "Context.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_context_html, NULL, NULL)) {
		priv->outgoing_context_html = g_strdup(get_incoming_context_html(priv, dir));
	}
	g_free(file);

	return priv->outgoing_context_html;
}

static const char *
get_outgoing_next_context_html(PidginConvThemePrivate *priv, const char *dir)
{
	char *file;

	if (priv->outgoing_next_context_html)
		return priv->outgoing_next_context_html;

	file = g_build_filename(dir, "Contents", "Resources", "Outgoing", "NextContext.html", NULL);
	if (!g_file_get_contents(file, &priv->outgoing_next_context_html, NULL, NULL)) {
		priv->outgoing_next_context_html = g_strdup(get_outgoing_context_html(priv, dir));
	}
	g_free(file);

	return priv->outgoing_next_context_html;
}

static void
_set_variant(PidginConvTheme *theme, const char *variant)
{
	PidginConvThemePrivate *priv;
	const GValue *val;
	char *prefname;

	g_return_if_fail(theme != NULL);
	g_return_if_fail(variant != NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	g_free(priv->variant);
	priv->variant = g_strdup(variant);

	val = get_key(priv, "CFBundleIdentifier", FALSE);
	prefname = g_strdup_printf(PIDGIN_PREFS_ROOT "/conversations/themes/%s/variant",
	                           g_value_get_string(val));
	purple_prefs_set_string(prefname, variant);
	g_free(prefname);
}

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

		case PROP_VARIANT:
			g_value_set_string(value, pidgin_conversation_theme_get_variant(theme));
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

		case PROP_VARIANT:
			_set_variant(theme, g_value_get_string(value));
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
	GList *list;

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(obj);

	g_free(priv->template_html);
	g_free(priv->header_html);
	g_free(priv->footer_html);
	g_free(priv->topic_html);
	g_free(priv->status_html);
	g_free(priv->content_html);
	g_free(priv->incoming_content_html);
	g_free(priv->outgoing_content_html);
	g_free(priv->incoming_next_content_html);
	g_free(priv->outgoing_next_content_html);
	g_free(priv->incoming_context_html);
	g_free(priv->outgoing_context_html);
	g_free(priv->incoming_next_context_html);
	g_free(priv->outgoing_next_context_html);
	g_free(priv->basestyle_css);

	if (priv->info)
		g_hash_table_destroy(priv->info);

	list = priv->variants;
	while (list) {
		g_free(list->data);
		list = g_list_delete_link(list, list);
	}
	g_free(priv->variant);

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
#if GLIB_CHECK_VERSION(2,26,0)
	properties[PROP_INFO] = pspec;
#endif

	/* VARIANT */
	pspec = g_param_spec_string("variant", "Variant",
			"The current variant for this theme",
			NULL, G_PARAM_READWRITE);
	g_object_class_install_property(obj_class, PROP_VARIANT, pspec);
#if GLIB_CHECK_VERSION(2,26,0)
	properties[PROP_VARIANT] = pspec;
#endif
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

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

const GHashTable *
pidgin_conversation_theme_get_info(const PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;

	g_return_val_if_fail(theme != NULL, NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	return priv->info;
}

void
pidgin_conversation_theme_set_info(PidginConvTheme *theme, GHashTable *info)
{
	PidginConvThemePrivate *priv;

	g_return_if_fail(theme != NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	if (priv->info)
		g_hash_table_destroy(priv->info);

	priv->info = info;
}

const GValue *
pidgin_conversation_theme_lookup(PidginConvTheme *theme, const char *key, gboolean specific)
{
	PidginConvThemePrivate *priv;

	g_return_val_if_fail(theme != NULL, NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return get_key(priv, key, specific);
}

const char *
pidgin_conversation_theme_get_template(PidginConvTheme *theme, PidginConvThemeTemplateType type)
{
	PidginConvThemePrivate *priv;
	const char *dir;
	const char *html;

	g_return_val_if_fail(theme != NULL, NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);
	dir = purple_theme_get_dir(PURPLE_THEME(theme));

	switch (type) {
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_MAIN:
			html = get_template_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_HEADER:
			html = get_header_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_FOOTER:
			html = get_footer_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_TOPIC:
			html = get_topic_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_STATUS:
			html = get_status_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_CONTENT:
			html = get_content_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTENT:
			html = get_incoming_content_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_NEXT_CONTENT:
			html = get_incoming_next_content_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTEXT:
			html = get_incoming_context_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_NEXT_CONTEXT:
			html = get_incoming_next_context_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_CONTENT:
			html = get_outgoing_content_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_NEXT_CONTENT:
			html = get_outgoing_next_content_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_CONTEXT:
			html = get_outgoing_context_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_NEXT_CONTEXT:
			html = get_outgoing_next_context_html(priv, dir);
			break;
		case PIDGIN_CONVERSATION_THEME_TEMPLATE_BASESTYLE_CSS:
			html = get_basestyle_css(priv, dir);
			break;
		default:
			purple_debug_error("gtkconv-theme",
			                   "Requested invalid template type (%d) for theme %s.\n",
			                   type, purple_theme_get_name(PURPLE_THEME(theme)));
			html = NULL;
	}

	return html;
}

void
pidgin_conversation_theme_add_variant(PidginConvTheme *theme, char *variant)
{
	PidginConvThemePrivate *priv;

	g_return_if_fail(theme != NULL);
	g_return_if_fail(variant != NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	priv->variants = g_list_prepend(priv->variants, variant);
}

const char *
pidgin_conversation_theme_get_variant(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;

	g_return_val_if_fail(theme != NULL, NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return priv->variant;
}

void
pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant)
{
	_set_variant(theme, variant);
#if GLIB_CHECK_VERSION(2,26,0)
	g_object_notify_by_pspec(G_OBJECT(theme), properties[PROP_VARIANT]);
#else
	g_object_notify(G_OBJECT(theme), "variant");
#endif
}

const GList *
pidgin_conversation_theme_get_variants(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;

	g_return_val_if_fail(theme != NULL, NULL);

	priv = PIDGIN_CONV_THEME_GET_PRIVATE(theme);

	return priv->variants;
}

char *
pidgin_conversation_theme_get_template_path(PidginConvTheme *theme)
{
	const char *dir;

	g_return_val_if_fail(theme != NULL, NULL);

	dir = purple_theme_get_dir(PURPLE_THEME(theme));

	return get_template_path(dir);
}

char *
pidgin_conversation_theme_get_css_path(PidginConvTheme *theme)
{
	PidginConvThemePrivate *priv;
	const char *dir;

	g_return_val_if_fail(theme != NULL, NULL);

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


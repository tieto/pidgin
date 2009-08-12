/*
 * Adium Message Styles
 * Copyright (C) 2009  Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2007
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#define PLUGIN_ID		"gtk-webview-adium-ims"
#define PLUGIN_NAME		"webview-adium-ims"

/*
 * A lot of this was originally written by Sean Egan, but I think I've 
 * rewrote enough to replace the author for now. 
 */
#define PLUGIN_AUTHOR		"Arnold Noronha <arnstein87@gmail.com>" 
#define PURPLE_PLUGINS          "Hell yeah"

/* System headers */
#include <string.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <webkit/webkit.h>

/* Purple headers */
#include <conversation.h>
#include <debug.h>
#include <notify.h>
#include <util.h>
#include <version.h>

/* Pidgin headers */
#include <gtkconv.h>
#include <gtkplugin.h>
#include <gtkwebview.h>
#include <smileyparser.h>

#include <libxml/xmlreader.h>
/* GObject data keys */
#define MESSAGE_STYLE_KEY "message-style"

/*
 * I'm going to allow a different style for each PidginConversation.
 * This way I can do two things: 1) change the theme on the fly and not
 * change existing themes, and 2) Use a different theme for IMs and
 * chats.
 */
typedef struct _PidginMessageStyle {
	int     ref_counter;

	/* current config options */
	char     *variant; /* allowed to be NULL if there are no variants */
	char     *bg_color;

	/* Info.plist keys */
	int      message_view_version;
	char     *cf_bundle_name;
	char     *cf_bundle_identifier;
	char     *cf_bundle_get_info_string;
	char     *default_font_family;
	int      default_font_size;
	gboolean shows_user_icons;
	gboolean disable_combine_consecutive;
	gboolean default_background_is_transparent;
	gboolean disable_custom_background;
	char     *default_background_color;
	gboolean allow_text_colors;
	char     *image_mask;
	
	/* paths */
	char    *style_dir;
	char    *template_path;

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
} PidginMessageStyle;

static GList *style_list; /**< List of PidginMessageStyles */
static char  *cur_style_dir = NULL;
static void  *handle = NULL;

static PidginMessageStyle* pidgin_message_style_new (const char* styledir);
static PidginMessageStyle* pidgin_message_style_load (const char* styledir);
static void pidgin_message_style_unref (PidginMessageStyle *style);
static void pidgin_message_style_read_info_plist (PidginMessageStyle *style, const char* variant);
static char* pidgin_message_style_get_variant (PidginMessageStyle *style);
static GList* pidgin_message_style_get_variants (PidginMessageStyle *style);
static void pidgin_message_style_set_variant (PidginMessageStyle *style, const char *variant);

static void
glist_free_all_string (GList *list)
{
	GList *first = list;
	for (; list; list = g_list_next (list)) 
		g_free (list->data);
	g_list_free (first);
}

static inline char* get_absolute_path (const char *path)
{
	if (g_path_is_absolute (path)) return g_strdup (path);
	else {
		char* cwd = g_get_current_dir (), *ret;
		ret = g_build_filename (cwd, path, NULL);
		g_free (cwd);
		return ret;
	}
}

static PidginMessageStyle* pidgin_message_style_new (const char* styledir)
{
	PidginMessageStyle* ret = g_new0 (PidginMessageStyle, 1);
	GList *iter;

	/* sanity check */
	for (iter = style_list; iter; iter = g_list_next (iter))
	  g_assert (!g_str_equal (((PidginMessageStyle*)iter->data)->style_dir, styledir));

	ret->ref_counter = 1;
	ret->style_dir = g_strdup (styledir);
	
	style_list = g_list_append (style_list, ret);
	return ret;
}

/**
 * deallocate any memory used for info.plist options 
 */
static void
pidgin_message_style_unset_info_plist (PidginMessageStyle *style)
{
	style->message_view_version = 0;
	g_free (style->cf_bundle_name);
	style->cf_bundle_name = NULL;

	g_free (style->cf_bundle_identifier);
	style->cf_bundle_identifier = NULL;

	g_free (style->cf_bundle_get_info_string);
	style->cf_bundle_get_info_string = NULL;

	g_free (style->default_font_family);
	style->default_font_family = NULL;

	style->default_font_size = 0;
	style->shows_user_icons = TRUE;
	style->disable_combine_consecutive = FALSE;
	style->default_background_is_transparent = FALSE;
	style->disable_custom_background = FALSE;

	g_free (style->default_background_color);
	style->default_background_color = NULL;
	
	style->allow_text_colors = TRUE;
	
	g_free (style->image_mask);
	style->image_mask = NULL;
}


static void pidgin_message_style_unref (PidginMessageStyle *style)
{
	if (!style) return;
	g_assert (style->ref_counter > 0);

	style->ref_counter--;
	if (style->ref_counter) return;

	g_free (style->style_dir);
	g_free (style->template_path);
	
	g_free (style->template_html);
	g_free (style->incoming_content_html);
	g_free (style->outgoing_content_html);
	g_free (style->outgoing_next_content_html);
	g_free (style->status_html);
	g_free (style->basestyle_css);

	style_list = g_list_remove (style_list, style);
	g_free (style);

	pidgin_message_style_unset_info_plist (style);
}

static void
pidgin_message_style_save_state (PidginMessageStyle *style)
{
	char *prefname = g_strdup_printf ("/plugins/gtk/adiumthemes/%s", style->cf_bundle_identifier);
	char *variant = g_strdup_printf ("%s/%s", prefname, style->variant);

	purple_prefs_add_none (prefname);
	purple_prefs_add_string (variant, "");
	purple_prefs_set_string (variant, style->variant);
	
	g_free (prefname);
	g_free (variant);
}

static void
pidgin_message_style_load_state (PidginMessageStyle *style)
{
	char *prefname = g_strdup_printf ("/plugins/gtk/adiumthemes/%s", style->cf_bundle_identifier);
	char *variant = g_strdup_printf ("%s/%s", prefname, style->variant);

	const char* value = purple_prefs_get_string (variant);
	gboolean changed = !style->variant || !g_str_equal (style->variant, value);

	g_free (style->variant);
	style->variant = g_strdup (value);

	if (changed) pidgin_message_style_read_info_plist (style, style->variant);

	g_free (prefname);
	g_free(variant);
}

static void webkit_on_webview_destroy (GtkObject* obj, gpointer data);

static gboolean
parse_info_plist_key_value (xmlnode* key, gpointer destination, const char* expected)
{
	xmlnode *val = key->next;

	for (; val && val->type != XMLNODE_TYPE_TAG; val = val->next);
	if (!val) return FALSE;
	
	if (expected == NULL || g_str_equal (expected, "string")) {
		char** dest = (char**) destination;
		if (!g_str_equal (val->name, BAD_CAST "string")) return FALSE;
		if (*dest) g_free (*dest);
		*dest = xmlnode_get_data_unescaped (val);
	} else if (g_str_equal (expected, "integer")) {
		int* dest = (int*) destination;
		char* value = xmlnode_get_data_unescaped (val);

		if (!g_str_equal (val->name, "integer")) return FALSE;
		*dest = atoi (value);
		g_free (value);
	} else if (g_str_equal (expected, "boolean")) {
		gboolean *dest = (gboolean*) destination;
		if (g_str_equal (val->name, BAD_CAST "true")) *dest = TRUE;
		else if (g_str_equal (val->name, BAD_CAST "false")) *dest = FALSE;
		else return FALSE;
	} else return FALSE;
	
	return TRUE;
}

static
gboolean str_for_key (const char *key, const char *found, const char *variant){
	if (g_str_equal (key, found)) return TRUE;
	if (!variant) return FALSE;
	return (g_str_has_prefix (found, key) 
		&& g_str_has_suffix (found, variant)
		&& strlen (found) == strlen (key) + strlen (variant) + 1);
}

/**
 * Info.plist should be re-read every time the variant changes, this is because
 * the keys that take precedence depend on the value of the current variant.
 */
static void
pidgin_message_style_read_info_plist (PidginMessageStyle *style, const char* variant)
{
	/* note that if a variant is used the option:VARIANTNAME takes precedence */
	char *contents = g_build_filename (style->style_dir, "Contents", NULL);
	xmlnode *plist = xmlnode_from_file (contents, "Info.plist", "Info.plist", "webkit"), *iter;
	xmlnode *dict = xmlnode_get_child (plist, "dict");

	g_assert (dict);
	for (iter = xmlnode_get_child (dict, "key"); iter; iter = xmlnode_get_next_twin (iter)) {
		char* key = xmlnode_get_data_unescaped (iter);
		gboolean pr = TRUE;

		if (g_str_equal ("MessageViewVersion", key)) 
			pr = parse_info_plist_key_value (iter, &style->message_view_version, "integer");
		else if (g_str_equal ("CFBundleName", key))
			pr = parse_info_plist_key_value (iter, &style->cf_bundle_name, "string");
		else if (g_str_equal ("CFBundleIdentifier", key))
			pr = parse_info_plist_key_value (iter, &style->cf_bundle_identifier, "string");
		else if (g_str_equal ("CFBundleGetInfoString", key))
			pr = parse_info_plist_key_value (iter, &style->cf_bundle_get_info_string, "string");
		else if (str_for_key ("DefaultFontFamily", key, variant))
			pr = parse_info_plist_key_value (iter, &style->default_font_family, "string");
		else if (str_for_key ("DefaultFontSize", key, variant))
			pr = parse_info_plist_key_value (iter, &style->default_font_size, "integer");
		else if (str_for_key ("ShowsUserIcons", key, variant))
			pr = parse_info_plist_key_value (iter, &style->shows_user_icons, "boolean");
		else if (str_for_key ("DisableCombineConsecutive", key, variant))
			pr = parse_info_plist_key_value (iter, &style->disable_combine_consecutive, "boolean");
		else if (str_for_key ("DefaultBackgroundIsTransparent", key, variant))
			pr = parse_info_plist_key_value (iter, &style->default_background_is_transparent, "boolean");
		else if (str_for_key ("DisableCustomBackground", key, variant))
			pr = parse_info_plist_key_value (iter, &style->disable_custom_background, "boolean");
		else if (str_for_key ("DefaultBackgroundColor", key, variant))
			pr = parse_info_plist_key_value (iter, &style->default_background_color, "string");
		else if (str_for_key ("AllowTextColors", key, variant))
			pr = parse_info_plist_key_value (iter, &style->allow_text_colors, "string");
		else if (str_for_key ("ImageMask", key, variant))
			pr = parse_info_plist_key_value (iter, &style->image_mask, "string");

		g_free (key);
		if (!pr) break; /* does not make sense */
	}

	xmlnode_free (plist);
}

static PidginMessageStyle*
pidgin_message_style_load (const char* styledir)
{
	/*
	 * the loading process described:
	 *
	 * First we load all the style .html files, etc.
	 * The we load any config options that have been stored for
	 * this variant.
	 * Then we load the Info.plist, for the currently decided variant.
	 * At this point, if we find that variants exist, yet
	 * we don't have a variant selected, we choose DefaultVariant
	 * and if that does not exist, we choose the first one in the
	 * directory.
	 */
	
	/* is this style already loaded? */
	GList  *cur = style_list;
	char   *file; /* temporary variable */
	PidginMessageStyle *style = NULL;

	g_assert (styledir);
	for (cur = style_list; cur; cur = g_list_next (cur)) {
		style = (PidginMessageStyle*) cur->data;
		if (g_str_equal (styledir, style->style_dir)) {
			style->ref_counter++;
			return style;
		}
	}

	/* else we need to load it */
	style = pidgin_message_style_new (styledir);
	
	/* load all other files */

	/* The template path can either come from the theme, or can
	 * be stock Template.html that comes with the plugin */
	style->template_path = g_build_filename(styledir, "Contents", "Resources", "Template.html", NULL);

	if (!g_file_test(style->template_path, G_FILE_TEST_EXISTS)) {
		g_free (style->template_path);
		style->template_path = g_build_filename(DATADIR, "pidgin", "webkit", "Template.html", NULL);
	}

	if (!g_file_get_contents(style->template_path, &style->template_html, NULL, NULL)) {
		pidgin_message_style_unref (style);
		purple_debug_error ("webkit", "Could not locate a Template.html\n");
		return NULL;
	}

	file = g_build_filename(styledir, "Contents", "Resources", "Status.html", NULL);
	if (!g_file_get_contents(file, &style->status_html, NULL, NULL)) {
		purple_debug_info ("webkit", "%s could not find Resources/Status.html", styledir);
		pidgin_message_style_unref (style);
		g_free (file);
		return NULL;
	}
	g_free (file);

	file = g_build_filename(styledir, "Contents", "Resources", "main.css", NULL);
	if (!g_file_get_contents(file, &style->basestyle_css, NULL, NULL))
		style->basestyle_css = g_strdup ("");
	g_free (file);
	
	file = g_build_filename(styledir, "Contents", "Resources", "Header.html", NULL);
	if (!g_file_get_contents(file, &style->header_html, NULL, NULL))
		style->header_html = g_strdup ("");
	g_free (file);

	file = g_build_filename(styledir, "Contents", "Resources", "Footer.html", NULL);
	if (!g_file_get_contents(file, &style->footer_html, NULL, NULL))
		style->footer_html = g_strdup ("");
	g_free (file);

	file = g_build_filename(styledir, "Contents", "Resources", "Incoming", "Content.html", NULL);
	if (!g_file_get_contents(file, &style->incoming_content_html, NULL, NULL)) {
		purple_debug_info ("webkit", "%s did not have a Incoming/Content.html\n", styledir);
		pidgin_message_style_unref (style);
		g_free (file);
		return NULL;
	}
	g_free (file);


	/* according to the spec, the following are optional files */
	file = g_build_filename(styledir, "Contents", "Resources", "Incoming", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &style->incoming_next_content_html, NULL, NULL)) {
		style->incoming_next_content_html = g_strdup (style->incoming_content_html);
	}
	g_free (file);

	file = g_build_filename(styledir, "Contents", "Resources", "Outgoing", "Content.html", NULL);
	if (!g_file_get_contents(file, &style->outgoing_content_html, NULL, NULL)) {
		style->outgoing_content_html = g_strdup(style->incoming_content_html);
	}
	g_free (file);

	file = g_build_filename(styledir, "Contents", "Resources", "Outgoing", "NextContent.html", NULL);
	if (!g_file_get_contents(file, &style->outgoing_next_content_html, NULL, NULL)) {
		style->outgoing_next_content_html = g_strdup (style->outgoing_content_html);
	}

	pidgin_message_style_load_state (style);
	pidgin_message_style_read_info_plist (style, style->variant);

	/* non variant dependent Info.plist checks */
	if (style->message_view_version < 3) {
		pidgin_message_style_unref (style);
		return NULL;
	}

	if (!style->variant)
	{
		GList *variants = pidgin_message_style_get_variants (style);

		if (variants)
			pidgin_message_style_set_variant (style, variants->data);

		glist_free_all_string (variants);
		pidgin_message_style_save_state (style);
	}

	return style;
}

static void
pidgin_message_style_set_variant (PidginMessageStyle *style, const char *variant)
{
	/* I'm not going to test whether this variant is valid! */
	g_free (style->variant);
	style->variant = g_strdup (variant);

	pidgin_message_style_read_info_plist (style, variant);
	pidgin_message_style_save_state (style);
	
	/* todo, the style has "changed". Ideally, I would like to use signals at this point. */
}

char* pidgin_message_style_get_variant (PidginMessageStyle *style)
{
	return g_strdup (style->variant);
}

/**
 * Get a list of variants supported by the style.
 */
static GList*
pidgin_message_style_get_variants (PidginMessageStyle *style) 
{
	GList *ret = NULL;
        GDir *variants;
	const char *css_file;
	char *css;
	char *variant_dir;

	g_assert (style->style_dir);
	variant_dir = g_build_filename(style->style_dir, "Contents", "Resources", "Variants", NULL);

	variants = g_dir_open(variant_dir, 0, NULL);
	if (!variants) return NULL;

	while ((css_file = g_dir_read_name(variants)) != NULL) {
		if (!g_str_has_suffix (css_file, ".css"))
			continue;

		css = g_strndup (css_file, strlen (css_file) - 4);
		ret = g_list_append(ret, css);
	}

	g_dir_close(variants);
	g_free(variant_dir);

	ret = g_list_sort (ret, (GCompareFunc)g_strcmp0);
	return ret;	
}

static char* pidgin_message_style_get_css (PidginMessageStyle *style)
{
	if (!style->variant) {
		return g_build_filename (style->style_dir, "Contents", "Resources", "main.css", NULL);
	} else {
		char *file = g_strdup_printf ("%s.css", style->variant);
		char *ret = g_build_filename (style->style_dir, "Contents", "Resources", "Variants",  file, NULL);
		g_free (file);
		return ret;
	}
}

static void* webkit_plugin_get_handle ()
{
	if (handle) return handle;
	else return (handle = g_malloc (1));
}

static void webkit_plugin_free_handle ()
{
	purple_signals_disconnect_by_handle (handle);
	g_free (handle);
}

static char *
replace_message_tokens(
	char *text, 
	gsize len, 
	PurpleConversation *conv, 
	const char *name, 
	const char *alias, 
	const char *message, 
	PurpleMessageFlags flags, 
	time_t mtime)
{
	GString *str = g_string_new_len(NULL, len);
	char *cur = text;
	char *prev = cur;

	while ((cur = strchr(cur, '%'))) {
		const char *replace = NULL;
		char *fin = NULL;
			
		if (!strncmp(cur, "%message%", strlen("%message%"))) {
			replace = message;
		} else if (!strncmp(cur, "%messageClasses%", strlen("%messageClasses%"))) {
			replace = flags & PURPLE_MESSAGE_SEND ? "outgoing" :
				  flags & PURPLE_MESSAGE_RECV ? "incoming" : "event";
		} else if (!strncmp(cur, "%time", strlen("%time"))) {
			char *format = NULL;
			if (*(cur + strlen("%time")) == '{') {
				char *start = cur + strlen("%time") + 1;
				char *end = strstr(start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup(start, end - start);
				fin = end + 1;
			} 
			replace = purple_utf8_strftime(format ? format : "%X", NULL);
			g_free(format);
		} else if (!strncmp(cur, "%userIconPath%", strlen("%userIconPath%"))) {
			if (flags & PURPLE_MESSAGE_SEND) {
				if (purple_account_get_bool(conv->account, "use-global-buddyicon", TRUE)) {
					replace = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon");
				} else {
					PurpleStoredImage *img = purple_buddy_icons_find_account_icon(conv->account);
					replace = purple_imgstore_get_filename(img);
				}
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = g_build_filename("Outgoing", "buddy_icon.png", NULL);
				}
			} else if (flags & PURPLE_MESSAGE_RECV) {
				PurpleBuddyIcon *icon = purple_conv_im_get_icon(PURPLE_CONV_IM(conv));
				replace = purple_buddy_icon_get_full_path(icon);
				if (replace == NULL || !g_file_test(replace, G_FILE_TEST_EXISTS)) {
					replace = g_build_filename("Incoming", "buddy_icon.png", NULL);
				}
			}
			
		} else if (!strncmp(cur, "%senderScreenName%", strlen("%senderScreenName%"))) {
			replace = name;
		} else if (!strncmp(cur, "%sender%", strlen("%sender%"))) {
			replace = alias;
		} else if (!strncmp(cur, "%service%", strlen("%service%"))) {
			replace = purple_account_get_protocol_name(conv->account);
		} else {
			cur++;
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
replace_header_tokens(char *text, gsize len, PurpleConversation *conv)
{
	GString *str = g_string_new_len(NULL, len);
	char *cur = text;
	char *prev = cur;

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
				char *start = cur + strlen("%timeOpened") + 1;
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
replace_template_tokens(PidginMessageStyle *style, char *text, int len, char *header, char *footer) {
	GString *str = g_string_new_len(NULL, len);

	char **ms = g_strsplit(text, "%@", 6);
	char *base = NULL;
	char *csspath = pidgin_message_style_get_css (style);
	if (ms[0] == NULL || ms[1] == NULL || ms[2] == NULL || ms[3] == NULL || ms[4] == NULL || ms[5] == NULL) {
		g_strfreev(ms);
		g_string_free(str, TRUE);
		return NULL;
	}

	g_string_append(str, ms[0]);
	g_string_append(str, "file://");
	base = g_build_filename (style->style_dir, "Contents", "Resources", "Template.html", NULL);
	g_string_append(str, base);
	g_free (base);

	g_string_append(str, ms[1]);

	/* I'm just going to skip main.css at this point */

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
	g_free (csspath);
	return g_string_free (str, FALSE);
}

static GtkWidget *
get_webkit(PurpleConversation *conv)
{
	PidginConversation *gtkconv;
	gtkconv = PIDGIN_CONVERSATION(conv);
	if (!gtkconv)
		return NULL;
	else 
		return gtkconv->webview;
}

static void set_theme_font_settings (WebKitWebView *webview, PidginMessageStyle *style)
{
	WebKitWebSettings *settings;

	g_object_get (G_OBJECT(webview), "settings", &settings, NULL);
	if (style->default_font_family)
		g_object_set (G_OBJECT (settings), "default-font-family", style->default_font_family, NULL);
	
	if (style->default_font_size)
		g_object_set (G_OBJECT (settings), "default-font-size", GINT_TO_POINTER (style->default_font_size), NULL);
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
static void
init_theme_for_webkit (PurpleConversation *conv, char *style_dir)
{
	GtkWidget *webkit = PIDGIN_CONVERSATION(conv)->webview;
	char *header, *footer;
	char *template;

	char* basedir;
	char* baseuri;
	PidginMessageStyle *style, *oldStyle;
	oldStyle = g_object_get_data (G_OBJECT(webkit), MESSAGE_STYLE_KEY);
	
	if (oldStyle) return;

	purple_debug_info ("webkit", "loading %s\n", style_dir);
	style = pidgin_message_style_load (style_dir);
	g_assert (style);

	basedir = g_build_filename (style->style_dir, "Contents", "Resources", "Template.html", NULL);
	baseuri = g_strdup_printf ("file://%s", basedir);
	header = replace_header_tokens(style->header_html, strlen(style->header_html), conv);
	g_assert (style);
	footer = replace_header_tokens(style->footer_html, strlen(style->footer_html), conv);
	template = replace_template_tokens(style, style->template_html, strlen(style->template_html) + strlen(style->header_html), header, footer);

	g_assert(template);
	
	purple_debug_info ("webkit", "template: %s\n", template);

	set_theme_font_settings (WEBKIT_WEB_VIEW(webkit), style);
	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webkit), template, "text/html", "UTF-8", baseuri);

	g_object_set_data (G_OBJECT(webkit), MESSAGE_STYLE_KEY, style);
	
	/* I need to unref this style when the webkit object destroys */
	g_signal_connect (G_OBJECT(webkit), "destroy", G_CALLBACK(webkit_on_webview_destroy), style);

	g_free (basedir);
	g_free (baseuri);
	g_free (header);
	g_free (footer);
	g_free (template);
}


/* restore the non theme version of the conversation window */
static void
finalize_theme_for_webkit (PurpleConversation *conv)
{
	GtkWidget *webview = PIDGIN_CONVERSATION(conv)->webview;
	PidginMessageStyle *style = g_object_get_data (G_OBJECT(webview), MESSAGE_STYLE_KEY);

	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webview), "", "text/html", "UTF-8", "");

	g_object_set_data (G_OBJECT(webview), MESSAGE_STYLE_KEY, NULL);
	pidgin_message_style_unref (style);
}

static void
webkit_on_webview_destroy (GtkObject *object, gpointer data)
{
	pidgin_message_style_unref ((PidginMessageStyle*) data);
	g_object_set_data (G_OBJECT(object), MESSAGE_STYLE_KEY, NULL);
}

static gboolean webkit_on_displaying_im_msg (PurpleAccount *account,
						 const char* name,
						 char **pmessage,
						 PurpleConversation *conv,
						 PurpleMessageFlags flags,
						 gpointer data)
{
	GtkWidget *webkit;
	char *message = *pmessage;
	const char *alias = name; /* FIXME: signal doesn't give me alias */
	char *stripped;
	char *message_html;
	char *msg;
	char *escape;
	char *script;
	char *func = "appendMessage";
	char *smileyed;
	time_t mtime = time (NULL); /* FIXME: this should come from the write_conv calback, but the signal doesn't pass this to me */

	PurpleMessageFlags old_flags = GPOINTER_TO_INT(purple_conversation_get_data(conv, "webkit-lastflags")); 
	PidginMessageStyle *style;

	fprintf (stderr, "hmm.. here %s %s\n", name, message);
	webkit = get_webkit(conv);
	stripped = g_strdup(message);

	style = g_object_get_data (G_OBJECT (webkit), MESSAGE_STYLE_KEY);
	g_assert (style);

	if (flags & PURPLE_MESSAGE_SEND && old_flags & PURPLE_MESSAGE_SEND) {
		message_html = style->outgoing_next_content_html;
		func = "appendNextMessage";
	} else if (flags & PURPLE_MESSAGE_SEND) {
		message_html = style->outgoing_content_html;
	} else if (flags & PURPLE_MESSAGE_RECV && old_flags & PURPLE_MESSAGE_RECV) {
		message_html = style->incoming_next_content_html;
		func = "appendNextMessage";
	} else if (flags & PURPLE_MESSAGE_RECV) {
		message_html = style->incoming_content_html;
	} else {
		message_html = style->status_html;
	}
	purple_conversation_set_data(conv, "webkit-lastflags", GINT_TO_POINTER(flags));

	smileyed = smiley_parse_markup(stripped, conv->account->protocol_id);
	msg = replace_message_tokens(message_html, 0, conv, name, alias, smileyed, flags, mtime);
	escape = gtk_webview_quote_js_string (msg);
	script = g_strdup_printf("%s(%s)", func, escape);

	purple_debug_info ("webkit", "JS: %s\n", script);
	gtk_webview_safe_execute_script (GTK_WEBVIEW (webkit), script);

	g_free(script);
	g_free(smileyed);
	g_free(msg);
	g_free(stripped);
	g_free(escape);

	return TRUE; /* GtkConv should not handle this IM */
}

static gboolean webkit_on_displaying_chat_msg (PurpleAccount *account,
					       const char *who,
					       char **message,
					       PurpleConversation *conv,
					       PurpleMessageFlags flags,
					       gpointer userdata)
{
	/* handle exactly like an IM message for now */
	return webkit_on_displaying_im_msg (account, who, message, conv, flags, NULL);
}

static void
webkit_on_conversation_displayed (PidginConversation *gtkconv, gpointer data)
{
	init_theme_for_webkit (gtkconv->active_conv, cur_style_dir);
}

static void
webkit_on_conversation_switched (PurpleConversation *conv, gpointer data)
{
	init_theme_for_webkit (conv, cur_style_dir);
}

static void
webkit_on_conversation_hiding (PidginConversation *gtkconv, gpointer data)
{
	/* 
	 * I'm not sure if I need to do anything here, but let's keep
	 * this anyway. 
	 */
}

static GList*
get_dir_dir_list (const char* dirname)
{
	GList *ret = NULL;
	GDir  *dir = g_dir_open (dirname, 0, NULL);
	const char* subdir;

	if (!dir) return NULL;
	while ((subdir = g_dir_read_name (dir))) {
		ret = g_list_append (ret, g_build_filename (dirname, subdir, NULL));
	}
	
	g_dir_close (dir);
	return ret;
}

/**
 * Get me a list of all the available themes specified by their
 * directories. I don't guarrantee that these are valid themes, just
 * that they are in the directories for themes.
 */
static GList*
get_style_directory_list ()
{
	char *user_dir, *user_style_dir, *global_style_dir;
	GList *list1, *list2;

	user_dir = get_absolute_path (purple_user_dir ());

	user_style_dir = g_build_filename (user_dir, "styles", NULL);
	global_style_dir = g_build_filename (DATADIR, "pidgin", "styles", NULL);

	list1 = get_dir_dir_list (user_style_dir);
	list2 = get_dir_dir_list (global_style_dir);
	
	g_free (global_style_dir);
	g_free (user_style_dir);
	g_free (user_dir);
	
	return g_list_concat (list1, list2);
}

/**
 * use heuristics or previous user options to figure out what 
 * theme to use as default in this Pidgin instance.
 */
static void
style_set_default ()
{
	GList* styles = get_style_directory_list (), *iter;
	const char *stylepath = purple_prefs_get_string ("/plugins/gtk/adiumthemes/stylepath");
	g_assert (cur_style_dir == NULL);

	if (stylepath) 
		styles = g_list_prepend (styles, g_strdup (stylepath));

	/* pick any one that works. Note that we have first preference
	 * for the one in the userdir */
	for (iter = styles; iter; iter = g_list_next (iter)) {
		PidginMessageStyle *style = pidgin_message_style_load (iter->data);
		if (style) {
			cur_style_dir = (char*) g_strdup (iter->data);
			pidgin_message_style_unref (style);
			break;
		}
		purple_debug_info ("webkit", "Style %s is invalid\n", (char*) iter->data);
	}

	for (iter = styles; iter; iter = g_list_next (iter))
		g_free (iter->data);
	g_list_free (styles);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	style_set_default ();
	if (!cur_style_dir) return FALSE; /* couldn't find a style */

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "displaying-im-msg",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_displaying_im_msg),
			       NULL);
			    
	purple_signal_connect (pidgin_conversations_get_handle (),
			       "displaying-chat-msg",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_displaying_chat_msg),
			       NULL);

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-displayed",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_conversation_displayed),
			       NULL);

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-switched",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_conversation_switched),
			       NULL);

	purple_signal_connect (pidgin_conversations_get_handle (),
			       "conversation-hiding",
			       webkit_plugin_get_handle (),
			       PURPLE_CALLBACK(webkit_on_conversation_hiding),
			       NULL);

	/* finally update each of the existing conversation windows */
	{
		GList* list = purple_get_conversations ();
		for (;list; list = g_list_next(list))
			init_theme_for_webkit (list->data, cur_style_dir);
			
	}
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	GList *list;

	webkit_plugin_free_handle ();
	cur_style_dir = NULL;
	list = purple_get_conversations ();
	while (list) {
		finalize_theme_for_webkit(list->data);
		list = g_list_next(list);
	}

	return TRUE;
}

/*
 * UI config code
 */

static void
style_changed (GtkWidget* combobox, gpointer null)
{
	char *name = gtk_combo_box_get_active_text (GTK_COMBO_BOX(combobox));
	GtkWidget *dialog;
	GList *styles = get_style_directory_list (), *iter;

	/* find the full path for this name, I wish I could store this info in the combobox itself. :( */
	for (iter = styles; iter; iter = g_list_next(iter)) {
		char* basename = g_path_get_basename (iter->data);
		if (g_str_equal (basename, name)) {
			g_free (basename);
			break;
		}
		g_free (basename);
	}

	g_assert (iter);
	g_free (name);
	g_free (cur_style_dir);
	cur_style_dir = g_strdup (iter->data);;

	/* inform the user that existing conversations haven't changed */
	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "The style for existing conversations have not been changed. Please close and re-open the conversation for the changes to take effect.");
	g_assert (dialog);
	gtk_widget_show (dialog);
	g_signal_connect_swapped (dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
}

static GtkWidget*
get_style_config_frame ()
{
	GtkWidget *combobox = gtk_combo_box_new_text ();
	GList *styles = get_style_directory_list (), *iter;
	int index = 0, selected = 0;

	for (iter = styles; iter; iter = g_list_next (iter)) {
		PidginMessageStyle *style = pidgin_message_style_load (iter->data);
		
		if (style) {
			char *text = g_path_get_basename (iter->data);
			gtk_combo_box_append_text (GTK_COMBO_BOX(combobox), text);
			g_free (text);

			if (g_str_equal (iter->data, cur_style_dir))
				selected = index;
			index++;
			pidgin_message_style_unref (style);
		}
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX(combobox), selected);
	g_signal_connect_after (G_OBJECT(combobox), "changed", G_CALLBACK(style_changed), NULL);
	return combobox;
}

static void
variant_update_conversation (PurpleConversation *conv)
{
	PidginConversation *gtkconv = PIDGIN_CONVERSATION (conv);
	WebKitWebView *webview = WEBKIT_WEB_VIEW (gtkconv->webview);
	PidginMessageStyle *style = (PidginMessageStyle*) g_object_get_data (G_OBJECT(webview), MESSAGE_STYLE_KEY);
	char *script;

	g_assert (style);

	script = g_strdup_printf ("setStylesheet(\"mainStyle\",\"%s\")", pidgin_message_style_get_css (style));
	gtk_webview_safe_execute_script (GTK_WEBVIEW(webview), script);

	set_theme_font_settings (WEBKIT_WEB_VIEW (gtkconv->webview), style);
	g_free (script);
}

static void
variant_changed (GtkWidget* combobox, gpointer null)
{
	char *name;
	GList *list;
	PidginMessageStyle *style = pidgin_message_style_load (cur_style_dir);

	g_assert (style);
	name = gtk_combo_box_get_active_text (GTK_COMBO_BOX (combobox));
	pidgin_message_style_set_variant (style, name);

	/* update conversations */
	list = purple_get_conversations ();
	while (list) {
		variant_update_conversation (list->data);
		list = g_list_next(list);
	}

	g_free (name);
	pidgin_message_style_unref (style);
}

static GtkWidget *
get_variant_config_frame() 
{
	PidginMessageStyle *style = pidgin_message_style_load (cur_style_dir);
	GList *variants = pidgin_message_style_get_variants (style), *iter;
	char *cur_variant = pidgin_message_style_get_variant (style);
	GtkWidget *combobox = gtk_combo_box_new_text();	
	int def = -1, index = 0;

	pidgin_message_style_unref (style);

	for (iter = variants; iter; iter = g_list_next (iter)) {
		gtk_combo_box_append_text (GTK_COMBO_BOX(combobox), iter->data);

		if (g_str_equal (cur_variant, iter->data))
			def = index;
		index ++;
		
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX(combobox), def);
	g_signal_connect (G_OBJECT(combobox), "changed", G_CALLBACK(variant_changed), NULL);

	return combobox;
}

static void
style_changed_reset_variants (GtkWidget* combobox, gpointer table)
{
	/* I hate to do this, I swear. But I don't know how to cleanly clean an existing combobox */
	GtkWidget* variants = g_object_get_data (G_OBJECT(table), "variants-cbox");
	gtk_widget_destroy (variants);
	variants = get_variant_config_frame ();
	gtk_table_attach_defaults (GTK_TABLE (table), variants, 1, 2, 1, 2);
	gtk_widget_show_all (GTK_WIDGET(table));

	g_object_set_data (G_OBJECT(table), "variants-cbox", variants);
}

static GtkWidget*
get_config_frame(PurplePlugin* plugin)
{
	GtkWidget *table = gtk_table_new (2, 2, FALSE);
	GtkWidget *style_config = get_style_config_frame ();
	GtkWidget *variant_config = get_variant_config_frame ();

	gtk_table_attach_defaults (GTK_TABLE(table), gtk_label_new ("Message Style"), 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table), style_config, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE(table), gtk_label_new ("Style Variant"), 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE(table), variant_config, 1, 2, 1, 2);


	g_object_set_data (G_OBJECT(table), "variants-cbox", variant_config);
	/* to clarify, this is a second signal connected on style config */
	g_signal_connect_after (G_OBJECT(style_config), "changed", G_CALLBACK(style_changed_reset_variants), table);

	return table;
}

PidginPluginUiInfo ui_info =
{
        get_config_frame,
        0, /* page_num (Reserved) */

        /* padding */
        NULL,
        NULL,
        NULL,
        NULL
};


static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,		/* Magic				*/
	PURPLE_MAJOR_VERSION,		/* Purple Major Version	*/
	PURPLE_MINOR_VERSION,		/* Purple Minor Version	*/
	PURPLE_PLUGIN_STANDARD,		/* plugin type			*/
PIDGIN_PLUGIN_TYPE,			/* ui requirement		*/
	0,							/* flags				*/
	NULL,						/* dependencies			*/
	PURPLE_PRIORITY_DEFAULT,	/* priority				*/

	PLUGIN_ID,					/* plugin id			*/
	NULL,						/* name					*/
	"0.1",					/* version				*/
	NULL,						/* summary				*/
	NULL,						/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	"http://pidgin.im",					/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,						/* destroy				*/

	&ui_info,						/* ui_info				*/
	NULL,						/* extra_info			*/
	NULL,						/* prefs_info			*/
	NULL,						/* actions				*/
	NULL,						/* reserved 1			*/
	NULL,						/* reserved 2			*/
	NULL,						/* reserved 3			*/
	NULL						/* reserved 4			*/
};

static void
init_plugin(PurplePlugin *plugin) {
	info.name = "Adium IMs";
	info.summary = "Adium-like IMs with Pidgin";
	info.description = "You can chat in Pidgin using Adium's WebKit view.";

	purple_prefs_add_none ("/plugins");
	purple_prefs_add_none ("/plugins/gtk");
	purple_prefs_add_none ("/plugins/gtk/adiumthemes");
	purple_prefs_add_string ("/plugins/gtk/adiumthemes/csspath", "");
}

PURPLE_INIT_PLUGIN(webkit, init_plugin, info)

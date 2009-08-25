/*
 * Adium Message Styles
 * Copyright (C) 2009  Arnold Noronha <arnstein87@gmail.com>
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

#include "message-style.h"

#include <string.h>

#include <glib.h>

#include <debug.h>
#include <util.h>

static void
glist_free_all_string (GList *list)
{
	GList *first = list;
	for (; list; list = g_list_next (list)) 
		g_free (list->data);
	g_list_free (first);
}

static
PidginMessageStyle* pidgin_message_style_new (const char* styledir)
{
	PidginMessageStyle* ret = g_new0 (PidginMessageStyle, 1);

	ret->ref_counter = 1;
	ret->style_dir = g_strdup (styledir);
	
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
	g_free (style->default_variant);
	style->default_variant = NULL;
}


void pidgin_message_style_unref (PidginMessageStyle *style)
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

	g_free (style);

	pidgin_message_style_unset_info_plist (style);
}

void
pidgin_message_style_save_state (const PidginMessageStyle *style)
{
	char *prefname = g_strdup_printf ("/plugins/gtk/adiumthemes/%s", style->cf_bundle_identifier);
	char *variant = g_strdup_printf ("%s/variant", prefname);

	purple_debug_info ("webkit", "saving state with variant %s\n", style->variant);
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
	char *variant = g_strdup_printf ("%s/variant", prefname);

	const char* value = purple_prefs_get_string (variant);
	gboolean changed = !style->variant || !g_str_equal (style->variant, value);

	g_free (style->variant);
	style->variant = g_strdup (value);

	if (changed) pidgin_message_style_read_info_plist (style, style->variant);

	g_free (prefname);
	g_free(variant);
}


static gboolean
parse_info_plist_key_value (xmlnode* key, gpointer destination, const char* expected)
{
	xmlnode *val = key->next;

	for (; val && val->type != XMLNODE_TYPE_TAG; val = val->next);
	if (!val) return FALSE;
	
	if (expected == NULL || g_str_equal (expected, "string")) {
		char** dest = (char**) destination;
		if (!g_str_equal (val->name, "string")) return FALSE;
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
		if (g_str_equal (val->name, "true")) *dest = TRUE;
		else if (g_str_equal (val->name, "false")) *dest = FALSE;
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
void
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
			pr = parse_info_plist_key_value (iter, &style->allow_text_colors, "integer");
		else if (str_for_key ("ImageMask", key, variant))
			pr = parse_info_plist_key_value (iter, &style->image_mask, "string");

		if (!pr)
			purple_debug_warning ("webkit", "Failed to parse key %s\n", key);
		g_free (key);
	}

	xmlnode_free (plist);
}

PidginMessageStyle*
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
	char   *file; /* temporary variable */
	PidginMessageStyle *style = NULL;

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
		purple_debug_error ("webkit", "Could not locate a Template.html (%s)\n", style->template_path);
		pidgin_message_style_unref (style);
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

	pidgin_message_style_read_info_plist (style, NULL);
	pidgin_message_style_load_state (style);

	/* non variant dependent Info.plist checks */
	if (style->message_view_version < 3) {
		purple_debug_info ("webkit", "%s is a legacy style (version %d) and will not be loaded\n", style->cf_bundle_name, style->message_view_version);
		pidgin_message_style_unref (style);
		return NULL;
	}

	if (!style->variant)
	{
		GList *variants = pidgin_message_style_get_variants (style);

		if (variants)
			pidgin_message_style_set_variant (style, variants->data);

		glist_free_all_string (variants);
	}

	return style;
}

PidginMessageStyle*
pidgin_message_style_copy (const PidginMessageStyle *style)
{
	/* it's at times like this that I miss C++ */
	PidginMessageStyle *ret = pidgin_message_style_new (style->style_dir);

	ret->variant = g_strdup (style->variant);
	ret->message_view_version = style->message_view_version;
	ret->cf_bundle_name = g_strdup (style->cf_bundle_name);
	ret->cf_bundle_identifier = g_strdup (style->cf_bundle_identifier);
	ret->cf_bundle_get_info_string = g_strdup (style->cf_bundle_get_info_string);
	ret->default_font_family = g_strdup (style->default_font_family);
	ret->default_font_size = style->default_font_size;
	ret->shows_user_icons = style->shows_user_icons;
	ret->disable_combine_consecutive = style->disable_combine_consecutive;
	ret->default_background_is_transparent = style->default_background_is_transparent;
	ret->disable_custom_background = style->disable_custom_background;
	ret->default_background_color = g_strdup (style->default_background_color);
	ret->allow_text_colors = style->allow_text_colors;
	ret->image_mask = g_strdup (style->image_mask);
	ret->default_variant = g_strdup (style->default_variant);
	
	ret->template_path = g_strdup (style->template_path);
	ret->template_html = g_strdup (style->template_html);
	ret->header_html = g_strdup (style->header_html);
	ret->footer_html = g_strdup (style->footer_html);
	ret->incoming_content_html = g_strdup (style->incoming_content_html);
	ret->outgoing_content_html = g_strdup (style->outgoing_content_html);
	ret->incoming_next_content_html = g_strdup (style->incoming_next_content_html);
	ret->outgoing_next_content_html = g_strdup (style->outgoing_next_content_html);
	ret->status_html = g_strdup (style->status_html);
	ret->basestyle_css = g_strdup (style->basestyle_css);
	return ret;
}

void
pidgin_message_style_set_variant (PidginMessageStyle *style, const char *variant)
{
	/* I'm not going to test whether this variant is valid! */
	g_free (style->variant);
	style->variant = g_strdup (variant);

	pidgin_message_style_read_info_plist (style, variant);
	
	/* todo, the style has "changed". Ideally, I would like to use signals at this point. */
}

char* pidgin_message_style_get_variant (PidginMessageStyle *style)
{
	return g_strdup (style->variant);
}

/**
 * Get a list of variants supported by the style.
 */
GList*
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


char* pidgin_message_style_get_css (PidginMessageStyle *style)
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



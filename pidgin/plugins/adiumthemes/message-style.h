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

#include <glib.h>

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

	/* Info.plist keys that change with Variant */

	/* Static Info.plist keys */
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
	char     *default_variant;

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

PidginMessageStyle *pidgin_message_style_load(const char *styledir);
PidginMessageStyle *pidgin_message_style_copy(const PidginMessageStyle *style);
void pidgin_message_style_save_state(const PidginMessageStyle *style);
void pidgin_message_style_unref(PidginMessageStyle *style);
void pidgin_message_style_read_info_plist(PidginMessageStyle *style, const char *variant);
char *pidgin_message_style_get_variant(PidginMessageStyle *style);
GList *pidgin_message_style_get_variants(PidginMessageStyle *style);
void pidgin_message_style_set_variant(PidginMessageStyle *style, const char *variant);

char *pidgin_message_style_get_css(PidginMessageStyle *style);


/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
#include "internal.h"
#include "pidgin.h"

#include "image-store.h"
#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "pidginstock.h"
#include "smiley-custom.h"
#include "smiley-list.h"
#include "util.h"
#include "debug.h"

#include "gtkdialogs.h"
#include "gtkwebviewtoolbar.h"
#include "gtksmiley-manager.h"
#include "gtksmiley-theme.h"
#include "gtkutils.h"

#include <gdk/gdkkeysyms.h>

#include "gtk3compat.h"

#define PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_WEBVIEWTOOLBAR, PidginWebViewToolbarPriv))

#define PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FONT "sans-serif"
#define PIDGIN_WEBVIEWTOOLBAR_DEFAULT_BGCOLOR "inherit"
#define PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FGCOLOR "#000000"

/******************************************************************************
 * Structs
 *****************************************************************************/

typedef struct _PidginWebViewToolbarPriv {
	PurpleConversation *active_conv;

	GtkWidget *wide_view;
	GtkWidget *lean_view;

	GtkWidget *font_label;
	GtkWidget *font_menu;

	GtkAction *bold;
	GtkAction *italic;
	GtkAction *underline;
	GtkAction *strike;

	GtkAction *larger_size;
#if 0
	GtkAction *normal_size;
#endif
	GtkAction *smaller_size;

	GtkAction *font;
	GtkAction *fgcolor;
	GtkAction *bgcolor;

	GtkAction *clear;

	GtkWidget *insert_menu;
	GtkAction *image;
	GtkAction *link;
	GtkAction *hr;

	GtkAction *smiley;
	GtkAction *attention;

	GtkWidget *font_dialog;
	GtkWidget *fgcolor_dialog;
	GtkWidget *bgcolor_dialog;
	GtkWidget *link_dialog;
	GtkWidget *smiley_dialog;
	GtkWidget *image_dialog;

	gboolean allow_smileys;
} PidginWebViewToolbarPriv;

/******************************************************************************
 * Globals
 *****************************************************************************/

static GtkBoxClass *parent_class = NULL;

/******************************************************************************
 * Prototypes
 *****************************************************************************/

static void
toggle_action_set_active_block(GtkToggleAction *action, gboolean is_active,
                               PidginWebViewToolbar *toolbar);

/******************************************************************************
 * Helpers
 *****************************************************************************/

static gboolean
pidgin_color_parse(const gchar *str, GdkRGBA *color)
{
	GdkRGBA dummy_color;

	if (str == NULL)
		return FALSE;

	while (isspace(str[0]))
		str++;

	if (str[0] == '\0')
		return FALSE;

	if (color == NULL)
		color = &dummy_color;

	if ((strcmp(str, "inherit") == 0) || (strcmp(str, "rgba(0, 0, 0, 0)") == 0)) {
		return FALSE;
	} else {
		return gdk_rgba_parse(color, str);
	}
}

static gchar*
pidgin_color_to_str(GdkRGBA *color)
{
	return g_strdup_printf("#%02X%02X%02X",
		(unsigned int)(color->red * 255),
		(unsigned int)(color->green * 255),
		(unsigned int)(color->blue * 255));
}

static void
do_bold(GtkAction *bold, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	pidgin_webview_toggle_bold(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
do_italic(GtkAction *italic, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	pidgin_webview_toggle_italic(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
do_underline(GtkAction *underline, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	pidgin_webview_toggle_underline(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
do_strikethrough(GtkAction *strikethrough, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	pidgin_webview_toggle_strike(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
do_small(GtkAction *small, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	pidgin_webview_font_shrink(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
do_big(GtkAction *large, PidginWebViewToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	pidgin_webview_font_grow(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
destroy_toolbar_font(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (priv->font_dialog != NULL)
	{
		gtk_widget_destroy(priv->font_dialog);
		priv->font_dialog = NULL;
	}
}

static void
apply_font(GtkDialog *dialog, gint response, PidginWebViewToolbar *toolbar)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	gchar *fontname = NULL;

	if (response == GTK_RESPONSE_OK)
		fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dialog));

	if (fontname) {
		PangoFontDescription *desc;
		const gchar *family_name;

		desc = pango_font_description_from_string(fontname);
		family_name = pango_font_description_get_family(desc);

		if (family_name) {
			pidgin_webview_toggle_fontface(PIDGIN_WEBVIEW(toolbar->webview),
			                            family_name);
		}

		pango_font_description_free(desc);
		g_free(fontname);
	} else {
		pidgin_webview_toggle_fontface(PIDGIN_WEBVIEW(toolbar->webview),
			PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FONT);
	}

	destroy_toolbar_font(toolbar);
}

static void
toggle_font(GtkAction *font, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(font))) {
		char *fontname = pidgin_webview_get_current_fontface(PIDGIN_WEBVIEW(toolbar->webview));

		if (!priv->font_dialog) {
			GtkWindow *window;
			window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(toolbar)));
			priv->font_dialog = gtk_font_chooser_dialog_new(_("Select Font"), window);

			if (fontname) {
				char *fonttif = g_strdup_printf("%s 12", fontname);
				gtk_font_chooser_set_font(GTK_FONT_CHOOSER(priv->font_dialog),
				                          fonttif);
				g_free(fonttif);
			} else {
				gtk_font_chooser_set_font(GTK_FONT_CHOOSER(priv->font_dialog),
				                          PIDGIN_DEFAULT_FONT_FACE);
			}

			g_signal_connect(G_OBJECT(priv->font_dialog), "response",
			                 G_CALLBACK(apply_font), toolbar);
		}

		gtk_window_present(GTK_WINDOW(priv->font_dialog));

		g_free(fontname);
	} else {
		pidgin_webview_toggle_fontface(PIDGIN_WEBVIEW(toolbar->webview),
			PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FONT);
		destroy_toolbar_font(toolbar);
	}

	gtk_widget_grab_focus(toolbar->webview);
}

static gboolean
destroy_toolbar_fgcolor(GtkWidget *widget, GdkEvent *event,
	PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (widget != NULL) {
		pidgin_webview_toggle_forecolor(
			PIDGIN_WEBVIEW(toolbar->webview),
			PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FGCOLOR);
	}

	if (priv->fgcolor_dialog != NULL) {
		gtk_widget_destroy(priv->fgcolor_dialog);
		priv->fgcolor_dialog = NULL;
	}

	return FALSE;
}

static void
do_fgcolor(GtkDialog *dialog, gint response, gpointer _toolbar)
{
	PidginWebViewToolbar *toolbar = _toolbar;
	GdkRGBA text_color;
	gchar *open_tag;

	if (response != GTK_RESPONSE_OK) {
		destroy_toolbar_fgcolor(GTK_WIDGET(toolbar), NULL, toolbar);
		return;
	}

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &text_color);
	open_tag = pidgin_color_to_str(&text_color);
	pidgin_webview_toggle_forecolor(PIDGIN_WEBVIEW(toolbar->webview),
		open_tag);
	g_free(open_tag);

	destroy_toolbar_fgcolor(NULL, NULL, toolbar);
}

static void
toggle_fg_color(GtkAction *color, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(color))) {
		GdkRGBA fgcolor;
		gchar *color = pidgin_webview_get_current_forecolor(
			PIDGIN_WEBVIEW(toolbar->webview));

		if (!priv->fgcolor_dialog) {
			priv->fgcolor_dialog = gtk_color_chooser_dialog_new(
				_("Select Text Color"), GTK_WINDOW(
				gtk_widget_get_ancestor(toolbar->webview,
				GTK_TYPE_WINDOW)));
			gtk_color_chooser_set_use_alpha(
				GTK_COLOR_CHOOSER(priv->fgcolor_dialog), FALSE);

			if (pidgin_color_parse(color, &fgcolor)) {
				gtk_color_chooser_set_rgba(
					GTK_COLOR_CHOOSER(priv->fgcolor_dialog),
					&fgcolor);
			}

			g_signal_connect(G_OBJECT(priv->fgcolor_dialog),
				"delete_event",
				G_CALLBACK(destroy_toolbar_fgcolor), toolbar);

			g_signal_connect(G_OBJECT(priv->fgcolor_dialog),
				"response", G_CALLBACK(do_fgcolor), toolbar);
		}

		g_free(color);

		gtk_window_present(GTK_WINDOW(priv->fgcolor_dialog));
	} else {
		destroy_toolbar_fgcolor(GTK_WIDGET(toolbar), NULL, toolbar);
	}

	gtk_widget_grab_focus(toolbar->webview);
}

static gboolean
destroy_toolbar_bgcolor(GtkWidget *widget, GdkEvent *event,
	PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	if (widget != NULL) {
		pidgin_webview_toggle_backcolor(
			PIDGIN_WEBVIEW(toolbar->webview),
			PIDGIN_WEBVIEWTOOLBAR_DEFAULT_BGCOLOR);
	}

	if (priv->bgcolor_dialog != NULL) {
		gtk_widget_destroy(priv->bgcolor_dialog);
		priv->bgcolor_dialog = NULL;
	}

	return FALSE;
}

static void
do_bgcolor(GtkDialog *dialog, gint response, gpointer _toolbar)
{
	PidginWebViewToolbar *toolbar = _toolbar;
	GdkRGBA text_color;
	gchar *open_tag;

	if (response != GTK_RESPONSE_OK) {
		destroy_toolbar_bgcolor(GTK_WIDGET(toolbar), NULL, toolbar);
		return;
	}

	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &text_color);
	open_tag = pidgin_color_to_str(&text_color);
	pidgin_webview_toggle_backcolor(PIDGIN_WEBVIEW(toolbar->webview),
		open_tag);
	g_free(open_tag);

	destroy_toolbar_bgcolor(NULL, NULL, toolbar);
}

static void
toggle_bg_color(GtkAction *color, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(color))) {
		GdkRGBA bgcolor;
		gchar *color = pidgin_webview_get_current_backcolor(
			PIDGIN_WEBVIEW(toolbar->webview));

		if (!priv->bgcolor_dialog) {
			priv->bgcolor_dialog = gtk_color_chooser_dialog_new(
				_("Select Background Color"), GTK_WINDOW(
				gtk_widget_get_ancestor(toolbar->webview,
				GTK_TYPE_WINDOW)));
			gtk_color_chooser_set_use_alpha(
				GTK_COLOR_CHOOSER(priv->bgcolor_dialog), FALSE);

			if (pidgin_color_parse(color, &bgcolor)) {
				gtk_color_chooser_set_rgba(
					GTK_COLOR_CHOOSER(priv->bgcolor_dialog),
					&bgcolor);
			}

			g_signal_connect(G_OBJECT(priv->bgcolor_dialog),
				"delete_event",
				G_CALLBACK(destroy_toolbar_bgcolor), toolbar);
			g_signal_connect(G_OBJECT(priv->bgcolor_dialog),
				"response", G_CALLBACK(do_bgcolor), toolbar);
		}
		g_free(color);

		gtk_window_present(GTK_WINDOW(priv->bgcolor_dialog));
	} else {
		destroy_toolbar_bgcolor(GTK_WIDGET(toolbar), NULL, toolbar);
	}

	gtk_widget_grab_focus(toolbar->webview);
}

static void
clear_formatting_cb(GtkAction *clear, PidginWebViewToolbar *toolbar)
{
	pidgin_webview_clear_formatting(PIDGIN_WEBVIEW(toolbar->webview));
	gtk_widget_grab_focus(toolbar->webview);
}

static void
cancel_link_cb(PidginWebViewToolbar *toolbar, PurpleRequestFields *fields)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(priv->link), FALSE);

	priv->link_dialog = NULL;
}

static void
close_link_dialog(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	if (priv->link_dialog != NULL)
	{
		purple_request_close(PURPLE_REQUEST_FIELDS, priv->link_dialog);
		priv->link_dialog = NULL;
	}
}

static void
do_insert_link_cb(PidginWebViewToolbar *toolbar, PurpleRequestFields *fields)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	const char *url, *description;

	url = purple_request_fields_get_string(fields, "url");
	if (pidgin_webview_get_format_functions(PIDGIN_WEBVIEW(toolbar->webview)) & PIDGIN_WEBVIEW_LINKDESC)
		description = purple_request_fields_get_string(fields, "description");
	else
		description = NULL;

	pidgin_webview_insert_link(PIDGIN_WEBVIEW(toolbar->webview), url, description);

	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(priv->link), FALSE);

	priv->link_dialog = NULL;
}

static void
insert_link_cb(GtkAction *action, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(priv->link))) {
		PurpleRequestFields *fields;
		PurpleRequestFieldGroup *group;
		PurpleRequestField *field;
		char *msg;
		char *desc = NULL;

		fields = purple_request_fields_new();

		group = purple_request_field_group_new(NULL);
		purple_request_fields_add_group(fields, group);

		field = purple_request_field_string_new("url", _("_URL"), NULL, FALSE);
		purple_request_field_set_required(field, TRUE);
		purple_request_field_group_add_field(group, field);

		if (pidgin_webview_get_format_functions(PIDGIN_WEBVIEW(toolbar->webview)) & PIDGIN_WEBVIEW_LINKDESC) {
			desc = pidgin_webview_get_selected_text(PIDGIN_WEBVIEW(toolbar->webview));
			field = purple_request_field_string_new("description", _("_Description"),
							      desc, FALSE);
			purple_request_field_group_add_field(group, field);
			msg = g_strdup(_("Please enter the URL and description of the "
							 "link that you want to insert. The description "
							 "is optional."));
		} else {
			msg = g_strdup(_("Please enter the URL of the "
									"link that you want to insert."));
		}

		priv->link_dialog =
			purple_request_fields(toolbar, _("Insert Link"), NULL,
				msg, fields, _("_Insert"),
				G_CALLBACK(do_insert_link_cb), _("Cancel"),
				G_CALLBACK(cancel_link_cb), NULL, toolbar);
		g_free(msg);
		g_free(desc);
	} else {
		close_link_dialog(toolbar);
	}

	gtk_widget_grab_focus(toolbar->webview);
}

static void
insert_hr_cb(GtkAction *action, PidginWebViewToolbar *toolbar)
{
	pidgin_webview_insert_hr(PIDGIN_WEBVIEW(toolbar->webview));
}

static void
do_insert_image_cb(GtkWidget *widget, int response, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	gchar *filename = NULL;
	PurpleImage *img;

	if (response == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

	/* The following triggers a callback that closes the widget */
	gtk_action_activate(priv->image);

	if (filename == NULL)
		return;

	img = purple_image_new_from_file(filename, TRUE);

	if (!img) {
		gchar *buf = g_strdup_printf(_("Failed to store image: %s"),
			filename);

		purple_notify_error(NULL, NULL, buf, NULL, NULL);

		g_free(buf);
		g_free(filename);

		return;
	}

	g_free(filename);

	pidgin_webview_insert_image(PIDGIN_WEBVIEW(toolbar->webview), img);
	/* TODO: do it after passing an image to protocol, not before
	 * g_object_unref(img);
	 */
}

static void
insert_image_cb(GtkAction *action, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkWidget *window;

	if (!priv->image_dialog) {
		window = gtk_file_chooser_dialog_new(_("Insert Image"), NULL,
		                                     GTK_FILE_CHOOSER_ACTION_OPEN,
		                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		                                     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		                                     NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_ACCEPT);
		g_signal_connect(G_OBJECT(window), "response",
		                 G_CALLBACK(do_insert_image_cb), toolbar);

		gtk_widget_show(window);
		priv->image_dialog = window;
	} else {
		gtk_widget_destroy(priv->image_dialog);
		priv->image_dialog = NULL;
	}

	gtk_widget_grab_focus(toolbar->webview);
}

static void
destroy_smiley_dialog(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	if (priv->smiley_dialog != NULL)
	{
		gtk_widget_destroy(priv->smiley_dialog);
		priv->smiley_dialog = NULL;
	}
}

static gboolean
close_smiley_dialog(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(priv->smiley), FALSE);
	return FALSE;
}

static void
insert_smiley_text(GtkWidget *widget, PidginWebViewToolbar *toolbar)
{
	PurpleSmiley *smiley;
	PurpleImage *image;
	guint image_id;
	gchar *escaped_smiley, *smiley_html;
	const gchar *smiley_class;

	smiley = g_object_get_data(G_OBJECT(widget), "smiley");
	smiley_class = g_object_get_data(G_OBJECT(widget), "smiley-class");
	image = purple_smiley_get_image(smiley);
	image_id = purple_image_store_add(image);

	escaped_smiley = g_markup_escape_text(
		purple_smiley_get_shortcut(smiley), -1);
	smiley_html = g_strdup_printf("<img src=\"" PURPLE_IMAGE_STORE_PROTOCOL
		"%u\" class=\"emoticon %s-emoticon\" alt=\"%s\" title=\"%s\">",
		image_id, smiley_class, escaped_smiley, escaped_smiley);

	g_free(escaped_smiley);

	pidgin_webview_append_html(PIDGIN_WEBVIEW(toolbar->webview),
		smiley_html);

	g_free(smiley_html);

	close_smiley_dialog(toolbar);
}

static gboolean
smiley_dialog_input_cb(GtkWidget *dialog, GdkEvent *event,
                       PidginWebViewToolbar *toolbar)
{
	if ((event->type == GDK_KEY_PRESS && event->key.keyval == GDK_KEY_Escape) ||
	    (event->type == GDK_BUTTON_PRESS && event->button.button == 1))
	{
		close_smiley_dialog(toolbar);
		return TRUE;
	}

	return FALSE;
}

/* returns: total width */
static gulong
smileys_load_button_thumbs(GList *smileys)
{
	GList *it;
	gulong total_width = 0;

	for (it = smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		GdkPixbuf *pixbuf;
		guint width;

		width = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(smiley),
			"pidgin-webviewtoolbar-button-width"));

		/* smiley is already loaded */
		if (width > 0) {
			total_width += width;
			continue;
		}

		pixbuf = pidgin_pixbuf_from_image(
			purple_smiley_get_image(smiley));
		pixbuf = pidgin_pixbuf_scale_down(pixbuf,
			24, 24, GDK_INTERP_BILINEAR, TRUE);

		if (pixbuf)
			width = gdk_pixbuf_get_width(pixbuf);
		if (width == 0)
			width = 1;

		/* XXX: a padding for the button */
		width += 12;

		g_object_set_data(G_OBJECT(smiley),
			"pidgin-webviewtoolbar-button-width",
			GINT_TO_POINTER(width));
		g_object_set_data_full(G_OBJECT(smiley),
			"pidgin-webviewtoolbar-button-image",
			pixbuf, g_object_unref);

		total_width += width;
	}

	return total_width;
}

static void
add_smiley_list(PidginWebViewToolbar *toolbar, GtkWidget *container,
	GList *smileys, int max_width, PurpleSmileyList *shadow_smileys,
	const gchar *smiley_class)
{
	GList *it;
	GtkWidget *line;
	int line_width = 0;

	if (!smileys)
		return;

	/* TODO: sort smileys by their position in theme */

	line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);
	for (it = smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		GtkWidget *button;
		GdkPixbuf *pixbuf;
		GtkImage *image;
		guint width;
		const gchar *smiley_shortcut;

		smiley_shortcut = purple_smiley_get_shortcut(smiley);

		width = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(smiley),
			"pidgin-webviewtoolbar-button-width"));
		pixbuf = g_object_get_data(G_OBJECT(smiley),
			"pidgin-webviewtoolbar-button-image");
		if (!pixbuf) {
			purple_debug_warning("gtkwebviewtoolbar", "Smiley does "
				"not exists: %s", smiley_shortcut);
			continue;
		}

		image = GTK_IMAGE(gtk_image_new_from_pixbuf(pixbuf));

		button = gtk_button_new();
		gtk_container_add(GTK_CONTAINER(button), GTK_WIDGET(image));

		g_object_ref(smiley);
		g_object_set_data_full(G_OBJECT(button), "smiley",
			smiley, g_object_unref);
		g_object_set_data(G_OBJECT(button),
			"smiley-class", (gpointer)smiley_class);

		g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(insert_smiley_text), toolbar);
		gtk_widget_set_tooltip_text(button, smiley_shortcut);
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

		/* Disable theme smileys shadowed by custom smileys.
		 * There is a case, when a theme smiley have another,
		 * non-shadowed shortcut. But we won't handle it.
		 */
		if (shadow_smileys && purple_smiley_list_get_by_shortcut(
			shadow_smileys, smiley_shortcut))
		{
			gchar tip[1000];
			g_snprintf(tip, sizeof(tip), _("This smiley is "
				"disabled because a custom smiley exists for "
				"this shortcut:\n %s"), smiley_shortcut);
			gtk_widget_set_tooltip_text(button, tip);
			gtk_widget_set_sensitive(button, FALSE);
		}

		gtk_box_pack_start(GTK_BOX(line), button, FALSE, FALSE, 0);
		gtk_widget_show(button);

		line_width += width;
		if (line_width >= max_width && g_list_next(it)) {
			line_width = 0;
			line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);
		}
	}

}

static void
insert_smiley_manage_cb(GtkButton *button, gpointer _dialog)
{
	GtkWidget *dialog = _dialog;

	gtk_widget_destroy(dialog);
	pidgin_smiley_manager_show();
}

static void
insert_smiley_cb(GtkAction *smiley, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	PurpleSmileyList *smileys_from_theme, *smileys_from_custom = NULL;
	GList *theme_smileys = NULL, *custom_smileys = NULL, *it, *it_next;
	PidginWebViewButtons webview_format;

	GtkWidget *dialog, *vbox;
	GtkWidget *smiley_table = NULL;
	gboolean supports_custom = FALSE;
	GtkRequisition req;
	GtkWidget *scrolled, *viewport;

	if (!gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(smiley))) {
		destroy_smiley_dialog(toolbar);
		gtk_widget_grab_focus(toolbar->webview);
		return;
	}

	webview_format = pidgin_webview_get_format_functions(
		PIDGIN_WEBVIEW(toolbar->webview));

	smileys_from_theme = pidgin_smiley_theme_for_conv(priv->active_conv);
	if (smileys_from_theme) {
		theme_smileys = purple_smiley_list_get_unique(
			smileys_from_theme);
	}

	/* remove hidden theme smileys */
	for (it = theme_smileys; it; it = it_next) {
		PurpleSmiley *smiley = it->data;
		it_next = g_list_next(it);

		if (!g_object_get_data(G_OBJECT(smiley),
			"pidgin-smiley-hidden"))
		{
			continue;
		}

		theme_smileys = g_list_delete_link(theme_smileys, it);
	}

	supports_custom = (webview_format & PIDGIN_WEBVIEW_CUSTOM_SMILEY);
	if (supports_custom) {
		smileys_from_custom = purple_smiley_custom_get_list();
		custom_smileys = purple_smiley_list_get_all(
			smileys_from_custom);
	}

	dialog = pidgin_create_dialog(_("Smile!"), 0, "smiley_dialog", FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	vbox = pidgin_dialog_get_vbox_with_properties(
		GTK_DIALOG(dialog), FALSE, 0);

	if (theme_smileys != NULL || custom_smileys != NULL) {
		guint max_line_width, num_lines, button_width = 0;

		/* Fill the cache (images and their widths). */
		max_line_width = smileys_load_button_thumbs(theme_smileys);
		max_line_width += smileys_load_button_thumbs(custom_smileys);
		num_lines = sqrt(g_list_length(theme_smileys) +
			g_list_length(custom_smileys));
		max_line_width /= num_lines;

		/* We use hboxes packed in a vbox. */
		smiley_table = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

		/* Custom smileys manager button. */
		if (supports_custom) {
			GtkWidget *manage = gtk_button_new_with_mnemonic(
				_("_Manage custom smileys"));
			GtkRequisition req;
			g_signal_connect(G_OBJECT(manage), "clicked",
				G_CALLBACK(insert_smiley_manage_cb), dialog);
			gtk_box_pack_end(GTK_BOX(vbox), manage, FALSE, TRUE, 0);
			gtk_widget_get_preferred_size(manage, NULL, &req);
			button_width = req.width;
			max_line_width = MAX(button_width, max_line_width);
		} else {
			max_line_width = MAX(max_line_width, 100);
		}

		/* Add buttons for smileys. */
		if (theme_smileys) {
			add_smiley_list(toolbar, smiley_table, theme_smileys,
				max_line_width, smileys_from_custom, "theme");
		}
		if (theme_smileys && custom_smileys) {
			gtk_box_pack_start(GTK_BOX(smiley_table),
				gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
				TRUE, FALSE, 0);
		}
		if (custom_smileys) {
			add_smiley_list(toolbar, smiley_table, custom_smileys,
				max_line_width, NULL, "custom");
		}

		gtk_widget_add_events(dialog, GDK_KEY_PRESS_MASK);
	} else {
		smiley_table = gtk_label_new(_("This theme has no available smileys."));
		gtk_widget_add_events(dialog, GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);
		g_signal_connect(G_OBJECT(dialog), "button-press-event", (GCallback)smiley_dialog_input_cb, toolbar);
	}

	g_list_free(theme_smileys);
	g_list_free(custom_smileys);

	scrolled = pidgin_make_scrollable(smiley_table, GTK_POLICY_NEVER, GTK_POLICY_NEVER, GTK_SHADOW_NONE, -1, -1);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
	gtk_widget_show(smiley_table);

	viewport = gtk_widget_get_parent(smiley_table);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);

	/* connect signals */
	g_signal_connect_swapped(G_OBJECT(dialog), "destroy", G_CALLBACK(close_smiley_dialog), toolbar);
	g_signal_connect(G_OBJECT(dialog), "key-press-event", G_CALLBACK(smiley_dialog_input_cb), toolbar);

	gtk_window_set_transient_for(GTK_WINDOW(dialog),
			GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(toolbar))));

	/* show everything */
	gtk_widget_show_all(dialog);

	gtk_widget_get_preferred_size(viewport, NULL, &req);
	gtk_widget_set_size_request(scrolled, MIN(300, req.width), MIN(290, req.height));

	/* The window has to be made resizable, and the scrollbars in the scrolled window
	 * enabled only after setting the desired size of the window. If we do either of
	 * these tasks before now, GTK+ miscalculates the required size, and erronously
	 * makes one or both scrollbars visible (sometimes).
	 * I too think this hack is gross. But I couldn't find a better way -- sadrul */
	gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
	g_object_set(G_OBJECT(scrolled),
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		NULL);

#ifdef _WIN32
	winpidgin_ensure_onscreen(dialog);
#endif

	priv->smiley_dialog = dialog;

	gtk_widget_grab_focus(toolbar->webview);
}

static void
update_smiley_button(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv =
		PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	PurpleSmileyList *sl;
	gboolean any_smileys;
	PidginWebViewButtons webview_format = 0;

	g_return_if_fail(priv != NULL);

	if (toolbar->webview) {
		webview_format = pidgin_webview_get_format_functions(
			PIDGIN_WEBVIEW(toolbar->webview));
	}

	if (!priv->allow_smileys) {
		gtk_action_set_sensitive(priv->smiley, FALSE);
		return;
	}

	sl = pidgin_smiley_theme_for_conv(priv->active_conv);
	/* it's possible, that all theme smileys are hidden,
	 * but we won't handle it */
	any_smileys = (sl ? !purple_smiley_list_is_empty(sl) : FALSE);

	if (!any_smileys && (webview_format & PIDGIN_WEBVIEW_CUSTOM_SMILEY)) {
		sl = purple_smiley_custom_get_list();
		any_smileys = (sl ? !purple_smiley_list_is_empty(sl) : FALSE);
	}

	gtk_action_set_sensitive(priv->smiley, any_smileys);
}

static void
send_attention_cb(GtkAction *attention, PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	PurpleConversation *conv = priv->active_conv;
	const gchar *who = purple_conversation_get_name(conv);
	PurpleConnection *gc = purple_conversation_get_connection(conv);

	purple_protocol_send_attention(gc, who, 0);
	gtk_widget_grab_focus(toolbar->webview);
}

static void
update_buttons_cb(PidginWebView *webview, PidginWebViewButtons buttons,
                  PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	gtk_action_set_sensitive(priv->bold, buttons & PIDGIN_WEBVIEW_BOLD);
	gtk_action_set_sensitive(priv->italic, buttons & PIDGIN_WEBVIEW_ITALIC);
	gtk_action_set_sensitive(priv->underline, buttons & PIDGIN_WEBVIEW_UNDERLINE);
	gtk_action_set_sensitive(priv->strike, buttons & PIDGIN_WEBVIEW_STRIKE);

	gtk_action_set_sensitive(priv->larger_size, buttons & PIDGIN_WEBVIEW_GROW);
	gtk_action_set_sensitive(priv->smaller_size, buttons & PIDGIN_WEBVIEW_SHRINK);

	gtk_action_set_sensitive(priv->font, buttons & PIDGIN_WEBVIEW_FACE);
	gtk_action_set_sensitive(priv->fgcolor, buttons & PIDGIN_WEBVIEW_FORECOLOR);
	gtk_action_set_sensitive(priv->bgcolor, buttons & PIDGIN_WEBVIEW_BACKCOLOR);

	gtk_action_set_sensitive(priv->clear,
	                         (buttons & PIDGIN_WEBVIEW_BOLD ||
	                          buttons & PIDGIN_WEBVIEW_ITALIC ||
	                          buttons & PIDGIN_WEBVIEW_UNDERLINE ||
	                          buttons & PIDGIN_WEBVIEW_STRIKE ||
	                          buttons & PIDGIN_WEBVIEW_GROW ||
	                          buttons & PIDGIN_WEBVIEW_SHRINK ||
	                          buttons & PIDGIN_WEBVIEW_FACE ||
	                          buttons & PIDGIN_WEBVIEW_FORECOLOR ||
	                          buttons & PIDGIN_WEBVIEW_BACKCOLOR));

	gtk_action_set_sensitive(priv->image, buttons & PIDGIN_WEBVIEW_IMAGE);
	gtk_action_set_sensitive(priv->link, buttons & PIDGIN_WEBVIEW_LINK);

	priv->allow_smileys = !!(buttons & PIDGIN_WEBVIEW_SMILEY);
	update_smiley_button(toolbar);
}

/* we call this when we want to _set_active the toggle button, it'll
 * block the callback that's connected to the button so we don't have to
 * do the double toggling hack
 */
static void
toggle_action_set_active_block(GtkToggleAction *action, gboolean is_active,
                               PidginWebViewToolbar *toolbar)
{
	GObject *object;
	g_return_if_fail(toolbar);

	object = g_object_ref(action);
	g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA,
	                                0, 0, NULL, NULL, toolbar);

	gtk_toggle_action_set_active(action, is_active);
	g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA,
	                                  0, 0, NULL, NULL, toolbar);
	g_object_unref(object);
}

static void
update_buttons(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	gboolean bold, italic, underline, strike;
	char *tmp, *color_str;
	char *label;
	GdkRGBA color;

	label = g_strdup(_("_Font"));

	pidgin_webview_get_current_format(PIDGIN_WEBVIEW(toolbar->webview),
	                               &bold, &italic, &underline, &strike);

	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(priv->bold)) != bold)
		toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->bold), bold,
		                               toolbar);
	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(priv->italic)) != italic)
		toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->italic), italic,
		                               toolbar);
	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(priv->underline)) != underline)
		toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->underline),
		                               underline, toolbar);
	if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(priv->strike)) != strike)
		toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->strike), strike,
		                               toolbar);

	if (bold) {
		gchar *markup = g_strdup_printf("<b>%s</b>", label);
		g_free(label);
		label = markup;
	}
	if (italic) {
		gchar *markup = g_strdup_printf("<i>%s</i>", label);
		g_free(label);
		label = markup;
	}
	if (underline) {
		gchar *markup = g_strdup_printf("<u>%s</u>", label);
		g_free(label);
		label = markup;
	}
	if (strike) {
		gchar *markup = g_strdup_printf("<s>%s</s>", label);
		g_free(label);
		label = markup;
	}

	tmp = pidgin_webview_get_current_fontface(PIDGIN_WEBVIEW(toolbar->webview));
	if (tmp && tmp[0] == '\0')
		tmp = NULL;
	if (g_strcmp0(tmp, PIDGIN_WEBVIEWTOOLBAR_DEFAULT_FONT) == 0)
		tmp = NULL;
	toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->font),
	                               (tmp && *tmp), toolbar);
	if (tmp && *tmp) {
		gchar *markup = g_strdup_printf("<span face=\"%s\">%s</span>",
		                                tmp, label);
		g_free(label);
		label = markup;
	}
	g_free(tmp);

	tmp = pidgin_webview_get_current_forecolor(
		PIDGIN_WEBVIEW(toolbar->webview));
	color_str = NULL;
	if (pidgin_color_parse(tmp, &color) &&
		(color.red > 0 || color.green > 0 || color.blue > 0))
	{
		color_str = pidgin_color_to_str(&color);
	}
	g_free(tmp);

	toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->fgcolor),
		color_str != NULL, toolbar);
	if (color_str) {
		gchar *markup = g_strdup_printf(
			"<span foreground=\"%s\">%s</span>", color_str, label);
		g_free(label);
		label = markup;
	}
	g_free(color_str);

	tmp = pidgin_webview_get_current_backcolor(
		PIDGIN_WEBVIEW(toolbar->webview));
	color_str = NULL;
	if (pidgin_color_parse(tmp, &color))
	{
		color_str = pidgin_color_to_str(&color);
	}
	g_free(tmp);

	toggle_action_set_active_block(GTK_TOGGLE_ACTION(priv->bgcolor),
		color_str != NULL, toolbar);
	if (color_str) {
		gchar *markup = g_strdup_printf(
			"<span background=\"%s\">%s</span>", color_str, label);
		g_free(label);
		label = markup;
	}
	g_free(color_str);

	gtk_label_set_markup_with_mnemonic(GTK_LABEL(priv->font_label), label);
}

static void
toggle_button_cb(PidginWebView *webview, PidginWebViewButtons buttons,
                 PidginWebViewToolbar *toolbar)
{
	update_buttons(toolbar);
}

static void
update_format_cb(PidginWebView *webview, PidginWebViewToolbar *toolbar)
{
	update_buttons(toolbar);
}

static void
mark_set_cb(PidginWebView *webview, PidginWebViewToolbar *toolbar)
{
	update_buttons(toolbar);
}

/* This comes from gtkmenutoolbutton.c from gtk+
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 */
static void
menu_position_func(GtkMenu  *menu,
                   int      *x,
                   int      *y,
                   gboolean *push_in,
                   gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkAllocation allocation;
	int savy;

	gtk_widget_get_allocation(widget, &allocation);
	gdk_window_get_origin(gtk_widget_get_window(widget), x, y);
	*x += allocation.x;
	*y += allocation.y + allocation.height;
	savy = *y;

	pidgin_menu_position_func_helper(menu, x, y, push_in, data);

	if (savy > *y + 1)
		*y -= allocation.height;
}

static void
pidgin_menu_clicked(GtkWidget *button, GtkMenu *menu)
{
	if (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(button))) {
		gtk_widget_show_all(GTK_WIDGET(menu));
		gtk_menu_popup(menu, NULL, NULL, menu_position_func, button, 0, gtk_get_current_event_time());
	}
}

static void
pidgin_menu_deactivate(GtkWidget *menu, GtkToggleButton *button)
{
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(button), FALSE);
}

static void
switch_toolbar_view(GtkWidget *item, PidginWebViewToolbar *toolbar)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
			!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide"));
}

static gboolean
pidgin_webviewtoolbar_popup_menu(GtkWidget *widget, GdkEventButton *event,
                              PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkWidget *menu;
	GtkWidget *item;
	gboolean wide;

	if (event->button != 3)
		return FALSE;

	wide = gtk_widget_get_visible(priv->wide_view);

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic(wide ? _("Group Items") : _("Ungroup Items"));
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(switch_toolbar_view), toolbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, pidgin_menu_position_func_helper,
	               widget, event->button, event->time);

	return TRUE;
}

static void
enable_markup(GtkWidget *widget, gpointer null)
{
	GtkWidget *label;
	label = gtk_bin_get_child(GTK_BIN(widget));
	if (GTK_IS_LABEL(label))
		g_object_set(G_OBJECT(label), "use-markup", TRUE, NULL);
}

static void
webviewtoolbar_view_pref_changed(const char *name, PurplePrefType type,
                                 gconstpointer value, gpointer toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	if (value) {
		gtk_widget_hide(priv->lean_view);
		gtk_widget_show_all(priv->wide_view);
	} else {
		gtk_widget_hide(priv->wide_view);
		gtk_widget_show_all(priv->lean_view);
	}
}

/******************************************************************************
 * GObject stuff
 *****************************************************************************/

static void
pidgin_webviewtoolbar_finalize(GObject *object)
{
	PidginWebViewToolbar *toolbar = PIDGIN_WEBVIEWTOOLBAR(object);
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	if (priv->image_dialog != NULL)
	{
		gtk_widget_destroy(priv->image_dialog);
		priv->image_dialog = NULL;
	}

	destroy_toolbar_font(toolbar);
	if (priv->smiley_dialog != NULL) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(priv->smiley_dialog), close_smiley_dialog, toolbar);
		destroy_smiley_dialog(toolbar);
	}
	destroy_toolbar_bgcolor(NULL, NULL, toolbar);
	destroy_toolbar_fgcolor(NULL, NULL, toolbar);
	close_link_dialog(toolbar);
	if (toolbar->webview) {
		g_signal_handlers_disconnect_matched(toolbar->webview,
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
				toolbar);
#if 0
		g_signal_handlers_disconnect_matched(PIDGIN_WEBVIEW(toolbar->webview)->text_buffer,
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
				toolbar);
#endif
	}

	if (priv->font_menu)
		gtk_widget_destroy(priv->font_menu);
	if (priv->insert_menu)
		gtk_widget_destroy(priv->insert_menu);

	purple_prefs_disconnect_by_handle(object);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
pidgin_webviewtoolbar_class_init(gpointer _class, gpointer class_data)
{
	PidginWebViewToolbarClass *class = _class;
	GObjectClass *gobject_class = _class;

	parent_class = g_type_class_ref(GTK_TYPE_BOX);
	gobject_class->finalize = pidgin_webviewtoolbar_finalize;

	g_type_class_add_private(class, sizeof(PidginWebViewToolbarPriv));

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/toolbar");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide", FALSE);
}

static void
pidgin_webviewtoolbar_create_actions(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkActionGroup *action_group;
	gsize i;
	struct {
		GtkAction **action;
		char *name;
		char *stock;
		char *label;
		char *tooltip;
		void (*cb)();
		gboolean toggle;
	} actions[] = {
		{&priv->bold, "ToggleBold", GTK_STOCK_BOLD, N_("<b>_Bold</b>"), N_("Bold"), do_bold, TRUE},
		{&priv->italic, "ToggleItalic", GTK_STOCK_ITALIC, N_("<i>_Italic</i>"), N_("Italic"), do_italic, TRUE},
		{&priv->underline, "ToggleUnderline", GTK_STOCK_UNDERLINE, N_("<u>_Underline</u>"), N_("Underline"), do_underline, TRUE},
		{&priv->strike, "ToggleStrike", GTK_STOCK_STRIKETHROUGH, N_("<span strikethrough='true'>Strikethrough</span>"), N_("Strikethrough"), do_strikethrough, TRUE},
		{&priv->larger_size, "ToggleLarger", PIDGIN_STOCK_TOOLBAR_TEXT_LARGER, N_("<span size='larger'>Larger</span>"), N_("Increase Font Size"), do_big, FALSE},
#if 0
		{&priv->normal_size, "ToggleNormal", NULL, N_("Normal"), N_("Normal Font Size"), NULL, FALSE},
#endif
		{&priv->smaller_size, "ToggleSmaller", PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER, N_("<span size='smaller'>Smaller</span>"), N_("Decrease Font Size"), do_small, FALSE},
		{&priv->font, "ToggleFontFace", PIDGIN_STOCK_TOOLBAR_FONT_FACE, N_("_Font face"), N_("Font Face"), toggle_font, TRUE},
		{&priv->fgcolor, "ToggleFG", PIDGIN_STOCK_TOOLBAR_FGCOLOR, N_("Foreground _color"), N_("Foreground Color"), toggle_fg_color, TRUE},
		{&priv->bgcolor, "ToggleBG", PIDGIN_STOCK_TOOLBAR_BGCOLOR, N_("Bac_kground color"), N_("Background Color"), toggle_bg_color, TRUE},
		{&priv->clear, "ResetFormat", PIDGIN_STOCK_CLEAR, N_("_Reset formatting"), N_("Reset Formatting"), clear_formatting_cb, FALSE},
		{&priv->image, "InsertImage", PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE, N_("_Image"), N_("Insert IM Image"), insert_image_cb, FALSE},
		{&priv->link, "InsertLink", PIDGIN_STOCK_TOOLBAR_INSERT_LINK, N_("_Link"), N_("Insert Link"), insert_link_cb, TRUE},
		{&priv->hr, "InsertHR", NULL, N_("_Horizontal rule"), N_("Insert Horizontal rule"), insert_hr_cb, FALSE},
		{&priv->smiley, "InsertSmiley", PIDGIN_STOCK_TOOLBAR_SMILEY, N_("_Smile!"), N_("Insert Smiley"), insert_smiley_cb, TRUE},
		{&priv->attention, "SendAttention", PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION, N_("_Attention!"), N_("Get Attention"), send_attention_cb, FALSE},
	};

	action_group = gtk_action_group_new("PidginWebViewToolbar");
#ifdef ENABLE_NLS
	gtk_action_group_set_translation_domain(action_group, PACKAGE);
#endif

	for (i = 0; i < G_N_ELEMENTS(actions); i++) {
		GtkAction *action;
		if (actions[i].toggle) {
			action = GTK_ACTION(gtk_toggle_action_new(
				actions[i].name, _(actions[i].label),
				_(actions[i].tooltip), actions[i].stock));
		} else {
			action = gtk_action_new(actions[i].name,
				_(actions[i].label), _(actions[i].tooltip),
				actions[i].stock);
		}
		gtk_action_set_is_important(action, TRUE);
		gtk_action_group_add_action(action_group, action);
		g_signal_connect(G_OBJECT(action), "activate", actions[i].cb, toolbar);
		*(actions[i].action) = action;
	}
}

static void
pidgin_webviewtoolbar_create_wide_view(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkAction *layout[] = {
		priv->bold,
		priv->italic,
		priv->underline,
		priv->strike,
		NULL,
		priv->larger_size,
#if 0
		priv->normal_size,
#endif
		priv->smaller_size,
		NULL,
		priv->font,
		priv->fgcolor,
		priv->bgcolor,
		NULL,
		priv->clear,
		NULL,
		priv->image,
		priv->link,
		NULL,
		priv->smiley,
		priv->attention
	};
	gsize i;
	GtkToolItem *item;

	priv->wide_view = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(priv->wide_view),
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_toolbar_set_style(GTK_TOOLBAR(priv->wide_view), GTK_TOOLBAR_ICONS);

	for (i = 0; i < G_N_ELEMENTS(layout); i++) {
		if (layout[i]) {
			item = GTK_TOOL_ITEM(gtk_action_create_tool_item(layout[i]));
			g_object_set_data(G_OBJECT(item), "action", layout[i]);
		} else
			item = gtk_separator_tool_item_new();
		gtk_toolbar_insert(GTK_TOOLBAR(priv->wide_view), item, -1);
	}
}

static inline void
lean_view_add_menu_item(GtkWidget *menu, GtkAction *action)
{
	GtkWidget *menuitem;

	menuitem = gtk_action_create_menu_item(action);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_object_set_data(G_OBJECT(menuitem), "action", action);
}

static void
pidgin_webviewtoolbar_create_lean_view(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkWidget *label;
	GtkToolItem *sep;
	GtkToolItem *font_button;
	GtkWidget *font_menu;
	GtkToolItem *insert_button;
	GtkWidget *insert_menu;
	GtkWidget *smiley_button;
	GtkWidget *attention_button;

	priv->lean_view = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(priv->lean_view),
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_toolbar_set_style(GTK_TOOLBAR(priv->lean_view), GTK_TOOLBAR_BOTH_HORIZ);

	/* Fonts */
	font_button = gtk_toggle_tool_button_new();
	g_object_set_data_full(G_OBJECT(font_button), "menu-name",
		g_strdup("font"), g_free);
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view), font_button, -1);
	gtk_tool_item_set_is_important(font_button, TRUE);
	gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(font_button), GTK_STOCK_BOLD);
	priv->font_label = label = gtk_label_new_with_mnemonic(_("_Font"));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(font_button), label);

	priv->font_menu = font_menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(font_button), "menu", font_menu);

	lean_view_add_menu_item(font_menu, priv->bold);
	lean_view_add_menu_item(font_menu, priv->italic);
	lean_view_add_menu_item(font_menu, priv->underline);
	lean_view_add_menu_item(font_menu, priv->strike);
	lean_view_add_menu_item(font_menu, priv->larger_size);
#if 0
	lean_view_add_menu_item(font_menu, priv->normal_size);
#endif
	lean_view_add_menu_item(font_menu, priv->smaller_size);
	lean_view_add_menu_item(font_menu, priv->font);
	lean_view_add_menu_item(font_menu, priv->fgcolor);
	lean_view_add_menu_item(font_menu, priv->bgcolor);
	lean_view_add_menu_item(font_menu, priv->clear);

	g_signal_connect(G_OBJECT(font_button), "toggled",
	                 G_CALLBACK(pidgin_menu_clicked), font_menu);
	g_signal_connect_object(G_OBJECT(font_menu), "deactivate",
	                        G_CALLBACK(pidgin_menu_deactivate), font_button, 0);

	gtk_container_foreach(GTK_CONTAINER(font_menu), enable_markup, NULL);

	/* Sep */
	sep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view), sep, -1);

	/* Insert */
	insert_button = gtk_toggle_tool_button_new();
	g_object_set_data_full(G_OBJECT(insert_button), "menu-name",
		g_strdup("insert"), g_free);
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view), insert_button, -1);
	gtk_tool_item_set_is_important(insert_button, TRUE);
	gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(insert_button),
	                             PIDGIN_STOCK_TOOLBAR_INSERT);
	label = gtk_label_new_with_mnemonic(_("_Insert"));
	gtk_tool_button_set_label_widget(GTK_TOOL_BUTTON(insert_button), label);

	priv->insert_menu = insert_menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(insert_button), "menu", insert_menu);

	lean_view_add_menu_item(insert_menu, priv->image);
	lean_view_add_menu_item(insert_menu, priv->link);
	lean_view_add_menu_item(insert_menu, priv->hr);

	g_signal_connect(G_OBJECT(insert_button), "toggled",
	                 G_CALLBACK(pidgin_menu_clicked), insert_menu);
	g_signal_connect_object(G_OBJECT(insert_menu), "deactivate",
	                        G_CALLBACK(pidgin_menu_deactivate), insert_button, 0);

	/* Sep */
	sep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view), sep, -1);

	/* Smiley */
	smiley_button = gtk_action_create_tool_item(priv->smiley);
	g_object_set_data(G_OBJECT(smiley_button), "action", priv->smiley);
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view),
	                   GTK_TOOL_ITEM(smiley_button), -1);

	/* Sep */
	sep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view), sep, -1);

	/* Attention */
	attention_button = gtk_action_create_tool_item(priv->attention);
	g_object_set_data(G_OBJECT(attention_button), "action", priv->attention);
	gtk_toolbar_insert(GTK_TOOLBAR(priv->lean_view),
	                   GTK_TOOL_ITEM(attention_button), -1);
}

static void
pidgin_webviewtoolbar_init(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	GtkWidget *hbox = GTK_WIDGET(toolbar);

	pidgin_webviewtoolbar_create_actions(toolbar);
	pidgin_webviewtoolbar_create_wide_view(toolbar);
	pidgin_webviewtoolbar_create_lean_view(toolbar);

	gtk_box_pack_start(GTK_BOX(hbox), priv->wide_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), priv->lean_view, TRUE, TRUE, 0);

	/* set attention button to be greyed out until we get a conversation */
	gtk_action_set_sensitive(priv->attention, FALSE);

	priv->allow_smileys = TRUE;
	update_smiley_button(toolbar);

	purple_prefs_connect_callback(toolbar,
	                              PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
	                              webviewtoolbar_view_pref_changed, toolbar);
	g_signal_connect_data(G_OBJECT(toolbar), "realize",
	                      G_CALLBACK(purple_prefs_trigger_callback),
	                      PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
	                      NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	g_signal_connect(G_OBJECT(hbox), "button-press-event",
	                 G_CALLBACK(pidgin_webviewtoolbar_popup_menu), toolbar);
}

/******************************************************************************
 * Public API
 *****************************************************************************/

GtkWidget *
pidgin_webviewtoolbar_new(void)
{
	return GTK_WIDGET(g_object_new(pidgin_webviewtoolbar_get_type(), NULL));
}

GType
pidgin_webviewtoolbar_get_type(void)
{
	static GType webviewtoolbar_type = 0;

	if (!webviewtoolbar_type) {
		static const GTypeInfo webviewtoolbar_info = {
			sizeof(PidginWebViewToolbarClass),
			NULL,
			NULL,
			pidgin_webviewtoolbar_class_init,
			NULL,
			NULL,
			sizeof(PidginWebViewToolbar),
			0,
			(GInstanceInitFunc)pidgin_webviewtoolbar_init,
			NULL
		};

		webviewtoolbar_type = g_type_register_static(GTK_TYPE_BOX,
				"PidginWebViewToolbar", &webviewtoolbar_info, 0);
	}

	return webviewtoolbar_type;
}

void
pidgin_webviewtoolbar_attach(PidginWebViewToolbar *toolbar, GtkWidget *webview)
{
	PidginWebViewButtons buttons;

	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(PIDGIN_IS_WEBVIEWTOOLBAR(toolbar));
	g_return_if_fail(webview != NULL);
	g_return_if_fail(PIDGIN_IS_WEBVIEW(webview));

	toolbar->webview = webview;
	g_signal_connect(G_OBJECT(webview), "allowed-formats-updated",
	                 G_CALLBACK(update_buttons_cb), toolbar);
	g_signal_connect_after(G_OBJECT(webview), "format-toggled",
	                       G_CALLBACK(toggle_button_cb), toolbar);
	g_signal_connect_after(G_OBJECT(webview), "format-cleared",
	                       G_CALLBACK(update_format_cb), toolbar);
	g_signal_connect(G_OBJECT(webview), "format-updated",
	                 G_CALLBACK(update_format_cb), toolbar);
	g_signal_connect_after(G_OBJECT(webview), "selection-changed",
	                       G_CALLBACK(mark_set_cb), toolbar);

	buttons = pidgin_webview_get_format_functions(PIDGIN_WEBVIEW(webview));
	update_buttons_cb(PIDGIN_WEBVIEW(webview), buttons, toolbar);
	update_buttons(toolbar);
}

void
pidgin_webviewtoolbar_switch_active_conversation(PidginWebViewToolbar *toolbar,
                                              PurpleConversation *conv)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	PurpleProtocol *protocol = purple_connection_get_protocol(gc);

	priv->active_conv = conv;

	/* gray out attention button on protocols that don't support it
	 for the time being it is always disabled for chats */
	gtk_action_set_sensitive(priv->attention,
		conv && protocol && PURPLE_IS_IM_CONVERSATION(conv) &&
		PURPLE_PROTOCOL_IMPLEMENTS(protocol, ATTENTION_IFACE, send));

	update_smiley_button(toolbar);
}

void
pidgin_webviewtoolbar_activate(PidginWebViewToolbar *toolbar,
                            PidginWebViewAction action)
{
	PidginWebViewToolbarPriv *priv;
	GtkAction *act;

	g_return_if_fail(toolbar != NULL);

	priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);
	switch (action) {
		case PIDGIN_WEBVIEW_ACTION_BOLD:
			act = priv->bold;
			break;

		case PIDGIN_WEBVIEW_ACTION_ITALIC:
			act = priv->italic;
			break;

		case PIDGIN_WEBVIEW_ACTION_UNDERLINE:
			act = priv->underline;
			break;

		case PIDGIN_WEBVIEW_ACTION_STRIKE:
			act = priv->strike;
			break;

		case PIDGIN_WEBVIEW_ACTION_LARGER:
			act = priv->larger_size;
			break;

#if 0
		case PIDGIN_WEBVIEW_ACTION_NORMAL:
			act = priv->normal_size;
			break;
#endif

		case PIDGIN_WEBVIEW_ACTION_SMALLER:
			act = priv->smaller_size;
			break;

		case PIDGIN_WEBVIEW_ACTION_FONTFACE:
			act = priv->font;
			break;

		case PIDGIN_WEBVIEW_ACTION_FGCOLOR:
			act = priv->fgcolor;
			break;

		case PIDGIN_WEBVIEW_ACTION_BGCOLOR:
			act = priv->bgcolor;
			break;

		case PIDGIN_WEBVIEW_ACTION_CLEAR:
			act = priv->clear;
			break;

		case PIDGIN_WEBVIEW_ACTION_IMAGE:
			act = priv->image;
			break;

		case PIDGIN_WEBVIEW_ACTION_LINK:
			act = priv->link;
			break;

		case PIDGIN_WEBVIEW_ACTION_HR:
			act = priv->hr;
			break;

		case PIDGIN_WEBVIEW_ACTION_SMILEY:
			act = priv->smiley;
			break;

		case PIDGIN_WEBVIEW_ACTION_ATTENTION:
			act = priv->attention;
			break;

		default:
			g_return_if_reached();
			break;
	}

	gtk_action_activate(act);
}

GtkWidget *
pidgin_webviewtoolbar_get_wide_view(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	g_return_val_if_fail(toolbar != NULL, NULL);

	return priv->wide_view;
}

GtkWidget *
pidgin_webviewtoolbar_get_lean_view(PidginWebViewToolbar *toolbar)
{
	PidginWebViewToolbarPriv *priv = PIDGIN_WEBVIEWTOOLBAR_GET_PRIVATE(toolbar);

	g_return_val_if_fail(toolbar != NULL, NULL);

	return priv->lean_view;
}

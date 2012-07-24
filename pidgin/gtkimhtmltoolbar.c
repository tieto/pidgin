/*
 * @file gtkimhtmltoolbar.c GTK+ IMHtml Toolbar
 * @ingroup pidgin
 */

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

#include "imgstore.h"
#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "pidginstock.h"
#include "util.h"
#include "debug.h"

#include "gtkdialogs.h"
#include "gtkimhtmltoolbar.h"
#include "gtksmiley.h"
#include "gtkthemes.h"
#include "gtkutils.h"

#include <gdk/gdkkeysyms.h>

#define GTK_TOOLTIPS_VAR toolbar->tooltips
#include "gtk3compat.h"

static GtkHBoxClass *parent_class = NULL;

static void toggle_button_set_active_block(GtkToggleButton *button,
										   gboolean is_active,
										   GtkIMHtmlToolbar *toolbar);

static gboolean
gtk_imhtmltoolbar_popup_menu(GtkWidget *widget,
		GdkEventButton *event, GtkIMHtmlToolbar *toolbar);

static void do_bold(GtkWidget *bold, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	gtk_imhtml_toggle_bold(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_italic(GtkWidget *italic, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	gtk_imhtml_toggle_italic(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_underline(GtkWidget *underline, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	gtk_imhtml_toggle_underline(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_strikethrough(GtkWidget *strikethrough, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	gtk_imhtml_toggle_strike(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_small(GtkWidget *smalltb, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	/* Only shrink the font on activation, not deactivation as well */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smalltb)))
		gtk_imhtml_font_shrink(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_big(GtkWidget *large, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	/* Only grow the font on activation, not deactivation as well */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(large)))
		gtk_imhtml_font_grow(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static gboolean
destroy_toolbar_font(GtkWidget *widget, GdkEvent *event,
					 GtkIMHtmlToolbar *toolbar)
{
	if (widget != NULL)
		gtk_imhtml_toggle_fontface(GTK_IMHTML(toolbar->imhtml), "");

	if (toolbar->font_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->font_dialog);
		toolbar->font_dialog = NULL;
	}
	return FALSE;
}

static void
realize_toolbar_font(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	GtkFontSelection *sel;

	sel = GTK_FONT_SELECTION(
		gtk_font_selection_dialog_get_font_selection(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)));
	gtk_widget_hide(gtk_widget_get_parent(
		gtk_font_selection_get_size_entry(sel)));
	gtk_widget_show_all(gtk_font_selection_get_family_list(sel));
	gtk_widget_show(gtk_widget_get_parent(
		gtk_font_selection_get_family_list(sel)));
	gtk_widget_show(gtk_widget_get_parent(gtk_widget_get_parent(
		gtk_font_selection_get_family_list(sel))));
}

static void
cancel_toolbar_font(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_font(widget, NULL, toolbar);
}

static void
apply_font(GtkWidget *widget, GtkFontSelectionDialog *fontsel)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	gchar *fontname = gtk_font_selection_dialog_get_font_name(fontsel);
	GtkIMHtmlToolbar *toolbar = g_object_get_data(G_OBJECT(fontsel),
	                                              "purple_toolbar");

	if (fontname) {
		const gchar *family_name = NULL;
		PangoFontDescription *desc = NULL;

		desc = pango_font_description_from_string(fontname);
		family_name = pango_font_description_get_family(desc);

		if (family_name) {
			gtk_imhtml_toggle_fontface(GTK_IMHTML(toolbar->imhtml),
			                           family_name);
		}

		pango_font_description_free(desc);
		g_free(fontname);
	}

	cancel_toolbar_font(NULL, toolbar);
}

static void
toggle_font(GtkWidget *font, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(font))) {
		char *fontname = gtk_imhtml_get_current_fontface(GTK_IMHTML(toolbar->imhtml));

		if (!toolbar->font_dialog) {
			toolbar->font_dialog = gtk_font_selection_dialog_new(_("Select Font"));

			g_object_set_data(G_OBJECT(toolbar->font_dialog), "purple_toolbar", toolbar);

			if(fontname) {
				char *fonttif = g_strdup_printf("%s 12", fontname);
				g_free(fontname);
				gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog),
														fonttif);
				g_free(fonttif);
			} else {
				gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog),
														DEFAULT_FONT_FACE);
			}

			g_signal_connect(G_OBJECT(toolbar->font_dialog), "delete_event",
							 G_CALLBACK(destroy_toolbar_font), toolbar);
			g_signal_connect(G_OBJECT(
				gtk_font_selection_dialog_get_ok_button(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog))),
				"clicked", G_CALLBACK(apply_font), toolbar->font_dialog);
			g_signal_connect(G_OBJECT(
				gtk_font_selection_dialog_get_cancel_button(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog))),
				"clicked", G_CALLBACK(cancel_toolbar_font), toolbar);
			g_signal_connect_after(G_OBJECT(toolbar->font_dialog), "realize",
							 G_CALLBACK(realize_toolbar_font), toolbar);
		}
		gtk_window_present(GTK_WINDOW(toolbar->font_dialog));
	} else {
		cancel_toolbar_font(font, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static gboolean
destroy_toolbar_fgcolor(GtkWidget *widget, GdkEvent *event,
						GtkIMHtmlToolbar *toolbar)
{
	if (widget != NULL)
		gtk_imhtml_toggle_forecolor(GTK_IMHTML(toolbar->imhtml), "");

	if (toolbar->fgcolor_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->fgcolor_dialog);
		toolbar->fgcolor_dialog = NULL;
	}
	return FALSE;
}

static void cancel_toolbar_fgcolor(GtkWidget *widget,
								   GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_fgcolor(widget, NULL, toolbar);
}

static void do_fgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GtkIMHtmlToolbar *toolbar = g_object_get_data(G_OBJECT(colorsel), "purple_toolbar");
	char *open_tag;

	open_tag = g_malloc(30);
	gtk_color_selection_get_current_color(colorsel, &text_color);
	g_snprintf(open_tag, 23, "#%02X%02X%02X",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);
	gtk_imhtml_toggle_forecolor(GTK_IMHTML(toolbar->imhtml), open_tag);
	g_free(open_tag);

	cancel_toolbar_fgcolor(NULL, toolbar);
}

static void
toggle_fg_color(GtkWidget *color, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color))) {
		GtkWidget *colorsel;
		GdkColor fgcolor;
		char *color = gtk_imhtml_get_current_forecolor(GTK_IMHTML(toolbar->imhtml));

		if (!toolbar->fgcolor_dialog) {
			GtkWidget *ok_button;
			GtkWidget *cancel_button;

			toolbar->fgcolor_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
			colorsel =
				gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog));
			if (color) {
				gdk_color_parse(color, &fgcolor);
				gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &fgcolor);
				g_free(color);
			}

			g_object_set_data(G_OBJECT(colorsel), "purple_toolbar", toolbar);

			g_object_get(G_OBJECT(toolbar->fgcolor_dialog), "ok-button", &ok_button, NULL);
			g_object_get(G_OBJECT(toolbar->fgcolor_dialog), "cancel-button", &cancel_button, NULL);
			g_signal_connect(G_OBJECT(toolbar->fgcolor_dialog), "delete_event",
							 G_CALLBACK(destroy_toolbar_fgcolor), toolbar);
			g_signal_connect(G_OBJECT(ok_button), "clicked", G_CALLBACK(do_fgcolor), colorsel);
			g_signal_connect(G_OBJECT(cancel_button), "clicked",
			                 G_CALLBACK(cancel_toolbar_fgcolor), toolbar);
		}
		gtk_window_present(GTK_WINDOW(toolbar->fgcolor_dialog));
	} else {
		cancel_toolbar_fgcolor(color, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static gboolean
destroy_toolbar_bgcolor(GtkWidget *widget, GdkEvent *event,
						GtkIMHtmlToolbar *toolbar)
{
	if (widget != NULL) {
		if (gtk_text_buffer_get_selection_bounds(GTK_IMHTML(toolbar->imhtml)->text_buffer, NULL, NULL))
			gtk_imhtml_toggle_backcolor(GTK_IMHTML(toolbar->imhtml), "");
		else
			gtk_imhtml_toggle_background(GTK_IMHTML(toolbar->imhtml), "");
	}

	if (toolbar->bgcolor_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->bgcolor_dialog);
		toolbar->bgcolor_dialog = NULL;
	}
	return FALSE;
}

static void
cancel_toolbar_bgcolor(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_bgcolor(widget, NULL, toolbar);
}

static void do_bgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GtkIMHtmlToolbar *toolbar = g_object_get_data(G_OBJECT(colorsel), "purple_toolbar");
	char *open_tag;

	open_tag = g_malloc(30);
	gtk_color_selection_get_current_color(colorsel, &text_color);
	g_snprintf(open_tag, 23, "#%02X%02X%02X",
			   text_color.red / 256,
			   text_color.green / 256,
			   text_color.blue / 256);
	if (gtk_text_buffer_get_selection_bounds(GTK_IMHTML(toolbar->imhtml)->text_buffer, NULL, NULL))
		gtk_imhtml_toggle_backcolor(GTK_IMHTML(toolbar->imhtml), open_tag);
	else
		gtk_imhtml_toggle_background(GTK_IMHTML(toolbar->imhtml), open_tag);
	g_free(open_tag);

	cancel_toolbar_bgcolor(NULL, toolbar);
}

static void
toggle_bg_color(GtkWidget *color, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color))) {
		GtkWidget *colorsel;
		GdkColor bgcolor;
		char *color = gtk_imhtml_get_current_backcolor(GTK_IMHTML(toolbar->imhtml));

		if (!toolbar->bgcolor_dialog) {
			GtkWidget *ok_button;
			GtkWidget *cancel_button;

			toolbar->bgcolor_dialog = gtk_color_selection_dialog_new(_("Select Background Color"));
			colorsel =
				gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog));

			if (color) {
				gdk_color_parse(color, &bgcolor);
				gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &bgcolor);
				g_free(color);
			}

			g_object_set_data(G_OBJECT(colorsel), "purple_toolbar", toolbar);

			g_object_get(G_OBJECT(toolbar->bgcolor_dialog), "ok-button", &ok_button, NULL);
			g_object_get(G_OBJECT(toolbar->bgcolor_dialog), "cancel-button",
			             &cancel_button, NULL);
			g_signal_connect(G_OBJECT(toolbar->bgcolor_dialog), "delete_event",
							 G_CALLBACK(destroy_toolbar_bgcolor), toolbar);
			g_signal_connect(G_OBJECT(ok_button), "clicked",
			                 G_CALLBACK(do_bgcolor), colorsel);
			g_signal_connect(G_OBJECT(cancel_button), "clicked",
							 G_CALLBACK(cancel_toolbar_bgcolor), toolbar);

		}
		gtk_window_present(GTK_WINDOW(toolbar->bgcolor_dialog));
	} else {
		cancel_toolbar_bgcolor(color, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
clear_formatting_cb(GtkWidget *clear, GtkIMHtmlToolbar *toolbar)
{
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->clear), FALSE, toolbar);
	gtk_imhtml_clear_formatting(GTK_IMHTML(toolbar->imhtml));
}

static void
cancel_link_cb(GtkIMHtmlToolbar *toolbar, PurpleRequestFields *fields)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->link), FALSE);

	toolbar->link_dialog = NULL;
}

static void
close_link_dialog(GtkIMHtmlToolbar *toolbar)
{
	if (toolbar->link_dialog != NULL)
	{
		purple_request_close(PURPLE_REQUEST_FIELDS, toolbar->link_dialog);
		toolbar->link_dialog = NULL;
	}
}

static void
do_insert_link_cb(GtkIMHtmlToolbar *toolbar, PurpleRequestFields *fields)
{
	const char *url, *description;

	url         = purple_request_fields_get_string(fields, "url");
	if (GTK_IMHTML(toolbar->imhtml)->format_functions & GTK_IMHTML_LINKDESC)
		description = purple_request_fields_get_string(fields, "description");
	else
		description = NULL;

	if (description == NULL)
		description = url;

	gtk_imhtml_insert_link(GTK_IMHTML(toolbar->imhtml),
	                       gtk_text_buffer_get_insert(GTK_IMHTML(toolbar->imhtml)->text_buffer),
	                       url, description);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->link), FALSE);

	toolbar->link_dialog = NULL;
}

static void
insert_link_cb(GtkWidget *w, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->link))) {
		PurpleRequestFields *fields;
		PurpleRequestFieldGroup *group;
		PurpleRequestField *field;
		GtkTextIter start, end;
		char *msg;
		char *desc = NULL;

		fields = purple_request_fields_new();

		group = purple_request_field_group_new(NULL);
		purple_request_fields_add_group(fields, group);

		field = purple_request_field_string_new("url", _("_URL"), NULL, FALSE);
		purple_request_field_set_required(field, TRUE);
		purple_request_field_group_add_field(group, field);

		if(GTK_IMHTML(toolbar->imhtml)->format_functions & GTK_IMHTML_LINKDESC) {
			if (gtk_text_buffer_get_selection_bounds(GTK_IMHTML(toolbar->imhtml)->text_buffer, &start, &end)) {
				desc = gtk_imhtml_get_text(GTK_IMHTML(toolbar->imhtml), &start, &end);
			}
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

		toolbar->link_dialog =
			purple_request_fields(toolbar, _("Insert Link"),
					    NULL,
						msg,
					    fields,
					    _("_Insert"), G_CALLBACK(do_insert_link_cb),
					    _("Cancel"), G_CALLBACK(cancel_link_cb),
						NULL, NULL, NULL,
					    toolbar);
		g_free(msg);
		g_free(desc);
	} else {
		close_link_dialog(toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void insert_hr_cb(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	GtkTextIter iter;
	GtkTextMark *ins;
	GtkIMHtmlScalable *hr;

	ins = gtk_text_buffer_get_insert(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)));
	gtk_text_buffer_get_iter_at_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)), &iter, ins);
	hr = gtk_imhtml_hr_new();
	gtk_imhtml_hr_add_to(hr, GTK_IMHTML(toolbar->imhtml), &iter);
}

static void
do_insert_image_cb(GtkWidget *widget, int response, GtkIMHtmlToolbar *toolbar)
{
	gchar *filename, *name, *buf;
	char *filedata;
	size_t size;
	GError *error = NULL;
	int id;
	GtkTextIter iter;
	GtkTextMark *ins;

	if (response != GTK_RESPONSE_ACCEPT)
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

	if (filename == NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

	/* The following triggers a callback that closes the widget */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);

	if (!g_file_get_contents(filename, &filedata, &size, &error)) {
		purple_notify_error(NULL, NULL, error->message, NULL);

		g_error_free(error);
		g_free(filename);

		return;
	}

	name = strrchr(filename, G_DIR_SEPARATOR) + 1;

	id = purple_imgstore_add_with_id(filedata, size, name);

	if (id == 0) {
		buf = g_strdup_printf(_("Failed to store image: %s\n"), filename);
		purple_notify_error(NULL, NULL, buf, NULL);

		g_free(buf);
		g_free(filename);

		return;
	}

	g_free(filename);

	ins = gtk_text_buffer_get_insert(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)));
	gtk_text_buffer_get_iter_at_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)),
									 &iter, ins);
	gtk_imhtml_insert_image_at_iter(GTK_IMHTML(toolbar->imhtml), id, &iter);
	purple_imgstore_unref_by_id(id);
}


static void
insert_image_cb(GtkWidget *save, GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *window;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->image))) {
		window = gtk_file_chooser_dialog_new(_("Insert Image"),
						NULL,
						GTK_FILE_CHOOSER_ACTION_OPEN,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_ACCEPT);
		g_signal_connect(G_OBJECT(GTK_FILE_CHOOSER(window)),
				"response", G_CALLBACK(do_insert_image_cb), toolbar);

		gtk_widget_show(window);
		toolbar->image_dialog = window;
	} else {
		gtk_widget_destroy(toolbar->image_dialog);
		toolbar->image_dialog = NULL;
	}

	gtk_widget_grab_focus(toolbar->imhtml);
}


static void
destroy_smiley_dialog(GtkIMHtmlToolbar *toolbar)
{
	if (toolbar->smiley_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->smiley_dialog);
		toolbar->smiley_dialog = NULL;
	}
}

static gboolean
close_smiley_dialog(GtkIMHtmlToolbar *toolbar)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smiley), FALSE);
	return FALSE;
}


static void
insert_smiley_text(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	char *smiley_text, *escaped_smiley;

	smiley_text = g_object_get_data(G_OBJECT(widget), "smiley_text");
	escaped_smiley = g_markup_escape_text(smiley_text, -1);

	gtk_imhtml_insert_smiley(GTK_IMHTML(toolbar->imhtml),
							 GTK_IMHTML(toolbar->imhtml)->protocol_name,
							 escaped_smiley);

	g_free(escaped_smiley);

	close_smiley_dialog(toolbar);
}

/* smiley buttons list */
struct smiley_button_list {
	int width, height;
	GtkWidget *button;
	const GtkIMHtmlSmiley *smiley;
	struct smiley_button_list *next;
};

static struct smiley_button_list *
sort_smileys(struct smiley_button_list *ls, GtkIMHtmlToolbar *toolbar,
			 int *width, const GtkIMHtmlSmiley *smiley)
{
	GtkWidget *image;
	GtkWidget *button;
	GtkRequisition size;
	struct smiley_button_list *cur;
	struct smiley_button_list *it, *it_last;
	const gchar *filename = smiley->file;
	gchar *face = smiley->smile;
	PurpleSmiley *psmiley = NULL;
	gboolean supports_custom = (gtk_imhtml_get_format_functions(GTK_IMHTML(toolbar->imhtml)) & GTK_IMHTML_CUSTOM_SMILEY);

	cur = g_new0(struct smiley_button_list, 1);
	it = ls;
	it_last = ls; /* list iterators*/
	image = gtk_image_new_from_file(filename);

	gtk_widget_size_request(image, &size);

	if (size.width > 24 &&
			smiley->flags & GTK_IMHTML_SMILEY_CUSTOM) { /* This is a custom smiley, let's scale it */
		GdkPixbuf *pixbuf = NULL;
		GtkImageType type;

		type = gtk_image_get_storage_type(GTK_IMAGE(image));

		if (type == GTK_IMAGE_PIXBUF) {
			pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
		} else if (type == GTK_IMAGE_ANIMATION) {
			GdkPixbufAnimation *animation;

			animation = gtk_image_get_animation(GTK_IMAGE(image));

			pixbuf = gdk_pixbuf_animation_get_static_image(animation);
		}

		if (pixbuf != NULL) {
			GdkPixbuf *resized;
			resized = gdk_pixbuf_scale_simple(pixbuf, 24, 24,
					GDK_INTERP_HYPER);

			gtk_image_set_from_pixbuf(GTK_IMAGE(image), resized); /* This unrefs pixbuf */
			gtk_widget_size_request(image, &size);
			g_object_unref(G_OBJECT(resized));
		}
	}

	(*width) += size.width;

	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), image);

	g_object_set_data(G_OBJECT(button), "smiley_text", face);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(insert_smiley_text), toolbar);

#if GTK_CHECK_VERSION(2,12,0)
	gtk_widget_set_tooltip_text(button, face);
#else
	gtk_tooltips_set_tip(toolbar->tooltips, button, face, NULL);
#endif

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	psmiley = purple_smileys_find_by_shortcut(smiley->smile);
	/* If this is a "non-custom" smiley, check to see if its shortcut is
	  "shadowed" by any custom smiley. This can only happen if the connection
	  is custom smiley-enabled */
	if (supports_custom && psmiley && !(smiley->flags & GTK_IMHTML_SMILEY_CUSTOM)) {
		gchar tip[128];
		g_snprintf(tip, sizeof(tip),
			_("This smiley is disabled because a custom smiley exists for this shortcut:\n %s"),
			face);
#if GTK_CHECK_VERSION(2,12,0)
		gtk_widget_set_tooltip_text(button, tip);
#else
		gtk_tooltips_set_tip(toolbar->tooltips, button, tip, NULL);
#endif
		gtk_widget_set_sensitive(button, FALSE);
	} else if (psmiley) {
		/* Remove the button if the smiley is destroyed */
		g_signal_connect_object(G_OBJECT(psmiley), "destroy", G_CALLBACK(gtk_widget_destroy),
				button, G_CONNECT_SWAPPED);
	}

	/* set current element to add */
	cur->height = size.height;
	cur->width = size.width;
	cur->button = button;
	cur->smiley = smiley;
	cur->next = ls;

	/* check where to insert by height */
	if (ls == NULL)
		return cur;
	while (it != NULL) {
		it_last = it;
		it = it->next;
	}
	cur->next = it;
	it_last->next = cur;
	return ls;
}

static gboolean
smiley_is_unique(GSList *list, GtkIMHtmlSmiley *smiley)
{
	while (list) {
		GtkIMHtmlSmiley *cur = (GtkIMHtmlSmiley *) list->data;
		if (!strcmp(cur->file, smiley->file))
			return FALSE;
		list = list->next;
	}
	return TRUE;
}

static gboolean
smiley_dialog_input_cb(GtkWidget *dialog, GdkEvent *event, GtkIMHtmlToolbar *toolbar)
{
	if ((event->type == GDK_KEY_PRESS && event->key.keyval == GDK_KEY_Escape) ||
	    (event->type == GDK_BUTTON_PRESS && event->button.button == 1))
	{
		close_smiley_dialog(toolbar);
		return TRUE;
	}

	return FALSE;
}

static void
add_smiley_list(GtkWidget *container, struct smiley_button_list *list,
		int max_width, gboolean custom)
{
	GtkWidget *line;
	int line_width = 0;

	if (!list)
		return;

	line = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);
	for (; list; list = list->next) {
		if (custom != !!(list->smiley->flags & GTK_IMHTML_SMILEY_CUSTOM))
			continue;
		gtk_box_pack_start(GTK_BOX(line), list->button, FALSE, FALSE, 0);
		gtk_widget_show(list->button);
		line_width += list->width;
		if (line_width >= max_width) {
			if (list->next) {
				line = gtk_hbox_new(FALSE, 0);
				gtk_box_pack_start(GTK_BOX(container), line, FALSE, FALSE, 0);
			}
			line_width = 0;
		}
	}
}

static void
insert_smiley_cb(GtkWidget *smiley, GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *dialog, *vbox;
	GtkWidget *smiley_table = NULL;
	GSList *smileys, *unique_smileys = NULL;
	const GSList *custom_smileys = NULL;
	gboolean supports_custom = FALSE;
	GtkRequisition req;
	GtkWidget *scrolled, *viewport;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smiley))) {
		destroy_smiley_dialog(toolbar);
		gtk_widget_grab_focus(toolbar->imhtml);
		return;
	}

	if (toolbar->sml)
		smileys = pidgin_themes_get_proto_smileys(toolbar->sml);
	else
		smileys = pidgin_themes_get_proto_smileys(NULL);

	/* Note: prepend smileys to list to avoid O(n^2) overhead when there is
	  a large number of smileys... need to revers the list after for the dialog
	  work... */
	while(smileys) {
		GtkIMHtmlSmiley *smiley = (GtkIMHtmlSmiley *) smileys->data;
		if(!smiley->hidden) {
			if(smiley_is_unique(unique_smileys, smiley)) {
				unique_smileys = g_slist_prepend(unique_smileys, smiley);
			}
		}
		smileys = smileys->next;
	}
	supports_custom = (gtk_imhtml_get_format_functions(GTK_IMHTML(toolbar->imhtml)) & GTK_IMHTML_CUSTOM_SMILEY);
	if (toolbar->imhtml && supports_custom) {
		const GSList *iterator = NULL;
		custom_smileys = pidgin_smileys_get_all();

		for (iterator = custom_smileys ; iterator ;
			 iterator = g_slist_next(iterator)) {
			GtkIMHtmlSmiley *smiley = (GtkIMHtmlSmiley *) iterator->data;
			unique_smileys = g_slist_prepend(unique_smileys, smiley);
		}
	}

	/* we need to reverse the list to get the smileys in the correct order */
	unique_smileys = g_slist_reverse(unique_smileys);

	dialog = pidgin_create_dialog(_("Smile!"), 0, "smiley_dialog", FALSE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(dialog), FALSE, 0);

	if (unique_smileys != NULL) {
		struct smiley_button_list *ls;
		int max_line_width, num_lines, button_width = 0;

		/* We use hboxes packed in a vbox */
		ls = NULL;
		max_line_width = 0;
		num_lines = floor(sqrt(g_slist_length(unique_smileys)));
		smiley_table = gtk_vbox_new(FALSE, 0);

		if (supports_custom) {
			GtkWidget *manage = gtk_button_new_with_mnemonic(_("_Manage custom smileys"));
			GtkRequisition req;
			g_signal_connect(G_OBJECT(manage), "clicked",
					G_CALLBACK(pidgin_smiley_manager_show), NULL);
			g_signal_connect_swapped(G_OBJECT(manage), "clicked",
					G_CALLBACK(gtk_widget_destroy), dialog);
			gtk_box_pack_end(GTK_BOX(vbox), manage, FALSE, TRUE, 0);
			gtk_widget_size_request(manage, &req);
			button_width = req.width;
		}

		/* create list of smileys sorted by height */
		while (unique_smileys) {
			GtkIMHtmlSmiley *smiley = (GtkIMHtmlSmiley *) unique_smileys->data;
			if (!smiley->hidden) {
				ls = sort_smileys(ls, toolbar, &max_line_width, smiley);
			}
			unique_smileys = g_slist_delete_link(unique_smileys, unique_smileys);
		}
		/* The window will be at least as wide as the 'Manage ..' button */
		max_line_width = MAX(button_width, max_line_width / num_lines);

		/* pack buttons of the list */
		add_smiley_list(smiley_table, ls, max_line_width, FALSE);
		if (supports_custom) {
			gtk_box_pack_start(GTK_BOX(smiley_table), gtk_hseparator_new(), TRUE, FALSE, 0);
			add_smiley_list(smiley_table, ls, max_line_width, TRUE);
		}
		while (ls) {
			struct smiley_button_list *tmp = ls->next;
			g_free(ls);
			ls = tmp;
		}

		gtk_widget_add_events(dialog, GDK_KEY_PRESS_MASK);
	}
	else {
		smiley_table = gtk_label_new(_("This theme has no available smileys."));
		gtk_widget_add_events(dialog, GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);
		g_signal_connect(G_OBJECT(dialog), "button-press-event", (GCallback)smiley_dialog_input_cb, toolbar);
	}


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

	gtk_widget_size_request(viewport, &req);
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

	toolbar->smiley_dialog = dialog;

	gtk_widget_grab_focus(toolbar->imhtml);
}

static void send_attention_cb(GtkWidget *attention, GtkIMHtmlToolbar *toolbar)
{
	PurpleConversation *conv =
		g_object_get_data(G_OBJECT(toolbar), "active_conv");
	const gchar *who = purple_conversation_get_name(conv);
	PurpleConnection *gc = purple_conversation_get_connection(conv);

	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(attention), FALSE, toolbar);
	purple_prpl_send_attention(gc, who, 0);
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void update_buttons_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, GtkIMHtmlToolbar *toolbar)
{
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->bold), buttons & GTK_IMHTML_BOLD);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->italic), buttons & GTK_IMHTML_ITALIC);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->underline), buttons & GTK_IMHTML_UNDERLINE);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->strikethrough), buttons & GTK_IMHTML_STRIKE);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->larger_size), buttons & GTK_IMHTML_GROW);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->smaller_size), buttons & GTK_IMHTML_SHRINK);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->font), buttons & GTK_IMHTML_FACE);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->fgcolor), buttons & GTK_IMHTML_FORECOLOR);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->bgcolor), buttons & GTK_IMHTML_BACKCOLOR);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->clear),
							 (buttons & GTK_IMHTML_BOLD ||
							  buttons & GTK_IMHTML_ITALIC ||
							  buttons & GTK_IMHTML_UNDERLINE ||
							  buttons & GTK_IMHTML_STRIKE ||
							  buttons & GTK_IMHTML_GROW ||
							  buttons & GTK_IMHTML_SHRINK ||
							  buttons & GTK_IMHTML_FACE ||
							  buttons & GTK_IMHTML_FORECOLOR ||
							  buttons & GTK_IMHTML_BACKCOLOR));

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->image), buttons & GTK_IMHTML_IMAGE);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->link), buttons & GTK_IMHTML_LINK);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->smiley), buttons & GTK_IMHTML_SMILEY);
}

/* we call this when we want to _set_active the toggle button, it'll
 * block the callback thats connected to the button so we don't have to
 * do the double toggling hack
 */
static void toggle_button_set_active_block(GtkToggleButton *button,
										   gboolean is_active,
										   GtkIMHtmlToolbar *toolbar)
{
	GObject *object;
	g_return_if_fail(toolbar);

	object = g_object_ref(button);
	g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA,
									0, 0, NULL, NULL, toolbar);
	gtk_toggle_button_set_active(button, is_active);
	g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA,
									  0, 0, NULL, NULL, toolbar);
	g_object_unref(object);
}

static void update_buttons(GtkIMHtmlToolbar *toolbar)
{
	gboolean bold, italic, underline, strike;
	char *tmp;
	char *tmp2;
	GtkLabel *label = g_object_get_data(G_OBJECT(toolbar), "font_label");

	gtk_label_set_label(label, _("_Font"));

	gtk_imhtml_get_current_format(GTK_IMHTML(toolbar->imhtml),
								  &bold, &italic, &underline);
	strike = GTK_IMHTML(toolbar->imhtml)->edit.strike;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->bold)) != bold)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->bold), bold,
									   toolbar);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->italic)) != italic)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->italic), italic,
									   toolbar);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->underline)) != underline)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->underline),
									   underline, toolbar);
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->strikethrough)) != strike)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->strikethrough),
									   strike, toolbar);

	/* These buttons aren't ever "active". */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smaller_size), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->larger_size), FALSE);

	if (bold) {
		gchar *markup = g_strdup_printf("<b>%s</b>",
				gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	if (italic) {
		gchar *markup = g_strdup_printf("<i>%s</i>",
				gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	if (underline) {
		gchar *markup = g_strdup_printf("<u>%s</u>",
				gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	if (strike) {
		gchar *markup = g_strdup_printf("<s>%s</s>",
				gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}

	tmp = gtk_imhtml_get_current_fontface(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->font),
								   (tmp != NULL), toolbar);
	if (tmp != NULL) {
		gchar *markup = g_strdup_printf("<span font_desc=\"%s\">%s</span>",
				tmp, gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	g_free(tmp);

	tmp = gtk_imhtml_get_current_forecolor(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->fgcolor),
								   (tmp != NULL), toolbar);
	if (tmp != NULL) {
		gchar *markup = g_strdup_printf("<span foreground=\"%s\">%s</span>",
				tmp, gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	g_free(tmp);

	tmp = gtk_imhtml_get_current_backcolor(GTK_IMHTML(toolbar->imhtml));
	tmp2 = gtk_imhtml_get_current_background(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->bgcolor),
								   (tmp != NULL || tmp2 != NULL), toolbar);
	if (tmp != NULL) {
		gchar *markup = g_strdup_printf("<span background=\"%s\">%s</span>",
				tmp, gtk_label_get_label(label));
		gtk_label_set_markup_with_mnemonic(label, markup);
		g_free(markup);
	}
	g_free(tmp);
	g_free(tmp2);
}

static void toggle_button_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, GtkIMHtmlToolbar *toolbar)
{
	update_buttons(toolbar);
}

static void update_format_cb(GtkIMHtml *imhtml, GtkIMHtmlToolbar *toolbar) {
	update_buttons(toolbar);
}

static void mark_set_cb(GtkTextBuffer *buffer, GtkTextIter *location,
						GtkTextMark *mark, GtkIMHtmlToolbar *toolbar)
{
	if(mark != gtk_text_buffer_get_insert(buffer))
		return;

	update_buttons(toolbar);
}


/* This comes from gtkmenutoolbutton.c from gtk+
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 */
static void
menu_position_func (GtkMenu           *menu,
                    int               *x,
                    int               *y,
                    gboolean          *push_in,
                    gpointer          data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkRequisition menu_req;
	GtkAllocation allocation;
	gint ythickness = gtk_widget_get_style(widget)->ythickness;
	int savy;

	gtk_widget_get_allocation(widget, &allocation);
	gtk_widget_size_request(GTK_WIDGET (menu), &menu_req);
	gdk_window_get_origin(gtk_widget_get_window(widget), x, y);
	*x += allocation.x;
	*y += allocation.y + allocation.height;
	savy = *y;

	pidgin_menu_position_func_helper(menu, x, y, push_in, data);

	if (savy > *y + ythickness + 1)
		*y -= allocation.height;
}

static gboolean
button_activate_on_click(GtkWidget *button, GdkEventButton *event, GtkIMHtmlToolbar *toolbar)
{
	if (event->button == 1 && GTK_IS_TOGGLE_BUTTON(button))
		gtk_widget_activate(button);
	else if (event->button == 3)
		return gtk_imhtmltoolbar_popup_menu(button, event, toolbar);
	return FALSE;
}

static void pidgin_menu_clicked(GtkWidget *button, GtkMenu *menu)
{
	gtk_widget_show_all(GTK_WIDGET(menu));
	gtk_menu_popup(menu, NULL, NULL, menu_position_func, button, 0, gtk_get_current_event_time());
}

static void pidgin_menu_deactivate(GtkWidget *menu, GtkToggleButton *button)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
}

enum {
	LAST_SIGNAL
};
/* static guint signals [LAST_SIGNAL] = { 0 }; */

static void
gtk_imhtmltoolbar_finalize (GObject *object)
{
	GtkIMHtmlToolbar *toolbar = GTK_IMHTMLTOOLBAR(object);
	GtkWidget *menu;

	if (toolbar->image_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->image_dialog);
		toolbar->image_dialog = NULL;
	}

	destroy_toolbar_font(NULL, NULL, toolbar);
	if (toolbar->smiley_dialog != NULL) {
		g_signal_handlers_disconnect_by_func(G_OBJECT(toolbar->smiley_dialog), close_smiley_dialog, toolbar);
		destroy_smiley_dialog(toolbar);
	}
	destroy_toolbar_bgcolor(NULL, NULL, toolbar);
	destroy_toolbar_fgcolor(NULL, NULL, toolbar);
	close_link_dialog(toolbar);
	if (toolbar->imhtml) {
		g_signal_handlers_disconnect_matched(toolbar->imhtml,
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
				toolbar);
		g_signal_handlers_disconnect_matched(GTK_IMHTML(toolbar->imhtml)->text_buffer,
				G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
				toolbar);
	}

	g_free(toolbar->sml);
#if !GTK_CHECK_VERSION(2,12,0)
	gtk_object_sink(GTK_OBJECT(toolbar->tooltips));
#endif

	menu = g_object_get_data(object, "font_menu");
	if (menu)
		gtk_widget_destroy(menu);
	menu = g_object_get_data(object, "insert_menu");
	if (menu)
		gtk_widget_destroy(menu);

	purple_prefs_disconnect_by_handle(object);

	G_OBJECT_CLASS(parent_class)->finalize (object);
}

static void
switch_toolbar_view(GtkWidget *item, GtkIMHtmlToolbar *toolbar)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
			!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide"));
}

static gboolean
gtk_imhtmltoolbar_popup_menu(GtkWidget *widget, GdkEventButton *event, GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *menu;
	GtkWidget *item;
	gboolean wide;

	if (event->button != 3)
		return FALSE;

	wide = gtk_widget_get_visible(toolbar->bold);

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic(wide ? _("Group Items") : _("Ungroup Items"));
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(switch_toolbar_view), toolbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, pidgin_menu_position_func_helper,
				widget, event->button, event->time);
	return TRUE;
}

/* Boring GTK+ stuff */
static void gtk_imhtmltoolbar_class_init (GtkIMHtmlToolbarClass *class)
{
	GObjectClass   *gobject_class;
	gobject_class = (GObjectClass*) class;
	parent_class = g_type_class_ref(GTK_TYPE_HBOX);
	gobject_class->finalize = gtk_imhtmltoolbar_finalize;

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations/toolbar");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/conversations/toolbar/wide", FALSE);
}

static void gtk_imhtmltoolbar_create_old_buttons(GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *hbox;
	GtkWidget *button;
	struct {
		char *stock;
		gpointer callback;
		GtkWidget **button;
		const char *tooltip;
	} buttons[] = {
		{GTK_STOCK_BOLD, G_CALLBACK(do_bold), &toolbar->bold, _("Bold")},
		{GTK_STOCK_ITALIC, do_italic, &toolbar->italic, _("Italic")},
		{GTK_STOCK_UNDERLINE, do_underline, &toolbar->underline, _("Underline")},
		{GTK_STOCK_STRIKETHROUGH, do_strikethrough, &toolbar->strikethrough, _("Strikethrough")},
		{"", NULL, NULL, NULL},
		{PIDGIN_STOCK_TOOLBAR_TEXT_LARGER, do_big, &toolbar->larger_size, _("Increase Font Size")},
		{PIDGIN_STOCK_TOOLBAR_TEXT_SMALLER, do_small, &toolbar->smaller_size, _("Decrease Font Size")},
		{"", NULL, NULL, NULL},
		{PIDGIN_STOCK_TOOLBAR_FONT_FACE, toggle_font, &toolbar->font, _("Font Face")},
		{PIDGIN_STOCK_TOOLBAR_FGCOLOR, toggle_fg_color, &toolbar->fgcolor, _("Foreground Color")},
		{PIDGIN_STOCK_TOOLBAR_BGCOLOR, toggle_bg_color, &toolbar->bgcolor, _("Background Color")},
		{"", NULL, NULL, NULL},
		{PIDGIN_STOCK_CLEAR, clear_formatting_cb, &toolbar->clear, _("Reset Formatting")},
		{"", NULL, NULL, NULL},
		{PIDGIN_STOCK_TOOLBAR_INSERT_IMAGE, insert_image_cb, &toolbar->image, _("Insert IM Image")},
		{PIDGIN_STOCK_TOOLBAR_INSERT_LINK, insert_link_cb, &toolbar->link, _("Insert Link")},
		{"", NULL, NULL, NULL},
		{PIDGIN_STOCK_TOOLBAR_SMILEY, insert_smiley_cb, &toolbar->smiley, _("Insert Smiley")},
		{PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION, send_attention_cb, &toolbar->attention, _("Send Attention")},
		{NULL, NULL, NULL, NULL}
	};
	int iter;

	hbox = gtk_hbox_new(FALSE, 0);

	for (iter = 0; buttons[iter].stock; iter++) {
		if (buttons[iter].stock[0]) {
			button = pidgin_pixbuf_toolbar_button_from_stock(buttons[iter].stock);
			g_signal_connect(G_OBJECT(button), "button-press-event", G_CALLBACK(gtk_imhtmltoolbar_popup_menu), toolbar);
			g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(buttons[iter].callback), toolbar);
			*(buttons[iter].button) = button;
#if GTK_CHECK_VERSION(2,12,0)
			gtk_widget_set_tooltip_text(button, buttons[iter].tooltip);
#else
			gtk_tooltips_set_tip(toolbar->tooltips, button, buttons[iter].tooltip, NULL);
#endif
		} else
			button = gtk_vseparator_new();
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	}

	gtk_box_pack_start(GTK_BOX(toolbar), hbox, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(toolbar), "wide-view", hbox);
}

static void
button_visibility_changed(GtkWidget *button, gpointer dontcare, GtkWidget *item)
{
	if (gtk_widget_get_visible(button))
		gtk_widget_hide(item);
	else
		gtk_widget_show(item);
}

static void
button_sensitiveness_changed(GtkWidget *button, gpointer dontcare, GtkWidget *item)
{
	gtk_widget_set_sensitive(item, gtk_widget_get_sensitive(button));
}

static void
update_menuitem(GtkToggleButton *button, GtkCheckMenuItem *item)
{
	g_signal_handlers_block_by_func(G_OBJECT(item), G_CALLBACK(gtk_button_clicked), button);
	gtk_check_menu_item_set_active(item, gtk_toggle_button_get_active(button));
	g_signal_handlers_unblock_by_func(G_OBJECT(item), G_CALLBACK(gtk_button_clicked), button);
}

static void
enable_markup(GtkWidget *widget, gpointer null)
{
	if (GTK_IS_LABEL(widget))
		g_object_set(G_OBJECT(widget), "use-markup", TRUE, NULL);
}

static void
imhtmltoolbar_view_pref_changed(const char *name, PurplePrefType type,
		gconstpointer value, gpointer toolbar)
{
	if (value) {
		gtk_widget_hide(g_object_get_data(G_OBJECT(toolbar), "lean-view"));
		gtk_widget_show_all(g_object_get_data(G_OBJECT(toolbar), "wide-view"));
	} else {
		gtk_widget_hide(g_object_get_data(G_OBJECT(toolbar), "wide-view"));
		gtk_widget_show_all(g_object_get_data(G_OBJECT(toolbar), "lean-view"));
	}
}

static void gtk_imhtmltoolbar_init (GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *hbox = GTK_WIDGET(toolbar), *event = gtk_event_box_new();
	GtkWidget *bbox, *box = gtk_hbox_new(FALSE, 0);
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *insert_button;
	GtkWidget *font_button;
	GtkWidget *smiley_button;
	GtkWidget *attention_button;
	GtkWidget *font_menu;
	GtkWidget *insert_menu;
	GtkWidget *menuitem;
	GtkWidget *sep;
	int i;
	struct {
		const char *label;
		GtkWidget **button;
		gboolean check;
	} buttons[] = {
		{_("<b>_Bold</b>"), &toolbar->bold, TRUE},
		{_("<i>_Italic</i>"), &toolbar->italic, TRUE},
		{_("<u>_Underline</u>"), &toolbar->underline, TRUE},
		{_("<span strikethrough='true'>Strikethrough</span>"), &toolbar->strikethrough, TRUE},
		{_("<span size='larger'>_Larger</span>"), &toolbar->larger_size, TRUE},
#if 0
		{_("_Normal"), &toolbar->normal_size, TRUE},
#endif
		{_("<span size='smaller'>_Smaller</span>"), &toolbar->smaller_size, TRUE},
		/* If we want to show the formatting for the following items, we would
		 * need to update them when formatting changes. The above items don't need
		 * no updating nor nothin' */
		{_("_Font face"), &toolbar->font, TRUE},
		{_("Foreground _color"), &toolbar->fgcolor, TRUE},
		{_("Bac_kground color"), &toolbar->bgcolor, TRUE},
		{_("_Reset formatting"), &toolbar->clear, FALSE},
		{NULL, NULL, FALSE}
	};

	toolbar->imhtml = NULL;
	toolbar->font_dialog = NULL;
	toolbar->fgcolor_dialog = NULL;
	toolbar->bgcolor_dialog = NULL;
	toolbar->link_dialog = NULL;
	toolbar->smiley_dialog = NULL;
	toolbar->image_dialog = NULL;

#if !GTK_CHECK_VERSION(2,12,0)
	toolbar->tooltips = gtk_tooltips_new();
#endif

	gtk_box_set_spacing(GTK_BOX(toolbar), 3);

	gtk_imhtmltoolbar_create_old_buttons(toolbar);

	/* Fonts */
	font_button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(font_button), GTK_RELIEF_NONE);
	bbox = gtk_hbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(font_button), bbox);
	image = gtk_image_new_from_stock(GTK_STOCK_BOLD, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Font"));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	g_object_set_data(G_OBJECT(hbox), "font_label", label);
	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), font_button, FALSE, FALSE, 0);
	gtk_widget_show_all(font_button);

	font_menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(toolbar), "font_menu", font_menu);

	for (i = 0; buttons[i].label; i++) {
		GtkWidget *old = *buttons[i].button;
		if (buttons[i].check) {
			menuitem = gtk_check_menu_item_new_with_mnemonic(buttons[i].label);
			g_signal_connect_after(G_OBJECT(old), "toggled",
						G_CALLBACK(update_menuitem), menuitem);
		} else {
			menuitem = gtk_menu_item_new_with_mnemonic(buttons[i].label);
		}
		g_signal_connect_swapped(G_OBJECT(menuitem), "activate",
				G_CALLBACK(gtk_button_clicked), old);
		gtk_menu_shell_append(GTK_MENU_SHELL(font_menu), menuitem);
		g_signal_connect(G_OBJECT(old), "notify::sensitive",
				G_CALLBACK(button_sensitiveness_changed), menuitem);
		g_signal_connect(G_OBJECT(old), "notify::visible",
				G_CALLBACK(button_visibility_changed), menuitem);
		gtk_container_foreach(GTK_CONTAINER(menuitem), (GtkCallback)enable_markup, NULL);
	}

	g_signal_connect(G_OBJECT(font_button), "button-press-event", G_CALLBACK(button_activate_on_click), toolbar);
	g_signal_connect(G_OBJECT(font_button), "activate", G_CALLBACK(pidgin_menu_clicked), font_menu);
	g_signal_connect_data(G_OBJECT(font_menu), "deactivate", G_CALLBACK(pidgin_menu_deactivate),
	                      g_object_ref(font_button), (GClosureNotify)g_object_unref, 0);

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
	gtk_widget_show_all(sep);

	/* Insert */
	insert_button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(insert_button), GTK_RELIEF_NONE);
	bbox = gtk_hbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(insert_button), bbox);
	image = gtk_image_new_from_stock(PIDGIN_STOCK_TOOLBAR_INSERT, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Insert"));
	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), insert_button, FALSE, FALSE, 0);
	gtk_widget_show_all(insert_button);

	insert_menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(toolbar), "insert_menu", insert_menu);

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Image"));
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_button_clicked), toolbar->image);
	gtk_menu_shell_append(GTK_MENU_SHELL(insert_menu), menuitem);
	g_signal_connect(G_OBJECT(toolbar->image), "notify::sensitive",
			G_CALLBACK(button_sensitiveness_changed), menuitem);
	g_signal_connect(G_OBJECT(toolbar->image), "notify::visible",
			G_CALLBACK(button_visibility_changed), menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Link"));
	g_signal_connect_swapped(G_OBJECT(menuitem), "activate", G_CALLBACK(gtk_button_clicked), toolbar->link);
	gtk_menu_shell_append(GTK_MENU_SHELL(insert_menu), menuitem);
	g_signal_connect(G_OBJECT(toolbar->link), "notify::sensitive",
			G_CALLBACK(button_sensitiveness_changed), menuitem);
	g_signal_connect(G_OBJECT(toolbar->link), "notify::visible",
			G_CALLBACK(button_visibility_changed), menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Horizontal rule"));
	g_signal_connect(G_OBJECT(menuitem), "activate"	, G_CALLBACK(insert_hr_cb), toolbar);
	gtk_menu_shell_append(GTK_MENU_SHELL(insert_menu), menuitem);
	toolbar->insert_hr = menuitem;

	g_signal_connect(G_OBJECT(insert_button), "button-press-event", G_CALLBACK(button_activate_on_click), toolbar);
	g_signal_connect(G_OBJECT(insert_button), "activate", G_CALLBACK(pidgin_menu_clicked), insert_menu);
	g_signal_connect_data(G_OBJECT(insert_menu), "deactivate", G_CALLBACK(pidgin_menu_deactivate),
	                      g_object_ref(insert_button), (GClosureNotify)g_object_unref, 0);
	toolbar->sml = NULL;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
	gtk_widget_show_all(sep);

	/* Smiley */
	smiley_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(smiley_button), GTK_RELIEF_NONE);
	bbox = gtk_hbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(smiley_button), bbox);
	image = gtk_image_new_from_stock(PIDGIN_STOCK_TOOLBAR_SMILEY, gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Smile!"));
	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), smiley_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(smiley_button), "button-press-event", G_CALLBACK(gtk_imhtmltoolbar_popup_menu), toolbar);
	g_signal_connect_swapped(G_OBJECT(smiley_button), "clicked", G_CALLBACK(gtk_button_clicked), toolbar->smiley);
	gtk_widget_show_all(smiley_button);

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
	gtk_widget_show_all(sep);

	/* Attention */
	attention_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(attention_button), GTK_RELIEF_NONE);
	bbox = gtk_hbox_new(FALSE, 3);
	gtk_container_add(GTK_CONTAINER(attention_button), bbox);
	image = gtk_image_new_from_stock(PIDGIN_STOCK_TOOLBAR_SEND_ATTENTION,
		gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Attention!"));
	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), attention_button, FALSE, FALSE, 0);
	g_signal_connect_swapped(G_OBJECT(attention_button), "clicked",
		G_CALLBACK(gtk_button_clicked), toolbar->attention);
	gtk_widget_show_all(attention_button);

	g_signal_connect(G_OBJECT(toolbar->attention), "notify::sensitive",
			G_CALLBACK(button_sensitiveness_changed), attention_button);
	g_signal_connect(G_OBJECT(toolbar->attention), "notify::visible",
			G_CALLBACK(button_visibility_changed), attention_button);

	/* set attention button to be greyed out until we get a conversation */
	gtk_widget_set_sensitive(toolbar->attention, FALSE);

	gtk_box_pack_start(GTK_BOX(hbox), box, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(hbox), "lean-view", box);
	gtk_widget_show(box);

	purple_prefs_connect_callback(toolbar, PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
			imhtmltoolbar_view_pref_changed, toolbar);
	g_signal_connect_data(G_OBJECT(toolbar), "realize",
			G_CALLBACK(purple_prefs_trigger_callback), PIDGIN_PREFS_ROOT "/conversations/toolbar/wide",
			NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	gtk_event_box_set_visible_window(GTK_EVENT_BOX(event), FALSE);

	gtk_widget_add_events(event, GDK_BUTTON_PRESS_MASK);
	gtk_box_pack_start(GTK_BOX(hbox), event, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(event), "button-press-event", G_CALLBACK(gtk_imhtmltoolbar_popup_menu), toolbar);
	gtk_widget_show(event);
}

GtkWidget *gtk_imhtmltoolbar_new()
{
	return GTK_WIDGET(g_object_new(gtk_imhtmltoolbar_get_type(), NULL));
}

GType gtk_imhtmltoolbar_get_type()
{
	static GType imhtmltoolbar_type = 0;

	if (!imhtmltoolbar_type) {
		static const GTypeInfo imhtmltoolbar_info = {
			sizeof(GtkIMHtmlToolbarClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_imhtmltoolbar_class_init,
			NULL,
			NULL,
			sizeof (GtkIMHtmlToolbar),
			0,
			(GInstanceInitFunc) gtk_imhtmltoolbar_init,
			NULL
		};

		imhtmltoolbar_type = g_type_register_static(GTK_TYPE_HBOX,
				"GtkIMHtmlToolbar", &imhtmltoolbar_info, 0);
	}

	return imhtmltoolbar_type;
}


void gtk_imhtmltoolbar_attach(GtkIMHtmlToolbar *toolbar, GtkWidget *imhtml)
{
	GtkIMHtmlButtons buttons;

	g_return_if_fail(toolbar != NULL);
	g_return_if_fail(GTK_IS_IMHTMLTOOLBAR(toolbar));
	g_return_if_fail(imhtml != NULL);
	g_return_if_fail(GTK_IS_IMHTML(imhtml));

	toolbar->imhtml = imhtml;
	g_signal_connect(G_OBJECT(imhtml), "format_buttons_update", G_CALLBACK(update_buttons_cb), toolbar);
	g_signal_connect_after(G_OBJECT(imhtml), "format_function_toggle", G_CALLBACK(toggle_button_cb), toolbar);
	g_signal_connect_after(G_OBJECT(imhtml), "format_function_clear", G_CALLBACK(update_format_cb), toolbar);
	g_signal_connect(G_OBJECT(imhtml), "format_function_update", G_CALLBACK(update_format_cb), toolbar);
	g_signal_connect_after(G_OBJECT(GTK_IMHTML(imhtml)->text_buffer), "mark-set", G_CALLBACK(mark_set_cb), toolbar);

	buttons = gtk_imhtml_get_format_functions(GTK_IMHTML(imhtml));
	update_buttons_cb(GTK_IMHTML(imhtml), buttons, toolbar);
	update_buttons(toolbar);
}

void gtk_imhtmltoolbar_associate_smileys(GtkIMHtmlToolbar *toolbar, const char *proto_id)
{
	g_free(toolbar->sml);
	toolbar->sml = g_strdup(proto_id);
}

void gtk_imhtmltoolbar_switch_active_conversation(GtkIMHtmlToolbar *toolbar,
	PurpleConversation *conv)
{
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	PurplePlugin *prpl = purple_connection_get_prpl(gc);

	g_object_set_data(G_OBJECT(toolbar), "active_conv", conv);

	/* gray out attention button on protocols that don't support it
	 for the time being it is always disabled for chats */
	gtk_widget_set_sensitive(toolbar->attention,
		conv && prpl && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM &&
		PURPLE_PLUGIN_PROTOCOL_INFO(prpl)->send_attention != NULL);
}


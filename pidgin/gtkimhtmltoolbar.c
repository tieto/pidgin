/*
 * @file gtkimhtmltoolbar.c GTK+ IMHtml Toolbar
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "gtkgaim.h"

#include "imgstore.h"
#include "notify.h"
#include "request.h"
#include "gaimstock.h"
#include "util.h"

#include "gtkdialogs.h"
#include "gtkimhtmltoolbar.h"
#include "gtkthemes.h"
#include "gtkutils.h"

static GtkHBoxClass *parent_class = NULL;

static void toggle_button_set_active_block(GtkToggleButton *button,
										   gboolean is_active,
										   GtkIMHtmlToolbar *toolbar);


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
do_small(GtkWidget *smalltb, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar != NULL);
	gtk_imhtml_font_shrink(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_big(GtkWidget *large, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_font_grow(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
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
}

static void
realize_toolbar_font(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	GtkFontSelection *sel;

	sel = GTK_FONT_SELECTION(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)->fontsel);
	gtk_widget_hide_all(gtk_widget_get_parent(sel->size_entry));
	gtk_widget_show_all(sel->family_list);
	gtk_widget_show(gtk_widget_get_parent(sel->family_list));
	gtk_widget_show(gtk_widget_get_parent(gtk_widget_get_parent(sel->family_list)));
}

static void
cancel_toolbar_font(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_font(widget, NULL, toolbar);
}

static void apply_font(GtkWidget *widget, GtkFontSelection *fontsel)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	char *fontname;
	char *space;
	GtkIMHtmlToolbar *toolbar =  g_object_get_data(G_OBJECT(fontsel), "gaim_toolbar");

	fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

	space = strrchr(fontname, ' ');
	if(space && isdigit(*(space+1)))
		*space = '\0';

	gtk_imhtml_toggle_fontface(GTK_IMHTML(toolbar->imhtml), fontname);
	g_free(fontname);

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

			g_object_set_data(G_OBJECT(toolbar->font_dialog), "gaim_toolbar", toolbar);

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
			g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)->ok_button), "clicked",
							 G_CALLBACK(apply_font), toolbar->font_dialog);
			g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)->cancel_button), "clicked",
							 G_CALLBACK(cancel_toolbar_font), toolbar);
			g_signal_connect_after(G_OBJECT(toolbar->font_dialog), "realize",
							 G_CALLBACK(realize_toolbar_font), toolbar);
		}
		gtk_window_present(GTK_WINDOW(toolbar->font_dialog));
	} else {
		cancel_toolbar_font(font, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
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
}

static void cancel_toolbar_fgcolor(GtkWidget *widget,
								   GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_fgcolor(widget, NULL, toolbar);
}

static void do_fgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GtkIMHtmlToolbar *toolbar = g_object_get_data(G_OBJECT(colorsel), "gaim_toolbar");
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

			toolbar->fgcolor_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
			colorsel = GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->colorsel;
			if (color) {
				gdk_color_parse(color, &fgcolor);
				gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &fgcolor);
				g_free(color);
			}

			g_object_set_data(G_OBJECT(colorsel), "gaim_toolbar", toolbar);

			g_signal_connect(G_OBJECT(toolbar->fgcolor_dialog), "delete_event",
							 G_CALLBACK(destroy_toolbar_fgcolor), toolbar);
			g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->ok_button), "clicked",
							 G_CALLBACK(do_fgcolor), colorsel);
			g_signal_connect(G_OBJECT (GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->cancel_button), "clicked",
							 G_CALLBACK(cancel_toolbar_fgcolor), toolbar);
		}
		gtk_window_present(GTK_WINDOW(toolbar->fgcolor_dialog));
	} else {
		cancel_toolbar_fgcolor(color, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
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
}

static void
cancel_toolbar_bgcolor(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	destroy_toolbar_bgcolor(widget, NULL, toolbar);
}

static void do_bgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	GtkIMHtmlToolbar *toolbar = g_object_get_data(G_OBJECT(colorsel), "gaim_toolbar");
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

			toolbar->bgcolor_dialog = gtk_color_selection_dialog_new(_("Select Background Color"));
			colorsel = GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->colorsel;
			if (color) {
				gdk_color_parse(color, &bgcolor);
				gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &bgcolor);
				g_free(color);
			}

			g_object_set_data(G_OBJECT(colorsel), "gaim_toolbar", toolbar);

			g_signal_connect(G_OBJECT(toolbar->bgcolor_dialog), "delete_event",
							 G_CALLBACK(destroy_toolbar_bgcolor), toolbar);
			g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->ok_button), "clicked",
							 G_CALLBACK(do_bgcolor), colorsel);
			g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->cancel_button), "clicked",
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
cancel_link_cb(GtkIMHtmlToolbar *toolbar, GaimRequestFields *fields)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->link), FALSE);

	toolbar->link_dialog = NULL;
}

static void
close_link_dialog(GtkIMHtmlToolbar *toolbar)
{
	if (toolbar->link_dialog != NULL)
	{
		gaim_request_close(GAIM_REQUEST_FIELDS, toolbar->link_dialog);
		toolbar->link_dialog = NULL;
	}
}

static void
do_insert_link_cb(GtkIMHtmlToolbar *toolbar, GaimRequestFields *fields)
{
	const char *url, *description;

	url         = gaim_request_fields_get_string(fields, "url");
	if (GTK_IMHTML(toolbar->imhtml)->format_functions & GTK_IMHTML_LINKDESC)
		description = gaim_request_fields_get_string(fields, "description");
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
		GaimRequestFields *fields;
		GaimRequestFieldGroup *group;
		GaimRequestField *field;
		GtkTextIter start, end;
		char *msg;
		char *desc = NULL;

		fields = gaim_request_fields_new();

		group = gaim_request_field_group_new(NULL);
		gaim_request_fields_add_group(fields, group);

		field = gaim_request_field_string_new("url", _("_URL"), NULL, FALSE);
		gaim_request_field_set_required(field, TRUE);
		gaim_request_field_group_add_field(group, field);

		if(GTK_IMHTML(toolbar->imhtml)->format_functions & GTK_IMHTML_LINKDESC) {
			if (gtk_text_buffer_get_selection_bounds(GTK_IMHTML(toolbar->imhtml)->text_buffer, &start, &end)) {
				desc = gtk_imhtml_get_text(GTK_IMHTML(toolbar->imhtml), &start, &end);
			}
			field = gaim_request_field_string_new("description", _("_Description"),
							      desc, FALSE);
			gaim_request_field_group_add_field(group, field);
			msg = g_strdup(_("Please enter the URL and description of the "
							 "link that you want to insert. The description "
							 "is optional."));
		} else {
			msg = g_strdup(_("Please enter the URL of the "
									"link that you want to insert."));
		}

		toolbar->link_dialog =
			gaim_request_fields(toolbar, _("Insert Link"),
					    NULL,
						msg,
					    fields,
					    _("_Insert"), G_CALLBACK(do_insert_link_cb),
					    _("Cancel"), G_CALLBACK(cancel_link_cb),
					    toolbar);
		g_free(msg);
		g_free(desc);
	} else {
		close_link_dialog(toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
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

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	if (response != GTK_RESPONSE_ACCEPT) {
#else /* FILECHOOSER */
	if (response != GTK_RESPONSE_OK) {
#endif /* FILECHOOSER */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
#else /* FILECHOOSER */
	filename = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(widget)));
#endif /* FILECHOOSER */

	if (filename == NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

#if !GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
	if (pidgin_check_if_dir(filename, GTK_FILE_SELECTION(widget))) {
		g_free(filename);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}
#endif /* FILECHOOSER */

	/* The following triggers a callback that closes the widget */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);

	if (!g_file_get_contents(filename, &filedata, &size, &error)) {
		gaim_notify_error(NULL, NULL, error->message, NULL);

		g_error_free(error);
		g_free(filename);

		return;
	}

	name = strrchr(filename, G_DIR_SEPARATOR) + 1;

	id = gaim_imgstore_add(filedata, size, name);
	g_free(filedata);

	if (id == 0) {
		buf = g_strdup_printf(_("Failed to store image: %s\n"), filename);
		gaim_notify_error(NULL, NULL, buf, NULL);

		g_free(buf);
		g_free(filename);

		return;
	}

	g_free(filename);

	ins = gtk_text_buffer_get_insert(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)));
	gtk_text_buffer_get_iter_at_mark(gtk_text_view_get_buffer(GTK_TEXT_VIEW(toolbar->imhtml)),
									 &iter, ins);
	gtk_imhtml_insert_image_at_iter(GTK_IMHTML(toolbar->imhtml), id, &iter);
	gaim_imgstore_unref(id);
}


static void
insert_image_cb(GtkWidget *save, GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *window;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->image))) {
#if GTK_CHECK_VERSION(2,4,0) /* FILECHOOSER */
		window = gtk_file_chooser_dialog_new(_("Insert Image"),
						NULL,
						GTK_FILE_CHOOSER_ACTION_OPEN,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						NULL);
		gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_ACCEPT);
		g_signal_connect(G_OBJECT(GTK_FILE_CHOOSER(window)),
				"response", G_CALLBACK(do_insert_image_cb), toolbar);
#else /* FILECHOOSER */
		window = gtk_file_selection_new(_("Insert Image"));
		gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_OK);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)),
				"response", G_CALLBACK(do_insert_image_cb), toolbar);
#endif /* FILECHOOSER */

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

static void
close_smiley_dialog(GtkWidget *widget, GdkEvent *event,
					GtkIMHtmlToolbar *toolbar)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smiley), FALSE);
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

	close_smiley_dialog(NULL, NULL, toolbar);
}

/* smiley buttons list */
struct smiley_button_list {
	int width, height;
	GtkWidget *button;
	struct smiley_button_list *next;
};

static struct smiley_button_list *
sort_smileys(struct smiley_button_list *ls, GtkIMHtmlToolbar *toolbar, int *width, char *filename, char *face)
{
	GtkWidget *image;
	GtkWidget *button;
	GtkRequisition size;
	struct smiley_button_list *cur;
	struct smiley_button_list *it, *it_last;

	cur = malloc(sizeof(struct smiley_button_list));
	it = ls;
	it_last = ls; /* list iterators*/
	image = gtk_image_new_from_file(filename);

	gtk_widget_size_request(image, &size);
	(*width) += size.width;

	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), image);

	g_object_set_data(G_OBJECT(button), "smiley_text", face);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(insert_smiley_text), toolbar);

	gtk_tooltips_set_tip(toolbar->tooltips, button, face, NULL);

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	/* set current element to add */
	cur->height = size.height;
	cur->width = size.width;
	cur->button = button;
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
		GtkIMHtmlSmiley *cur = list->data;
		if (!strcmp(cur->file, smiley->file))
			return FALSE;
		list = list->next;
	}
	return TRUE;
}


static void
insert_smiley_cb(GtkWidget *smiley, GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *dialog;
	GtkWidget *smiley_table = NULL;
	GSList *smileys, *unique_smileys = NULL;

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smiley))) {
		destroy_smiley_dialog(toolbar);
		gtk_widget_grab_focus(toolbar->imhtml);
		return;
	}

	if (toolbar->sml)
		smileys = pidginthemes_get_proto_smileys(toolbar->sml);
	else
		smileys = pidginthemes_get_proto_smileys(NULL);

	while(smileys) {
		GtkIMHtmlSmiley *smiley = smileys->data;
		if(!smiley->hidden) {
			if(smiley_is_unique(unique_smileys, smiley))
				unique_smileys = g_slist_append(unique_smileys, smiley);
		}
		smileys = smileys->next;
	}

	GAIM_DIALOG(dialog);

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_role(GTK_WINDOW(dialog), "smiley_dialog");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

	if (g_slist_length(unique_smileys)) {
		struct smiley_button_list *ls, *it, *it_tmp;
		GtkWidget *line;
		int line_width = 0;
		int max_line_width, num_lines;
		int col=0;

		/* We use hboxes packed in a vbox */
		ls = NULL;
		line = gtk_hbox_new(FALSE, 0);
		line_width = 0;
		max_line_width = 0;
		num_lines = floor(sqrt(g_slist_length(unique_smileys)));
		smiley_table = gtk_vbox_new(FALSE, 0);

		/* create list of smileys sorted by height */
		while (unique_smileys) {
			GtkIMHtmlSmiley *smiley = unique_smileys->data;
			if (!smiley->hidden) {
				fflush(stdout);
				ls = sort_smileys(ls, toolbar, &max_line_width, smiley->file, smiley->smile);
			}
			unique_smileys = unique_smileys->next;
		}
		/* pack buttons of the list */
		max_line_width = max_line_width / num_lines;
		it = ls;
		while (it != NULL)
		{
			it_tmp = it;
			gtk_box_pack_start(GTK_BOX(line), it->button, TRUE, TRUE, 0);
			gtk_widget_show(it->button);
			line_width += it->width;
			if (line_width >= max_line_width) {
				gtk_box_pack_start(GTK_BOX(smiley_table), line, FALSE, TRUE, 0);
				line = gtk_hbox_new(FALSE, 0);
				line_width = 0;
				col = 0;
			}
			col++;
			it = it->next;
			free(it_tmp);
		}
		gtk_box_pack_start(GTK_BOX(smiley_table), line, FALSE, TRUE, 0);
	}
	else {
		smiley_table = gtk_label_new(_("This theme has no available smileys."));
	}

	gtk_container_add(GTK_CONTAINER(dialog), smiley_table);

	gtk_widget_show(smiley_table);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

	/* connect signals */
	g_signal_connect(G_OBJECT(dialog), "delete_event",
					 G_CALLBACK(close_smiley_dialog), toolbar);

	/* show everything */
	gtk_window_set_title(GTK_WINDOW(dialog), _("Smile!"));
	gtk_widget_show_all(dialog);

	toolbar->smiley_dialog = dialog;

	gtk_widget_grab_focus(toolbar->imhtml);
}

static void update_buttons_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, GtkIMHtmlToolbar *toolbar)
{
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->bold), buttons & GTK_IMHTML_BOLD);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->italic), buttons & GTK_IMHTML_ITALIC);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->underline), buttons & GTK_IMHTML_UNDERLINE);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->larger_size), buttons & GTK_IMHTML_GROW);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->smaller_size), buttons & GTK_IMHTML_SHRINK);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->font), buttons & GTK_IMHTML_FACE);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->fgcolor), buttons & GTK_IMHTML_FORECOLOR);
	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->bgcolor), buttons & GTK_IMHTML_BACKCOLOR);

	gtk_widget_set_sensitive(GTK_WIDGET(toolbar->clear),
							 (buttons & GTK_IMHTML_BOLD ||
							  buttons & GTK_IMHTML_ITALIC ||
							  buttons & GTK_IMHTML_UNDERLINE ||
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

static void update_buttons(GtkIMHtmlToolbar *toolbar) {
	gboolean bold, italic, underline;
	char *tmp;
	char *tmp2;

	gtk_imhtml_get_current_format(GTK_IMHTML(toolbar->imhtml),
								  &bold, &italic, &underline);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->bold)) != bold)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->bold), bold,
									   toolbar);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->italic)) != italic)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->italic), italic,
									   toolbar);

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->underline)) != underline)
		toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->underline),
									   underline, toolbar);

	/* These buttons aren't ever "active". */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smaller_size), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->larger_size), FALSE);

	tmp = gtk_imhtml_get_current_fontface(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->font),
								   (tmp != NULL), toolbar);
	g_free(tmp);

	tmp = gtk_imhtml_get_current_forecolor(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->fgcolor),
								   (tmp != NULL), toolbar);
	g_free(tmp);

	tmp = gtk_imhtml_get_current_backcolor(GTK_IMHTML(toolbar->imhtml));
	tmp2 = gtk_imhtml_get_current_background(GTK_IMHTML(toolbar->imhtml));
	toggle_button_set_active_block(GTK_TOGGLE_BUTTON(toolbar->bgcolor),
								   (tmp != NULL || tmp2 != NULL), toolbar);
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

enum {
	LAST_SIGNAL
};
/* static guint signals [LAST_SIGNAL] = { 0 }; */

static void
gtk_imhtmltoolbar_finalize (GObject *object)
{
	GtkIMHtmlToolbar *toolbar = GTK_IMHTMLTOOLBAR(object);

	if (toolbar->image_dialog != NULL)
	{
		gtk_widget_destroy(toolbar->image_dialog);
		toolbar->image_dialog = NULL;
	}

	destroy_toolbar_font(NULL, NULL, toolbar);
	destroy_smiley_dialog(toolbar);
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

	free(toolbar->sml);
	gtk_object_sink(GTK_OBJECT(toolbar->tooltips));

	G_OBJECT_CLASS(parent_class)->finalize (object);
}

/* Boring GTK+ stuff */
static void gtk_imhtmltoolbar_class_init (GtkIMHtmlToolbarClass *class)
{
	GtkObjectClass *object_class;
	GObjectClass   *gobject_class;
	object_class = (GtkObjectClass*) class;
	gobject_class = (GObjectClass*) class;
	parent_class = gtk_type_class(GTK_TYPE_HBOX);
	gobject_class->finalize = gtk_imhtmltoolbar_finalize;
}

static void gtk_imhtmltoolbar_init (GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *hbox = GTK_WIDGET(toolbar);
	GtkWidget *button;
	GtkWidget *sep;
	GtkSizeGroup *sg;

	toolbar->imhtml = NULL;
	toolbar->font_dialog = NULL;
	toolbar->fgcolor_dialog = NULL;
	toolbar->bgcolor_dialog = NULL;
	toolbar->link_dialog = NULL;
	toolbar->smiley_dialog = NULL;
	toolbar->image_dialog = NULL;

	toolbar->tooltips = gtk_tooltips_new();

	gtk_box_set_spacing(GTK_BOX(toolbar), GAIM_HIG_BOX_SPACE);
	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	/* Bold */
	button = pidgin_pixbuf_toolbar_button_from_stock(GTK_STOCK_BOLD);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Bold"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_bold), toolbar);

	toolbar->bold = button;

	/* Italic */
	button = pidgin_pixbuf_toolbar_button_from_stock(GTK_STOCK_ITALIC);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Italic"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_italic), toolbar);

	toolbar->italic = button;

	/* Underline */
	button = pidgin_pixbuf_toolbar_button_from_stock(GTK_STOCK_UNDERLINE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Underline"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_underline), toolbar);

	toolbar->underline = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Increase font size */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_TEXT_BIGGER);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Larger font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_big), toolbar);

	toolbar->larger_size = button;

	/* Decrease font size */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_TEXT_SMALLER);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Smaller font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_small), toolbar);

	toolbar->smaller_size = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Font Face */

	button = pidgin_pixbuf_toolbar_button_from_stock(GTK_STOCK_SELECT_FONT);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			_("Font face"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(toggle_font), toolbar);

	toolbar->font = button;

	/* Foreground Color */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_FGCOLOR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Foreground font color"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(toggle_fg_color), toolbar);

	toolbar->fgcolor = button;

	/* Background Color */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_BGCOLOR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Background color"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(toggle_bg_color), toolbar);

	toolbar->bgcolor = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Reset Formatting */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_CLEAR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Reset formatting"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(clear_formatting_cb), toolbar);

	toolbar->clear = button;

	/* Sep */
	sep = gtk_vseparator_new();
	gtk_box_pack_start(GTK_BOX(hbox), sep, FALSE, FALSE, 0);

	/* Insert Link */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_LINK);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert link"), NULL);
	g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(insert_link_cb), toolbar);

	toolbar->link = button;

	/* Insert IM Image */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_IMAGE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert image"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(insert_image_cb), toolbar);

	toolbar->image = button;

	/* Insert Smiley */
	button = pidgin_pixbuf_toolbar_button_from_stock(PIDGIN_STOCK_SMILEY);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert smiley"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(insert_smiley_cb), toolbar);

	toolbar->smiley = button;

	toolbar->sml = NULL;
	gtk_widget_show_all(hbox);
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
	gboolean bold, italic, underline;

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

	gtk_imhtml_get_current_format(GTK_IMHTML(imhtml), &bold, &italic, &underline);

	update_buttons(toolbar);
}

void gtk_imhtmltoolbar_associate_smileys(GtkIMHtmlToolbar *toolbar, const char *proto_id)
{
	g_free(toolbar->sml);
	toolbar->sml = g_strdup(proto_id);
}

/*
 * GtkIMHtmlToolbar
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
#include "gtkinternal.h"

#include "gtkimhtmltoolbar.h"
#include "gtkutils.h"

#include "imgstore.h"
#include "notify.h"
#include "request.h"
#include "stock.h"
#include "ui.h"
#include "util.h"

static GtkVBoxClass *parent_class = NULL;

static void do_bold(GtkWidget *bold, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_toggle_bold(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_italic(GtkWidget *italic, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_toggle_italic(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_underline(GtkWidget *underline, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_toggle_underline(GTK_IMHTML(toolbar->imhtml));
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_small(GtkWidget *smalltb, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_font_shrink(GTK_IMHTML(toolbar->imhtml));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smaller_size), FALSE);
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void
do_big(GtkWidget *large, GtkIMHtmlToolbar *toolbar)
{
	g_return_if_fail(toolbar);
	gtk_imhtml_font_grow(GTK_IMHTML(toolbar->imhtml));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->larger_size), FALSE);
	gtk_widget_grab_focus(toolbar->imhtml);
}



static void toolbar_cancel_font(GtkWidget *widget, GdkEvent *event,
								GtkIMHtmlToolbar *toolbar)
{

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->font), FALSE);

	if (toolbar->font_dialog) {
		gtk_widget_destroy(toolbar->font_dialog);
		toolbar->font_dialog = NULL;
	}
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

	toolbar_cancel_font(NULL, NULL, toolbar);
}

static void
toggle_font(GtkWidget *font, GtkIMHtmlToolbar *toolbar)
{
#if 0
	char fonttif[128];
	const char *fontface;
#endif

	g_return_if_fail(toolbar);

	if (!toolbar->font_dialog) {
		toolbar->font_dialog = gtk_font_selection_dialog_new(_("Select Font"));

		g_object_set_data(G_OBJECT(toolbar->font_dialog), "gaim_toolbar", toolbar);

		/*	if (gtkconv->fontface[0]) {
		  g_snprintf(fonttif, sizeof(fonttif), "%s 12", gtkconv->fontface);
		  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font),
		  fonttif);
		  } else {
		  gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(gtkconv->dialogs.font),
		  DEFAULT_FONT_FACE);
		  }
		*/

		g_signal_connect(G_OBJECT(toolbar->font_dialog), "delete_event",
				 G_CALLBACK(toolbar_cancel_font), toolbar);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)->ok_button),
				 "clicked", G_CALLBACK(apply_font), toolbar->font_dialog);
		g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(toolbar->font_dialog)->cancel_button),
				 "clicked", G_CALLBACK(toolbar_cancel_font), toolbar);


		gtk_window_present(GTK_WINDOW(toolbar->font_dialog));
	} else {
		toolbar_cancel_font(NULL, NULL, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void cancel_toolbar_fgcolor(GtkWidget *widget, GdkEvent *event,
								   GtkIMHtmlToolbar *toolbar)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->fgcolor), FALSE);
	gtk_widget_destroy(toolbar->fgcolor_dialog);
	toolbar->fgcolor_dialog = NULL;
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
	cancel_toolbar_fgcolor(NULL, NULL, toolbar);
}

static void
toggle_fg_color(GtkWidget *color, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color))) {
		GtkWidget *colorsel;
		/* GdkColor fgcolor; */

		/*gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"),
		  &fgcolor);*/
		if (!toolbar->fgcolor_dialog) {

			toolbar->fgcolor_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
			colorsel = GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->colorsel;
			//gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &fgcolor);
			g_object_set_data(G_OBJECT(colorsel), "gaim_toolbar", toolbar);

			g_signal_connect(G_OBJECT(toolbar->fgcolor_dialog), "delete_event",
					 G_CALLBACK(cancel_toolbar_fgcolor), toolbar);
			g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->ok_button),
					 "clicked", G_CALLBACK(do_fgcolor), colorsel);
			g_signal_connect(G_OBJECT
					 (GTK_COLOR_SELECTION_DIALOG(toolbar->fgcolor_dialog)->cancel_button),
					 "clicked", G_CALLBACK(cancel_toolbar_fgcolor), toolbar);

		}
		gtk_window_present(GTK_WINDOW(toolbar->fgcolor_dialog));
	} else if (toolbar->fgcolor_dialog != NULL) {
		cancel_toolbar_fgcolor(color, NULL, toolbar);
	} else {
		//gaim_gtk_advance_past(gtkconv, "<FONT COLOR>", "</FONT>");
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

static void cancel_toolbar_bgcolor(GtkWidget *widget, GdkEvent *event,
								   GtkIMHtmlToolbar *toolbar)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->bgcolor), FALSE);
	gtk_widget_destroy(toolbar->bgcolor_dialog);
	toolbar->bgcolor_dialog = NULL;
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
	gtk_imhtml_toggle_backcolor(GTK_IMHTML(toolbar->imhtml), open_tag);

	g_free(open_tag);
	cancel_toolbar_bgcolor(NULL, NULL, toolbar);
}

static void
toggle_bg_color(GtkWidget *color, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(color))) {
		GtkWidget *colorsel;
		/* GdkColor bgcolor; */

		/*gdk_color_parse(gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"),
		  &bgcolor);*/
		if (!toolbar->bgcolor_dialog) {

			toolbar->bgcolor_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
			colorsel = GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->colorsel;
			//gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &bgcolor);
			g_object_set_data(G_OBJECT(colorsel), "gaim_toolbar", toolbar);

			g_signal_connect(G_OBJECT(toolbar->bgcolor_dialog), "delete_event",
					 G_CALLBACK(cancel_toolbar_bgcolor), toolbar);
			g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->ok_button),
					 "clicked", G_CALLBACK(do_bgcolor), colorsel);
			g_signal_connect(G_OBJECT
					 (GTK_COLOR_SELECTION_DIALOG(toolbar->bgcolor_dialog)->cancel_button),
					 "clicked", G_CALLBACK(cancel_toolbar_bgcolor), toolbar);

		}
		gtk_window_present(GTK_WINDOW(toolbar->bgcolor_dialog));
	} else if (toolbar->bgcolor_dialog != NULL) {
		cancel_toolbar_bgcolor(color, NULL, toolbar);
	} else {
		//gaim_gtk_advance_past(gtkconv, "<FONT COLOR>", "</FONT>");
	}
	gtk_widget_grab_focus(toolbar->imhtml);
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
	description = gaim_request_fields_get_string(fields, "description");

	if (description == NULL)
		description = url;

	gtk_imhtml_insert_link(GTK_IMHTML(toolbar->imhtml), url, description);

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

		fields = gaim_request_fields_new();

		group = gaim_request_field_group_new(NULL);
		gaim_request_fields_add_group(fields, group);

		field = gaim_request_field_string_new("url", _("_URL"), NULL, FALSE);
		gaim_request_field_set_required(field, TRUE);
		gaim_request_field_group_add_field(group, field);

		field = gaim_request_field_string_new("description", _("_Description"),
						      NULL, FALSE);
		gaim_request_field_group_add_field(group, field);

		toolbar->link_dialog =
			gaim_request_fields(toolbar, _("Insert Link"),
					    NULL,
					    _("Please enter the URL and description of the "
					      "link that you want to insert. The description "
					      "is optional."),
					    fields,
					    _("_Insert"), G_CALLBACK(do_insert_link_cb),
					    _("Cancel"), G_CALLBACK(cancel_link_cb),
					    toolbar);
	} else {
		close_link_dialog(toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}


static void
do_insert_image_cb(GtkWidget *widget, int resp, GtkIMHtmlToolbar *toolbar)
{
	char *name, *filename;
	char *buf, *filedata;
	size_t size;
	GError *error = NULL;
	int id;

	if (resp != GTK_RESPONSE_OK) {
		//set_toggle(toolbar->image, FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

	name = g_strdup(gtk_file_selection_get_filename(GTK_FILE_SELECTION(widget)));

	if (!name) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

	if (gaim_gtk_check_if_dir(name, GTK_FILE_SELECTION(widget))) {
		g_free(name);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);
		return;
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->image), FALSE);

	if (!g_file_get_contents(name, &filedata, &size, &error)) {
		gaim_notify_error(NULL, NULL, error->message, NULL);

		g_error_free(error);
		g_free(name);

		return;
	}

	filename = name;
	while (strchr(filename, '/'))
		filename = strchr(filename, '/') + 1;

	id = gaim_imgstore_add(filedata, size, filename);
	g_free(filedata);

	if (!id) {
		buf = g_strdup_printf(_("Failed to store image: %s\n"), name);
		gaim_notify_error(NULL, NULL, buf, NULL);

		g_free(buf);
		g_free(name);

		return;
	}

	//im->images = g_slist_append(im->images, GINT_TO_POINTER(id));

	/*buf = g_strdup_printf("<IMG ID=\"%d\" SRC=\"file://%s\">", id, filename);
	gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtkconv->entry_buffer), buf, -1);
	g_free(buf);
	*/
	g_free(name);
}


static void
insert_image_cb(GtkWidget *save, GtkIMHtmlToolbar *toolbar)
{
	char buf[BUF_LONG];
	GtkWidget *window;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toolbar->image))) {
		window = gtk_file_selection_new(_("Insert Image"));
		g_snprintf(buf, sizeof(buf), "%s" G_DIR_SEPARATOR_S, gaim_home_dir());
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(window), buf);

		gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_OK);
		g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(window)),
				"response", G_CALLBACK(do_insert_image_cb), toolbar);

		gtk_widget_show(window);
		toolbar->image_dialog = window;
	} else {
		gtk_widget_destroy(toolbar->image_dialog);
		toolbar->image_dialog = NULL;
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}


void close_smiley_dialog(GtkWidget *widget, GdkEvent *event,
						 GtkIMHtmlToolbar *toolbar)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toolbar->smiley), FALSE);

	if (toolbar->smiley_dialog) {
		gtk_widget_destroy(toolbar->smiley_dialog);
		toolbar->smiley_dialog = NULL;
	}
}


void insert_smiley_text(GtkWidget *widget, GtkIMHtmlToolbar *toolbar)
{
	char *smiley_text = g_object_get_data(G_OBJECT(widget), "smiley_text");
	//GaimPlugin *proto = gaim_find_prpl(gaim_account_get_protocol_id(gaim_conversation_get_account(c)));

	gtk_imhtml_insert_smiley(GTK_IMHTML(toolbar->imhtml), NULL, smiley_text); //proto->info->name, smiley_text);

	close_smiley_dialog(NULL, NULL, toolbar);
}


static void add_smiley(GtkIMHtmlToolbar *toolbar, GtkWidget *table, int row, int col, char *filename, char *face)
{
	GtkWidget *image;
	GtkWidget *button;

	image = gtk_image_new_from_file(filename);
	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), image);
	g_object_set_data(G_OBJECT(button), "smiley_text", face);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(insert_smiley_text), toolbar);

	gtk_tooltips_set_tip(toolbar->tooltips, button, face, NULL);

	gtk_table_attach_defaults(GTK_TABLE(table), button, col, col+1, row, row+1);

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	gtk_widget_show(button);
}


static gboolean smiley_is_unique(GSList *list, GtkIMHtmlSmiley *smiley) {
	while(list) {
		GtkIMHtmlSmiley *cur = list->data;
		if(!strcmp(cur->file, smiley->file))
			return FALSE;
		list = list->next;
	}
	return TRUE;
}


static void
insert_smiley_cb(GtkWidget *smiley, GtkIMHtmlToolbar *toolbar)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smiley))) {

		GtkWidget *dialog;
		GtkWidget *smiley_table = NULL;
		GSList *smileys, *unique_smileys = NULL;
		int width;
		int row = 0, col = 0;

		if (toolbar->smiley_dialog) {
			gtk_widget_grab_focus(toolbar->imhtml);
			return;
		}

		/*
		  if(c->account)
		  smileys = get_proto_smileys(
		  gaim_account_get_protocol_id(gaim_conversation_get_account(c)));
		  else
		*/

		smileys = get_proto_smileys(GAIM_PROTO_DEFAULT);

		while(smileys) {
			GtkIMHtmlSmiley *smiley = smileys->data;
			if(!smiley->hidden) {
				if(smiley_is_unique(unique_smileys, smiley))
					unique_smileys = g_slist_append(unique_smileys, smiley);
			}
			smileys = smileys->next;
		}


		width = floor(sqrt(g_slist_length(unique_smileys)));

		GAIM_DIALOG(dialog);
		gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
		gtk_window_set_role(GTK_WINDOW(dialog), "smiley_dialog");
		gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

		smiley_table = gtk_table_new(width, width, TRUE);

		/* pack buttons */

		while(unique_smileys) {
			GtkIMHtmlSmiley *smiley = unique_smileys->data;
			if(!smiley->hidden) {
				add_smiley(toolbar, smiley_table, row, col, smiley->file, smiley->smile);
				if(++col >= width) {
					col = 0;
					row++;
				}
			}
			unique_smileys = unique_smileys->next;
		}

		gtk_container_add(GTK_CONTAINER(dialog), smiley_table);

		gtk_widget_show(smiley_table);

		gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

		/* connect signals */
		g_object_set_data(G_OBJECT(dialog), "dialog_type", "smiley dialog");
		g_signal_connect(G_OBJECT(dialog), "delete_event",
				 G_CALLBACK(close_smiley_dialog), toolbar);

		/* show everything */
		gtk_window_set_title(GTK_WINDOW(dialog), _("Smile!"));
		gtk_widget_show_all(dialog);

		toolbar->smiley_dialog = dialog;

	} else if (toolbar->smiley_dialog) {
		close_smiley_dialog(smiley, NULL, toolbar);
	}
	gtk_widget_grab_focus(toolbar->imhtml);
}

enum {
	LAST_SIGNAL
};
//static guint signals [LAST_SIGNAL] = { 0 };

static void
gtk_imhtmltoolbar_finalize (GObject *object)
{
	/*GtkIMHtml *imhtml = GTK_IMHTML(object);
	GList *scalables;

	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	gdk_cursor_unref(imhtml->hand_cursor);
	gdk_cursor_unref(imhtml->arrow_cursor);
	gdk_cursor_unref(imhtml->text_cursor);
	if(imhtml->tip_window){
		gtk_widget_destroy(imhtml->tip_window);
	}
	if(imhtml->tip_timer)
		gtk_timeout_remove(imhtml->tip_timer);

	for(scalables = imhtml->scalables; scalables; scalables = scalables->next) {
		GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(scalables->data);
		scale->free(scale);
	}

	g_list_free(imhtml->scalables);
	G_OBJECT_CLASS(parent_class)->finalize (object);*/
}

/* Boring GTK stuff */
static void gtk_imhtmltoolbar_class_init (GtkIMHtmlToolbarClass *class)
{
	GtkObjectClass *object_class;
	GObjectClass   *gobject_class;
	object_class = (GtkObjectClass*) class;
	gobject_class = (GObjectClass*) class;
	parent_class = gtk_type_class(GTK_TYPE_VBOX);
	/*	signals[URL_CLICKED] = g_signal_new(url_clicked",
						G_TYPE_FROM_CLASS(gobject_class),
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET(GtkIMHtmlClass, url_clicked),
						NULL,
						0,
						g_cclosure_marshal_VOID__POINTER,
						G_TYPE_NONE, 1,
						G_TYPE_POINTER);*/
	gobject_class->finalize = gtk_imhtmltoolbar_finalize;
}

static void gtk_imhtmltoolbar_init (GtkIMHtmlToolbar *toolbar)
{
	GtkWidget *hbox;
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

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(toolbar), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(toolbar), hbox, FALSE, FALSE, 0);

	/* Bold */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_BOLD);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Bold"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_bold), toolbar);

	toolbar->bold = button;

	/* Italic */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_ITALIC);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Italic"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_italic), toolbar);

	toolbar->italic = button;

	/* Underline */
	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_UNDERLINE);
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
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_TEXT_BIGGER);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Larger font size"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(do_big), toolbar);

	toolbar->larger_size = button;

	/* Decrease font size */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_TEXT_SMALLER);
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

	button = gaim_pixbuf_toolbar_button_from_stock(GTK_STOCK_SELECT_FONT);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			_("Font Face"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(toggle_font), toolbar);

	toolbar->font = button;

	/* Foreground Color */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_FGCOLOR);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button,
			     _("Foreground font color"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(toggle_fg_color), toolbar);

	toolbar->fgcolor = button;

	/* Background Color */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_BGCOLOR);
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

	/* Insert Link */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_LINK);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert link"), NULL);
	g_signal_connect(G_OBJECT(button), "clicked",
				 G_CALLBACK(insert_link_cb), toolbar);

	toolbar->link = button;

	/* Insert IM Image */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_IMAGE);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert image"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(insert_image_cb), toolbar);

	toolbar->image = button;

	/* Insert Smiley */
	button = gaim_pixbuf_toolbar_button_from_stock(GAIM_STOCK_SMILEY);
	gtk_size_group_add_widget(sg, button);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(toolbar->tooltips, button, _("Insert smiley"), NULL);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(insert_smiley_cb), toolbar);

	toolbar->smiley = button;


	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(toolbar), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);


//if (!gaim_prefs_get_bool("/gaim/gtk/conversations/show_formatting_toolbar"))
//	gtk_widget_hide(vbox);

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
			(GInstanceInitFunc) gtk_imhtmltoolbar_init
		};

		imhtmltoolbar_type = g_type_register_static(GTK_TYPE_VBOX,
				"GtkIMHtmlToolbar", &imhtmltoolbar_info, 0);
	}

	return imhtmltoolbar_type;
}


void gtk_imhtmltoolbar_attach    (GtkIMHtmlToolbar *toolbar, GtkWidget *imhtml)
{
	toolbar->imhtml = imhtml;
}

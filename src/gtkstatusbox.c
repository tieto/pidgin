/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "internal.h"
#include "account.h"
#include "status.h"
#include "gtkgaim.h"
#include "gtkstock.h"
#include "gtkstatusbox.h"

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data);

static void gtk_gaim_status_box_changed(GtkComboBox *box);
static void gtk_gaim_status_box_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_gaim_status_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean gtk_gaim_status_box_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void gtk_gaim_status_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

static void (*combo_box_size_request)(GtkWidget *widget, GtkRequisition *requisition);
static void (*combo_box_size_allocate)(GtkWidget *widget, GtkAllocation *allocation);
static gboolean (*combo_box_expose_event)(GtkWidget *widget, GdkEventExpose *event);
static void (*combo_box_forall) (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
enum {
	ICON_COLUMN,
	TEXT_COLUMN,
	TITLE_COLUMN,
	DESC_COLUMN,
	TYPE_COLUMN,
	NUM_COLUMNS
};

static void gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass);
static void gtk_gaim_status_box_init (GtkGaimStatusBox *status_box);

GType
gtk_gaim_status_box_get_type (void)
{
	static GType status_box_type = 0;

	if (!status_box_type)
	{
		static const GTypeInfo status_box_info =
		{
			sizeof (GtkGaimStatusBoxClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gtk_gaim_status_box_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkGaimStatusBox),
			0,
			(GInstanceInitFunc) gtk_gaim_status_box_init
		};

		status_box_type = g_type_register_static(GTK_TYPE_COMBO_BOX,
												 "GtkGaimStatusBox",
												 &status_box_info,
												 0);
	}

	return status_box_type;
}

static void
gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkComboBoxClass *parent_class = (GtkComboBoxClass*)klass;
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	parent_class->changed = gtk_gaim_status_box_changed;
	widget_class = (GtkWidgetClass*)klass;
	combo_box_size_request = widget_class->size_request;
	widget_class->size_request = gtk_gaim_status_box_size_request;
	combo_box_size_allocate = widget_class->size_allocate;
	widget_class->size_allocate = gtk_gaim_status_box_size_allocate;
	combo_box_expose_event = widget_class->expose_event;
	widget_class->expose_event = gtk_gaim_status_box_expose_event;

	combo_box_forall = container_class->forall;
	container_class->forall = gtk_gaim_status_box_forall;

	object_class = (GObjectClass *)klass;
}

static void
gtk_gaim_status_box_refresh(GtkGaimStatusBox *status_box)
{
	char *text, *title;
	char aa_color[8];
	GdkPixbuf *pixbuf;
	GtkTreePath *path;

	GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(status_box));
	snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
		 style->text_aa[GTK_STATE_NORMAL].red >> 8,
		 style->text_aa[GTK_STATE_NORMAL].green >> 8,
		 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

	title = status_box->title;
	if (!title)
		title = "";

	if (status_box->error) {
		text = g_strdup_printf("%s\n<span size=\"smaller\" weight=\"bold\" color=\"red\">%s</span>", 
							   title, status_box->error);
	} else if (status_box->typing) {
		text = g_strdup_printf("%s\n<span size=\"smaller\" color=\"%s\">%s</span>",
							   title, aa_color, _("Typing"));
	} else if (status_box->connecting) {
		text = g_strdup_printf("%s\n<span size=\"smaller\" color=\"%s\">%s</span>",
							   title, aa_color, _("Connecting"));
	} else if (status_box->desc) {
		text = g_strdup_printf("%s\n<span size=\"smaller\" color=\"%s\">%s</span>",
							   title, aa_color, status_box->desc);
	} else {
		text = g_strdup_printf("%s", title);
	}

	if (status_box->error)
		pixbuf = status_box->error_pixbuf;
	else if (status_box->typing)
		pixbuf = status_box->typing_pixbufs[status_box->typing_index];
	else if (status_box->connecting)
		pixbuf = status_box->connecting_pixbufs[status_box->connecting_index];
	else
		pixbuf = status_box->pixbuf;

	gtk_list_store_set(status_box->store, &(status_box->iter),
			   ICON_COLUMN, pixbuf,
			   TEXT_COLUMN, text,
			   TITLE_COLUMN, title,
			   DESC_COLUMN, status_box->desc,
			   TYPE_COLUMN, NULL, -1);
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	g_free(text);
}

static void
gtk_gaim_status_box_init (GtkGaimStatusBox *status_box)
{
	GtkCellRenderer *text_rend = gtk_cell_renderer_text_new();
	GtkCellRenderer *icon_rend = gtk_cell_renderer_pixbuf_new();
	GtkTextBuffer *buffer;
	GdkPixbuf *pixbuf, *pixbuf2, *pixbuf3, *pixbuf4;
	GtkIconSize icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);

	status_box->imhtml_visible = FALSE;
	status_box->error_pixbuf = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_OFFLINE,
							   icon_size, "GtkGaimStatusBox");
	status_box->connecting_index = 0;
	status_box->connecting_pixbufs[0] = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_CONNECT0,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[1] = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_CONNECT1,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[2] = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_CONNECT2,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[3] = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_CONNECT3,
								     icon_size, "GtkGaimStatusBox");

	status_box->typing_index = 0;
	status_box->typing_pixbufs[0] =  gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_TYPING0,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[1] =  gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_TYPING1,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[2] =  gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_TYPING2,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[3] =  gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_TYPING3,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting = FALSE;
	status_box->typing = FALSE;
	status_box->title = NULL;
	status_box->pixbuf = NULL;
	status_box->cell_view = gtk_cell_view_new();
	gtk_widget_show (status_box->cell_view);

	status_box->store = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	status_box->dropdown_store = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));
	gtk_cell_view_set_model(GTK_CELL_VIEW(status_box->cell_view), GTK_TREE_MODEL(status_box->store));
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(status_box), 0);
	gtk_list_store_append(status_box->store, &(status_box->iter));
	gtk_gaim_status_box_refresh(status_box);
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), gtk_tree_path_new_from_string("0"));
	gtk_container_add(GTK_CONTAINER(status_box), status_box->cell_view);

	status_box->icon_rend = gtk_cell_renderer_pixbuf_new();
	status_box->text_rend = gtk_cell_renderer_text_new();

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), text_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), text_rend, "markup", TEXT_COLUMN, NULL);

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, TRUE); 
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, "markup", TEXT_COLUMN, NULL);

	status_box->vbox = gtk_vbox_new(0, FALSE);
	status_box->imhtml = gtk_imhtml_new(NULL, NULL);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(imhtml_changed_cb), status_box);
	gtk_imhtml_set_editable(GTK_IMHTML(status_box->imhtml), TRUE);
	gtk_widget_set_parent(status_box->vbox, GTK_WIDGET(status_box));
	status_box->sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(status_box->sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(status_box->sw), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(status_box->sw), status_box->imhtml);
	gtk_box_pack_start(GTK_BOX(status_box->vbox), status_box->sw, TRUE, TRUE, 0);
	pixbuf = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_ONLINE,
					 icon_size, "GtkGaimStatusBox");
	pixbuf2 = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_AWAY,
					 icon_size, "GtkGaimStatusBox");
	pixbuf3 = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_OFFLINE,
					 icon_size, "GtkGaimStatusBox");
	pixbuf4 = gtk_widget_render_icon (GTK_WIDGET(status_box), GAIM_STOCK_STATUS_INVISIBLE,
					 icon_size, "GtkGaimStatusBox");
	/* hacks */
	gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), pixbuf, _("Available"), NULL, "available");
	gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), pixbuf2, _("Away"), NULL, "away");
	gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), pixbuf4, _("Invisible"), NULL, "invisible");
	gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), pixbuf3, _("Offline"), NULL, "offline");
	/*
	 * TODO: This triggers a callback of gaim_gtk_status_box_changed().
	 *       That's bad.  We should at least try not figure out what
	 *       status the user's accounts are set to instead of always
	 *       using "Available."
	 */
	gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), 0);

}


static void
gtk_gaim_status_box_size_request(GtkWidget *widget,
								 GtkRequisition *requisition)
{
	GtkRequisition box_req;
	combo_box_size_request(widget, requisition);

	gtk_widget_size_request(GTK_GAIM_STATUS_BOX(widget)->vbox, &box_req);
	if (box_req.height > 1)
		requisition->height = requisition->height + box_req.height + 6;

	requisition->width = 1;

}

static void
gtk_gaim_status_box_size_allocate(GtkWidget *widget,
								  GtkAllocation *allocation)
{
	GtkRequisition req = {0,0};
	GtkAllocation parent_alc = *allocation, box_alc = *allocation ;
	combo_box_size_request(widget, &req);

	/* EVIL XXX */
	box_alc.height = 80;
	/* box_alc.height = MAX(1,box_alc.height - req.height - 6); */

	box_alc.y = box_alc.y + req.height + 6;
	gtk_widget_size_allocate((GTK_GAIM_STATUS_BOX(widget))->vbox, &box_alc);

	parent_alc.height = MAX(1,req.height);
	combo_box_size_allocate(widget, &parent_alc);
	widget->allocation = *allocation;
}


static gboolean
gtk_gaim_status_box_expose_event(GtkWidget *widget,
								 GdkEventExpose *event)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX(widget);
	combo_box_expose_event(widget, event);

	gtk_container_propagate_expose(GTK_CONTAINER(widget),
								   status_box->vbox, event);
	return FALSE;
}

static void
gtk_gaim_status_box_forall(GtkContainer *container,
						   gboolean include_internals,
						   GtkCallback callback,
						   gpointer callback_data)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX (container);

	if (include_internals)
	{
		(* callback) (status_box->vbox, callback_data);
	}

	combo_box_forall(container, include_internals, callback, callback_data);
}

GtkWidget *
gtk_gaim_status_box_new()
{
	return g_object_new(GTK_GAIM_TYPE_STATUS_BOX, NULL);
}


void
gtk_gaim_status_box_add(GtkGaimStatusBox *status_box, GdkPixbuf *pixbuf, const char *text, const char *sec_text, char *edit)
{
	GtkTreeIter iter;
	char *t;

	if (sec_text) {
		char aa_color[8];
		GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(status_box));
		snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
			 style->text_aa[GTK_STATE_NORMAL].red >> 8,
			 style->text_aa[GTK_STATE_NORMAL].green >> 8,
			 style->text_aa[GTK_STATE_NORMAL].blue >> 8);
		t = g_strdup_printf("%s\n<span color=\"%s\">%s</span>", text, aa_color, sec_text);
	} else {
		t = g_strdup(text);
	}

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   ICON_COLUMN, pixbuf,
			   TEXT_COLUMN, t,
			   TITLE_COLUMN, text,
			   DESC_COLUMN, sec_text,
			   TYPE_COLUMN, edit, -1);
}

void
gtk_gaim_status_box_set_error(GtkGaimStatusBox *status_box, const gchar *error)
{
	status_box->error = g_strdup(error);
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_set_connecting(GtkGaimStatusBox *status_box, gboolean connecting)
{
	if (!status_box)
		return;
	status_box->connecting = connecting;
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_pulse_connecting(GtkGaimStatusBox *status_box)
{
	if (!status_box)
		return;
	if (status_box->connecting_index == 3)
		status_box->connecting_index = 0;
	else
		status_box->connecting_index++;
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_pulse_typing(GtkGaimStatusBox *status_box)
{
	if (status_box->typing_index == 3)
		status_box->typing_index = 0;
	else
		status_box->typing_index++;
	gtk_gaim_status_box_refresh(status_box);
}

static void remove_typing_cb(GtkGaimStatusBox *box)
{
	gchar *status_type_id;
	GList *l;
	GtkTreeIter iter;

	gtk_combo_box_get_active_iter(GTK_COMBO_BOX(box), &iter);
	gtk_tree_model_get(GTK_TREE_MODEL(box->dropdown_store), &iter, TYPE_COLUMN, &status_type_id, -1);
	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		GaimAccount *account = (GaimAccount*)l->data;
		GaimStatusType *status_type;

		if (!gaim_account_get_enabled(account, GAIM_GTK_UI))
			continue;

		status_type = gaim_account_get_status_type(account, status_type_id);

		if (status_type == NULL)
			continue;
		gaim_account_set_status(account, status_type_id, TRUE,
					"message",gtk_imhtml_get_markup(GTK_IMHTML(box->imhtml)), NULL);
	}
	g_source_remove(box->typing);
	box->typing = 0;
	gtk_gaim_status_box_refresh(box);
}

static void gtk_gaim_status_box_changed(GtkComboBox *box)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX(box);
	GtkTreeIter iter;
	char *text, *sec_text;
	GdkPixbuf *pixbuf;
	gchar *status_type_id;
	GList *l;

	gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter);
	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter, TITLE_COLUMN, &text,
			   DESC_COLUMN, &sec_text, ICON_COLUMN, &pixbuf,
			   TYPE_COLUMN, &status_type_id, -1);
	if (status_box->title)
		g_free(status_box->title);
	status_box->title = g_strdup(text);
	if (status_box->desc && sec_text)
 		g_free(status_box->desc);
	status_box->desc = g_strdup(sec_text);
	if (status_box->pixbuf)
		g_object_unref(status_box->pixbuf);
	status_box->pixbuf = pixbuf;

	if (!strcmp(status_type_id, "away")) {
		gtk_widget_show_all(status_box->vbox);
		status_box->typing = g_timeout_add(3000, (GSourceFunc)remove_typing_cb, status_box);
		gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
		gtk_widget_grab_focus(status_box->imhtml);
	} else {
		if (status_box->typing) {
			g_source_remove(status_box->typing);
			status_box->typing = 0;
		}
		gtk_widget_hide_all(status_box->vbox);
		for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
			GaimAccount *account = (GaimAccount*)l->data;
			GaimStatusType *status_type;

			if (!gaim_account_get_enabled(account, GAIM_GTK_UI))
				continue;

			status_type = gaim_account_get_status_type(account, status_type_id);

			if (status_type == NULL)
				continue;
			gaim_account_set_status(account, status_type_id, TRUE, NULL);
		}
	}
	gtk_gaim_status_box_refresh(status_box);
}

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data)
{
	GtkGaimStatusBox *box = (GtkGaimStatusBox*)data;
	if (box->typing) {
		gtk_gaim_status_box_pulse_typing(box);
		g_source_remove(box->typing);
	}
	box->typing = g_timeout_add(3000, (GSourceFunc)remove_typing_cb, box);
	gtk_gaim_status_box_refresh(box);
}

const char *gtk_gaim_status_box_get_active_type(GtkGaimStatusBox *status_box)
{
	GtkTreeIter iter;
	char *type;
	gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter);
	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   TYPE_COLUMN, &type, -1);
	return type;
}

const char *gtk_gaim_status_box_get_message(GtkGaimStatusBox *status_box)
{
	if (status_box->imhtml_visible)
		return gtk_imhtml_get_markup(GTK_IMHTML(status_box->imhtml));
	else
		return NULL;
}

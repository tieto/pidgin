/**
 * @file gtkrequest.c GTK+ Request API
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "gtkrequest.h"
#include "stock.h"
#include <gtk/gtk.h>
#include <string.h>

/* XXX For _(..) */
#include "gaim.h"

typedef struct
{
	GaimRequestType type;

	void *user_data;
	GtkWidget *dialog;
	GtkWidget *entry;

	size_t cb_count;
	GCallback *cbs;

} GaimRequestData;

static void
__input_response_cb(GtkDialog *dialog, gint id, GaimRequestData *data)
{
	if (id < data->cb_count && data->cbs[id] != NULL) {
		((GaimRequestInputCb)data->cbs[id])(
			gtk_entry_get_text(GTK_ENTRY(data->entry)), data->user_data);
	}

	gaim_request_close(GAIM_REQUEST_INPUT, data);
}

#define STOCK_ITEMIZE(r, l) \
	if (!strcmp((r), text)) \
		return (l);

static const char *
__text_to_stock(const char *text)
{
	STOCK_ITEMIZE(_("Yes"),    GTK_STOCK_YES);
	STOCK_ITEMIZE(_("No"),     GTK_STOCK_NO);
	STOCK_ITEMIZE(_("OK"),     GTK_STOCK_OK);
	STOCK_ITEMIZE(_("Cancel"), GTK_STOCK_CANCEL);
	STOCK_ITEMIZE(_("Apply"),  GTK_STOCK_APPLY);
	STOCK_ITEMIZE(_("Close"),  GTK_STOCK_CLOSE);
	STOCK_ITEMIZE(_("Delete"), GTK_STOCK_DELETE);
	STOCK_ITEMIZE(_("Add"),    GTK_STOCK_ADD);
	STOCK_ITEMIZE(_("Remove"), GTK_STOCK_REMOVE);

	return NULL;
}

void *
gaim_gtk_request_input(const char *title, const char *primary,
					   const char *secondary, const char *default_value,
					   gboolean multiline,
					   const char *ok_text, GCallback ok_cb,
					   const char *cancel_text, GCallback cancel_cb,
					   void *user_data)
{
	GaimRequestData *data;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *img;
	char *label_text;

	data            = g_new0(GaimRequestData, 1);
	data->type      = GAIM_REQUEST_INPUT;
	data->user_data = user_data;

	data->cb_count = 2;
	data->cbs = g_new0(GCallback, 2);

	data->cbs[0] = ok_cb;
	data->cbs[1] = cancel_cb;

#if 0
	data->dialog = dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_role(GTK_WINDOW(dialog), "input");
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
#endif

	/* Create the dialog. */
	dialog = gtk_dialog_new_with_buttons("", NULL, 0,
					     __text_to_stock(cancel_text), 1,
					     __text_to_stock(ok_text),     0,
					     NULL);
	data->dialog = dialog;

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(__input_response_cb), data);

	/* Setup the dialog */
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 12);
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);

	/* Setup the main horizontal box */
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	/* Dialog icon. */
	img = gtk_image_new_from_stock(GAIM_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	/* Vertical box */
	vbox = gtk_vbox_new(FALSE, 12);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

	/* Descriptive label */
	label_text = g_strdup_printf("<span weight=\"bold\" size=\"larger\">"
								 "%s</span>\n\n%s",
								 primary, (secondary ? secondary : ""));

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	g_free(label_text);

	/* Entry field. */
	data->entry = entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

	if (default_value != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), default_value);

	/* Show everything. */
	gtk_widget_show_all(dialog);

	return data;
}

void *
gaim_gtk_request_choice(const char *title, const char *primary,
						const char *secondary, unsigned int default_value,
						const char *ok_text, GCallback ok_cb,
						const char *cancel_text, GCallback cancel_cb,
						void *user_data, va_list args)
{
	return NULL;
}

void *
gaim_gtk_request_action(const char *title, const char *primary,
						const char *secondary, unsigned int default_action,
						void *user_data, va_list actions)
{
	return NULL;
}

void
gaim_gtk_close_request(GaimRequestType type, void *ui_handle)
{
	GaimRequestData *data = (GaimRequestData *)ui_handle;

	if (data->cbs != NULL)
		g_free(data->cbs);

	gtk_widget_destroy(data->dialog);

	g_free(data);
}

static GaimRequestUiOps ops =
{
	gaim_gtk_request_input,
	gaim_gtk_request_choice,
	gaim_gtk_request_action,
	gaim_gtk_close_request
};

GaimRequestUiOps *
gaim_get_gtk_request_ui_ops(void)
{
	return &ops;
}

/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */
#include "internal.h"

#include "account.h"
#include "debug.h"
#include "prefs.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkutils.h"

#include "ui.h"

struct signon_meter {
	GaimAccount *account;
	GtkWidget *button;
	GtkWidget *progress;
	GtkWidget *status;
};

struct meter_window {
		GtkWidget *window;
		GtkWidget *table;
		gint rows;
		gint active_count;
		GSList *meters;
} *meter_win = NULL;

static void kill_meter(struct signon_meter *meter, const char *text);

static void cancel_signon(GtkWidget *button, struct signon_meter *meter)
{
	if (meter->account->gc != NULL) {
		meter->account->gc->wants_to_die = TRUE;
		gaim_connection_destroy(meter->account->gc);
	}
	else {
		kill_meter(meter, _("Done."));

		if (gaim_connections_get_all() == NULL) {
			destroy_all_dialogs();

			gaim_blist_destroy();

			show_login();
		}
	}
}

static void cancel_all () {
	GSList *m = meter_win ? meter_win->meters : NULL;
	struct signon_meter *meter;

	while (m) {
		meter = m->data;
		cancel_signon(NULL, meter);
		m = m->next;
	}
}

static gint meter_destroy(GtkWidget *window, GdkEvent *evt, struct signon_meter *meter)
{
	return TRUE;
}

static struct signon_meter *find_signon_meter(GaimConnection *gc)
{
	GSList *m = meter_win ? meter_win->meters : NULL;
	struct signon_meter *meter;

	while (m) {
		meter = m->data;
		if (meter->account == gaim_connection_get_account(gc))
			return m->data;
		m = m->next;
	}

	return NULL;
}

static GtkWidget* create_meter_pixmap (GaimConnection *gc)
{
	GdkPixbuf *pb = create_prpl_icon(gc->account);
	GdkPixbuf *scale = gdk_pixbuf_scale_simple(pb, 30,30,GDK_INTERP_BILINEAR);
	GtkWidget *image =
		gtk_image_new_from_pixbuf(scale);
	g_object_unref(G_OBJECT(pb));
	g_object_unref(G_OBJECT(scale));
	return image;
}



static struct signon_meter *
new_meter(GaimConnection *gc, GtkWidget *widget,
			   GtkWidget *table, gint *rows)
{
	GtkWidget *graphic;
	GtkWidget *label;
	GtkWidget *nest_vbox;
	GString *name_to_print;
	struct signon_meter *meter;


	meter = g_new0(struct signon_meter, 1);

	meter->account = gaim_connection_get_account(gc);
	name_to_print = g_string_new(gaim_account_get_username(meter->account));

	(*rows)++;
	gtk_table_resize (GTK_TABLE(table), *rows, 4);

	graphic = create_meter_pixmap(gc);

	nest_vbox = gtk_vbox_new (FALSE, 0);

	g_string_prepend(name_to_print, _("Signon: "));
	label = gtk_label_new (name_to_print->str);
	g_string_free(name_to_print, TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	meter->status = gtk_label_new("");
	gtk_misc_set_alignment(GTK_MISC(meter->status), 0, 0.5);
	gtk_widget_set_size_request(meter->status, 250, -1);

	meter->progress = gtk_progress_bar_new ();

	meter->button = gaim_pixbuf_button_from_stock (_("Cancel"), GTK_STOCK_CANCEL, GAIM_BUTTON_HORIZONTAL);
	g_signal_connect(G_OBJECT (meter->button), "clicked",
					 G_CALLBACK (cancel_signon), meter);

	gtk_table_attach (GTK_TABLE (table), graphic, 0, 1, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), nest_vbox, 1, 2, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (label), FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (nest_vbox), GTK_WIDGET (meter->status), FALSE, FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), meter->progress, 2, 3, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
	gtk_table_attach (GTK_TABLE (table), meter->button, 3, 4, *rows, *rows+1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);

	gtk_widget_show_all (GTK_WIDGET (meter_win->window));

	meter_win->active_count++;

	return meter;
}

static void kill_meter(struct signon_meter *meter, const char *text) {
	gtk_widget_set_sensitive (meter->button, FALSE);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress), 1);
	gtk_label_set_text(GTK_LABEL(meter->status), text);
	meter_win->active_count--;
	if (meter_win->active_count == 0) {
		gtk_widget_destroy(meter_win->window);
		g_free (meter_win);
		meter_win = NULL;
	}
}

static void gaim_gtk_connection_connect_progress(GaimConnection *gc,
		const char *text, size_t step, size_t step_count)
{
	struct signon_meter *meter;

	if(!meter_win) {
		GtkWidget *vbox;
		GtkWidget *cancel_button;

		if(mainwindow)
			gtk_widget_hide(mainwindow);

		meter_win = g_new0(struct meter_window, 1);
		meter_win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_resizable(GTK_WINDOW(meter_win->window), FALSE);
		gtk_window_set_role(GTK_WINDOW(meter_win->window), "signon");
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->window), 5);
		gtk_window_set_title(GTK_WINDOW(meter_win->window), _("Signon"));
		gtk_widget_realize(meter_win->window);

		vbox = gtk_vbox_new (FALSE, 0);
		gtk_container_add(GTK_CONTAINER(meter_win->window), vbox);

		meter_win->table = gtk_table_new(1, 4, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(meter_win->table),
				FALSE, FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(meter_win->table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(meter_win->table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(meter_win->table), 10);

		cancel_button = gaim_pixbuf_button_from_stock(_("Cancel All"),
				GTK_STOCK_QUIT, GAIM_BUTTON_HORIZONTAL);
		g_signal_connect_swapped(G_OBJECT(cancel_button), "clicked",
				G_CALLBACK(cancel_all), NULL);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(cancel_button),
				FALSE, FALSE, 0);

		g_signal_connect(G_OBJECT(meter_win->window), "delete_event",
				G_CALLBACK(meter_destroy), NULL);
	}

	meter = find_signon_meter(gc);
	if(!meter) {
		meter = new_meter(gc, meter_win->window, meter_win->table,
				&meter_win->rows);

		meter_win->meters = g_slist_append(meter_win->meters, meter);
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(meter->progress),
			(float)step / (float)step_count);
	gtk_label_set_text(GTK_LABEL(meter->status), text);
}

static void gaim_gtk_connection_connected(GaimConnection *gc)
{
	struct signon_meter *meter = find_signon_meter(gc);

	gaim_setup(gc);

	update_privacy_connections();
	do_away_menu();
	gaim_gtk_blist_update_protocol_actions();

	if(meter)
		kill_meter(meter, _("Done."));
}

static void gaim_gtk_connection_disconnected(GaimConnection *gc,
		const char *reason)
{
	struct signon_meter *meter = find_signon_meter(gc);

	update_privacy_connections();
	do_away_menu();
	gaim_gtk_blist_update_protocol_actions();

	if(meter)
		kill_meter(meter, _("Done."));

	if (gaim_connections_get_all() != NULL)
		return;

	destroy_all_dialogs();

	gaim_blist_destroy();

	show_login();
}

static void gaim_gtk_connection_notice(GaimConnection *gc,
		const char *text)
{
}

static GaimConnectionUiOps conn_ui_ops =
{
	gaim_gtk_connection_connect_progress,
	gaim_gtk_connection_connected,
	gaim_gtk_connection_disconnected,
	gaim_gtk_connection_notice
};

GaimConnectionUiOps *gaim_get_gtk_connection_ui_ops(void)
{
	return &conn_ui_ops;
}


void away_on_login(const char *mesg)
{
	GSList *awy = away_messages;
	struct away_message *a, *message = NULL;
	struct gaim_gtk_buddy_list *gtkblist;

	gtkblist = GAIM_GTK_BLIST(gaim_get_blist());

	if (!gtkblist->window) {
		return;
	}

	if (mesg == NULL)
		mesg = gaim_prefs_get_string("/core/away/default_message");
	while (awy) {
		a = (struct away_message *)awy->data;
		if (strcmp(a->name, mesg) == 0) {
			message = a;
			break;
		}
		awy = awy->next;
	}
	if (message == NULL) {
		if(!away_messages)
			return;
		message = away_messages->data;
	}
	do_away_message(NULL, message);
}

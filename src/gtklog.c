/**
 * @file gtklog.c GTK+ Log viewer
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2003 Error of Ruto
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

#include "gtkinternal.h"

#include "account.h"
#include "util.h"
#include "gtkblist.h"
#include "gtkimhtml.h"
#include "gtklog.h"
#include "gtkutils.h"
#include "log.h"

static GHashTable *log_viewers = NULL;

struct log_viewer_hash_t {
	char *screenname;
	GaimAccount *account;
};

static guint log_viewer_hash(gconstpointer data)
{
	const struct log_viewer_hash_t *viewer = data;
	return g_str_hash(viewer->screenname) + g_str_hash(gaim_account_get_username(viewer->account));

}

static gint log_viewer_equal(gconstpointer y, gconstpointer z)
{
	const struct log_viewer_hash_t *a, *b;
	int ret;
	char *normal;

	a = y;
	b = z;

	normal = g_strdup(gaim_normalize(a->account, a->screenname));
	ret = (a->account == b->account) &&
		!strcmp(normal, gaim_normalize(b->account, b->screenname));
	g_free(normal);
	return ret;
}

static gboolean destroy_cb(GtkWidget *w, gint resp, struct log_viewer_hash_t *ht) {
	GaimGtkLogViewer *lv = g_hash_table_lookup(log_viewers, ht);

	g_hash_table_remove(log_viewers, ht);
	g_free(ht->screenname);
	g_free(ht);
	for (;lv->logs;lv->logs = lv->logs->next) {
		GaimLog *log = lv->logs->data;
		g_free(log->name);
		g_free(log);
	}
	g_free(lv);		
	gtk_widget_destroy(w);

	return TRUE;
}

static void log_select_cb(GtkTreeSelection *sel, GaimGtkLogViewer *viewer) {
	GtkTreeIter   iter;
	GValue val = { 0, };
	GtkTreeModel *model = GTK_TREE_MODEL(viewer->treestore);
	GaimLog *log = NULL;
	GaimLogReadFlags flags;
	char *read = NULL;
	char time[64];

	char *title;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	log = g_value_get_pointer(&val);
	g_value_unset(&val);

	if (!log)
		return;

	read = gaim_log_read(log, &flags);
	viewer->flags = flags;
	strftime(time, sizeof(time), "%c", localtime(&log->time));
	title = g_strdup_printf("%s - %s", log->name, time);
	gtk_window_set_title(GTK_WINDOW(viewer->window), title);
	gtk_imhtml_clear(GTK_IMHTML(viewer->imhtml));
       	gtk_imhtml_append_text(GTK_IMHTML(viewer->imhtml), read, 
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL | 
			       ((flags & GAIM_LOG_READ_NO_NEWLINE) ? GTK_IMHTML_NO_NEWLINE : 0));
	g_free(read);
	g_free(title);
}

/* I want to make this smarter, but haven't come up with a cool algorithm to do so, yet.
 * I want the tree to be divided into groups like "Today," "Yesterday," "Last week,"
 * "August," "2002," etc. based on how many conversation took place in each subdivision.
 *
 * For now, I'll just make it a flat list.
 */
static void populate_log_tree(GaimGtkLogViewer *lv)
     /* Logs are made from trees in real life. 
        This is a tree made from logs */
{
	char title[64];
	GtkTreeIter iter;
	GList *logs = lv->logs;
	while (logs) {
		GaimLog *log = logs->data;
		strftime(title, sizeof(title), "%c", localtime(&log->time));
		gtk_tree_store_append (lv->treestore, &iter, NULL);
		gtk_tree_store_set(lv->treestore, &iter,
				   0, title,
				   1, log, -1);
		logs = logs->next;
	}
}

void gaim_gtk_log_show(const char *screenname, GaimAccount *account) {
	/* if (log_viewers && g_hash_table */
	GtkWidget *hbox, *vbox;
	GdkPixbuf *pixbuf, *scale;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GaimGtkLogViewer *lv = NULL;
	GtkTreeSelection *sel;
	GtkWidget *icon, *label, *pane, *sw, *button;
	GList *logs;
	char *text;
	struct log_viewer_hash_t *ht = g_new0(struct log_viewer_hash_t, 1);

	ht->screenname = g_strdup(screenname);
	ht->account = account;

	if (!log_viewers) {
		log_viewers = g_hash_table_new(log_viewer_hash, log_viewer_equal);
	} else if ((lv = g_hash_table_lookup(log_viewers, ht))) {
		gtk_window_present(GTK_WINDOW(lv->window));
		return;
	}

	lv = g_new0(GaimGtkLogViewer, 1);
	lv->logs = logs = gaim_log_get_logs(screenname, account);
	g_hash_table_insert(log_viewers, ht, lv);

	/* Window ***********/
	lv->window = gtk_dialog_new_with_buttons(screenname, NULL, 0, 
					     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_container_set_border_width (GTK_CONTAINER(lv->window), 6);
	gtk_dialog_set_has_separator(GTK_DIALOG(lv->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(lv->window)->vbox), 0);
	g_signal_connect(G_OBJECT(lv->window), "response", 
					 G_CALLBACK(destroy_cb), ht);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(lv->window)->vbox), hbox, FALSE, FALSE, 0);

	/* Icon *************/
	pixbuf = create_prpl_icon(account);
	scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
	icon = gtk_image_new_from_pixbuf(scale);
	gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
	g_object_unref(G_OBJECT(pixbuf));
	g_object_unref(G_OBJECT(scale));

	/* Label ************/
	label = gtk_label_new(NULL);
	text = g_strdup_printf("<span size='larger' weight='bold'>%s %s</span>",
			       _("Conversations with"), screenname);
	gtk_label_set_markup(GTK_LABEL(label), text);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	g_free(text);

	/* Pane *************/
	pane = gtk_hpaned_new();
	gtk_container_set_border_width(GTK_CONTAINER(pane), 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(lv->window)->vbox), pane, TRUE, TRUE, 0);
	
	/* List *************/
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_paned_add1(GTK_PANED(pane), sw);
	lv->treestore = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	lv->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (lv->treestore));
	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("time", rend, "markup", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(lv->treeview), col);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (lv->treeview), FALSE);
	gtk_container_add (GTK_CONTAINER (sw), lv->treeview);

	gtk_widget_set_size_request(lv->treeview, 170, 200);
	populate_log_tree(lv);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (lv->treeview));
	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (log_select_cb),
			  lv);
		
	/* Viewer ************/
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_paned_add2(GTK_PANED(pane), vbox);
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	lv->imhtml = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), lv->imhtml);
	gaim_setup_imhtml(lv->imhtml);
	gtk_widget_set_size_request(lv->imhtml, 320, 200);
		
	/* Search box **********/
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	lv->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), lv->entry, TRUE, TRUE, 0);
	button = gtk_button_new_from_stock(GTK_STOCK_FIND);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(lv->window);
}

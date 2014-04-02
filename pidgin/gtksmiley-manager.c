/*
 * pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "http.h"
#include "notify.h"
#include "smiley.h"

#include "gtksmiley-manager.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"

#include "gtk3compat.h"

#define PIDGIN_RESPONSE_MODIFY 1000

#if 0
typedef struct _PidginSmiley PidginSmiley;
struct _PidginSmiley
{
	PurpleSmiley *smiley;
	GtkWidget *parent;
	GtkWidget *smile;
	GtkWidget *smiley_image;
	gchar *filename;
	GdkPixbuf *custom_pixbuf;
	gpointer data;
	gsize datasize;
	gint entry_len;
};
#endif

typedef struct
{
	gchar *filename;

	GtkDialog *window;
	GtkImage *thumbnail;
	GtkEntry *shortcut;
} SmileyEditDialog;

typedef struct
{
	GtkDialog *window;

	GtkWidget *treeview;
	GtkListStore *model;
	PurpleHttpConnection *running_request;
} SmileyManager;

enum
{
	ICON,
	SHORTCUT,
	SMILEY,
	N_COL
};

static SmileyManager *smiley_manager = NULL;

/******************************************************************************
 * New routines (TODO)
 *****************************************************************************/

static void
edit_dialog_destroy(GtkWidget *window, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	g_free(edit_dialog->filename);
	g_free(edit_dialog);
}

static void
edit_dialog_update_thumb(SmileyEditDialog *edit_dialog)
{
	GdkPixbuf *pixbuf = NULL;

	if (edit_dialog->filename) {
		pixbuf = pidgin_pixbuf_new_from_file_at_scale(
			edit_dialog->filename, 64, 64, TRUE);
		if (!pixbuf) {
			g_free(edit_dialog->filename);
			edit_dialog->filename = NULL;
		}
	}
	if (!pixbuf) {
		GtkIconSize icon_size =
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		pixbuf = gtk_widget_render_icon(GTK_WIDGET(edit_dialog->window),
			PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR, icon_size,
			"PidginSmiley");
	}
	g_return_if_fail(pixbuf != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(edit_dialog->thumbnail), pixbuf);

	g_object_unref(G_OBJECT(pixbuf));
}

static void
edit_dialog_update_buttons(SmileyEditDialog *edit_dialog)
{
	gboolean shortcut_ok, image_ok;

	shortcut_ok = (gtk_entry_get_text_length(edit_dialog->shortcut) > 0);
	image_ok = (edit_dialog->filename != NULL);

	gtk_dialog_set_response_sensitive(edit_dialog->window,
		GTK_RESPONSE_ACCEPT, shortcut_ok && image_ok);
}


/******************************************************************************
 * Manager stuff
 *****************************************************************************/

#if 0
static void refresh_list(void);
#endif

/******************************************************************************
 * The Add dialog
 ******************************************************************************/

#if 0
static void do_add(GtkWidget *widget, PidginSmiley *s)
{
	const gchar *entry;
	PurpleSmiley *emoticon;

	entry = gtk_entry_get_text(GTK_ENTRY(s->smile));

	emoticon = purple_smileys_find_by_shortcut(entry);
	if (emoticon && emoticon != s->smiley) {
		gchar *msg;
		msg = g_strdup_printf(_("A custom smiley for '%s' already exists.  "
				"Please use a different shortcut."), entry);
		purple_notify_error(s->parent, _("Custom Smiley"),
				_("Duplicate Shortcut"), msg, NULL);
		g_free(msg);
		return;
	}

	if (s->smiley) {
		if (s->filename) {
			gchar *data = NULL;
			size_t len;
			GError *err = NULL;

			if (!g_file_get_contents(s->filename, &data, &len, &err)) {
				purple_debug_error("gtksmiley", "Error reading %s: %s\n",
						s->filename, err->message);
				g_error_free(err);

				return;
			}
			purple_smiley_set_data(s->smiley, (guchar*)data, len);
		}
		purple_smiley_set_shortcut(s->smiley, entry);
	} else {
		purple_debug_info("gtksmiley", "adding a new smiley\n");

		if (s->filename == NULL) {
			gchar *buffer = NULL;
			gsize size = 0;
			gchar *filename;
			const gchar *dirname = purple_smileys_get_storing_dir();

			/* since this may be called before purple_smiley_new_* has ever been
			 called, we create the storing dir, if it doesn't exist yet, to be
			 able to save the pixbuf before adding the smiley */
			if (!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
				purple_debug_info("gtksmiley", "Creating smileys directory.\n");

				if (g_mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
					purple_debug_error("gtksmiley",
			                   "Unable to create directory %s: %s\n",
			                   dirname, g_strerror(errno));
				}
			}

			if (s->data && s->datasize) {
				/* Cached data & size in memory */
				buffer = s->data;
				size = s->datasize;
			}
			else {
				/* Get the smiley from the custom pixbuf */
				gdk_pixbuf_save_to_buffer(s->custom_pixbuf, &buffer, &size,
					"png", NULL, "compression", "9", NULL, NULL);
			}
			filename = purple_util_get_image_filename(buffer, size);
			s->filename = g_build_filename(dirname, filename, NULL);
			purple_util_write_data_to_file_absolute(s->filename, buffer, size);
			g_free(filename);
			g_free(buffer);
		}
		emoticon = purple_smiley_new_from_file(entry, s->filename);
		if (emoticon)
			pidgin_smiley_add_to_list(emoticon);
	}

	if (smiley_manager != NULL)
		refresh_list();

	gtk_widget_destroy(s->parent);
}
#endif

static void
pidgin_smiley_edit_response(GtkDialog *window, gint response_id,
	gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	switch (response_id) {
#if 0
		case GTK_RESPONSE_ACCEPT:
			do_add(widget, s);
			break;
#endif
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
			break;
		default:
			g_warn_if_reached();
	}
}

static void do_add_file_cb(const char *filename, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	if (!filename)
		return;

	g_free(edit_dialog->filename);
	edit_dialog->filename = g_strdup(filename);

	edit_dialog_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	gtk_widget_grab_focus(GTK_WIDGET(edit_dialog->shortcut));
}

static void
open_image_selector(GtkWidget *widget, gpointer _edit_dialog)
{
	GtkWidget *file_chooser;
	file_chooser = pidgin_buddy_icon_chooser_new(
		GTK_WINDOW(gtk_widget_get_toplevel(widget)),
		do_add_file_cb, _edit_dialog);
	gtk_window_set_title(GTK_WINDOW(file_chooser), _("Custom Smiley"));
	gtk_window_set_role(GTK_WINDOW(file_chooser),
		"file-selector-custom-smiley");
	gtk_widget_show_all(file_chooser);
}

static void
smiley_shortcut_changed(GtkEditable *shortcut, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	edit_dialog_update_buttons(edit_dialog);
}

/* TODO: is <widget> really necessary? */
static void
pidgin_smiley_edit(GtkWidget *widget, PurpleSmiley *smiley)
{
	SmileyEditDialog *edit_dialog;

	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *filech;

	edit_dialog = g_new0(SmileyEditDialog, 1);

	if (smiley) {
		edit_dialog->filename =
			g_strdup(purple_smiley_get_path(smiley));
	}

	edit_dialog->window = GTK_DIALOG(gtk_dialog_new_with_buttons(smiley ? _("Edit Smiley") : _("Add Smiley"),
			widget ? GTK_WINDOW(widget) : NULL,
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			smiley ? GTK_STOCK_SAVE : GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
			NULL));
//	if (smiley)
//		g_object_set_data(G_OBJECT(smiley), "edit-dialog", window);

#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(
		GTK_CONTAINER(edit_dialog->window), PIDGIN_HIG_BORDER);
#endif

	gtk_dialog_set_default_response(
		edit_dialog->window, GTK_RESPONSE_ACCEPT);
	g_signal_connect(edit_dialog->window, "response",
		G_CALLBACK(pidgin_smiley_edit_response), edit_dialog);

	/* The vbox */
#if GTK_CHECK_VERSION(3,0,0)
	vbox = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(vbox), PIDGIN_HIG_BORDER);
#else
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
#endif
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(
		edit_dialog->window)), vbox);
	gtk_widget_show(vbox);

	/* The hbox */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);
	gtk_grid_attach(GTK_GRID(vbox), hbox, 0, 0, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)), hbox);
#endif

	label = gtk_label_new_with_mnemonic(_("_Image:"));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), label, 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
#endif
	gtk_widget_show(label);

	filech = gtk_button_new();
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox), filech, NULL, GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), filech, FALSE, FALSE, 0);
#endif
	pidgin_set_accessible_label(filech, label);

	edit_dialog->thumbnail = GTK_IMAGE(gtk_image_new());
	gtk_container_add(GTK_CONTAINER(filech),
		GTK_WIDGET(edit_dialog->thumbnail));

	g_signal_connect(G_OBJECT(filech), "clicked",
		G_CALLBACK(open_image_selector), edit_dialog);

	gtk_widget_show_all(hbox);

	/* info */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);

	gtk_grid_attach_next_to(GTK_GRID(vbox), hbox, NULL, GTK_POS_BOTTOM, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)),hbox);
#endif

	/* Shortcut text */
	label = gtk_label_new_with_mnemonic(_("S_hortcut text:"));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), label, 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
#endif
	gtk_widget_show(label);

	edit_dialog->shortcut = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_activates_default(edit_dialog->shortcut, TRUE);
	pidgin_set_accessible_label(GTK_WIDGET(edit_dialog->shortcut), label);
	if (smiley) {
		gtk_entry_set_text(edit_dialog->shortcut,
			purple_smiley_get_shortcut(smiley));
	}

	edit_dialog_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	g_signal_connect(edit_dialog->shortcut, "changed",
		G_CALLBACK(smiley_shortcut_changed), edit_dialog);

#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox), GTK_WIDGET(edit_dialog->shortcut), NULL, GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(edit_dialog->shortcut), FALSE, FALSE, 0);
#endif
	gtk_widget_show(GTK_WIDGET(edit_dialog->shortcut));

	gtk_widget_show(hbox);

	gtk_widget_show(GTK_WIDGET(edit_dialog->window));
	g_signal_connect(edit_dialog->window, "destroy",
		G_CALLBACK(edit_dialog_destroy), edit_dialog);
//	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(purple_notify_close_with_handle), s);
}

#if 0
static void
pidgin_smiley_editor_set_shortcut(PidginSmiley *editor, const gchar *shortcut)
{
	gtk_entry_set_text(GTK_ENTRY(editor->smile), shortcut ? shortcut : "");
}
#endif

#if 0
static void
pidgin_smiley_editor_set_image(PidginSmiley *editor, GdkPixbuf *image)
{
	if (editor->custom_pixbuf)
		g_object_unref(G_OBJECT(editor->custom_pixbuf));
	editor->custom_pixbuf = image ? g_object_ref(G_OBJECT(image)) : NULL;
	if (image) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(editor->smiley_image), image);
		if (editor->entry_len > 0)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(editor->parent),
			                                  GTK_RESPONSE_ACCEPT, TRUE);
	}
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(editor->parent),
		                                  GTK_RESPONSE_ACCEPT, FALSE);

	edit_dialog_update_buttons(...);
}
#endif

#if 0
static void
pidgin_smiley_editor_set_data(PidginSmiley *editor, gpointer data, gsize datasize)
{
	editor->data = data;
	editor->datasize = datasize;
}
#endif

/******************************************************************************
 * Delete smiley
 *****************************************************************************/
#if 0
static void delete_foreach(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	PurpleSmiley *smiley = NULL;

	gtk_tree_model_get(model, iter,
			SMILEY, &smiley,
			-1);

	if(smiley != NULL) {
		g_object_unref(G_OBJECT(smiley));
		pidgin_smiley_del_from_list(smiley);
		purple_smiley_delete(smiley);
	}
}
#endif

#if 0
static void append_to_list(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	GList **list = data;
	*list = g_list_prepend(*list, gtk_tree_path_copy(path));
}
#endif

#if 0
static void smiley_delete(SmileyManager *dialog)
{
	GtkTreeSelection *selection;
	GList *list = NULL;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));
	gtk_tree_selection_selected_foreach(selection, delete_foreach, dialog);
	gtk_tree_selection_selected_foreach(selection, append_to_list, &list);

	while (list) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, list->data))
			gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &iter);
		gtk_tree_path_free(list->data);
		list = g_list_delete_link(list, list);
	}
}
#endif
/******************************************************************************
 * The Smiley Manager
 *****************************************************************************/
#if 0
static void add_columns(GtkWidget *treeview, SmileyManager *dialog)
{
	GtkCellRenderer *rend;
	GtkTreeViewColumn *column;

	/* Icon */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Smiley"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_add_attribute(column, rend, "pixbuf", ICON);

	/* Shortcut Text */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Shortcut Text"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, rend, TRUE);
	gtk_tree_view_column_add_attribute(column, rend, "text", SHORTCUT);
}
#endif

#if 0
static void store_smiley_add(PurpleSmiley *smiley)
{
	GtkTreeIter iter;
	PurpleStoredImage *img;
	GdkPixbuf *sized_smiley = NULL;

	if (smiley_manager == NULL)
		return;

	img = purple_smiley_get_stored_image(smiley);

	if (img != NULL) {
		GdkPixbuf *smiley_image = pidgin_pixbuf_from_imgstore(img);
		purple_imgstore_unref(img);

		if (smiley_image != NULL) {
			if (gdk_pixbuf_get_width(smiley_image) > 22 ||
				gdk_pixbuf_get_height(smiley_image) > 22) {
				sized_smiley = gdk_pixbuf_scale_simple(smiley_image,
					22, 22, GDK_INTERP_HYPER);
				g_object_unref(G_OBJECT(smiley_image));
			} else {
				/* don't scale up smaller smileys, avoid blurryness */
				sized_smiley = smiley_image;
			}
		}
	}


	gtk_list_store_append(smiley_manager->model, &iter);

	gtk_list_store_set(smiley_manager->model, &iter,
			ICON, sized_smiley,
			SHORTCUT, purple_smiley_get_shortcut(smiley),
			SMILEY, smiley,
			-1);

	if (sized_smiley != NULL)
		g_object_unref(G_OBJECT(sized_smiley));
}
#endif

#if 0
static void populate_smiley_list(SmileyManager *dialog)
{
	GList *list;
	PurpleSmiley *emoticon;

	gtk_list_store_clear(dialog->model);

	for(list = purple_smileys_get_all(); list != NULL;
			list = g_list_delete_link(list, list)) {
		emoticon = (PurpleSmiley*)list->data;

		store_smiley_add(emoticon);
	}
}
#endif

#if 0
static void smile_selected_cb(GtkTreeSelection *sel, SmileyManager *dialog)
{
	gint selected;

	selected = gtk_tree_selection_count_selected_rows(sel);

	gtk_dialog_set_response_sensitive(dialog->window,
			GTK_RESPONSE_NO, selected > 0);

	gtk_dialog_set_response_sensitive(dialog->window,
	                                  PIDGIN_RESPONSE_MODIFY, selected > 0);
}
#endif

#if 0
static void
smiley_edit_iter(SmileyManager *dialog, GtkTreeIter *iter)
{
	PurpleSmiley *smiley = NULL;
	GtkWidget *window = NULL;
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), iter, SMILEY, &smiley, -1);
	if ((window = g_object_get_data(G_OBJECT(smiley), "edit-dialog")) != NULL)
		gtk_window_present(GTK_WINDOW(window));
	else
		pidgin_smiley_edit(gtk_widget_get_toplevel(GTK_WIDGET(dialog->treeview)), smiley);
	g_object_unref(G_OBJECT(smiley));
}
#endif

#if 0
static void smiley_edit_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
	GtkTreeIter iter;
	SmileyManager *dialog = data;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	smiley_edit_iter(dialog, &iter);
}
#endif

#if 0
static void
edit_selected_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	smiley_edit_iter(data, iter);
}
#endif

#if 0
static void
smiley_got_url(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _dialog)
{
	SmileyManager *dialog = _dialog;
	PidginSmiley *ps;
	GdkPixbuf *image;
	const gchar *smileydata;
	size_t len;

	g_assert(http_conn == smiley_manager->running_request);
	smiley_manager->running_request = NULL;

	if (!purple_http_response_is_successful(response))
		return;

	smileydata = purple_http_response_get_data(response, &len);
	image = pidgin_pixbuf_from_data((const guchar *)smileydata, len);
	if (!image)
		return;

	ps = pidgin_smiley_edit(GTK_WIDGET(dialog->window), NULL);
	pidgin_smiley_editor_set_image(ps, image);
	pidgin_smiley_editor_set_data(ps, g_memdup(smileydata, len), len);
}
#endif

#if 0
static void
smiley_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer user_data)
{
	SmileyManager *dialog = user_data;
	gchar *name = g_strchomp((gchar *) gtk_selection_data_get_data(sd));

	if ((gtk_selection_data_get_length(sd) >= 0)
      && (gtk_selection_data_get_format(sd) == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			PidginSmiley *ps;
			/* It looks like we're dealing with a local file. Let's
			 * just try and read it */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug_error("smiley dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				return;
			}
			ps = pidgin_smiley_edit(GTK_WIDGET(dialog->window), NULL);
			do_add_file_cb(tmp, ps);
			if (gtk_image_get_pixbuf(GTK_IMAGE(ps->smiley_image)) == NULL)
				gtk_dialog_response(GTK_DIALOG(ps->parent), GTK_RESPONSE_CANCEL);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7) ||
			!g_ascii_strncasecmp(name, "https://", 8))
		{
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			purple_http_conn_cancel(smiley_manager->
				running_request);
			smiley_manager->running_request = purple_http_get(NULL,
				smiley_got_url, dialog, name);
		}

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}
#endif

#if 0
static GtkWidget *smiley_list_create(SmileyManager *dialog)
{
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTargetEntry te[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};

	/* Create the list model */
	dialog->model = gtk_list_store_new(N_COL,
			GDK_TYPE_PIXBUF,	/* ICON */
			G_TYPE_STRING,		/* SHORTCUT */
			G_TYPE_OBJECT		/* SMILEY */
			);

	/* the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->model), SHORTCUT, GTK_SORT_ASCENDING);
	g_object_unref(G_OBJECT(dialog->model));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(smile_selected_cb), dialog);
	g_signal_connect(G_OBJECT(treeview), "row_activated", G_CALLBACK(smiley_edit_cb), dialog);

	gtk_drag_dest_set(treeview,
	                  GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
	                  te, G_N_ELEMENTS(te), GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(treeview), "drag_data_received", G_CALLBACK(smiley_dnd_recv), dialog);

	gtk_widget_show(treeview);

	add_columns(treeview, dialog);
	populate_smiley_list(dialog);

	return pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1);
}
#endif

#if 0
static void refresh_list()
{
	populate_smiley_list(smiley_manager);
}
#endif

static void
smiley_manager_select_cb(GtkWidget *widget, gint resp, SmileyManager *dialog)
{
#if 0
	GtkTreeSelection *selection = NULL;
#endif

	switch (resp) {
		case GTK_RESPONSE_YES:
			pidgin_smiley_edit(GTK_WIDGET(dialog->window), NULL);
			break;
#if 0
		case GTK_RESPONSE_NO:
			smiley_delete(dialog);
			break;
#endif
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(GTK_WIDGET(dialog->window));
			purple_http_conn_cancel(smiley_manager->running_request);
			g_free(smiley_manager);
			smiley_manager = NULL;
			break;
#if 0
		case PIDGIN_RESPONSE_MODIFY:
			/* Find smiley of selection... */
			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));
			gtk_tree_selection_selected_foreach(selection, edit_selected_cb, dialog);
			break;
#endif
		default:
			purple_debug_info("gtksmiley", "No valid selection\n");
			break;
	}
}

void
pidgin_smiley_manager_show(void)
{
	SmileyManager *dialog;
	GtkDialog *win;
#if 0
	GtkWidget *sw;
#endif
	GtkWidget *vbox;

	if (smiley_manager) {
		gtk_window_present(GTK_WINDOW(smiley_manager->window));
		return;
	}

	dialog = g_new0(SmileyManager, 1);
	smiley_manager = dialog;

	dialog->window = win = GTK_DIALOG(gtk_dialog_new_with_buttons(
		_("Custom Smiley Manager"), NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		PIDGIN_STOCK_ADD, GTK_RESPONSE_YES,
		PIDGIN_STOCK_MODIFY, PIDGIN_RESPONSE_MODIFY,
		GTK_STOCK_DELETE, GTK_RESPONSE_NO,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL));

	gtk_window_set_default_size(GTK_WINDOW(win), 50, 400);
	gtk_window_set_role(GTK_WINDOW(win), "custom_smiley_manager");
#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(GTK_CONTAINER(win),PIDGIN_HIG_BORDER);
#endif
	gtk_dialog_set_response_sensitive(win, GTK_RESPONSE_NO, FALSE);
	gtk_dialog_set_response_sensitive(win, PIDGIN_RESPONSE_MODIFY, FALSE);

	g_signal_connect(win, "response",
		G_CALLBACK(smiley_manager_select_cb), dialog);

	/* The vbox */
	vbox = gtk_dialog_get_content_area(win);

#if 0
	/* get the scrolled window with all stuff */
	sw = smiley_list_create(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);
#endif

	gtk_widget_show(GTK_WIDGET(win));
}

/**
 * @file gtkprefs.c GTK+ Preferences
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
 *
 */
#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sound.h"
#include "util.h"
#include "network.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkimhtmltoolbar.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksound.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "gaimstock.h"

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYUSER 2
#define PROXYPASS 3

static int sound_row_sel = 0;
static GtkWidget *prefsnotebook;

static GtkWidget *sound_entry = NULL;
static GtkListStore *smiley_theme_store = NULL;
static GtkWidget *prefs_proxy_frame = NULL;

static GtkWidget *prefs = NULL;
static GtkWidget *debugbutton = NULL;
static int notebook_page = 0;
static GtkTreeRowReference *previous_smiley_row = NULL;

/*
 * PROTOTYPES
 */
static void delete_prefs(GtkWidget *, void *);

static void
update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	const char *key = g_object_get_data(G_OBJECT(spin), "val");
	int value;

	value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));

	gaim_prefs_set_int(key, value);
}

GtkWidget *
pidgin_prefs_labeled_spin_button(GtkWidget *box, const gchar *title,
		const char *key, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *spin;
	GtkObject *adjust;
	int val;

	val = gaim_prefs_get_int(key);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	adjust = gtk_adjustment_new(val, min, max, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	if (max < 10000)
		gtk_widget_set_size_request(spin, 50, -1);
	else
		gtk_widget_set_size_request(spin, 60, -1);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
					 G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

	if (sg) {
		gtk_size_group_add_widget(sg, label);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	}

	pidgin_set_accessible_label (spin, label);

	return hbox;
}

static void
entry_set(GtkEntry *entry, gpointer data) {
	const char *key = (const char*)data;

	gaim_prefs_set_string(key, gtk_entry_get_text(entry));
}

GtkWidget *
pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *hbox, *label, *entry;
	const gchar *value;

	value = gaim_prefs_get_string(key);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(page), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

	if(sg) {
		gtk_size_group_add_widget(sg, label);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	}

	pidgin_set_accessible_label(entry, label);

	return hbox;
}

static void
dropdown_set(GObject *w, const char *key)
{
	const char *str_value;
	int int_value;
	GaimPrefType type;

	type = GPOINTER_TO_INT(g_object_get_data(w, "type"));

	if (type == GAIM_PREF_INT) {
		int_value = GPOINTER_TO_INT(g_object_get_data(w, "value"));

		gaim_prefs_set_int(key, int_value);
	}
	else if (type == GAIM_PREF_STRING) {
		str_value = (const char *)g_object_get_data(w, "value");

		gaim_prefs_set_string(key, str_value);
	}
	else if (type == GAIM_PREF_BOOLEAN) {
		gaim_prefs_set_bool(key,
				GPOINTER_TO_INT(g_object_get_data(w, "value")));
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		GaimPrefType type, const char *key, GList *menuitems)
{
	GtkWidget  *dropdown, *opt, *menu;
	GtkWidget  *label = NULL;
	GtkWidget  *hbox;
	gchar      *text;
	const char *stored_str = NULL;
	int         stored_int = 0;
	int         int_value  = 0;
	const char *str_value  = NULL;
	int         o = 0;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (title != NULL) {
		hbox = gtk_hbox_new(FALSE, 5);
		/*gtk_container_add (GTK_CONTAINER (box), hbox);*/
		gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);

		label = gtk_label_new_with_mnemonic(title);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
	} else {
		hbox = box;
	}

#if 0 /* GTK_CHECK_VERSION(2,4,0) */
	if(type == GAIM_PREF_INT)
		model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	else if(type == GAIM_PREF_STRING)
		model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	dropdown = gtk_combo_box_new_with_model(model);
#else
	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();
#endif

	if (label != NULL) {
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), dropdown);
		pidgin_set_accessible_label (dropdown, label);
	}

	if (type == GAIM_PREF_INT)
		stored_int = gaim_prefs_get_int(key);
	else if (type == GAIM_PREF_STRING)
		stored_str = gaim_prefs_get_string(key);

	while (menuitems != NULL && (text = (char *) menuitems->data) != NULL) {
		menuitems = g_list_next(menuitems);
		g_return_val_if_fail(menuitems != NULL, NULL);

		opt = gtk_menu_item_new_with_label(text);

		g_object_set_data(G_OBJECT(opt), "type", GINT_TO_POINTER(type));

		if (type == GAIM_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			g_object_set_data(G_OBJECT(opt), "value",
							  GINT_TO_POINTER(int_value));
		}
		else if (type == GAIM_PREF_STRING) {
			str_value = (const char *)menuitems->data;

			g_object_set_data(G_OBJECT(opt), "value", (char *)str_value);
		}
		else if (type == GAIM_PREF_BOOLEAN) {
			g_object_set_data(G_OBJECT(opt), "value",
					menuitems->data);
		}

		g_signal_connect(G_OBJECT(opt), "activate",
						 G_CALLBACK(dropdown_set), (char *)key);

		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);

		if ((type == GAIM_PREF_INT && stored_int == int_value) ||
			(type == GAIM_PREF_STRING && stored_str != NULL &&
			 !strcmp(stored_str, str_value)) ||
			(type == GAIM_PREF_BOOLEAN &&
			 (gaim_prefs_get_bool(key) == GPOINTER_TO_INT(menuitems->data)))) {

			gtk_menu_set_active(GTK_MENU(menu), o);
		}

		menuitems = g_list_next(menuitems);

		o++;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);
	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);
	gtk_widget_show(dropdown);

	return label;
}

GtkWidget *
pidgin_prefs_dropdown(GtkWidget *box, const gchar *title, GaimPrefType type,
			   const char *key, ...)
{
	va_list ap;
	GList *menuitems = NULL;
	GtkWidget *dropdown = NULL;
	char *name;
	int int_value;
	const char *str_value;

	g_return_val_if_fail(type == GAIM_PREF_BOOLEAN || type == GAIM_PREF_INT ||
			type == GAIM_PREF_STRING, NULL);

	va_start(ap, key);
	while ((name = va_arg(ap, char *)) != NULL) {

		menuitems = g_list_prepend(menuitems, name);

		if (type == GAIM_PREF_INT || type == GAIM_PREF_BOOLEAN) {
			int_value = va_arg(ap, int);
			menuitems = g_list_prepend(menuitems, GINT_TO_POINTER(int_value));
		}
		else {
			str_value = va_arg(ap, const char *);
			menuitems = g_list_prepend(menuitems, (char *)str_value);
		}
	}
	va_end(ap);

	g_return_val_if_fail(menuitems != NULL, NULL);

	menuitems = g_list_reverse(menuitems);

	dropdown = pidgin_prefs_dropdown_from_list(box, title, type, key,
			menuitems);

	g_list_free(menuitems);

	return dropdown;
}

static void
delete_prefs(GtkWidget *asdf, void *gdsa)
{
	/* Close any "select sound" request dialogs */
	gaim_request_close_with_handle(prefs);

	/* Unregister callbacks. */
	gaim_prefs_disconnect_by_handle(prefs);

	prefs = NULL;
	sound_entry = NULL;
	debugbutton = NULL;
	notebook_page = 0;
	smiley_theme_store = NULL;
	if (previous_smiley_row)
		gtk_tree_row_reference_free(previous_smiley_row);
	previous_smiley_row = NULL;

}

static void smiley_sel(GtkTreeSelection *sel, GtkTreeModel *model) {
	GtkTreeIter  iter;
	const char *themename;
	char *description;
	GValue val;
	GtkTreePath *path, *oldpath;
	struct smiley_theme *new_theme, *old_theme;

	if (!gtk_tree_selection_get_selected(sel, &model, &iter))
		return;

	old_theme = current_smiley_theme;
	val.g_type = 0;
	gtk_tree_model_get_value(model, &iter, 3, &val);
	path = gtk_tree_model_get_path(model, &iter);
	themename = g_value_get_string(&val);
	gaim_prefs_set_string("/gaim/gtk/smileys/theme", themename);
	g_value_unset (&val);

	/* current_smiley_theme is set in callback for the above pref change */
	new_theme = current_smiley_theme;
	description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
								"<span size='smaller' foreground='white'>%s</span>",
								new_theme->name, new_theme->author, new_theme->desc);
	gtk_list_store_set(smiley_theme_store, &iter, 1, description, -1);
	g_free(description);

	if (new_theme != old_theme && previous_smiley_row) {
		oldpath = gtk_tree_row_reference_get_path(previous_smiley_row);
		if (gtk_tree_model_get_iter(model, &iter, oldpath)) {
			description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
								"<span size='smaller' foreground='dim grey'>%s</span>",
								old_theme->name, old_theme->author, old_theme->desc);
			gtk_list_store_set(smiley_theme_store, &iter, 1,
				description, -1);
			g_free(description);
		}
		gtk_tree_path_free(oldpath);
	}
	if (previous_smiley_row)
		gtk_tree_row_reference_free(previous_smiley_row);
	previous_smiley_row = gtk_tree_row_reference_new(model, path);
	gtk_tree_path_free(path);
}

static GtkTreeRowReference *theme_refresh_theme_list()
{
	GdkPixbuf *pixbuf;
	GSList *themes;
	GtkTreeIter iter;
	GtkTreeRowReference *row_ref = NULL;

	if (previous_smiley_row)
		gtk_tree_row_reference_free(previous_smiley_row);
	previous_smiley_row = NULL;

	pidginthemes_smiley_theme_probe();

	if (!(themes = smiley_themes))
		return NULL;

	gtk_list_store_clear(smiley_theme_store);

	while (themes) {
		struct smiley_theme *theme = themes->data;
		char *description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
						    "<span size='smaller' foreground='dim grey'>%s</span>",
						    theme->name, theme->author, theme->desc);
		gtk_list_store_append (smiley_theme_store, &iter);

		/*
		 * LEAK - Gentoo memprof thinks pixbuf is leaking here... but it
		 * looks like it should be ok to me.  Anyone know what's up?  --Mark
		 */
		pixbuf = (theme->icon ? gdk_pixbuf_new_from_file(theme->icon, NULL) : NULL);

		gtk_list_store_set(smiley_theme_store, &iter,
				   0, pixbuf,
				   1, description,
				   2, theme->path,
				   3, theme->name,
				   -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

		g_free(description);
		themes = themes->next;

		/* If this is the currently selected theme,
		 * we will need to select it. Grab the row reference. */
		if (theme == current_smiley_theme) {
			GtkTreePath *path = gtk_tree_model_get_path(
				GTK_TREE_MODEL(smiley_theme_store), &iter);
			row_ref = gtk_tree_row_reference_new(
				GTK_TREE_MODEL(smiley_theme_store), path);
			gtk_tree_path_free(path);
		}
	}

	return row_ref;
}

static void theme_install_theme(char *path, char *extn) {
#ifndef _WIN32
	gchar *command;
#endif
	gchar *destdir;
	gchar *tail;
	GtkTreeRowReference *theme_rowref;

	/* Just to be safe */
	g_strchomp(path);

	/* I dont know what you are, get out of here */
	if (extn != NULL)
		tail = extn;
	else if ((tail = strrchr(path, '.')) == NULL)
		return;

	destdir = g_strconcat(gaim_user_dir(), G_DIR_SEPARATOR_S "smileys", NULL);

	/* We'll check this just to make sure. This also lets us do something different on
	 * other platforms, if need be */
	if (!g_ascii_strcasecmp(tail, ".gz") || !g_ascii_strcasecmp(tail, ".tgz")) {
#ifndef _WIN32
		gchar *path_escaped = g_shell_quote(path);
		gchar *destdir_escaped = g_shell_quote(destdir);
		command = g_strdup_printf("tar > /dev/null xzf %s -C %s", path_escaped, destdir_escaped);
		g_free(path_escaped);
		g_free(destdir_escaped);
#else
		if(!winpidgin_gz_untar(path, destdir)) {
			g_free(destdir);
			return;
		}
#endif
	}
	else {
		g_free(destdir);
		return;
	}

#ifndef _WIN32
	/* Fire! */
	if (system(command))
	{
		gaim_notify_error(NULL, NULL, _("Smiley theme failed to unpack."), NULL);
	}

	g_free(command);
#endif
	g_free(destdir);

	theme_rowref = theme_refresh_theme_list();
	if (theme_rowref != NULL)
		gtk_tree_row_reference_free(theme_rowref);
}

static void
theme_got_url(GaimUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *themedata, size_t len, const gchar *error_message)
{
	FILE *f;
	gchar *path;

	if ((error_message != NULL) || (len == 0))
		return;

	f = gaim_mkstemp(&path, TRUE);
	fwrite(themedata, len, 1, f);
	fclose(f);

	theme_install_theme(path, user_data);

	g_unlink(path);
	g_free(path);
}

static void
theme_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer data)
{
	gchar *name = (gchar *)sd->data;

	if ((sd->length >= 0) && (sd->format == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			/* It looks like we're dealing with a local file. Let's
			 * just untar it in the right place */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				gaim_debug(GAIM_DEBUG_ERROR, "theme dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				return;
			}
			theme_install_theme(tmp, NULL);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7)) {
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			gaim_util_fetch_url(name, TRUE, NULL, FALSE, theme_got_url, ".tgz");
		} else if (!g_ascii_strncasecmp(name, "https://", 8)) {
			/* gaim_util_fetch_url() doesn't support HTTPS, but we want users
			 * to be able to drag and drop links from the SF trackers, so
			 * we'll try it as an HTTP URL. */
			char *tmp = g_strdup(name + 1);
			tmp[0] = 'h';
			tmp[1] = 't';
			tmp[2] = 't';
			tmp[3] = 'p';
			gaim_util_fetch_url(tmp, TRUE, NULL, FALSE, theme_got_url, ".tgz");
			g_free(tmp);
		}

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}

/* Does same as normal sort, except "none" is sorted first */
static gint gaim_sort_smileys (GtkTreeModel	*model,
						GtkTreeIter		*a,
						GtkTreeIter		*b,
						gpointer		userdata)
{
	gint ret = 0;
	gchar *name1 = NULL, *name2 = NULL;

	gtk_tree_model_get(model, a, 3, &name1, -1);
	gtk_tree_model_get(model, b, 3, &name2, -1);

	if (name1 == NULL || name2 == NULL) {
		if (!(name1 == NULL && name2 == NULL))
			ret = (name1 == NULL) ? -1: 1;
	} else if (!g_ascii_strcasecmp(name1, "none")) {
		if (!g_utf8_collate(name1, name2))
			ret = 0;
		else
			/* Sort name1 first */
			ret = -1;
	} else if (!g_ascii_strcasecmp(name2, "none")) {
		/* Sort name2 first */
		ret = 1;
	} else {
		/* Neither string is "none", default to normal sort */
		ret = g_utf8_collate(name1,name2);
	}

	g_free(name1);
	g_free(name2);

	return ret;
}

static GtkWidget *
theme_page()
{
	GtkWidget *ret;
	GtkWidget *sw;
	GtkWidget *view;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreeRowReference *rowref;
	GtkWidget *label;
	GtkTargetEntry te[3] = {{"text/plain", 0, 0},{"text/uri-list", 0, 1},{"STRING", 0, 2}};

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);

	label = gtk_label_new(_("Select a smiley theme that you would like to use from the list below. New themes can be installed by dragging and dropping them onto the theme list."));

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);
	smiley_theme_store = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	rowref = theme_refresh_theme_list();

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(smiley_theme_store));

	gtk_drag_dest_set(view, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, te,
					sizeof(te) / sizeof(GtkTargetEntry) , GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(view), "drag_data_received", G_CALLBACK(theme_dnd_recv), smiley_theme_store);

	rend = gtk_cell_renderer_pixbuf_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	/* Custom sort so "none" theme is at top of list */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(smiley_theme_store),
									3, gaim_sort_smileys, NULL, NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(smiley_theme_store),
										 3, GTK_SORT_ASCENDING);

	col = gtk_tree_view_column_new_with_attributes (_("Icon"),
							rend,
							"pixbuf", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes (_("Description"),
							rend,
							"markup", 1,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	g_object_unref(G_OBJECT(smiley_theme_store));
	gtk_container_add(GTK_CONTAINER(sw), view);

	g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(smiley_sel), NULL);

	if (rowref) {
		GtkTreePath *path = gtk_tree_row_reference_get_path(rowref);
		gtk_tree_row_reference_free(rowref);
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
	}

	gtk_widget_show_all(ret);

	pidgin_set_accessible_label (view, label);

	return ret;
}

static void
formatting_toggle_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, void *toolbar)
{
	gboolean bold, italic, uline;

	gtk_imhtml_get_current_format(GTK_IMHTML(imhtml),
								  &bold, &italic, &uline);

	if (buttons & GTK_IMHTML_BOLD)
		gaim_prefs_set_bool("/gaim/gtk/conversations/send_bold", bold);
	if (buttons & GTK_IMHTML_ITALIC)
		gaim_prefs_set_bool("/gaim/gtk/conversations/send_italic", italic);
	if (buttons & GTK_IMHTML_UNDERLINE)
		gaim_prefs_set_bool("/gaim/gtk/conversations/send_underline", uline);

	if (buttons & GTK_IMHTML_GROW || buttons & GTK_IMHTML_SHRINK)
		gaim_prefs_set_int("/gaim/gtk/conversations/font_size",
						   gtk_imhtml_get_current_fontsize(GTK_IMHTML(imhtml)));
	if (buttons & GTK_IMHTML_FACE) {
		char *face = gtk_imhtml_get_current_fontface(GTK_IMHTML(imhtml));
		if (!face)
			face = g_strdup("");

		gaim_prefs_set_string("/gaim/gtk/conversations/font_face", face);
		g_free(face);
	}

	if (buttons & GTK_IMHTML_FORECOLOR) {
		char *color = gtk_imhtml_get_current_forecolor(GTK_IMHTML(imhtml));
		if (!color)
			color = g_strdup("");

		gaim_prefs_set_string("/gaim/gtk/conversations/fgcolor", color);
		g_free(color);
	}

	if (buttons & GTK_IMHTML_BACKCOLOR) {
		char *color;
		GObject *object;

		color = gtk_imhtml_get_current_backcolor(GTK_IMHTML(imhtml));
		if (!color)
			color = g_strdup("");

		/* Block the signal to prevent a loop. */
		object = g_object_ref(G_OBJECT(imhtml));
		g_signal_handlers_block_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
										NULL, toolbar);
		/* Clear the backcolor. */
		gtk_imhtml_toggle_backcolor(GTK_IMHTML(imhtml), "");
		/* Unblock the signal. */
		g_signal_handlers_unblock_matched(object, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
										  NULL, toolbar);
		g_object_unref(object);

		/* This will fire a toggle signal and get saved below. */
		gtk_imhtml_toggle_background(GTK_IMHTML(imhtml), color);

		g_free(color);
	}

	if (buttons & GTK_IMHTML_BACKGROUND) {
		char *color = gtk_imhtml_get_current_background(GTK_IMHTML(imhtml));
		if (!color)
			color = g_strdup("");

		gaim_prefs_set_string("/gaim/gtk/conversations/bgcolor", color);
		g_free(color);
	}
}

static void
formatting_clear_cb(GtkIMHtml *imhtml, void *data)
{
	gaim_prefs_set_bool("/gaim/gtk/conversations/send_bold", FALSE);
	gaim_prefs_set_bool("/gaim/gtk/conversations/send_italic", FALSE);
	gaim_prefs_set_bool("/gaim/gtk/conversations/send_underline", FALSE);

	gaim_prefs_set_int("/gaim/gtk/conversations/font_size", 3);

	gaim_prefs_set_string("/gaim/gtk/conversations/font_face", "");
	gaim_prefs_set_string("/gaim/gtk/conversations/fgcolor", "");
	gaim_prefs_set_string("/gaim/gtk/conversations/bgcolor", "");
}

static void
conversation_usetabs_cb(const char *name, GaimPrefType type,
						gconstpointer value, gpointer data)
{
	gboolean usetabs = GPOINTER_TO_INT(value);

	if (usetabs)
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
}

static GtkWidget *
interface_page()
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkSizeGroup *sg;
	GList *names = NULL;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), GAIM_HIG_BORDER);
	
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = pidgin_make_frame(ret, _("System Tray Icon"));
	label = pidgin_prefs_dropdown(vbox, _("_Show system tray icon:"), GAIM_PREF_STRING,
					"/gaim/gtk/docklet/show",
					_("Always"), "always",
					_("Never"), "never",
					_("On unread messages"), "pending",
					NULL);
	gtk_size_group_add_widget(sg, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	
	vbox = pidgin_make_frame(ret, _("Conversation Window Hiding"));
	label = pidgin_prefs_dropdown(vbox, _("_Hide new IM conversations:"),
					GAIM_PREF_STRING, "/gaim/gtk/conversations/im/hide_new",
					_("Never"), "never",
					_("When away"), "away",
					_("Always"), "always",
					NULL);
	gtk_size_group_add_widget(sg, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
			

	/* All the tab options! */
	vbox = pidgin_make_frame(ret, _("Tabs"));
	
	pidgin_prefs_checkbox(_("Show IMs and chats in _tabbed windows"),
							"/gaim/gtk/conversations/tabs", vbox);

	/*
	 * Connect a signal to the above preference.  When conversations are not
	 * shown in a tabbed window then all tabbing options should be disabled.
	 */
	vbox2 = gtk_vbox_new(FALSE, 9);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);
	gaim_prefs_connect_callback(prefs, "/gaim/gtk/conversations/tabs",
	                            conversation_usetabs_cb, vbox2);
	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/tabs"))
	  gtk_widget_set_sensitive(vbox2, FALSE);

	pidgin_prefs_checkbox(_("Show close b_utton on tabs"),
				"/gaim/gtk/conversations/close_on_tabs", vbox2);

	label = pidgin_prefs_dropdown(vbox2, _("_Placement:"), GAIM_PREF_INT,
					"/gaim/gtk/conversations/tab_side",
					_("Top"), GTK_POS_TOP,
					_("Bottom"), GTK_POS_BOTTOM,
					_("Left"), GTK_POS_LEFT,
					_("Right"), GTK_POS_RIGHT,
#if GTK_CHECK_VERSION(2,6,0)
					_("Left Vertical"), GTK_POS_LEFT|8,
					_("Right Vertical"), GTK_POS_RIGHT|8,
#endif
					NULL);
	gtk_size_group_add_widget(sg, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		
	names = pidgin_conv_placement_get_options();
	label = pidgin_prefs_dropdown_from_list(vbox2, _("N_ew conversations:"),
				GAIM_PREF_STRING, "/gaim/gtk/conversations/placement", names);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
			
	gtk_size_group_add_widget(sg, label);

	g_list_free(names);

	gtk_widget_show_all(ret);
	return ret;
}

static GtkWidget *
conv_page()
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *iconpref1;
	GtkWidget *iconpref2;
	GtkWidget *imhtml;
	GtkWidget *frame;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), GAIM_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Conversations"));

	pidgin_prefs_checkbox(_("Show _formatting on incoming messages"),
				"/gaim/gtk/conversations/show_incoming_formatting", vbox);

	iconpref1 = pidgin_prefs_checkbox(_("Show buddy _icons"),
			"/gaim/gtk/conversations/im/show_buddy_icons", vbox);
	iconpref2 = pidgin_prefs_checkbox(_("Enable buddy ic_on animation"),
			"/gaim/gtk/conversations/im/animate_buddy_icons", vbox);
	if (!gaim_prefs_get_bool("/gaim/gtk/conversations/im/show_buddy_icons"))
		gtk_widget_set_sensitive(iconpref2, FALSE);
	g_signal_connect(G_OBJECT(iconpref1), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), iconpref2);

	pidgin_prefs_checkbox(_("_Notify buddies that you are typing to them"),
			"/core/conversations/im/send_typing", vbox);
#ifdef USE_GTKSPELL
	pidgin_prefs_checkbox(_("Highlight _misspelled words"),
			"/gaim/gtk/conversations/spellcheck", vbox);
#endif

	pidgin_prefs_checkbox(_("Use smooth-scrolling"), "/gaim/gtk/conversations/use_smooth_scrolling", vbox);

#ifdef _WIN32
	pidgin_prefs_checkbox(_("F_lash window when IMs are received"), "/gaim/gtk/win32/blink_im", vbox);
#endif

	vbox = pidgin_make_frame(ret, _("Default Formatting"));

	frame = pidgin_create_imhtml(TRUE, &imhtml, &toolbar, NULL);
	gtk_widget_set_name(imhtml, "pidginprefs_font_imhtml");
	gtk_imhtml_set_whole_buffer_formatting_only(GTK_IMHTML(imhtml), TRUE);
	gtk_imhtml_set_format_functions(GTK_IMHTML(imhtml),
									GTK_IMHTML_BOLD |
									GTK_IMHTML_ITALIC |
									GTK_IMHTML_UNDERLINE |
									GTK_IMHTML_GROW |
									GTK_IMHTML_SHRINK |
									GTK_IMHTML_FACE |
									GTK_IMHTML_FORECOLOR |
									GTK_IMHTML_BACKCOLOR |
									GTK_IMHTML_BACKGROUND);

	gtk_imhtml_append_text(GTK_IMHTML(imhtml), _("This is how your outgoing message text will appear when you use protocols that support formatting. :)"), 0);

	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_bold"))
		gtk_imhtml_toggle_bold(GTK_IMHTML(imhtml));
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_italic"))
		gtk_imhtml_toggle_italic(GTK_IMHTML(imhtml));
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/send_underline"))
		gtk_imhtml_toggle_underline(GTK_IMHTML(imhtml));

	gtk_imhtml_font_set_size(GTK_IMHTML(imhtml), gaim_prefs_get_int("/gaim/gtk/conversations/font_size"));
	gtk_imhtml_toggle_forecolor(GTK_IMHTML(imhtml), gaim_prefs_get_string("/gaim/gtk/conversations/fgcolor"));
	gtk_imhtml_toggle_background(GTK_IMHTML(imhtml), gaim_prefs_get_string("/gaim/gtk/conversations/bgcolor"));
	gtk_imhtml_toggle_fontface(GTK_IMHTML(imhtml), gaim_prefs_get_string("/gaim/gtk/conversations/font_face"));

	g_signal_connect_after(G_OBJECT(imhtml), "format_function_toggle",
					 G_CALLBACK(formatting_toggle_cb), toolbar);
	g_signal_connect_after(G_OBJECT(imhtml), "format_function_clear",
					 G_CALLBACK(formatting_clear_cb), NULL);


	gtk_widget_show_all(ret);

	return ret;
}

static void network_ip_changed(GtkEntry *entry, gpointer data)
{
	/*
	 * TODO: It would be nice if we could validate this and show a
	 *       red background in the box when the IP address is invalid
	 *       and a green background when the IP address is valid.
	 */
	gaim_network_set_public_ip(gtk_entry_get_text(entry));
}

static void
proxy_changed_cb(const char *name, GaimPrefType type,
				 gconstpointer value, gpointer data)
{
	GtkWidget *frame = data;
	const char *proxy = value;

	if (strcmp(proxy, "none") && strcmp(proxy, "envvar"))
	        gtk_widget_show_all(frame);
	else
		gtk_widget_hide(frame);
}

static void proxy_print_option(GtkEntry *entry, int entrynum)
{
	if (entrynum == PROXYHOST)
		gaim_prefs_set_string("/core/proxy/host", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPORT)
		gaim_prefs_set_int("/core/proxy/port", atoi(gtk_entry_get_text(entry)));
	else if (entrynum == PROXYUSER)
		gaim_prefs_set_string("/core/proxy/username", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPASS)
		gaim_prefs_set_string("/core/proxy/password", gtk_entry_get_text(entry));
}

static GtkWidget *
network_page()
{
	GtkWidget *ret;
	GtkWidget *vbox, *hbox, *entry;
	GtkWidget *table, *label, *auto_ip_checkbox, *ports_checkbox, *spin_button;
	GtkSizeGroup *sg;
	GaimProxyInfo *proxy_info = NULL;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("IP Address"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	pidgin_prefs_labeled_entry(vbox,_("ST_UN server:"),
			"/core/network/stun_server", sg);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	label = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_size_group_add_widget(sg, label);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label),
			_("<span style=\"italic\">Example: stunserver.org</span>"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_container_add(GTK_CONTAINER(hbox), label);

	auto_ip_checkbox = pidgin_prefs_checkbox(_("_Autodetect IP address"),
			"/core/network/auto_ip", vbox);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 0);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	label = gtk_label_new_with_mnemonic(_("Public _IP:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_size_group_add_widget(sg, label);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(network_ip_changed), NULL);

	/*
	 * TODO: This could be better by showing the autodeteced
	 * IP separately from the user-specified IP.
	 */
	if (gaim_network_get_my_ip(-1) != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry),
		                   gaim_network_get_my_ip(-1));

	pidgin_set_accessible_label (entry, label);


	if (gaim_prefs_get_bool("/core/network/auto_ip")) {
		gtk_widget_set_sensitive(GTK_WIDGET(table), FALSE);
	}

	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), table);

	vbox = pidgin_make_frame (ret, _("Ports"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	ports_checkbox = pidgin_prefs_checkbox(_("_Manually specify range of ports to listen on"),
			"/core/network/ports_range_use", vbox);

	spin_button = pidgin_prefs_labeled_spin_button(vbox, _("_Start port:"),
			"/core/network/ports_range_start", 0, 65535, sg);
	if (!gaim_prefs_get_bool("/core/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	spin_button = pidgin_prefs_labeled_spin_button(vbox, _("_End port:"),
			"/core/network/ports_range_end", 0, 65535, sg);
	if (!gaim_prefs_get_bool("/core/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	if (!gaim_running_gnome()) {
		vbox = pidgin_make_frame(ret, _("Proxy Server"));
		prefs_proxy_frame = gtk_vbox_new(FALSE, 0);
		pidgin_prefs_dropdown(vbox, _("Proxy _type:"), GAIM_PREF_STRING,
					"/core/proxy/type",
					_("No proxy"), "none",
					"SOCKS 4", "socks4",
					"SOCKS 5", "socks5",
					"HTTP", "http",
					_("Use Environmental Settings"), "envvar",
					NULL);
		gtk_box_pack_start(GTK_BOX(vbox), prefs_proxy_frame, 0, 0, 0);
		proxy_info = gaim_global_proxy_get_info();

		gaim_prefs_connect_callback(prefs, "/core/proxy/type",
					    proxy_changed_cb, prefs_proxy_frame);

		table = gtk_table_new(4, 2, FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(table), 0);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 10);
		gtk_container_add(GTK_CONTAINER(prefs_proxy_frame), table);


		label = gtk_label_new_with_mnemonic(_("_Host:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYHOST);

		if (proxy_info != NULL && gaim_proxy_info_get_host(proxy_info))
			gtk_entry_set_text(GTK_ENTRY(entry),
					   gaim_proxy_info_get_host(proxy_info));

		hbox = gtk_hbox_new(TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		pidgin_set_accessible_label (entry, label);

		label = gtk_label_new_with_mnemonic(_("_Port:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPORT);

		if (proxy_info != NULL && gaim_proxy_info_get_port(proxy_info) != 0) {
			char buf[128];
			g_snprintf(buf, sizeof(buf), "%d",
				   gaim_proxy_info_get_port(proxy_info));

			gtk_entry_set_text(GTK_ENTRY(entry), buf);
		}
		pidgin_set_accessible_label (entry, label);

		label = gtk_label_new_with_mnemonic(_("_User:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYUSER);

		if (proxy_info != NULL && gaim_proxy_info_get_username(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
						   gaim_proxy_info_get_username(proxy_info));

		hbox = gtk_hbox_new(TRUE, 5);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		pidgin_set_accessible_label (entry, label);

		label = gtk_label_new_with_mnemonic(_("Pa_ssword:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);

		entry = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 1, 2, GTK_FILL , 0, 0, 0);
		gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
		if (gtk_entry_get_invisible_char(GTK_ENTRY(entry)) == '*')
			gtk_entry_set_invisible_char(GTK_ENTRY(entry), GAIM_INVISIBLE_CHAR);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPASS);

		if (proxy_info != NULL && gaim_proxy_info_get_password(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
					   gaim_proxy_info_get_password(proxy_info));
		pidgin_set_accessible_label (entry, label);
	}

	gtk_widget_show_all(ret);
	if (proxy_info == NULL ||
	    gaim_proxy_info_get_type(proxy_info) == GAIM_PROXY_NONE ||
	    gaim_proxy_info_get_type(proxy_info) == GAIM_PROXY_USE_ENVVAR)
		gtk_widget_hide(table);
	return ret;
}

#ifndef _WIN32
static gboolean manual_browser_set(GtkWidget *entry, GdkEventFocus *event, gpointer data) {
	const char *program = gtk_entry_get_text(GTK_ENTRY(entry));

	gaim_prefs_set_path("/gaim/gtk/browsers/command", program);

	/* carry on normally */
	return FALSE;
}

static GList *get_available_browsers()
{
	struct browser {
		char *name;
		char *command;
	};

	/* Sorted reverse alphabetically */
	static struct browser possible_browsers[] = {
		{N_("Seamonkey"), "seamonkey"},
		{N_("Opera"), "opera"},
		{N_("Netscape"), "netscape"},
		{N_("Mozilla"), "mozilla"},
		{N_("Konqueror"), "kfmclient"},
		{N_("GNOME Default"), "gnome-open"},
		{N_("Galeon"), "galeon"},
		{N_("Firefox"), "firefox"},
		{N_("Firebird"), "mozilla-firebird"},
		{N_("Epiphany"), "epiphany"}
	};
	static const int num_possible_browsers = G_N_ELEMENTS(possible_browsers);

	GList *browsers = NULL;
	int i = 0;
	char *browser_setting = (char *)gaim_prefs_get_string("/gaim/gtk/browsers/browser");

	browsers = g_list_prepend(browsers, (gpointer)"custom");
	browsers = g_list_prepend(browsers, (gpointer)_("Manual"));

	for (i = 0; i < num_possible_browsers; i++) {
		if (gaim_program_is_valid(possible_browsers[i].command)) {
			browsers = g_list_prepend(browsers,
									  possible_browsers[i].command);
			browsers = g_list_prepend(browsers, (gpointer)_(possible_browsers[i].name));
			if(browser_setting && !strcmp(possible_browsers[i].command, browser_setting))
				browser_setting = NULL;
		}
	}

	if(browser_setting)
		gaim_prefs_set_string("/gaim/gtk/browsers/browser", "custom");

	return browsers;
}

static void
browser_changed1_cb(const char *name, GaimPrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, strcmp(browser, "custom"));
}

static void
browser_changed2_cb(const char *name, GaimPrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, !strcmp(browser, "custom"));
}

static GtkWidget *
browser_page()
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkSizeGroup *sg;
	GList *browsers = NULL;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	vbox = pidgin_make_frame (ret, _("Browser Selection"));

	browsers = get_available_browsers();
	if (browsers != NULL) {
		label = pidgin_prefs_dropdown_from_list(vbox,_("_Browser:"), GAIM_PREF_STRING,
										 "/gaim/gtk/browsers/browser",
										 browsers);
		g_list_free(browsers);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(sg, label);

		hbox = gtk_hbox_new(FALSE, 0);
		label = pidgin_prefs_dropdown(hbox, _("_Open link in:"), GAIM_PREF_INT,
			"/gaim/gtk/browsers/place",
			_("Browser default"), PIDGIN_BROWSER_DEFAULT,
			_("Existing window"), PIDGIN_BROWSER_CURRENT,
			_("New window"), PIDGIN_BROWSER_NEW_WINDOW,
			_("New tab"), PIDGIN_BROWSER_NEW_TAB,
			NULL);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(sg, label);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		if (!strcmp(gaim_prefs_get_string("/gaim/gtk/browsers/browser"), "custom"))
			gtk_widget_set_sensitive(hbox, FALSE);
		gaim_prefs_connect_callback(prefs, "/gaim/gtk/browsers/browser",
									browser_changed1_cb, hbox);
	}

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Manual:\n(%s for URL)"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_size_group_add_widget(sg, label);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

	if (strcmp(gaim_prefs_get_string("/gaim/gtk/browsers/browser"), "custom"))
		gtk_widget_set_sensitive(hbox, FALSE);
	gaim_prefs_connect_callback(prefs, "/gaim/gtk/browsers/browser",
								browser_changed2_cb, hbox);

	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);

	gtk_entry_set_text(GTK_ENTRY(entry),
					   gaim_prefs_get_path("/gaim/gtk/browsers/command"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
					 G_CALLBACK(manual_browser_set), NULL);
	pidgin_set_accessible_label (entry, label);

	gtk_widget_show_all(ret);
	return ret;
}
#endif /*_WIN32*/

static GtkWidget *
logging_page()
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GList *names;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);


	vbox = pidgin_make_frame (ret, _("Logging"));
	names = gaim_log_logger_get_options();

	pidgin_prefs_dropdown_from_list(vbox, _("Log _format:"), GAIM_PREF_STRING,
				 "/core/logging/format", names);

	g_list_free(names);

	pidgin_prefs_checkbox(_("Log all _instant messages"),
				  "/core/logging/log_ims", vbox);
	pidgin_prefs_checkbox(_("Log all c_hats"),
				  "/core/logging/log_chats", vbox);
	pidgin_prefs_checkbox(_("Log all _status changes to system log"),
				  "/core/logging/log_system", vbox);

	gtk_widget_show_all(ret);

	return ret;
}

#ifndef _WIN32
static gint sound_cmd_yeah(GtkEntry *entry, gpointer d)
{
	gaim_prefs_set_path("/gaim/gtk/sound/command",
			gtk_entry_get_text(GTK_ENTRY(entry)));
	return TRUE;
}

static void
sound_changed1_cb(const char *name, GaimPrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox, !strcmp(method, "custom"));
}

static void
sound_changed2_cb(const char *name, GaimPrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *vbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(vbox, strcmp(method, "none"));
}

#ifdef USE_GSTREAMER
static void
sound_changed3_cb(const char *name, GaimPrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox,
			!strcmp(method, "automatic") ||
			!strcmp(method, "esd"));
}
#endif /* USE_GSTREAMER */
#endif /* !_WIN32 */


static void
event_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	char *pref;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
						2, &pref,
						-1);

	gaim_prefs_set_bool(pref, !gtk_cell_renderer_toggle_get_active(cell));
	g_free(pref);

	gtk_list_store_set(GTK_LIST_STORE (model), &iter,
					   0, !gtk_cell_renderer_toggle_get_active(cell),
					   -1);

	gtk_tree_path_free(path);
}

static void
test_sound(GtkWidget *button, gpointer i_am_NULL)
{
	char *pref;
	gboolean temp_value;

	pref = g_strdup_printf("/gaim/gtk/sound/enabled/%s",
			pidgin_sound_get_event_option(sound_row_sel));

	temp_value = gaim_prefs_get_bool(pref);

	if (!temp_value) gaim_prefs_set_bool(pref, TRUE);

	gaim_sound_play_event(sound_row_sel, NULL);

	if (!temp_value) gaim_prefs_set_bool(pref, FALSE);

	g_free(pref);
}

/*
 * Resets a sound file back to default.
 */
static void
reset_sound(GtkWidget *button, gpointer i_am_also_NULL)
{
	gchar *pref;

	pref = g_strdup_printf("/gaim/gtk/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	gaim_prefs_set_path(pref, "");
	g_free(pref);

	gtk_entry_set_text(GTK_ENTRY(sound_entry), "(default)");
}

static void
sound_chosen_cb(void *user_data, const char *filename)
{
	gchar *pref;
	int sound;

	sound = GPOINTER_TO_INT(user_data);

	/* Set it -- and forget it */
	pref = g_strdup_printf("/gaim/gtk/sound/file/%s",
						   pidgin_sound_get_event_option(sound));
	gaim_prefs_set_path(pref, filename);
	g_free(pref);

	/*
	 * If the sound we just changed is still the currently selected
	 * sound, then update the box showing the file name.
	 */
	if (sound == sound_row_sel)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), filename);
}

static void select_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	gchar *pref;
	const char *filename;

	pref = g_strdup_printf("/gaim/gtk/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	filename = gaim_prefs_get_path(pref);
	g_free(pref);

	if (*filename == '\0')
		filename = NULL;

	gaim_request_file(prefs, _("Sound Selection"), filename, FALSE,
					  G_CALLBACK(sound_chosen_cb), NULL, GINT_TO_POINTER(sound_row_sel));
}

#ifdef USE_GSTREAMER
static gchar* prefs_sound_volume_format(GtkScale *scale, gdouble val)
{
	if(val < 15) {
		return g_strdup_printf(_("Quietest"));
	} else if(val < 30) {
		return g_strdup_printf(_("Quieter"));
	} else if(val < 45) {
		return g_strdup_printf(_("Quiet"));
	} else if(val < 55) {
		return g_strdup_printf(_("Normal"));
	} else if(val < 70) {
		return g_strdup_printf(_("Loud"));
	} else if(val < 85) {
		return g_strdup_printf(_("Louder"));
	} else {
		return g_strdup_printf(_("Loudest"));
	}
}

static void prefs_sound_volume_changed(GtkRange *range)
{
	int val = (int)gtk_range_get_value(GTK_RANGE(range));
	gaim_prefs_set_int("/gaim/gtk/sound/volume", val);
}
#endif

static void prefs_sound_sel(GtkTreeSelection *sel, GtkTreeModel *model) {
	GtkTreeIter  iter;
	GValue val;
	const char *file;
	char *pref;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 3, &val);
	sound_row_sel = g_value_get_uint(&val);

	pref = g_strdup_printf("/gaim/gtk/sound/file/%s",
			pidgin_sound_get_event_option(sound_row_sel));
	file = gaim_prefs_get_path(pref);
	g_free(pref);
	if (sound_entry)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : "(default)");
	g_value_unset (&val);
}

static GtkWidget *
sound_page()
{
	GtkWidget *ret;
	GtkWidget *vbox, *sw, *button;
	GtkSizeGroup *sg;
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkListStore *event_store;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	GtkWidget *hbox;
	int j;
	const char *file;
	char *pref;
#ifndef _WIN32
	GtkWidget *dd;
	GtkWidget *label;
	GtkWidget *entry;
	const char *cmd;
#endif

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

#ifndef _WIN32
	vbox = pidgin_make_frame (ret, _("Sound Method"));
	dd = pidgin_prefs_dropdown(vbox, _("_Method:"), GAIM_PREF_STRING,
			"/gaim/gtk/sound/method",
			_("Console beep"), "beep",
#ifdef USE_GSTREAMER
			_("Automatic"), "automatic",
			"ESD", "esd",
#endif
			_("Command"), "custom",
			_("No sounds"), "none",
			NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("Sound c_ommand:\n(%s for filename)"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

	gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
	cmd = gaim_prefs_get_path("/gaim/gtk/sound/command");
	if(cmd)
		gtk_entry_set_text(GTK_ENTRY(entry), cmd);

	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(sound_cmd_yeah), NULL);

	gaim_prefs_connect_callback(prefs, "/gaim/gtk/sound/method",
								sound_changed1_cb, hbox);
	gtk_widget_set_sensitive(hbox,
			!strcmp(gaim_prefs_get_string("/gaim/gtk/sound/method"),
					"custom"));

	pidgin_set_accessible_label (entry, label);
#endif /* _WIN32 */

	vbox = pidgin_make_frame (ret, _("Sound Options"));
	pidgin_prefs_checkbox(_("Sounds when conversation has _focus"),
				   "/gaim/gtk/sound/conv_focus", vbox);
	pidgin_prefs_dropdown(vbox, _("Enable sounds:"),
				 GAIM_PREF_INT, "/core/sound/while_status",
				_("Only when available"), 1,
				_("Only when not available"), 2,
				_("Always"), 3,
				NULL);

#ifdef USE_GSTREAMER
	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("Volume:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	sw = gtk_hscale_new_with_range(0.0, 100.0, 5.0);
	gtk_range_set_increments(GTK_RANGE(sw), 5.0, 25.0);
	gtk_range_set_value(GTK_RANGE(sw), gaim_prefs_get_int("/gaim/gtk/sound/volume"));
	g_signal_connect (G_OBJECT (sw), "format-value",
			  G_CALLBACK (prefs_sound_volume_format),
			  NULL);
	g_signal_connect (G_OBJECT (sw), "value-changed",
			  G_CALLBACK (prefs_sound_volume_changed),
			  NULL);
	gtk_box_pack_start(GTK_BOX(hbox), sw, TRUE, TRUE, 0);

	gaim_prefs_connect_callback(prefs, "/gaim/gtk/sound/method",
								sound_changed3_cb, hbox);
	sound_changed3_cb("/gaim/gtk/sound/method", GAIM_PREF_STRING,
			  gaim_prefs_get_string("/gaim/gtk/sound/method"), hbox);
#endif

#ifndef _WIN32
	gtk_widget_set_sensitive(vbox,
			strcmp(gaim_prefs_get_string("/gaim/gtk/sound/method"), "none"));
	gaim_prefs_connect_callback(prefs, "/gaim/gtk/sound/method",
								sound_changed2_cb, vbox);
#endif

	vbox = pidgin_make_frame(ret, _("Sound Events"));

	/* The following is an ugly hack to make the frame expand so the
	 * sound events list is big enough to be usable */
	gtk_box_set_child_packing(GTK_BOX(vbox->parent), vbox, TRUE, TRUE, 0,
			GTK_PACK_START);
	gtk_box_set_child_packing(GTK_BOX(vbox->parent->parent), vbox->parent, TRUE,
			TRUE, 0, GTK_PACK_START);
	gtk_box_set_child_packing(GTK_BOX(vbox->parent->parent->parent),
			vbox->parent->parent, TRUE, TRUE, 0, GTK_PACK_START);

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	event_store = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	for (j=0; j < GAIM_NUM_SOUNDS; j++) {
		char *pref = g_strdup_printf("/gaim/gtk/sound/enabled/%s",
					     pidgin_sound_get_event_option(j));
		const char *label = pidgin_sound_get_event_label(j);

		if (label == NULL) {
			g_free(pref);
			continue;
		}

		gtk_list_store_append (event_store, &iter);
		gtk_list_store_set(event_store, &iter,
				   0, gaim_prefs_get_bool(pref),
				   1, _(label),
				   2, pref,
				   3, j,
				   -1);
		g_free(pref);
	}

	event_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(event_store));

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));
	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (prefs_sound_sel),
			  NULL);
	g_signal_connect (G_OBJECT(rend), "toggled",
			  G_CALLBACK(event_toggled), event_store);
	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);

	col = gtk_tree_view_column_new_with_attributes (_("Play"),
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes (_("Event"),
							rend,
							"text", 1,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	g_object_unref(G_OBJECT(event_store));
	gtk_container_add(GTK_CONTAINER(sw), event_view);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	sound_entry = gtk_entry_new();
	pref = g_strdup_printf("/gaim/gtk/sound/file/%s",
			       pidgin_sound_get_event_option(0));
	file = gaim_prefs_get_path(pref);
	g_free(pref);
	gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : "(default)");
	gtk_editable_set_editable(GTK_EDITABLE(sound_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sound_entry, FALSE, FALSE, GAIM_HIG_BOX_SPACE);

	button = gtk_button_new_with_label(_("Test"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(test_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_label(_("Reset"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(reset_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_label(_("Choose..."));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(select_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	gtk_widget_show_all(ret);

	return ret;
}


static void
set_idle_away(GaimSavedStatus *status)
{
	gaim_prefs_set_int("/core/savedstatus/idleaway", gaim_savedstatus_get_creation_time(status));
}

static void
set_startupstatus(GaimSavedStatus *status)
{
	gaim_prefs_set_int("/core/savedstatus/startup", gaim_savedstatus_get_creation_time(status));
}

static GtkWidget *
away_page()
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *dd;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *select;
	GtkWidget *menu;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, GAIM_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), GAIM_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Idle stuff */
	vbox = pidgin_make_frame(ret, _("Idle"));

	dd = pidgin_prefs_dropdown(vbox, _("_Report idle time:"),
		GAIM_PREF_STRING, "/core/away/idle_reporting",
		_("Never"), "none",
		_("From last sent message"), "gaim",
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
		_("Based on keyboard or mouse use"), "system",
#endif
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	/* Away stuff */
	vbox = pidgin_make_frame(ret, _("Away"));

	dd = pidgin_prefs_dropdown(vbox, _("_Auto-reply:"),
		GAIM_PREF_STRING, "/core/away/auto_reply",
		_("Never"), "never",
		_("When away"), "away",
		_("When both away and idle"), "awayidle",
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	/* Auto-away stuff */
	vbox = pidgin_make_frame(ret, _("Auto-away"));

	button = pidgin_prefs_checkbox(_("Change status when _idle"),
						   "/core/away/away_when_idle", vbox);

	select = pidgin_prefs_labeled_spin_button(vbox,
			_("_Minutes before changing status:"), "/core/away/mins_before_away",
			1, 24 * 60, sg);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), select);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	label = gtk_label_new_with_mnemonic(_("Change _status to:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(gaim_savedstatus_get_idleaway(), G_CALLBACK(set_idle_away));
	gtk_box_pack_start(GTK_BOX(hbox), menu, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), menu);

	if (!gaim_prefs_get_bool("/core/away/away_when_idle")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	/* Signon status stuff */
	vbox = pidgin_make_frame(ret, _("Status at Startup"));

	button = pidgin_prefs_checkbox(_("Use status from last _exit at startup"),
		"/core/savedstatus/startup_current_status", vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	label = gtk_label_new_with_mnemonic(_("Status to a_pply at startup:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(gaim_savedstatus_get_startup(), G_CALLBACK(set_startupstatus));
	gtk_box_pack_start(GTK_BOX(hbox), menu, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), menu);

	if (gaim_prefs_get_bool("/core/savedstatus/startup_current_status")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	gtk_widget_show_all(ret);

	return ret;
}

static int 
prefs_notebook_add_page(const char *text,
  		        GtkWidget *page,
			int ind) {

#if GTK_CHECK_VERSION(2,4,0)
	return gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
#else
	gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
	return gtk_notebook_page_num(GTK_NOTEBOOK(prefsnotebook), page);
#endif
}

static void prefs_notebook_init() {
	prefs_notebook_add_page(_("Interface"), interface_page(), notebook_page++);
	prefs_notebook_add_page(_("Conversations"), conv_page(), notebook_page++);
	prefs_notebook_add_page(_("Smiley Themes"), theme_page(), notebook_page++);
	prefs_notebook_add_page(_("Sounds"), sound_page(), notebook_page++);
	prefs_notebook_add_page(_("Network"), network_page(), notebook_page++);
#ifndef _WIN32
	/* We use the registered default browser in windows */
	/* if the user is running gnome 2.x or Mac OS X, hide the browsers tab */
	if ((gaim_running_gnome() == FALSE) && (gaim_running_osx() == FALSE)) {
		prefs_notebook_add_page(_("Browser"), browser_page(), notebook_page++);
	}
#endif
	prefs_notebook_add_page(_("Logging"), logging_page(), notebook_page++);
	prefs_notebook_add_page(_("Status / Idle"), away_page(), notebook_page++);
}

void pidgin_prefs_show(void)
{
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *notebook;
	GtkWidget *button;

	if (prefs) {
		gtk_window_present(GTK_WINDOW(prefs));
		return;
	}

	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	/* (that should have been "effect," right?) */

	/* Back to instant-apply! I win!  BU-HAHAHA! */

	/* Create the window */
	prefs = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(prefs), "preferences");
	gtk_window_set_title(GTK_WINDOW(prefs), _("Preferences"));
	gtk_window_set_resizable (GTK_WINDOW(prefs), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(prefs), GAIM_HIG_BORDER);
	g_signal_connect(G_OBJECT(prefs), "destroy",
					 G_CALLBACK(delete_prefs), NULL);

	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(prefs), vbox);
	gtk_widget_show(vbox);

	/* The notebook */
	prefsnotebook = notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(prefsnotebook);

	/* The buttons to press! */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show (bbox);

	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), prefs);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	prefs_notebook_init();

	/* Show everything. */
	gtk_widget_show(prefs);
}

static void
set_bool_pref(GtkWidget *w, const char *key)
{
	gaim_prefs_set_bool(key,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

GtkWidget *
pidgin_prefs_checkbox(const char *text, const char *key, GtkWidget *page)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
								 gaim_prefs_get_bool(key));

	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(set_bool_pref), (char *)key);

	gtk_widget_show(button);

	return button;
}

static void
smiley_theme_pref_cb(const char *name, GaimPrefType type,
					 gconstpointer value, gpointer data)
{
	const char *themename = value;
	GSList *themes;

	for (themes = smiley_themes; themes; themes = themes->next) {
		struct smiley_theme *smile = themes->data;
		if (smile->name && strcmp(themename, smile->name) == 0) {
			pidginthemes_load_smiley_theme(smile->path, TRUE);
			break;
		}
	}
}

void
pidgin_prefs_init(void)
{
	gaim_prefs_add_none("/gaim");
	gaim_prefs_add_none("/gaim/gtk");
	gaim_prefs_add_none("/plugins/gtk");

#ifndef _WIN32
	/* Browsers */
	gaim_prefs_add_none("/gaim/gtk/browsers");
	gaim_prefs_add_int("/gaim/gtk/browsers/place", PIDGIN_BROWSER_DEFAULT);
	gaim_prefs_add_path("/gaim/gtk/browsers/command", "");
	gaim_prefs_add_string("/gaim/gtk/browsers/browser", "mozilla");
#endif

	/* Plugins */
	gaim_prefs_add_none("/gaim/gtk/plugins");
	gaim_prefs_add_path_list("/gaim/gtk/plugins/loaded", NULL);

	/* File locations */
	gaim_prefs_add_none("/gaim/gtk/filelocations");
	gaim_prefs_add_path("/gaim/gtk/filelocations/last_save_folder", "");
	gaim_prefs_add_path("/gaim/gtk/filelocations/last_open_folder", "");
	gaim_prefs_add_path("/gaim/gtk/filelocations/last_icon_folder", "");

	/* Smiley Themes */
	gaim_prefs_add_none("/gaim/gtk/smileys");
	gaim_prefs_add_string("/gaim/gtk/smileys/theme", "Default");

	/* Smiley Callbacks */
	gaim_prefs_connect_callback(prefs, "/gaim/gtk/smileys/theme",
								smiley_theme_pref_cb, NULL);
}

void pidgin_prefs_update_old() {
	/* Rename some old prefs */
	gaim_prefs_rename("/gaim/gtk/logging/log_ims", "/core/logging/log_ims");
	gaim_prefs_rename("/gaim/gtk/logging/log_chats", "/core/logging/log_chats");
	gaim_prefs_rename("/core/conversations/placement",
					  "/gaim/gtk/conversations/placement");

	gaim_prefs_rename("/gaim/gtk/debug/timestamps", "/core/debug/timestamps");
	gaim_prefs_rename("/gaim/gtk/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	gaim_prefs_rename_boolean_toggle("/gaim/gtk/conversations/ignore_colors",
									 "/gaim/gtk/conversations/show_incoming_formatting");

	/* this string pref moved into the core, try to be friendly */
	gaim_prefs_rename("/gaim/gtk/idle/reporting_method", "/core/away/idle_reporting");

	/* Remove some no-longer-used prefs */
	gaim_prefs_remove("/gaim/gtk/blist/auto_expand_contacts");
	gaim_prefs_remove("/gaim/gtk/blist/button_style");
	gaim_prefs_remove("/gaim/gtk/blist/grey_idle_buddies");
	gaim_prefs_remove("/gaim/gtk/blist/raise_on_events");
	gaim_prefs_remove("/gaim/gtk/blist/show_group_count");
	gaim_prefs_remove("/gaim/gtk/blist/show_warning_level");
	gaim_prefs_remove("/gaim/gtk/conversations/button_type");
	gaim_prefs_remove("/gaim/gtk/conversations/ctrl_enter_sends");
	gaim_prefs_remove("/gaim/gtk/conversations/enter_sends");
	gaim_prefs_remove("/gaim/gtk/conversations/escape_closes");
	gaim_prefs_remove("/gaim/gtk/conversations/html_shortcuts");
	gaim_prefs_remove("/gaim/gtk/conversations/icons_on_tabs");
	gaim_prefs_remove("/gaim/gtk/conversations/send_formatting");
	gaim_prefs_remove("/gaim/gtk/conversations/show_smileys");
	gaim_prefs_remove("/gaim/gtk/conversations/show_urls_as_links");
	gaim_prefs_remove("/gaim/gtk/conversations/smiley_shortcuts");
	gaim_prefs_remove("/gaim/gtk/conversations/use_custom_bgcolor");
	gaim_prefs_remove("/gaim/gtk/conversations/use_custom_fgcolor");
	gaim_prefs_remove("/gaim/gtk/conversations/use_custom_font");
	gaim_prefs_remove("/gaim/gtk/conversations/use_custom_size");
	gaim_prefs_remove("/gaim/gtk/conversations/chat/old_tab_complete");
	gaim_prefs_remove("/gaim/gtk/conversations/chat/tab_completion");
	gaim_prefs_remove("/gaim/gtk/conversations/im/hide_on_send");
	gaim_prefs_remove("/gaim/gtk/conversations/chat/color_nicks");
	gaim_prefs_remove("/gaim/gtk/conversations/chat/raise_on_events");
	gaim_prefs_remove("/gaim/gtk/conversations/ignore_fonts");
	gaim_prefs_remove("/gaim/gtk/conversations/ignore_font_sizes");
	gaim_prefs_remove("/gaim/gtk/conversations/passthrough_unknown_commands");
	gaim_prefs_remove("/gaim/gtk/idle");
	gaim_prefs_remove("/gaim/gtk/logging/individual_logs");
	gaim_prefs_remove("/gaim/gtk/sound/signon");
	gaim_prefs_remove("/gaim/gtk/sound/silent_signon");

	/* Convert old queuing prefs to hide_new 3-way pref. */
	if (gaim_prefs_exists("/plugins/gtk/docklet/queue_messages") &&
	    gaim_prefs_get_bool("/plugins/gtk/docklet/queue_messages"))
	{
		gaim_prefs_set_string("/gaim/gtk/conversations/im/hide_new", "always");
	}
	else if (gaim_prefs_exists("/gaim/gtk/away/queue_messages") &&
	         gaim_prefs_get_bool("/gaim/gtk/away/queue_messages"))
	{
		gaim_prefs_set_string("/gaim/gtk/conversations/im/hide_new", "away");
	}
	gaim_prefs_remove("/gaim/gtk/away/queue_messages");
	gaim_prefs_remove("/gaim/gtk/away");
	gaim_prefs_remove("/plugins/gtk/docklet/queue_messages");
}

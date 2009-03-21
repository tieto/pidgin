/**
 * @file gtkprefs.c GTK+ Preferences
 * @ingroup pidgin
 */

/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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
#include "sound-theme.h"
#include "theme-manager.h"
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
#include "gtkstatus-icon-theme.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "pidginstock.h"

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYUSER 2
#define PROXYPASS 3

#define PREFS_OPTIMAL_ICON_SIZE 32

static int sound_row_sel = 0;
static GtkWidget *prefsnotebook;

static GtkWidget *sound_entry = NULL;
static GtkListStore *smiley_theme_store = NULL;
static GtkTreeSelection *smiley_theme_sel = NULL;
static GtkWidget *prefs_proxy_frame = NULL;

static GtkWidget *prefs = NULL;
static GtkWidget *debugbutton = NULL;
static int notebook_page = 0;
static GtkTreeRowReference *previous_smiley_row = NULL;

static gboolean prefs_themes_unsorted = TRUE;
static GtkListStore *prefs_sound_themes;
static GtkListStore *prefs_blist_themes;
static GtkListStore *prefs_status_icon_themes;


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

	purple_prefs_set_int(key, value);
}

GtkWidget *
pidgin_prefs_labeled_spin_button(GtkWidget *box, const gchar *title,
		const char *key, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *spin;
	GtkObject *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = gtk_adjustment_new(val, min, max, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	if (max < 10000)
		gtk_widget_set_size_request(spin, 50, -1);
	else
		gtk_widget_set_size_request(spin, 60, -1);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
					 G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	return pidgin_add_widget_to_vbox(GTK_BOX(box), title, sg, spin, FALSE, NULL);
}

static void
entry_set(GtkEntry *entry, gpointer data) {
	const char *key = (const char*)data;

	purple_prefs_set_string(key, gtk_entry_get_text(entry));
}

GtkWidget *
pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

static void
dropdown_set(GObject *w, const char *key)
{
	const char *str_value;
	int int_value;
	PurplePrefType type;

	type = GPOINTER_TO_INT(g_object_get_data(w, "type"));

	if (type == PURPLE_PREF_INT) {
		int_value = GPOINTER_TO_INT(g_object_get_data(w, "value"));

		purple_prefs_set_int(key, int_value);
	}
	else if (type == PURPLE_PREF_STRING) {
		str_value = (const char *)g_object_get_data(w, "value");

		purple_prefs_set_string(key, str_value);
	}
	else if (type == PURPLE_PREF_BOOLEAN) {
		purple_prefs_set_bool(key,
				GPOINTER_TO_INT(g_object_get_data(w, "value")));
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	GtkWidget  *dropdown, *opt, *menu;
	GtkWidget  *label = NULL;
	gchar      *text;
	const char *stored_str = NULL;
	int         stored_int = 0;
	int         int_value  = 0;
	const char *str_value  = NULL;
	int         o = 0;

	g_return_val_if_fail(menuitems != NULL, NULL);

#if 0 /* GTK_CHECK_VERSION(2,4,0) */
	if(type == PURPLE_PREF_INT)
		model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	else if(type == PURPLE_PREF_STRING)
		model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	dropdown = gtk_combo_box_new_with_model(model);
#else
	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();
#endif

	if (type == PURPLE_PREF_INT)
		stored_int = purple_prefs_get_int(key);
	else if (type == PURPLE_PREF_STRING)
		stored_str = purple_prefs_get_string(key);

	while (menuitems != NULL && (text = (char *) menuitems->data) != NULL) {
		menuitems = g_list_next(menuitems);
		g_return_val_if_fail(menuitems != NULL, NULL);

		opt = gtk_menu_item_new_with_label(text);

		g_object_set_data(G_OBJECT(opt), "type", GINT_TO_POINTER(type));

		if (type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			g_object_set_data(G_OBJECT(opt), "value",
							  GINT_TO_POINTER(int_value));
		}
		else if (type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;

			g_object_set_data(G_OBJECT(opt), "value", (char *)str_value);
		}
		else if (type == PURPLE_PREF_BOOLEAN) {
			g_object_set_data(G_OBJECT(opt), "value",
					menuitems->data);
		}

		g_signal_connect(G_OBJECT(opt), "activate",
						 G_CALLBACK(dropdown_set), (char *)key);

		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);

		if ((type == PURPLE_PREF_INT && stored_int == int_value) ||
			(type == PURPLE_PREF_STRING && stored_str != NULL &&
			 !strcmp(stored_str, str_value)) ||
			(type == PURPLE_PREF_BOOLEAN &&
			 (purple_prefs_get_bool(key) == GPOINTER_TO_INT(menuitems->data)))) {

			gtk_menu_set_active(GTK_MENU(menu), o);
		}

		menuitems = g_list_next(menuitems);

		o++;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);

	pidgin_add_widget_to_vbox(GTK_BOX(box), title, NULL, dropdown, FALSE, &label);

	return label;
}

GtkWidget *
pidgin_prefs_dropdown(GtkWidget *box, const gchar *title, PurplePrefType type,
			   const char *key, ...)
{
	va_list ap;
	GList *menuitems = NULL;
	GtkWidget *dropdown = NULL;
	char *name;
	int int_value;
	const char *str_value;

	g_return_val_if_fail(type == PURPLE_PREF_BOOLEAN || type == PURPLE_PREF_INT ||
			type == PURPLE_PREF_STRING, NULL);

	va_start(ap, key);
	while ((name = va_arg(ap, char *)) != NULL) {

		menuitems = g_list_prepend(menuitems, name);

		if (type == PURPLE_PREF_INT || type == PURPLE_PREF_BOOLEAN) {
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
	purple_request_close_with_handle(prefs);

	/* Unregister callbacks. */
	purple_prefs_disconnect_by_handle(prefs);

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
	GtkWidget *remove_button = g_object_get_data(G_OBJECT(sel), "remove_button");

	if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
		gtk_widget_set_sensitive(remove_button, FALSE);
		return;
	}

	old_theme = current_smiley_theme;
	val.g_type = 0;
	gtk_tree_model_get_value(model, &iter, 3, &val);
	path = gtk_tree_model_get_path(model, &iter);
	themename = g_value_get_string(&val);
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/smileys/theme", themename);

	gtk_widget_set_sensitive(remove_button, (strcmp(themename, "none") &&
	                                         strcmp(themename, _("Default"))));
	g_value_unset (&val);

	/* current_smiley_theme is set in callback for the above pref change */
	new_theme = current_smiley_theme;
	description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
								"<span size='smaller' foreground='white'>%s</span>",
								_(new_theme->name), _(new_theme->author), _(new_theme->desc));
	gtk_list_store_set(smiley_theme_store, &iter, 1, description, -1);
	g_free(description);

	if (new_theme != old_theme && previous_smiley_row) {
		oldpath = gtk_tree_row_reference_get_path(previous_smiley_row);
		if (gtk_tree_model_get_iter(model, &iter, oldpath)) {
			description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
								"<span size='smaller' foreground='dim grey'>%s</span>",
								_(old_theme->name), _(old_theme->author), _(old_theme->desc));
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

static GtkTreeRowReference *theme_refresh_theme_list(void)
{
	GdkPixbuf *pixbuf;
	GSList *themes;
	GtkTreeIter iter;
	GtkTreeRowReference *row_ref = NULL;

	if (previous_smiley_row)
		gtk_tree_row_reference_free(previous_smiley_row);
	previous_smiley_row = NULL;

	pidgin_themes_smiley_theme_probe();

	if (!(themes = smiley_themes))
		return NULL;

	gtk_list_store_clear(smiley_theme_store);

	while (themes) {
		struct smiley_theme *theme = themes->data;
		char *description = g_strdup_printf("<span size='larger' weight='bold'>%s</span> - %s\n"
						    "<span size='smaller' foreground='dim grey'>%s</span>",
						    _(theme->name), _(theme->author), _(theme->desc));
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

	destdir = g_strconcat(purple_user_dir(), G_DIR_SEPARATOR_S "smileys", NULL);

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
		purple_notify_error(NULL, NULL, _("Smiley theme failed to unpack."), NULL);
	}

	g_free(command);
#endif
	g_free(destdir);

	theme_rowref = theme_refresh_theme_list();
	if (theme_rowref != NULL) {
		GtkTreePath *tp = gtk_tree_row_reference_get_path(theme_rowref);

		if (tp)
			gtk_tree_selection_select_path(smiley_theme_sel, tp);
		gtk_tree_row_reference_free(theme_rowref);
	}
}

static void
theme_got_url(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *themedata, size_t len, const gchar *error_message)
{
	FILE *f;
	gchar *path;
	size_t wc;

	if ((error_message != NULL) || (len == 0))
		return;

	f = purple_mkstemp(&path, TRUE);
	wc = fwrite(themedata, len, 1, f);
	if (wc != 1) {
		purple_debug_warning("theme_got_url", "Unable to write theme data.\n");
		fclose(f);
		g_unlink(path);
		g_free(path);
		return;
	}
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
				purple_debug(PURPLE_DEBUG_ERROR, "theme dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				return;
			}
			theme_install_theme(tmp, NULL);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7)) {
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			purple_util_fetch_url(name, TRUE, NULL, FALSE, theme_got_url, ".tgz");
		} else if (!g_ascii_strncasecmp(name, "https://", 8)) {
			/* purple_util_fetch_url() doesn't support HTTPS, but we want users
			 * to be able to drag and drop links from the SF trackers, so
			 * we'll try it as an HTTP URL. */
			char *tmp = g_strdup(name + 1);
			tmp[0] = 'h';
			tmp[1] = 't';
			tmp[2] = 't';
			tmp[3] = 'p';
			purple_util_fetch_url(tmp, TRUE, NULL, FALSE, theme_got_url, ".tgz");
			g_free(tmp);
		}

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}

/* Rebuild the markup for the sound theme selection for "(Custom)" themes */
static void
pref_sound_generate_markup()
{
	gboolean print_custom, customized;
	const gchar *name, *author, *description, *current_theme;
	gchar *markup;
	PurpleSoundTheme *theme;
	GtkTreeIter iter;

	customized = pidgin_sound_is_customized();
	current_theme = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(prefs_sound_themes), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &iter, 2, &name, -1);

			print_custom = customized && g_str_equal(current_theme, name);

			if (g_str_equal(name, ""))
				markup = g_strdup_printf("<b>(Default)</b>%s%s - None\n<span foreground='dim grey'>The default Pidgin sound theme</span>",
							 print_custom ? " " : "", print_custom ? "(Custom)" : "");
			else {
				theme = PURPLE_SOUND_THEME(purple_theme_manager_find_theme(name, "sound"));
				author = purple_theme_get_author(PURPLE_THEME(theme));
				description = purple_theme_get_description(PURPLE_THEME(theme));

				markup = g_strdup_printf("<b>%s</b>%s%s%s%s\n<span foreground='dim grey'>%s</span>",
							 name, print_custom ? " " : "", print_custom ? "(Custom)" : "",
							 author != NULL ? " - " : "", author != NULL ? author : "", description != NULL ? description : "");
			}

			gtk_list_store_set(prefs_sound_themes, &iter, 1, markup, -1);

			g_free(markup);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(prefs_sound_themes), &iter));
	}
}

/* adds the themes to the theme list from the manager so they can be sisplayed in prefs */
static void
prefs_themes_sort(PurpleTheme *theme)
{
	GdkPixbuf *pixbuf = NULL;
	GtkTreeIter iter;
	gchar *image_full = NULL, *markup;
	const gchar *name, *author, *description;

	if (PURPLE_IS_SOUND_THEME(theme)){

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = gdk_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE, NULL);
			g_free(image_full);
		} else pixbuf = NULL;

		gtk_list_store_append(prefs_sound_themes, &iter);
		gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, purple_theme_get_name(theme), -1);

		if (pixbuf != NULL)
			gdk_pixbuf_unref(pixbuf);

	} else if (PIDGIN_IS_BLIST_THEME(theme) || PIDGIN_IS_STATUS_ICON_THEME(theme)){
		GtkListStore *store;

		if (PIDGIN_IS_BLIST_THEME(theme))
			store = prefs_blist_themes;
		else store = prefs_status_icon_themes;

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = gdk_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE, NULL);
			g_free(image_full);
		} else pixbuf = NULL;

		name = purple_theme_get_name(theme);
		author = purple_theme_get_author(theme);
		description = purple_theme_get_description(theme);

		markup = g_strdup_printf("<b>%s</b>%s%s\n<span foreground='dim grey'>%s</span>", name, author != NULL ? " - " : "",
					 author != NULL ? author : "", description != NULL ? description : "");

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, markup, 2, name, -1);

		g_free(markup);
		if (pixbuf != NULL)
			gdk_pixbuf_unref(pixbuf);
	}

}

/* init all the theme variables so that the themes can be sorted later and used by pref pages */
static void
prefs_themes_init()
{
	GdkPixbuf *pixbuf = NULL;
	gchar *filename;
	GtkTreeIter iter;

	filename = g_build_filename(DATADIR, "icons", "hicolor", "32x32", "apps", "pidgin.png", NULL);
	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE, NULL);
	g_free(filename);

	/* sound themes */
	prefs_sound_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append(prefs_sound_themes, &iter);
	gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, "", -1);

	/* blist themes */
	prefs_blist_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append(prefs_blist_themes, &iter);
	gtk_list_store_set(prefs_blist_themes, &iter, 0, pixbuf, 1, "<b>(Default)</b> - None\n<span color='dim grey'>"
								    "The default Pidgin buddy list theme</span>", 2, "", -1);

	/* status icon themes */
	prefs_status_icon_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append(prefs_status_icon_themes, &iter);
	gtk_list_store_set(prefs_status_icon_themes, &iter, 0, pixbuf, 1, "<b>(Default)</b> - None\n<span color='dim grey'>"
								    "The default Pidgin status icon theme</span>", 2, "", -1);

	gdk_pixbuf_unref(pixbuf);
}

/* builds a theme combo box from a list store with colums: icon preview, markup, theme name */
static GtkWidget *
prefs_build_theme_combo_box(GtkListStore *store, const gchar *current_theme)
{
	GtkWidget *combo_box;
	GtkCellRenderer *cell_rend;
	GtkTreeIter iter;
	gchar *theme = NULL;
	gboolean unset = TRUE;

	g_return_val_if_fail(store != NULL && current_theme != NULL, NULL);

	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

	cell_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_fixed_size(cell_rend, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "pixbuf", 0, NULL);

	cell_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "markup", 1, NULL);
/*#if GTK_CHECK_VERSION(2,6,0)
			g_object_set(cell_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif*/

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &theme, -1);

			if (g_str_equal(current_theme, theme)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo_box), &iter);
				unset = FALSE;
			}

			g_free(theme);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}

	if (unset)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);

	return combo_box;
}

/* sets the current sound theme */
static void
prefs_set_sound_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gint i;
	gchar *pref;
	gchar *new_theme;
	gboolean success;
	GtkTreeIter new_iter;

	success = gtk_combo_box_get_active_iter(combo_box, &new_iter);
	g_return_if_fail(success);

	gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &new_iter, 2, &new_theme, -1);

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/sound/theme", new_theme);

	/* New theme removes all customization */
	for(i=0; i <  PURPLE_NUM_SOUNDS; i++){
		pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
					pidgin_sound_get_event_option(i));
		purple_prefs_set_path(pref, "");
		g_free(pref);
	}

	/* gets rid of the "(Custom)" from the last selection */
	pref_sound_generate_markup();

	gtk_entry_set_text(GTK_ENTRY(sound_entry), _("(default)"));

	g_free(new_theme);
}

/* Does same as normal sort, except "none" is sorted first */
static gint pidgin_sort_smileys (GtkTreeModel	*model,
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
		ret = purple_utf8_strcasecmp(name1,name2);
	}

	g_free(name1);
	g_free(name2);

	return ret;
}

static void
request_theme_file_name_cb(gpointer data, char *theme_file_name)
{
	theme_install_theme(theme_file_name, NULL) ;
}

static void
add_theme_button_clicked_cb(GtkWidget *widget, gpointer null)
{
	purple_request_file(NULL, _("Install Theme"), NULL, FALSE, (GCallback)request_theme_file_name_cb, NULL, NULL, NULL, NULL, NULL) ;
}

static void
remove_theme_button_clicked_cb(GtkWidget *button, GtkTreeView *tv)
{
	char *theme_name = NULL, *theme_file = NULL;
	GtkTreeModel *tm;
	GtkTreeIter itr;
	GtkTreeRowReference *trr = NULL;

	if ((tm = gtk_tree_view_get_model(tv)) == NULL)
		return;
	if (!gtk_tree_selection_get_selected(smiley_theme_sel, NULL, &itr))
		return;
	gtk_tree_model_get(tm, &itr, 2, &theme_file, 3, &theme_name, -1);

	if (theme_file && theme_name && strcmp(theme_name, "none"))
		pidgin_themes_remove_smiley_theme(theme_file);

	if ((trr = theme_refresh_theme_list()) != NULL) {
		GtkTreePath *tp = gtk_tree_row_reference_get_path(trr);

		if (tp) {
			gtk_tree_selection_select_path(smiley_theme_sel, tp);
			gtk_tree_path_free(tp);
		}
		gtk_tree_row_reference_free(trr);
	}

	g_free(theme_file);
	g_free(theme_name);
}

static GtkWidget *
theme_page(void)
{
	GtkWidget *add_button, *remove_button;
	GtkWidget *hbox_buttons;
	GtkWidget *alignment;
	GtkWidget *ret;
	GtkWidget *sw;
	GtkWidget *view;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreeRowReference *rowref;
	GtkWidget *label;
	GtkTargetEntry te[3] = {{"text/plain", 0, 0},{"text/uri-list", 0, 1},{"STRING", 0, 2}};

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

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
	smiley_theme_sel = sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

	/* Custom sort so "none" theme is at top of list */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(smiley_theme_store),
									3, pidgin_sort_smileys, NULL, NULL);

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

	alignment = gtk_alignment_new(1.0, 0.5, 0.0, 1.0);
	gtk_widget_show(alignment);
	gtk_box_pack_start(GTK_BOX(ret), alignment, FALSE, TRUE, 0);

	hbox_buttons = gtk_hbox_new(TRUE, PIDGIN_HIG_CAT_SPACE);
	gtk_widget_show(hbox_buttons);
	gtk_container_add(GTK_CONTAINER(alignment), hbox_buttons);

	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(add_button);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), add_button, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(add_button), "clicked", (GCallback)add_theme_button_clicked_cb, view);

	remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	gtk_widget_show(remove_button);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), remove_button, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(remove_button), "clicked", (GCallback)remove_theme_button_clicked_cb, view);
	g_object_set_data(G_OBJECT(sel), "remove_button", remove_button);

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
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", bold);
	if (buttons & GTK_IMHTML_ITALIC)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", italic);
	if (buttons & GTK_IMHTML_UNDERLINE)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", uline);

	if (buttons & GTK_IMHTML_GROW || buttons & GTK_IMHTML_SHRINK)
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/font_size",
						   gtk_imhtml_get_current_fontsize(GTK_IMHTML(imhtml)));
	if (buttons & GTK_IMHTML_FACE) {
		char *face = gtk_imhtml_get_current_fontface(GTK_IMHTML(imhtml));
		if (!face)
			face = g_strdup("");

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", face);
		g_free(face);
	}

	if (buttons & GTK_IMHTML_FORECOLOR) {
		char *color = gtk_imhtml_get_current_forecolor(GTK_IMHTML(imhtml));
		if (!color)
			color = g_strdup("");

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", color);
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

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", color);
		g_free(color);
	}
}

static void
formatting_clear_cb(GtkIMHtml *imhtml, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);

	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/font_size", 3);

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");
}

static void
conversation_usetabs_cb(const char *name, PurplePrefType type,
						gconstpointer value, gpointer data)
{
	gboolean usetabs = GPOINTER_TO_INT(value);

	if (usetabs)
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	else
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
}


#define CONVERSATION_CLOSE_ACCEL_PATH "<main>/Conversation/Close"

/* Filled in in keyboard_shortcuts(). */
static GtkAccelKey ctrl_w = { 0, 0, 0 };
static GtkAccelKey escape = { 0, 0, 0 };

static guint escape_closes_conversation_cb_id = 0;

static gboolean
accel_is_escape(GtkAccelKey *k)
{
	return (k->accel_key == escape.accel_key
		&& k->accel_mods == escape.accel_mods);
}

/* Update the tickybox in Preferences when the keybinding for Conversation ->
 * Close is changed via Gtk.
 */
static void
conversation_close_accel_changed_cb (GtkAccelMap    *object,
                                     gchar          *accel_path,
                                     guint           accel_key,
                                     GdkModifierType accel_mods,
                                     gpointer        checkbox_)
{
	GtkToggleButton *checkbox = GTK_TOGGLE_BUTTON(checkbox_);
	GtkAccelKey new = { accel_key, accel_mods, 0 };

	g_signal_handler_block(checkbox, escape_closes_conversation_cb_id);
	gtk_toggle_button_set_active(checkbox, accel_is_escape(&new));
	g_signal_handler_unblock(checkbox, escape_closes_conversation_cb_id);
}


static void
escape_closes_conversation_cb(GtkWidget *w,
                              gpointer unused)
{
	gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
	gboolean changed;
	GtkAccelKey *new_key = active ? &escape : &ctrl_w;

	changed = gtk_accel_map_change_entry(CONVERSATION_CLOSE_ACCEL_PATH,
		new_key->accel_key, new_key->accel_mods, TRUE);

	/* If another path is already bound to the new accelerator,
	 * _change_entry tries to delete that binding (because it was passed
	 * replace=TRUE).  If that other path is locked, then _change_entry
	 * will fail.  We don't ever lock any accelerator paths, so this case
	 * should never arise.
	 */
	if(!changed)
		purple_debug_warning("gtkprefs", "Escape accel failed to change\n");
}


/* Creates preferences for keyboard shortcuts that it's hard to change with the
 * standard Gtk accelerator-changing mechanism.
 */
static void
keyboard_shortcuts(GtkWidget *page)
{
	GtkWidget *vbox = pidgin_make_frame(page, _("Keyboard Shortcuts"));
	GtkWidget *checkbox;
	GtkAccelKey current = { 0, 0, 0 };
	GtkAccelMap *map = gtk_accel_map_get();

	/* Maybe it would be better just to hardcode the values?
	 * -- resiak, 2007-04-30
	 */
	if (ctrl_w.accel_key == 0)
	{
		gtk_accelerator_parse ("<Control>w", &(ctrl_w.accel_key),
			&(ctrl_w.accel_mods));
		g_assert(ctrl_w.accel_key != 0);

		gtk_accelerator_parse ("Escape", &(escape.accel_key),
			&(escape.accel_mods));
		g_assert(escape.accel_key != 0);
	}

	checkbox = gtk_check_button_new_with_mnemonic(
		_("Cl_ose conversations with the Escape key"));
	gtk_accel_map_lookup_entry(CONVERSATION_CLOSE_ACCEL_PATH, &current);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
		accel_is_escape(&current));

	escape_closes_conversation_cb_id = g_signal_connect(checkbox,
		"clicked", G_CALLBACK(escape_closes_conversation_cb), NULL);

	g_signal_connect_object(map, "changed::" CONVERSATION_CLOSE_ACCEL_PATH,
		G_CALLBACK(conversation_close_accel_changed_cb), checkbox, (GConnectFlags)0);

	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
}

/* sets the current buddy list theme */
static void
prefs_set_blist_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginBlistTheme *theme;
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_if_fail(gtk_combo_box_get_active_iter(combo_box, &iter));
	gtk_tree_model_get(GTK_TREE_MODEL(prefs_blist_themes), &iter, 2, &name, -1);

	theme = PIDGIN_BLIST_THEME(purple_theme_manager_find_theme(name, "blist"));
	g_free(name);

	pidgin_blist_set_theme(theme);
}

/* sets the current icon theme */
static void
prefs_set_status_icon_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginStatusIconTheme *theme;
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_if_fail(gtk_combo_box_get_active_iter(combo_box, &iter));
	gtk_tree_model_get(GTK_TREE_MODEL(prefs_status_icon_themes), &iter, 2, &name, -1);

	theme = PIDGIN_STATUS_ICON_THEME(purple_theme_manager_find_theme(name, "status-icon"));
	g_free(name);

	pidgin_stock_load_status_icon_theme(theme);
}

static GtkWidget *
interface_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *combo_box;
	GtkSizeGroup *sg;
	GList *names = NULL;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Buddy List Themes */
	vbox = pidgin_make_frame(ret, _("Buddy List Theme"));

	combo_box = prefs_build_theme_combo_box(prefs_blist_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/theme"));
	gtk_box_pack_start(GTK_BOX (vbox), combo_box, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(combo_box), "changed", (GCallback)prefs_set_blist_theme_cb, NULL);

	/* Status Icon Themes */
	combo_box = prefs_build_theme_combo_box(prefs_status_icon_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme"));
	gtk_box_pack_start(GTK_BOX (vbox), combo_box, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(combo_box), "changed", (GCallback)prefs_set_status_icon_theme_cb, NULL);

	/* System Tray */
	vbox = pidgin_make_frame(ret, _("System Tray Icon"));
	label = pidgin_prefs_dropdown(vbox, _("_Show system tray icon:"), PURPLE_PREF_STRING,
					PIDGIN_PREFS_ROOT "/docklet/show",
					_("Always"), "always",
					_("On unread messages"), "pending",
					_("Never"), "never",
					NULL);
	gtk_size_group_add_widget(sg, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	vbox = pidgin_make_frame(ret, _("Conversation Window Hiding"));
	label = pidgin_prefs_dropdown(vbox, _("_Hide new IM conversations:"),
					PURPLE_PREF_STRING, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
					_("Never"), "never",
					_("When away"), "away",
					_("Always"), "always",
					NULL);
	gtk_size_group_add_widget(sg, label);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);


	/* All the tab options! */
	vbox = pidgin_make_frame(ret, _("Tabs"));

	pidgin_prefs_checkbox(_("Show IMs and chats in _tabbed windows"),
							PIDGIN_PREFS_ROOT "/conversations/tabs", vbox);

	/*
	 * Connect a signal to the above preference.  When conversations are not
	 * shown in a tabbed window then all tabbing options should be disabled.
	 */
	vbox2 = gtk_vbox_new(FALSE, 9);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/conversations/tabs",
	                            conversation_usetabs_cb, vbox2);
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/tabs"))
	  gtk_widget_set_sensitive(vbox2, FALSE);

	pidgin_prefs_checkbox(_("Show close b_utton on tabs"),
				PIDGIN_PREFS_ROOT "/conversations/close_on_tabs", vbox2);

	label = pidgin_prefs_dropdown(vbox2, _("_Placement:"), PURPLE_PREF_INT,
					PIDGIN_PREFS_ROOT "/conversations/tab_side",
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
				PURPLE_PREF_STRING, PIDGIN_PREFS_ROOT "/conversations/placement", names);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	gtk_size_group_add_widget(sg, label);

	g_list_free(names);


	keyboard_shortcuts(ret);


	gtk_widget_show_all(ret);
	g_object_unref(sg);
	return ret;
}

#if GTK_CHECK_VERSION(2,4,0)
static void
pidgin_custom_font_set(GtkFontButton *font_button, gpointer nul)
{
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/custom_font",
				gtk_font_button_get_font_name(font_button));
}
#endif

static GtkWidget *
conv_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *iconpref1;
	GtkWidget *iconpref2;
	GtkWidget *fontpref;
	GtkWidget *imhtml;
	GtkWidget *frame;

#if GTK_CHECK_VERSION(2,4,0)
	GtkWidget *hbox;
	GtkWidget *font_button;
	const char *font_name;
#endif

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Conversations"));

	pidgin_prefs_checkbox(_("Show _formatting on incoming messages"),
				PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting", vbox);
	pidgin_prefs_checkbox(_("Close IMs immediately when the tab is closed"),
				PIDGIN_PREFS_ROOT "/conversations/im/close_immediately", vbox);

	iconpref1 = pidgin_prefs_checkbox(_("Show _detailed information"),
			PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons", vbox);
	iconpref2 = pidgin_prefs_checkbox(_("Enable buddy ic_on animation"),
			PIDGIN_PREFS_ROOT "/conversations/im/animate_buddy_icons", vbox);
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/im/show_buddy_icons"))
		gtk_widget_set_sensitive(iconpref2, FALSE);
	g_signal_connect(G_OBJECT(iconpref1), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), iconpref2);

	pidgin_prefs_checkbox(_("_Notify buddies that you are typing to them"),
			"/purple/conversations/im/send_typing", vbox);
#ifdef USE_GTKSPELL
	pidgin_prefs_checkbox(_("Highlight _misspelled words"),
			PIDGIN_PREFS_ROOT "/conversations/spellcheck", vbox);
#endif

	pidgin_prefs_checkbox(_("Use smooth-scrolling"), PIDGIN_PREFS_ROOT "/conversations/use_smooth_scrolling", vbox);

#ifdef _WIN32
	pidgin_prefs_checkbox(_("F_lash window when IMs are received"), PIDGIN_PREFS_ROOT "/win32/blink_im", vbox);

	pidgin_prefs_checkbox(_("Minimi_ze new conversation windows"), PIDGIN_PREFS_ROOT "/win32/minimize_new_convs", vbox);
#endif

	pidgin_prefs_labeled_spin_button(vbox,
		_("Minimum input area height in lines:"),
		PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		1, 8, NULL);


#if GTK_CHECK_VERSION(2,4,0)
	vbox = pidgin_make_frame(ret, _("Font"));
	if (purple_running_gnome())
		fontpref = pidgin_prefs_checkbox(_("Use document font from _theme"), PIDGIN_PREFS_ROOT "/conversations/use_theme_font", vbox);
	else
		fontpref = pidgin_prefs_checkbox(_("Use font from _theme"), PIDGIN_PREFS_ROOT "/conversations/use_theme_font", vbox);

	font_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
	font_button = gtk_font_button_new_with_font(font_name ? font_name : NULL);

	gtk_font_button_set_show_style(GTK_FONT_BUTTON(font_button), TRUE);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Conversation _font:"), NULL, font_button, FALSE, NULL);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font"))
		gtk_widget_set_sensitive(hbox, FALSE);
	g_signal_connect(G_OBJECT(fontpref), "clicked", G_CALLBACK(pidgin_toggle_sensitive), hbox);
	g_signal_connect(G_OBJECT(font_button), "font-set", G_CALLBACK(pidgin_custom_font_set), NULL);
#endif

	vbox = pidgin_make_frame(ret, _("Default Formatting"));
	gtk_box_set_child_packing(GTK_BOX(vbox->parent), vbox, TRUE, TRUE, 0, GTK_PACK_START);

	frame = pidgin_create_imhtml(TRUE, &imhtml, &toolbar, NULL);
	gtk_widget_show(frame);
	gtk_widget_set_name(imhtml, "pidgin_prefs_font_imhtml");
	gtk_widget_set_size_request(frame, 300, -1);
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

	gtk_imhtml_append_text(GTK_IMHTML(imhtml), _("This is how your outgoing message text will appear when you use protocols that support formatting."), 0);

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	gtk_imhtml_setup_entry(GTK_IMHTML(imhtml), PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_FORMATTING_WBFO);

	g_signal_connect_after(G_OBJECT(imhtml), "format_function_toggle",
					 G_CALLBACK(formatting_toggle_cb), toolbar);
	g_signal_connect_after(G_OBJECT(imhtml), "format_function_clear",
					 G_CALLBACK(formatting_clear_cb), NULL);


	gtk_widget_show(ret);

	return ret;
}

static void network_ip_changed(GtkEntry *entry, gpointer data)
{
	/*
	 * TODO: It would be nice if we could validate this and show a
	 *       red background in the box when the IP address is invalid
	 *       and a green background when the IP address is valid.
	 */
	purple_network_set_public_ip(gtk_entry_get_text(entry));
}

static void
proxy_changed_cb(const char *name, PurplePrefType type,
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
		purple_prefs_set_string("/purple/proxy/host", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPORT)
		purple_prefs_set_int("/purple/proxy/port", atoi(gtk_entry_get_text(entry)));
	else if (entrynum == PROXYUSER)
		purple_prefs_set_string("/purple/proxy/username", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPASS)
		purple_prefs_set_string("/purple/proxy/password", gtk_entry_get_text(entry));
}

static void
proxy_button_clicked_cb(GtkWidget *button, gpointer null)
{
	GError *err = NULL;

	if (g_spawn_command_line_async ("gnome-network-preferences", &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start proxy configuration program."), err->message);
	g_error_free(err);
}

static void
browser_button_clicked_cb(GtkWidget *button, gpointer null)
{
	GError *err = NULL;

	if (g_spawn_command_line_async ("gnome-default-applications-properties", &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start browser configuration program."), err->message);
	g_error_free(err);
}

static GtkWidget *
network_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox, *hbox, *entry;
	GtkWidget *table, *label, *auto_ip_checkbox, *ports_checkbox, *spin_button;
	GtkWidget *proxy_warning = NULL, *browser_warning = NULL;
	GtkWidget *proxy_button = NULL, *browser_button = NULL;
	GtkSizeGroup *sg;
	PurpleProxyInfo *proxy_info = NULL;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("IP Address"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	pidgin_prefs_labeled_entry(vbox,_("ST_UN server:"),
			"/purple/network/stun_server", sg);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
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
			"/purple/network/auto_ip", vbox);

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
	if (purple_network_get_my_ip(-1) != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry),
		                   purple_network_get_my_ip(-1));

	pidgin_set_accessible_label (entry, label);


	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		gtk_widget_set_sensitive(GTK_WIDGET(table), FALSE);
	}

	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), table);

	g_object_unref(sg);

	vbox = pidgin_make_frame (ret, _("Ports"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	pidgin_prefs_checkbox(_("_Enable automatic router port forwarding"),
			"/purple/network/map_ports", vbox);

	ports_checkbox = pidgin_prefs_checkbox(_("_Manually specify range of ports to listen on"),
			"/purple/network/ports_range_use", vbox);

	spin_button = pidgin_prefs_labeled_spin_button(vbox, _("_Start port:"),
			"/purple/network/ports_range_start", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	spin_button = pidgin_prefs_labeled_spin_button(vbox, _("_End port:"),
			"/purple/network/ports_range_end", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	if (purple_running_gnome()) {
		vbox = pidgin_make_frame(ret, _("Proxy Server &amp; Browser"));
		prefs_proxy_frame = gtk_vbox_new(FALSE, 0);

		proxy_warning = hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label),
		                     _("<b>Proxy configuration program was not found.</b>"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		browser_warning = hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		label = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label),
		                     _("<b>Browser configuration program was not found.</b>"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		label = gtk_label_new(_("Proxy & Browser preferences are configured\n"
		                        "in GNOME Preferences"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		proxy_button = gtk_button_new_with_mnemonic(_("Configure _Proxy"));
		g_signal_connect(G_OBJECT(proxy_button), "clicked",
		                 G_CALLBACK(proxy_button_clicked_cb), NULL);
		gtk_box_pack_start(GTK_BOX(hbox), proxy_button, FALSE, FALSE, 0);
		gtk_widget_show(proxy_button);
		browser_button = gtk_button_new_with_mnemonic(_("Configure _Browser"));
		g_signal_connect(G_OBJECT(browser_button), "clicked",
		                 G_CALLBACK(browser_button_clicked_cb), NULL);
		gtk_box_pack_start(GTK_BOX(hbox), browser_button, FALSE, FALSE, 0);
		gtk_widget_show(browser_button);
	} else {
		vbox = pidgin_make_frame(ret, _("Proxy Server"));
		prefs_proxy_frame = gtk_vbox_new(FALSE, 0);

		pidgin_prefs_dropdown(vbox, _("Proxy _type:"), PURPLE_PREF_STRING,
					"/purple/proxy/type",
					_("No proxy"), "none",
					"SOCKS 4", "socks4",
					"SOCKS 5", "socks5",
					"HTTP", "http",
					_("Use Environmental Settings"), "envvar",
					NULL);
		gtk_box_pack_start(GTK_BOX(vbox), prefs_proxy_frame, 0, 0, 0);
		proxy_info = purple_global_proxy_get_info();

		purple_prefs_connect_callback(prefs, "/purple/proxy/type",
					    proxy_changed_cb, prefs_proxy_frame);

		/* This is a global option that affects SOCKS4 usage even with account-specific proxy settings */
		pidgin_prefs_checkbox(_("Use remote DNS with SOCKS4 proxies"),
							  "/purple/proxy/socks4_remotedns", prefs_proxy_frame);

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

		if (proxy_info != NULL && purple_proxy_info_get_host(proxy_info))
			gtk_entry_set_text(GTK_ENTRY(entry),
					   purple_proxy_info_get_host(proxy_info));

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

		if (proxy_info != NULL && purple_proxy_info_get_port(proxy_info) != 0) {
			char buf[128];
			g_snprintf(buf, sizeof(buf), "%d",
				   purple_proxy_info_get_port(proxy_info));

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

		if (proxy_info != NULL && purple_proxy_info_get_username(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
						   purple_proxy_info_get_username(proxy_info));

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
#if !GTK_CHECK_VERSION(2,16,0)
		if (gtk_entry_get_invisible_char(GTK_ENTRY(entry)) == '*')
			gtk_entry_set_invisible_char(GTK_ENTRY(entry), PIDGIN_INVISIBLE_CHAR);
#endif /* Less than GTK+ 2.16 */
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPASS);

		if (proxy_info != NULL && purple_proxy_info_get_password(proxy_info) != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry),
					   purple_proxy_info_get_password(proxy_info));
		pidgin_set_accessible_label (entry, label);
	}

	gtk_widget_show_all(ret);
	g_object_unref(sg);
	/* Only hide table if not running gnome otherwise we hide the IP address table! */
	if (!purple_running_gnome() && (proxy_info == NULL ||
	    purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_NONE ||
	    purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_ENVVAR)) {
		gtk_widget_hide(table);
	} else if (purple_running_gnome()) {
		gchar *path;
		path = g_find_program_in_path("gnome-network-preferences");
		if (path != NULL) {
			gtk_widget_set_sensitive(proxy_button, TRUE);
			gtk_widget_hide(proxy_warning);
			g_free(path);
		} else {
			gtk_widget_set_sensitive(proxy_button, FALSE);
			gtk_widget_show(proxy_warning);
		}
		path = g_find_program_in_path("gnome-default-applications-properties");
		if (path != NULL) {
			gtk_widget_set_sensitive(browser_button, TRUE);
			gtk_widget_hide(browser_warning);
			g_free(path);
		} else {
			gtk_widget_set_sensitive(browser_button, FALSE);
			gtk_widget_show(browser_warning);
		}
	}

	return ret;
}

#ifndef _WIN32
static gboolean manual_browser_set(GtkWidget *entry, GdkEventFocus *event, gpointer data) {
	const char *program = gtk_entry_get_text(GTK_ENTRY(entry));

	purple_prefs_set_path(PIDGIN_PREFS_ROOT "/browsers/command", program);

	/* carry on normally */
	return FALSE;
}

static GList *get_available_browsers(void)
{
	struct browser {
		char *name;
		char *command;
	};

	/* Sorted reverse alphabetically */
	static const struct browser possible_browsers[] = {
		{N_("Seamonkey"), "seamonkey"},
		{N_("Opera"), "opera"},
		{N_("Netscape"), "netscape"},
		{N_("Mozilla"), "mozilla"},
		{N_("Konqueror"), "kfmclient"},
		{N_("Desktop Default"), "xdg-open"},
		{N_("GNOME Default"), "gnome-open"},
		{N_("Galeon"), "galeon"},
		{N_("Firefox"), "firefox"},
		{N_("Firebird"), "mozilla-firebird"},
		{N_("Epiphany"), "epiphany"}
	};
	static const int num_possible_browsers = G_N_ELEMENTS(possible_browsers);

	GList *browsers = NULL;
	int i = 0;
	char *browser_setting = (char *)purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser");

	browsers = g_list_prepend(browsers, (gpointer)"custom");
	browsers = g_list_prepend(browsers, (gpointer)_("Manual"));

	for (i = 0; i < num_possible_browsers; i++) {
		if (purple_program_is_valid(possible_browsers[i].command)) {
			browsers = g_list_prepend(browsers,
									  possible_browsers[i].command);
			browsers = g_list_prepend(browsers, (gpointer)_(possible_browsers[i].name));
			if(browser_setting && !strcmp(possible_browsers[i].command, browser_setting))
				browser_setting = NULL;
			/* If xdg-open is valid, prefer it over gnome-open and skip forward */
			if(!strcmp(possible_browsers[i].command, "xdg-open")) {
				if (browser_setting && !strcmp("gnome-open", browser_setting)) {
					purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", possible_browsers[i].command);
					browser_setting = NULL;
				}
				i++;
			}
		}
	}

	if(browser_setting)
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/browser", "custom");

	return browsers;
}

static void
browser_changed1_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, strcmp(browser, "custom"));
}

static void
browser_changed2_cb(const char *name, PurplePrefType type,
					gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *browser = value;

	gtk_widget_set_sensitive(hbox, !strcmp(browser, "custom"));
}

static GtkWidget *
browser_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkSizeGroup *sg;
	GList *browsers = NULL;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	vbox = pidgin_make_frame (ret, _("Browser Selection"));

	browsers = get_available_browsers();
	if (browsers != NULL) {
		label = pidgin_prefs_dropdown_from_list(vbox,_("_Browser:"), PURPLE_PREF_STRING,
										 PIDGIN_PREFS_ROOT "/browsers/browser",
										 browsers);
		g_list_free(browsers);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(sg, label);

		hbox = gtk_hbox_new(FALSE, 0);
		label = pidgin_prefs_dropdown(hbox, _("_Open link in:"), PURPLE_PREF_INT,
			PIDGIN_PREFS_ROOT "/browsers/place",
			_("Browser default"), PIDGIN_BROWSER_DEFAULT,
			_("Existing window"), PIDGIN_BROWSER_CURRENT,
			_("New window"), PIDGIN_BROWSER_NEW_WINDOW,
			_("New tab"), PIDGIN_BROWSER_NEW_TAB,
			NULL);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_size_group_add_widget(sg, label);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"), "custom"))
			gtk_widget_set_sensitive(hbox, FALSE);
		purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/browsers/browser",
									browser_changed1_cb, hbox);
	}

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry),
					   purple_prefs_get_path(PIDGIN_PREFS_ROOT "/browsers/command"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
					 G_CALLBACK(manual_browser_set), NULL);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Manual:\n(%s for URL)"), sg, entry, TRUE, NULL);
	if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"), "custom"))
		gtk_widget_set_sensitive(hbox, FALSE);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/browsers/browser",
			browser_changed2_cb, hbox);

	gtk_widget_show_all(ret);
	g_object_unref(sg);
	return ret;
}
#endif /*_WIN32*/

static GtkWidget *
logging_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GList *names;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);


	vbox = pidgin_make_frame (ret, _("Logging"));
	names = purple_log_logger_get_options();

	pidgin_prefs_dropdown_from_list(vbox, _("Log _format:"), PURPLE_PREF_STRING,
				 "/purple/logging/format", names);

	g_list_free(names);

	pidgin_prefs_checkbox(_("Log all _instant messages"),
				  "/purple/logging/log_ims", vbox);
	pidgin_prefs_checkbox(_("Log all c_hats"),
				  "/purple/logging/log_chats", vbox);
	pidgin_prefs_checkbox(_("Log all _status changes to system log"),
				  "/purple/logging/log_system", vbox);

	gtk_widget_show_all(ret);

	return ret;
}

#ifndef _WIN32
static gint sound_cmd_yeah(GtkEntry *entry, gpointer d)
{
	purple_prefs_set_path(PIDGIN_PREFS_ROOT "/sound/command",
			gtk_entry_get_text(GTK_ENTRY(entry)));
	return TRUE;
}

static void
sound_changed1_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox, !strcmp(method, "custom"));
}

static void
sound_changed2_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *vbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(vbox, strcmp(method, "none"));
}
#endif /* !_WIN32 */

#ifdef USE_GSTREAMER
static void
sound_changed3_cb(const char *name, PurplePrefType type,
				  gconstpointer value, gpointer data)
{
	GtkWidget *hbox = data;
	const char *method = value;

	gtk_widget_set_sensitive(hbox,
			!strcmp(method, "automatic") ||
			!strcmp(method, "esd"));
}
#endif /* USE_GSTREAMER */


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

	purple_prefs_set_bool(pref, !gtk_cell_renderer_toggle_get_active(cell));
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
	gboolean temp_enabled;
	gboolean temp_mute;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
			pidgin_sound_get_event_option(sound_row_sel));

	temp_enabled = purple_prefs_get_bool(pref);
	temp_mute = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute");

	if (!temp_enabled) purple_prefs_set_bool(pref, TRUE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", FALSE);

	purple_sound_play_event(sound_row_sel, NULL);

	if (!temp_enabled) purple_prefs_set_bool(pref, FALSE);
	if (temp_mute) purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute", TRUE);

	g_free(pref);
}

/*
 * Resets a sound file back to default.
 */
static void
reset_sound(GtkWidget *button, gpointer i_am_also_NULL)
{
	gchar *pref;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	purple_prefs_set_path(pref, "");
	g_free(pref);

	gtk_entry_set_text(GTK_ENTRY(sound_entry), _("(default)"));

	pref_sound_generate_markup();
}

static void
sound_chosen_cb(void *user_data, const char *filename)
{
	gchar *pref;
	int sound;

	sound = GPOINTER_TO_INT(user_data);

	/* Set it -- and forget it */
	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound));
	purple_prefs_set_path(pref, filename);
	g_free(pref);

	/*
	 * If the sound we just changed is still the currently selected
	 * sound, then update the box showing the file name.
	 */
	if (sound == sound_row_sel)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), filename);

	pref_sound_generate_markup();
}

static void select_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	gchar *pref;
	const char *filename;

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
						   pidgin_sound_get_event_option(sound_row_sel));
	filename = purple_prefs_get_path(pref);
	g_free(pref);

	if (*filename == '\0')
		filename = NULL;

	purple_request_file(prefs, _("Sound Selection"), filename, FALSE,
					  G_CALLBACK(sound_chosen_cb), NULL,
					  NULL, NULL, NULL,
					  GINT_TO_POINTER(sound_row_sel));
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
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/sound/volume", val);
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

	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			pidgin_sound_get_event_option(sound_row_sel));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	if (sound_entry)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : _("(default)"));
	g_value_unset (&val);

	pref_sound_generate_markup();
}


static void
mute_changed_cb(const char *pref_name,
                PurplePrefType pref_type,
                gconstpointer val,
                gpointer data)
{
	GtkToggleButton *button = data;
	gboolean muted = GPOINTER_TO_INT(val);

	g_return_if_fail(!strcmp (pref_name, PIDGIN_PREFS_ROOT "/sound/mute"));

	/* Block the handler that re-sets the preference. */
	g_signal_handlers_block_matched(button, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)pref_name);
	gtk_toggle_button_set_active (button, muted);
	g_signal_handlers_unblock_matched(button, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)pref_name);
}


static GtkWidget *
sound_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox, *sw, *button, *combo_box;
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
	GtkWidget *entry;
	const char *cmd;
#endif

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

#ifndef _WIN32
	vbox = pidgin_make_frame (ret, _("Sound Method"));
	dd = pidgin_prefs_dropdown(vbox, _("_Method:"), PURPLE_PREF_STRING,
			PIDGIN_PREFS_ROOT "/sound/method",
			_("Console beep"), "beep",
#ifdef USE_GSTREAMER
			_("Automatic"), "automatic",
			"ESD", "esd",
			"ALSA", "alsa",
#endif
			_("Command"), "custom",
			_("No sounds"), "none",
			NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
	cmd = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/sound/command");
	if(cmd)
		gtk_entry_set_text(GTK_ENTRY(entry), cmd);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(sound_cmd_yeah), NULL);

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Sound c_ommand:\n(%s for filename)"), sg, entry, TRUE, NULL);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
								sound_changed1_cb, hbox);
	gtk_widget_set_sensitive(hbox,
			!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"),
					"custom"));
#endif /* _WIN32 */

	vbox = pidgin_make_frame (ret, _("Sound Options"));

	button = pidgin_prefs_checkbox(_("M_ute sounds"), PIDGIN_PREFS_ROOT "/sound/mute", vbox);
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/mute", mute_changed_cb, button);

	pidgin_prefs_checkbox(_("Sounds when conversation has _focus"),
				   PIDGIN_PREFS_ROOT "/sound/conv_focus", vbox);
	pidgin_prefs_dropdown(vbox, _("_Enable sounds:"),
				 PURPLE_PREF_INT, "/purple/sound/while_status",
				_("Only when available"), 1,
				_("Only when not available"), 2,
				_("Always"), 3,
				NULL);

#ifdef USE_GSTREAMER
	sw = gtk_hscale_new_with_range(0.0, 100.0, 5.0);
	gtk_range_set_increments(GTK_RANGE(sw), 5.0, 25.0);
	gtk_range_set_value(GTK_RANGE(sw), purple_prefs_get_int(PIDGIN_PREFS_ROOT "/sound/volume"));
	g_signal_connect (G_OBJECT (sw), "format-value",
			  G_CALLBACK (prefs_sound_volume_format),
			  NULL);
	g_signal_connect (G_OBJECT (sw), "value-changed",
			  G_CALLBACK (prefs_sound_volume_changed),
			  NULL);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("V_olume:"), NULL, sw, TRUE, NULL);

	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
								sound_changed3_cb, hbox);
	sound_changed3_cb(PIDGIN_PREFS_ROOT "/sound/method", PURPLE_PREF_STRING,
			  purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), hbox);
#endif

#ifndef _WIN32
	gtk_widget_set_sensitive(vbox,
			strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), "none"));
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/sound/method",
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

	/* SOUND THEMES */
	combo_box = prefs_build_theme_combo_box(prefs_sound_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme"));
	pref_sound_generate_markup();
	gtk_box_pack_start(GTK_BOX (vbox), combo_box, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(combo_box), "changed", (GCallback)prefs_set_sound_theme_cb, NULL);

	/* SOUND SELECTION */
	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_widget_set_size_request(sw, -1, 100);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	event_store = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	for (j=0; j < PURPLE_NUM_SOUNDS; j++) {
		char *pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/enabled/%s",
					     pidgin_sound_get_event_option(j));
		const char *label = pidgin_sound_get_event_label(j);

		if (label == NULL) {
			g_free(pref);
			continue;
		}

		gtk_list_store_append (event_store, &iter);
		gtk_list_store_set(event_store, &iter,
				   0, purple_prefs_get_bool(pref),
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

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	sound_entry = gtk_entry_new();
	pref = g_strdup_printf(PIDGIN_PREFS_ROOT "/sound/file/%s",
			       pidgin_sound_get_event_option(0));
	file = purple_prefs_get_path(pref);
	g_free(pref);
	gtk_entry_set_text(GTK_ENTRY(sound_entry), (file && *file != '\0') ? file : _("(default)"));
	gtk_editable_set_editable(GTK_EDITABLE(sound_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sound_entry, FALSE, FALSE, PIDGIN_HIG_BOX_SPACE);

	button = gtk_button_new_with_mnemonic(_("_Browse..."));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(select_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_mnemonic(_("Pre_view"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(test_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_mnemonic(_("_Reset"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(reset_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}


static void
set_idle_away(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/idleaway", purple_savedstatus_get_creation_time(status));
}

static void
set_startupstatus(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/startup", purple_savedstatus_get_creation_time(status));
}

static GtkWidget *
away_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *dd;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *select;
	GtkWidget *menu;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Idle stuff */
	vbox = pidgin_make_frame(ret, _("Idle"));

	dd = pidgin_prefs_dropdown(vbox, _("_Report idle time:"),
		PURPLE_PREF_STRING, "/purple/away/idle_reporting",
		_("Never"), "none",
		_("From last sent message"), "purple",
#if defined(USE_SCREENSAVER) || defined(HAVE_IOKIT)
		_("Based on keyboard or mouse use"), "system",
#endif
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	/* Away stuff */
	vbox = pidgin_make_frame(ret, _("Away"));

	dd = pidgin_prefs_dropdown(vbox, _("_Auto-reply:"),
		PURPLE_PREF_STRING, "/purple/away/auto_reply",
		_("Never"), "never",
		_("When away"), "away",
		_("When both away and idle"), "awayidle",
		NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0.5);

	/* Auto-away stuff */
	vbox = pidgin_make_frame(ret, _("Auto-away"));

	select = pidgin_prefs_labeled_spin_button(vbox,
			_("_Minutes before becoming idle:"), "/purple/away/mins_before_away",
			1, 24 * 60, sg);

	button = pidgin_prefs_checkbox(_("Change status when _idle"),
						   "/purple/away/away_when_idle", vbox);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(), G_CALLBACK(set_idle_away));
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Change _status to:"), sg, menu, TRUE, &label);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);

	if (!purple_prefs_get_bool("/purple/away/away_when_idle")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	/* Signon status stuff */
	vbox = pidgin_make_frame(ret, _("Status at Startup"));

	button = pidgin_prefs_checkbox(_("Use status from last _exit at startup"),
		"/purple/savedstatus/startup_current_status", vbox);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(), G_CALLBACK(set_startupstatus));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Status to a_pply at startup:"), sg, menu, TRUE, &label);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);

	if (purple_prefs_get_bool("/purple/savedstatus/startup_current_status")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	gtk_widget_show_all(ret);
	g_object_unref(sg);

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

static void prefs_notebook_init(void) {
	prefs_notebook_add_page(_("Interface"), interface_page(), notebook_page++);
	prefs_notebook_add_page(_("Conversations"), conv_page(), notebook_page++);
	prefs_notebook_add_page(_("Smiley Themes"), theme_page(), notebook_page++);
	prefs_notebook_add_page(_("Sounds"), sound_page(), notebook_page++);
	prefs_notebook_add_page(_("Network"), network_page(), notebook_page++);
#ifndef _WIN32
	/* We use the registered default browser in windows */
	/* if the user is running gnome 2.x or Mac OS X, hide the browsers tab */
	if ((purple_running_gnome() == FALSE) && (purple_running_osx() == FALSE)) {
		prefs_notebook_add_page(_("Browser"), browser_page(), notebook_page++);
	}
#endif
	prefs_notebook_add_page(_("Logging"), logging_page(), notebook_page++);
	prefs_notebook_add_page(_("Status / Idle"), away_page(), notebook_page++);
}

void pidgin_prefs_show(void)
{
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *button;

	if (prefs) {
		gtk_window_present(GTK_WINDOW(prefs));
		return;
	}

	/* Refresh the list of themes before showing the preferences window */
	purple_theme_manager_refresh();

	/* add everything in the theme manager before the window is loaded */
	if (prefs_themes_unsorted) {
		purple_theme_manager_for_each_theme(prefs_themes_sort);
		prefs_themes_unsorted = FALSE;
	}
	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	/* (that should have been "effect," right?) */

	/* Back to instant-apply! I win!  BU-HAHAHA! */

	/* Create the window */
	prefs = pidgin_create_dialog(_("Preferences"), PIDGIN_HIG_BORDER, "preferences", FALSE);
	g_signal_connect(G_OBJECT(prefs), "destroy",
					 G_CALLBACK(delete_prefs), NULL);

	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(prefs), FALSE, PIDGIN_HIG_BORDER);

	/* The notebook */
	prefsnotebook = notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(prefsnotebook);

	button = pidgin_dialog_add_button(GTK_DIALOG(prefs), GTK_STOCK_CLOSE, NULL, NULL);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), prefs);

	prefs_notebook_init();

	/* Show everything. */
	gtk_widget_show(prefs);
}

static void
set_bool_pref(GtkWidget *w, const char *key)
{
	purple_prefs_set_bool(key,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

GtkWidget *
pidgin_prefs_checkbox(const char *text, const char *key, GtkWidget *page)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
								 purple_prefs_get_bool(key));

	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(set_bool_pref), (char *)key);

	gtk_widget_show(button);

	return button;
}

static void
smiley_theme_pref_cb(const char *name, PurplePrefType type,
					 gconstpointer value, gpointer data)
{
	const char *themename = value;
	GSList *themes;

	for (themes = smiley_themes; themes; themes = themes->next) {
		struct smiley_theme *smile = themes->data;
		if (smile->name && strcmp(themename, smile->name) == 0) {
			pidgin_themes_load_smiley_theme(smile->path, TRUE);
			break;
		}
	}
}

void
pidgin_prefs_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "");
	purple_prefs_add_none("/plugins/gtk");

#ifndef _WIN32
	/* Browsers */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/browsers");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/browsers/place", PIDGIN_BROWSER_DEFAULT);
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/browsers/command", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/browser", "mozilla");
#endif

	/* Plugins */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/plugins");
	purple_prefs_add_path_list(PIDGIN_PREFS_ROOT "/plugins/loaded", NULL);

	/* File locations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/filelocations");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", "");

	/* Themes */
	prefs_themes_init();

	/* Smiley Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/smileys");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/smileys/theme", "Default");

	/* Smiley Callbacks */
	purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/smileys/theme",
								smiley_theme_pref_cb, NULL);

	pidgin_prefs_update_old();
}

void pidgin_prefs_update_old()
{
	const char *str;

	purple_prefs_rename("/gaim/gtk", PIDGIN_PREFS_ROOT);

	/* Rename some old prefs */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_ims", "/purple/logging/log_ims");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_chats", "/purple/logging/log_chats");
	purple_prefs_rename("/purple/conversations/placement",
					  PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_rename(PIDGIN_PREFS_ROOT "/debug/timestamps", "/purple/debug/timestamps");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	purple_prefs_rename_boolean_toggle(PIDGIN_PREFS_ROOT "/conversations/ignore_colors",
									 PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting");

	/* this string pref moved into the core, try to be friendly */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/idle/reporting_method", "/purple/away/idle_reporting");
	if ((str = purple_prefs_get_string("/purple/away/idle_reporting")) &&
			strcmp(str, "gaim") == 0)
		purple_prefs_set_string("/purple/away/idle_reporting", "purple");

	/* Remove some no-longer-used prefs */
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/auto_expand_contacts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/button_style");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/grey_idle_buddies");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_group_count");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_warning_level");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/button_type");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ctrl_enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/escape_closes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/html_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/icons_on_tabs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/send_formatting");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_smileys");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_urls_as_links");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/smiley_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_bgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_fgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_font");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_size");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/old_tab_complete");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/tab_completion");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/hide_on_send");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/color_nicks");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_fonts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_font_sizes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/passthrough_unknown_commands");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/idle");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/logging/individual_logs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/signon");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/silent_signon");

	/* Convert old queuing prefs to hide_new 3-way pref. */
	if (purple_prefs_exists("/plugins/gtk/docklet/queue_messages") &&
	    purple_prefs_get_bool("/plugins/gtk/docklet/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "always");
	}
	else if (purple_prefs_exists(PIDGIN_PREFS_ROOT "/away/queue_messages") &&
	         purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/away/queue_messages"))
	{
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new", "away");
	}
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away/queue_messages");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away");
	purple_prefs_remove("/plugins/gtk/docklet/queue_messages");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_height");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_height");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/x",
			PIDGIN_PREFS_ROOT "/conversations/im/x");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/y",
			PIDGIN_PREFS_ROOT "/conversations/im/y");
}

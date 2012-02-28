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
#include "nat-pmp.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sound.h"
#include "sound-theme.h"
#include "stun.h"
#include "theme-manager.h"
#include "upnp.h"
#include "util.h"
#include "network.h"
#include "keyring.h"

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconv-theme.h"
#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksound.h"
#include "gtkstatus-icon-theme.h"
#include "gtkthemes.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "gtkwebviewtoolbar.h"
#include "pidginstock.h"

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYUSER 2
#define PROXYPASS 3

#define PREFS_OPTIMAL_ICON_SIZE 32

struct theme_info {
	gchar *type;
	gchar *extension;
	gchar *original_name;
};

/* Main dialog */
static GtkWidget *prefs = NULL;

/* Notebook */
static GtkWidget *prefsnotebook = NULL;
static int notebook_page = 0;

/* Conversations page */
static GtkWidget *sample_webview = NULL;

/* Themes page */
static GtkWidget *prefs_sound_themes_combo_box;
static GtkWidget *prefs_blist_themes_combo_box;
static GtkWidget *prefs_conv_themes_combo_box;
static GtkWidget *prefs_conv_variants_combo_box;
static GtkWidget *prefs_status_themes_combo_box;
static GtkWidget *prefs_smiley_themes_combo_box;

/* Sound theme specific */
static GtkWidget *sound_entry = NULL;
static int sound_row_sel = 0;
static gboolean prefs_sound_themes_loading;

/* These exist outside the lifetime of the prefs dialog */
static GtkListStore *prefs_sound_themes;
static GtkListStore *prefs_blist_themes;
static GtkListStore *prefs_conv_themes;
static GtkListStore *prefs_conv_variants;
static GtkListStore *prefs_status_icon_themes;
static GtkListStore *prefs_smiley_themes;

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

	adjust = gtk_adjustment_new(val, min, max, 1, 1, 0);
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
entry_set(GtkEntry *entry, gpointer data)
{
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

GtkWidget *
pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

/* TODO: Maybe move this up somewheres... */
enum {
	PREF_DROPDOWN_TEXT,
	PREF_DROPDOWN_VALUE,
	PREF_DROPDOWN_COUNT
};

static void
dropdown_set(GObject *w, const char *key)
{
	const char *str_value;
	int int_value;
	gboolean bool_value;
	PurplePrefType type;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;

	tree_model = gtk_combo_box_get_model(GTK_COMBO_BOX(w));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(w), &iter))
		return;

	type = GPOINTER_TO_INT(g_object_get_data(w, "type"));

	if (type == PURPLE_PREF_INT) {
		gtk_tree_model_get(tree_model, &iter,
		                   PREF_DROPDOWN_VALUE, &int_value,
		                   -1);

		purple_prefs_set_int(key, int_value);
	}
	else if (type == PURPLE_PREF_STRING) {
		gtk_tree_model_get(tree_model, &iter,
		                   PREF_DROPDOWN_VALUE, &str_value,
		                   -1);

		purple_prefs_set_string(key, str_value);
	}
	else if (type == PURPLE_PREF_BOOLEAN) {
		gtk_tree_model_get(tree_model, &iter,
		                   PREF_DROPDOWN_VALUE, &bool_value,
		                   -1);

		purple_prefs_set_bool(key, bool_value);
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	GtkWidget  *dropdown;
	GtkWidget  *label = NULL;
	gchar      *text;
	const char *stored_str = NULL;
	int         stored_int = 0;
	gboolean    stored_bool = FALSE;
	int         int_value  = 0;
	const char *str_value  = NULL;
	gboolean    bool_value = FALSE;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;
	GtkCellRenderer *renderer;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (type == PURPLE_PREF_INT) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_INT);
		stored_int = purple_prefs_get_int(key);
	} else if (type == PURPLE_PREF_STRING) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_STRING);
		stored_str = purple_prefs_get_string(key);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_BOOLEAN);
		stored_bool = purple_prefs_get_bool(key);
	} else {
		g_warn_if_reached();
		return NULL;
	}

	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	g_object_set_data(G_OBJECT(dropdown), "type", GINT_TO_POINTER(type));

	while (menuitems != NULL && (text = (char *)menuitems->data) != NULL) {
		menuitems = g_list_next(menuitems);
		g_return_val_if_fail(menuitems != NULL, NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PREF_DROPDOWN_TEXT, text,
		                   -1);

		if (type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((type == PURPLE_PREF_INT && stored_int == int_value) ||
			(type == PURPLE_PREF_STRING && stored_str != NULL &&
			 !strcmp(stored_str, str_value)) ||
			(type == PURPLE_PREF_BOOLEAN &&
			 (stored_bool == bool_value))) {

			active = iter;
		}

		menuitems = g_list_next(menuitems);
	}

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dropdown), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dropdown), renderer,
	                               "text", 0,
	                               NULL);

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dropdown), &active);

	g_signal_connect(G_OBJECT(dropdown), "changed",
	                 G_CALLBACK(dropdown_set), (char *)key);

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

	/* NULL-ify globals */
	sound_entry = NULL;
	sound_row_sel = 0;
	prefs_sound_themes_loading = FALSE;

	prefs_sound_themes_combo_box = NULL;
	prefs_blist_themes_combo_box = NULL;
	prefs_conv_themes_combo_box = NULL;
	prefs_conv_variants_combo_box = NULL;
	prefs_status_themes_combo_box = NULL;
	prefs_smiley_themes_combo_box = NULL;

	sample_webview = NULL;

	notebook_page = 0;
	prefsnotebook = NULL;
	prefs = NULL;
}

static gchar *
get_theme_markup(const char *name, gboolean custom, const char *author,
				 const char *description)
{

	return g_strdup_printf("<b>%s</b>%s%s%s%s\n<span foreground='dim grey'>%s</span>",
						   name, custom ? " " : "", custom ? _("(Custom)") : "",
						   author != NULL ? " - " : "", author != NULL ? author : "",
						   description != NULL ? description : "");
}

static void
smileys_refresh_theme_list(void)
{
	GdkPixbuf *pixbuf;
	GSList *themes;
	GtkTreeIter iter;

	pidgin_themes_smiley_theme_probe();

	if (!(themes = smiley_themes))
		return;

	while (themes) {
		struct smiley_theme *theme = themes->data;
		char *description = get_theme_markup(_(theme->name), FALSE,
		                                     _(theme->author), _(theme->desc));
		gtk_list_store_append(prefs_smiley_themes, &iter);

		/*
		 * LEAK - Gentoo memprof thinks pixbuf is leaking here... but it
		 * looks like it should be ok to me.  Anyone know what's up?  --Mark
		 */
		pixbuf = (theme->icon ? pidgin_pixbuf_new_from_file(theme->icon) : NULL);

		gtk_list_store_set(prefs_smiley_themes, &iter,
				   0, pixbuf,
				   1, description,
				   2, theme->name,
				   -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

		g_free(description);
		themes = themes->next;
	}
}

/* Rebuild the markup for the sound theme selection for "(Custom)" themes */
static void
pref_sound_generate_markup(void)
{
	gboolean print_custom, customized;
	const gchar *author, *description, *current_theme;
	gchar *name, *markup;
	PurpleSoundTheme *theme;
	GtkTreeIter iter;

	customized = pidgin_sound_is_customized();
	current_theme = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme");

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(prefs_sound_themes), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &iter, 2, &name, -1);

			print_custom = customized && name && g_str_equal(current_theme, name);

			if (!name || *name == '\0') {
				g_free(name);
				name = g_strdup(_("Default"));
				author = _("Penguin Pimps");
				description = _("The default Pidgin sound theme");
			} else {
				theme = PURPLE_SOUND_THEME(purple_theme_manager_find_theme(name, "sound"));
				author = purple_theme_get_author(PURPLE_THEME(theme));
				description = purple_theme_get_description(PURPLE_THEME(theme));
			}

			markup = get_theme_markup(name, print_custom, author, description);

			gtk_list_store_set(prefs_sound_themes, &iter, 1, markup, -1);

			g_free(name);
			g_free(markup);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(prefs_sound_themes), &iter));
	}
}

/* adds the themes to the theme list from the manager so they can be displayed in prefs */
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
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		gtk_list_store_append(prefs_sound_themes, &iter);
		gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, purple_theme_get_name(theme), -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	} else if (PIDGIN_IS_BLIST_THEME(theme) || PIDGIN_IS_STATUS_ICON_THEME(theme)){
		GtkListStore *store;

		if (PIDGIN_IS_BLIST_THEME(theme))
			store = prefs_blist_themes;
		else
			store = prefs_status_icon_themes;

		image_full = purple_theme_get_image_full(theme);
		if (image_full != NULL){
			pixbuf = pidgin_pixbuf_new_from_file_at_scale(image_full, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
			g_free(image_full);
		} else
			pixbuf = NULL;

		name = purple_theme_get_name(theme);
		author = purple_theme_get_author(theme);
		description = purple_theme_get_description(theme);

		markup = get_theme_markup(name, FALSE, author, description);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, pixbuf, 1, markup, 2, name, -1);

		g_free(markup);
		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));

	} else if (PIDGIN_IS_CONV_THEME(theme)) {
		/* No image available? */

		name = purple_theme_get_name(theme);
		/* No author available */
		/* No description available */

		markup = get_theme_markup(name, FALSE, NULL, NULL);

		gtk_list_store_append(prefs_conv_themes, &iter);
		gtk_list_store_set(prefs_conv_themes, &iter, 1, markup, 2, name, -1);
	}
}

static void
prefs_set_active_theme_combo(GtkWidget *combo_box, GtkListStore *store, const gchar *current_theme)
{
	GtkTreeIter iter;
	gchar *theme = NULL;
	gboolean unset = TRUE;

	if (current_theme && *current_theme && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
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
}

static void
prefs_themes_refresh(void)
{
	GdkPixbuf *pixbuf = NULL;
	gchar *tmp;
	GtkTreeIter iter;

	prefs_sound_themes_loading = TRUE;
	/* refresh the list of themes in the manager */
	purple_theme_manager_refresh();

	tmp = g_build_filename(DATADIR, "icons", "hicolor", "32x32", "apps", "pidgin.png", NULL);
	pixbuf = pidgin_pixbuf_new_from_file_at_scale(tmp, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE, TRUE);
	g_free(tmp);

	/* sound themes */
	gtk_list_store_clear(prefs_sound_themes);
	gtk_list_store_append(prefs_sound_themes, &iter);
	gtk_list_store_set(prefs_sound_themes, &iter, 0, pixbuf, 2, "", -1);

	/* blist themes */
	gtk_list_store_clear(prefs_blist_themes);
	gtk_list_store_append(prefs_blist_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin buddy list theme"));
	gtk_list_store_set(prefs_blist_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);

	/* conversation themes */
	gtk_list_store_clear(prefs_conv_themes);
	gtk_list_store_append(prefs_conv_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin conversation theme"));
	gtk_list_store_set(prefs_conv_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);

	/* conversation theme variants */
	gtk_list_store_clear(prefs_conv_variants);

	/* status icon themes */
	gtk_list_store_clear(prefs_status_icon_themes);
	gtk_list_store_append(prefs_status_icon_themes, &iter);
	tmp = get_theme_markup(_("Default"), FALSE, _("Penguin Pimps"),
		_("The default Pidgin status icon theme"));
	gtk_list_store_set(prefs_status_icon_themes, &iter, 0, pixbuf, 1, tmp, 2, "", -1);
	g_free(tmp);
	if (pixbuf)
		g_object_unref(G_OBJECT(pixbuf));

	/* smiley themes */
	gtk_list_store_clear(prefs_smiley_themes);

	purple_theme_manager_for_each_theme(prefs_themes_sort);
	pref_sound_generate_markup();
	smileys_refresh_theme_list();

	/* set active */
	prefs_set_active_theme_combo(prefs_sound_themes_combo_box, prefs_sound_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/theme"));
	prefs_set_active_theme_combo(prefs_blist_themes_combo_box, prefs_blist_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/theme"));
	prefs_set_active_theme_combo(prefs_conv_themes_combo_box, prefs_conv_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/theme"));
	prefs_set_active_theme_combo(prefs_status_themes_combo_box, prefs_status_icon_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme"));
	prefs_set_active_theme_combo(prefs_smiley_themes_combo_box, prefs_smiley_themes, purple_prefs_get_string(PIDGIN_PREFS_ROOT "/smileys/theme"));
	prefs_sound_themes_loading = FALSE;
}

/* init all the theme variables so that the themes can be sorted later and used by pref pages */
static void
prefs_themes_init(void)
{
	prefs_sound_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_blist_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_conv_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_conv_variants = gtk_list_store_new(1, G_TYPE_STRING);

	prefs_status_icon_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

	prefs_smiley_themes = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
}

static PurpleTheme *
prefs_theme_find_theme(const gchar *path, const gchar *type)
{
	PurpleTheme *theme = purple_theme_manager_load_theme(path, type);
	GDir *dir = g_dir_open(path, 0, NULL);
	const gchar *next;

	while (!PURPLE_IS_THEME(theme) && (next = g_dir_read_name(dir))) {
		gchar *next_path = g_build_filename(path, next, NULL);

		if (g_file_test(next_path, G_FILE_TEST_IS_DIR))
			theme = prefs_theme_find_theme(next_path, type);

		g_free(next_path);
	}

	g_dir_close(dir);

	return theme;
}

/* Eww. Seriously ewww. But thanks, grim! This is taken from guifications2 */
static gboolean
purple_theme_file_copy(const gchar *source, const gchar *destination)
{
	FILE *src, *dest;
	gint chr = EOF;

	if(!(src = g_fopen(source, "rb")))
		return FALSE;
	if(!(dest = g_fopen(destination, "wb"))) {
		fclose(src);
		return FALSE;
	}

	while((chr = fgetc(src)) != EOF) {
		fputc(chr, dest);
	}

	fclose(dest);
	fclose(src);

	return TRUE;
}

static void
free_theme_info(struct theme_info *info)
{
	if (info != NULL) {
		g_free(info->type);
		g_free(info->extension);
		g_free(info->original_name);
		g_free(info);
	}
}

/* installs a theme, info is freed by function */
static void
theme_install_theme(char *path, struct theme_info *info)
{
#ifndef _WIN32
	gchar *command;
#endif
	gchar *destdir;
	const char *tail;
	gboolean is_smiley_theme, is_archive;
	PurpleTheme *theme = NULL;

	if (info == NULL)
		return;

	/* check the extension */
	tail = info->extension ? info->extension : strrchr(path, '.');

	if (!tail) {
		free_theme_info(info);
		return;
	}

	is_archive = !g_ascii_strcasecmp(tail, ".gz") || !g_ascii_strcasecmp(tail, ".tgz");

	/* Just to be safe */
	g_strchomp(path);

	if ((is_smiley_theme = g_str_equal(info->type, "smiley")))
		destdir = g_build_filename(purple_user_dir(), "smileys", NULL);
	else
		destdir = g_build_filename(purple_user_dir(), "themes", "temp", NULL);

	/* We'll check this just to make sure. This also lets us do something different on
	 * other platforms, if need be */
	if (is_archive) {
#ifndef _WIN32
		gchar *path_escaped = g_shell_quote(path);
		gchar *destdir_escaped = g_shell_quote(destdir);

		if (!g_file_test(destdir, G_FILE_TEST_IS_DIR))
			purple_build_dir(destdir, S_IRUSR | S_IWUSR | S_IXUSR);

		command = g_strdup_printf("tar > /dev/null xzf %s -C %s", path_escaped, destdir_escaped);
		g_free(path_escaped);
		g_free(destdir_escaped);

		/* Fire! */
		if (system(command)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL);
			g_free(command);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
#else
		if (!winpidgin_gz_untar(path, destdir)) {
			purple_notify_error(NULL, NULL, _("Theme failed to unpack."), NULL);
			g_free(destdir);
			free_theme_info(info);
			return;
		}
#endif
	}

	if (is_smiley_theme) {
		/* just extract the folder to the smiley directory */
		prefs_themes_refresh();

	} else if (is_archive) {
		theme = prefs_theme_find_theme(destdir, info->type);

		if (PURPLE_IS_THEME(theme)) {
			/* create the location for the theme */
			gchar *theme_dest = g_build_filename(purple_user_dir(), "themes",
						 purple_theme_get_name(theme),
						 "purple", info->type, NULL);

			if (!g_file_test(theme_dest, G_FILE_TEST_IS_DIR))
				purple_build_dir(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);

			g_free(theme_dest);
			theme_dest = g_build_filename(purple_user_dir(), "themes",
						 purple_theme_get_name(theme),
						 "purple", info->type, NULL);

			/* move the entire directory to new location */
			g_rename(purple_theme_get_dir(theme), theme_dest);

			g_free(theme_dest);
			g_remove(destdir);
			g_object_unref(theme);

			prefs_themes_refresh();

		} else {
			/* something was wrong with the theme archive */
			g_unlink(destdir);
			purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL);
		}

	} else { /* just a single file so copy it to a new temp directory and attempt to load it*/
		gchar *temp_path, *temp_file;

		temp_path = g_build_filename(purple_user_dir(), "themes", "temp", "sub_folder", NULL);

		if (info->original_name != NULL) {
			/* name was changed from the original (probably a dnd) change it back before loading */
			temp_file = g_build_filename(temp_path, info->original_name, NULL);

		} else {
			gchar *source_name = g_path_get_basename(path);
			temp_file = g_build_filename(temp_path, source_name, NULL);
			g_free(source_name);
		}

		if (!g_file_test(temp_path, G_FILE_TEST_IS_DIR))
			purple_build_dir(temp_path, S_IRUSR | S_IWUSR | S_IXUSR);

		if (purple_theme_file_copy(path, temp_file)) {
			/* find the theme, could be in subfolder */
			theme = prefs_theme_find_theme(temp_path, info->type);

			if (PURPLE_IS_THEME(theme)) {
				gchar *theme_dest = g_build_filename(purple_user_dir(), "themes",
							 purple_theme_get_name(theme),
							 "purple", info->type, NULL);

				if(!g_file_test(theme_dest, G_FILE_TEST_IS_DIR))
					purple_build_dir(theme_dest, S_IRUSR | S_IWUSR | S_IXUSR);

				g_rename(purple_theme_get_dir(theme), theme_dest);

				g_free(theme_dest);
				g_object_unref(theme);

				prefs_themes_refresh();
			} else {
				g_remove(temp_path);
				purple_notify_error(NULL, NULL, _("Theme failed to load."), NULL);
			}
		} else {
			purple_notify_error(NULL, NULL, _("Theme failed to copy."), NULL);
		}

		g_free(temp_file);
		g_free(temp_path);
	}

	g_free(destdir);
	free_theme_info(info);
}

static void
theme_got_url(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *themedata, size_t len, const gchar *error_message)
{
	FILE *f;
	gchar *path;
	size_t wc;

	if ((error_message != NULL) || (len == 0)) {
		free_theme_info(user_data);
		return;
	}

	f = purple_mkstemp(&path, TRUE);
	wc = fwrite(themedata, len, 1, f);
	if (wc != 1) {
		purple_debug_warning("theme_got_url", "Unable to write theme data.\n");
		fclose(f);
		g_unlink(path);
		g_free(path);
		free_theme_info(user_data);
		return;
	}
	fclose(f);

	theme_install_theme(path, user_data);

	g_unlink(path);
	g_free(path);
}

static void
theme_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer user_data)
{
	gchar *name = g_strchomp((gchar *)sd->data);

	if ((sd->length >= 0) && (sd->format == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		gchar *temp;
		struct theme_info *info =  g_new0(struct theme_info, 1);
		info->type = g_strdup((gchar *)user_data);
		info->extension = g_strdup(g_strrstr(name,"."));
		temp = g_strrstr(name, "/");
		info->original_name = temp ? g_strdup(++temp) : NULL;

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			/* It looks like we're dealing with a local file. Let's
			 * just untar it in the right place */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug(PURPLE_DEBUG_ERROR, "theme dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				free_theme_info(info);
				return;
			}
			theme_install_theme(tmp, info);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7)) {
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			purple_util_fetch_url(name, TRUE, NULL, FALSE, -1, theme_got_url, info);
		} else if (!g_ascii_strncasecmp(name, "https://", 8)) {
			/* purple_util_fetch_url() doesn't support HTTPS, but we want users
			 * to be able to drag and drop links from the SF trackers, so
			 * we'll try it as an HTTP URL. */
			char *tmp = g_strdup(name + 1);
			tmp[0] = 'h';
			tmp[1] = 't';
			tmp[2] = 't';
			tmp[3] = 'p';

			purple_util_fetch_url(tmp, TRUE, NULL, FALSE, -1, theme_got_url, info);
			g_free(tmp);
		} else
			free_theme_info(info);

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}

/* builds a theme combo box from a list store with colums: icon preview, markup, theme name */
static GtkWidget *
prefs_build_theme_combo_box(GtkListStore *store, const char *current_theme, const char *type)
{
	GtkCellRenderer *cell_rend;
	GtkWidget *combo_box;
	GtkTargetEntry te[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};

	g_return_val_if_fail(store != NULL && current_theme != NULL, NULL);

	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

	cell_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_fixed_size(cell_rend, PREFS_OPTIMAL_ICON_SIZE, PREFS_OPTIMAL_ICON_SIZE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "pixbuf", 0, NULL);

	cell_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT (combo_box), cell_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "markup", 1, NULL);
	g_object_set(cell_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_drag_dest_set(combo_box, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, te,
					sizeof(te) / sizeof(GtkTargetEntry) , GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(combo_box), "drag_data_received", G_CALLBACK(theme_dnd_recv), (gpointer) type);

	return combo_box;
}

/* sets the current sound theme */
static void
prefs_set_sound_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gint i;
	gchar *pref;
	gchar *new_theme;
	GtkTreeIter new_iter;

	if(gtk_combo_box_get_active_iter(combo_box, &new_iter) && !prefs_sound_themes_loading) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_sound_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/sound/theme", new_theme);

		/* New theme removes all customization */
		for(i = 0; i < PURPLE_NUM_SOUNDS; i++){
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
}

/* sets the current smiley theme */
static void
prefs_set_smiley_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	gchar *new_theme;
	GtkTreeIter new_iter;

	if (gtk_combo_box_get_active_iter(combo_box, &new_iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_smiley_themes), &new_iter, 2, &new_theme, -1);

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/smileys/theme", new_theme);
#if 0
/* TODO: WebKit-ify smileys */
		pidgin_themes_smiley_themeize(sample_webview);
#endif

		g_free(new_theme);
	}
}


/* Does same as normal sort, except "none" is sorted first */
static gint pidgin_sort_smileys (GtkTreeModel	*model,
						GtkTreeIter		*a,
						GtkTreeIter		*b,
						gpointer		userdata)
{
	gint ret = 0;
	gchar *name1 = NULL, *name2 = NULL;

	gtk_tree_model_get(model, a, 2, &name1, -1);
	gtk_tree_model_get(model, b, 2, &name2, -1);

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
		ret = purple_utf8_strcasecmp(name1, name2);
	}

	g_free(name1);
	g_free(name2);

	return ret;
}

/* sets the current buddy list theme */
static void
prefs_set_blist_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginBlistTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_blist_themes), &iter, 2, &name, -1);

		if(!name || !g_str_equal(name, ""))
			theme = PIDGIN_BLIST_THEME(purple_theme_manager_find_theme(name, "blist"));

		g_free(name);

		pidgin_blist_set_theme(theme);
	}
}

/* sets the current conversation theme */
static void
prefs_set_conv_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginConvTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if (gtk_combo_box_get_active_iter(combo_box, &iter)) {
		const GList *variants;
		const char *current_variant;
		gboolean unset = TRUE;

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_themes), &iter, 2, &name, -1);
		if (!name || !*name) {
			g_free(name);
			return;
		}

		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/theme", name);

		/* Update list of variants */
		gtk_list_store_clear(prefs_conv_variants);

		theme = PIDGIN_CONV_THEME(purple_theme_manager_find_theme(name, "conversation"));
		current_variant = pidgin_conversation_theme_get_variant(theme);

		variants = pidgin_conversation_theme_get_variants(theme);
		for (; variants && current_variant; variants = g_list_next(variants)) {
			gtk_list_store_append(prefs_conv_variants, &iter);
			gtk_list_store_set(prefs_conv_variants, &iter, 0, variants->data, -1);

			if (g_str_equal(variants->data, current_variant)) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(prefs_conv_variants_combo_box), &iter);
				unset = FALSE;
			}
		}

		if (unset)
			gtk_combo_box_set_active(GTK_COMBO_BOX(prefs_conv_variants_combo_box), 0);

		g_free(name);
	}
}

/* sets the current conversation theme variant */
static void
prefs_set_conv_variant_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginConvTheme *theme =  NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(prefs_conv_themes_combo_box), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_themes), &iter, 2, &name, -1);
		theme = PIDGIN_CONV_THEME(purple_theme_manager_find_theme(name, "conversation"));
		g_free(name);

		if (gtk_combo_box_get_active_iter(combo_box, &iter)) {
			gtk_tree_model_get(GTK_TREE_MODEL(prefs_conv_variants), &iter, 0, &name, -1);
			pidgin_conversation_theme_set_variant(theme, name);
			g_free(name);
		}
	}
}

/* sets the current icon theme */
static void
prefs_set_status_icon_theme_cb(GtkComboBox *combo_box, gpointer user_data)
{
	PidginStatusIconTheme *theme = NULL;
	GtkTreeIter iter;
	gchar *name = NULL;

	if(gtk_combo_box_get_active_iter(combo_box, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(prefs_status_icon_themes), &iter, 2, &name, -1);

		if(!name || !g_str_equal(name, ""))
			theme = PIDGIN_STATUS_ICON_THEME(purple_theme_manager_find_theme(name, "status-icon"));

		g_free(name);

		pidgin_stock_load_status_icon_theme(theme);
		pidgin_blist_refresh(purple_get_blist());
	}
}

static GtkWidget *
add_theme_prefs_combo(GtkWidget *vbox,
                      GtkSizeGroup *combo_sg, GtkSizeGroup *label_sg,
                      GtkListStore *theme_store,
                      GCallback combo_box_cb, gpointer combo_box_cb_user_data,
                      const char *label_str, const char *prefs_path,
                      const char *theme_type)
{
	GtkWidget *label;
	GtkWidget *combo_box = NULL;
	GtkWidget *themesel_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

	label = gtk_label_new(label_str);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(label_sg, label);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), label, FALSE, FALSE, 0);

	combo_box = prefs_build_theme_combo_box(theme_store,
						purple_prefs_get_string(prefs_path),
						theme_type);
	g_signal_connect(G_OBJECT(combo_box), "changed",
						(GCallback)combo_box_cb, combo_box_cb_user_data);
	gtk_size_group_add_widget(combo_sg, combo_box);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), combo_box, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), themesel_hbox, FALSE, FALSE, 0);

	return combo_box;
}

static GtkWidget *
add_child_theme_prefs_combo(GtkWidget *vbox, GtkSizeGroup *combo_sg,
                             GtkSizeGroup *label_sg, GtkListStore *theme_store,
                             GCallback combo_box_cb, gpointer combo_box_cb_user_data,
                             const char *label_str)
{
	GtkWidget *label;
	GtkWidget *combo_box;
	GtkWidget *themesel_hbox;
	GtkCellRenderer *cell_rend;

	themesel_hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), themesel_hbox, FALSE, FALSE, 0);

	label = gtk_label_new(label_str);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_size_group_add_widget(label_sg, label);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), label, FALSE, FALSE, 0);

	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(theme_store));

	cell_rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), cell_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), cell_rend, "text", 0, NULL);
	g_object_set(cell_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	g_signal_connect(G_OBJECT(combo_box), "changed",
						(GCallback)combo_box_cb, combo_box_cb_user_data);
	gtk_size_group_add_widget(combo_sg, combo_box);
	gtk_box_pack_start(GTK_BOX(themesel_hbox), combo_box, TRUE, TRUE, 0);

	return combo_box;
}

static GtkWidget *
theme_page(void)
{
	GtkWidget *label;
	GtkWidget *ret, *vbox;
	GtkSizeGroup *label_sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup *combo_sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame(ret, _("Theme Selections"));

	/* Instructions */
	label = gtk_label_new(_("Select a theme that you would like to use from "
							"the lists below.\nNew themes can be installed by "
							"dragging and dropping them onto the theme list."));

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, FALSE, 0);
	gtk_widget_show(label);

	/* Buddy List Themes */
	prefs_blist_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_blist_themes,
		(GCallback)prefs_set_blist_theme_cb, NULL,
		_("Buddy List Theme:"), PIDGIN_PREFS_ROOT "/blist/theme", "blist");

	/* Conversation Themes */
	prefs_conv_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_conv_themes,
		(GCallback)prefs_set_conv_theme_cb, NULL,
		_("Conversation Theme:"), PIDGIN_PREFS_ROOT "/conversations/theme", "conversation");

	/* Conversation Theme Variants */
	prefs_conv_variants_combo_box = add_child_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_conv_variants,
		(GCallback)prefs_set_conv_variant_cb, NULL, _("\tVariant:"));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(prefs_conv_variants),
	                                     0, GTK_SORT_ASCENDING);

	/* Status Icon Themes */
	prefs_status_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_status_icon_themes,
		(GCallback)prefs_set_status_icon_theme_cb, NULL,
		_("Status Icon Theme:"), PIDGIN_PREFS_ROOT "/status/icon-theme", "icon");

	/* Sound Themes */
	prefs_sound_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_sound_themes,
		(GCallback)prefs_set_sound_theme_cb, NULL,
		_("Sound Theme:"), PIDGIN_PREFS_ROOT "/sound/theme", "sound");

	/* Smiley Themes */
	prefs_smiley_themes_combo_box = add_theme_prefs_combo(
		vbox, combo_sg, label_sg, prefs_smiley_themes,
		(GCallback)prefs_set_smiley_theme_cb, NULL,
		_("Smiley Theme:"), PIDGIN_PREFS_ROOT "/smileys/theme", "smiley");

	/* Custom sort so "none" theme is at top of list */
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(prefs_smiley_themes),
	                                2, pidgin_sort_smileys, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(prefs_smiley_themes),
										 2, GTK_SORT_ASCENDING);

	gtk_widget_show_all(ret);

	return ret;
}

static void
formatting_toggle_cb(GtkWebView *webview, GtkWebViewButtons buttons, void *toolbar)
{
	gboolean bold, italic, uline, strike;

	gtk_webview_get_current_format(webview, &bold, &italic, &uline, &strike);

	if (buttons & GTK_WEBVIEW_BOLD)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold",
		                      bold);
	if (buttons & GTK_WEBVIEW_ITALIC)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic",
		                      italic);
	if (buttons & GTK_WEBVIEW_UNDERLINE)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline",
		                      uline);
	if (buttons & GTK_WEBVIEW_STRIKE)
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike",
		                      strike);

	if (buttons & GTK_WEBVIEW_GROW || buttons & GTK_WEBVIEW_SHRINK)
		purple_prefs_set_int(PIDGIN_PREFS_ROOT "/conversations/font_size",
		                     gtk_webview_get_current_fontsize(webview));
	if (buttons & GTK_WEBVIEW_FACE) {
		char *face = gtk_webview_get_current_fontface(webview);

		if (face)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", face);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/font_face", "");

		g_free(face);
	}

	if (buttons & GTK_WEBVIEW_FORECOLOR) {
		char *color = gtk_webview_get_current_forecolor(webview);

		if (color)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", color);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/fgcolor", "");

		g_free(color);
	}

	if (buttons & GTK_WEBVIEW_BACKCOLOR) {
		char *color = gtk_webview_get_current_backcolor(webview);

		if (color)
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", color);
		else
			purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/bgcolor", "");

		g_free(color);
	}
}

static void
formatting_clear_cb(GtkWebView *webview, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_bold", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_italic", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_underline", FALSE);
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/conversations/send_strike", FALSE);

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

static GtkWidget *
interface_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *label;
	GtkSizeGroup *sg;
	GList *names = NULL;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

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

	vbox = pidgin_make_frame(ret, _("Conversation Window"));
	label = pidgin_prefs_dropdown(vbox, _("_Hide new IM conversations:"),
					PURPLE_PREF_STRING, PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
					_("Never"), "never",
					_("When away"), "away",
					_("Always"), "always",
					NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

#ifdef _WIN32
	pidgin_prefs_checkbox(_("Minimi_ze new conversation windows"), PIDGIN_PREFS_ROOT "/win32/minimize_new_convs", vbox);
#endif

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
					_("Left Vertical"), GTK_POS_LEFT|8,
					_("Right Vertical"), GTK_POS_RIGHT|8,
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

#ifdef _WIN32
static void
apply_custom_font(void)
{
	PangoFontDescription *desc = NULL;
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font")) {
		const char *font = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
		desc = pango_font_description_from_string(font);
	}

	gtk_widget_modify_font(sample_webview, desc);
	if (desc)
		pango_font_description_free(desc);

}
static void
pidgin_custom_font_set(GtkFontButton *font_button, gpointer nul)
{

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/custom_font",
				gtk_font_button_get_font_name(font_button));

	apply_custom_font();
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
	GtkWidget *webview;
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *checkbox;
	GtkWidget *spin_button;

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
#endif
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

	checkbox = pidgin_prefs_checkbox(_("Resize incoming custom smileys"),
			PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys", hbox);

	spin_button = pidgin_prefs_labeled_spin_button(hbox,
		_("Maximum size:"),
		PIDGIN_PREFS_ROOT "/conversations/custom_smileys_size",
		16, 512, NULL);

	if (!purple_prefs_get_bool(
				PIDGIN_PREFS_ROOT "/conversations/resize_custom_smileys"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);

	g_signal_connect(G_OBJECT(checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), NULL, NULL, hbox, TRUE, NULL);

	pidgin_prefs_labeled_spin_button(vbox,
		_("Minimum input area height in lines:"),
		PIDGIN_PREFS_ROOT "/conversations/minimum_entry_lines",
		1, 8, NULL);

#ifdef _WIN32
	{
	GtkWidget *fontpref, *font_button, *hbox;
	const char *font_name;
	vbox = pidgin_make_frame(ret, _("Font"));

	fontpref = pidgin_prefs_checkbox(_("Use font from _theme"),
									 PIDGIN_PREFS_ROOT "/conversations/use_theme_font", vbox);

	font_name = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/conversations/custom_font");
	if ((font_name == NULL) || (*font_name == '\0')) {
		font_button = gtk_font_button_new();
	} else {
		font_button = gtk_font_button_new_with_font(font_name);
	}

	gtk_font_button_set_show_style(GTK_FONT_BUTTON(font_button), TRUE);
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Conversation _font:"), NULL, font_button, FALSE, NULL);
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/conversations/use_theme_font"))
		gtk_widget_set_sensitive(hbox, FALSE);
	g_signal_connect(G_OBJECT(fontpref), "clicked", G_CALLBACK(pidgin_toggle_sensitive), hbox);
	g_signal_connect(G_OBJECT(fontpref), "clicked", G_CALLBACK(apply_custom_font), hbox);
	g_signal_connect(G_OBJECT(font_button), "font-set", G_CALLBACK(pidgin_custom_font_set), NULL);

	}
#endif

	vbox = pidgin_make_frame(ret, _("Default Formatting"));

	frame = pidgin_create_webview(TRUE, &webview, &toolbar, NULL);
	gtk_widget_show(frame);
	gtk_widget_set_name(webview, "pidgin_prefs_font_webview");
	gtk_widget_set_size_request(frame, 450, -1);
	gtk_webview_set_whole_buffer_formatting_only(GTK_WEBVIEW(webview), TRUE);
	gtk_webview_set_format_functions(GTK_WEBVIEW(webview),
	                                 GTK_WEBVIEW_BOLD |
	                                 GTK_WEBVIEW_ITALIC |
	                                 GTK_WEBVIEW_UNDERLINE |
	                                 GTK_WEBVIEW_STRIKE |
	                                 GTK_WEBVIEW_GROW |
	                                 GTK_WEBVIEW_SHRINK |
	                                 GTK_WEBVIEW_FACE |
	                                 GTK_WEBVIEW_FORECOLOR |
	                                 GTK_WEBVIEW_BACKCOLOR);

	gtk_webview_append_html(GTK_WEBVIEW(webview),
	                        _("This is how your outgoing message text will "
	                          "appear when you use protocols that support "
	                          "formatting."));

	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	gtk_webview_setup_entry(GTK_WEBVIEW(webview),
	                        PURPLE_CONNECTION_HTML |
	                        PURPLE_CONNECTION_FORMATTING_WBFO);

	g_signal_connect_after(G_OBJECT(webview), "format-toggled",
	                       G_CALLBACK(formatting_toggle_cb), toolbar);
	g_signal_connect_after(G_OBJECT(webview), "format-cleared",
	                       G_CALLBACK(formatting_clear_cb), NULL);
	sample_webview = webview;

	gtk_widget_show(ret);

	return ret;
}

static void
network_ip_changed(GtkEntry *entry, gpointer data)
{
	const gchar *text = gtk_entry_get_text(entry);
	GdkColor color;

	if (text && *text) {
		if (purple_ip_address_is_valid(text)) {
			color.red = 0xAFFF;
			color.green = 0xFFFF;
			color.blue = 0xAFFF;

			purple_network_set_public_ip(text);
		} else {
			color.red = 0xFFFF;
			color.green = 0xAFFF;
			color.blue = 0xAFFF;
		}

		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, &color);

	} else {
		purple_network_set_public_ip("");
		gtk_widget_modify_base(GTK_WIDGET(entry), GTK_STATE_NORMAL, NULL);
	}
}

static gboolean
network_stun_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/stun_server",
		gtk_entry_get_text(entry));
	purple_network_set_stun_server(gtk_entry_get_text(entry));

	return FALSE;
}

static gboolean
network_turn_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/turn_server",
		gtk_entry_get_text(entry));
	purple_network_set_turn_server(gtk_entry_get_text(entry));

	return FALSE;
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

static void
proxy_print_option(GtkEntry *entry, int entrynum)
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
proxy_button_clicked_cb(GtkWidget *button, gchar *program)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(program, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start proxy configuration program."), err->message);
	g_error_free(err);
}

#ifndef _WIN32
static void
browser_button_clicked_cb(GtkWidget *button, gchar *path)
{
	GError *err = NULL;

	if (g_spawn_command_line_async(path, &err))
		return;

	purple_notify_error(NULL, NULL, _("Cannot start browser configuration program."), err->message);
	g_error_free(err);
}
#endif

static void
auto_ip_button_clicked_cb(GtkWidget *button, gpointer null)
{
	const char *ip;
	PurpleStunNatDiscovery *stun;
	char *auto_ip_text;

	/* purple_network_get_my_ip will return the IP that was set by the user with
	   purple_network_set_public_ip, so make a lookup for the auto-detected IP
	   ourselves. */

	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED)) {
			ip = stun->publicip;
		} else {
			/* Attempt to get the IP from a NAT device using UPnP */
			ip = purple_upnp_get_public_ip();
			if (ip == NULL) {
				/* Attempt to get the IP from a NAT device using NAT-PMP */
				ip = purple_pmp_get_public_ip();
				if (ip == NULL) {
					/* Just fetch the IP of the local system */
					ip = purple_network_get_local_system_ip(-1);
				}
			}
		}
	}
	else
		ip = _("Disabled");

	auto_ip_text = g_strdup_printf(_("Use _automatically detected IP address: %s"), ip);
	gtk_button_set_label(GTK_BUTTON(button), auto_ip_text);
	g_free(auto_ip_text);
}

static GtkWidget *
network_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox, *hbox, *entry;
	GtkWidget *label, *auto_ip_checkbox, *ports_checkbox, *spin_button;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("IP Address"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(
			"/purple/network/stun_server"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(network_stun_server_changed_cb), NULL);
	gtk_widget_show(entry);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("ST_UN server:"),
			sg, entry, TRUE, NULL);

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

	auto_ip_checkbox = pidgin_prefs_checkbox("Use _automatically detected IP address",
	                                         "/purple/network/auto_ip", vbox);
	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
	                 G_CALLBACK(auto_ip_button_clicked_cb), NULL);
	auto_ip_button_clicked_cb(auto_ip_checkbox, NULL); /* Update label */

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_network_get_public_ip());
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(network_ip_changed), NULL);

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Public _IP:"),
			sg, entry, TRUE, NULL);

	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		gtk_widget_set_sensitive(GTK_WIDGET(hbox), FALSE);
	}

	g_signal_connect(G_OBJECT(auto_ip_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), hbox);

	g_object_unref(sg);

	vbox = pidgin_make_frame (ret, _("Ports"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	pidgin_prefs_checkbox(_("_Enable automatic router port forwarding"),
			"/purple/network/map_ports", vbox);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

	ports_checkbox = pidgin_prefs_checkbox(_("_Manually specify range of ports to listen on:"),
			"/purple/network/ports_range_use", hbox);

	spin_button = pidgin_prefs_labeled_spin_button(hbox, _("_Start:"),
			"/purple/network/ports_range_start", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	spin_button = pidgin_prefs_labeled_spin_button(hbox, _("_End:"),
			"/purple/network/ports_range_end", 0, 65535, sg);
	if (!purple_prefs_get_bool("/purple/network/ports_range_use"))
		gtk_widget_set_sensitive(GTK_WIDGET(spin_button), FALSE);
	g_signal_connect(G_OBJECT(ports_checkbox), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), spin_button);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), NULL, NULL, hbox, TRUE, NULL);

	g_object_unref(sg);

	/* TURN server */
	vbox = pidgin_make_frame(ret, _("Relay Server (TURN)"));
	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), purple_prefs_get_string(
			"/purple/network/turn_server"));
	g_signal_connect(G_OBJECT(entry), "focus-out-event",
			G_CALLBACK(network_turn_server_changed_cb), NULL);
	gtk_widget_show(entry);

	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_TURN server:"),
			sg, entry, TRUE, NULL);

	pidgin_prefs_labeled_spin_button(hbox, _("_UDP Port:"),
		"/purple/network/turn_port", 0, 65535, NULL);

	pidgin_prefs_labeled_spin_button(hbox, _("T_CP Port:"),
		"/purple/network/turn_port_tcp", 0, 65535, NULL);

	hbox = pidgin_prefs_labeled_entry(vbox, _("Use_rname:"),
		"/purple/network/turn_username", sg);
	pidgin_prefs_labeled_password(hbox, _("Pass_word:"),
		"/purple/network/turn_password", NULL);

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}

#ifndef _WIN32
static gboolean
manual_browser_set(GtkWidget *entry, GdkEventFocus *event, gpointer data)
{
	const char *program = gtk_entry_get_text(GTK_ENTRY(entry));

	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", program);

	/* carry on normally */
	return FALSE;
}

static GList *
get_available_browsers(void)
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
		{N_("Google Chrome"), "google-chrome"},
		/* Do not move the line below.  Code below expects gnome-open to be in
		 * this list immediately after xdg-open! */
		{N_("Desktop Default"), "xdg-open"},
		{N_("GNOME Default"), "gnome-open"},
		{N_("Galeon"), "galeon"},
		{N_("Firefox"), "firefox"},
		{N_("Firebird"), "mozilla-firebird"},
		{N_("Epiphany"), "epiphany"},
		/* Translators: please do not translate "chromium-browser" here! */
		{N_("Chromium (chromium-browser)"), "chromium-browser"},
		/* Translators: please do not translate "chrome" here! */
		{N_("Chromium (chrome)"), "chrome"}
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
	GtkWidget *ret, *vbox, *hbox, *label, *entry, *browser_button;
	GtkSizeGroup *sg;
	GList *browsers = NULL;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width (GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	vbox = pidgin_make_frame (ret, _("Browser Selection"));

	if (purple_running_gnome()) {
		gchar *path;

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		label = gtk_label_new(_("Browser preferences are configured in GNOME preferences"));
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		path = g_find_program_in_path("gnome-control-center");
		if (path != NULL) {
			gchar *tmp = g_strdup_printf("%s info", path);
			g_free(path);
			path = tmp;
		} else {
			path = g_find_program_in_path("gnome-default-applications-properties");
		}

		if (path == NULL) {
			label = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(label),
								 _("<b>Browser configuration program was not found.</b>"));
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		} else {
			browser_button = gtk_button_new_with_mnemonic(_("Configure _Browser"));
			g_signal_connect_data(G_OBJECT(browser_button), "clicked",
			                      G_CALLBACK(browser_button_clicked_cb), path,
			                      (GClosureNotify)g_free, 0);
			gtk_box_pack_start(GTK_BOX(hbox), browser_button, FALSE, FALSE, 0);
		}

		gtk_widget_show_all(ret);
	} else {
		sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

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
						   purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/manual_command"));
		g_signal_connect(G_OBJECT(entry), "focus-out-event",
						 G_CALLBACK(manual_browser_set), NULL);
		hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Manual:\n(%s for URL)"), sg, entry, TRUE, NULL);
		if (strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/browsers/browser"), "custom"))
			gtk_widget_set_sensitive(hbox, FALSE);
		purple_prefs_connect_callback(prefs, PIDGIN_PREFS_ROOT "/browsers/browser",
				browser_changed2_cb, hbox);

		gtk_widget_show_all(ret);
		g_object_unref(sg);
	}

	return ret;
}
#endif /*_WIN32*/

static GtkWidget *
proxy_page(void)
{
	GtkWidget *ret = NULL, *vbox = NULL, *hbox = NULL;
	GtkWidget *table = NULL, *entry = NULL, *label = NULL, *proxy_button = NULL;
	GtkWidget *prefs_proxy_frame = NULL;
	PurpleProxyInfo *proxy_info;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);
	vbox = pidgin_make_frame(ret, _("Proxy Server"));
	prefs_proxy_frame = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);

	if(purple_running_gnome()) {
		gchar *path = NULL;

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		label = gtk_label_new(_("Proxy preferences are configured in GNOME preferences"));
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		path = g_find_program_in_path("gnome-network-properties");
		if (path == NULL)
			path = g_find_program_in_path("gnome-network-preferences");
		if (path == NULL) {
			path = g_find_program_in_path("gnome-control-center");
			if (path != NULL) {
				char *tmp = g_strdup_printf("%s network", path);
				g_free(path);
				path = tmp;
			}
		}

		if (path == NULL) {
			label = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(label),
								 _("<b>Proxy configuration program was not found.</b>"));
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		} else {
			proxy_button = gtk_button_new_with_mnemonic(_("Configure _Proxy"));
			g_signal_connect(G_OBJECT(proxy_button), "clicked",
							 G_CALLBACK(proxy_button_clicked_cb),
							 path);
			gtk_box_pack_start(GTK_BOX(hbox), proxy_button, FALSE, FALSE, 0);
		}

		/* NOTE: path leaks, but only when the prefs window is destroyed,
		         which is never */
		gtk_widget_show_all(ret);
	} else {
		GtkWidget *prefs_proxy_subframe = gtk_vbox_new(FALSE, 0);

		/* This is a global option that affects SOCKS4 usage even with
		 * account-specific proxy settings */
		pidgin_prefs_checkbox(_("Use remote _DNS with SOCKS4 proxies"),
							  "/purple/proxy/socks4_remotedns", prefs_proxy_frame);
		gtk_box_pack_start(GTK_BOX(vbox), prefs_proxy_frame, 0, 0, 0);

		pidgin_prefs_dropdown(prefs_proxy_frame, _("Proxy t_ype:"), PURPLE_PREF_STRING,
					"/purple/proxy/type",
					_("No proxy"), "none",
					_("SOCKS 4"), "socks4",
					_("SOCKS 5"), "socks5",
					_("Tor/Privacy (SOCKS5)"), "tor",
					_("HTTP"), "http",
					_("Use Environmental Settings"), "envvar",
					NULL);
		gtk_box_pack_start(GTK_BOX(prefs_proxy_frame), prefs_proxy_subframe, 0, 0, 0);
		proxy_info = purple_global_proxy_get_info();

		gtk_widget_show_all(ret);

		purple_prefs_connect_callback(prefs, "/purple/proxy/type",
					    proxy_changed_cb, prefs_proxy_subframe);

		table = gtk_table_new(4, 2, FALSE);
		gtk_container_set_border_width(GTK_CONTAINER(table), 0);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);
		gtk_table_set_row_spacings(GTK_TABLE(table), 10);
		gtk_container_add(GTK_CONTAINER(prefs_proxy_subframe), table);

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

		label = gtk_label_new_with_mnemonic(_("P_ort:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);

		entry = gtk_spin_button_new_with_range(0, 65535, 1);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_table_attach(GTK_TABLE(table), entry, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);
		g_signal_connect(G_OBJECT(entry), "changed",
				 G_CALLBACK(proxy_print_option), (void *)PROXYPORT);

		if (proxy_info != NULL && purple_proxy_info_get_port(proxy_info) != 0) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(entry),
				purple_proxy_info_get_port(proxy_info));
		}
		pidgin_set_accessible_label (entry, label);

		label = gtk_label_new_with_mnemonic(_("User_name:"));
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

		proxy_changed_cb("/purple/proxy/type", PURPLE_PREF_STRING,
			purple_prefs_get_string("/purple/proxy/type"),
			prefs_proxy_subframe);

	}

	return ret;
}

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

static void
change_master_password_cb(GtkWidget *button, gpointer ptr)
{
	purple_keyring_change_master(NULL, NULL);
}

static void
keyring_page_pref_changed(const char *name, PurplePrefType type, gconstpointer val, gpointer data)
{
	GtkWidget *button = data;
	PurpleKeyring *keyring;

	g_return_if_fail(type == PURPLE_PREF_STRING);
	g_return_if_fail(g_strcmp0(name, "/purple/keyring/active") == 0);

	/**
	 * This part is annoying.
	 * Since we do not know if purple_keyring_inuse was changed yet,
	 * as we do not know the order the callbacks are called in, we
	 * have to rely on the prefs system, and find the keyring that
	 * is (or will be) used, from there.
	 */

	keyring = purple_keyring_find_keyring_by_id(val);

	if (purple_keyring_get_change_master(keyring))
		gtk_widget_set_sensitive(button, TRUE);
	else
		gtk_widget_set_sensitive(button, FALSE);
}

static GtkWidget *
keyring_page(void)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *button;
	GList *names;
	void *prefs;
	const char *keyring_id;
	PurpleKeyring *keyring;

	keyring_id = purple_prefs_get_string("/purple/keyring/active");
	keyring = purple_keyring_find_keyring_by_id(keyring_id);

	prefs = purple_prefs_get_handle();

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER (ret), PIDGIN_HIG_BORDER);

	/*  Keyring selection */
	vbox = pidgin_make_frame(ret, _("Keyring"));
	names = purple_keyring_get_options();
	pidgin_prefs_dropdown_from_list(vbox, _("Keyring :"), PURPLE_PREF_STRING,
				 "/purple/keyring/active", names);
	g_list_free(names);

	/* Change master password */
	button = gtk_button_new_with_mnemonic(_("_Change master password."));
	
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(change_master_password_cb), NULL);
	purple_prefs_connect_callback (prefs, "/purple/keyring/active", keyring_page_pref_changed, button);

	if (purple_keyring_get_change_master(keyring))
		gtk_widget_set_sensitive(button, TRUE);
	else
		gtk_widget_set_sensitive(button, FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 1);
	gtk_widget_show_all(ret);

	return ret;
}

#ifndef _WIN32
static gint
sound_cmd_yeah(GtkEntry *entry, gpointer d)
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
			!strcmp(method, "alsa") ||
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

static void
select_sound(GtkWidget *button, gpointer being_NULL_is_fun)
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
static gchar *
prefs_sound_volume_format(GtkScale *scale, gdouble val)
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

static void
prefs_sound_volume_changed(GtkRange *range)
{
	int val = (int)gtk_range_get_value(GTK_RANGE(range));
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/sound/volume", val);
}
#endif

static void
prefs_sound_sel(GtkTreeSelection *sel, GtkTreeModel *model)
{
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
	GtkWidget *vbox, *vbox2, *sw, *button;
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

	vbox2 = pidgin_make_frame(ret, _("Sound Options"));

	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), vbox, FALSE, FALSE, 0);

#ifndef _WIN32
	dd = pidgin_prefs_dropdown(vbox2, _("_Method:"), PURPLE_PREF_STRING,
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

	/* SOUND SELECTION */
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
	gtk_box_pack_start(GTK_BOX(vbox),
		pidgin_make_scrollable(event_view, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, 100),
		TRUE, TRUE, 0);

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
	GtkWidget *hbox;
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

	select = pidgin_prefs_labeled_spin_button(vbox,
			_("_Minutes before becoming idle:"), "/purple/away/mins_before_away",
			1, 24 * 60, sg);

	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = pidgin_prefs_checkbox(_("Change to this status when _idle:"),
						   "/purple/away/away_when_idle", hbox);
	gtk_size_group_add_widget(sg, button);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(), G_CALLBACK(set_idle_away));
	gtk_size_group_add_widget(sg, menu);
	gtk_box_pack_start(GTK_BOX(hbox), menu, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);

	if(!purple_prefs_get_bool("/purple/away/away_when_idle"))
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);

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

	/* Signon status stuff */
	vbox = pidgin_make_frame(ret, _("Status at Startup"));

	button = pidgin_prefs_checkbox(_("Use status from last _exit at startup"),
		"/purple/savedstatus/startup_current_status", vbox);
	gtk_size_group_add_widget(sg, button);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(), G_CALLBACK(set_startupstatus));
	gtk_size_group_add_widget(sg, menu);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(pidgin_toggle_sensitive), menu);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("Status to a_pply at startup:"), sg, menu, TRUE, &label);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive), label);

	if(purple_prefs_get_bool("/purple/savedstatus/startup_current_status")) {
		gtk_widget_set_sensitive(GTK_WIDGET(menu), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(label), FALSE);
	}

	gtk_widget_show_all(ret);
	g_object_unref(sg);

	return ret;
}

static int
prefs_notebook_add_page(const char *text, GtkWidget *page, int ind)
{
	return gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
}

static void
prefs_notebook_init(void)
{
	prefs_notebook_add_page(_("Interface"), interface_page(), notebook_page++);

#ifndef _WIN32
	/* We use the registered default browser in windows */
	/* if the user is running Mac OS X, hide the browsers tab */
	if(purple_running_osx() == FALSE)
		prefs_notebook_add_page(_("Browser"), browser_page(), notebook_page++);
#endif

	prefs_notebook_add_page(_("Conversations"), conv_page(), notebook_page++);
	prefs_notebook_add_page(_("Logging"), logging_page(), notebook_page++);
	prefs_notebook_add_page(_("Network"), network_page(), notebook_page++);
	prefs_notebook_add_page(_("Proxy"), proxy_page(), notebook_page++);
	prefs_notebook_add_page(_("Password Storage"), keyring_page(), notebook_page++);

	prefs_notebook_add_page(_("Sounds"), sound_page(), notebook_page++);
	prefs_notebook_add_page(_("Status / Idle"), away_page(), notebook_page++);
	prefs_notebook_add_page(_("Themes"), theme_page(), notebook_page++);
}

void
pidgin_prefs_show(void)
{
	GtkWidget *vbox;
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
	prefs = pidgin_create_dialog(_("Preferences"), PIDGIN_HIG_BORDER, "preferences", FALSE);
	g_signal_connect(G_OBJECT(prefs), "destroy",
					 G_CALLBACK(delete_prefs), NULL);

	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(prefs), FALSE, PIDGIN_HIG_BORDER);

	/* The notebook */
	prefsnotebook = notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
	gtk_box_pack_start(GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(prefsnotebook);

	button = pidgin_dialog_add_button(GTK_DIALOG(prefs), GTK_STOCK_CLOSE, NULL, NULL);
	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), prefs);

	prefs_notebook_init();

	/* Refresh the list of themes before showing the preferences window */
	prefs_themes_refresh();

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
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", "");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/browsers/browser", "xdg-open");
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

	/* Conversation Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/conversations");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/conversations/theme", "Default");

	/* Smiley Themes */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/smileys");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/smileys/theme", "Default");

	/* Smiley Callbacks */
	purple_prefs_connect_callback(&prefs, PIDGIN_PREFS_ROOT "/smileys/theme",
								smiley_theme_pref_cb, NULL);

	pidgin_prefs_update_old();
}

void
pidgin_prefs_update_old(void)
{
	const char *str = NULL;

	/* Rename some old prefs */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_ims", "/purple/logging/log_ims");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/logging/log_chats", "/purple/logging/log_chats");
	purple_prefs_rename("/purple/conversations/placement",
					  PIDGIN_PREFS_ROOT "/conversations/placement");

	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	purple_prefs_rename_boolean_toggle(PIDGIN_PREFS_ROOT "/conversations/ignore_colors",
									 PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting");

	/*
	 * this path pref changed to a string, so migrate.  I know this will break
	 * things for and confuse users that use multiple versions with the same
	 * config directory, but I'm not inclined to want to deal with that at the
	 * moment. -- rekkanoryo
	 */
	if((str = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/browsers/command")) != NULL) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/browsers/manual_command", str);
		purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/command");
	}

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
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/debug/timestamps");
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

/**
 * @file gtkdebug.c GTK+ Debug API
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
#include "gtkgaim.h"

#include "notify.h"
#include "prefs.h"
#include "request.h"
#include "util.h"

#include "gtkdebug.h"
#include "gtkdialogs.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "gaimstock.h"

#ifdef HAVE_REGEX_H
# include <regex.h>
#endif /* HAVE_REGEX_H */

#include <gdk/gdkkeysyms.h>

typedef struct
{
	GtkWidget *window;
	GtkWidget *text;

	GtkListStore *store;

	gboolean timestamps;
	gboolean paused;

#ifdef HAVE_REGEX_H
	GtkWidget *filter;
	GtkWidget *expression;

	gboolean invert;
	gboolean highlight;

	guint timer;

	regex_t regex;
#else
	GtkWidget *find;
#endif /* HAVE_REGEX_H */
	GtkWidget *filterlevel;
} DebugWindow;

static char debug_fg_colors[][8] = {
	"#000000",    /**< All debug levels. */
	"#666666",    /**< Misc.             */
	"#000000",    /**< Information.      */
	"#660000",    /**< Warnings.         */
	"#FF0000",    /**< Errors.           */
	"#FF0000",    /**< Fatal errors.     */
};

static DebugWindow *debug_win = NULL;

#ifdef HAVE_REGEX_H
static void regex_filter_all(DebugWindow *win);
static void regex_show_all(DebugWindow *win);
#endif /* HAVE_REGEX_H */

static gint
debug_window_destroy(GtkWidget *w, GdkEvent *event, void *unused)
{
	gaim_prefs_disconnect_by_handle(pidgin_debug_get_handle());

#ifdef HAVE_REGEX_H
	if(debug_win->timer != 0) {
		const gchar *text;

		g_source_remove(debug_win->timer);

		text = gtk_entry_get_text(GTK_ENTRY(debug_win->expression));
		gaim_prefs_set_string("/gaim/gtk/debug/regex", text);
	}

	regfree(&debug_win->regex);
#endif

	/* If the "Save Log" dialog is open then close it */
	gaim_request_close_with_handle(debug_win);

	g_free(debug_win);
	debug_win = NULL;

	gaim_prefs_set_bool("/gaim/gtk/debug/enabled", FALSE);

	return FALSE;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, DebugWindow *win)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		gaim_prefs_set_int("/gaim/gtk/debug/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/debug/height", event->height);
	}

	return FALSE;
}

#ifndef HAVE_REGEX_H
struct _find {
	DebugWindow *window;
	GtkWidget *entry;
};

static void
do_find_cb(GtkWidget *widget, gint response, struct _find *f)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		gtk_imhtml_search_find(GTK_IMHTML(f->window->text),
							   gtk_entry_get_text(GTK_ENTRY(f->entry)));
		break;

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CLOSE:
		gtk_imhtml_search_clear(GTK_IMHTML(f->window->text));
		gtk_widget_destroy(f->window->find);
		f->window->find = NULL;
		g_free(f);
		break;
	}
}

static void
find_cb(GtkWidget *w, DebugWindow *win)
{
	GtkWidget *hbox, *img, *label;
	struct _find *f;

	if(win->find)
	{
		gtk_window_present(GTK_WINDOW(win->find));
		return;
	}

	f = g_malloc(sizeof(struct _find));
	f->window = win;
	win->find = gtk_dialog_new_with_buttons(_("Find"),
					GTK_WINDOW(win->window), GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					GTK_STOCK_FIND, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(win->find),
					 GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(win->find), "response",
					G_CALLBACK(do_find_cb), f);

	gtk_container_set_border_width(GTK_CONTAINER(win->find), GAIM_HIG_BOX_SPACE);
	gtk_window_set_resizable(GTK_WINDOW(win->find), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(win->find), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(win->find)->vbox), GAIM_HIG_BORDER);
	gtk_container_set_border_width(
		GTK_CONTAINER(GTK_DIALOG(win->find)->vbox), GAIM_HIG_BOX_SPACE);

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(win->find)->vbox),
					  hbox);
	img = gtk_image_new_from_stock(PIDGIN_STOCK_DIALOG_QUESTION,
								   GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(win->find),
									  GTK_RESPONSE_OK, FALSE);

	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Search for:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	f->entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(f->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(f->entry));
	g_signal_connect(G_OBJECT(f->entry), "changed",
					 G_CALLBACK(pidgin_set_sensitive_if_input),
					 win->find);
	gtk_box_pack_start(GTK_BOX(hbox), f->entry, FALSE, FALSE, 0);

	gtk_widget_show_all(win->find);
	gtk_widget_grab_focus(f->entry);
}
#endif /* HAVE_REGEX_H */

static void
save_writefile_cb(void *user_data, const char *filename)
{
	DebugWindow *win = (DebugWindow *)user_data;
	FILE *fp;
	char *tmp;

	if ((fp = g_fopen(filename, "w+")) == NULL) {
		gaim_notify_error(win, NULL, _("Unable to open file."), NULL);
		return;
	}

	tmp = gtk_imhtml_get_text(GTK_IMHTML(win->text), NULL, NULL);
	fprintf(fp, "Pidgin Debug Log : %s\n", gaim_date_format_full(NULL));
	fprintf(fp, "%s", tmp);
	g_free(tmp);

	fclose(fp);
}

static void
save_cb(GtkWidget *w, DebugWindow *win)
{
	gaim_request_file(win, _("Save Debug Log"), "gaim-debug.log", TRUE,
					  G_CALLBACK(save_writefile_cb), NULL, win);
}

static void
clear_cb(GtkWidget *w, DebugWindow *win)
{
	gtk_imhtml_clear(GTK_IMHTML(win->text));

#ifdef HAVE_REGEX_H
	gtk_list_store_clear(win->store);
#endif /* HAVE_REGEX_H */
}

static void
pause_cb(GtkWidget *w, DebugWindow *win)
{
	win->paused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));

#ifdef HAVE_REGEX_H
	if(!win->paused) {
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
			regex_filter_all(win);
		else
			regex_show_all(win);
	}
#endif /* HAVE_REGEX_H */
}

static void
timestamps_cb(GtkWidget *w, DebugWindow *win)
{
	win->timestamps = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));

	gaim_prefs_set_bool("/core/debug/timestamps", win->timestamps);
}

static void
timestamps_pref_cb(const char *name, GaimPrefType type,
				   gconstpointer value, gpointer data)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), GPOINTER_TO_INT(value));
}

/******************************************************************************
 * regex stuff
 *****************************************************************************/
#ifdef HAVE_REGEX_H
static void
regex_clear_color(GtkWidget *w) {
	gtk_widget_modify_base(w, GTK_STATE_NORMAL, NULL);
}

static void
regex_change_color(GtkWidget *w, guint16 r, guint16 g, guint16 b) {
	GdkColor color;

	color.red = r;
	color.green = g;
	color.blue = b;

	gtk_widget_modify_base(w, GTK_STATE_NORMAL, &color);
}

static void
regex_highlight_clear(DebugWindow *win) {
	GtkIMHtml *imhtml = GTK_IMHTML(win->text);
	GtkTextIter s, e;

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &s);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &e);
	gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "regex", &s, &e);
}

static void
regex_match(DebugWindow *win, const gchar *text) {
	GtkIMHtml *imhtml = GTK_IMHTML(win->text);
	regmatch_t matches[4]; /* adjust if necessary */
	size_t n_matches = sizeof(matches) / sizeof(matches[0]);
	gchar *plaintext;
	gint inverted;

	if(!text)
		return;

	inverted = (win->invert) ? REG_NOMATCH : 0;

	/* I don't like having to do this, but we need it for highlighting.  Plus
	 * it makes the ^ and $ operators work :)
	 */
	plaintext = gaim_markup_strip_html(text);

	/* we do a first pass to see if it matches at all.  If it does we append
	 * it, and work out the offsets to highlight.
	 */
	if(regexec(&win->regex, plaintext, n_matches, matches, 0) == inverted) {
		GtkTextIter ins;
		gchar *p = plaintext;
		gint i, offset = 0;

		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &ins,
							gtk_text_buffer_get_insert(imhtml->text_buffer));
		i = gtk_text_iter_get_offset(&ins);

		gtk_imhtml_append_text(imhtml, text, 0);

		/* If we're not highlighting or the expression is inverted, we're
		 * done and move on.
		 */
		if(!win->highlight || inverted == REG_NOMATCH) {
			g_free(plaintext);
			return;
		}

		/* we use a do-while to highlight the first match, and then continue
		 * if necessary...
		 */
		do {
			size_t m;

			for(m = 0; m < n_matches; m++) {
				GtkTextIter ms, me;

				if(matches[m].rm_eo == -1)
					break;

				i += offset;

				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &ms,
												   i + matches[m].rm_so);
				gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &me,
												   i + matches[m].rm_eo);
				gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "regex",
												  &ms, &me);
				offset = matches[m].rm_eo;
			}

			p += offset;
		} while(regexec(&win->regex, p, n_matches, matches, REG_NOTBOL) == inverted);
	}

	g_free(plaintext);
}

static gboolean
regex_filter_all_cb(GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *iter,
				    gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gchar *text;
	GaimDebugLevel level;

	gtk_tree_model_get(m, iter, 0, &text, 1, &level, -1);

	if (level >= gaim_prefs_get_int("/gaim/gtk/debug/filterlevel"))
		regex_match(win, text);

	g_free(text);

	return FALSE;
}

static void
regex_filter_all(DebugWindow *win) {
	gtk_imhtml_clear(GTK_IMHTML(win->text));

	if(win->highlight)
		regex_highlight_clear(win);

	gtk_tree_model_foreach(GTK_TREE_MODEL(win->store), regex_filter_all_cb,
						   win);
}

static gboolean
regex_show_all_cb(GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *iter,
				  gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gchar *text;
	GaimDebugLevel level;

	gtk_tree_model_get(m, iter, 0, &text, 1, &level, -1);
	if (level >= gaim_prefs_get_int("/gaim/gtk/debug/filterlevel"))
		gtk_imhtml_append_text(GTK_IMHTML(win->text), text, 0);
	g_free(text);

	return FALSE;
}

static void
regex_show_all(DebugWindow *win) {
	gtk_imhtml_clear(GTK_IMHTML(win->text));

	if(win->highlight)
		regex_highlight_clear(win);

	gtk_tree_model_foreach(GTK_TREE_MODEL(win->store), regex_show_all_cb,
						   win);
}

static void
regex_compile(DebugWindow *win) {
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));

	if(text == NULL || *text == '\0') {
		regex_clear_color(win->expression);
		gtk_widget_set_sensitive(win->filter, FALSE);
		return;
	}

	regfree(&win->regex);

	if(regcomp(&win->regex, text, REG_EXTENDED | REG_ICASE) != 0) {
		/* failed to compile */
		regex_change_color(win->expression, 0xFFFF, 0xAFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, FALSE);
	} else {
		/* compiled successfully */
		regex_change_color(win->expression, 0xAFFF, 0xFFFF, 0xAFFF);
		gtk_widget_set_sensitive(win->filter, TRUE);
	}

	/* we check if the filter is on in case it was only of the options that
	 * got changed, and not the expression.
	 */
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_pref_filter_cb(const gchar *name, GaimPrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val), current;

	if(!win || !win->window)
		return;

	current = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter));
	if(active != current)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->filter), active);
}

static void
regex_pref_expression_cb(const gchar *name, GaimPrefType type,
						 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	const gchar *exp = (const gchar *)val;

	gtk_entry_set_text(GTK_ENTRY(win->expression), exp);
}

static void
regex_pref_invert_cb(const gchar *name, GaimPrefType type,
					 gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->invert = active;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_pref_highlight_cb(const gchar *name, GaimPrefType type,
						gconstpointer val, gpointer data)
{
	DebugWindow *win = (DebugWindow *)data;
	gboolean active = GPOINTER_TO_INT(val);

	win->highlight = active;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
		regex_filter_all(win);
}

static void
regex_row_changed_cb(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, DebugWindow *win)
{
	gchar *text;
	GaimDebugLevel level;

	if(!win || !win->window)
		return;

	/* If the debug window is paused, we just return since it's in the store.
	 * We don't call regex_match because it doesn't make sense to check the
	 * string if it's paused.  When we unpause we clear the imhtml and
	 * reiterate over the store to handle matches that were outputted when
	 * we were paused.
	 */
	if(win->paused)
		return;

	gtk_tree_model_get(model, iter, 0, &text, 1, &level, -1);

	if (level >= gaim_prefs_get_int("/gaim/gtk/debug/filterlevel")) {
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter))) {
			regex_match(win, text);
		} else {
			gtk_imhtml_append_text(GTK_IMHTML(win->text), text, 0);
		}
	}

	g_free(text);
}

static gboolean
regex_timer_cb(DebugWindow *win) {
	const gchar *text;

	text = gtk_entry_get_text(GTK_ENTRY(win->expression));
	gaim_prefs_set_string("/gaim/gtk/debug/regex", text);

	win->timer = 0;

	return FALSE;
}

static void
regex_changed_cb(GtkWidget *w, DebugWindow *win) {
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->filter),
									 FALSE);
	}

	if(win->timer == 0)
		win->timer = gaim_timeout_add(5000, (GSourceFunc)regex_timer_cb, win);

	regex_compile(win);
}

static void
regex_key_release_cb(GtkWidget *w, GdkEventKey *e, DebugWindow *win) {
	if(e->keyval == GDK_Return &&
	   GTK_WIDGET_IS_SENSITIVE(win->filter) &&
	   !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->filter), TRUE);
	}
}

static void
regex_menu_cb(GtkWidget *item, const gchar *pref) {
	gboolean active;

	active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));

	gaim_prefs_set_bool(pref, active);
}

static void
regex_popup_cb(GtkEntry *entry, GtkWidget *menu, DebugWindow *win) {
	pidgin_separator(menu);
	pidgin_new_check_item(menu, _("Invert"),
						G_CALLBACK(regex_menu_cb),
						"/gaim/gtk/debug/invert", win->invert);
	pidgin_new_check_item(menu, _("Highlight matches"),
						G_CALLBACK(regex_menu_cb),
						"/gaim/gtk/debug/highlight", win->highlight);
}

static void
regex_filter_toggled_cb(GtkToggleButton *button, DebugWindow *win) {
	gboolean active;

	active = gtk_toggle_button_get_active(button);

	gaim_prefs_set_bool("/gaim/gtk/debug/filter", active);

	if(!GTK_IS_IMHTML(win->text))
		return;

	if(active)
		regex_filter_all(win);
	else
		regex_show_all(win);
}

static void
filter_level_pref_changed(const char *name, GaimPrefType type, gconstpointer value, gpointer data)
{
	DebugWindow *win = data;

	if (GPOINTER_TO_INT(value) != gtk_combo_box_get_active(GTK_COMBO_BOX(win->filterlevel)))
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel), GPOINTER_TO_INT(value));
	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->filter)))
		regex_filter_all(win);
	else
		regex_show_all(win);
}
#endif /* HAVE_REGEX_H */

static void
filter_level_changed_cb(GtkWidget *combo, gpointer null)
{
	gaim_prefs_set_int("/gaim/gtk/debug/filterlevel",
				gtk_combo_box_get_active(GTK_COMBO_BOX(combo)));
}

static void
toolbar_style_pref_changed_cb(const char *name, GaimPrefType type, gconstpointer value, gpointer data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(data), GPOINTER_TO_INT(value));
}

static void
toolbar_icon_pref_changed(GtkWidget *item, GtkWidget *toolbar)
{
	int style = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "user_data"));
	gaim_prefs_set_int("/gaim/gtk/debug/style", style);
}

static gboolean
toolbar_context(GtkWidget *toolbar, GdkEventButton *event, gpointer null)
{
	GtkWidget *menu, *item;
	const char *text[3];
	GtkToolbarStyle value[3];
	int i;

	if (!(event->button == 3 && event->type == GDK_BUTTON_PRESS))
		return FALSE;

	text[0] = _("_Icon Only");          value[0] = GTK_TOOLBAR_ICONS;
	text[1] = _("_Text Only");          value[1] = GTK_TOOLBAR_TEXT;
	text[2] = _("_Both Icon & Text");   value[2] = GTK_TOOLBAR_BOTH_HORIZ;

	menu = gtk_menu_new();

	for (i = 0; i < 3; i++) {
		item = gtk_check_menu_item_new_with_mnemonic(text[i]);
		g_object_set_data(G_OBJECT(item), "user_data", GINT_TO_POINTER(value[i]));
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(toolbar_icon_pref_changed), toolbar);
		if (value[i] == gaim_prefs_get_int("/gaim/gtk/debug/style"))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}

	gtk_widget_show_all(menu);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
	return FALSE;
}

static DebugWindow *
debug_window_new(void)
{
	DebugWindow *win;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *image;
	gint width, height;
	void *handle;

	win = g_new0(DebugWindow, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/debug/width");
	height = gaim_prefs_get_int("/gaim/gtk/debug/height");

	PIDGIN_DIALOG(win->window);
	gaim_debug_info("gtkdebug", "Setting dimensions to %d, %d\n",
					width, height);

	gtk_window_set_default_size(GTK_WINDOW(win->window), width, height);
	gtk_window_set_role(GTK_WINDOW(win->window), "debug");
	gtk_window_set_title(GTK_WINDOW(win->window), _("Debug Window"));

	g_signal_connect(G_OBJECT(win->window), "delete_event",
	                 G_CALLBACK(debug_window_destroy), NULL);
	g_signal_connect(G_OBJECT(win->window), "configure_event",
	                 G_CALLBACK(configure_cb), win);

	handle = pidgin_debug_get_handle();
	
#ifdef HAVE_REGEX_H
	/* the list store for all the messages */
	win->store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

	/* row-changed gets called when we do gtk_list_store_set, and row-inserted
	 * gets called with gtk_list_store_append, which is a
	 * completely empty row. So we just ignore row-inserted, and deal with row
	 * changed. -Gary
	 */
	g_signal_connect(G_OBJECT(win->store), "row-changed",
					 G_CALLBACK(regex_row_changed_cb), win);

#endif /* HAVE_REGEX_H */

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(win->window), vbox);

	if (gaim_prefs_get_bool("/gaim/gtk/debug/toolbar")) {
		/* Setup our top button bar thingie. */
		toolbar = gtk_toolbar_new();
		gtk_toolbar_set_tooltips(GTK_TOOLBAR(toolbar), TRUE);
#if GTK_CHECK_VERSION(2,4,0)
		gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), TRUE);
#endif
		g_signal_connect(G_OBJECT(toolbar), "button-press-event", G_CALLBACK(toolbar_context), win);

		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar),
		                      gaim_prefs_get_int("/gaim/gtk/debug/style"));
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/style",
	                                toolbar_style_pref_changed_cb, toolbar);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
		                          GTK_ICON_SIZE_SMALL_TOOLBAR);

		gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

#ifndef HAVE_REGEX_H
		/* Find button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_FIND,
		                         _("Find"), NULL, G_CALLBACK(find_cb),
		                         win, -1);
#endif /* HAVE_REGEX_H */

		/* Save */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_SAVE,
		                         _("Save"), NULL, G_CALLBACK(save_cb),
		                         win, -1);

		/* Clear button */
		gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_CLEAR,
		                         _("Clear"), NULL, G_CALLBACK(clear_cb),
		                         win, -1);

		gtk_toolbar_insert_space(GTK_TOOLBAR(toolbar), -1);

		/* Pause */
		image = gtk_image_new_from_stock(PIDGIN_STOCK_PAUSE, GTK_ICON_SIZE_MENU);
		gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
		                                    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
		                                    NULL, _("Pause"), _("Pause"),
		                                    NULL, image,
		                                    G_CALLBACK(pause_cb), win);

		/* Timestamps */
		button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
		                                    GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
		                                    NULL, _("Timestamps"),
		                                    _("Timestamps"), NULL, NULL,
		                                    G_CALLBACK(timestamps_cb),
		                                    win);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
		                             gaim_prefs_get_bool("/core/debug/timestamps"));

		gaim_prefs_connect_callback(handle, "/core/debug/timestamps",
		                            timestamps_pref_cb, button);

#ifdef HAVE_REGEX_H
		/* regex stuff */
		gtk_toolbar_insert_space(GTK_TOOLBAR(toolbar), -1);

		/* regex toggle button */
		win->filter =
			gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
									   GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
									   NULL, _("Filter"), _("Filter"), 
									   NULL, NULL,
									   G_CALLBACK(regex_filter_toggled_cb),
									   win);
		/* we purposely disable the toggle button here in case
		 * /gaim/gtk/debug/expression has an empty string.  If it does not have
		 * an empty string, the change signal will get called and make the
		 * toggle button sensitive.
		 */
		gtk_widget_set_sensitive(win->filter, FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->filter),
									 gaim_prefs_get_bool("/gaim/gtk/debug/filter"));
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/filter",
									regex_pref_filter_cb, win);

		/* regex entry */
		win->expression = gtk_entry_new();
		gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
								   GTK_TOOLBAR_CHILD_WIDGET, win->expression,
								   NULL, _("Right click for more options."),
								   NULL, NULL, NULL, NULL);
		/* this needs to be before the text is set from the pref if we want it
		 * to colorize a stored expression.
		 */
		g_signal_connect(G_OBJECT(win->expression), "changed",
						 G_CALLBACK(regex_changed_cb), win);
		gtk_entry_set_text(GTK_ENTRY(win->expression),
						   gaim_prefs_get_string("/gaim/gtk/debug/regex"));
		g_signal_connect(G_OBJECT(win->expression), "populate-popup",
						 G_CALLBACK(regex_popup_cb), win);
		g_signal_connect(G_OBJECT(win->expression), "key-release-event",
						 G_CALLBACK(regex_key_release_cb), win);
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/regex",
									regex_pref_expression_cb, win);

		/* connect the rest of our pref callbacks */
		win->invert = gaim_prefs_get_bool("/gaim/gtk/debug/invert");
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/invert",
									regex_pref_invert_cb, win);

		win->highlight = gaim_prefs_get_bool("/gaim/gtk/debug/highlight");
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/highlight",
									regex_pref_highlight_cb, win);

#endif /* HAVE_REGEX_H */

		gtk_toolbar_insert_space(GTK_TOOLBAR(toolbar), -1);

		gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
		                           GTK_TOOLBAR_CHILD_WIDGET, gtk_label_new(_("Level ")),
		                           NULL, _("Select the debug filter level."),
		                           NULL, NULL, NULL, NULL);
		
		win->filterlevel = gtk_combo_box_new_text();
		gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
		                           GTK_TOOLBAR_CHILD_WIDGET, win->filterlevel,
		                           NULL, _("Select the debug filter level."),
		                           NULL, NULL, NULL, NULL);
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("All"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Misc"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Info"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Warning"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Error "));
		gtk_combo_box_append_text(GTK_COMBO_BOX(win->filterlevel), _("Fatal Error"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(win->filterlevel),
					gaim_prefs_get_int("/gaim/gtk/debug/filterlevel"));
#ifdef HAVE_REGEX_H
		gaim_prefs_connect_callback(handle, "/gaim/gtk/debug/filterlevel",
						filter_level_pref_changed, win);
#endif
		g_signal_connect(G_OBJECT(win->filterlevel), "changed",
						 G_CALLBACK(filter_level_changed_cb), NULL);
	}

	/* Add the gtkimhtml */
	frame = pidgin_create_imhtml(FALSE, &win->text, NULL, NULL);
	gtk_imhtml_set_format_functions(GTK_IMHTML(win->text),
									GTK_IMHTML_ALL ^ GTK_IMHTML_SMILEY ^ GTK_IMHTML_IMAGE);
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

#ifdef HAVE_REGEX_H
	/* add the tag for regex highlighting */
	gtk_text_buffer_create_tag(GTK_IMHTML(win->text)->text_buffer, "regex",
							   "background", "#FFAFAF",
							   "weight", "bold",
							   NULL);
#endif /* HAVE_REGEX_H */

	gtk_widget_show_all(win->window);

	return win;
}

static void
debug_enabled_cb(const char *name, GaimPrefType type,
				 gconstpointer value, gpointer data)
{
	if (value)
		pidgin_debug_window_show();
	else
		pidgin_debug_window_hide();
}

static void
gaim_glib_log_handler(const gchar *domain, GLogLevelFlags flags,
					  const gchar *msg, gpointer user_data)
{
	GaimDebugLevel level;
	char *new_msg = NULL;
	char *new_domain = NULL;

	if ((flags & G_LOG_LEVEL_ERROR) == G_LOG_LEVEL_ERROR)
		level = GAIM_DEBUG_ERROR;
	else if ((flags & G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL)
		level = GAIM_DEBUG_FATAL;
	else if ((flags & G_LOG_LEVEL_WARNING) == G_LOG_LEVEL_WARNING)
		level = GAIM_DEBUG_WARNING;
	else if ((flags & G_LOG_LEVEL_MESSAGE) == G_LOG_LEVEL_MESSAGE)
		level = GAIM_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_INFO) == G_LOG_LEVEL_INFO)
		level = GAIM_DEBUG_INFO;
	else if ((flags & G_LOG_LEVEL_DEBUG) == G_LOG_LEVEL_DEBUG)
		level = GAIM_DEBUG_MISC;
	else
	{
		gaim_debug_warning("gtkdebug",
				   "Unknown glib logging level in %d\n", flags);

		level = GAIM_DEBUG_MISC; /* This will never happen. */
	}

	if (msg != NULL)
		new_msg = gaim_utf8_try_convert(msg);

	if (domain != NULL)
		new_domain = gaim_utf8_try_convert(domain);

	if (new_msg != NULL)
	{
		gaim_debug(level, (new_domain != NULL ? new_domain : "g_log"),
				   "%s\n", new_msg);

		g_free(new_msg);
	}

	g_free(new_domain);
}

#ifdef _WIN32
static void
gaim_glib_dummy_print_handler(const gchar *string)
{
}
#endif

void
pidgin_debug_init(void)
{
	/* Debug window preferences. */
	/*
	 * NOTE: This must be set before prefs are loaded, and the callbacks
	 *       set after they are loaded, since prefs sets the enabled
	 *       preference here and that loads the window, which calls the
	 *       configure event, which overrides the width and height! :P
	 */

	gaim_prefs_add_none("/gaim/gtk/debug");

	/* Controls printing to the debug window */
	gaim_prefs_add_bool("/gaim/gtk/debug/enabled", FALSE);
	gaim_prefs_add_int("/gaim/gtk/debug/filterlevel", GAIM_DEBUG_ALL);
	gaim_prefs_add_int("/gaim/gtk/debug/style", GTK_TOOLBAR_BOTH_HORIZ);

	gaim_prefs_add_bool("/gaim/gtk/debug/toolbar", TRUE);
	gaim_prefs_add_int("/gaim/gtk/debug/width",  450);
	gaim_prefs_add_int("/gaim/gtk/debug/height", 250);

#ifdef HAVE_REGEX_H
	gaim_prefs_add_string("/gaim/gtk/debug/regex", "");
	gaim_prefs_add_bool("/gaim/gtk/debug/filter", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/debug/invert", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/debug/case_insensitive", FALSE);
	gaim_prefs_add_bool("/gaim/gtk/debug/highlight", FALSE);
#endif /* HAVE_REGEX_H */

	gaim_prefs_connect_callback(NULL, "/gaim/gtk/debug/enabled",
								debug_enabled_cb, NULL);

#define REGISTER_G_LOG_HANDLER(name) \
	g_log_set_handler((name), G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL \
					  | G_LOG_FLAG_RECURSION, \
					  gaim_glib_log_handler, NULL)

	/* Register the glib/gtk log handlers. */
	REGISTER_G_LOG_HANDLER(NULL);
	REGISTER_G_LOG_HANDLER("Gdk");
	REGISTER_G_LOG_HANDLER("Gtk");
	REGISTER_G_LOG_HANDLER("GdkPixbuf");
	REGISTER_G_LOG_HANDLER("GLib");
	REGISTER_G_LOG_HANDLER("GModule");
	REGISTER_G_LOG_HANDLER("GLib-GObject");
	REGISTER_G_LOG_HANDLER("GThread");

#ifdef _WIN32
	if (!gaim_debug_is_enabled())
		g_set_print_handler(gaim_glib_dummy_print_handler);
#endif
}

void
pidgin_debug_uninit(void)
{
	gaim_debug_set_ui_ops(NULL);
}

void
pidgin_debug_window_show(void)
{
	if (debug_win == NULL)
		debug_win = debug_window_new();

	gtk_widget_show(debug_win->window);

	gaim_prefs_set_bool("/gaim/gtk/debug/enabled", TRUE);
}

void
pidgin_debug_window_hide(void)
{
	if (debug_win != NULL) {
		gtk_widget_destroy(debug_win->window);
		debug_window_destroy(NULL, NULL, NULL);
	}
}

static void
pidgin_debug_print(GaimDebugLevel level, const char *category,
					 const char *arg_s)
{
#ifdef HAVE_REGEX_H
	GtkTreeIter iter;
#endif /* HAVE_REGEX_H */
	gboolean timestamps;
	gchar *ts_s;
	gchar *esc_s, *cat_s, *tmp, *s;

	if (!gaim_prefs_get_bool("/gaim/gtk/debug/enabled") ||
	    (debug_win == NULL))
	{
		return;
	}

	timestamps = gaim_prefs_get_bool("/core/debug/timestamps");

	/*
	 * For some reason we only print the timestamp if category is
	 * not NULL.  Why the hell do we do that?  --Mark
	 */
	if ((category != NULL) && (timestamps)) {
		const char *mdate;

		time_t mtime = time(NULL);
		mdate = gaim_utf8_strftime("%H:%M:%S", localtime(&mtime));
		ts_s = g_strdup_printf("(%s) ", mdate);
	} else {
		ts_s = g_strdup("");
	}

	if (category == NULL)
		cat_s = g_strdup("");
	else
		cat_s = g_strdup_printf("<b>%s:</b> ", category);

	esc_s = g_markup_escape_text(arg_s, -1);

	s = g_strdup_printf("<font color=\"%s\">%s%s%s</font>",
						debug_fg_colors[level], ts_s, cat_s, esc_s);

	g_free(ts_s);
	g_free(cat_s);
	g_free(esc_s);

	tmp = gaim_utf8_try_convert(s);
	g_free(s);
	s = tmp;

	if (level == GAIM_DEBUG_FATAL) {
		tmp = g_strdup_printf("<b>%s</b>", s);
		g_free(s);
		s = tmp;
	}

#ifdef HAVE_REGEX_H
	/* add the text to the list store */
	gtk_list_store_append(debug_win->store, &iter);
	gtk_list_store_set(debug_win->store, &iter, 0, s, 1, level, -1);
#else /* HAVE_REGEX_H */
	if(!debug_win->paused && level >= gaim_prefs_get_int("/gaim/gtk/debug/filterlevel"))
		gtk_imhtml_append_text(GTK_IMHTML(debug_win->text), s, 0);
#endif /* !HAVE_REGEX_H */

	g_free(s);
}

static GaimDebugUiOps ops =
{
	pidgin_debug_print,
};

GaimDebugUiOps *
pidgin_debug_get_ui_ops(void)
{
	return &ops;
}

void *
pidgin_debug_get_handle() {
	static int handle;

	return &handle;
}

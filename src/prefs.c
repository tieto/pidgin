/*
 * gaim
 *
 * Copyright (C) 1998-2002, Mark Spencer <markster@marko.net>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include "gaim.h"
#include "prpl.h"
#include "proxy.h"

/* temporary preferences */
static guint misc_options_new;
static guint logging_options_new;
static guint blist_options_new;
static guint convo_options_new;
static guint im_options_new;
static guint chat_options_new;
static guint font_options_new;
static guint sound_options_new;
static char *sound_file_new[NUM_SOUNDS];
static guint away_options_new;
static guint away_resend_new;
static int auto_away_new;
static int report_idle_new;
static int proxytype_new;
static struct away_message* default_away_new;
static int web_browser_new;
static char sound_cmd_new[2048];
static char web_command_new[2048];
static int fontsize_new;
GdkColor fgcolor_new, bgcolor_new;
static struct window_size conv_size_new, buddy_chat_size_new;
char fontface_new[128];
char fontface[128];

GtkWidget *tree_v = NULL;
GtkWidget *prefs_away_list = NULL;
GtkWidget *prefs_away_menu = NULL;
GtkWidget *preftree = NULL;
GtkWidget *fontseld = NULL;

GtkListStore *prefs_away_store = NULL;

static int sound_row_sel = 0;
static char *last_sound_dir = NULL;

static GtkWidget *sounddialog = NULL;
static GtkWidget *browser_entry = NULL;
static GtkWidget *sound_entry = NULL;
static GtkWidget *away_text = NULL;
GtkCTreeNode *general_node = NULL;
GtkCTreeNode *deny_node = NULL;
GtkWidget *prefs_proxy_frame = NULL;
static GtkWidget *gaim_button(const char *, guint *, int, GtkWidget *);
GtkWidget *gaim_labeled_spin_button(GtkWidget *, const gchar *, int*, int, int, GtkSizeGroup *);
static GtkWidget *gaim_dropdown(GtkWidget *, const gchar *, int *, int, ...);
static GtkWidget *show_color_pref(GtkWidget *, gboolean);
static void delete_prefs(GtkWidget *, void *);
void set_default_away(GtkWidget *, gpointer);
static void apply_prefs();

struct debug_window *dw = NULL;
GtkWidget *prefs = NULL;
GtkWidget *debugbutton = NULL;
static int notebook_page = 0;
static GtkTreeIter plugin_iter;

/*
 * PROTOTYPES
 */
GtkTreeIter *prefs_notebook_add_page(char*, GdkPixbuf*, GtkWidget*, GtkTreeIter*, GtkTreeIter*, int);

void delete_prefs(GtkWidget *asdf, void *gdsa) {
	int v;

	prefs = NULL;
	for (v = 0; v < NUM_SOUNDS; v++) {
		if (sound_file_new[v]) {
			g_free(sound_file_new[v]);
		sound_file_new[v] = NULL;
		}
	}
	tree_v = NULL;
	sound_entry = NULL;
	browser_entry = NULL;
	debugbutton = NULL;
	prefs_away_menu = NULL;
	notebook_page = 0;
	if(sounddialog)
		gtk_widget_destroy(sounddialog);
	g_object_unref(G_OBJECT(prefs_away_store));
}

GtkWidget *preflabel;
GtkWidget *prefsnotebook;
GtkTreeStore *prefstree;

static void set_misc_options();
static void set_logging_options();
static void set_blist_options();
static void set_convo_options();
static void set_im_options();
static void set_chat_options();
static void set_font_options();
static void set_sound_options();
static void set_away_options();

#define PROXYHOST 0
#define PROXYPORT 1
#define PROXYTYPE 2
#define PROXYUSER 3
#define PROXYPASS 4
static void proxy_print_option(GtkEntry *entry, int entrynum)
{
	if (entrynum == PROXYHOST)
		g_snprintf(proxyhost, sizeof(proxyhost), "%s", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPORT)
		proxyport = atoi(gtk_entry_get_text(entry));
	else if (entrynum == PROXYUSER)
		g_snprintf(proxyuser, sizeof(proxyuser), "%s", gtk_entry_get_text(entry));
	else if (entrynum == PROXYPASS)
		g_snprintf(proxypass, sizeof(proxypass), "%s", gtk_entry_get_text(entry));
	apply_prefs();
}


GtkWidget *make_frame(GtkWidget *ret, char *text) {
	GtkWidget *vbox, *label, *hbox;
	char labeltext[256];

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ret), vbox, FALSE, FALSE, 0);
	label = gtk_label_new(NULL);
	g_snprintf(labeltext, sizeof(labeltext), "<span weight=\"bold\">%s</span>", text);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	return vbox;
}

/* OK, Apply and Cancel */

static void apply_cb(GtkWidget *button, void *data)
{
	apply_prefs();
}

static void apply_prefs()
{
	int r;
	if (misc_options != misc_options_new)
		set_misc_options();
	if (logging_options != logging_options_new)
		set_logging_options();
	if (blist_options != blist_options_new)
		set_blist_options();
	if (convo_options != convo_options_new)
		set_convo_options();
	if (im_options != im_options_new)
		set_im_options();
	if (chat_options != chat_options_new)
		set_chat_options();
		chat_options = chat_options_new;
	if (font_options != font_options_new)
		set_font_options();
	if (sound_options != sound_options_new)
		set_sound_options();
	for (r = 0; r < NUM_SOUNDS; r++) {
		if (sound_file[r])
			g_free(sound_file[r]);
		sound_file[r] = g_strdup(sound_file_new[r]);
	}
	if (away_options != away_options_new)
		set_away_options();
	away_resend = away_resend_new;
	auto_away = auto_away_new;
	report_idle = report_idle_new;
	web_browser = web_browser_new;
	proxytype = proxytype_new;
	default_away = default_away_new;
	fontsize = fontsize_new;
#ifndef _WIN32	
	g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", sound_cmd_new);
#endif
	g_snprintf(web_command, sizeof(web_command), "%s", web_command_new);
	memcpy(&conv_size, &conv_size_new, sizeof(struct window_size));
	memcpy(&conv_size, &conv_size_new, sizeof(struct window_size));
	memcpy(&fgcolor, &fgcolor_new, sizeof(GdkColor));
	memcpy(&bgcolor, &bgcolor_new, sizeof(GdkColor));


	g_snprintf(fontface, sizeof(fontface), fontface_new);
	update_convo_font();
	update_convo_color();
	save_prefs();
}


static void ok_cb(GtkWidget *button, void *data)
{
	apply_cb(button, data);
	gtk_widget_destroy(prefs);
}

static void pref_nb_select(GtkTreeSelection *sel, GtkNotebook *nb) {
	GtkTreeIter   iter;
	char text[128];
	GValue val = { 0, };
	GtkTreeModel *model = GTK_TREE_MODEL(prefstree);

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	g_snprintf(text, sizeof(text), "<span weight=\"bold\" size=\"larger\">%s</span>",
		   g_value_get_string(&val));
	gtk_label_set_markup (GTK_LABEL(preflabel), text);
	g_value_unset (&val);
	gtk_tree_model_get_value (model, &iter, 2, &val);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (prefsnotebook), g_value_get_int (&val));

}

/* These are the pages in the preferences notebook */
GtkWidget *interface_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame(ret, _("Interface Options"));
	/* This shouldn't have to wait for user to click OK or APPLY or whatnot */
	/* This really shouldn't be in preferences at all */
	debugbutton = gaim_button(_("Show _Debug Window"), &misc_options_new, OPT_MISC_DEBUG, vbox);


	gaim_button(_("Use _borderless buttons"), &misc_options_new, OPT_MISC_COOL_LOOK, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *font_page() {
	GtkWidget *ret;
	GtkWidget *button;
	GtkWidget *vbox, *hbox;
	GtkWidget *select = NULL;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame(ret, _("Style"));
	gaim_button(_("_Bold"), &font_options_new, OPT_FONT_BOLD, vbox);
	gaim_button(_("_Italics"), &font_options_new, OPT_FONT_ITALIC, vbox);
	gaim_button(_("_Underline"), &font_options_new, OPT_FONT_UNDERLINE, vbox);
	gaim_button(_("_Strikethough"), &font_options_new, OPT_FONT_STRIKE, vbox);

	vbox = make_frame(ret, _("Face"));
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	button = gaim_button(_("Use custo_m face"), &font_options_new, OPT_FONT_FACE, hbox);
	gtk_size_group_add_widget(sg, button);
	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_FONT);

	if (!(font_options_new & OPT_FONT_FACE))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_font_dialog), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	button = gaim_button(_("Use custom si_ze"), &font_options_new, OPT_FONT_SIZE, hbox);
	gtk_size_group_add_widget(sg, button);
	select = gaim_labeled_spin_button(hbox, NULL, &fontsize_new, 1, 7, NULL);
	if (!(font_options_new & OPT_FONT_SIZE))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);

	vbox = make_frame(ret, _("Color"));
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);


	button = gaim_button(_("_Text color"), &font_options_new, OPT_FONT_FGCOL, hbox);
	gtk_size_group_add_widget(sg, button);

	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_COLOR);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	pref_fg_picture = show_color_pref(hbox, TRUE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_fg_picture);

	if (!(font_options_new & OPT_FONT_FGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_fgcolor_dialog), NULL);
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	button = gaim_button(_("Bac_kground color"), &font_options_new, OPT_FONT_BGCOL, hbox);
	gtk_size_group_add_widget(sg, button);
	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_COLOR);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	pref_bg_picture = show_color_pref(hbox, FALSE);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_bg_picture);

	if (!(font_options_new & OPT_FONT_BGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_bgcolor_dialog), NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);

	gtk_widget_show_all(ret);
	return ret;
}


GtkWidget *messages_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame (ret, _("Display"));
	gaim_button(_("Show graphical _smileys"), &convo_options_new, OPT_CONVO_SHOW_SMILEY, vbox);
	gaim_button(_("Show _timestamp on messages"), &convo_options_new, OPT_CONVO_SHOW_TIME, vbox);
	gaim_button(_("Show _URLs as links"), &convo_options_new, OPT_CONVO_SEND_LINKS, vbox);
#ifdef USE_GTKSPELL
	gaim_button(_("_Highlight misspelled words"), &convo_options_new, OPT_CONVO_CHECK_SPELLING, vbox);
#endif
	vbox = make_frame (ret, _("Ignore"));
	gaim_button(_("Ignore c_olors"), &convo_options_new, OPT_CONVO_IGNORE_COLOUR, vbox);
	gaim_button(_("Ignore font _faces"), &convo_options_new, OPT_CONVO_IGNORE_FONTS, vbox);
	gaim_button(_("Ignore font si_zes"), &convo_options_new, OPT_CONVO_IGNORE_SIZES, vbox);
/*	gaim_button(_("Ignore Ti_K Automated Messages"), &away_options_new, OPT_AWAY_TIK_HACK, vbox); */

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *hotkeys_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame(ret, _("Send Message"));
	gaim_button(_("_Enter sends message"), &convo_options_new, OPT_CONVO_ENTER_SENDS, vbox);
	gaim_button(_("C_ontrol-Enter sends message"), &convo_options_new, OPT_CONVO_CTL_ENTER, vbox);

	vbox = make_frame (ret, _("Window Closing"));
	gaim_button(_("E_scape closes window"), &convo_options_new, OPT_CONVO_ESC_CAN_CLOSE, vbox);
	gaim_button(_("Control-_W closes window"), &convo_options_new, OPT_CONVO_CTL_W_CLOSES, vbox);

	vbox = make_frame(ret, "Insertions");
	gaim_button(_("Control-{B/I/U/S} inserts _HTML tags"), &convo_options_new, OPT_CONVO_CTL_CHARS, vbox);
	gaim_button(_("Control-(number) inserts _smileys"), &convo_options_new, OPT_CONVO_CTL_SMILEYS, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *list_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame (ret, _("Buttons"));
	gaim_button(_("_Hide IM/Info/Chat buttons"), &blist_options_new, OPT_BLIST_NO_BUTTONS, vbox);
	gaim_button(_("Show _pictures on buttons"), &blist_options_new, OPT_BLIST_SHOW_BUTTON_XPM, vbox);

	vbox = make_frame (ret, _("Buddy List Window"));
	gaim_button(_("_Save Window Size/Position"), &blist_options_new, OPT_BLIST_SAVED_WINDOWS, vbox);
	gaim_button(_("_Raise Window on Events"), &blist_options_new, OPT_BLIST_POPUP, vbox);

	vbox = make_frame (ret, _("Group Display"));
	gaim_button(_("Hide _groups with no online buddies"), &blist_options_new, OPT_BLIST_NO_MT_GRP, vbox);
	gaim_button(_("Show _numbers in groups"), &blist_options_new, OPT_BLIST_SHOW_GRPNUM, vbox);

	vbox = make_frame (ret, _("Buddy Display"));
	gaim_button(_("Show buddy type _icons"), &blist_options_new, OPT_BLIST_SHOW_PIXMAPS, vbox);
	gaim_button(_("Show _warning levels"), &blist_options_new, OPT_BLIST_SHOW_WARN, vbox);
	gaim_button(_("Show idle _times"), &blist_options_new, OPT_BLIST_SHOW_IDLETIME, vbox);
	gaim_button(_("Grey i_dle buddies"), &blist_options_new, OPT_BLIST_GREY_IDLERS, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *im_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *typingbutton, *widge;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = make_frame (ret, _("Window"));
	widge = gaim_dropdown(vbox, "Show _buttons as:", &im_options_new, OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM,
		      "Pictures", OPT_IM_BUTTON_XPM,
		      "Text", OPT_IM_BUTTON_TEXT,
		      "Pictures and text", OPT_IM_BUTTON_XPM | OPT_IM_BUTTON_TEXT, NULL);
	gtk_size_group_add_widget(sg, widge);
	gtk_misc_set_alignment(GTK_MISC(widge), 0, 0);
	gaim_labeled_spin_button(vbox, _("New window _width:"), &conv_size_new.width, 25, 9999, sg);
	gaim_labeled_spin_button(vbox, _("New window _height:"), &conv_size_new.height, 25, 9999, sg);
	gaim_labeled_spin_button(vbox, _("_Entry widget height:"), &conv_size_new.entry_height, 25, 9999, sg);
	gaim_button(_("_Raise windows on events"), &im_options_new, OPT_IM_POPUP, vbox);
	gaim_button(_("Hide window on _send"), &im_options_new, OPT_IM_POPDOWN, vbox);
	gtk_widget_show (vbox);

	vbox = make_frame (ret, _("Buddy Icons"));
	gaim_button(_("Hide Buddy _Icons"), &im_options_new, OPT_IM_HIDE_ICONS, vbox);
	gaim_button(_("Disable Buddy Icon A_nimation"), &im_options_new, OPT_IM_NO_ANIMATION, vbox);

	vbox = make_frame (ret, _("Display"));
	gaim_button(_("Show _logins in window"), &im_options_new, OPT_IM_LOGON, vbox);

	vbox = make_frame (ret, _("Typing Notification"));
	typingbutton = gaim_button(_("Notify buddies that you are _typing to them"), &misc_options_new,
				   OPT_MISC_STEALTH_TYPING, vbox);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(typingbutton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(typingbutton)));
	misc_options_new ^= OPT_MISC_STEALTH_TYPING;

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *chat_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *dd;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	vbox = make_frame (ret, _("Window"));
	dd = gaim_dropdown(vbox, "Show _buttons as:", &chat_options_new, OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM,
			   "Pictures", OPT_CHAT_BUTTON_XPM,
			   "Text", OPT_CHAT_BUTTON_TEXT,
			   "Pictures and Text", OPT_CHAT_BUTTON_XPM | OPT_CHAT_BUTTON_TEXT, NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0);
	gaim_labeled_spin_button(vbox, _("New window _width:"), &buddy_chat_size_new.width, 25, 9999, sg);
	gaim_labeled_spin_button(vbox, _("New window _height:"), &buddy_chat_size_new.height, 25, 9999, sg);
	gaim_labeled_spin_button(vbox, _("_Entry widget height:"), &buddy_chat_size_new.entry_height, 25, 9999, sg);
	gaim_button(_("_Raise windows on events"), &chat_options_new, OPT_CHAT_POPUP, vbox);

	vbox = make_frame (ret, _("Tab Completion"));
	gaim_button(_("_Tab-Complete Nicks"), &chat_options_new, OPT_CHAT_TAB_COMPLETE, vbox);
	gaim_button(_("_Old-Style Tab Completion"), &chat_options_new, OPT_CHAT_OLD_STYLE_TAB, vbox);

	vbox = make_frame (ret, _("Display"));
	gaim_button(_("_Show people joining/leaving in window"), &chat_options_new, OPT_CHAT_LOGON, vbox);
	gaim_button(_("Co_lorize screennames"), &chat_options_new, OPT_CHAT_COLORIZE, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *tab_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *dd;
	GtkSizeGroup *sg;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = make_frame (ret, _("IM Tabs"));
	dd = gaim_dropdown(vbox, "Tab _Placement:", &im_options_new, OPT_IM_SIDE_TAB | OPT_IM_BR_TAB,
		      "Top", 0,
		      "Bottom", OPT_IM_BR_TAB,
		      "Left", OPT_IM_SIDE_TAB,
		      "Right", OPT_IM_BR_TAB | OPT_IM_SIDE_TAB, NULL);
	gtk_size_group_add_widget(sg, dd);
	gaim_button(_("Show all _Instant Messages in one tabbed\nwindow"), &im_options_new, OPT_IM_ONE_WINDOW, vbox);
	gaim_button(_("Show a_liases in tabs/titles"), &im_options_new, OPT_IM_ALIAS_TAB, vbox);

	vbox = make_frame (ret, _("Chat Tabs"));
	dd = gaim_dropdown(vbox, "Tab _Placement:", &chat_options_new, OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB,
			   "Top", 0,
			   "Bottom", OPT_CHAT_BR_TAB,
			   "Left", OPT_CHAT_SIDE_TAB,
			   "Right", OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB, NULL);
	gtk_size_group_add_widget(sg, dd);
	gaim_button(_("Show all c_hats in one tabbed window"), &chat_options_new, OPT_CHAT_ONE_WINDOW,
		    vbox);

	vbox = make_frame (ret, _("Combined Tabs"));
	gaim_button(_("Show IMs and chats in _same tabbed\nwindow."), &convo_options_new, OPT_CONVO_COMBINE, vbox);

	vbox = make_frame (ret, _("Buddy List Tabs"));
	dd = gaim_dropdown(vbox, "Tab _Placement:", &blist_options_new, OPT_BLIST_BOTTOM_TAB,
		      "Top", 0,
		      "Bottom", OPT_BLIST_BOTTOM_TAB, NULL);
	gtk_size_group_add_widget(sg, dd);

	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *proxy_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *table;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame (ret, _("Proxy Type"));
	gaim_dropdown(vbox, "Proxy _Type:", &proxytype_new, -1,
		      "No Proxy", PROXY_NONE,
		      "SOCKS 4", PROXY_SOCKS4,
		      "SOCKS 5", PROXY_SOCKS5,
		      "HTTP", PROXY_HTTP, NULL);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);

	vbox = make_frame(ret, _("Proxy Server"));
	prefs_proxy_frame = vbox;

	if (proxytype == PROXY_NONE)
		gtk_widget_set_sensitive(GTK_WIDGET(vbox), FALSE);

	table = gtk_table_new(2, 4, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_container_add(GTK_CONTAINER(vbox), table);


	label = gtk_label_new_with_mnemonic(_("_Host"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYHOST);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyhost);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("Port"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPORT);

	if (proxyport) {
		char buf[128];
		g_snprintf(buf, sizeof(buf), "%d", proxyport);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
	}

	label = gtk_label_new_with_mnemonic(_("_User"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYUSER);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyuser);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("Pa_ssword"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 3, 4, GTK_FILL , 0, 0, 0);
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPASS);
	gtk_entry_set_text(GTK_ENTRY(entry), proxypass);

	gtk_widget_show_all(ret);
	return ret;
}

static void browser_print_option(GtkEntry *entry, void *nullish) {
	g_snprintf(web_command_new, sizeof(web_command_new), "%s", gtk_entry_get_text(entry));
}

GtkWidget *browser_page() {
	GtkWidget *ret;
	GtkWidget *vbox, *hbox;
	GtkWidget *label;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
#ifndef _WIN32
	/* Registered default browser is used by Windows */
	vbox = make_frame (ret, _("Browser Selection"));
	label = gaim_dropdown(vbox, "_Browser", &web_browser_new, -1,
			      "Netscape", BROWSER_NETSCAPE,
			      "Konqueror", BROWSER_KONQ,
			      "Mozilla", BROWSER_MOZILLA,
			      "Manual", BROWSER_MANUAL,
/* fixme: GNOME binary helper
			      "GNOME URL Handler", BROWSER_GNOME, */
			      "Galeon", BROWSER_GALEON,
			      "Opera", BROWSER_OPERA, NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_size_group_add_widget(sg, label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic("_Manual: ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_size_group_add_widget(sg, label);
	browser_entry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), browser_entry);
	if (web_browser_new != BROWSER_MANUAL)
		gtk_widget_set_sensitive(browser_entry, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), browser_entry, FALSE, FALSE, 0);

	gtk_entry_set_text(GTK_ENTRY(browser_entry), web_command_new);
	gtk_signal_connect(GTK_OBJECT(browser_entry), "changed",
			   GTK_SIGNAL_FUNC(browser_print_option), NULL);
#endif /* end !_WIN32 */
	vbox = make_frame (ret, _("Browser Options"));
	label = gaim_button(_("Open new _window by default"), &misc_options_new, OPT_MISC_BROWSER_POPUP, vbox);
#ifdef _WIN32
	/* Until I figure out how to implement this on windows */
	gtk_widget_set_sensitive(label, FALSE);
#endif
	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *logging_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	vbox = make_frame (ret, _("Message Logs"));
	gaim_button(_("_Log all Instant Messages"), &logging_options_new, OPT_LOG_CONVOS, vbox);
	gaim_button(_("Log all c_hats"), &logging_options_new, OPT_LOG_CHATS, vbox);
	gaim_button(_("Strip _HTML from logs"), &logging_options_new, OPT_LOG_STRIP_HTML, vbox);

	vbox = make_frame (ret, _("System Logs"));
	gaim_button(_("Log when buddies _sign on/sign off"), &logging_options_new, OPT_LOG_BUDDY_SIGNON,
		    vbox);
	gaim_button(_("Log when buddies become _idle/un-idle"), &logging_options_new, OPT_LOG_BUDDY_IDLE,
		    vbox);
	gaim_button(_("Log when buddies go away/come _back"), &logging_options_new, OPT_LOG_BUDDY_AWAY, vbox);
	gaim_button(_("Log your _own signons/idleness/awayness"), &logging_options_new, OPT_LOG_MY_SIGNON,
		    vbox);
	gaim_button(_("I_ndividual log file for each buddy's signons"), &logging_options_new,
		    OPT_LOG_INDIVIDUAL, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

static GtkWidget *sndcmd = NULL;

static gint sound_cmd_yeah(GtkEntry *entry, GdkEvent *event, gpointer d)
{
	g_snprintf(sound_cmd_new, sizeof(sound_cmd_new), "%s", gtk_entry_get_text(GTK_ENTRY(sndcmd)));
	return TRUE;
}

GtkWidget *sound_page() {
	GtkWidget *ret;
	GtkWidget *label;
	GtkWidget *vbox, *hbox;
	GtkSizeGroup *sg;
	GtkWidget *dd;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = make_frame (ret, _("Sound Options"));
	gaim_button(_("_No sounds when you log in"), &sound_options_new, OPT_SOUND_SILENT_SIGNON, vbox);
	gaim_button(_("_Sounds while away"), &sound_options_new, OPT_SOUND_WHEN_AWAY, vbox);

#ifndef _WIN32
	vbox = make_frame (ret, _("Sound Method"));
	dd = gaim_dropdown(vbox, "_Method", &sound_options_new, OPT_SOUND_BEEP |
		      OPT_SOUND_ESD | OPT_SOUND_ARTSC | OPT_SOUND_NAS | OPT_SOUND_NORMAL |
		      OPT_SOUND_CMD,
		      "Console Beep", OPT_SOUND_BEEP,
#ifdef ESD_SOUND
		      "ESD", OPT_SOUND_ESD,
#endif /* ESD_SOUND */
#ifdef ARTSC_SOUND
		      "ArtsC", OPT_SOUND_ARTSC,
#endif /* ARTSC_SOUND */
#ifdef NAS_SOUND
		      "NAS", OPT_SOUND_NAS,
#endif /* NAS_SOUND */
		      "Internal", OPT_SOUND_NORMAL,
		      "Command", OPT_SOUND_CMD, NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	label = gtk_label_new_with_mnemonic(_("Sound c_ommand\n(%s for filename)"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	sndcmd = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), sndcmd);

	gtk_entry_set_editable(GTK_ENTRY(sndcmd), TRUE);
	gtk_entry_set_text(GTK_ENTRY(sndcmd), sound_cmd);
	gtk_widget_set_size_request(sndcmd, 75, -1);

	gtk_widget_set_sensitive(sndcmd, (sound_options_new & OPT_SOUND_CMD));
	gtk_box_pack_start(GTK_BOX(hbox), sndcmd, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(sndcmd), "focus_out_event", GTK_SIGNAL_FUNC(sound_cmd_yeah), NULL);
#endif /* _WIN32 */
	gtk_widget_show_all(ret);
	return ret;
}

GtkWidget *away_page() {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *select;
	GtkWidget *dd;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	vbox = make_frame (ret, _("Away"));
	gaim_button(_("_Sending messages removes away status"), &away_options_new, OPT_AWAY_BACK_ON_IM, vbox);
	gaim_button(_("_Queue new messages when away"), &away_options_new, OPT_AWAY_QUEUE, vbox);
	gaim_button(_("_Ignore new conversations when away"), &away_options_new, OPT_AWAY_DISCARD, vbox);

	vbox = make_frame (ret, _("Auto-response"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gaim_labeled_spin_button(hbox, _("Seconds before _resending:"),
				 &away_resend_new, 1, 24 * 60 * 60, sg);
	gaim_button(_("_Don't send auto-response"), &away_options_new, OPT_AWAY_NO_AUTO_RESP, vbox);
	gaim_button(_("_Only send auto-response when idle"), &away_options_new, OPT_AWAY_IDLE_RESP, vbox);
	gaim_button(_("Do_n't send auto-response in active conversations"), &away_options_new, OPT_AWAY_DELAY_IN_USE, vbox);

	if (away_options_new & OPT_AWAY_NO_AUTO_RESP)
		gtk_widget_set_sensitive(hbox, FALSE);

	vbox = make_frame (ret, _("Idle"));
	dd = gaim_dropdown(vbox, "Idle _Time Reporting:", &report_idle_new, -1,
			   "None", IDLE_NONE,
			   "Gaim Usage", IDLE_GAIM,
#ifdef USE_SCREENSAVER
			   "X Usage", IDLE_SCREENSAVER,
#endif
			   NULL);
	gtk_size_group_add_widget(sg, dd);
	gtk_misc_set_alignment(GTK_MISC(dd), 0, 0);

	vbox = make_frame (ret, _("Auto-away"));
	button = gaim_button(_("Set away _when idle"), &away_options_new, OPT_AWAY_AUTO, vbox);
	select = gaim_labeled_spin_button(vbox, "_Minutes before setting away:", &auto_away_new, 1, 24 * 60, sg);
	if (!(away_options_new & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);

	label = gtk_label_new_with_mnemonic("Away m_essage:");
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	prefs_away_menu = gtk_option_menu_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), prefs_away_menu);
	if (!(away_options_new & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(prefs_away_menu), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), prefs_away_menu);
	default_away_menu_init(prefs_away_menu);
	gtk_widget_show(prefs_away_menu);
	gtk_box_pack_start(GTK_BOX(hbox), prefs_away_menu, FALSE, FALSE, 0);

	gtk_widget_show_all(ret);
	return ret;
}

#if USE_PLUGINS
GtkWidget *plugin_description=NULL, *plugin_details=NULL;
static void prefs_plugin_sel (GtkTreeSelection *sel, GtkTreeModel *model) 
{
	gchar buf[1024];
	GtkTreeIter  iter;
	GValue val = { 0, };
	struct gaim_plugin *plug;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 2, &val);
	plug = g_value_get_pointer(&val);
	
	if (plug->error[0]) 
		g_snprintf(buf, sizeof(buf), _("<span size=\"larger\">%s %s</span>\n\n"
					       "<span weight=\"bold\" color=\"red\">%s</span>\n\n"
					       "%s"), plug->desc.name, plug->desc.version, plug->error, plug->desc.description); 
	else
		g_snprintf(buf, sizeof(buf), _("<span size=\"larger\">%s %s</span>\n\n"
					       "%s"), plug->desc.name, plug->desc.version, plug->desc.description); 
	gtk_label_set_markup(GTK_LABEL(plugin_description), buf);
	g_snprintf(buf, sizeof(buf), 
#ifndef _WIN32
		   _("<span size=\"larger\">%s %s</span>\n\n"
		     "<span weight=\"bold\">Written by:</span>\t%s\n"
		     "<span weight=\"bold\">URL:</span>\t%s\n"
		     "<span weight=\"bold\">File name:</span>\t%s"),
#else
		   _("<span size=\"larger\">%s %s</span>\n\n"
		     "<span weight=\"bold\">Written by:</span>  %s\n"
		     "<span weight=\"bold\">URL:</span>  %s\n"
		     "<span weight=\"bold\">File name:</span>  %s"),
#endif
		   plug->desc.name, plug->desc.version, plug->desc.authors, plug->desc.url, plug->path);
	gtk_label_set_markup(GTK_LABEL(plugin_details), buf);
	g_value_unset (&val);
}

static void plugin_load (GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	struct gaim_plugin *plug;
	gchar buf[1024];
	GtkWidget *(*config)();
	
	GdkCursor *wait = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor(prefs->window, wait);
	gdk_cursor_unref(wait);
  
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 2, &plug, -1);
	
	if (!plug->handle)

		if (plug->type == plugin)
#ifdef GAIM_PLUGINS
			{
				load_plugin(plug->path);
				if (g_module_symbol(plug->handle, "gaim_plugin_config_gtk", (gpointer *)&config)) {
					plug->iter = g_new0(GtkTreeIter, 1);
					prefs_notebook_add_page(plug->desc.name, NULL, config(), plug->iter, &plugin_iter, notebook_page++);
					if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(prefstree), &plugin_iter) == 1) {
						/* Expand the tree for the first plugin added */
						GtkTreePath *path2  = gtk_tree_model_get_path(GTK_TREE_MODEL(prefstree), &plugin_iter);
						gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_v), path2, TRUE);
						gtk_tree_path_free (path2);
					}
				}
			}
#else
	        {}	
#endif
		else
#ifdef USE_PERL
			perl_load_file(plug->path);
#else
	        {}
#endif 
	else
		if (plug->type == plugin)
#ifdef GAIM_PLUGINS
			{
				unload_plugin(plug);
				if (plug->iter) {
					gtk_tree_store_remove(GTK_TREE_STORE(prefstree), plug->iter);
					g_free(plug->iter);
					plug->iter = NULL;
				}
			}
#else
	                {} 
#endif
		else
#ifdef USE_PERL
			perl_unload_file(plug);
#else
	                {}
#endif
	gdk_window_set_cursor(prefs->window, NULL);
	if (plug->error[0]) 
		g_snprintf(buf, sizeof(buf), _("<span size=\"larger\">%s %s</span>\n\n"
					       "<span weight=\"bold\" color=\"red\">%s</span>\n\n"
					       "%s"), plug->desc.name, plug->desc.version, plug->error, plug->desc.description); 
	else
		g_snprintf(buf, sizeof(buf), _("<span size=\"larger\">%s %s</span>\n\n"
					       "%s"), plug->desc.name, plug->desc.version, plug->desc.description); 
	gtk_label_set_markup(GTK_LABEL(plugin_description), buf);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, plug->handle, -1);
	
	gtk_label_set_markup(GTK_LABEL(plugin_description), buf);
	gtk_tree_path_free(path);
}

static GtkWidget *plugin_page ()
{
	GtkWidget *ret;

	GtkWidget *sw, *vp;
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkListStore *ls;
	GtkCellRenderer *rend, *rendt;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	
	GtkWidget *nb;

	GList *probes = probed_plugins;
	struct gaim_plugin *plug;
	
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);

	ls = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
	while (probes) {
		plug = probes->data;
		gtk_list_store_append (ls, &iter);
		gtk_list_store_set(ls, &iter,
				   0, plug->handle,
				   1, plug->desc.name ? plug->desc.name : plug->path, 
				   2, plug, -1);
		probes = probes->next;
	}
	
	event_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(ls));

	rend = gtk_cell_renderer_toggle_new();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));

	
	col = gtk_tree_view_column_new_with_attributes ("Load",
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);

	rendt = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Name",
							rendt,
							"text", 1,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	g_object_unref(G_OBJECT(ls));
	gtk_container_add(GTK_CONTAINER(sw), event_view);
	

	nb = gtk_notebook_new();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK(nb), GTK_POS_BOTTOM);
	gtk_notebook_popup_disable(GTK_NOTEBOOK(nb));
	
	/* Description */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	plugin_description = gtk_label_new(NULL);
	
	vp = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_NONE);

	gtk_container_add(GTK_CONTAINER(vp), plugin_description);
	gtk_container_add(GTK_CONTAINER(sw), vp);

	gtk_label_set_selectable(GTK_LABEL(plugin_description), TRUE);  
	gtk_label_set_line_wrap(GTK_LABEL(plugin_description), TRUE);
	gtk_misc_set_alignment(GTK_MISC(plugin_description), 0, 0);
	gtk_misc_set_padding(GTK_MISC(plugin_description), 6, 6);
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), sw, gtk_label_new(_("Description")));

	/* Details */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	plugin_details = gtk_label_new(NULL);
	
	vp = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_NONE);

	gtk_container_add(GTK_CONTAINER(vp), plugin_details);
	gtk_container_add(GTK_CONTAINER(sw), vp);

	gtk_label_set_selectable(GTK_LABEL(plugin_details), TRUE);  
	gtk_label_set_line_wrap(GTK_LABEL(plugin_details), TRUE);
	gtk_misc_set_alignment(GTK_MISC(plugin_details), 0, 0);
	gtk_misc_set_padding(GTK_MISC(plugin_details), 6, 6);	
	gtk_notebook_append_page(GTK_NOTEBOOK(nb), sw, gtk_label_new(_("Details")));
	gtk_box_pack_start(GTK_BOX(ret), nb, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (prefs_plugin_sel),
			  NULL); 
	g_signal_connect (G_OBJECT(rend), "toggled",
			  G_CALLBACK(plugin_load), ls);

	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);

	gtk_widget_show_all(ret);
	return ret;
}
#endif

static void event_toggled (GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	gint soundnum;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 2, &soundnum, -1);

	sound_options_new ^= sounds[soundnum].opt;
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, sound_options_new & sounds[soundnum].opt, -1);

	gtk_tree_path_free(path);
}

static void test_sound(GtkWidget *button, gpointer i_am_NULL)
{
	guint32 tmp_sound = sound_options;
	char *save_me=NULL;
	if (!(sound_options & OPT_SOUND_WHEN_AWAY))
		sound_options ^= OPT_SOUND_WHEN_AWAY;
	if (!(sound_options & sounds[sound_row_sel].opt))
		sound_options ^= sounds[sound_row_sel].opt;
	save_me = sound_file[sound_row_sel];
	sound_file[sound_row_sel] = sound_file_new[sound_row_sel];
	play_sound(sound_row_sel);

	sound_file[sound_row_sel] = save_me;
	sound_options = tmp_sound;
}

static void reset_sound(GtkWidget *button, gpointer i_am_also_NULL)
{
	/* This just resets a sound file back to default */
	if (sound_file_new[sound_row_sel]) {
		g_free(sound_file_new[sound_row_sel]);
		sound_file_new[sound_row_sel] = NULL;
	}

	gtk_entry_set_text(GTK_ENTRY(sound_entry), "(default)");
}

void close_sounddialog(GtkWidget *w, GtkWidget *w2)
{

	GtkWidget *dest;

	if (!GTK_IS_WIDGET(w2))
		dest = w;
	else
		dest = w2;

	sounddialog = NULL;

	gtk_widget_destroy(dest);
}

void do_select_sound(GtkWidget *w, int snd)
{
	const char *file;

	file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(sounddialog));

	/* If they type in a directory, change there */
	if (file_is_dir(file, sounddialog))
		return;

	/* Let's just be safe */
	if (sound_file_new[snd])
		g_free(sound_file_new[snd]);

	/* Set it -- and forget it */
	sound_file_new[snd] = g_strdup(file);

	/* Set our text entry */
	gtk_entry_set_text(GTK_ENTRY(sound_entry), sound_file_new[snd]);

	/* Close the window! It's getting cold in here! */
	close_sounddialog(NULL, sounddialog);

	if (last_sound_dir)
		g_free(last_sound_dir);
	last_sound_dir = g_dirname(sound_file_new[snd]);
}

static void sel_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	char *buf = g_malloc(BUF_LEN);

	if (!sounddialog) {
		sounddialog = gtk_file_selection_new(_("Gaim - Sound Configuration"));

		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(sounddialog));

		g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S, last_sound_dir ? last_sound_dir : gaim_home_dir());

		gtk_file_selection_set_filename(GTK_FILE_SELECTION(sounddialog), buf);

		gtk_signal_connect(GTK_OBJECT(sounddialog), "destroy",
				   GTK_SIGNAL_FUNC(close_sounddialog), sounddialog);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(sounddialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(do_select_sound), (int *)sound_row_sel);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(sounddialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(close_sounddialog), sounddialog);
	}

	g_free(buf);
	gtk_widget_show(sounddialog);
	gdk_window_raise(sounddialog->window);
}


static void prefs_sound_sel (GtkTreeSelection *sel, GtkTreeModel *model) {
	GtkTreeIter  iter;
	GValue val = { 0, };

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 2, &val);
	sound_row_sel = g_value_get_uint(&val);
	if (sound_entry)
		gtk_entry_set_text(GTK_ENTRY(sound_entry), sound_file_new[sound_row_sel] ? sound_file_new[sound_row_sel] : "(default)");
	g_value_unset (&val);
	if (sounddialog)
		gtk_widget_destroy(sounddialog);
}

GtkWidget *sound_events_page() {

	GtkWidget *ret;
	GtkWidget *sw;
	GtkWidget *button, *hbox;
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkListStore *event_store;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
	int j;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);
	event_store = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_UINT);

	for (j=0; j < NUM_SOUNDS; j++) {
		if (sounds[j].opt == 0)
			continue;

		gtk_list_store_append (event_store, &iter);
		gtk_list_store_set(event_store, &iter,
				   0, sound_options & sounds[j].opt,
				   1, sounds[j].label,
				   2, j, -1);
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

	col = gtk_tree_view_column_new_with_attributes ("Play",
							rend,
							"active", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Event",
							rend,
							"text", 1,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	g_object_unref(G_OBJECT(event_store));
	gtk_container_add(GTK_CONTAINER(sw), event_view);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ret), hbox, FALSE, FALSE, 0);
	sound_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(sound_entry), sound_file_new[0] ? sound_file_new[0] : "(default)");
	gtk_entry_set_editable(GTK_ENTRY(sound_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sound_entry, FALSE, FALSE, 5);

	button = gtk_button_new_with_label(_("Test"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(test_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(reset_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	button = gtk_button_new_with_label(_("Choose..."));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(sel_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

	gtk_widget_show_all (ret);

	return ret;
}

void away_message_sel(GtkTreeSelection *sel, GtkTreeModel *model)
{
	GtkTreeIter  iter;
	GValue val = { 0, };
	gchar buffer[BUF_LONG];
	char *tmp;
	struct away_message *am;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	am = g_value_get_pointer(&val);
	gtk_imhtml_clear(GTK_IMHTML(away_text));
	strncpy(buffer, am->message, BUF_LONG);
	tmp = stylize(buffer, BUF_LONG);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), tmp, -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), "<BR>", -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	g_free(tmp);
	g_value_unset (&val);

}

void remove_away_message(GtkWidget *widget, GtkTreeView *tv) {
        struct away_message *am;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeStore *ts = GTK_TREE_STORE(gtk_tree_view_get_model(tv));
	GtkTreeSelection *sel = gtk_tree_view_get_selection(tv);
	GtkTreeModel *model = GTK_TREE_MODEL(prefs_away_store);
	GValue val = { 0, };

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (GTK_TREE_MODEL(prefs_away_store), &iter, 1, &val);
	am = g_value_get_pointer (&val);
	gtk_imhtml_clear(GTK_IMHTML(away_text));
	rem_away_mess(NULL, am);
	gtk_list_store_remove(GTK_LIST_STORE(ts), &iter);
	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
	gtk_tree_path_free(path);
}

GtkWidget *away_message_page() {
	GtkWidget *ret;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *sw;
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GSList *awy = away_messages;
	struct away_message *a;
	GtkWidget *sw2;
	GtkSizeGroup *sg;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	sw = gtk_scrolled_window_new(NULL,NULL);
	away_text = gtk_imhtml_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
//	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
//					GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);

	prefs_away_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	while (awy) {
		a = (struct away_message *)awy->data;
		gtk_list_store_append (prefs_away_store, &iter);
		gtk_list_store_set(prefs_away_store, &iter,
				   0, a->name,
				   1, a, -1);
		awy = awy->next;
	}
	event_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(prefs_away_store));


	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("NULL",
							rend,
							"text", 0,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(event_view), col);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(event_view), FALSE);
	gtk_widget_show(event_view);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), event_view);

	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(ret), sw2, TRUE, TRUE, 0);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw2), away_text);
	GTK_LAYOUT(away_text)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(away_text)->vadjustment->step_increment = 10.0;
	gaim_setup_imhtml(away_text);
	gtk_imhtml_set_defaults(GTK_IMHTML(away_text), NULL, NULL, NULL);
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));
	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (away_message_sel),
			  NULL);
	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(ret), hbox, FALSE, FALSE, 0);
	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), NULL);

	button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(remove_away_message), event_view);

	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = gaim_pixbuf_button(_("_Edit"), "edit.png", GAIM_BUTTON_HORIZONTAL);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), event_view);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(ret);
	return ret;
}

GtkTreeIter *prefs_notebook_add_page(char *text,
				     GdkPixbuf *pixbuf,
				     GtkWidget *page,
				     GtkTreeIter *iter,
				     GtkTreeIter *parent,
				     int ind) {
	GdkPixbuf *icon = NULL;

	if (pixbuf)
		icon = gdk_pixbuf_scale_simple (pixbuf, 18, 18, GDK_INTERP_BILINEAR);

	gtk_tree_store_append (prefstree, iter, parent);
	gtk_tree_store_set (prefstree, iter, 0, icon, 1, text, 2, ind, -1);

	if (pixbuf)
		g_object_unref(pixbuf);
	if (icon)
		g_object_unref(icon);
	gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
	return iter;
}

void prefs_notebook_init() {
	GtkTreeIter p, c;
#if USE_PLUGINS
	GtkWidget *(*config)();
	GList *l = plugins;
	struct gaim_plugin *plug;
#endif
	prefs_notebook_add_page(_("Interface"), NULL, interface_page(), &p, NULL, notebook_page++);
	prefs_notebook_add_page(_("Fonts"), NULL, font_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Message Text"), NULL, messages_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Shortcuts"), NULL, hotkeys_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Buddy List"), NULL, list_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("IM Window"), NULL, im_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Chat Window"), NULL, chat_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Tabs"), NULL, tab_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Proxy"), NULL, proxy_page(), &p, NULL, notebook_page++);
	prefs_notebook_add_page(_("Browser"), NULL, browser_page(), &p, NULL, notebook_page++);

	prefs_notebook_add_page(_("Logging"), NULL, logging_page(), &p, NULL, notebook_page++);
	prefs_notebook_add_page(_("Sounds"), NULL, sound_page(), &p, NULL, notebook_page++);
	prefs_notebook_add_page(_("Sound Events"), NULL, sound_events_page(), &c, &p, notebook_page++);
	prefs_notebook_add_page(_("Away / Idle"), NULL, away_page(), &p, NULL, notebook_page++);
	prefs_notebook_add_page(_("Away Messages"), NULL, away_message_page(), &c, &p, notebook_page++);
#if USE_PLUGINS
	prefs_notebook_add_page(_("Plugins"), NULL, plugin_page(), &plugin_iter, NULL, notebook_page++);
	while (l) {
		plug = l->data;
		if (plug->type == plugin && g_module_symbol(plug->handle, "gaim_plugin_config_gtk", (gpointer *)&config)) {
			plug->iter = g_new0(GtkTreeIter, 1);
			prefs_notebook_add_page(plug->desc.name, NULL, config(), plug->iter, &plugin_iter, notebook_page++);
		}
		l = l->next;
	}
#endif
}

void show_prefs()
{
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *sel;
	GtkWidget *notebook;
	GtkWidget *sep;
	GtkWidget *button;
	GtkSizeGroup *sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);

	int r;

	if (prefs) {
		gtk_window_present(GTK_WINDOW(prefs));
		return;
	}

	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	misc_options_new = misc_options;
	logging_options_new = logging_options;
	blist_options_new = blist_options;
	convo_options_new = convo_options;
	im_options_new = im_options;
	chat_options_new = chat_options;
	font_options_new = font_options;
	sound_options_new = sound_options;
	for (r = 0; r < NUM_SOUNDS; r++)
		sound_file_new[r] = g_strdup(sound_file[r]);
	away_options_new = away_options;
	away_resend_new = away_resend;
	report_idle_new = report_idle;
	auto_away_new = auto_away;
	default_away_new = default_away;
	fontsize_new = fontsize;
	web_browser_new = web_browser;
	proxytype_new = proxytype;
#ifndef _WIN32
	g_snprintf(sound_cmd_new, sizeof(sound_cmd_new), "%s", sound_cmd);
#endif
	g_snprintf(web_command_new, sizeof(web_command_new), "%s", 
		   web_command ? web_command : "xterm -e lynx %%s");
	g_snprintf(fontface_new, sizeof(fontface_new), fontface);
	memcpy(&conv_size_new, &conv_size, sizeof(struct window_size));
	memcpy(&buddy_chat_size_new, &buddy_chat_size, sizeof(struct window_size));
	memcpy(&fgcolor_new, &fgcolor, sizeof(GdkColor));
	memcpy(&bgcolor_new, &bgcolor, sizeof(GdkColor));

	/* Create the window */
	prefs = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(prefs), "preferences", "Gaim");
	gtk_widget_realize(prefs);
	gtk_window_set_title(GTK_WINDOW(prefs), _("Gaim - Preferences"));
	gtk_window_set_policy (GTK_WINDOW(prefs), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(prefs), "destroy", GTK_SIGNAL_FUNC(delete_prefs), NULL);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(prefs), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER(vbox), hbox);
	gtk_widget_show (hbox);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	/* The tree -- much inspired by the Gimp */
	prefstree = gtk_tree_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
	tree_v = gtk_tree_view_new_with_model (GTK_TREE_MODEL (prefstree));
	gtk_container_add (GTK_CONTAINER (frame), tree_v);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_v), FALSE);
	gtk_widget_show(tree_v);
	/* icons */
	cell = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("icons", cell, "pixbuf", 0, NULL);

	/* text */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("text", cell, "text", 1, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_v), column);

	/* The right side */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	gtk_widget_show (vbox2);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_box_pack_start (GTK_BOX (vbox2), frame, FALSE, TRUE, 0);
	gtk_widget_show (frame);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	gtk_widget_show (hbox);

	preflabel = gtk_label_new(NULL);
	gtk_box_pack_end (GTK_BOX (hbox), preflabel, FALSE, FALSE, 0);
	gtk_widget_show (preflabel);

	/* The notebook */
	prefsnotebook = notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox2), notebook, FALSE, FALSE, 0);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_v));
	g_signal_connect (G_OBJECT (sel), "changed",
			   G_CALLBACK (pref_nb_select),
			   notebook);
	gtk_widget_show(notebook);
	sep = gtk_hseparator_new();
	gtk_widget_show(sep);
	gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);

	/* The buttons to press! */
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER(vbox), hbox);
	gtk_widget_show (hbox);

	button = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(ok_cb), prefs);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), prefs);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_from_stock (GTK_STOCK_APPLY);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(apply_cb), prefs);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);


	prefs_notebook_init();

	gtk_tree_view_expand_all (GTK_TREE_VIEW(tree_v));
	gtk_widget_show(prefs);
}

static gint debug_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	if (debugbutton)
		gtk_button_clicked(GTK_BUTTON(debugbutton));
	if (misc_options & OPT_MISC_DEBUG) {
		misc_options_new ^= OPT_MISC_DEBUG;
		misc_options ^= OPT_MISC_DEBUG;
		save_prefs();
	}
	g_free(dw);
	dw = NULL;
	return FALSE;

}

static void build_debug()
{
	GtkWidget *scroll;
	GtkWidget *box;
	if (!dw)
		dw = g_new0(struct debug_window, 1);

	GAIM_DIALOG(dw->window);
	gtk_window_set_title(GTK_WINDOW(dw->window), _("Gaim debug output window"));
	gtk_window_set_wmclass(GTK_WINDOW(dw->window), "debug_out", "Gaim");
	gtk_signal_connect(GTK_OBJECT(dw->window), "delete_event", GTK_SIGNAL_FUNC(debug_delete), NULL);
	gtk_widget_realize(dw->window);

	box = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dw->window), box);
	gtk_widget_show(box);

	dw->entry = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(dw->entry), TRUE);
	gtk_text_set_editable(GTK_TEXT(dw->entry), FALSE);
	gtk_container_add(GTK_CONTAINER(box), dw->entry);
	gtk_widget_set_usize(dw->entry, 500, 200);
	gtk_widget_show(dw->entry);

	scroll = gtk_vscrollbar_new(GTK_TEXT(dw->entry)->vadj);
	gtk_box_pack_start(GTK_BOX(box), scroll, FALSE, FALSE, 0);
	gtk_widget_show(scroll);

	gtk_widget_show(dw->window);
}

void show_debug()
{
	if ((misc_options & OPT_MISC_DEBUG)) {
		if (!dw || !dw->window)
			build_debug();
		gtk_widget_show(dw->window);
	} else {
		if (!dw)
			return;
		gtk_widget_destroy(dw->window);
		dw->window = NULL;
	}
}

void debug_printf(char *fmt, ...)
{
	va_list ap;
	gchar *s;

	va_start(ap, fmt);
	s = g_strdup_vprintf(fmt, ap);
	va_end(ap);

	if (misc_options & OPT_MISC_DEBUG && dw) {
		GtkAdjustment *adj = GTK_TEXT(dw->entry)->vadj;
		gboolean scroll = (adj->value == adj->upper - adj->lower - adj->page_size);

		gtk_text_freeze(GTK_TEXT(dw->entry));
		gtk_text_insert(GTK_TEXT(dw->entry), NULL, NULL, NULL, s, -1);
		gtk_text_thaw(GTK_TEXT(dw->entry));

		if (scroll)
			gtk_adjustment_set_value(adj, adj->upper - adj->lower - adj->page_size);
	}
	if (opt_debug)
		g_print("%s", s);
	g_free(s);
}






void set_option(GtkWidget *w, int *option)
{
	*option = !(*option);
}

static void set_misc_option(GtkWidget *w, int option)
{
	misc_options_new ^= option;

	/* gross hack because we don't want to save all prefs just because
	 * we enabled the debug window.  then again, what about the debug
	 * window _isn't_ an ugly hack? */

	if (option == OPT_MISC_DEBUG) {
		misc_options ^= option;
		save_prefs();
		show_debug();
	}
}


/* Functions for each _options variable that sees what's changed, and makes
 * effective those changes that take place immediately (UI stuff) */
static void set_misc_options()
{

/*	int option = misc_options ^ misc_options_new; */
	misc_options = misc_options_new;

}

static void set_logging_options()
{
	int option = logging_options ^ logging_options_new;
	logging_options = logging_options_new;

	if (option & OPT_LOG_CONVOS || option & OPT_LOG_CHATS)
		update_log_convs();

}

static void set_blist_options()
{
	int option = blist_options ^ blist_options_new;
	blist_options = blist_options_new;

	if (!blist)
		return;

	if (option & OPT_BLIST_BOTTOM_TAB)
		set_blist_tab();

	if (option & OPT_BLIST_NO_BUTTONS)
		build_imchat_box(!(blist_options & OPT_BLIST_NO_BUTTONS));

	if (option & OPT_BLIST_SHOW_GRPNUM)
		update_num_groups();

	if (option & OPT_BLIST_NO_MT_GRP)
		toggle_show_empty_groups();

	if ((option & OPT_BLIST_SHOW_BUTTON_XPM) || (option & OPT_BLIST_NO_BUTTONS))
		update_button_pix();

	if (option & OPT_BLIST_SHOW_PIXMAPS)
		toggle_buddy_pixmaps();

	if ((option & OPT_BLIST_GREY_IDLERS) || (option & OPT_BLIST_SHOW_IDLETIME))
		update_idle_times();

}

static void set_convo_options()
{
	int option = convo_options ^ convo_options_new;
	convo_options = convo_options_new;

	if (option & OPT_CONVO_SHOW_SMILEY)
		toggle_smileys();

	if (option & OPT_CONVO_SHOW_TIME)
		toggle_timestamps();

	if (option & OPT_CONVO_CHECK_SPELLING)
		toggle_spellchk();

	if (option & OPT_CONVO_COMBINE) {
		/* (OPT_IM_SIDE_TAB | OPT_IM_BR_TAB) == (OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB) */

	}

}

static void set_im_options()
{
	int option = im_options ^ im_options_new;
	im_options = im_options_new;

	if (option & OPT_IM_ONE_WINDOW)
		im_tabize();

	if (option & OPT_IM_SIDE_TAB || option & OPT_IM_BR_TAB)
		update_im_tabs();

	if (option & OPT_IM_HIDE_ICONS)
		set_hide_icons();

	if (option & OPT_IM_ALIAS_TAB)
		set_convo_titles();

	if (option & OPT_IM_NO_ANIMATION)
		set_anim();

	if (option & OPT_IM_BUTTON_TEXT || option & OPT_IM_BUTTON_XPM)
		update_im_button_pix();
}

static void set_chat_options()
{
	int option = chat_options ^ chat_options_new;
	chat_options = chat_options_new;

	if (option & OPT_CHAT_ONE_WINDOW)
		chat_tabize();

	if (option & OPT_CHAT_BUTTON_TEXT || option & OPT_CHAT_BUTTON_XPM)
		update_chat_button_pix();
}

void set_sound_options()
{
	/* int option = sound_options ^ sound_options_new; */
	sound_options = sound_options_new;

}

static void set_font_options()
{
	/* int option = font_options ^ font_options_new; */
	font_options = font_options_new;

	update_font_buttons();
}

static void set_away_options()
{
	int option = away_options ^ away_options_new;
	away_options = away_options_new;

	if (option & OPT_AWAY_QUEUE)
		toggle_away_queue();
}

static void toggle_option(GtkWidget *w, int option) {
	int *o = gtk_object_get_user_data(GTK_OBJECT(w));
	*o ^= option;
}

GtkWidget *gaim_button(const char *text, guint *options, int option, GtkWidget *page)
{
	GtkWidget *button;
	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (*options & option));
	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(button), options);

	/* So that the debug window happens immediately
	 * I don't think it should be "preferences," anyway. */
	if (options == &misc_options_new && option == OPT_MISC_DEBUG)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(set_misc_option),
				   (int *)option);
	else
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_option),
				   (int *)option);

	gtk_widget_show(button);

	return button;
}

void away_list_clicked(GtkWidget *widget, struct away_message *a)
{}
void default_away_menu_init(GtkWidget *omenu)
{
	GtkWidget *menu, *opt;
	int index = 0;
	GSList *awy = away_messages;
	struct away_message *a;

	menu = gtk_menu_new();

	while (awy) {
		a = (struct away_message *)awy->data;
		opt = gtk_menu_item_new_with_label(a->name);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_default_away),
				   (gpointer)index);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);

		awy = awy->next;
		index++;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(omenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), g_slist_index(away_messages, default_away));
}

GtkWidget *pref_fg_picture = NULL;
GtkWidget *pref_bg_picture = NULL;

void destroy_colorsel(GtkWidget *w, gpointer d)
{
	if (d) {
		gtk_widget_destroy(fgcseld);
		fgcseld = NULL;
	} else {
		gtk_widget_destroy(bgcseld);
		bgcseld = NULL;
	}
}

void apply_color_dlg(GtkWidget *w, gpointer d)
{
	if ((int)d == 1) {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION
						      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel),
						      &fgcolor_new);
		destroy_colorsel(NULL, (void *)1);
		update_color(NULL, pref_fg_picture);
	} else {
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION
						      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel),
						      &bgcolor_new);
		destroy_colorsel(NULL, (void *)0);
		update_color(NULL, pref_bg_picture);
	}
}

void update_color(GtkWidget *w, GtkWidget *pic)
{
	GdkColor c;
	GtkStyle *style;
	c.pixel = 0;

	if (pic == pref_fg_picture) {
		if (font_options_new & OPT_FONT_FGCOL) {
			c.red = fgcolor_new.red;
			c.blue = fgcolor_new.blue;
			c.green = fgcolor_new.green;
		} else {
			c.red = 0;
			c.blue = 0;
			c.green = 0;
		}
	} else {
		if (font_options_new & OPT_FONT_BGCOL) {
			c.red = bgcolor_new.red;
			c.blue = bgcolor_new.blue;
			c.green = bgcolor_new.green;
		} else {
			c.red = 0xffff;
			c.blue = 0xffff;
			c.green = 0xffff;
		}
	}

	style = gtk_style_new();
	style->bg[0] = c;
	gtk_widget_set_style(pic, style);
	gtk_style_unref(style);
}
void set_default_away(GtkWidget *w, gpointer i)
{

	int length = g_slist_length(away_messages);

	if (away_messages == NULL)
		default_away_new = NULL;
	else if ((int)i >= length)
		default_away_new = g_slist_nth_data(away_messages, length - 1);
	else
		default_away_new = g_slist_nth_data(away_messages, (int)i);
}

static void update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	int *value = gtk_object_get_user_data(GTK_OBJECT(spin));
	*value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
}
GtkWidget *gaim_labeled_spin_button(GtkWidget *box, const gchar *title, int *val, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *spin;
	GtkObject *adjust;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(title);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	adjust = gtk_adjustment_new(*val, min, max, 1, 1, 1);
	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adjust), 1, 0);
	gtk_object_set_user_data(GTK_OBJECT(spin), val);
	gtk_widget_set_usize(spin, 50, -1);
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(adjust), "value-changed",
			   GTK_SIGNAL_FUNC(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

	if (sg) {
		gtk_size_group_add_widget(sg, label);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	}
	return label;
}

void dropdown_set(GtkObject *w, int *option)
{
	int opt = (int)gtk_object_get_user_data(w);
	int clear = (int)gtk_object_get_data(w, "clear");

	if (clear != -1) {
		*option = *option & ~clear;
		*option = *option | opt;
	} else {
		debug_printf("HELLO %d\n", opt);
		*option = opt;
	}

	if (option == &proxytype_new) {
		if (opt == PROXY_NONE)
			gtk_widget_set_sensitive(prefs_proxy_frame, FALSE);
		else
			gtk_widget_set_sensitive(prefs_proxy_frame, TRUE);
	} else if (option == &web_browser_new) {
		if (opt == BROWSER_MANUAL)
			gtk_widget_set_sensitive(browser_entry, TRUE);
		else
			gtk_widget_set_sensitive(browser_entry, FALSE);
	} else if (*option == sound_options_new) {
		if (opt == OPT_SOUND_CMD)
			gtk_widget_set_sensitive(sndcmd, TRUE);
		else
			gtk_widget_set_sensitive(sndcmd, FALSE);
	}

}

static GtkWidget *gaim_dropdown(GtkWidget *box, const gchar *title, int *option, int clear, ...)
{
	va_list menuitems;
	GtkWidget *dropdown, *opt, *menu;
	GtkWidget *label;
	gchar     *text;
	int       value;
	int       o = 0;
	GtkWidget *hbox;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (box), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(title);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	va_start(menuitems, clear);
	text = va_arg(menuitems, gchar *);

	if (!text)
		return NULL;

	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();

	gtk_label_set_mnemonic_widget(GTK_LABEL(label), dropdown);

	while (text) {
		value = va_arg(menuitems, int);
		opt = gtk_menu_item_new_with_label(text);
		gtk_object_set_user_data(GTK_OBJECT(opt), (void *)value);
		gtk_object_set_data(GTK_OBJECT(opt), "clear", (void *)clear);
		gtk_signal_connect(GTK_OBJECT(opt), "activate",
				   GTK_SIGNAL_FUNC(dropdown_set), (void *)option);
		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);

		if (((clear > -1) && ((*option & value) == value)) || *option == value) {
			gtk_menu_set_active(GTK_MENU(menu), o);
		}
		text = va_arg(menuitems, gchar *);
		o++;
	}

	va_end(menuitems);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);
	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);
	gtk_widget_show(dropdown);
	return label;
}

static GtkWidget *show_color_pref(GtkWidget *box, gboolean fgc)
{
	/* more stuff stolen from X-Chat */
	GtkWidget *swid;
	GdkColor c;
	GtkStyle *style;
	c.pixel = 0;
	if (fgc) {
		if (font_options_new & OPT_FONT_FGCOL) {
			c.red = fgcolor_new.red;
			c.blue = fgcolor_new.blue;
			c.green = fgcolor_new.green;
		} else {
			c.red = 0;
			c.blue = 0;
			c.green = 0;
		}
	} else {
		if (font_options_new & OPT_FONT_BGCOL) {
			c.red = bgcolor_new.red;
			c.blue = bgcolor_new.blue;
			c.green = bgcolor_new.green;
		} else {
			c.red = 0xffff;
			c.blue = 0xffff;
			c.green = 0xffff;
		}
	}

	style = gtk_style_new();
	style->bg[0] = c;

	swid = gtk_event_box_new();
	gtk_widget_set_style(GTK_WIDGET(swid), style);
	gtk_style_unref(style);
	gtk_widget_set_usize(GTK_WIDGET(swid), 40, -1);
	gtk_box_pack_start(GTK_BOX(box), swid, FALSE, FALSE, 5);
	gtk_widget_show(swid);
	return swid;
}
void apply_font_dlg(GtkWidget *w, GtkWidget *f)
{
	int i = 0;
	char *fontname;

	fontname = g_strdup(gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(f)));
	destroy_fontsel(0, 0);

	while(fontname[i] && !isdigit(fontname[i]) && i < sizeof(fontface_new)) {
		fontface_new[i] = fontname[i];
		i++;
	}
	fontface_new[i] = 0;
	debug_printf("fontface_new: %s\n", fontface_new);
	g_free(fontname);
}


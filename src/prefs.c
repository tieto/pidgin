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
#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include "gaim.h"
#include "prpl.h"
#include "proxy.h"

/* xpms for gtk1.2 */
#if !GTK_CHECK_VERSION (1,3,0)
#include "pixmaps/cancel.xpm"
#include "pixmaps/fontface2.xpm"
#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"
#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/bgcolor.xpm"
#include "pixmaps/fgcolor.xpm"
#include "pixmaps/save.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/join.xpm"
#endif


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
#if !GTK_CHECK_VERSION(1,3,0)
char fontxfld_new[256];
char fontfacexfld[256];
#endif
char fontface[128];


GtkWidget *prefs_away_list = NULL;
GtkWidget *prefs_away_menu = NULL;
GtkWidget *preftree = NULL;
GtkWidget *fontseld = NULL;

#if GTK_CHECK_VERSION(1,3,0)
GtkTreeStore *prefs_away_store = NULL;
#endif

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
GtkWidget *gaim_labeled_spin_button(GtkWidget *, const gchar *, int*, int, int);
static GtkWidget *gaim_dropdown(GtkWidget *, const gchar *, int *, int, ...);
static GtkWidget *show_color_pref(GtkWidget *, gboolean);
static void delete_prefs(GtkWidget *, void *);
void set_default_away(GtkWidget *, gpointer);
static void set_font_option(GtkWidget *w, int option);

struct debug_window *dw = NULL;
static GtkWidget *prefs = NULL;
GtkWidget *debugbutton = NULL;

void delete_prefs(GtkWidget *asdf, void *gdsa) {
	int v;
	
	 prefs = NULL;
	 for (v = 0; v < NUM_SOUNDS; v++) {
		if (sound_file_new[v])
			g_free(sound_file_new[v]);
		sound_file_new[v] = NULL;
	 }
	 sound_entry = NULL;
	 browser_entry = NULL;
	 debugbutton=NULL;
	 gtk_widget_destroy(sounddialog);
#if GTK_CHECK_VERSION(1,3,0)
	 g_object_unref(G_OBJECT(prefs_away_store)); 	
#endif
}

GtkWidget *preflabel;
GtkWidget *prefsnotebook;
#if GTK_CHECK_VERSION(1,3,0)
GtkTreeStore *prefstree;
#else
GtkWidget *prefstree;
#endif

static void set_misc_options();	
static void set_logging_options();
static void set_blist_options(); 
static void set_convo_options();
static void set_im_options();
static void set_chat_options();
static void set_font_options();
static void set_sound_options();
static void set_away_options();

extern void BuddyTickerShow();

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
	save_prefs();
}


/* OK, Apply and Cancel */

static void apply_cb(GtkWidget *button, void *data)
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
		sound_file[r] = sound_file_new[r];
		sound_file_new[r] = NULL;
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
	g_snprintf(sound_cmd, sizeof(sound_cmd), "%s", sound_cmd_new);
	g_snprintf(web_command, sizeof(web_command), "%s", web_command_new);
	memcpy(&conv_size, &conv_size_new, sizeof(struct window_size));
	memcpy(&conv_size, &conv_size_new, sizeof(struct window_size));	
	memcpy(&fgcolor, &fgcolor_new, sizeof(GdkColor));
	memcpy(&bgcolor, &bgcolor_new, sizeof(GdkColor));


	g_snprintf(fontface, sizeof(fontface), fontface_new);
#if !GTK_CHECK_VERSION(1,3,0)
	g_snprintf(fontxfld, sizeof(fontxfld), fontxfld_new);
#endif	
	update_convo_font();
	update_convo_color();
	save_prefs();
}


static void ok_cb(GtkWidget *button, void *data)
{
	apply_cb(button, data);
	gtk_widget_destroy(prefs);
}
#if GTK_CHECK_VERSION(1,3,0)
static void pref_nb_select(GtkTreeSelection *sel, GtkNotebook *nb) {
	GtkTreeIter   iter;
	GValue val = { 0, };
	GtkTreeModel *model = GTK_TREE_MODEL(prefstree);
	
	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
	gtk_tree_model_get_value (model, &iter, 1, &val);
	gtk_label_set_text (GTK_LABEL(preflabel), g_value_get_string (&val));
	g_value_unset (&val);
	gtk_tree_model_get_value (model, &iter, 2, &val);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (prefsnotebook), g_value_get_int (&val));

}
#else
static void pref_nb_select(GtkCTree *ctree, GtkCTreeNode *node, gint column, GtkNotebook *nb) {
	char *text;
	gtk_ctree_get_node_info(GTK_CTREE(ctree), node, &text, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	gtk_label_set_text (GTK_LABEL(preflabel), text);
	gtk_notebook_set_page (GTK_NOTEBOOK (prefsnotebook), gtk_ctree_node_get_row_data(GTK_CTREE(ctree), node));

}
#endif /* GTK_CHECK_VERSION */

/* These are the pages in the preferences notebook */
GtkWidget *interface_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	/* All the pages are pretty similar--a vbox packed with frames... */
	frame = gtk_frame_new (_("Windows"));
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	/* And a vbox in each frame */
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	/* These shouldn't have to wait for user to click OK or APPLY or whatnot */
	/* They really shouldn't be in preferences at all */
	gaim_button(_("Show Buddy Ticker"), &misc_options, OPT_MISC_BUDDY_TICKER, vbox);
	debugbutton = gaim_button(_("Show Debug Window"), &misc_options, OPT_MISC_DEBUG, vbox);
	gtk_widget_show (vbox);

	
	frame = gtk_frame_new (_("Style"));
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Use borderless buttons"), &misc_options_new, OPT_MISC_COOL_LOOK, vbox);
	gtk_widget_show (vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *font_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *button;
	GtkWidget *vbox, *hbox;
	GtkWidget *select = NULL;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new (_("Style"));
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);	
	gaim_button(_("Bold"), &font_options_new, OPT_FONT_BOLD, vbox);	
	gaim_button(_("Italics"), &font_options_new, OPT_FONT_ITALIC, vbox);
	gaim_button(_("Underline"), &font_options_new, OPT_FONT_UNDERLINE, vbox);
	gaim_button(_("Strikethough"), &font_options_new, OPT_FONT_STRIKE, vbox);
	gtk_widget_show (vbox);
 

	frame = gtk_frame_new (_("Face"));
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	button = gaim_button(_("Use custom face"), &font_options_new, OPT_FONT_FACE, hbox);
#if GTK_CHECK_VERSION(1,3,0)
	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_FONT);
#else
	select = picture_button(prefs, _("Select"), fontface2_xpm);
#endif
	if (!(font_options_new & OPT_FONT_FACE))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_font_dialog), NULL);	
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(select), GTK_RELIEF_NONE);
	gtk_widget_show(select);
	gtk_widget_show(hbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	button = gaim_button(_("Use custom size"), &font_options_new, OPT_FONT_SIZE, hbox);
	select = gaim_labeled_spin_button(hbox, NULL, &fontsize_new, 1, 7);
	if (!(font_options_new & OPT_FONT_SIZE))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_widget_show(hbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Color");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	
	pref_fg_picture = show_color_pref(hbox, TRUE);
	button = gaim_button(_("Text color"), &font_options_new, OPT_FONT_FGCOL, hbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_fg_picture);
	
#if GTK_CHECK_VERSION(1,3,0)
	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_COLOR);
#else
	select = picture_button(prefs, _("Select"), fgcolor_xpm);
#endif
	if (!(font_options_new & OPT_FONT_FGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_fgcolor_dialog), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(select), GTK_RELIEF_NONE);
	gtk_widget_show(select);
	gtk_widget_show(hbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	pref_bg_picture = show_color_pref(hbox, FALSE);
	button = gaim_button(_("Background color"), &font_options_new, OPT_FONT_BGCOL, hbox);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(update_color),
			   pref_bg_picture);
#if GTK_CHECK_VERSION(1,3,0)
	select = gtk_button_new_from_stock(GTK_STOCK_SELECT_COLOR);
#else
	select = picture_button(prefs, _("Select"), bgcolor_xpm);
#endif	
	if (!(font_options_new & OPT_FONT_BGCOL))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(select), "clicked", GTK_SIGNAL_FUNC(show_bgcolor_dialog), NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);
	gtk_box_pack_start(GTK_BOX(hbox), select, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(select), GTK_RELIEF_NONE);
	gtk_widget_show(select);
	gtk_widget_show(hbox);
	gtk_widget_show (vbox);


	gtk_widget_show(ret);
	return ret;
}


GtkWidget *messages_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Display");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Show graphical smileys"), &convo_options_new, OPT_CONVO_SHOW_SMILEY, vbox);
	gaim_button(_("Show timestamp on messages"), &convo_options_new, OPT_CONVO_SHOW_TIME, vbox);
	gaim_button(_("Show URLs as links"), &convo_options_new, OPT_CONVO_SEND_LINKS, vbox);
	gaim_button(_("Highlight misspelled words"), &convo_options_new, OPT_CONVO_CHECK_SPELLING, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Ignore");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Ignore colors"), &convo_options_new, OPT_CONVO_IGNORE_COLOUR, vbox);
	gaim_button(_("Ignore font faces"), &convo_options_new, OPT_CONVO_IGNORE_FONTS, vbox);
	gaim_button(_("Ignore font sizes"), &convo_options_new, OPT_CONVO_IGNORE_SIZES, vbox);
	gaim_button(_("Ignore TiK Automated Messages"), &away_options_new, OPT_AWAY_TIK_HACK, vbox);
	gtk_widget_show (vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *hotkeys_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Send Message");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Enter sends message"), &convo_options_new, OPT_CONVO_ENTER_SENDS, vbox);
	gaim_button(_("Control-Enter sends message"), &convo_options_new, OPT_CONVO_CTL_ENTER, vbox);
	gtk_widget_show (vbox);

	frame = gtk_frame_new ("Window Closing");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Escape closes window"), &convo_options_new, OPT_CONVO_ESC_CAN_CLOSE, vbox);
	gaim_button(_("Control-W closes window"), &convo_options_new, OPT_CONVO_CTL_W_CLOSES, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Insertions");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Control-{B/I/U/S} inserts HTML tags"), &convo_options_new, OPT_CONVO_CTL_CHARS, vbox);
	gaim_button(_("Control-(number) inserts smileys"), &convo_options_new, OPT_CONVO_CTL_SMILEYS, vbox);
	gtk_widget_show (vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *list_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Buttons");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Hide IM/Info/Chat buttons"), &blist_options_new, OPT_BLIST_NO_BUTTONS, vbox);
	gaim_button(_("Show pictures on buttons"), &blist_options_new, OPT_BLIST_SHOW_BUTTON_XPM, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Buddy List Window");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
#ifdef USE_APPLET
	gaim_button(_("Automatically show buddy list on sign on"), &blist_options_new,
		    OPT_BLIST_APP_BUDDY_SHOW, vbox);
	gaim_button(_("Display Buddy List near applet"), &blist_options_new, OPT_BLIST_NEAR_APPLET, vbox);
	
#endif
	gaim_button(_("Save Window Size/Position"), &blist_options_new, OPT_BLIST_SAVED_WINDOWS, vbox);
	gaim_button(_("Raise Window on Events"), &blist_options_new, OPT_BLIST_POPUP, vbox);
	gtk_widget_show(vbox);


	frame = gtk_frame_new ("Group Display");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Hide groups with no online buddies"), &blist_options_new, OPT_BLIST_NO_MT_GRP, vbox);
	gaim_button(_("Show numbers in groups"), &blist_options_new, OPT_BLIST_SHOW_GRPNUM, vbox);
	gtk_widget_show(vbox);
			

	frame = gtk_frame_new ("Buddy Display");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Show buddy type icons"), &blist_options_new, OPT_BLIST_SHOW_PIXMAPS, vbox);
	gaim_button(_("Show warning levels"), &blist_options_new, OPT_BLIST_SHOW_WARN, vbox);
	gaim_button(_("Show idle times"), &blist_options_new, OPT_BLIST_SHOW_IDLETIME, vbox);
	gaim_button(_("Grey idle buddies"), &blist_options_new, OPT_BLIST_GREY_IDLERS, vbox);
	gtk_widget_show(vbox);
		


	gtk_widget_show (vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *im_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;	
	GtkWidget *typingbutton;

	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Window");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_dropdown(vbox, "Show buttons as:", &im_options_new, OPT_IM_BUTTON_TEXT | OPT_IM_BUTTON_XPM, 
		      "Pictures", OPT_IM_BUTTON_XPM, 
		      "Text", OPT_IM_BUTTON_TEXT,
		      "Pictures and text", OPT_IM_BUTTON_XPM | OPT_IM_BUTTON_TEXT, NULL);
	gaim_labeled_spin_button(vbox, _("New window width:"), &conv_size_new.width, 25, 9999);
	gaim_labeled_spin_button(vbox, _("New window height:"), &conv_size_new.height, 25, 9999);
	gaim_labeled_spin_button(vbox, _("Entry widget height:"), &conv_size_new.entry_height, 25, 9999);
	gaim_button(_("Raise windows on events"), &im_options_new, OPT_IM_POPUP, vbox);
	gaim_button(_("Hide window on send"), &im_options_new, OPT_IM_POPDOWN, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Buddy Icons");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Hide Buddy Icons"), &im_options_new, OPT_IM_HIDE_ICONS, vbox);
	gaim_button(_("Disable Buddy Icon Animation"), &im_options_new, OPT_IM_NO_ANIMATION, vbox);
	gtk_widget_show(vbox);


	frame = gtk_frame_new ("Display");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Show logins in window"), &im_options_new, OPT_IM_LOGON, vbox);
	gtk_widget_show(vbox);
			

	frame = gtk_frame_new ("Typing Notification");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	typingbutton = gaim_button(_("Notify buddies that you are typing to them"), &misc_options_new,
				   OPT_MISC_STEALTH_TYPING, vbox);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(typingbutton), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(typingbutton)));
	misc_options ^= OPT_MISC_STEALTH_TYPING;
	gtk_widget_show(vbox);
		


	gtk_widget_show (vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *chat_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Window");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_dropdown(vbox, "Show buttons as:", &chat_options_new, OPT_CHAT_BUTTON_TEXT | OPT_CHAT_BUTTON_XPM,
		      "Pictures", OPT_CHAT_BUTTON_XPM, 
		      "Text", OPT_CHAT_BUTTON_TEXT,
		      "Pictures and Text", OPT_CHAT_BUTTON_XPM | OPT_CHAT_BUTTON_TEXT, NULL);
	gaim_labeled_spin_button(vbox, _("New window width:"), &buddy_chat_size_new.width, 25, 9999);
	gaim_labeled_spin_button(vbox, _("New window height:"), &buddy_chat_size_new.height, 25, 9999);
	gaim_labeled_spin_button(vbox, _("Entry widget height:"), &buddy_chat_size_new.entry_height, 25, 9999);
	gaim_button(_("Raise windows on events"), &chat_options, OPT_CHAT_POPUP, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Tab Completion");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Tab-Complete Nicks"), &chat_options_new, OPT_CHAT_TAB_COMPLETE, vbox);
	gaim_button(_("Old-Style Tab Completion"), &chat_options_new, OPT_CHAT_OLD_STYLE_TAB, vbox);
	gtk_widget_show(vbox);


	frame = gtk_frame_new ("Display");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Show people joining/leaving in window"), &chat_options_new, OPT_CHAT_LOGON, vbox);
	gaim_button(_("Colorize screennames"), &chat_options_new, OPT_CHAT_COLORIZE, vbox);
	gtk_widget_show(vbox);
			
	gtk_widget_show(ret);
	return ret;
}

GtkWidget *tab_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("IM Tabs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gaim_dropdown(vbox, "Tab Placement:", &im_options_new, OPT_IM_SIDE_TAB | OPT_IM_BR_TAB, 
		      "Top", 0, 
		      "Bottom", OPT_IM_BR_TAB,
		      "Left", OPT_IM_SIDE_TAB,
		      "Right", OPT_IM_BR_TAB | OPT_IM_SIDE_TAB, NULL);
	gaim_button(_("Show all Instant Messages in one tabbed\nwindow"), &im_options_new, OPT_IM_ONE_WINDOW, vbox);
	gaim_button(_("Show aliases in tabs/titles"), &im_options_new, OPT_IM_ALIAS_TAB, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Chat Tabs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gaim_dropdown(vbox, "Tab Placement:", &chat_options_new, OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB,
		      "Top", 0, 
		      "Bottom", OPT_CHAT_BR_TAB,
		      "Left", OPT_CHAT_SIDE_TAB,
		      "Right", OPT_CHAT_SIDE_TAB | OPT_CHAT_BR_TAB, NULL);
	gaim_button(_("Show all chats in one tabbed window"), &chat_options_new, OPT_CHAT_ONE_WINDOW,
		    vbox);
	gtk_widget_show(vbox);

	frame = gtk_frame_new ("Combined Tabs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_widget_show(vbox);
	gaim_button(_("Show IMs and chats in same tabbed\nwindow."), &convo_options_new, OPT_CONVO_COMBINE, vbox);
	
	frame = gtk_frame_new ("Buddy List Tabs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gaim_dropdown(vbox, "Tab Placement:", &blist_options_new, OPT_BLIST_BOTTOM_TAB, 
		      "Top", 0, 
		      "Bottom", OPT_BLIST_BOTTOM_TAB, NULL);
	gtk_widget_show(vbox);
		      
	gtk_widget_show(ret);
	return ret;
}

GtkWidget *proxy_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *table;

	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Proxy Type");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_dropdown(vbox, "Proxy Type:", &proxytype_new, -1,
		      "No Proxy", PROXY_NONE, 
		      "SOCKS 4", PROXY_SOCKS4,
		      "SOCKS 5", PROXY_SOCKS5,
		      "HTTP", PROXY_HTTP, NULL);
	gtk_widget_show (vbox);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);

	frame = gtk_frame_new(_("Proxy Server"));
	prefs_proxy_frame = frame;

	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 5);

	if (proxytype == PROXY_NONE)
		gtk_widget_set_sensitive(GTK_WIDGET(frame), FALSE);

	table = gtk_table_new(2, 4, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_widget_show(table);
	gtk_container_add(GTK_CONTAINER(frame), table);


	label = gtk_label_new(_("Host"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYHOST);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyhost);
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Port"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPORT);

	if (proxyport) {
		char buf[128];
		g_snprintf(buf, sizeof(buf), "%d", proxyport);
		gtk_entry_set_text(GTK_ENTRY(entry), buf);
	}
	gtk_widget_show(entry);

	label = gtk_label_new(_("User"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYUSER);
	gtk_entry_set_text(GTK_ENTRY(entry), proxyuser);
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Password"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, 3, 4, GTK_FILL , 0, 0, 0);
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			   GTK_SIGNAL_FUNC(proxy_print_option), (void *)PROXYPASS);
	gtk_entry_set_text(GTK_ENTRY(entry), proxypass);
	gtk_widget_show(entry);
	
	gtk_widget_show(ret);
	return ret;
}

GtkWidget *browser_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox, *hbox;
	GtkWidget *label;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Browser Selection");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_dropdown(vbox, "Browser", &web_browser_new, -1, 
		      "Netscape", BROWSER_NETSCAPE,
		      "Konqueror", BROWSER_KONQ,
		      "Mozilla", BROWSER_MOZILLA,
		      "Manual", BROWSER_MANUAL,
#ifdef USE_GNOME
		      "GNOME URL Handler", BROWSER_GNOME, 
#endif /* USE_GNOME */
		      "Galeon", BROWSER_GALEON,
		      "Opera", BROWSER_OPERA, NULL);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new("Manual: ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	browser_entry = gtk_entry_new();
	if (web_browser_new != BROWSER_MANUAL)
		gtk_widget_set_sensitive(browser_entry, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), browser_entry, FALSE, FALSE, 0);
	gtk_widget_show(browser_entry);
	gtk_widget_show(hbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Browser Options");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Open new window by default"), &misc_options_new, OPT_MISC_BROWSER_POPUP, vbox);
	gtk_widget_show(vbox);

	gtk_widget_show(ret);
	return ret;
}	

GtkWidget *logging_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Message Logs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Log all conversations"), &logging_options_new, OPT_LOG_ALL, vbox);
	gaim_button(_("Strip HTML from logs"), &logging_options_new, OPT_LOG_STRIP_HTML, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("System Logs");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Log when buddies sign on/sign off"), &logging_options_new, OPT_LOG_BUDDY_SIGNON,
		    vbox);
	gaim_button(_("Log when buddies become idle/un-idle"), &logging_options_new, OPT_LOG_BUDDY_IDLE,
		    vbox);
	gaim_button(_("Log when buddies go away/come back"), &logging_options_new, OPT_LOG_BUDDY_AWAY, vbox);
	gaim_button(_("Log your own signons/idleness/awayness"), &logging_options_new, OPT_LOG_MY_SIGNON,
		    vbox);
	gaim_button(_("Individual log file for each buddy's signons"), &logging_options_new,
		    OPT_LOG_INDIVIDUAL, vbox);
	gtk_widget_show(vbox);

	gtk_widget_show(ret);
	return ret;
}

static GtkWidget *sndcmd = NULL;
/* static void set_sound_driver(GtkWidget *w, int option)
{
	if (option == OPT_SOUND_CMD)
		gtk_widget_set_sensitive(sndcmd, TRUE);
	else
		gtk_widget_set_sensitive(sndcmd, FALSE);

	sound_options &= ~(OPT_SOUND_NORMAL | OPT_SOUND_BEEP |
			   OPT_SOUND_NAS | OPT_SOUND_ARTSC |
			   OPT_SOUND_ESD | OPT_SOUND_CMD);
	sound_options |= option;
	save_prefs();
	} */

static gint sound_cmd_yeah(GtkEntry *entry, GdkEvent *event, gpointer d)
{
	g_snprintf(sound_cmd_new, sizeof(sound_cmd_new), "%s", gtk_entry_get_text(GTK_ENTRY(sndcmd)));
	return TRUE;
}

GtkWidget *sound_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *vbox, *hbox;

	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Sound Options");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("No sounds when you log in"), &sound_options, OPT_SOUND_SILENT_SIGNON, vbox);
	gaim_button(_("Sounds while away"), &sound_options, OPT_SOUND_WHEN_AWAY, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Sound Method");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	gaim_dropdown(vbox, "Method", &sound_options_new, OPT_SOUND_BEEP |
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

	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);
	label = gtk_label_new("Sound Method");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);
	label = gtk_label_new(_("Sound command\n(%s for filename)"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sndcmd = gtk_entry_new();

	gtk_entry_set_editable(GTK_ENTRY(sndcmd), TRUE);
	gtk_entry_set_text(GTK_ENTRY(sndcmd), sound_cmd);
#if GTK_CHECK_VERSION(1,3,0)
	gtk_widget_set_size_request(sndcmd, 75, -1);
#else
	gtk_widget_set_usize(sndcmd, 75, -1);
#endif
	gtk_widget_set_sensitive(sndcmd, (sound_options_new & OPT_SOUND_CMD));
	gtk_box_pack_start(GTK_BOX(hbox), sndcmd, TRUE, TRUE, 5);
	gtk_signal_connect(GTK_OBJECT(sndcmd), "focus_out_event", GTK_SIGNAL_FUNC(sound_cmd_yeah), NULL);
	gtk_widget_show(sndcmd);


	gtk_widget_show(vbox);

	gtk_widget_show(ret);
	return ret;
}

GtkWidget *away_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *select;

	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Away");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_button(_("Sending messages removes away status"), &away_options_new, OPT_AWAY_BACK_ON_IM, vbox);
	gaim_button(_("Queue new messages when away"), &away_options_new, OPT_AWAY_QUEUE, vbox);
	gaim_button(_("Ignore new conversations when away"), &away_options_new, OPT_AWAY_DISCARD, vbox);
	gtk_widget_show (vbox);


	frame = gtk_frame_new ("Auto-response");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gaim_labeled_spin_button(hbox, _("Seconds before resending:"),
				 &away_resend_new, 1, 24 * 60 * 60);
       	gtk_widget_show(hbox);
	gaim_button(_("Don't send auto-response"), &away_options, OPT_AWAY_NO_AUTO_RESP, vbox);
	gaim_button(_("Only send auto-response when idle"), &away_options_new, OPT_AWAY_IDLE_RESP, vbox);

	if (away_options_new & OPT_AWAY_NO_AUTO_RESP)
		gtk_widget_set_sensitive(hbox, FALSE);
	gtk_widget_show(vbox);


	frame = gtk_frame_new ("Idle");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gaim_dropdown(vbox, "Idle Time Reporting:", &away_resend_new, -1,
		      "None", IDLE_NONE,
		      "Gaim Usage", IDLE_GAIM, 
#ifdef USE_SCREENSAVER
		      "X Usage", IDLE_SCREENSAVER, 
#endif
		      NULL);
	gtk_widget_show(vbox);
	
	frame = gtk_frame_new ("Auto-away");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	button = gaim_button(_("Set away when idle"), &away_options_new, OPT_AWAY_AUTO, vbox);
	select = gaim_labeled_spin_button(vbox, "Minutes before setting away:", &auto_away_new, 1, 24 * 60);
	if (!(away_options_new & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(select), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), select);

	label = gtk_label_new("Away message:");
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	prefs_away_menu = gtk_option_menu_new();
	if (!(away_options_new & OPT_AWAY_AUTO))
		gtk_widget_set_sensitive(GTK_WIDGET(prefs_away_menu), FALSE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), prefs_away_menu);
	default_away_menu_init(prefs_away_menu);
	gtk_widget_show(prefs_away_menu);
	gtk_box_pack_start(GTK_BOX(hbox), prefs_away_menu, FALSE, FALSE, 0);
	gtk_widget_show (vbox);
	
	gtk_widget_show(ret);
	return ret;
}

#if GTK_CHECK_VERSION (1,3,0)
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
}
#endif

static void test_sound(GtkWidget *button, gpointer i_am_NULL)
{
	guint32 tmp_sound = sound_options;
	if (!(sound_options & OPT_SOUND_WHEN_AWAY))
		sound_options ^= OPT_SOUND_WHEN_AWAY;
	if (!(sound_options & sounds[sound_row_sel].opt))
		sound_options ^= sounds[sound_row_sel].opt;
	play_sound(sound_row_sel);
	sound_options = tmp_sound;
}

static void reset_sound(GtkWidget *button, gpointer i_am_also_NULL)
{

	/* This just resets a sound file back to default */
	sound_file_new[sound_row_sel] = NULL;

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
		free(sound_file_new[snd]);

	/* Set it -- and forget it */
	sound_file_new[snd] = g_strdup(file);
	
	/* Set our text entry */
	gtk_entry_set_text(GTK_ENTRY(sound_entry), sound_file_new[snd]);

	/* Close the window! It's getting cold in here! */
	close_sounddialog(NULL, sounddialog);

	if (last_sound_dir)
		g_free(last_sound_dir);
	last_sound_dir = g_dirname(sound_file[snd]);
}

static void sel_sound(GtkWidget *button, gpointer being_NULL_is_fun)
{
	char *buf = g_malloc(BUF_LEN);

	if (!sounddialog) {
		sounddialog = gtk_file_selection_new(_("Gaim - Sound Configuration"));

		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(sounddialog));

		g_snprintf(buf, BUF_LEN - 1, "%s/", last_sound_dir ? last_sound_dir : g_get_home_dir());

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


#if GTK_CHECK_VERSION (1,3,0)
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
#endif

GtkWidget *sound_events_page() {

	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *button, *hbox;

#if GTK_CHECK_VERSION(1,3,0)
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkListStore *event_store;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
#else
	GtkWidget *list;
#endif
	int j;

	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Sound Events");
	gtk_box_pack_start (GTK_BOX (ret), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	
#if GTK_CHECK_VERSION(1,3,0)
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
	gtk_widget_show(event_view);
	g_object_unref(G_OBJECT(event_store)); 
	gtk_container_add(GTK_CONTAINER(sw), event_view);
#else
	list = gtk_clist_new(1);
	for (j=0; sound_order[j] != 0; j++) {
		if (sounds[sound_order[j]].opt == 0)
			continue;
		gtk_clist_append(GTK_CLIST(list), &(sounds[sound_order[j]].label));
	}
	gtk_widget_show(list);
	gtk_container_add(GTK_CONTAINER(sw), list);
#endif
	
	gtk_widget_show (vbox);
	gtk_widget_show_all (ret);

	frame = gtk_frame_new ("Event Options");
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	sound_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(sound_entry), sound_file_new[0] ? sound_file_new[0] : "(default)");
	gtk_entry_set_editable(GTK_ENTRY(sound_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), sound_entry, FALSE, FALSE, 5);
	gtk_widget_show(sound_entry);
	

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	button = gtk_button_new_with_label(_("Test"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(test_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(reset_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Choose..."));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(sel_sound), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);
	gtk_widget_show(button);
	
	gtk_widget_show (vbox);

	return ret;
}

#if GTK_CHECK_VERSION (1,3,0)
void away_message_sel(GtkTreeSelection *sel, GtkTreeModel *model)
{
	GtkTreeIter  iter;
	GValue val = { 0, };
	gchar *message;
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
	GValue val = { 0, };
	
	if (! gtk_tree_selection_get_selected (sel, &prefs_away_store, &iter))
		return;
	gtk_tree_model_get_value (prefs_away_store, &iter, 1, &val);
	am = g_value_get_pointer (&val);
	gtk_imhtml_clear(GTK_IMHTML(away_text));
	rem_away_mess(NULL, am);
	gtk_list_store_remove(ts, &iter);
       	path = gtk_tree_path_new_first();
	gtk_tree_selection_select_path(sel, path);
}

#else
static struct away_message *cur_message;
void away_message_sel(GtkWidget *w, struct away_message *a) {
	gchar buffer[BUF_LONG];
	char *tmp;

	cur_message = a;

	/* Clear the Box */
	gtk_imhtml_clear(GTK_IMHTML(away_text));

	/* Fill the text box with new message */
	strncpy(buffer, a->message, BUF_LONG);
	tmp = stylize(buffer, BUF_LONG);

	debug_printf("FSD: %s\n", tmp);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), tmp, -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	gtk_imhtml_append_text(GTK_IMHTML(away_text), "<BR>", -1, GTK_IMHTML_NO_TITLE |
			       GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_SCROLL);
	g_free(tmp);
}
void remove_away_message(GtkWidget *widget, GtkWidget *list) {
	GList *i;
	struct away_message *a;

	i = GTK_LIST(prefs_away_list)->selection;

	if (!i)
		return;
	if (!i->next) {
		gtk_imhtml_clear(GTK_IMHTML(away_text));
	}
	a = gtk_object_get_user_data(GTK_OBJECT(i->data));
	rem_away_mess(NULL, a);
}
#endif

GtkWidget *away_message_page() {
	GtkWidget *ret;
	GtkWidget *frame;
	GtkWidget *vbox, *hbox, *bbox;
	GtkWidget *button, *image, *label;
	GtkWidget *sw;

#if GTK_CHECK_VERSION(1,3,0)
	GtkTreeIter iter;
	GtkWidget *event_view;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTreePath *path;
#endif
	GSList *awy = away_messages;
	struct away_message *a;
	GtkWidget *sw2;
	
	ret = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 6);

	frame = gtk_frame_new ("Title");
	gtk_box_pack_start (GTK_BOX (ret), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	sw = gtk_scrolled_window_new(NULL,NULL);
	away_text = gtk_imhtml_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
      
#if GTK_CHECK_VERSION(1,3,0)
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
	gtk_container_add(GTK_CONTAINER(sw), event_view);
#else
	prefs_away_list = gtk_list_new();
	while (awy) {
		GtkWidget *ambox = gtk_hbox_new(FALSE, 5);
		GtkWidget *list_item =gtk_list_item_new();
		GtkWidget *label;
		a = (struct away_message *)awy->data;
		gtk_container_add(GTK_CONTAINER(prefs_away_list), list_item);
		gtk_signal_connect(GTK_OBJECT(list_item), "select", GTK_SIGNAL_FUNC(away_message_sel),
				   a);
		gtk_object_set_user_data(GTK_OBJECT(list_item), a);

		gtk_widget_show(list_item);

		ambox = gtk_hbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(list_item), ambox);
		gtk_widget_show(ambox);

		label = gtk_label_new(a->name);
		gtk_box_pack_start(GTK_BOX(ambox), label, FALSE, FALSE, 5);
		gtk_widget_show(label);

		awy = awy->next;
		
	}
	gtk_widget_show(prefs_away_list);
	gtk_container_add(GTK_CONTAINER(sw), prefs_away_list);
#endif

	gtk_widget_show (vbox);
	gtk_widget_show (sw);
	

	frame = gtk_frame_new ("Message");
	gtk_box_pack_start (GTK_BOX (ret), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	
	sw2 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw2),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), sw2, TRUE, TRUE, 0);
	gtk_widget_show(sw2);
	
	gtk_container_add(GTK_CONTAINER(sw2), away_text);
	GTK_LAYOUT(away_text)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(away_text)->vadjustment->step_increment = 10.0;
	gaim_setup_imhtml(away_text);
	gtk_widget_show(away_text);
	gtk_widget_show (vbox);
	gtk_widget_show (ret);
	gtk_imhtml_set_defaults(GTK_IMHTML(away_text), NULL, NULL, NULL);
#if GTK_CHECK_VERSION(1,3,0)
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (event_view));
	path = gtk_tree_path_new_first();
	g_signal_connect (G_OBJECT (sel), "changed",
			  G_CALLBACK (away_message_sel),
			  NULL);
#endif	
	hbox = gtk_hbox_new(TRUE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION (1,3,0)
	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
#else
	button = picture_button(prefs, _("Add"), gnome_add_xpm);
#endif
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_show(button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), NULL);
	
#if GTK_CHECK_VERSION (1,3,0)
	button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(remove_away_message), event_view);
#else
	button = picture_button(prefs, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(remove_away_message), prefs_away_list);
#endif	
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_show(button);
	
#if GTK_CHECK_VERSION (1,3,0)
	button = pixbuf_button(_("_Edit"), "edit.png");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), event_view);
#else
	button = picture_button(prefs, _("Edit"), save_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_away_mess), button);
#endif	
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_show(button);
	
	return ret;
}
#if GTK_CHECK_VERSION (1,3,0)
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
#else
GtkCTreeNode *prefs_notebook_add_page(char *text, 
				     GdkPixmap *pixmap, 
				     GtkWidget *page, 
				     GtkCTreeNode **iter,
				     GtkCTreeNode **parent, 
				     int ind) {

	GtkCTreeNode *itern;
	char *texts[1];

	texts[0] = text;

	*iter = gtk_ctree_insert_node (GTK_CTREE(prefstree), parent ? *parent : NULL, NULL, &text,
			       0, NULL, NULL, NULL, NULL, 0, 1);
	gtk_ctree_node_set_row_data(GTK_CTREE(prefstree), GTK_CTREE_NODE(*iter), (void *)ind);
	if (pixmap)
		gdk_pixmap_unref(pixmap);
	
	debug_printf("%s\n", texts[0]);

	gtk_notebook_append_page(GTK_NOTEBOOK(prefsnotebook), page, gtk_label_new(text));
	return iter;
}
#endif

void prefs_notebook_init() {
	int a = 0;
#if GTK_CHECK_VERSION(1,3,0)	
	GtkTreeIter p, c;
#else
	GtkCTreeNode *p = NULL;
	GtkCTreeNode *c = NULL;
#endif	
	prefs_notebook_add_page(_("Interface"), NULL, interface_page(), &p, NULL, a++);
	prefs_notebook_add_page(_("Fonts"), NULL, font_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Message Text"), NULL, messages_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Shortcuts"), NULL, hotkeys_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Buddy List"), NULL, list_page(), &c, &p, a++);
	prefs_notebook_add_page(_("IM Window"), NULL, im_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Chat Window"), NULL, chat_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Tabs"), NULL, tab_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Proxy"), NULL, proxy_page(), &p, NULL, a++);
	prefs_notebook_add_page(_("Browser"), NULL, browser_page(), &p, NULL, a++);

	prefs_notebook_add_page(_("Logging"), NULL, logging_page(), &p, NULL, a++);
	prefs_notebook_add_page(_("Sounds"), NULL, sound_page(), &p, NULL, a++);
	prefs_notebook_add_page(_("Sound Events"), NULL, sound_events_page(), &c, &p, a++);
	prefs_notebook_add_page(_("Away / Idle"), NULL, away_page(), &p, NULL, a++);
	prefs_notebook_add_page(_("Away Messages"), NULL, away_message_page(), &c, &p, a++);
}

void show_prefs() 
{
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *frame;
#if GTK_CHECK_VERSION (1,3,0)
	GtkWidget *tree_v;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *sel;
#endif	
	GtkWidget *notebook;
	GtkWidget *sep;
	GtkWidget *button;
	
	int r;

	if (prefs) {
		gtk_widget_show(prefs);
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
	g_snprintf(sound_cmd_new, sizeof(sound_cmd_new), "%s", sound_cmd);
	g_snprintf(web_command_new, sizeof(web_command_new), "%s", web_command);
	g_snprintf(fontface_new, sizeof(fontface_new), fontface);
#if !GTK_CHECK_VERSION(1,3,0)
	g_snprintf(fontxfld_new, sizeof(fontxfld_new), fontxfld);
#endif
	memcpy(&conv_size_new, &conv_size, sizeof(struct window_size));
	memcpy(&buddy_chat_size_new, &buddy_chat_size, sizeof(struct window_size));	
	memcpy(&fgcolor_new, &fgcolor, sizeof(GdkColor));
	memcpy(&bgcolor_new, &bgcolor, sizeof(GdkColor));
		
	/* Create the window */
	prefs = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(prefs), "preferences", "Gaim");
	gtk_widget_realize(prefs);
	aol_icon(prefs->window);
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
#if GTK_CHECK_VERSION(1,3,0)
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

#else
	 prefstree = gtk_ctree_new(1,0);
	 gtk_ctree_set_line_style(prefstree, GTK_CTREE_LINES_NONE);
	 gtk_ctree_set_expander_style(GTK_CTREE(prefstree), GTK_CTREE_EXPANDER_TRIANGLE);
	 gtk_container_add(GTK_CONTAINER (frame), prefstree);
	 gtk_widget_set_usize(prefstree, 150, -1);
	 gtk_widget_show(prefstree);
#endif /* GTK_CHECK_VERSION */

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

#if GTK_CHECK_VERSION(1,3,0)
	 sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_v));
	 g_signal_connect (G_OBJECT (sel), "changed",
	 		   G_CALLBACK (pref_nb_select),
	 		   notebook);
#else
	 gtk_signal_connect(GTK_OBJECT(prefstree), "tree-select-row", GTK_SIGNAL_FUNC(pref_nb_select), notebook);
#endif
	 gtk_widget_show(notebook);	 
	 sep = gtk_hseparator_new();
	 gtk_widget_show(sep);
	 gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	 
	 /* The buttons to press! */
	 hbox = gtk_hbox_new (FALSE, 6);
	 gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	 gtk_container_add (GTK_CONTAINER(vbox), hbox);
	 gtk_widget_show (hbox); 
#if GTK_CHECK_VERSION(1,3,0)
	 button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
#else
	 button =  picture_button(prefs, _("Close"), cancel_xpm);
#endif
	 gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), prefs);
	 gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	 if (misc_options & OPT_MISC_COOL_LOOK)
		 gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	 gtk_widget_show(button);
#if GTK_CHECK_VERSION(1,3,0) 
	 button = gtk_button_new_from_stock (GTK_STOCK_APPLY);
#else
	 button  = picture_button(prefs, _("Apply"), ok_xpm);
#endif
	 gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(apply_cb), prefs); 
	 gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	 if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	 gtk_widget_show(button);
	 
#if GTK_CHECK_VERSION(1,3,0)
	 button = gtk_button_new_from_stock (GTK_STOCK_OK);
#else
	 button  = picture_button(prefs, _("OK"), join_xpm);
#endif
	 gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(ok_cb), prefs); 
	 gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	 if (misc_options & OPT_MISC_COOL_LOOK)
		 gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	 gtk_widget_show(button);

	 prefs_notebook_init(); 
#if GTK_CHECK_VERSION(1,3,0)
	 gtk_tree_view_expand_all (GTK_TREE_VIEW(tree_v));
#endif	 
	 gtk_widget_show(prefs);
}

static gint debug_delete(GtkWidget *w, GdkEvent *event, void *dummy)
{
	if (debugbutton)
		gtk_button_clicked(GTK_BUTTON(debugbutton));
	if (misc_options & OPT_MISC_DEBUG) {
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
	aol_icon(dw->window->window);

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
	misc_options ^= option;

	if (option == OPT_MISC_DEBUG)
		show_debug();

	if (option == OPT_MISC_BUDDY_TICKER)
		BuddyTickerShow();

	save_prefs();
}


/* Functions for each _options variable that sees what's changed, and makes
 * effective those changes that take place immediately (UI stuff) */
static void set_misc_options()
{
	
/* 	int option = misc_options ^ misc_options_new; */
	misc_options = misc_options_new;
	
}

static void set_logging_options()
{
	int option = logging_options ^ logging_options_new;
	logging_options = logging_options_new;
	
	if (option & OPT_LOG_ALL)
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
	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), (*options & option));
	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(button), options);

	/* So that the ticker and debug window happen immediately
	 * I don't think they should be "preferences," anyway. */
	if (options == &misc_options && (option == OPT_MISC_DEBUG || option == OPT_MISC_BUDDY_TICKER))
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
	gdouble color[3];
	if ((int)d == 1) {
#if GTK_CHECK_VERSION(1,3,0)
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION
						      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel), 
						      &fgcolor_new);
#else
		gtk_color_selection_get_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel), color);

		fgcolor_new.red = ((guint16)(color[0] * 65535)) >> 8;
		fgcolor_new.green = ((guint16)(color[1] * 65535)) >> 8;
		fgcolor_new.blue = ((guint16)(color[2] * 65535)) >> 8;
#endif
		destroy_colorsel(NULL, (void *)1);
		update_color(NULL, pref_fg_picture);
	} else {
#if GTK_CHECK_VERSION(1,3,0)
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION
						      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel), 
						      &bgcolor_new);
#else
		gtk_color_selection_get_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel), color);
		
		bgcolor_new.red = ((guint16)(color[0] * 65535)) >> 8;
		bgcolor_new.green = ((guint16)(color[1] * 65535)) >> 8;
		bgcolor_new.blue = ((guint16)(color[2] * 65535)) >> 8;
#endif
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
GtkWidget *gaim_labeled_spin_button(GtkWidget *box, const gchar *title, int *val, int min, int max)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *spin;
	GtkObject *adjust;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(title);
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
	return spin;
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
	} else if (option == &sound_options_new) {
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

	label = gtk_label_new(title);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	va_start(menuitems, clear);
	text = va_arg(menuitems, gchar *);

	if (!text)
		return NULL;
	
	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();

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
	return dropdown;
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
#if !GTK_CHECK_VERSION(1,3,0)
	int j = 0, k = 0;
#endif
	char *fontname;

	fontname = g_strdup(gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontseld)));
	destroy_fontsel(0, 0);

#if !GTK_CHECK_VERSION(1,3,0)
	for (i = 0; i < strlen(fontname); i++) {
		if (fontname[i] == '-') {
			if (++j > 2)
				break;
		} else if (j == 2)
			fontface_new[k++] = fontname[i];
	}
	fontface_new[k] = '\0';
	g_snprintf(fontxfld_new, sizeof(fontxfld_new), "%s", fontname);
#else
	while(fontname[i] && !isdigit(fontname[i]) && i < sizeof(fontface_new)) { 
		fontface_new[i] = fontname[i];
		i++;
	}
	
#endif	
	g_free(fontname);
}

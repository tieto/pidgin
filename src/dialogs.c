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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <errno.h>
#include <math.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "gtkimhtml.h"
#include "prpl.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/save.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/add.xpm"
#include "pixmaps/close.xpm"
#include "pixmaps/gnome_add.xpm"
#include "pixmaps/gnome_remove.xpm"

#include "pixmaps/protocols/oscar/aol_icon.xpm"
#include "pixmaps/protocols/oscar/free_icon.xpm"
#include "pixmaps/protocols/oscar/dt_icon.xpm"
#include "pixmaps/protocols/oscar/admin_icon.xpm"
#include "pixmaps/protocols/oscar/ab.xpm"
#include "pixmaps/protocols/oscar/wireless_icon.xpm"
#include "pixmaps/protocols/oscar/gnomeicu-online.xpm"

#define PATHSIZE 1024

int smiley_array[FACE_TOTAL];
char *current_smiley;
GdkColor bgcolor;
GdkColor fgcolor;

static GtkWidget *imdialog = NULL;	/*I only want ONE of these :) */
static GList *dialogwindows = NULL;
static GtkWidget *importdialog;
static struct gaim_connection *importgc;
static GtkWidget *icondlg;
static GtkWidget *aliasdlg = NULL;
static GtkWidget *aliasentry = NULL;
static GtkWidget *aliasname = NULL;
static GtkWidget *rename_dialog = NULL;
static GtkWidget *rename_bud_dialog = NULL;


struct confirm_del {
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *ok;
	GtkWidget *cancel;
	char name[1024];
	struct gaim_connection *gc;
};

struct create_away {
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *text;
	struct away_message *mess;
};

struct warning {
	GtkWidget *window;
	GtkWidget *anon;
	char *who;
	struct gaim_connection *gc;
};

struct addbuddy {
	GtkWidget *window;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *entry_for_alias;
	GtkWidget *account;
	struct gaim_connection *gc;
};

struct addperm {
	GtkWidget *window;
	GtkWidget *entry;
	struct gaim_connection *gc;
	gboolean permit;
};

struct addbp {
	GtkWidget *window;
	GtkWidget *nameentry;
	GtkWidget *messentry;
	GtkWidget *commentry;
	GtkWidget *command;
	GtkWidget *sendim;
	GtkWidget *openwindow;
	GtkWidget *popupnotify;
	GtkWidget *p_signon;
	GtkWidget *p_unaway;
	GtkWidget *p_unidle;
	GtkWidget *p_typing;
	GtkWidget *save;
	GtkWidget *menu;
	GtkWidget *sound;
	GtkWidget *soundentry;

	struct aim_user *user;
	struct buddy_pounce *buddy_pounce;
};

struct findbyemail {
	GtkWidget *window;
	GtkWidget *emailentry;
	struct gaim_connection *gc;
};

struct findbyinfo {
	struct gaim_connection *gc;
	GtkWidget *window;
	GtkWidget *firstentry;
	GtkWidget *middleentry;
	GtkWidget *lastentry;
	GtkWidget *maidenentry;
	GtkWidget *cityentry;
	GtkWidget *stateentry;
	GtkWidget *countryentry;
};

struct info_dlg {
	struct gaim_connection *gc;
	char *who;
	GtkWidget *window;
	GtkWidget *text;
};

struct getuserinfo {
	GtkWidget *window;
	GtkWidget *entry;
	GtkWidget *account;
	struct gaim_connection *gc; 
};

static GSList *info_dlgs = NULL;

static struct info_dlg *find_info_dlg(struct gaim_connection *gc, char *who)
{
	GSList *i = info_dlgs;
	while (i) {
		struct info_dlg *d = i->data;
		i = i->next;
		if (d->gc != gc)
			continue;
		if (d->who == NULL)
			continue;
		if (!who)
			continue;
		if (!g_strcasecmp(normalize(who), d->who))
			return d;
	}
	return NULL;
}

struct set_info_dlg {
	GtkWidget *window;
	GtkWidget *menu;
	struct aim_user *user;
	GtkWidget *text;
	GtkWidget *save;
	GtkWidget *cancel;
};

struct set_icon_dlg {
	GtkWidget *window;
	struct aim_user *user;
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *entry;
};

struct set_dir_dlg {
	struct gaim_connection *gc;
	GtkWidget *window;
	GtkWidget *first;
	GtkWidget *middle;
	GtkWidget *last;
	GtkWidget *maiden;
	GtkWidget *city;
	GtkWidget *state;
	GtkWidget *country;
	GtkWidget *web;
	GtkWidget *cancel;
	GtkWidget *save;
};

struct linkdlg {
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *window;
	GtkWidget *url;
	GtkWidget *text;
	GtkWidget *toggle;
	GtkWidget *entry;
};

struct passwddlg {
	GtkWidget *window;
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *original;
	GtkWidget *new1;
	GtkWidget *new2;
	struct gaim_connection *gc;
};

struct view_log {
	long offset;
	int options;
	char *name;
	GtkWidget *bbox;
	GtkWidget *window;
	GtkWidget *layout;
};

/*------------------------------------------------------------------------*/
/*  Destroys                                                              */
/*------------------------------------------------------------------------*/

static gint delete_event_dialog(GtkWidget *w, GdkEventAny *e, struct conversation *c)
{
	gchar *object_data;
	object_data = gtk_object_get_user_data(GTK_OBJECT(w));

	if (GTK_IS_COLOR_SELECTION_DIALOG(w)) {
		set_state_lock(1);
		if (w == c->fg_color_dialog) {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->fgcolorbtn), FALSE);
			c->fg_color_dialog = NULL;
		} else {
			gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->bgcolorbtn), FALSE);
			c->bg_color_dialog = NULL;
		}
		set_state_lock(0);
	} else if (GTK_IS_FONT_SELECTION_DIALOG(w)) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->font), FALSE);
		set_state_lock(0);
		c->font_dialog = NULL;
	} else if (!g_strcasecmp(object_data, "smiley dialog")) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->smiley), FALSE);
		set_state_lock(0);
		c->smiley_dialog = NULL;
	} else if (!g_strcasecmp(object_data, "log dialog")) {
		set_state_lock(1);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(c->log_button), FALSE);
		set_state_lock(0);
		c->log_dialog = NULL;
	}

	dialogwindows = g_list_remove(dialogwindows, w);
	gtk_widget_destroy(w);

	return FALSE;
}

static void destroy_dialog(GtkWidget *w, GtkWidget *w2)
{
	GtkWidget *dest;

	if (!GTK_IS_WIDGET(w2))
		dest = w;
	else
		dest = w2;

	if (dest == imdialog)
		imdialog = NULL;

	if (dest == importdialog) {
		importdialog = NULL;
		importgc = NULL;
	}

	if (dest == icondlg)
		icondlg = NULL;

	if (dest == aliasdlg) {
		aliasdlg = NULL;
		aliasentry = NULL;
		aliasname = NULL;
	}

	if (dest == rename_dialog)
		rename_dialog = NULL;

	if (dest == rename_bud_dialog)
		rename_bud_dialog = NULL;

	dialogwindows = g_list_remove(dialogwindows, dest);
	gtk_widget_destroy(dest);

}


void destroy_all_dialogs()
{
	while (dialogwindows)
		destroy_dialog(NULL, dialogwindows->data);

	if (awaymessage)
		do_im_back(NULL, NULL);

	if (imdialog) {
		destroy_dialog(NULL, imdialog);
		imdialog = NULL;
	}

	if (importdialog) {
		destroy_dialog(NULL, importdialog);
		importdialog = NULL;
	}

	if (icondlg) {
		destroy_dialog(NULL, icondlg);
		icondlg = NULL;
	}
}

static void do_warn(GtkWidget *widget, gint resp, struct warning *w)
{
	if (resp == GTK_RESPONSE_OK)
		serv_warn(w->gc, w->who, (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->anon))) ? 1 : 0);

	destroy_dialog(NULL, w->window);
	g_free(w);
}

void show_warn_dialog(struct gaim_connection *gc, char *who)
{
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_warning.png", NULL);
	char *labeltext;
	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *img = gtk_image_new_from_file(filename);
	struct conversation *c = find_conversation(who);

	struct warning *w = g_new0(struct warning, 1);
	w->who = who;
	w->gc = gc;

	g_free(filename);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	w->window = gtk_dialog_new_with_buttons("", GTK_WINDOW(c->window), GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, "_Warn", GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(w->window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(w->window), "response", G_CALLBACK(do_warn), w);

	gtk_container_set_border_width (GTK_CONTAINER(w->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(w->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(w->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(w->window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(w->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);
	labeltext = g_strdup_printf(_("<span weight=\"bold\" size=\"larger\">Warn %s?</span>\n\n"
				      "This will increase %s's warning level and he or she will be subject to harsher rate limiting.\n"), who, who);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	g_free(labeltext);

	w->anon = gtk_check_button_new_with_mnemonic(_("Warn _anonymously?"));
	gtk_box_pack_start(GTK_BOX(vbox), w->anon, FALSE, FALSE, 0);
	
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	img = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	labeltext = _("<b>Anonymous warnings are less severe.</b>");
	/* labeltext = _("Anonymous warnings are less severe."); */
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	dialogwindows = g_list_prepend(dialogwindows, w->window);
	gtk_widget_show_all(w->window);
}

void do_remove_buddy(struct buddy *b)
{
	struct group *g = find_group_by_buddy(b->gc, b->name);
	struct gaim_connection *gc = b->gc;
	struct conversation *cv;

	if (!b)
		return;

	debug_printf(_("Removing '%s' from buddy list.\n"), b->name);
	serv_remove_buddy(b->gc, b->name, g->name);
	remove_buddy(gc, g, b);
	do_export(gc);

	cv = find_conversation(b->name);

	if (cv)
		update_convo_add_button(cv);

}

void show_confirm_del(struct gaim_connection *gc, gchar *name)
{
	struct buddy *bd = find_buddy(gc, name);
	char *text;
	if (!bd)
		return;

	text = g_strdup_printf("You are about to remove %s from your buddy list.  Do you want to continue?", name);
	do_ask_dialog("Remove Buddy", text, bd, "Remove Buddy", do_remove_buddy, "Cancel", NULL);
	g_free(text);
}



/*------------------------------------------------------------------------*/
/*  The dialog for getting an error                                       */
/*------------------------------------------------------------------------*/

GtkWidget *do_error_dialog(const char *primary, const char *secondary, int type)
{
	GtkWidget *d;
	GtkWidget *label;
	GtkWidget *img = NULL;
	GtkWidget *hbox;
	char labeltext[1024 * 2];
 	char *filename = NULL;

	/* These are the GTK stock dialog icons with our little Gaim logo on top.
	 * Inspired by the GIMP. */
	switch (type){
	case GAIM_LOGO:
		 filename = g_build_filename(DATADIR, "pixmaps", "gaim", "gaim.png", NULL);
		 break;
 	case GAIM_INFO:
	 	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_info.png", NULL);
		break; 
	case GAIM_WARNING:
	 	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_warning.png", NULL);
	  	break;
	case GAIM_ERROR:
	 	filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_error.png", NULL);
	 	break;
		/*	 case GAIM_QUESTION:
			 filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_question.png", NULL);
			 break;
		*/
	}
	if (filename) {
		img = gtk_image_new_from_file(filename);
		g_free(filename);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	}
	d = gtk_dialog_new_with_buttons("", NULL, GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	g_signal_connect(d, "response", G_CALLBACK(gtk_widget_destroy), NULL);
	
	gtk_container_set_border_width (GTK_CONTAINER(d), 6);
	gtk_window_set_resizable(GTK_WINDOW(d), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(d), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(d)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(d)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(d)->vbox), hbox);
	if (filename) {
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	}
	gtk_widget_show(img);

	g_snprintf(labeltext, sizeof(labeltext), "<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s", primary, secondary ? secondary : "");
	
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), labeltext);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_widget_show(hbox);

	gtk_widget_show(d);
	return d;
}



static void do_im(GtkWidget *widget, int resp, struct getuserinfo *info)
{
	char *who;
	struct conversation *c;

	if (resp == GTK_RESPONSE_OK) {
		who = g_strdup(gtk_entry_get_text(GTK_ENTRY(info->entry)));
		
		if (!g_strcasecmp(who, "")) {
			g_free(who);
			return;
		}
		
		c = find_conversation(who);
		
		if (c == NULL) {
			c = new_conversation(who);
		} else {
			gdk_window_raise(c->window->window);
			}
		if (info->gc)
			set_convo_gc(c, info->gc);
		
		g_free(who);
	}
	
	destroy_dialog(NULL, imdialog);
	imdialog = NULL;
}

static void do_info(GtkWidget *widget, int resp, struct getuserinfo *info)
{
	char *who;

	if (resp == GTK_RESPONSE_OK) {
		who = g_strdup(normalize(gtk_entry_get_text(GTK_ENTRY(info->entry))));

		if (!g_strcasecmp(who, "")) {
			g_free(who);
			return;
		}
	
		/* what do we want to do about this case? */
		if (info->gc)
			serv_get_info(info->gc, who);
		g_free(who);
	}
	gtk_widget_destroy(GTK_WIDGET(widget));
}

void show_ee_dialog(int ee)
{
	GtkWidget *window;
	GtkWidget *hbox;
	GtkWidget *label;
	
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_cool.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);
	
	label = gtk_label_new(NULL);
	if (ee == 0)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"purple\">Amazing!  Simply Amazing!</span>");
	else if (ee == 1)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"#1f6bad\">Pimpin\' Penguin Style! *Waddle Waddle*</span>");
	else if (ee == 2)
		gtk_label_set_markup(GTK_LABEL(label), 
				      "<span weight=\"bold\" size=\"large\" foreground=\"blue\">You should be me.  I'm so cute!</span>");
	else if (ee == 3)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"orange\">Now that's what I like!</span>");
	else if (ee == 4)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"brown\">Ahh, and excellent choice!</span>");
	else  if (ee == 5)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"#009900\">Everytime you click my name, an angel gets its wings.</span>");
	else if (ee == 6)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"red\">This sunflower seed taste like pizza.</span>");
	else if (ee == 7)
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"#6364B1\">Hey!  I was in that tumbleweed!</span>");
	else
		gtk_label_set_markup(GTK_LABEL(label), 
				     "<span weight=\"bold\" size=\"large\" foreground=\"gray\">I'm not anything.</span>");
	
	window = gtk_dialog_new_with_buttons("", GTK_WINDOW(blist), GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(gtk_widget_destroy), NULL);
	
	gtk_container_set_border_width (GTK_CONTAINER(window), 6);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), 6);

	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	
	gtk_widget_show_all(window);
}

void show_info_select_account(GtkObject *w, struct gaim_connection *gc)
{
	struct getuserinfo *info = gtk_object_get_user_data(w);
	info->gc = gc;
}

void show_im_dialog()
{
	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	GtkWidget *table, *menu, *opt;
	GSList *g = connections;
	struct gaim_connection *c;
	char buf[256];
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_question.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);
	struct getuserinfo *info = NULL;

	if (!imdialog) {
		info = g_new0(struct getuserinfo, 1);
		info->gc = connections->data;
		imdialog = gtk_dialog_new_with_buttons("", NULL, GTK_DIALOG_MODAL, 
						       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG(imdialog), GTK_RESPONSE_OK);
		gtk_container_set_border_width (GTK_CONTAINER(imdialog), 6);
		gtk_window_set_resizable(GTK_WINDOW(imdialog), FALSE);
		gtk_dialog_set_has_separator(GTK_DIALOG(imdialog), FALSE);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(imdialog)->vbox), 12);
		gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(imdialog)->vbox), 6);
		
		hbox = gtk_hbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(imdialog)->vbox), hbox);
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(hbox), vbox);
		
		label = gtk_label_new(_("Please enter the screenname of the person you would like to IM.\n"));
		gtk_widget_set_size_request(GTK_WIDGET(label), 350, -1);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		
		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);
		
		table = gtk_table_new(2, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(table), 6);
		gtk_table_set_col_spacings(GTK_TABLE(table), 6);
		gtk_container_set_border_width(GTK_CONTAINER(table), 12);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	
		label = gtk_label_new(NULL);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Screenname:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

		info->entry = gtk_entry_new();
		gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
		gtk_entry_set_activates_default (GTK_ENTRY(info->entry), TRUE);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->entry));

		if (connections->next) {

			label = gtk_label_new(NULL);
			gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
			gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
			
			info->account = gtk_option_menu_new();
			gtk_table_attach_defaults(GTK_TABLE(table), info->account, 1, 2, 1, 2);
			gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->account));
			
			menu = gtk_menu_new();
			
			while (g) {
				c = (struct gaim_connection *)g->data;
				if (!c->prpl->send_im) {
					g = g->next;
					continue;
				}
				g_snprintf(buf, sizeof(buf), "%s (%s)", c->username, c->prpl->name);
				opt = gtk_menu_item_new_with_label(buf);
				gtk_object_set_user_data(GTK_OBJECT(opt), info);
				
				gtk_signal_connect(GTK_OBJECT(opt), "activate",
						   GTK_SIGNAL_FUNC(show_info_select_account), c);
				
				gtk_menu_append(GTK_MENU(menu), opt);
				g = g->next;
			}
			
			gtk_option_menu_set_menu(GTK_OPTION_MENU(info->account), menu);
		}
		
		g_signal_connect(G_OBJECT(imdialog), "response", G_CALLBACK(do_im), info);
		
		dialogwindows = g_list_prepend(dialogwindows, imdialog->window);
	}
	
	gtk_widget_show_all(imdialog);
	if (info)
		gtk_widget_grab_focus(GTK_WIDGET(info->entry));
}

void show_info_dialog()
{
	GtkWidget *window, *hbox, *vbox;
	GtkWidget *label;
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_question.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);
	GtkWidget *table, *menu, *opt;
	GSList *g = connections;
	struct gaim_connection *c;
	struct getuserinfo *info = g_new0(struct getuserinfo, 1);
	char buf[256];

	info->gc = connections->data;

	window = gtk_dialog_new_with_buttons("", blist ? GTK_WINDOW(blist) : NULL, GTK_DIALOG_MODAL, 
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(window), GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER(window), 6);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(window)->vbox), 6);
		
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);
		
	label = gtk_label_new(_("Please enter the screenname of the person whose info you would like to view.\n"));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	
	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_container_set_border_width(GTK_CONTAINER(table), 12);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	
	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Screenname:"));
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

	info->entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), info->entry, 1, 2, 0, 1);
	gtk_entry_set_activates_default (GTK_ENTRY(info->entry), TRUE);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->entry));
	
	if (connections->next) {

		label = gtk_label_new(NULL);
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
		gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Account:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

		info->account = gtk_option_menu_new();
		gtk_table_attach_defaults(GTK_TABLE(table), info->account, 1, 2, 1, 2);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(info->account));
		
		menu = gtk_menu_new();

		while (g) {
			c = (struct gaim_connection *)g->data;
			if (!c->prpl->get_info) {
				g = g->next;
				continue;
			}
			g_snprintf(buf, sizeof(buf), "%s (%s)", c->username, c->prpl->name);
			opt = gtk_menu_item_new_with_label(buf);
			gtk_object_set_user_data(GTK_OBJECT(opt), info);

			gtk_signal_connect(GTK_OBJECT(opt), "activate",
					   GTK_SIGNAL_FUNC(show_info_select_account), c);

			gtk_menu_append(GTK_MENU(menu), opt);
			g = g->next;
		}
		
		gtk_option_menu_set_menu(GTK_OPTION_MENU(info->account), menu);
	}

	g_signal_connect(G_OBJECT(window), "response", G_CALLBACK(do_info), info);
	
	
	gtk_widget_show_all(window);
	if (info->entry)
		gtk_widget_grab_focus(GTK_WIDGET(info->entry));
}


/*------------------------------------------------------------------------*/
/*  The dialog for adding buddies                                         */
/*------------------------------------------------------------------------*/

extern void add_callback(GtkWidget *, struct conversation *);

void do_add_buddy(GtkWidget *w, struct addbuddy *a)
{
	const char *grp, *who, *whoalias;
	struct conversation *c;

	who = gtk_entry_get_text(GTK_ENTRY(a->entry));
	grp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry));
	whoalias = gtk_entry_get_text(GTK_ENTRY(a->entry_for_alias));

	c = find_conversation(who);

	add_buddy(a->gc, grp, who, whoalias);
	serv_add_buddy(a->gc, who);

	if (c != NULL) {
		update_buttons_by_protocol(c);
	}

	do_export(a->gc);

	destroy_dialog(NULL, a->window);
}

void do_add_group(GtkWidget *w, int resp, struct addbuddy *a)
{
	const char *grp;
	
	if (resp == GTK_RESPONSE_OK) {
		grp = gtk_entry_get_text(GTK_ENTRY(a->entry));

		if (!a->gc)
			a->gc = connections->data;

		add_group(a->gc, grp);
		do_export(a->gc);
	}

	destroy_dialog(NULL, a->window);
}


static GList *groups_tree(struct gaim_connection *gc)
{
	GList *tmp = NULL;
	char *tmp2;
	struct group *g;
	GSList *grp = gc->groups;

	if (!grp) {
		tmp2 = g_strdup(_("Buddies"));
		tmp = g_list_append(tmp, tmp2);
	} else {
		while (grp) {
			g = (struct group *)grp->data;
			tmp2 = g->name;
			tmp = g_list_append(tmp, tmp2);
			grp = g_slist_next(grp);
		}
	}
	return tmp;
}

static void free_dialog(GtkWidget *w, struct addbuddy *a)
{
	g_free(a);
}


void show_add_group(struct gaim_connection *gc)
{

	GtkWidget *hbox, *vbox;
	GtkWidget *label;
	char *filename = g_build_filename(DATADIR, "pixmaps", "gaim", "dialogs", "gaim_question.png", NULL);
	GtkWidget *img = gtk_image_new_from_file(filename);

	struct addbuddy *a = g_new0(struct addbuddy, 1);
	a->gc = gc;

	a->window =  gtk_dialog_new_with_buttons("", GTK_WINDOW(blist), GTK_DIALOG_MODAL, 
						 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD, GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG(a->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER(a->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(a->window), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(a->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(a->window)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), 6);
		
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(a->window)->vbox), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(img), 0, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);
		
	label = gtk_label_new(_("Please enter the name of the group to be added.\n"));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
		
	label = gtk_label_new(NULL);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), _("_Group:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		
	a->entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY(a->entry), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), a->entry, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(a->entry));

	g_signal_connect(G_OBJECT(a->window), "response", G_CALLBACK(do_add_group), a);

	gtk_widget_show_all(a->window);
	gtk_widget_grab_focus(GTK_WIDGET(a->entry));
}

static void addbuddy_select_account(GtkObject *w, struct gaim_connection *gc)
{
	struct addbuddy *b = gtk_object_get_user_data(w);

	/* Save our account */
	b->gc = gc;

	/* We also want to update our group list */	
	gtk_combo_set_popdown_strings(GTK_COMBO(b->combo), groups_tree(gc));
}

static void create_online_user_names(struct addbuddy *b)
{
	char buf[2048]; /* Never hurts to be safe ;-) */
	GSList *g = connections;
	struct gaim_connection *c;
	GtkWidget *menu, *opt;
	int count = 0;
	int place = 0;

	menu = gtk_menu_new();

	while (g) {
		c = (struct gaim_connection *)g->data;
		g_snprintf(buf, sizeof(buf), "%s (%s)", 
				c->username, c->prpl->name);
		opt = gtk_menu_item_new_with_label(buf);
		gtk_object_set_user_data(GTK_OBJECT(opt), b);
		gtk_signal_connect(GTK_OBJECT(opt), "activate",
				GTK_SIGNAL_FUNC(addbuddy_select_account),
				c);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);

		/* Now check to see if it's our current menu */
		if (c == b->gc) {
			place = count;
			gtk_menu_item_activate(GTK_MENU_ITEM(opt));
			gtk_option_menu_set_history(GTK_OPTION_MENU(b->account), count);

			/* Do the cha cha cha */
		}

		count++;

		g = g->next;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(b->account));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(b->account), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(b->account), place);

	gtk_widget_show(b->account);
	gtk_widget_show(b->account->parent);

}

void show_add_buddy(struct gaim_connection *gc, char *buddy, char *group, char *alias)
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *bbox;
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;

	struct addbuddy *a = g_new0(struct addbuddy, 1);
	a->gc = gc ? gc : connections->data;

	GAIM_DIALOG(a->window);
	gtk_window_set_role(GTK_WINDOW(a->window), "add_buddy");
	gtk_window_set_policy(GTK_WINDOW(a->window), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(a->window), _("Gaim - Add Buddy"));

	gtk_widget_realize(a->window);
	gtk_signal_connect(GTK_OBJECT(a->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), a->window);
	gtk_signal_connect(GTK_OBJECT(a->window), "destroy", GTK_SIGNAL_FUNC(free_dialog), a);
	dialogwindows = g_list_prepend(dialogwindows, a->window);

	mainbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
	gtk_container_add(GTK_CONTAINER(a->window), mainbox);

	frame = gtk_frame_new(_("Add Buddy"));
	gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_container_add(GTK_CONTAINER(frame), table);

	label = gtk_label_new(_("Contact"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);

	a->entry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), a->entry, 1, 2, 0, 1);
	gtk_widget_grab_focus(a->entry);
	if (buddy != NULL)
		gtk_entry_set_text(GTK_ENTRY(a->entry), buddy);
	gtk_signal_connect(GTK_OBJECT(a->entry), "activate", GTK_SIGNAL_FUNC(do_add_buddy), a);

	label = gtk_label_new(_("Alias"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);

	a->entry_for_alias = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(table), a->entry_for_alias, 1, 2, 1, 2);
	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(a->entry_for_alias), alias);

	label = gtk_label_new(_("Group"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);

	a->combo = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(a->combo), groups_tree(a->gc));
	gtk_table_attach_defaults(GTK_TABLE(table), a->combo, 1, 2, 2, 3);

	/* Set up stuff for the account box */
	label = gtk_label_new(_("Add To"));
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

	a->account = gtk_option_menu_new();
	gtk_table_attach_defaults(GTK_TABLE(table), a->account, 1, 2, 3, 4);

	create_online_user_names(a);
	
	/* End of account box */

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(mainbox), bbox, TRUE, TRUE, 0);

	cancel = picture_button(a->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), a->window);

	add = picture_button(a->window, _("Add"), add_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), add, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(do_add_buddy), a);

	gtk_widget_show_all(a->window);
	if (group != NULL) 
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry), group);
}


/*------------------------------------------------------------------------*
 *  Privacy Settings                                                      *
 *------------------------------------------------------------------------*/
static GtkWidget *deny_type = NULL;
static GtkWidget *deny_conn_hbox = NULL;
static GtkWidget *deny_opt_menu = NULL;
static struct gaim_connection *current_deny_gc = NULL;
static gboolean current_is_deny = FALSE;
static GtkWidget *allow_list = NULL;
static GtkWidget *block_list = NULL;

static void set_deny_mode(GtkWidget *w, int data)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		return;
	debug_printf("setting deny mode %d\n", data);
	current_deny_gc->permdeny = data;
	serv_set_permit_deny(current_deny_gc);
	do_export(current_deny_gc);
}

static GtkWidget *deny_opt(char *label, int which, GtkWidget *box, GtkWidget *set)
{
	GtkWidget *opt;

	if (!set)
		opt = gtk_radio_button_new_with_label(NULL, label);
	else
		opt =
		    gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(set)),
						    label);
	gtk_box_pack_start(GTK_BOX(box), opt, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(opt), "toggled", GTK_SIGNAL_FUNC(set_deny_mode), (void *)which);
	gtk_widget_show(opt);
	if (current_deny_gc->permdeny == which)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(opt), TRUE);

	return opt;
}

static void des_deny_opt(GtkWidget *d, gpointer e)
{
	gtk_widget_destroy(d);
	current_deny_gc = NULL;
	deny_conn_hbox = NULL;
	deny_type = NULL;
	deny_opt_menu = NULL;
	current_is_deny = FALSE;
	allow_list = NULL;
	block_list = NULL;
}

static void set_deny_type()
{
	GSList *bg = gtk_radio_button_group(GTK_RADIO_BUTTON(deny_type));

	switch (current_deny_gc->permdeny) {
	case 5:
		bg = bg->next->next;
		break;
	case 4:
		break;
	case 3:
		bg = bg->next->next->next;
		break;
	case 2:
		bg = bg->next;
		break;
	case 1:
		bg = bg->next->next->next->next;
		break;
	}

	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(bg->data), TRUE);
}

void build_allow_list()
{
	GtkWidget *label;
	GtkWidget *list_item;
	GSList *p;

	if (!current_is_deny)
		return;

	p = current_deny_gc->permit;

	gtk_list_remove_items(GTK_LIST(allow_list), GTK_LIST(allow_list)->children);

	while (p) {
		label = gtk_label_new(p->data);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_object_set_user_data(GTK_OBJECT(list_item), p->data);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(allow_list), list_item);
		gtk_widget_show(list_item);
		p = p->next;
	}
}

void build_block_list()
{
	GtkWidget *label;
	GtkWidget *list_item;
	GSList *d;

	if (!current_is_deny)
		return;

	d = current_deny_gc->deny;

	gtk_list_remove_items(GTK_LIST(block_list), GTK_LIST(block_list)->children);

	while (d) {
		label = gtk_label_new(d->data);
		list_item = gtk_list_item_new();
		gtk_container_add(GTK_CONTAINER(list_item), label);
		gtk_object_set_user_data(GTK_OBJECT(list_item), d->data);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(block_list), list_item);
		gtk_widget_show(list_item);
		d = d->next;
	}
}

static void deny_gc_opt(GtkWidget *opt, struct gaim_connection *gc)
{
	current_deny_gc = gc;
	set_deny_type();
	build_allow_list();
	build_block_list();
}

static void build_deny_menu()
{
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *c = connections;
	struct gaim_connection *gc;
	int count = 0;
	gboolean found = FALSE;
	char buf[2048];

	if (g_slist_length(connections) == 1) {
		gtk_widget_hide(deny_conn_hbox);
		return;
	} else
		gtk_widget_show(deny_conn_hbox);

	menu = gtk_menu_new();

	while (c) {
		gc = (struct gaim_connection *)c->data;
		c = c->next;
		if (!gc->prpl->set_permit_deny)
			continue;
		g_snprintf(buf, sizeof buf, "%s (%s)", gc->username, gc->prpl->name);
		opt = gtk_menu_item_new_with_label(buf);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(deny_gc_opt), gc);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);
		if (gc == current_deny_gc)
			found = TRUE;
		else if (!found)
			count++;
	}

	if (!found) {
		current_deny_gc = connections->data;
		count = 0;
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(deny_opt_menu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(deny_opt_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(deny_opt_menu), count);

	gtk_widget_show(menu);
	gtk_widget_show(deny_opt_menu);
}

static void pref_deny_add(GtkWidget *button, gboolean permit)
{
	show_add_perm(current_deny_gc, NULL, permit);
}

static void pref_deny_rem(GtkWidget *button, gboolean permit)
{
	GList *i;
	char *who;

	if (permit && !allow_list)
		return;
	if (!permit && !block_list)
		return;

	if (permit)
		i = GTK_LIST(allow_list)->selection;
	else
		i = GTK_LIST(block_list)->selection;

	if (!i)
		return;
	who = gtk_object_get_user_data(GTK_OBJECT(i->data));
	if (permit) {
		current_deny_gc->permit = g_slist_remove(current_deny_gc->permit, who);
		serv_rem_permit(current_deny_gc, who);
		build_allow_list();
	} else {
		current_deny_gc->deny = g_slist_remove(current_deny_gc->deny, who);
		serv_rem_deny(current_deny_gc, who);
		build_block_list();
	}

	do_export(current_deny_gc);
}

GtkWidget *privacy_win;
void update_privacy_connections() { /* This is a slightly better name */
	gboolean needdeny = FALSE;
	GSList *c = connections;
	struct gaim_connection *gc = NULL;

	if (!privacy_win)
		return;

	while (c) {
		gc = c->data;
		if (gc->prpl->set_permit_deny)
			break;
		gc = NULL;
		c = c->next;
	}
	needdeny = (gc != NULL);
	

	if (needdeny) {
		gtk_widget_set_sensitive(privacy_win, TRUE);
		build_deny_menu();
		build_allow_list();
		build_block_list();
	} else {
		gtk_widget_set_sensitive(privacy_win, FALSE);
	}
}
static void destroy_privacy() {
	current_deny_gc = NULL;
	privacy_win = NULL;
}

void show_privacy_options()
{
	GtkWidget *pwin;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *sep;
	GtkWidget *close_button;

	current_deny_gc = connections->data;	/* this is safe because this screen will only be
						   available when there are connections */
	current_is_deny = TRUE;

	
	privacy_win = pwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(pwin), FALSE, TRUE, TRUE);
	gtk_window_set_role(GTK_WINDOW(pwin), "privacy");
	gtk_window_set_title(GTK_WINDOW(pwin), _("Gaim - Privacy"));
	gtk_signal_connect(GTK_OBJECT(pwin), "destroy", GTK_SIGNAL_FUNC(destroy_privacy), NULL);
	gtk_widget_realize(pwin);

	gtk_widget_set_usize(pwin, 0, 400);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(box), 5);
	gtk_container_add(GTK_CONTAINER(pwin), box);
	gtk_widget_show(box);

	label = gtk_label_new(_("Privacy settings are affected immediately."));
	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	deny_conn_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), deny_conn_hbox, FALSE, FALSE, 0);
	gtk_widget_show(deny_conn_hbox);

	label = gtk_label_new(_("Set privacy for:"));
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	deny_opt_menu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(deny_conn_hbox), deny_opt_menu, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(deny_opt_menu), "destroy", GTK_SIGNAL_FUNC(des_deny_opt), NULL);
	gtk_widget_show(deny_opt_menu);

	build_deny_menu();

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 5);
	gtk_widget_show(hbox);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	deny_type = deny_opt(_("Allow all users to contact me"), 1, vbox, NULL);
	deny_type = deny_opt(_("Allow only the users below"), 3, vbox, deny_type);
	deny_type = deny_opt(_("Allow only users on my buddy list"), 5, vbox, deny_type);

	label = gtk_label_new(_("Allow List"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);
	gtk_widget_show(sw);

	allow_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), allow_list);
	gtk_widget_show(allow_list);

	build_allow_list();

	bbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_widget_show(bbox);

	button = picture_button(pwin, _("Add"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_add), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	button = picture_button(pwin, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_rem), (void *)TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
	gtk_widget_show(vbox);

	deny_type = deny_opt(_("Deny all users"), 2, vbox, deny_type);
	deny_type = deny_opt(_("Block the users below"), 4, vbox, deny_type);

	label = gtk_label_new(_("Block List"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);
	gtk_widget_show(sw);

	block_list = gtk_list_new();
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), block_list);
	gtk_widget_show(block_list);

	build_block_list();

	bbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_widget_show(bbox);

	button = picture_button(pwin, _("Add"), gnome_add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_add), FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	button = picture_button(pwin, _("Remove"), gnome_remove_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pref_deny_rem), FALSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 5);
	gtk_widget_show(sep);

	hbox = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);
	close_button = picture_button(pwin, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), close_button, FALSE, FALSE, 5);
	gtk_signal_connect_object(GTK_OBJECT(close_button), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), pwin);

	gtk_widget_show(pwin);
}

 

/*------------------------------------------------------------------------*/
/*  The dialog for new buddy pounces                                      */
/*------------------------------------------------------------------------*/


void do_new_bp(GtkWidget *w, struct addbp *b)
{
	struct buddy_pounce *bp;
	
	if (strlen(gtk_entry_get_text(GTK_ENTRY(b->nameentry))) == 0) {
		do_error_dialog(_("Please enter a buddy to pounce."), NULL, GAIM_ERROR);
		return;
	}

        if(!b->buddy_pounce)
		bp = g_new0(struct buddy_pounce, 1);
	else
		bp = b->buddy_pounce;

	
	g_snprintf(bp->name, 80, "%s", gtk_entry_get_text(GTK_ENTRY(b->nameentry)));
	g_snprintf(bp->message, 2048, "%s", gtk_entry_get_text(GTK_ENTRY(b->messentry)));
	g_snprintf(bp->command, 2048, "%s", gtk_entry_get_text(GTK_ENTRY(b->commentry)));
	g_snprintf(bp->sound, 2048, "%s", gtk_entry_get_text(GTK_ENTRY(b->soundentry)));
	g_snprintf(bp->pouncer, 80, "%s", b->user->username);

	bp->protocol = b->user->protocol;

	bp->options = 0;

	if (GTK_TOGGLE_BUTTON(b->popupnotify)->active)
		bp->options |= OPT_POUNCE_NOTIFY;

	if (GTK_TOGGLE_BUTTON(b->openwindow)->active)
		bp->options |= OPT_POUNCE_POPUP;

	if (GTK_TOGGLE_BUTTON(b->sendim)->active)
		bp->options |= OPT_POUNCE_SEND_IM;

	if (GTK_TOGGLE_BUTTON(b->command)->active)
		bp->options |= OPT_POUNCE_COMMAND;

	if (GTK_TOGGLE_BUTTON(b->sound)->active)
		bp->options |= OPT_POUNCE_SOUND;

	if (GTK_TOGGLE_BUTTON(b->p_signon)->active)
		bp->options |= OPT_POUNCE_SIGNON;

	if (GTK_TOGGLE_BUTTON(b->p_unaway)->active)
		bp->options |= OPT_POUNCE_UNAWAY;

	if (GTK_TOGGLE_BUTTON(b->p_unidle)->active)
		bp->options |= OPT_POUNCE_UNIDLE;
	
	if (GTK_TOGGLE_BUTTON(b->p_typing)->active)
		bp->options |= OPT_POUNCE_TYPING;

	if (GTK_TOGGLE_BUTTON(b->save)->active)
		bp->options |= OPT_POUNCE_SAVE;

	if(!b->buddy_pounce)
		buddy_pounces = g_list_append(buddy_pounces, bp);

	do_bp_menu();

	destroy_dialog(NULL, b->window);

	save_prefs();
	g_free(b);
}

static void pounce_choose(GtkWidget *opt, struct addbp *b)
{
	struct aim_user *u = gtk_object_get_user_data(GTK_OBJECT(opt));
	b->user = u;
}

static GtkWidget *pounce_user_menu(struct addbp *b, struct gaim_connection *gc)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *u = aim_users;
	struct aim_user *a;
	struct prpl *p;
	int count = 0;
	int place = 0;
	char buf[2048];


	optmenu = gtk_option_menu_new();

	menu = gtk_menu_new();

	while (u) {
		a = (struct aim_user *)u->data;
		p = (struct prpl *)find_prpl(a->protocol);
		g_snprintf(buf, sizeof buf, "%s (%s)", a->username, (p && p->name)?p->name:"Unknown");
		opt = gtk_menu_item_new_with_label(buf);
		gtk_object_set_user_data(GTK_OBJECT(opt), a);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(pounce_choose), b);
		gtk_menu_append(GTK_MENU(menu), opt);
		gtk_widget_show(opt);

		if (b->user == a) {
			gtk_menu_item_activate(GTK_MENU_ITEM(opt));
			place = count;
		}

		count++;
			
		u = u->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), place);

	b->menu = optmenu;

	return optmenu;
}


void show_new_bp(char *name, struct gaim_connection *gc, int idle, int away, struct buddy_pounce *edit_bp)
{
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *optmenu;
	GtkWidget *sep;
	GtkSizeGroup *sg;

	struct addbp *b = g_new0(struct addbp, 1);
	
	if(edit_bp) {
		b->buddy_pounce = edit_bp;
		b->user = find_user(edit_bp->pouncer, edit_bp->protocol);
	} else {
		b->user = gc ? gc->user : aim_users->data;
		b->buddy_pounce = NULL;
	}

	GAIM_DIALOG(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "new_bp");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - New Buddy Pounce"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
	gtk_widget_show(vbox);

	/* <pounce type="who"> */
	frame = gtk_frame_new(_("Pounce Who"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(frame));

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);

	label = gtk_label_new(_("Account"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	optmenu = pounce_user_menu(b, gc);
	gtk_table_attach(GTK_TABLE(table), optmenu, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(optmenu);

	label = gtk_label_new(_("Buddy"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(label);

	b->nameentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->nameentry, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	if (name !=NULL)
		gtk_entry_set_text(GTK_ENTRY(b->nameentry), name);
	else if(edit_bp)
		gtk_entry_set_text(GTK_ENTRY(b->nameentry), edit_bp->name);
	gtk_window_set_focus(GTK_WINDOW(b->window), b->nameentry);
	gtk_widget_show(b->nameentry);
	/* </pounce type="who"> */


	/* <pounce type="when"> */
	frame = gtk_frame_new(_("Pounce When"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(frame));

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);
	
	b->p_signon = gtk_check_button_new_with_label(_("Pounce on sign on"));
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_signon), 
		                           (edit_bp->options & OPT_POUNCE_SIGNON) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_signon), TRUE);
	gtk_table_attach(GTK_TABLE(table), b->p_signon, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->p_signon);

	b->p_unaway = gtk_check_button_new_with_label(_("Pounce on return from away"));
	if (away)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_unaway), TRUE);
	else if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_unaway), 
					   (edit_bp->options & OPT_POUNCE_UNAWAY) ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), b->p_unaway, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(b->p_unaway);

	b->p_unidle = gtk_check_button_new_with_label(_("Pounce on return from idle"));
	if (idle)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_unidle), TRUE);
	else if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_unidle), 
				           (edit_bp->options & OPT_POUNCE_UNIDLE) ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), b->p_unidle, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->p_unidle);
	
	b->p_typing = gtk_check_button_new_with_label(_("Pounce when buddy is typing to you"));
	if (edit_bp)		
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->p_typing),
					   (edit_bp->options & OPT_POUNCE_TYPING) ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), b->p_typing,1,2,1,2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(b->p_typing);

	/* </pounce type="when"> */
	
	/* <pounce type="action"> */
	frame = gtk_frame_new(_("Pounce Action"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(frame));

	table = gtk_table_new(4, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_widget_show(table);
	
	b->openwindow = gtk_check_button_new_with_label(_("Open IM Window"));
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->openwindow), 
			                   (edit_bp->options & OPT_POUNCE_POPUP) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->openwindow), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->openwindow, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->openwindow);
	
	b->popupnotify = gtk_check_button_new_with_label(_("Popup Notification"));
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->popupnotify), 
					   (edit_bp->options & OPT_POUNCE_NOTIFY) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->popupnotify), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->popupnotify, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->popupnotify);

	b->sendim = gtk_check_button_new_with_label(_("Send Message"));
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sendim), 
					   (edit_bp->options & OPT_POUNCE_SEND_IM) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sendim), TRUE);
	gtk_table_attach(GTK_TABLE(table), b->sendim, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->sendim);

	b->messentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->messentry, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(b->messentry), "activate", GTK_SIGNAL_FUNC(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->messentry), 
					(edit_bp->options & OPT_POUNCE_SEND_IM) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->messentry), edit_bp->message);
	}
	gtk_widget_show(b->messentry);

	gtk_signal_connect(GTK_OBJECT(b->sendim), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive),	b->messentry);

	b->command = gtk_check_button_new_with_label(_("Execute command on pounce"));
	gtk_table_attach(GTK_TABLE(table), b->command, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->command),
					   (edit_bp->options & OPT_POUNCE_COMMAND) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->command), FALSE);
	gtk_widget_show(b->command);

	b->commentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->commentry, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(b->commentry), "activate", GTK_SIGNAL_FUNC(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->commentry), 
					(edit_bp->options & OPT_POUNCE_COMMAND) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->commentry), edit_bp->command);
	}
	else
		gtk_widget_set_sensitive(GTK_WIDGET(b->commentry), FALSE);
	gtk_widget_show(b->commentry);
	gtk_signal_connect(GTK_OBJECT(b->command), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), b->commentry);
	
	b->sound = gtk_check_button_new_with_label(_("Play sound on pounce"));
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sound), 
					   (edit_bp->options & OPT_POUNCE_SOUND) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sound), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->sound, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->sound);

	b->soundentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->soundentry, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(b->soundentry), "activate", GTK_SIGNAL_FUNC(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->soundentry), 
					(edit_bp->options & OPT_POUNCE_SOUND) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->soundentry), edit_bp->sound);
	} else 
		gtk_widget_set_sensitive(GTK_WIDGET(b->soundentry), FALSE);
	gtk_widget_show(b->soundentry);
	gtk_signal_connect(GTK_OBJECT(b->sound), "clicked", GTK_SIGNAL_FUNC(toggle_sensitive), b->soundentry);
	/* </pounce type="action"> */

	b->save = gtk_check_button_new_with_label(_("Save this pounce after activation"));
	gtk_container_set_border_width(GTK_CONTAINER(b->save), 7);
	if(edit_bp)
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->save),
					   (edit_bp->options & OPT_POUNCE_SAVE) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->save), FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), b->save, FALSE, FALSE, 0);
	gtk_widget_show(b->save);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);
	
	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	button = gaim_pixbuf_button_from_stock(_("_Save"), "gtk-execute", GAIM_BUTTON_HORIZONTAL);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_new_bp), b);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(_("C_ancel"), "gtk-cancel", GAIM_BUTTON_HORIZONTAL);
	gtk_size_group_add_widget(sg, button);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);


	gtk_widget_show(b->window);
}



/*------------------------------------------------------------------------*/
/*  The dialog for SET INFO / SET DIR INFO                                */
/*------------------------------------------------------------------------*/

void do_save_info(GtkWidget *widget, struct set_info_dlg *b)
{
	gchar *junk;
	struct gaim_connection *gc;

	junk = gtk_editable_get_chars(GTK_EDITABLE(b->text), 0, -1);

	if (b->user) {
		strncpy_withhtml(b->user->user_info, junk, sizeof b->user->user_info);
		gc = b->user->gc;

		save_prefs();

		if (gc)
			serv_set_info(gc, b->user->user_info);
	}
	g_free(junk);
	destroy_dialog(NULL, b->window);
	g_free(b);
}

void do_set_dir(GtkWidget *widget, struct set_dir_dlg *b)
{
	const char *first = gtk_entry_get_text(GTK_ENTRY(b->first));
	int web = GTK_TOGGLE_BUTTON(b->web)->active;
	const char *middle = gtk_entry_get_text(GTK_ENTRY(b->middle));
	const char *last = gtk_entry_get_text(GTK_ENTRY(b->last));
	const char *maiden = gtk_entry_get_text(GTK_ENTRY(b->maiden));
	const char *city = gtk_entry_get_text(GTK_ENTRY(b->city));
	const char *state = gtk_entry_get_text(GTK_ENTRY(b->state));
	const char *country = gtk_entry_get_text(GTK_ENTRY(b->country));

	serv_set_dir(b->gc, first, middle, last, maiden, city, state, country, web);

	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_set_dir(struct gaim_connection *gc)
{
	GtkWidget *label;
	GtkWidget *bot;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	char buf[256];

	struct set_dir_dlg *b = g_new0(struct set_dir_dlg, 1);
	b->gc = gc;

	GAIM_DIALOG(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_role(GTK_WINDOW(b->window), "set_dir");
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set Dir Info"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_widget_show(fbox);

	frame = gtk_frame_new(_("Directory Info"));
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	g_snprintf(buf, sizeof(buf), "Setting Dir Info for %s:", gc->username);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	b->first = gtk_entry_new();
	b->middle = gtk_entry_new();
	b->last = gtk_entry_new();
	b->maiden = gtk_entry_new();
	b->city = gtk_entry_new();
	b->state = gtk_entry_new();
	b->country = gtk_entry_new();
	b->web = gtk_check_button_new_with_label(_("Allow Web Searches To Find Your Info"));

	/* Line 1 */
	label = gtk_label_new(_("First Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->first, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 2 */
	label = gtk_label_new(_("Middle Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->middle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);


	/* Line 3 */
	label = gtk_label_new(_("Last Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->last, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 4 */
	label = gtk_label_new(_("Maiden Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->maiden, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 5 */
	label = gtk_label_new(_("City"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->city, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 6 */
	label = gtk_label_new(_("State"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->state, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 7 */
	label = gtk_label_new(_("Country"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->country, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	/* Line 8 */

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), b->web, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(b->first);
	gtk_widget_show(b->middle);
	gtk_widget_show(b->last);
	gtk_widget_show(b->maiden);
	gtk_widget_show(b->city);
	gtk_widget_show(b->state);
	gtk_widget_show(b->country);
	gtk_widget_show(b->web);

	/* And add the buttons */

	bot = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), bot, FALSE, FALSE, 0);
	gtk_widget_show(bot);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(bot), b->cancel, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	b->save = picture_button(b->window, _("Save"), save_xpm);
	gtk_box_pack_end(GTK_BOX(bot), b->save, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->save), "clicked", GTK_SIGNAL_FUNC(do_set_dir), b);

	gtk_window_set_focus(GTK_WINDOW(b->window), b->first);

	gtk_widget_show(b->window);
}

void do_change_password(GtkWidget *widget, struct passwddlg *b)
{
	const gchar *orig, *new1, *new2;

	orig = gtk_entry_get_text(GTK_ENTRY(b->original));
	new1 = gtk_entry_get_text(GTK_ENTRY(b->new1));
	new2 = gtk_entry_get_text(GTK_ENTRY(b->new2));

	if (g_strcasecmp(new1, new2)) {
		do_error_dialog(_("New Passwords Do Not Match"), NULL, GAIM_ERROR);
		return;
	}

	if ((strlen(orig) < 1) || (strlen(new1) < 1) || (strlen(new2) < 1)) {
		do_error_dialog(_("Fill out all fields completely"), NULL, GAIM_ERROR);
		return;
	}

	serv_change_passwd(b->gc, orig, new1);

	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_change_passwd(struct gaim_connection *gc)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *fbox;
	GtkWidget *frame;
	char buf[256];

	struct passwddlg *b = g_new0(struct passwddlg, 1);
	b->gc = gc;

	GAIM_DIALOG(b->window);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "change_passwd");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Password Change"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(fbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_widget_show(fbox);

	frame = gtk_frame_new(_("Change Password"));
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	g_snprintf(buf, sizeof(buf), "Changing password for %s:", gc->username);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	/* First Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Original Password"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	b->original = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->original), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->original, FALSE, FALSE, 0);
	gtk_widget_show(b->original);

	/* Next Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("New Password"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	b->new1 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new1), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->new1, FALSE, FALSE, 0);
	gtk_widget_show(b->new1);

	/* Next Line */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("New Password (again)"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	b->new2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new2), FALSE);
	gtk_box_pack_end(GTK_BOX(hbox), b->new2, FALSE, FALSE, 0);
	gtk_widget_show(b->new2);

	/* Now do our row of buttons */
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), b->cancel, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	b->ok = picture_button(b->window, _("OK"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), b->ok, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->ok), "clicked", GTK_SIGNAL_FUNC(do_change_password), b);


	gtk_widget_show(b->window);
}

void show_set_info(struct gaim_connection *gc)
{
	GtkWidget *buttons;
	GtkWidget *label;
	GtkWidget *vbox;
	gchar *buf;
	struct aim_user *tmp;

	struct set_info_dlg *b = g_new0(struct set_info_dlg, 1);
	tmp = gc->user;
	b->user = tmp;

	GAIM_DIALOG(b->window);
	gtk_window_set_role(GTK_WINDOW(b->window), "set_info");
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set User Info"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
	gtk_widget_show(vbox);

	buf = g_malloc(256);
	g_snprintf(buf, 256, "Changing info for %s:", tmp->username);
	label = gtk_label_new(buf);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	b->text = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(b->text), TRUE);
	gtk_text_set_editable(GTK_TEXT(b->text), TRUE);
	gtk_widget_set_usize(b->text, 300, 200);
	buf = g_malloc(strlen(tmp->user_info) + 1);
	strncpy_nohtml(buf, tmp->user_info, strlen(tmp->user_info) + 1);
	gtk_text_insert(GTK_TEXT(b->text), NULL, NULL, NULL, buf, -1);
	g_free(buf);
	gtk_box_pack_start(GTK_BOX(vbox), b->text, TRUE, TRUE, 0);
	gtk_widget_show(b->text);
	gtk_window_set_focus(GTK_WINDOW(b->window), b->text);

	buttons = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), buttons, FALSE, FALSE, 0);
	gtk_widget_show(buttons);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(buttons), b->cancel, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	b->save = picture_button(b->window, _("Save"), save_xpm);
	gtk_box_pack_end(GTK_BOX(buttons), b->save, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(b->save), "clicked", GTK_SIGNAL_FUNC(do_save_info), b);

	gtk_widget_show(b->window);

}

/*------------------------------------------------------------------------*/
/*  The dialog for the info requests                                      */
/*------------------------------------------------------------------------*/

static void info_dlg_free(GtkWidget *b, struct info_dlg *d)
{
	if (g_slist_find(info_dlgs, d))
		info_dlgs = g_slist_remove(info_dlgs, d);
	g_free(d->who);
	g_free(d);
}

gchar **info_img_handler(gchar *url)
{
	if (!g_strcasecmp(url, "free_icon.gif"))
		return free_icon_xpm;
	if (!g_strcasecmp(url, "aol_icon.gif"))
		return aol_icon_xpm;
	if (!g_strcasecmp(url, "dt_icon.gif"))
		return dt_icon_xpm;
	if (!g_strcasecmp(url, "admin_icon.gif"))
		return admin_icon_xpm;
	if (!g_strcasecmp(url, "ab_icon.gif"))
		return ab_xpm;
	if (!g_strcasecmp(url, "wireless_icon.gif"))
		return wireless_icon_xpm;
	if (!g_strcasecmp(url, "icq_icon.gif"))
		return icon_online_xpm;
	return NULL;
}

/* if away is 0, show regardless and try to get away message
 *            1, don't show if regular info isn't shown
 *            2, show regardless but don't try to get away message
 *
 * i wish this were my client. if i were i wouldn't have to deal with this shit.
 */
void g_show_info_text(struct gaim_connection *gc, char *who, int away, char *info, ...)
{
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *text;
	GtkWidget *bbox;
	GtkWidget *sw;
	gint options = 0;
	char *more_info;
	va_list ap;

	struct info_dlg *b = find_info_dlg(gc, who);
	if (!b && (away == 1))
		return;
	if (!b) {
		b = g_new0(struct info_dlg, 1);
		b->gc = gc;
		b->who = who ? g_strdup(normalize(who)) : NULL;
		info_dlgs = g_slist_append(info_dlgs, b);

		GAIM_DIALOG(b->window);
		gtk_window_set_title(GTK_WINDOW(b->window), "Gaim");
		gtk_container_border_width(GTK_CONTAINER(b->window), 5);
		gtk_widget_realize(GTK_WIDGET(b->window));
		gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(info_dlg_free), b);

		bbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(b->window), bbox);

		label = gtk_label_new(_("Below are the results of your search: "));
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 0);

		text = gtk_imhtml_new(NULL, NULL);
		b->text = text;
		gtk_container_add(GTK_CONTAINER(sw), text);
		gtk_widget_set_usize(sw, 300, 250);
		gtk_imhtml_set_img_handler(GTK_IMHTML(text), info_img_handler);
		gaim_setup_imhtml(text);

		ok = picture_button(b->window, _("OK"), ok_xpm);
		gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
					  GTK_OBJECT(b->window));
		gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);

		gtk_widget_show_all(b->window);
	}

	if (convo_options & OPT_CONVO_IGNORE_COLOUR)
		options ^= GTK_IMHTML_NO_COLOURS;
	if (convo_options & OPT_CONVO_IGNORE_FONTS)
		options ^= GTK_IMHTML_NO_FONTS;
	if (convo_options & OPT_CONVO_IGNORE_SIZES)
		options ^= GTK_IMHTML_NO_SIZES;
	options ^= GTK_IMHTML_NO_COMMENTS;
	options ^= GTK_IMHTML_NO_TITLE;
	options ^= GTK_IMHTML_NO_NEWLINE;
	options ^= GTK_IMHTML_NO_SCROLL;

	gtk_imhtml_append_text(GTK_IMHTML(b->text), info, -1, options);

	va_start(ap, info);
	while ((more_info = va_arg(ap, char *)) != NULL)
		 gtk_imhtml_append_text(GTK_IMHTML(b->text), more_info, -1, options);
	va_end(ap);

	if (away)
		info_dlgs = g_slist_remove(info_dlgs, b);
	else
		serv_get_away(gc, who);
}

/*------------------------------------------------------------------------*/
/*  The dialog for adding to permit/deny                                  */
/*------------------------------------------------------------------------*/


static void do_add_perm(GtkWidget *w, struct addperm *p)
{

	const char *who;
	char *name;


	who = gtk_entry_get_text(GTK_ENTRY(p->entry));

	name = g_malloc(strlen(who) + 2);
	g_snprintf(name, strlen(who) + 2, "%s", who);

	if (!p->permit) {
		GSList *d = p->gc->deny;
		char *n;
		n = g_strdup(normalize(name));
		while (d) {
			if (!g_strcasecmp(n, normalize(d->data)))
				break;
			d = d->next;
		}
		g_free(n);
		if (!d) {
			p->gc->deny = g_slist_append(p->gc->deny, name);
			serv_add_deny(p->gc, name);
			build_block_list();
			do_export(p->gc);
		} else
			g_free(name);
	} else {
		GSList *d = p->gc->permit;
		char *n;
		n = g_strdup(normalize(name));
		while (d) {
			if (!g_strcasecmp(n, normalize(d->data)))
				break;
			d = d->next;
		}
		g_free(n);
		if (!d) {
			p->gc->permit = g_slist_append(p->gc->permit, name);
			serv_add_permit(p->gc, name);
			build_allow_list();
			do_export(p->gc);
		} else
			g_free(name);
	}

	destroy_dialog(NULL, p->window);
}



void show_add_perm(struct gaim_connection *gc, char *who, gboolean permit)
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;
	GtkWidget *frame;

	struct addperm *p = g_new0(struct addperm, 1);
	p->gc = gc;
	p->permit = permit;

	GAIM_DIALOG(p->window);
	gtk_container_set_border_width(GTK_CONTAINER(p->window), 5);
	gtk_window_set_policy(GTK_WINDOW(p->window), FALSE, FALSE, TRUE);
	gtk_widget_realize(p->window);

	dialogwindows = g_list_prepend(dialogwindows, p->window);

	bbox = gtk_hbox_new(TRUE, 10);
	topbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);
	p->entry = gtk_entry_new();

	if (permit)
		frame = gtk_frame_new(_("Permit"));
	else
		frame = gtk_frame_new(_("Deny"));

	/* Build Add Button */

	if (permit)
		add = picture_button(p->window, _("Permit"), add_xpm);
	else
		add = picture_button(p->window, _("Deny"), add_xpm);
	cancel = picture_button(p->window, _("Cancel"), cancel_xpm);

	/* End of Cancel Button */
	if (who != NULL)
		gtk_entry_set_text(GTK_ENTRY(p->entry), who);

	/* Put the buttons in the box */

	gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

	label = gtk_label_new(_("Add"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), p->entry, FALSE, FALSE, 5);
	/* And the boxes in the box */
	gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);


	/* Handle closes right */
	gtk_signal_connect(GTK_OBJECT(p->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), p->window);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), p->window);
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(do_add_perm), p);
	gtk_signal_connect(GTK_OBJECT(p->entry), "activate", GTK_SIGNAL_FUNC(do_add_perm), p);

	/* Finish up */
	gtk_widget_show(add);
	gtk_widget_show(cancel);
	gtk_widget_show(p->entry);
	gtk_widget_show(topbox);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(frame);
	if (permit)
		gtk_window_set_title(GTK_WINDOW(p->window), _("Gaim - Add Permit"));
	else
		gtk_window_set_title(GTK_WINDOW(p->window), _("Gaim - Add Deny"));
	gtk_window_set_focus(GTK_WINDOW(p->window), p->entry);
	gtk_container_add(GTK_CONTAINER(p->window), frame);
	gtk_widget_realize(p->window);

	gtk_widget_show(p->window);
}


/*------------------------------------------------------------------------*/
/*  Functions Called To Add A Log                                          */
/*------------------------------------------------------------------------*/

void cancel_log(GtkWidget *widget, struct conversation *c)
{
	if (c->log_button) {
		set_state_lock(1);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(c->log_button), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->log_dialog);
	gtk_widget_destroy(c->log_dialog);
	c->log_dialog = NULL;
}

void do_log(GtkWidget *w, struct conversation *c)
{
	struct log_conversation *l;
	char buf[128];
	const char *file;
	char path[PATHSIZE];

	if (!find_log_info(c->name)) {
		file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(c->log_dialog));
		strncpy(path, file, PATHSIZE - 1);
		if (file_is_dir(path, c->log_dialog)) {
			return;
		}

		l = (struct log_conversation *)g_new0(struct log_conversation, 1);
		strcpy(l->name, c->name);
		strcpy(l->filename, file);
		log_conversations = g_list_append(log_conversations, l);

		if (c != NULL) {
			g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, c->name);
			gtk_window_set_title(GTK_WINDOW(c->window), buf);
		}
	}

	save_prefs();
	cancel_log(NULL, c);
}

void show_log_dialog(struct conversation *c)
{
	char *buf = g_malloc(BUF_LEN);

	if (!c->log_dialog) {
		c->log_dialog = gtk_file_selection_new(_("Gaim - Log Conversation"));

		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(c->log_dialog));

		g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S "%s.log", gaim_home_dir(), normalize(c->name));
		gtk_object_set_user_data(GTK_OBJECT(c->log_dialog), "log dialog");
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(c->log_dialog), buf);
		gtk_signal_connect(GTK_OBJECT(c->log_dialog), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->log_dialog)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(do_log), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->log_dialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(cancel_log), c);
	}

	g_free(buf);

	gtk_widget_show(c->log_dialog);
	gdk_window_raise(c->log_dialog->window);
}

/*------------------------------------------------------*/
/* Find Buddy By Email                                  */
/*------------------------------------------------------*/

void do_find_info(GtkWidget *w, struct findbyinfo *b)
{
	const char *first;
	const char *middle;
	const char *last;
	const char *maiden;
	const char *city;
	const char *state;
	const char *country;

	first = gtk_entry_get_text(GTK_ENTRY(b->firstentry));
	middle = gtk_entry_get_text(GTK_ENTRY(b->middleentry));
	last = gtk_entry_get_text(GTK_ENTRY(b->lastentry));
	maiden = gtk_entry_get_text(GTK_ENTRY(b->maidenentry));
	city = gtk_entry_get_text(GTK_ENTRY(b->cityentry));
	state = gtk_entry_get_text(GTK_ENTRY(b->stateentry));
	country = gtk_entry_get_text(GTK_ENTRY(b->countryentry));

	serv_dir_search(b->gc, first, middle, last, maiden, city, state, country, "");
	destroy_dialog(NULL, b->window);
}

void do_find_email(GtkWidget *w, struct findbyemail *b)
{
	const char *email;

	email = gtk_entry_get_text(GTK_ENTRY(b->emailentry));

	serv_dir_search(b->gc, "", "", "", "", "", "", "", email);

	destroy_dialog(NULL, b->window);
}

void show_find_info(struct gaim_connection *gc)
{
	GtkWidget *cancel;
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *fbox;
	GtkWidget *frame;

	struct findbyinfo *b = g_new0(struct findbyinfo, 1);
	b->gc = gc;
	GAIM_DIALOG(b->window);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "find_info");
	gtk_widget_show(b->window);

	dialogwindows = g_list_prepend(dialogwindows, b->window);

	frame = gtk_frame_new(_("Search for Buddy"));
	fbox = gtk_vbox_new(FALSE, 5);

	/* Build OK Button */

	ok = picture_button(b->window, _("OK"), ok_xpm);
	cancel = picture_button(b->window, _("Cancel"), cancel_xpm);

	bbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	b->firstentry = gtk_entry_new();
	b->middleentry = gtk_entry_new();
	b->lastentry = gtk_entry_new();
	b->maidenentry = gtk_entry_new();
	b->cityentry = gtk_entry_new();
	b->stateentry = gtk_entry_new();
	b->countryentry = gtk_entry_new();

	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(bbox), ok, FALSE, FALSE, 0);

	/* Line 1 */
	label = gtk_label_new(_("First Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->firstentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 2 */

	label = gtk_label_new(_("Middle Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->middleentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 3 */

	label = gtk_label_new(_("Last Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->lastentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 4 */

	label = gtk_label_new(_("Maiden Name"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->maidenentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 5 */

	label = gtk_label_new(_("City"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->cityentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 6 */
	label = gtk_label_new(_("State"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->stateentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Line 7 */
	label = gtk_label_new(_("Country"));
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), b->countryentry, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* Merge The Boxes */

	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(fbox), bbox, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(do_find_info), b);

	gtk_widget_show(ok);
	gtk_widget_show(cancel);
	gtk_widget_show(b->firstentry);
	gtk_widget_show(b->middleentry);
	gtk_widget_show(b->lastentry);
	gtk_widget_show(b->maidenentry);
	gtk_widget_show(b->cityentry);
	gtk_widget_show(b->stateentry);
	gtk_widget_show(b->countryentry);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(frame);
	gtk_widget_show(fbox);

	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Find Buddy By Info"));
	gtk_window_set_focus(GTK_WINDOW(b->window), b->firstentry);
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_container_border_width(GTK_CONTAINER(b->window), 5);
	gtk_widget_realize(b->window);

	gtk_widget_show(b->window);
}

void show_find_email(struct gaim_connection *gc)
{
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *frame;
	GtkWidget *topbox;
	GtkWidget *button;

	struct findbyemail *b = g_new0(struct findbyemail, 1);
	if (g_slist_find(connections, gc))
		b->gc = gc;
	GAIM_DIALOG(b->window);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "find_email");
	gtk_widget_realize(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Find Buddy By Email"));

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);

	frame = gtk_frame_new(_("Search for Buddy"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	topbox = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), topbox);
	gtk_container_set_border_width(GTK_CONTAINER(topbox), 5);

	label = gtk_label_new(_("Email"));
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 0);

	b->emailentry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(topbox), b->emailentry, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(b->emailentry), "activate", GTK_SIGNAL_FUNC(do_find_email), b);
	gtk_window_set_focus(GTK_WINDOW(b->window), b->emailentry);

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	button = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = picture_button(b->window, _("OK"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_find_email), b);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(b->window);
}

/*------------------------------------------------------*/
/* Link Dialog                                          */
/*------------------------------------------------------*/

void cancel_link(GtkWidget *widget, struct conversation *c)
{
	if (c->link) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->link), FALSE);
		set_state_lock(0);
	}
	destroy_dialog(NULL, c->link_dialog);
	c->link_dialog = NULL;
}

void do_add_link(GtkWidget *widget, struct linkdlg *b)
{
	char *open_tag;
	const char *urltext, *showtext;
	open_tag = g_malloc(2048);


	urltext = gtk_entry_get_text(GTK_ENTRY(b->url));
	showtext = gtk_entry_get_text(GTK_ENTRY(b->text));

	g_snprintf(open_tag, 2048, "<A HREF=\"%s\">%s", urltext, showtext);
/* FIXME	surround(b, open_tag, "</A>");*/

	g_free(open_tag);
	destroy_dialog(NULL, b->window);
}


void show_add_link(GtkWidget *linky, struct conversation *c)
{
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *fbox;

	if (!c->link_dialog) {
		struct linkdlg *b = g_new0(struct linkdlg, 1);
		GAIM_DIALOG(c->link_dialog);
		gtk_window_set_role(GTK_WINDOW(c->link_dialog), "add_link");
		dialogwindows = g_list_prepend(dialogwindows, c->link_dialog);

		gtk_widget_set_usize(c->link_dialog, 270, 165);
		gtk_window_set_policy(GTK_WINDOW(c->link_dialog), FALSE, FALSE, TRUE);
		gtk_widget_show(c->link_dialog);

		vbox = gtk_vbox_new(FALSE, 10);
		bbox = gtk_hbox_new(TRUE, 10);
		frame = gtk_frame_new(_("Insert Link"));
		fbox = gtk_vbox_new(FALSE, 5);

		/* Build OK Button */

		b->ok = picture_button(c->link_dialog, _("OK"), ok_xpm);
		b->cancel = picture_button(c->link_dialog, _("Cancel"), cancel_xpm);
/*
		gtk_widget_set_usize(b->ok, 75, 30);
		gtk_widget_set_usize(b->cancel, 75, 30);
*/
		gtk_box_pack_start(GTK_BOX(bbox), b->ok, FALSE, FALSE, 10);
		gtk_box_pack_end(GTK_BOX(bbox), b->cancel, FALSE, FALSE, 10);
		gtk_widget_show(bbox);

		b->url = gtk_entry_new();
		b->text = gtk_entry_new();

		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("URL"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_box_pack_end(GTK_BOX(hbox), b->url, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

		hbox = gtk_hbox_new(FALSE, 5);
		label = gtk_label_new(_("Description"));
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_box_pack_end(GTK_BOX(hbox), b->text, FALSE, FALSE, 5);
		gtk_widget_show(label);
		gtk_widget_show(hbox);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

		gtk_widget_show(b->url);
		gtk_widget_show(b->text);
		gtk_widget_show(frame);
		gtk_widget_show(fbox);

		gtk_container_add(GTK_CONTAINER(frame), vbox);
		gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);
		gtk_box_pack_start(GTK_BOX(fbox), bbox, TRUE, TRUE, 5);
		gtk_widget_show(vbox);

		gtk_signal_connect(GTK_OBJECT(c->link_dialog), "destroy",
				   GTK_SIGNAL_FUNC(cancel_link), c);
		gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(cancel_link), c);
		gtk_signal_connect(GTK_OBJECT(b->ok), "clicked", GTK_SIGNAL_FUNC(do_add_link), b);

		gtk_container_add(GTK_CONTAINER(c->link_dialog), fbox);
		gtk_container_border_width(GTK_CONTAINER(c->link_dialog), 10);
		gtk_window_set_title(GTK_WINDOW(c->link_dialog), _("Gaim - Add URL"));
		gtk_window_set_focus(GTK_WINDOW(c->link_dialog), b->url);
		b->window = c->link_dialog;
		b->toggle = linky;
/* FIXME		b->entry_view = c->entry_view;*/
		gtk_widget_realize(c->link_dialog);

	}

	gtk_widget_show(c->link_dialog);
	gdk_window_raise(c->link_dialog->window);
}


/*------------------------------------------------------*/
/* Color Selection Dialog                               */
/*------------------------------------------------------*/

GtkWidget *fgcseld = NULL;
GtkWidget *bgcseld = NULL;

void cancel_fgcolor(GtkWidget *widget, struct conversation *c)
{
	if (c->fgcolorbtn && widget) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->fgcolorbtn), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->fg_color_dialog);
	gtk_widget_destroy(c->fg_color_dialog);
	c->fg_color_dialog = NULL;
}

void cancel_bgcolor(GtkWidget *widget, struct conversation *c)
{
	if (c->bgcolorbtn && widget) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->bgcolorbtn), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->bg_color_dialog);
	gtk_widget_destroy(c->bg_color_dialog);
	c->bg_color_dialog = NULL;
}

void do_fgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	struct conversation *c;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_current_color(colorsel, &text_color);

	c = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	/* GTK_IS_EDITABLE(c->entry); huh? */

	text_color.red = text_color.red / 256;
	text_color.green = text_color.green / 256;
	text_color.blue = text_color.blue / 256;
	
	c->fgcol = text_color;
	c->hasfg = 1;
	g_snprintf(open_tag, 23, "<FONT COLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green,
		   text_color.blue);
	surround(c, open_tag, "</FONT>");
	debug_printf("#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	g_free(open_tag);
	cancel_fgcolor(NULL, c);
}

void do_bgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	GdkColor text_color;
	struct conversation *c;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_current_color(colorsel, &text_color);

	c = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	/* GTK_IS_EDITABLE(c->entry); huh? */

	text_color.red = text_color.red / 256;
	text_color.green = text_color.green / 256;
	text_color.blue = text_color.blue / 256;

	c->bgcol = text_color;
	c->hasbg = 1;
	g_snprintf(open_tag, 25, "<BODY BGCOLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green,
		   text_color.blue);
	surround(c, open_tag, "</BODY>");
	debug_printf("#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	g_free(open_tag);
	cancel_bgcolor(NULL, c);
}

void show_fgcolor_dialog(struct conversation *c, GtkWidget *color)
{
	GtkWidget *colorsel;
	gdouble fgclr[3];

	if (color == NULL) {	/* we came from the prefs */
		if (fgcseld)
			return;
		fgclr[0] = (gdouble)(fgcolor.red) / 255;
		fgclr[1] = (gdouble)(fgcolor.green) / 255;
		fgclr[2] = (gdouble)(fgcolor.blue) / 255;

		fgcseld = gtk_color_selection_dialog_new(_("Select Text Color"));
		gtk_color_selection_set_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel), fgclr);
		gtk_signal_connect(GTK_OBJECT(fgcseld), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_colorsel), (void *)1);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(fgcseld)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(destroy_colorsel), (void *)1);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(fgcseld)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(apply_color_dlg), (void *)1);
		gtk_widget_realize(fgcseld);
		gtk_widget_show(fgcseld);
		gdk_window_raise(fgcseld->window);
		return;
	}

	if (!c->fg_color_dialog) {
		fgclr[0] = (gdouble)(c->fgcol.red) / 255;
		fgclr[1] = (gdouble)(c->fgcol.green) / 255;
		fgclr[2] = (gdouble)(c->fgcol.blue) / 255;

		c->fg_color_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
		colorsel = GTK_COLOR_SELECTION_DIALOG(c->fg_color_dialog)->colorsel;
		gtk_color_selection_set_color(GTK_COLOR_SELECTION(colorsel), fgclr);
		gtk_object_set_user_data(GTK_OBJECT(colorsel), c);

		gtk_signal_connect(GTK_OBJECT(c->fg_color_dialog), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(c->fg_color_dialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(do_fgcolor), colorsel);
		gtk_signal_connect(GTK_OBJECT
				   (GTK_COLOR_SELECTION_DIALOG(c->fg_color_dialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(cancel_fgcolor), c);

		gtk_widget_realize(c->fg_color_dialog);
	}

	gtk_widget_show(c->fg_color_dialog);
	gdk_window_raise(c->fg_color_dialog->window);
}

void show_bgcolor_dialog(struct conversation *c, GtkWidget *color)
{
	GtkWidget *colorsel;
	gdouble bgclr[3];

	if (color == NULL) {	/* we came from the prefs */
		if (bgcseld)
			return;
		bgclr[0] = (gdouble)(bgcolor.red) / 255;
		bgclr[1] = (gdouble)(bgcolor.green) / 255;
		bgclr[2] = (gdouble)(bgcolor.blue) / 255;

		bgcseld = gtk_color_selection_dialog_new(_("Select Background Color"));
		gtk_color_selection_set_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel), bgclr);
		gtk_signal_connect(GTK_OBJECT(bgcseld), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_colorsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(bgcseld)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(destroy_colorsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(bgcseld)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(apply_color_dlg), (void *)2);
		gtk_widget_realize(bgcseld);
		gtk_widget_show(bgcseld);
		gdk_window_raise(bgcseld->window);
		return;
	}

	if (!c->bg_color_dialog) {
		bgclr[0] = (gdouble)(c->bgcol.red) / 255;
		bgclr[1] = (gdouble)(c->bgcol.green) / 255;
		bgclr[2] = (gdouble)(c->bgcol.blue) / 255;

		c->bg_color_dialog = gtk_color_selection_dialog_new(_("Select Background Color"));
		colorsel = GTK_COLOR_SELECTION_DIALOG(c->bg_color_dialog)->colorsel;
		gtk_color_selection_set_color(GTK_COLOR_SELECTION(colorsel), bgclr);
		gtk_object_set_user_data(GTK_OBJECT(colorsel), c);

		gtk_signal_connect(GTK_OBJECT(c->bg_color_dialog), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(c->bg_color_dialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(do_bgcolor), colorsel);
		gtk_signal_connect(GTK_OBJECT
				   (GTK_COLOR_SELECTION_DIALOG(c->bg_color_dialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(cancel_bgcolor), c);

		gtk_widget_realize(c->bg_color_dialog);
	}

	gtk_widget_show(c->bg_color_dialog);
	gdk_window_raise(c->bg_color_dialog->window);
}

/*------------------------------------------------------------------------*/
/*  Font Selection Dialog                                                 */
/*------------------------------------------------------------------------*/

void cancel_font(GtkWidget *widget, struct conversation *c)
{
	if (c->font && widget) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->font), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->font_dialog);
	gtk_widget_destroy(c->font_dialog);
	c->font_dialog = NULL;
}

void apply_font(GtkWidget *widget, GtkFontSelection *fontsel)
{
	/* this could be expanded to include font size, weight, etc.
	   but for now only works with font face */
	int i = 0;
	char *fontname;
	struct conversation *c = gtk_object_get_user_data(GTK_OBJECT(fontsel));

	if (c) {
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));
		while(fontname[i] && !isdigit(fontname[i])) { 
			i++;
		}
		fontname[i] = 0;
		set_font_face(fontname, c);
	} else {
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));
		while(fontface[i] && !isdigit(fontname[i]) && i < sizeof(fontface)) { 
			fontface[i] = fontname[i];
			i++;
		}
		fontface[i] = 0;
	}

	cancel_font(NULL, c);
}

void destroy_fontsel(GtkWidget *w, gpointer d)
{
	gtk_widget_destroy(fontseld);
	fontseld = NULL;
}

void show_font_dialog(struct conversation *c, GtkWidget *font)
{
	char fonttif[128];
	if (!font) {		/* we came from the prefs dialog */
		if (fontseld)
			return;
		fontseld = gtk_font_selection_dialog_new(_("Select Font"));
		if (fontface[0]) {
			g_snprintf(fonttif, sizeof(fonttif), "%s 12", fontface);
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								fonttif);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								DEFAULT_FONT_FACE " 12");
		}

		gtk_object_set_user_data(GTK_OBJECT(fontseld), NULL);
		gtk_signal_connect(GTK_OBJECT(fontseld), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(apply_font_dlg), fontseld);
		gtk_widget_realize(fontseld);
		gtk_widget_show(fontseld);
		gdk_window_raise(fontseld->window);
		return;
	}

	if (!c->font_dialog) {
		c->font_dialog = gtk_font_selection_dialog_new(_("Select Font"));

		if (font)
			gtk_object_set_user_data(GTK_OBJECT(c->font_dialog), c);
		else
			gtk_object_set_user_data(GTK_OBJECT(c->font_dialog), NULL);

		if (c->fontface[0]) {
			g_snprintf(fonttif, sizeof(fonttif), "%s 12", c->fontface);
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(c->font_dialog),
							       fonttif);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(c->font_dialog),
								DEFAULT_FONT_FACE);
		}

		gtk_signal_connect(GTK_OBJECT(c->font_dialog), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(apply_font), c->font_dialog);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(cancel_font), c);

		gtk_widget_realize(c->font_dialog);

	}
	gtk_widget_show(c->font_dialog);
	gdk_window_raise(c->font_dialog->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for import/export                                          */
/*------------------------------------------------------------------------*/

static void do_import_dialog(GtkWidget *w, struct gaim_connection *gc)
{
	const char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(importdialog));
	if (file_is_dir(file, importdialog)) {
		return;
	}
	if (g_slist_find(connections, importgc)) {
		do_import(importgc, file);
		do_export(importgc);
	}
	destroy_dialog(NULL, importdialog);
}

static void set_import_gc(gpointer data, struct gaim_connection *gc)
{
	importgc = gc;
}

static void create_import_dropdown(GtkFileSelection *fs)
{
	GtkWidget *hbox;
	GtkWidget *label;
	GSList *g = connections;
	struct gaim_connection *c;
	GtkWidget *optmenu;
	GtkWidget *menu;
	char buf[256];
	GtkWidget *opt;

	if (!connections)
		return;
	importgc = connections->data;
	if (!connections->next)
		return;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(fs->action_area), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_end(GTK_BOX(hbox), optmenu, FALSE, FALSE, 5);
	gtk_widget_show(optmenu);

	label = gtk_label_new(_("Import to:"));
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	menu = gtk_menu_new();

	while (g) {
		c = (struct gaim_connection *)g->data;
		g_snprintf(buf, sizeof buf, "%s (%s)", c->username, c->prpl->name);
		opt = gtk_menu_item_new_with_label(buf);
		gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(set_import_gc), c);
		gtk_widget_show(opt);
		gtk_menu_append(GTK_MENU(menu), opt);
		g = g->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
}

void show_import_dialog()
{
	char *buf = g_malloc(BUF_LEN);
	if (!importdialog) {
		importdialog = gtk_file_selection_new(_("Gaim - Import Buddy List"));

		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(importdialog));

		g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S, gaim_home_dir());

		gtk_file_selection_set_filename(GTK_FILE_SELECTION(importdialog), buf);
		gtk_signal_connect(GTK_OBJECT(importdialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), importdialog);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(importdialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(do_import_dialog), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(importdialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(destroy_dialog), importdialog);

		create_import_dropdown(GTK_FILE_SELECTION(importdialog));
	}

	g_free(buf);
	gtk_widget_show(importdialog);
	gdk_window_raise(importdialog->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for new away messages                                      */
/*------------------------------------------------------------------------*/

static struct away_message *save_away_message(struct create_away *ca)
{
	struct away_message *am;
	guint text_len;
	gchar *away_message;

	if (!ca->mess)
		am = g_new0(struct away_message, 1);
	else {
		am = ca->mess;
	}

	g_snprintf(am->name, sizeof(am->name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	text_len = gtk_text_get_length(GTK_TEXT(ca->text));
	away_message = gtk_editable_get_chars(GTK_EDITABLE(ca->text), 0, text_len);

	g_snprintf(am->message, sizeof(am->message), "%s", away_message);
	g_free(away_message);

	if (!ca->mess) {
		away_messages = g_slist_insert_sorted(away_messages, am, sort_awaymsg_list);
	}

	save_prefs();
	do_away_menu();
	if (prefs_away_list != NULL)
		gtk_list_select_item(GTK_LIST(prefs_away_list), g_slist_index(away_messages, am));

	return am;
}

int check_away_mess(struct create_away *ca, int type)
{
	if ((strlen(gtk_entry_get_text(GTK_ENTRY(ca->entry))) == 0) && (type == 1)) {
		/* We shouldn't allow a blank title */
		do_error_dialog(_("You cannot save an away message with a blank title"), 
				_("Please give the message a title, or choose \"Make Away\" to use "
				  "without saving."), GAIM_ERROR);
		return 0;
	}

	if ((gtk_text_get_length(GTK_TEXT(ca->text)) == 0) && (type <= 1)) {
		/* We shouldn't allow a blank message */
		do_error_dialog(_("You cannot create an empty away message"), NULL, GAIM_ERROR);
		return 0;
	}

	return 1;
}

void save_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;

	save_away_message(ca);
	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void use_away_mess(GtkWidget *widget, struct create_away *ca)
{
	static struct away_message am;
	guint text_len;
	gchar *away_message;

	if (!check_away_mess(ca, 0))
		return;

	g_snprintf(am.name, sizeof(am.name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));
	text_len = gtk_text_get_length(GTK_TEXT(ca->text));
	if (text_len < 0)
		return;
	away_message = gtk_editable_get_chars(GTK_EDITABLE(ca->text), 0, text_len);

	g_snprintf(am.message, sizeof(am.message), "%s", away_message);
	g_free(away_message);

	do_away_message(NULL, &am);

	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void su_away_mess(GtkWidget *widget, struct create_away *ca)
{
	if (!check_away_mess(ca, 1))
		return;
	do_away_message(NULL, save_away_message(ca));
	destroy_dialog(NULL, ca->window);
	g_free(ca);
}

void create_away_mess(GtkWidget *widget, void *dummy)
{
	GtkWidget *hbox;
	GtkWidget *titlebox;
	GtkWidget *tbox;
	GtkWidget *sw;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *button;

	struct create_away *ca = g_new0(struct create_away, 1);

	/* Set up window */
	GAIM_DIALOG(ca->window);
	gtk_widget_set_usize(ca->window, -1, 250);
	gtk_container_border_width(GTK_CONTAINER(ca->window), 5);
	gtk_window_set_role(GTK_WINDOW(ca->window), "away_mess");
	gtk_window_set_title(GTK_WINDOW(ca->window), _("Gaim - New away message"));
	gtk_signal_connect(GTK_OBJECT(ca->window), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), ca->window);
	gtk_widget_realize(ca->window);

	tbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(ca->window), tbox);
	gtk_widget_show(tbox);

	frame = gtk_frame_new(_("New away message"));
	gtk_box_pack_start(GTK_BOX(tbox), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_container_add(GTK_CONTAINER(frame), fbox);
	gtk_widget_show(fbox);

	titlebox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), titlebox, FALSE, FALSE, 0);
	gtk_widget_show(titlebox);

	label = gtk_label_new(_("Away title: "));
	gtk_box_pack_start(GTK_BOX(titlebox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	ca->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(titlebox), ca->entry, TRUE, TRUE, 0);
	gtk_widget_grab_focus(ca->entry);
	gtk_widget_show(ca->entry);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(fbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	ca->text = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(ca->text), TRUE);
	gtk_text_set_editable(GTK_TEXT(ca->text), TRUE);
	gtk_container_add(GTK_CONTAINER(sw), ca->text);
	gtk_widget_show(ca->text);

       
	if (dummy) {
		struct away_message *amt;
		GtkTreeIter iter;
		int pos = 0;
		GtkListStore *ls = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dummy)));
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dummy));
		GValue val = { 0, };

		if (! gtk_tree_selection_get_selected (sel, (GtkTreeModel**)&ls, &iter))
			return;
		gtk_tree_model_get_value (GTK_TREE_MODEL(ls), &iter, 1, &val);
		amt = g_value_get_pointer (&val);
		gtk_entry_set_text(GTK_ENTRY(ca->entry), amt->name);
		gtk_editable_insert_text(GTK_EDITABLE(ca->text), amt->message,
					 strlen(amt->message), &pos);
		ca->mess = amt;
	}

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(tbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = picture_button(ca->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), ca->window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = picture_button(ca->window, _("Use"), gnome_preferences_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(use_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = picture_button(ca->window, _("Save & Use"), add_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(su_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	button = picture_button(ca->window, _("Save"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(save_away_mess), ca);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	gtk_widget_show(ca->window);
}

/* smiley dialog */

void close_smiley_dialog(GtkWidget *widget, struct conversation *c)
{
	if (c->smiley) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->smiley), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->smiley_dialog);
	gtk_widget_destroy(c->smiley_dialog);
	c->smiley_dialog = NULL;
}

void set_smiley(GtkWidget *w, char *face) 
{
	current_smiley = face;
}

void set_smiley_array(GtkWidget *widget, int smiley_type)
{
	int i;

	for (i = 0; i < FACE_TOTAL; i++)
		smiley_array[i] = 0;

	smiley_array[smiley_type] = 1;

	return;
}

void insert_smiley_text(GtkWidget *widget, struct conversation *c)
{
	gtk_text_buffer_insert_at_cursor(c->entry_buffer,
					 current_smiley, -1);
	close_smiley_dialog(NULL, c);
}

static void toolbar_add_smiley(struct conversation *c, GtkWidget *bar, char* path, char *filename, char *face)
{
	GtkWidget *image;
	GtkWidget *button;
	char *buf;

	buf = g_build_filename(path, filename, NULL);
	image = gtk_image_new_from_file(buf);
	g_free(buf);
	button =
		gtk_toolbar_append_item(GTK_TOOLBAR(bar), NULL, NULL, NULL,
					image, GTK_SIGNAL_FUNC(set_smiley), (char *)face);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(insert_smiley_text), c);

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
}

void show_smiley_dialog(struct conversation *c, GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkWidget *vbox, *smiley_box = NULL;
	GtkWidget *win;
	GtkWidget *bbox;
	char *smiley_path = 0;

	if (c->smiley_dialog)
		return;

	win = c->window;

	GAIM_DIALOG(dialog);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	gtk_window_set_role(GTK_WINDOW(dialog), "smiley_dialog");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

	/* setup boxes */
	vbox = gtk_vbox_new(TRUE, 5);
	bbox = gtk_hbox_new(FALSE, 5);


	/* setup buttons */

	/* pack buttons */
	
	smiley_box = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);

	smiley_path = g_build_filename(DATADIR, "pixmaps", "gaim", "smileys", "default", NULL); 
	toolbar_add_smiley(c, smiley_box, smiley_path, "angel.png", "O:-)");
	toolbar_add_smiley(c, smiley_box, smiley_path, "bigsmile.png", ":-D");
	toolbar_add_smiley(c, smiley_box, smiley_path, "burp.png", ":-!");
	toolbar_add_smiley(c, smiley_box, smiley_path, "crossedlips.png", ":-X");

	smiley_box = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
	toolbar_add_smiley(c, smiley_box, smiley_path, "cry.png", ":'(");
	toolbar_add_smiley(c, smiley_box, smiley_path, "embarrassed.png", ":-[");
	toolbar_add_smiley(c, smiley_box, smiley_path, "kiss.png", ":-*");
	toolbar_add_smiley(c, smiley_box, smiley_path, "moneymouth.png", ":-$");

	smiley_box = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
	toolbar_add_smiley(c, smiley_box, smiley_path, "sad.png", ":-(");
	toolbar_add_smiley(c, smiley_box, smiley_path, "scream.png", "=-O");
	toolbar_add_smiley(c, smiley_box, smiley_path, "smile.png", ":-)");
      	toolbar_add_smiley(c, smiley_box, smiley_path, "cool.png", "8-)");

	smiley_box = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
	toolbar_add_smiley(c, smiley_box, smiley_path, "think.png", ":-/");
	toolbar_add_smiley(c, smiley_box, smiley_path, "tongue.png", ":-P");
	toolbar_add_smiley(c, smiley_box, smiley_path, "wink.png", ";-)");
	toolbar_add_smiley(c, smiley_box, smiley_path, "yell.png", ">:o");

	g_free(smiley_path);

	gtk_container_add(GTK_CONTAINER(dialog), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);

	/* connect signals */
	gtk_object_set_user_data(GTK_OBJECT(dialog), "smiley dialog");
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_dialog), c);

	/* show everything */
	gtk_window_set_title(GTK_WINDOW(dialog), _("Smile!"));
	gtk_widget_show_all(dialog);

	c->smiley_dialog = dialog;

	return;
}

static void do_alias_bud(GtkWidget *w, struct buddy *b)
{
	const char *al = gtk_entry_get_text(GTK_ENTRY(aliasname));
	g_snprintf(b->show, sizeof(b->show), "%s", (al && strlen(al)) ? al : b->name);
	handle_buddy_rename(b, b->name);
	serv_alias_buddy(b);
	destroy_dialog(aliasdlg, aliasdlg);
}

void alias_dialog_bud(struct buddy *b)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *cancel;
	static GtkWidget *add = NULL;
	GtkWidget *label;
	GtkWidget *topbox;
	static int a, c;

	if (aliasdlg)
		gtk_widget_destroy(aliasdlg);

	GAIM_DIALOG(aliasdlg);
	gtk_window_set_policy(GTK_WINDOW(aliasdlg), FALSE, FALSE, TRUE);
	gtk_window_set_role(GTK_WINDOW(aliasdlg), "alias_dialog");
	gtk_widget_realize(aliasdlg);
	dialogwindows = g_list_prepend(dialogwindows, aliasdlg);

	bbox = gtk_hbox_new(TRUE, 10);
	topbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);

	aliasentry = gtk_entry_new();
	aliasname = gtk_entry_new();
	/* Put the buttons in the box */

	add = picture_button(aliasdlg, _("Alias"), add_xpm);

	cancel = picture_button(aliasdlg, _("Cancel"), cancel_xpm);

	gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Alias Buddy"));

	label = gtk_label_new(_("Buddy"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), aliasentry, FALSE, FALSE, 5);
	gtk_entry_set_editable(GTK_ENTRY(aliasentry), FALSE);
	label = gtk_label_new(_("Alias"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), aliasname, FALSE, FALSE, 5);

	gtk_entry_set_text(GTK_ENTRY(aliasentry), b->name);
	gtk_entry_set_text(GTK_ENTRY(aliasname), b->show);

	/* And the boxes in the box */
	gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

	/* Handle closes right */
	gtk_signal_connect(GTK_OBJECT(aliasdlg), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), aliasdlg);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), aliasdlg);
	a = gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(do_alias_bud), b);
	c = gtk_signal_connect(GTK_OBJECT(aliasname), "activate", GTK_SIGNAL_FUNC(do_alias_bud), b);
	/* Finish up */
	gtk_widget_show(add);
	gtk_widget_show(cancel);
	gtk_widget_show(aliasentry);
	gtk_widget_show(aliasname);
	gtk_widget_show(topbox);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(frame);
	gtk_window_set_title(GTK_WINDOW(aliasdlg), _("Gaim - Alias Buddy"));
	gtk_window_set_focus(GTK_WINDOW(aliasdlg), aliasentry);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_container_add(GTK_CONTAINER(aliasdlg), frame);
	gtk_container_set_border_width(GTK_CONTAINER(aliasdlg), 5);
	gtk_widget_realize(aliasdlg);

	gtk_widget_show(aliasdlg);
}


static gboolean dont_destroy(gpointer a, gpointer b, gpointer c)
{
	return TRUE;
}

static void do_save_log(GtkWidget *w, GtkWidget *filesel)
{
	const char *file;
	char path[PATHSIZE];
	char buf[BUF_LONG];
	char error[BUF_LEN];
	FILE *fp_old, *fp_new;
	char filename[PATHSIZE];
	char *name;
	char *tmp;

	name = gtk_object_get_user_data(GTK_OBJECT(filesel));
	tmp = gaim_user_dir();
	g_snprintf(filename, PATHSIZE, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s%s", tmp,
		   name ? normalize(name) : "system", name ? ".log" : "");

	file = (const char*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	strncpy(path, file, PATHSIZE - 1);
	if (file_is_dir(path, filesel))
		return;

	if ((fp_new = fopen(path, "w")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   _("Couldn't write to %s."), path);
		do_error_dialog(error, strerror(errno), GAIM_ERROR);
		return;
	}

	if ((fp_old = fopen(filename, "r")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   _("Couldn't write to %s."), filename);
		do_error_dialog(error, strerror(errno), GAIM_ERROR);
		fclose(fp_new);
		return;
	}

	while (fgets(buf, BUF_LONG, fp_old))
		fputs(buf, fp_new);
	fclose(fp_old);
	fclose(fp_new);

	gtk_widget_destroy(filesel);

	return;
}

static void show_save_log(GtkWidget *w, gchar *name)
{
	GtkWidget *filesel;
	gchar buf[BUF_LEN];

	g_snprintf(buf, BUF_LEN - 1, "%s" G_DIR_SEPARATOR_S "%s%s", gaim_home_dir(),
		   name ? normalize(name) : "system", name ? ".log" : "");

	filesel = gtk_file_selection_new(_("Gaim - Save Log File"));
	gtk_signal_connect(GTK_OBJECT(filesel), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), filesel);

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(filesel));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), buf);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(do_save_log), filesel);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(destroy_dialog), filesel);
	gtk_object_set_user_data(GTK_OBJECT(filesel), name);

	gtk_widget_realize(filesel);
	gtk_widget_show(filesel);

	return;
}

static void do_clear_log_file(GtkWidget *w, gchar *name)
{
	gchar buf[256];
	gchar filename[256];
	GtkWidget *window;
	char *tmp;

	tmp = gaim_user_dir();
	g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s%s", tmp,
		   name ? normalize(name) : "system", name ? ".log" : "");

	if ((remove(filename)) == -1) {
		g_snprintf(buf, 256, _("Couldn't remove file %s." ), filename);
		do_error_dialog(buf, strerror(errno), GAIM_ERROR);
	}

	window = gtk_object_get_user_data(GTK_OBJECT(w));
	destroy_dialog(NULL, window);
}

static void show_clear_log(GtkWidget *w, gchar *name)
{
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *hsep;

	GAIM_DIALOG(window);
	gtk_window_set_role(GTK_WINDOW(window), "dialog");
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(destroy_dialog), window);
	gtk_widget_realize(window);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	label = gtk_label_new(_("Really clear log?"));
	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 15);

	hsep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(box), hsep, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_widget_show(button);

	button = picture_button(window, _("Okay"), ok_xpm);
	gtk_object_set_user_data(GTK_OBJECT(button), gtk_object_get_user_data(GTK_OBJECT(w)));
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_clear_log_file), name);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_widget_show(button);

	gtk_widget_show_all(window);

	return;
}

static void log_show_convo(GtkWidget *w, struct view_log *view)
{
	gchar buf[BUF_LONG];
	FILE *fp;
	char filename[256];
	int i=0;
	GString *string;
	guint block;

	string = g_string_new("");

	if (view->name) {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, normalize(view->name));
	} else {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "system", tmp);
	}
	if ((fp = fopen(filename, "r")) == NULL) {
		g_snprintf(buf, BUF_LONG, "Couldn't open log file %s.", filename);
		do_error_dialog(buf, strerror(errno), GAIM_ERROR);
		return;
	}

	gtk_widget_set_sensitive(view->bbox, FALSE);
	gtk_signal_disconnect_by_func(GTK_OBJECT(view->window),
				      GTK_SIGNAL_FUNC(destroy_dialog), view->window);
	block = gtk_signal_connect(GTK_OBJECT(view->window), "delete_event",
				   GTK_SIGNAL_FUNC(dont_destroy), view->window);

	fseek(fp, view->offset, SEEK_SET);
	gtk_imhtml_clear(GTK_IMHTML(view->layout));
	/*
	while (gtk_events_pending())
		gtk_main_iteration();
	*/

	while (fgets(buf, BUF_LONG, fp) && !strstr(buf, "---- New C")) {
		i++;
		if (strlen(buf) >= 5 && (!strncmp(buf + strlen(buf) - 5, "<BR>\n", 5)))
			/* take off the \n */
			buf[strlen(buf) - 1] = '\0';

		/* don't lose the thirtieth line of conversation. thanks FeRD */
		g_string_append(string, buf);

		if (i == 30) {
			gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, -1, view->options);
			g_string_free(string, TRUE);
			string = g_string_new("");
			/* you can't have these anymore. if someone clicks on another item while one is
			 * drawing, it will try to move to that item, and that causes problems here.
			while (gtk_events_pending())
				gtk_main_iteration();
			*/
			i = 0;
		}

	}
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), string->str, -1, view->options);
	gtk_imhtml_append_text(GTK_IMHTML(view->layout), "<BR>", -1, view->options);

	gtk_widget_set_sensitive(view->bbox, TRUE);
	gtk_signal_disconnect(GTK_OBJECT(view->window), block);
	gtk_signal_connect(GTK_OBJECT(view->window), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), view->window);
	g_string_free(string, TRUE);
	fclose(fp);
}

static void des_view_item(GtkObject *obj, struct view_log *view)
{
	if (view->name)
		g_free(view->name);
	g_free(view);
}

static void des_log_win(GtkObject *win, gpointer data)
{
	char *x = gtk_object_get_user_data(win);
	if (x)
		g_free(x);
}

void conv_show_log(GtkWidget *w, gpointer data)
{
	char *name = g_strdup(data);
	show_log(name);
	g_free(name);
}

void chat_show_log(GtkWidget *w, gpointer data)
{
	char *name = g_strdup_printf("%s.chat", (char*)data);
	show_log(name);
	g_free(name);
}

void show_log(char *nm)
{
	gchar filename[256];
	gchar buf[BUF_LONG];
	FILE *fp;
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *hbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *layout;
	GtkWidget *close_button;
	GtkWidget *clear_button;
	GtkWidget *save_button;
	GtkWidget *list = NULL;
	GList *item_list = NULL;
	GtkWidget *item = NULL;
	GtkWidget *last = NULL;
	GtkWidget *frame;
	struct view_log *view;
	char *name = nm ? g_strdup(nm) : NULL;

	int options;
	guint block;
	char convo_start[32];
	long offset = 0;
	unsigned int i = 0;

	options = GTK_IMHTML_NO_COMMENTS | GTK_IMHTML_NO_TITLE | GTK_IMHTML_NO_SCROLL;
	if (convo_options & OPT_CONVO_IGNORE_COLOUR)
		options ^= GTK_IMHTML_NO_COLOURS;
	if (convo_options & OPT_CONVO_IGNORE_FONTS)
		options ^= GTK_IMHTML_NO_FONTS;
	if (convo_options & OPT_CONVO_IGNORE_SIZES)
		options ^= GTK_IMHTML_NO_SIZES;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_user_data(GTK_OBJECT(window), name);
	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(des_log_win), NULL);
	gtk_window_set_role(GTK_WINDOW(window), "log");
	if (name)
		g_snprintf(buf, BUF_LONG, "Gaim - Conversations with %s", name);
	else
		g_snprintf(buf, BUF_LONG, "Gaim - System Log");
	gtk_window_set_title(GTK_WINDOW(window), buf);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	block = gtk_signal_connect(GTK_OBJECT(window), "delete_event",
				   GTK_SIGNAL_FUNC(dont_destroy), window);
	gtk_widget_realize(window);

	layout = gtk_imhtml_new(NULL, NULL);
	bbox = gtk_hbox_new(FALSE, 0);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);

	if (name) {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s" G_DIR_SEPARATOR_S "logs" G_DIR_SEPARATOR_S "%s.log", tmp, normalize(name));
		if ((fp = fopen(filename, "r")) == NULL) {
			g_snprintf(buf, BUF_LONG, "Couldn't open log file %s", filename);
			do_error_dialog(buf, strerror(errno), GAIM_ERROR);
			return;
		}

		list = gtk_list_new();

		frame = gtk_frame_new(_("Date"));
		gtk_widget_show(frame);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(sw), 5);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), list);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
					       GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_widget_set_usize(sw, 220, 220);
		gtk_container_add(GTK_CONTAINER(frame), sw);
		gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

		while (fgets(buf, BUF_LONG, fp)) {
			if (strstr(buf, "---- New C")) {
				int length;
				char *temp = strchr(buf, '@');

				if (temp == NULL || strlen(temp) < 2)
					continue;

				if (i == 1 && item_list) {
					item_list = g_list_remove(item_list, last);
					last = NULL;
				}

				i = 0;

				temp++;
				length = strcspn(temp, "-");
				if (length > 31) length = 31;

				offset = ftell(fp);
				g_snprintf(convo_start, length, "%s", temp);
				item = gtk_list_item_new_with_label(convo_start);
				view = g_new0(struct view_log, 1);
				view->options = options;
				view->offset = offset;
				view->name = g_strdup(name);
				view->bbox = bbox;
				view->window = window;
				view->layout = layout;
				gtk_signal_connect(GTK_OBJECT(item), "select",
						   GTK_SIGNAL_FUNC(log_show_convo), view);
				gtk_signal_connect(GTK_OBJECT(item), "destroy",
						   GTK_SIGNAL_FUNC(des_view_item), view);
				last = item;
				item_list = g_list_append(item_list, item);

				gtk_widget_show(item);
			}
			i++;
		}
		fclose(fp);
		if (i == 1 && item_list)
			item_list = g_list_remove(item_list, item);
		
		if (item_list)
			gtk_list_insert_items(GTK_LIST(list), item_list, 0 );
	}

	gtk_signal_disconnect(GTK_OBJECT(window), block);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(destroy_dialog), window);

	frame = gtk_frame_new(_("Conversation"));
	gtk_widget_show(frame);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(sw), 5);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(sw, 390, 220);
	gtk_container_add(GTK_CONTAINER(frame), sw);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(layout), "url_clicked", GTK_SIGNAL_FUNC(open_url), NULL);
	gtk_container_add(GTK_CONTAINER(sw), layout);
	gaim_setup_imhtml(layout);

	gtk_box_pack_start(GTK_BOX(box), bbox, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(bbox, FALSE);

	close_button = picture_button(window, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), close_button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(close_button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), window);

	clear_button = picture_button(window, _("Clear"), close_xpm);
	gtk_object_set_user_data(GTK_OBJECT(clear_button), window);
	gtk_box_pack_end(GTK_BOX(bbox), clear_button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(clear_button), "clicked", GTK_SIGNAL_FUNC(show_clear_log), name);

	save_button = picture_button(window, _("Save"), save_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), save_button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(save_button), "clicked", GTK_SIGNAL_FUNC(show_save_log), name);

	gtk_widget_show_all(window);				
	
	if (!name) {
		view = g_new0(struct view_log, 1);
		view->options = options;
		view->name = NULL;
		view->bbox = bbox;
		view->window = window;
		view->layout = layout;
		log_show_convo(layout, view);
		gtk_signal_connect(GTK_OBJECT(layout), "destroy", GTK_SIGNAL_FUNC(des_view_item), view);
	} else {
		gtk_list_select_item(GTK_LIST(list), 0);
	}
	
	gtk_widget_set_sensitive(bbox, TRUE);

	return;
}

/*------------------------------------------------------------------------*/
/*  The dialog for renaming groups                                        */
/*------------------------------------------------------------------------*/

static void do_rename_group(GtkObject *obj, GtkWidget *entry)
{
	const char *new_name;
	struct group *g;
	struct group *orig;
	struct gaim_connection *gc;

	new_name = gtk_entry_get_text(GTK_ENTRY(entry));
	g = gtk_object_get_user_data(obj);

	if (new_name && (strlen(new_name) != 0) && strcmp(new_name, g->name)) {
		char *prevname;
		gc = g->gc;

		if ((orig = find_group(g->gc, new_name)) != NULL && g_strcasecmp(new_name, g->name)) {
			orig->members = g_slist_concat(orig->members, g->members);
			handle_group_rename(orig, g->name);
			g->gc->groups = g_slist_remove(g->gc->groups, g);
			/* FIXME, i don't like calling this. it's sloppy. */ build_edit_tree();
			serv_rename_group(gc, g, new_name);
			g_free(g);
		} else {
			prevname = g_strdup(g->name);
			serv_rename_group(gc, g, new_name);
			g_snprintf(g->name, sizeof(g->name), "%s", new_name);
			handle_group_rename(g, prevname);
			/* FIXME, i don't like calling this. it's sloppy. */ build_edit_tree();
			g_free(prevname);
		}
		do_export(gc);
	}

	destroy_dialog(rename_dialog, rename_dialog);
}

void show_rename_group(GtkWidget *unused, struct group *g)
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *name_entry;
	GtkWidget *label;

	if (!rename_dialog) {
		GAIM_DIALOG(rename_dialog);
		gtk_window_set_role(GTK_WINDOW(rename_dialog), "rename_dialog");
		gtk_window_set_policy(GTK_WINDOW(rename_dialog), FALSE, TRUE, TRUE);
		gtk_window_set_title(GTK_WINDOW(rename_dialog), _("Gaim - Rename Group"));
		gtk_signal_connect(GTK_OBJECT(rename_dialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_dialog);
		gtk_widget_realize(rename_dialog);

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(rename_dialog), mainbox);

		frame = gtk_frame_new(_("Rename Group"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		label = gtk_label_new(_("New name:"));
		gtk_box_pack_start(GTK_BOX(fbox), label, FALSE, FALSE, 0);

		name_entry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(fbox), name_entry, TRUE, TRUE, 0);
		gtk_object_set_user_data(GTK_OBJECT(name_entry), g);
		gtk_entry_set_text(GTK_ENTRY(name_entry), g->name);
		gtk_signal_connect(GTK_OBJECT(name_entry), "activate",
				   GTK_SIGNAL_FUNC(do_rename_group), name_entry);
		gtk_widget_grab_focus(name_entry);

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		button = picture_button(rename_dialog, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_dialog);

		button = picture_button(rename_dialog, _("OK"), ok_xpm);
		gtk_object_set_user_data(GTK_OBJECT(button), g);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(do_rename_group), name_entry);
	}

	gtk_widget_show_all(rename_dialog);
}


/*------------------------------------------------------------------------*/
/*  The dialog for renaming buddies                                       */
/*------------------------------------------------------------------------*/

static void do_rename_buddy(GtkObject *obj, GtkWidget *entry)
{
	const char *new_name;
	struct buddy *b;
	GSList *gr;

	new_name = gtk_entry_get_text(GTK_ENTRY(entry));
	b = gtk_object_get_user_data(obj);

	if (!g_slist_find(connections, b->gc)) {
		destroy_dialog(rename_bud_dialog, rename_bud_dialog);
		return;
	}

	gr = b->gc->groups;
	while (gr) {
		if (g_slist_find(((struct group *)gr->data)->members, b))
			break;
		gr = gr->next;
	}
	if (!gr) {
		destroy_dialog(rename_bud_dialog, rename_bud_dialog);
		return;
	}

	if (new_name && (strlen(new_name) != 0) && strcmp(new_name, b->name)) {
		struct group *g = find_group_by_buddy(b->gc, b->name);
		char *prevname = g_strdup(b->name);
		if (g)
			serv_remove_buddy(b->gc, b->name, g->name);
		if (!strcmp(b->name, b->show))
			 g_snprintf(b->show, sizeof(b->show), "%s", new_name);
		g_snprintf(b->name, sizeof(b->name), "%s", new_name);
		serv_add_buddy(b->gc, b->name);
		handle_buddy_rename(b, prevname);
		g_free(prevname);
	}

	destroy_dialog(rename_bud_dialog, rename_bud_dialog);
}

void show_rename_buddy(GtkWidget *unused, struct buddy *b)
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *name_entry;
	GtkWidget *label;

	if (!rename_bud_dialog) {
		GAIM_DIALOG(rename_bud_dialog);
		gtk_window_set_role(GTK_WINDOW(rename_bud_dialog), "rename_bud_dialog");
		gtk_window_set_policy(GTK_WINDOW(rename_bud_dialog), FALSE, TRUE, TRUE);
		gtk_window_set_title(GTK_WINDOW(rename_bud_dialog), _("Gaim - Rename Buddy"));
		gtk_signal_connect(GTK_OBJECT(rename_bud_dialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_bud_dialog);
		gtk_widget_realize(rename_bud_dialog);

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(rename_bud_dialog), mainbox);

		frame = gtk_frame_new(_("Rename Buddy"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		label = gtk_label_new(_("New name:"));
		gtk_box_pack_start(GTK_BOX(fbox), label, FALSE, FALSE, 0);

		name_entry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(fbox), name_entry, TRUE, TRUE, 0);
		gtk_object_set_user_data(GTK_OBJECT(name_entry), b);
		gtk_entry_set_text(GTK_ENTRY(name_entry), b->name);
		gtk_signal_connect(GTK_OBJECT(name_entry), "activate",
				   GTK_SIGNAL_FUNC(do_rename_buddy), name_entry);
		gtk_widget_grab_focus(name_entry);

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		button = picture_button(rename_bud_dialog, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_bud_dialog);

		button = picture_button(rename_bud_dialog, _("OK"), ok_xpm);
		gtk_object_set_user_data(GTK_OBJECT(button), b);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(do_rename_buddy), name_entry);
	}

	gtk_widget_show_all(rename_bud_dialog);
}

#ifdef USE_PERL

static GtkWidget *perl_config = NULL;
static char *perl_last_dir = NULL;

static void cfdes(GtkWidget *m, gpointer n)
{
	if (perl_config)
		gtk_widget_destroy(perl_config);
	perl_config = NULL;
}

static void do_load(GtkWidget *m, gpointer n)
{
	const char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(perl_config));
	gchar *f = NULL;
	if (!file || !strlen(file)) {
		return;
	}

	if (file_is_dir(file, perl_config)) {
		return;
	}

	if (perl_last_dir) {
		g_free(perl_last_dir);
	}
	perl_last_dir = g_dirname(file);

	debug_printf("Loading perl script: %s\n", file);

	f = g_strdup(file);
	perl_load_file(f);
	g_free(f);
	cfdes(perl_config, NULL);
}

void load_perl_script()
{
	char *buf, *temp;

	if (perl_config) {
		gtk_widget_show(perl_config);
		gdk_window_raise(perl_config->window);
		return;
	}

	/* Below is basically stolen from plugins.c */
	perl_config = gtk_file_selection_new(_("Gaim - Select Perl Script"));

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(perl_config));

	if (!perl_last_dir) {
		temp = gaim_user_dir();
		buf = g_strconcat(temp, G_DIR_SEPARATOR_S, NULL);
	} else {
		buf = g_strconcat(perl_last_dir, G_DIR_SEPARATOR_S, NULL);
	}

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(perl_config), buf);
	gtk_file_selection_complete(GTK_FILE_SELECTION(perl_config), "*.pl");
	gtk_signal_connect(GTK_OBJECT(perl_config), "destroy", GTK_SIGNAL_FUNC(cfdes), perl_config);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(perl_config)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(do_load), NULL);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(perl_config)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(cfdes), NULL);

	g_free(buf);
	gtk_widget_show(perl_config);
	gdk_window_raise(perl_config->window);
}

#endif /* USE_PERL */

GtkWidget *gaim_pixbuf_toolbar_button_from_stock(char *icon)
{
	GtkWidget *button, *image,  *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);
	return button;
}

GtkWidget *gaim_pixbuf_toolbar_button_from_file(char *icon)
{
	GtkWidget *button, *image,  *bbox;
	char *filename;

	if (!icon)
		return NULL;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);
	
	filename = g_build_filename (DATADIR, "pixmaps", "gaim", "buttons", icon, NULL);
	debug_printf("Loading: %s\n", filename);
	image = gtk_image_new_from_file(filename);
	g_free(filename);

	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);
	return button;
}


GtkWidget *gaim_pixbuf_button_from_stock(char *text, char *icon, GaimButtonStyle style)
{
	GtkWidget *button, *image, *label, *bbox;
	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL)
			bbox = gtk_hbox_new(FALSE, 5);
	else
			bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	if (icon) {
		image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
	}
	
	if (text) {
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(bbox);
	return button;
}

GtkWidget *gaim_pixbuf_button(char *text, char *iconfile, GaimButtonStyle style)
{
	GtkWidget *button, *image, *label, *bbox;
	button = gtk_button_new();

	if (style == GAIM_BUTTON_HORIZONTAL)
			bbox = gtk_hbox_new(FALSE, 5);
	else
			bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	if (iconfile) {
		char *filename;
		filename = g_build_filename (DATADIR, "pixmaps", "gaim", "buttons", iconfile, NULL);
		debug_printf("Loading: %s\n", filename);
		image = gtk_image_new_from_file(filename);
		gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);
		g_free(filename);
	}
	
	if (text) {
		label = gtk_label_new(NULL);
		gtk_label_set_text_with_mnemonic(GTK_LABEL(label), text);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	}
	gtk_widget_show_all(bbox);
	return button;
}

GtkWidget *picture_button(GtkWidget *window, char *text, char **xpm)
{
	GtkWidget *button;
	GtkWidget *button_box, *button_box_2, *button_box_3;
	GtkWidget *label;
	GdkBitmap *mask;
	GdkPixmap *pm;
	GtkWidget *pixmap;

	button = gtk_button_new();

	button_box = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(button), button_box);

	button_box_2 = gtk_hbox_new(FALSE, 0);
	button_box_3 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_box_2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_box_3, TRUE, TRUE, 0);
	pm = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, xpm);
	pixmap = gtk_pixmap_new(pm, mask);
	gtk_box_pack_end(GTK_BOX(button_box_2), pixmap, FALSE, FALSE, 0);

	if (text) {
		label = gtk_label_new(text);
		gtk_box_pack_start(GTK_BOX(button_box_3), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}

	gtk_widget_show(pixmap);
	gtk_widget_show(button_box_2);
	gtk_widget_show(button_box_3);
	gtk_widget_show(button_box);

/* this causes clipping on lots of buttons with long text */
/*  gtk_widget_set_usize(button, 75, 30);*/
	gtk_widget_show(button);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);

	return button;
}

static GtkTooltips *button_tips = NULL;
GtkWidget *picture_button2(GtkWidget *window, char *text, char **xpm, short dispstyle)
{
	GtkWidget *button;
	GtkWidget *button_box, *button_box_2;
	GdkBitmap *mask;
	GdkPixmap *pm;
	GtkWidget *pixmap;
	GtkWidget *label;

	if (!button_tips)
		button_tips = gtk_tooltips_new();

	button = gtk_button_new();

	button_box = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(button), button_box);

	button_box_2 = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(button_box), button_box_2, TRUE, TRUE, 0);
	gtk_widget_show(button_box_2);
	gtk_widget_show(button_box);
	if (dispstyle == 2 || dispstyle == 0) {
		pm = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, xpm);
		pixmap = gtk_pixmap_new(pm, mask);
		gtk_box_pack_start(GTK_BOX(button_box_2), pixmap, FALSE, FALSE, 0);

		gtk_widget_show(pixmap);

		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(mask);
	}

	if (dispstyle == 2 || dispstyle == 1) {
		label = gtk_label_new(text);
		gtk_widget_show(label);
		gtk_box_pack_end(GTK_BOX(button_box_2), label, FALSE, FALSE, 0);
	}

	gtk_tooltips_set_tip(button_tips, button, text, "Gaim");
	gtk_widget_show(button);
	return button;
}

int file_is_dir(const char *path, GtkWidget *w)
{
	struct stat st;
	char *name;

	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != '/') {
			name = g_strconcat(path, "/", NULL);
		} else {
			name = g_strdup(path);
		}
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(w), name);
		g_free(name);
		return 1;
	}

	return 0;
}

/*------------------------------------------------------------------------*/
/*  The dialog for setting V-Card info                                    */
/*------------------------------------------------------------------------*/
/*
 * There are actually two "chunks" of code following:  generic "multi-entry dialog"
 * support and V-Card dialog specific support.
 *
 * At first blush, this may seem like an unnecessary duplication of effort given
 * that a "set dir info" dialog already exists.  However, this is not so because:
 *
 *	1. V-Cards can have a lot more data in them than what the current
 *	   "set dir" dialog supports.
 *
 *	2. V-Card data, at least with respect to Jabber, is currently in a
 *	   state of flux.  As the data and format changes, all that need be
 *	   changed with the V-Card support I've written is the "template"
 *	   data.
 *
 *	3. The "multi entry dialog" support itself was originally written
 *	   to support Jabber server user registration (TBD).  A "dynamically
 *	   configurable" multi-entry dialog is needed for that, as different
 *	   servers may require different registration information.  It just
 *	   turned out to be well-suited to adding V-Card setting support, as
 *	   well :-).
 *
 * TBD: Add check-box support to the generic multi-entry dialog support so that
 *      it can be used to "replace" the "set dir info" support?
 *
 *      Multiple-language support.  Currently Not In There.  I think this should
 *      be easy.  Note that when it's added: if anybody saved their data in
 *      English, it'll be lost when MLS is added and they'll have to re-enter it.
 *
 * More "TBDs" noted in the code.
 */


/*------------------------------------*/
/* generic multi-entry dialog support */
/*------------------------------------*/

/*
 * Print all multi-entry items
 *
 * Note: Simply a debug helper
 */
void multi_entry_item_print_all(const GSList *list) {

	int cnt = 0;

	/* While there's something to print... */
	while(list != NULL) {
		fprintf(stderr, "label %2d: \"%s\"", ++cnt, ((MultiEntryData *) (list->data))->label);
		if(((MultiEntryData *) (list->data))->text != NULL) {
			fprintf(stderr, ", text: \"%s\"", ((MultiEntryData *) (list->data))->text);
		}
		fputs("\n", stderr);
		list = list->next;
	}
}

/*
 * Print all multi-text items
 *
 * Note: Simply a debug helper
 */
void multi_text_item_print_all(const GSList *list) {

	int cnt = 0;

	/* While there's something to print... */
	while(list != NULL) {
		fprintf(stderr, "label %2d: \"%s\"", ++cnt, ((MultiTextData *) (list->data))->label);
		if(((MultiTextData *) (list->data))->text != NULL) {
			fprintf(stderr, ", text: \"%s\"", ((MultiTextData *) (list->data))->text);
		}
		fputs("\n", stderr);
		list = list->next;
	}
}


/*
 * Free all multi-entry item allocs and NULL the list pointer
 */
void multi_entry_items_free_all(GSList **list)
{

	GSList *next = *list;
	MultiEntryData *data;

	/* While there's something to free() ... */
	while(next != NULL) {
		data = (MultiEntryData *) next->data;
		g_free(data->label);
		g_free(data->text);
		g_free(data);
		next = next->next;
	}
	g_slist_free(*list);
	*list = NULL;
}

/*
 * Free all multi-text item allocs and NULL the list pointer
 */
void multi_text_items_free_all(GSList **list)
{

	GSList *next = *list;
	MultiTextData *data;

	/* While there's something to free() ... */
	while(next != NULL) {
		data = (MultiTextData *) next->data;
		g_free(data->label);
		g_free(data->text);
		g_free(data);
		next = next->next;
	}
	g_slist_free(*list);
	*list = NULL;
}

/*
 * See if a MultiEntryData item contains a given label
 *
 * See: glib docs for g_slist_compare_custom() for details
 */
static gint multi_entry_data_label_compare(gconstpointer data, gconstpointer label)
{
	return(strcmp(((MultiEntryData *) (data))->label, (char *) label));
}

/*
 * Add a new multi-entry item to list
 *
 * If adding to existing list: will search the list for existence of 
 * "label" and change/create "text" entry if necessary.
 */

MultiEntryData *multi_entry_list_update(GSList **list, const char *label, const char *text, int add_it)
{
	GSList *found;
	MultiEntryData *data;

	if((found = g_slist_find_custom(*list, (void *)label, multi_entry_data_label_compare)) == NULL) {
		if(add_it) {
			data = (MultiEntryData *) g_slist_last(*list =
				g_slist_append(*list, g_malloc(sizeof(MultiEntryData))))->data;
			data->label = strcpy(g_malloc(strlen(label) +1), label);
			data->text = NULL;
			/*
			 * default to setting "visible" and editable to TRUE - they can be
			 * overridden later, of course.
			 */
			data->visible  = TRUE;
			data->editable = TRUE;
		} else {
			data = NULL;
		}
	} else {
		data = found->data;
	}

	if(data != NULL && text != NULL && text[0] != '\0') {
		if(data->text == NULL) {
			data->text = g_malloc(strlen(text) + 1);
		} else {
			data->text = g_realloc(data->text, strlen(text) + 1);
		}
		strcpy(data->text, text);
	}

	return(data);
}

/*
 * See if a MultiTextData item contains a given label
 *
 * See: glib docs for g_slist_compare_custom() for details
 */
static gint multi_text_data_label_compare(gconstpointer data, gconstpointer label)
{
	return(strcmp(((MultiTextData *) (data))->label, (char *) label));
}

/*
 * Add a new multi-text item to list
 *
 * If adding to existing list: will search the list for existence of 
 * "label" and change/create "text" text if necessary.
 */

MultiTextData *multi_text_list_update(GSList **list, const char *label, const char *text, int add_it)
{
	GSList *found;
	MultiTextData *data;

	if((found = g_slist_find_custom(*list, (void *)label, multi_text_data_label_compare)) == NULL) {
		if(add_it) {
			data = (MultiTextData *) g_slist_last(*list =
				g_slist_append(*list, g_malloc(sizeof(MultiTextData))))->data;
			data->label = strcpy(g_malloc(strlen(label) +1), label);
			data->text = NULL;
		} else {
			data = NULL;
		}
	} else {
		data = found->data;
	}

	if(data != NULL && text != NULL && text[0] != '\0') {
		if(data->text == NULL) {
			data->text = g_malloc(strlen(text) + 1);
		} else {
			data->text = g_realloc(data->text, strlen(text) + 1);
		}
		strcpy(data->text, text);
	}

	return(data);
}

/*
 * Free-up the multi-entry item list and the MultiEntryDlg
 * struct alloc.
 */
void multi_entry_free(struct multi_entry_dlg *b)
{
	multi_entry_items_free_all(&(b->multi_entry_items));
	multi_text_items_free_all(&(b->multi_text_items));
	g_free(b->instructions->text);
	g_free(b->instructions);
	g_free(b->entries_title);
	g_free(b);
}

/*
 * Multi-Entry dialog "destroyed" catcher
 *
 * Free-up the multi-entry item list, destroy the dialog widget
 * and free the MultiEntryDlg struct alloc.
 *
 */
void multi_entry_dialog_destroy(GtkWidget *widget, gpointer  data)
{
	MultiEntryDlg *b = data;

	multi_entry_free(b);
}

/*
 * Show/Re-show instructions
 */
void re_show_multi_entry_instr(MultiInstrData *instructions)
{
	if(instructions->label != NULL) {
		if(instructions->text == NULL) {
			gtk_widget_hide(instructions->label);
		} else {
			gtk_label_set_text(GTK_LABEL (instructions->label), instructions->text);
			gtk_widget_show(instructions->label);
		}
	}
}

/*
 * Show/Re-show entry boxes
 */
void re_show_multi_entry_entries(GtkWidget **entries_table,
				 GtkWidget *entries_frame,
				 GSList *multi_entry_items)
{
	GtkWidget *label;
	GSList *multi_entry;
	MultiEntryData *med;
	int rows, row_num, col_num, col_offset;
	int cols = 1;

	/* Figure-out number of rows needed for table */
	if((rows = g_slist_length(multi_entry_items)) > 9) {
		rows /= 2;
		++cols;
	}

	if(*entries_table != NULL) {
		gtk_widget_destroy(GTK_WIDGET (*entries_table));
	}
	*entries_table = gtk_table_new(rows, 3 * cols, FALSE);
	gtk_container_add(GTK_CONTAINER (entries_frame), *entries_table);

	for(col_num = 0, multi_entry = multi_entry_items; col_num < cols && multi_entry != NULL;
			++col_num) {
		col_offset = col_num * 3;
		for(row_num = 0; row_num < rows && multi_entry != NULL;
				++row_num, multi_entry = multi_entry->next) {

			med = (MultiEntryData *) multi_entry->data;

			label = gtk_label_new(med->label);
			gtk_misc_set_alignment(GTK_MISC(label), (gfloat) 1.0, (gfloat) 0.5);
			gtk_table_attach_defaults(GTK_TABLE (*entries_table), label,
				    col_offset, 1 + col_offset, row_num, row_num +1);
			gtk_widget_show(label);

			label = gtk_label_new(": ");
			gtk_misc_set_alignment(GTK_MISC(label), (gfloat) 0.0, (gfloat) 0.5);
			gtk_table_attach_defaults(GTK_TABLE (*entries_table), label,
				1 + col_offset, 2 + col_offset, row_num, row_num +1);
			gtk_widget_show(label);

			med->widget = gtk_entry_new_with_max_length(50);
			if(med->text != NULL) {
				gtk_entry_set_text(GTK_ENTRY (med->widget), med->text);
			}
			gtk_entry_set_visibility(GTK_ENTRY (med->widget), med->visible);
			gtk_entry_set_editable(GTK_ENTRY (med->widget), med->editable);
			gtk_table_attach(GTK_TABLE (*entries_table), med->widget,
				2 + col_offset, 3 + col_offset, row_num, row_num +1,
				GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 5, 0);
			gtk_widget_show(med->widget);
		}
	}

	gtk_widget_show(*entries_table);
}

/*
 * Show/Re-show textboxes
 */
void re_show_multi_entry_textboxes(GtkWidget **texts_ibox,
				   GtkWidget *texts_obox,
				   GSList *multi_text_items)
{
	GSList *multi_text;
	MultiTextData *mtd;
        GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vscrollbar;

	if(*texts_ibox != NULL) {
		gtk_widget_destroy(GTK_WIDGET (*texts_ibox));
	}
	*texts_ibox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER (texts_obox), *texts_ibox);

	for(multi_text = multi_text_items; multi_text != NULL; multi_text = multi_text->next) {
		mtd = (MultiTextData *) multi_text->data;
		frame = gtk_frame_new(mtd->label);
		gtk_container_add(GTK_CONTAINER (*texts_ibox), frame);
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER (frame), hbox);
		mtd->textbox = gtk_text_new(NULL, NULL);
		gtk_text_set_editable(GTK_TEXT(mtd->textbox), TRUE);
		gtk_text_set_word_wrap(GTK_TEXT(mtd->textbox), TRUE);
		gtk_widget_set_usize(mtd->textbox, 300, 100);
		gtk_text_insert(GTK_TEXT(mtd->textbox), NULL, NULL, NULL, mtd->text?mtd->text:"", -1);
		gtk_box_pack_start(GTK_BOX (hbox), mtd->textbox, FALSE, FALSE, 0);
		vscrollbar = gtk_vscrollbar_new (GTK_TEXT(mtd->textbox)->vadj);
		gtk_box_pack_start(GTK_BOX (hbox), vscrollbar, FALSE, FALSE, 0);
		gtk_widget_show(mtd->textbox);
		gtk_widget_show (vscrollbar);
		gtk_widget_show(hbox);
		gtk_widget_show(frame);
	}

	gtk_widget_show(*texts_ibox);
}

/*
 *  Create and initialize a new Multi-Entry Dialog struct
 */
MultiEntryDlg *multi_entry_dialog_new()
{
	MultiEntryDlg *b = g_new0(MultiEntryDlg, 1);
	b->instructions = g_new0(MultiInstrData, 1);
	b->multi_entry_items = NULL;
	b->multi_text_items = NULL;
	return(b);
}

/*
 * Instantiate a new multi-entry dialog
 *
 * data == pointer to MultiEntryDlg with the following
 *         initialized:
 *
 *           role
 *           title
 *	     user
 *           multi_entry_items - pointers to MultiEntryData list
 *	       and MultiTextData list
 *           instructions (optional)
 *           ok function pointer
 *           cancel function pointer (actually used to set
 *             window destroy signal--cancel asserts destroy)
 *
 *         sets the following in the MultiEntryDialog struct:
 *
 *           window
 */
void show_multi_entry_dialog(gpointer data)
{
	GtkWidget *vbox, *hbox;
	GtkWidget *button;
	MultiEntryDlg *b = data;

	GAIM_DIALOG(b->window);
	gtk_container_set_border_width(GTK_CONTAINER(b->window), 5);
	gtk_window_set_role(GTK_WINDOW(b->window), b->role);
	gtk_window_set_title(GTK_WINDOW (b->window), b->title);

	/* Clean up if user dismisses window via window manager! */
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(b->cancel), (gpointer) b);
	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER (b->window), vbox);

	b->instructions->label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL (b->instructions->label), TRUE);
	gtk_box_pack_start(GTK_BOX (vbox), b->instructions->label, TRUE, TRUE, 5);
	re_show_multi_entry_instr(b->instructions);

	b->entries_frame = gtk_frame_new(b->entries_title);
	gtk_box_pack_start(GTK_BOX (vbox), b->entries_frame, TRUE, TRUE, 5);
	gtk_widget_show(b->entries_frame);
	b->entries_table = NULL;
	re_show_multi_entry_entries(&(b->entries_table), b->entries_frame, b->multi_entry_items);

	b->texts_obox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), b->texts_obox, TRUE, TRUE, 5);
	gtk_widget_show(b->texts_obox);
	b->texts_ibox = NULL;
	re_show_multi_entry_textboxes(&(b->texts_ibox), b->texts_obox, b->multi_text_items);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(b->window, _("Cancel"), cancel_xpm);

	/* Let "destroy handling" (set above) handle cleanup */
	gtk_signal_connect_object(GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (b->window));
	gtk_box_pack_end(GTK_BOX (hbox), button, FALSE, FALSE, 5);
	gtk_widget_show(button);

	button = picture_button(b->window, _("Save"), save_xpm);
	gtk_signal_connect(GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (b->ok), (gpointer) b);
	gtk_box_pack_end(GTK_BOX (hbox), button, FALSE, FALSE, 5);
	gtk_widget_show(button);

	gtk_widget_show(vbox);
	gtk_widget_show(b->window);
}


/*------------------------------------*/
/* V-Card dialog specific support     */
/*------------------------------------*/

/*
 * V-Card "set info" dialog "Save" clicked
 *
 * Copy data from GTK+ dialogs into GSLists, call protocol-specific
 * formatter and save the user info data.
 */
void set_vcard_dialog_ok_clicked(GtkWidget *widget, gpointer  data)
{
	MultiEntryDlg *b = (MultiEntryDlg *) data;
	struct gaim_connection *gc;
	gchar *tmp;
	GSList *list;

	for(list = b->multi_entry_items; list != NULL; list = list->next) {
		if(((MultiEntryData *) list->data)->text != NULL) {
			g_free(((MultiEntryData *) list->data)->text);
		}
		((MultiEntryData *) list->data)->text =
			g_strdup(gtk_entry_get_text(GTK_ENTRY(((MultiEntryData *) list->data)->widget)));
	}

	for(list = b->multi_text_items; list != NULL; list = list->next) {
		if(((MultiTextData *) list->data)->text != NULL) {
			g_free(((MultiTextData *) list->data)->text);
		}
		((MultiTextData *) list->data)->text =
			gtk_editable_get_chars((GtkEditable *) (((MultiTextData *) list->data)->textbox),
				0, -1);
	}


	tmp = b->custom(b);

	/*
	 * Set the user info and (possibly) send to the server
	 */
        if (b->user) {
                strncpy(b->user->user_info, tmp, sizeof b->user->user_info);
                gc = b->user->gc;

                save_prefs();

                if (gc)
                        serv_set_info(gc, b->user->user_info);
        }

	g_free(tmp);

	/* Let multi-edit dialog window "destroy" event catching handle remaining cleanup */
	gtk_widget_destroy(GTK_WIDGET (b->window));
}

/*
 * Instantiate a v-card dialog
 */
void show_set_vcard(MultiEntryDlg *b)
{
	b->ok = set_vcard_dialog_ok_clicked;
	b->cancel = multi_entry_dialog_destroy;

	show_multi_entry_dialog(b);
}


/*------------------------------------------------------------------------*/
/*  End dialog for setting v-card info                                    */
/*------------------------------------------------------------------------*/


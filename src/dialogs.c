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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "gtkimhtml.h"
#include "prpl.h"
#ifdef USE_APPLET
#include "applet.h"
#endif

#include "pixmaps/gnome_preferences.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/save.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/add.xpm"
#include "pixmaps/warn.xpm"
#include "pixmaps/close.xpm"

#include "pixmaps/angel.xpm"
#include "pixmaps/bigsmile.xpm"
#include "pixmaps/burp.xpm"
#include "pixmaps/crossedlips.xpm"
#include "pixmaps/cry.xpm"
#include "pixmaps/embarrassed.xpm"
#include "pixmaps/kiss.xpm"
#include "pixmaps/moneymouth.xpm"
#include "pixmaps/sad.xpm"
#include "pixmaps/scream.xpm"
#include "pixmaps/smile.xpm"
#include "pixmaps/smile8.xpm"
#include "pixmaps/think.xpm"
#include "pixmaps/tongue.xpm"
#include "pixmaps/wink.xpm"
#include "pixmaps/yell.xpm"

#include "pixmaps/aimicon.xpm"

#include "pixmaps/aol_icon.xpm"
#include "pixmaps/free_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#include "pixmaps/admin_icon.xpm"
#include "pixmaps/ab.xpm"
#include "pixmaps/wireless_icon.xpm"
#include "pixmaps/gnomeicu-online.xpm"

#define PATHSIZE 1024

int smiley_array[FACE_TOTAL];
char *current_smiley;
GdkColor bgcolor;
GdkColor fgcolor;

static GtkWidget *imdialog = NULL;	/*I only want ONE of these :) */
static GtkWidget *infodialog = NULL;
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
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->wood), FALSE);
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

	if (dest == infodialog)
		infodialog = NULL;

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

	if (infodialog) {
		destroy_dialog(NULL, infodialog);
		infodialog = NULL;
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

static void do_warn(GtkWidget *widget, struct warning *w)
{
	serv_warn(w->gc, w->who, (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->anon))) ? 1 : 0);

	destroy_dialog(NULL, w->window);
}

static void free_warn_data(GtkObject *obj, struct warning *w)
{
	g_free(w);
}

void show_warn_dialog(struct gaim_connection *gc, char *who)
{
	GtkWidget *cancel;
	GtkWidget *warn;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	char buf[128];

	struct warning *w = g_new0(struct warning, 1);
	w->who = who;
	w->gc = gc;

	GAIM_DIALOG(w->window);
	dialogwindows = g_list_prepend(dialogwindows, w->window);
	gtk_window_set_wmclass(GTK_WINDOW(w->window), "warning", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(w->window), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(w->window), _("Gaim - Warn user?"));
	gtk_container_set_border_width(GTK_CONTAINER(w->window), 5);
	gtk_signal_connect(GTK_OBJECT(w->window), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), w->window);
	gtk_signal_connect(GTK_OBJECT(w->window), "delete_event", GTK_SIGNAL_FUNC(free_warn_data), w);
	gtk_widget_realize(w->window);
	aol_icon(w->window->window);

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(w->window), fbox);
	gtk_widget_show(fbox);

	frame = gtk_frame_new(_("Warn"));
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	g_snprintf(buf, 127, _("Do you really want to warn %s?"), who);
	label = gtk_label_new(buf);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
	gtk_widget_show(label);

	w->anon = gtk_check_button_new_with_label(_("Warn anonymously?"));
	gtk_box_pack_start(GTK_BOX(vbox), w->anon, TRUE, TRUE, 5);
	gtk_widget_show(w->anon);

	label = gtk_label_new(_("Anonymous warnings are less harsh."));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
	gtk_widget_show(label);

	bbox = gtk_hbox_new(TRUE, 10);
	gtk_box_pack_start(GTK_BOX(fbox), bbox, FALSE, FALSE, 5);
	gtk_widget_show(bbox);

	cancel = picture_button(w->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), w->window);

	warn = picture_button(w->window, _("Warn"), warn_xpm);
	gtk_box_pack_end(GTK_BOX(bbox), warn, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(warn), "clicked", GTK_SIGNAL_FUNC(do_warn), w);

	gtk_widget_show(w->window);
}

void do_remove_buddy(GtkWidget *w, struct buddy *b)
{
	struct group *g = find_group_by_buddy(b->gc, b->name);
	struct gaim_connection *gc = b->gc;
	struct conversation *cv;

	if (!b)
		return;

	debug_printf(_("Removing '%s' from buddylist.\n"), b->name);
	serv_remove_buddy(b->gc, b->name, g->name);
	remove_buddy(gc, g, b);
	do_export(gc);

	cv = find_conversation(b->name);

	if (cv)
		update_convo_add_button(cv);

}

void show_confirm_del(struct gaim_connection *gc, gchar *name)
{
	struct confirm_del *b = g_new0(struct confirm_del, 1);
	struct buddy *bd;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *fbox;
	GtkWidget *frame;
	gchar tmp[2048];

	GAIM_DIALOG(b->window);
	dialogwindows = g_list_prepend(dialogwindows, b->window);

	g_snprintf(tmp, sizeof(tmp), _("Gaim - Remove %s?"), name);
	gtk_window_set_title(GTK_WINDOW(b->window), tmp);
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "confirm_del", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_signal_connect(GTK_OBJECT(b->window), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	gtk_widget_realize(b->window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
	gtk_widget_show(vbox);

	frame = gtk_frame_new(_("Remove Buddy"));
	gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_widget_show(frame);	

	fbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
	gtk_widget_show(fbox);

	g_snprintf(tmp, sizeof(tmp), _("You are about to remove '%s' from\nyour buddylist. Do you want to continue?"), name);
	b->label = gtk_label_new(tmp);
	gtk_misc_set_alignment(GTK_MISC(b->label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(b->label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(fbox), b->label, FALSE, FALSE, 0);
	gtk_widget_show(b->label);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 0);
	gtk_widget_show(hbox);

	b->ok = picture_button(b->window, _("Accept"), ok_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), b->ok, FALSE, FALSE, 5);

	bd = find_buddy(gc, name);

	if (bd)
		gtk_signal_connect(GTK_OBJECT(b->ok), "clicked", GTK_SIGNAL_FUNC(do_remove_buddy), bd);
	
	gtk_signal_connect(GTK_OBJECT(b->ok), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_box_pack_start(GTK_BOX(hbox), b->cancel, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	
	gtk_widget_show(b->window);	
}



/*------------------------------------------------------------------------*/
/*  The dialog for getting an error                                       */
/*------------------------------------------------------------------------*/

GtkWidget *do_error_dialog(char *message, char *title)
{
	GtkWidget *d;
	GtkWidget *label;
	GtkWidget *close;


	d = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(d), FALSE, FALSE, TRUE);
	gtk_widget_realize(d);
	label = gtk_label_new(message);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox), label, FALSE, FALSE, 5);

	close = picture_button(d, _("Close"), cancel_xpm);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area), close, FALSE, FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(d), 5);
	gtk_window_set_title(GTK_WINDOW(d), title);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), d);
	aol_icon(d->window);

	gtk_widget_show(d);
	gtk_widget_grab_focus(close);
	return d;
}



static void do_im(GtkWidget *widget, GtkWidget *imentry)
{
	char *who;
	struct conversation *c;

	who = g_strdup(gtk_entry_get_text(GTK_ENTRY(imentry)));
	destroy_dialog(NULL, imdialog);
	imdialog = NULL;

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
	g_free(who);
}

static void do_info(GtkWidget *widget, GtkWidget *infoentry)
{
	char *who;

	who = g_strdup(normalize(gtk_entry_get_text(GTK_ENTRY(infoentry))));
	destroy_dialog(NULL, infodialog);
	infodialog = NULL;

	if (!g_strcasecmp(who, "")) {
		g_free(who);
		return;
	}

	/* what do we want to do about this case? */
	if (connections)
		serv_get_info(connections->data, who);

	g_free(who);
}

void show_ee_dialog(int ee)
{
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *eedialog;

	GAIM_DIALOG(eedialog);
	gtk_widget_realize(eedialog);
	ok = picture_button(eedialog, _("OK"), ok_xpm);
	box = gtk_vbox_new(FALSE, 10);

	if (ee == 0)
		label = gtk_label_new("Amazing!  Simply Amazing!");
	else if (ee == 1)
		label = gtk_label_new("Pimpin\' Penguin Style! *Waddle Waddle*");
	else if (ee == 2)
		label = gtk_label_new("You should be me.  I'm so cute!");
	else if (ee == 3)
		label = gtk_label_new("Now that's what I like!");
	else if (ee == 4)
		label = gtk_label_new("Ahh, and excellent choice!");
	else  if (ee == 5)
		label = gtk_label_new("Everytime you click my name, an angel gets its wings.");
	else 
		label = gtk_label_new("This sunflower seed taste like pizza.");

	gtk_widget_show(label);
	gtk_widget_show(ok);

	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 10);
	gtk_box_pack_start(GTK_BOX(box), ok, FALSE, FALSE, 10);

	gtk_widget_show(box);

	gtk_container_add(GTK_CONTAINER(eedialog), box);
	gtk_window_set_title(GTK_WINDOW(eedialog), "Gaim - SUPRISE!");

	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), eedialog);
	gtk_widget_realize(eedialog);
	aol_icon(eedialog->window);

	gtk_widget_show(eedialog);
}

void show_im_dialog()
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *imentry;
	GtkWidget *label;

	if (!imdialog) {
		GAIM_DIALOG(imdialog);
		gtk_window_set_wmclass(GTK_WINDOW(imdialog), "imdialog", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(imdialog), FALSE, TRUE, TRUE);
		gtk_window_set_title(GTK_WINDOW(imdialog), _("Gaim - IM user"));
		gtk_signal_connect(GTK_OBJECT(imdialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);
		gtk_widget_realize(imdialog);
		aol_icon(imdialog->window);

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(imdialog), mainbox);

		frame = gtk_frame_new(_("Send Instant Message"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		label = gtk_label_new(_("IM who:"));
		gtk_box_pack_start(GTK_BOX(fbox), label, FALSE, FALSE, 0);

		imentry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(fbox), imentry, TRUE, TRUE, 0);
		gtk_signal_connect(GTK_OBJECT(imentry), "activate", GTK_SIGNAL_FUNC(do_im), imentry);
		gtk_widget_grab_focus(imentry);

		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		button = picture_button(imdialog, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);

		button = picture_button(imdialog, _("OK"), ok_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_im), imentry);
	}

	gtk_widget_show_all(imdialog);
}

void show_info_dialog()
{
	GtkWidget *mainbox;
	GtkWidget *frame;
	GtkWidget *fbox;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *infoentry;
	GtkWidget *label;

	if (!infodialog) {

		GAIM_DIALOG(infodialog);
		gtk_window_set_wmclass(GTK_WINDOW(infodialog), "infodialog", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(infodialog), FALSE, TRUE, TRUE);
		gtk_widget_realize(infodialog);

		mainbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(mainbox), 5);
		gtk_container_add(GTK_CONTAINER(infodialog), mainbox);

		frame = gtk_frame_new(_("Get User Info"));
		gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

		fbox = gtk_hbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(fbox), 5);
		gtk_container_add(GTK_CONTAINER(frame), fbox);

		label = gtk_label_new(_("User:"));
		gtk_box_pack_start(GTK_BOX(fbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);

		infoentry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(fbox), infoentry, TRUE, TRUE, 0);

		/* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(infoentry), "activate",
				   GTK_SIGNAL_FUNC(do_info), infoentry);
		gtk_signal_connect(GTK_OBJECT(infodialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), infodialog);

		/* Buttons */
		bbox = gtk_hbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(mainbox), bbox, FALSE, FALSE, 0);

		button = picture_button(infodialog, _("Cancel"), cancel_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(destroy_dialog), infodialog);

		button = picture_button(infodialog, _("OK"), ok_xpm);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_info), infoentry);

		/* Finish up */
		gtk_window_set_title(GTK_WINDOW(infodialog), _("Gaim - Get User Info"));
		gtk_widget_grab_focus(infoentry);

		aol_icon(infodialog->window);
	}
	gtk_widget_show_all(infodialog);
}


/*------------------------------------------------------------------------*/
/*  The dialog for adding buddies                                         */
/*------------------------------------------------------------------------*/

extern void add_callback(GtkWidget *, struct conversation *);

void do_add_buddy(GtkWidget *w, struct addbuddy *a)
{
	char *grp, *who, *whoalias;
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

void do_add_group(GtkWidget *w, struct addbuddy *a)
{
	char *grp;

	grp = gtk_entry_get_text(GTK_ENTRY(a->entry));

	if (!a->gc)
		a->gc = connections->data;

	add_group(a->gc, grp);
	do_export(a->gc);

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
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *topbox;
	GtkWidget *frame;

	struct addbuddy *a = g_new0(struct addbuddy, 1);
	a->gc = gc;

	GAIM_DIALOG(a->window);
	gtk_window_set_wmclass(GTK_WINDOW(a->window), "add_group", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(a->window), FALSE, FALSE, TRUE);
	gtk_widget_realize(a->window);
	dialogwindows = g_list_prepend(dialogwindows, a->window);

	bbox = gtk_hbox_new(TRUE, 10);
	topbox = gtk_hbox_new(FALSE, 5);
	vbox = gtk_vbox_new(FALSE, 5);

	a->entry = gtk_entry_new();
	/* Put the buttons in the box */

	add = picture_button(a->window, _("Add"), add_xpm);

	cancel = picture_button(a->window, _("Cancel"), cancel_xpm);

	gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Add Group"));

	label = gtk_label_new(_("Group"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(topbox), a->entry, FALSE, FALSE, 5);

	/* And the boxes in the box */
	gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

	/* Handle closes right */
	gtk_signal_connect(GTK_OBJECT(a->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), a->window);
	gtk_signal_connect(GTK_OBJECT(a->window), "destroy", GTK_SIGNAL_FUNC(free_dialog), a);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), a->window);
	gtk_signal_connect(GTK_OBJECT(add), "clicked", GTK_SIGNAL_FUNC(do_add_group), a);
	gtk_signal_connect(GTK_OBJECT(a->entry), "activate", GTK_SIGNAL_FUNC(do_add_group), a);
	/* Finish up */
	gtk_widget_show(add);
	gtk_widget_show(cancel);
	gtk_widget_show(a->entry);
	gtk_widget_show(topbox);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_widget_show(frame);
	gtk_window_set_title(GTK_WINDOW(a->window), _("Gaim - Add Group"));
	gtk_window_set_focus(GTK_WINDOW(a->window), a->entry);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_container_add(GTK_CONTAINER(a->window), frame);
	gtk_container_set_border_width(GTK_CONTAINER(a->window), 5);
	gtk_widget_realize(a->window);
	aol_icon(a->window->window);

	gtk_widget_show(a->window);
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
				c->username, c->prpl->name());
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
	gtk_window_set_wmclass(GTK_WINDOW(a->window), "add_buddy", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(a->window), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(a->window), _("Gaim - Add Buddy"));

	gtk_widget_realize(a->window);
	aol_icon(a->window->window);
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
	gtk_window_set_focus(GTK_WINDOW(a->window), a->entry);
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


/*------------------------------------------------------------------------*/
/*  The dialog for new buddy pounces                                      */
/*------------------------------------------------------------------------*/


void do_new_bp(GtkWidget *w, struct addbp *b)
{
	struct buddy_pounce *bp;
	
	if (strlen(gtk_entry_get_text(GTK_ENTRY(b->nameentry))) == 0) {
		do_error_dialog(_("Please enter a buddy to pounce."), _("Buddy Pounce Error"));
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
		g_snprintf(buf, sizeof buf, "%s (%s)", a->username, (p && p->name)?p->name():"Unknown");
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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "new_bp", "Gaim");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - New Buddy Pounce"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);

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

	bbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	button = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	button = picture_button(b->window, _("OK"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_new_bp), b);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);

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
	char *first = gtk_entry_get_text(GTK_ENTRY(b->first));
	int web = GTK_TOGGLE_BUTTON(b->web)->active;
	char *middle = gtk_entry_get_text(GTK_ENTRY(b->middle));
	char *last = gtk_entry_get_text(GTK_ENTRY(b->last));
	char *maiden = gtk_entry_get_text(GTK_ENTRY(b->maiden));
	char *city = gtk_entry_get_text(GTK_ENTRY(b->city));
	char *state = gtk_entry_get_text(GTK_ENTRY(b->state));
	char *country = gtk_entry_get_text(GTK_ENTRY(b->country));

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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "set_dir", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set Dir Info"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);

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
	gchar *orig, *new1, *new2;

	orig = gtk_entry_get_text(GTK_ENTRY(b->original));
	new1 = gtk_entry_get_text(GTK_ENTRY(b->new1));
	new2 = gtk_entry_get_text(GTK_ENTRY(b->new2));

	if (g_strcasecmp(new1, new2)) {
		do_error_dialog(_("New Passwords Do Not Match"), _("Gaim - Change Password Error"));
		return;
	}

	if ((strlen(orig) < 1) || (strlen(new1) < 1) || (strlen(new2) < 1)) {
		do_error_dialog(_("Fill out all fields completely"), _("Gaim - Change Password Error"));
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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "change_passwd", "Gaim");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Password Change"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);
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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "set_info", "Gaim");
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set User Info"));
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);

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
		aol_icon(b->window->window);

		bbox = gtk_vbox_new(FALSE, 5);
		gtk_container_add(GTK_CONTAINER(b->window), bbox);

		label = gtk_label_new(_("Below are the results of your search: "));
		gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);

		sw = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 0);

		text = gtk_imhtml_new(NULL, NULL);
		b->text = text;
		gtk_container_add(GTK_CONTAINER(sw), text);
		GTK_LAYOUT(text)->hadjustment->step_increment = 10.0;
		GTK_LAYOUT(text)->vadjustment->step_increment = 10.0;
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

	char *who;
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
	aol_icon(p->window->window);

	gtk_widget_show(p->window);
}


/*------------------------------------------------------------------------*/
/*  Functions Called To Add A Log                                          */
/*------------------------------------------------------------------------*/

void cancel_log(GtkWidget *widget, struct conversation *c)
{
	if (c->wood) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->wood), FALSE);
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
	char *file;
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

		g_snprintf(buf, BUF_LEN - 1, "%s/%s.log", getenv("HOME"), normalize(c->name));
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
	char *first;
	char *middle;
	char *last;
	char *maiden;
	char *city;
	char *state;
	char *country;

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
	char *email;

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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "find_info", "Gaim");
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
	aol_icon(b->window->window);

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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), "find_email", "Gaim");
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);
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
	char *urltext, *showtext;
	open_tag = g_malloc(2048);


	urltext = gtk_entry_get_text(GTK_ENTRY(b->url));
	showtext = gtk_entry_get_text(GTK_ENTRY(b->text));

	g_snprintf(open_tag, 2048, "<A HREF=\"%s\">%s", urltext, showtext);
	surround(b->entry, open_tag, "</A>");

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
		gtk_window_set_wmclass(GTK_WINDOW(c->link_dialog), "add_link", "Gaim");
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

		gtk_widget_set_usize(b->ok, 75, 30);
		gtk_widget_set_usize(b->cancel, 75, 30);

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
		b->entry = c->entry;
		gtk_widget_realize(c->link_dialog);
		aol_icon(c->link_dialog->window);

	}

	gtk_widget_show(c->link_dialog);
	gdk_window_raise(c->link_dialog->window);
}


/*------------------------------------------------------*/
/* Color Selection Dialog                               */
/*------------------------------------------------------*/

static GtkWidget *fgcseld = NULL;
static GtkWidget *bgcseld = NULL;

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
	gdouble color[3];
	GdkColor text_color;
	struct conversation *c;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_color(colorsel, color);

	c = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	/* GTK_IS_EDITABLE(c->entry); huh? */

	text_color.red = ((guint16)(color[0] * 65535)) >> 8;
	text_color.green = ((guint16)(color[1] * 65535)) >> 8;
	text_color.blue = ((guint16)(color[2] * 65535)) >> 8;

	c->fgcol = text_color;
	c->hasfg = 1;
	g_snprintf(open_tag, 23, "<FONT COLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green,
		   text_color.blue);
	surround(c->entry, open_tag, "</FONT>");
	debug_printf("#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	g_free(open_tag);
	cancel_fgcolor(NULL, c);
}

void do_bgcolor(GtkWidget *widget, GtkColorSelection *colorsel)
{
	gdouble color[3];
	GdkColor text_color;
	struct conversation *c;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_color(colorsel, color);

	c = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	/* GTK_IS_EDITABLE(c->entry); huh? */

	text_color.red = ((guint16)(color[0] * 65535)) >> 8;
	text_color.green = ((guint16)(color[1] * 65535)) >> 8;
	text_color.blue = ((guint16)(color[2] * 65535)) >> 8;

	c->bgcol = text_color;
	c->hasbg = 1;
	g_snprintf(open_tag, 25, "<BODY BGCOLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green,
		   text_color.blue);
	surround(c->entry, open_tag, "</BODY>");
	debug_printf("#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	g_free(open_tag);
	cancel_bgcolor(NULL, c);
}

static void destroy_colorsel(GtkWidget *w, gpointer d)
{
	if (d) {
		gtk_widget_destroy(fgcseld);
		fgcseld = NULL;
	} else {
		gtk_widget_destroy(bgcseld);
		bgcseld = NULL;
	}
}

static void apply_color_dlg(GtkWidget *w, gpointer d)
{
	gdouble color[3];
	if ((int)d == 1) {
		gtk_color_selection_get_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(fgcseld)->colorsel), color);
		destroy_colorsel(NULL, (void *)1);

		fgcolor.red = ((guint16)(color[0] * 65535)) >> 8;
		fgcolor.green = ((guint16)(color[1] * 65535)) >> 8;
		fgcolor.blue = ((guint16)(color[2] * 65535)) >> 8;
		update_color(NULL, pref_fg_picture);
		update_convo_color(TRUE);
	} else {
		gtk_color_selection_get_color(GTK_COLOR_SELECTION
					      (GTK_COLOR_SELECTION_DIALOG(bgcseld)->colorsel), color);
		destroy_colorsel(NULL, (void *)0);

		bgcolor.red = ((guint16)(color[0] * 65535)) >> 8;
		bgcolor.green = ((guint16)(color[1] * 65535)) >> 8;
		bgcolor.blue = ((guint16)(color[2] * 65535)) >> 8;
		update_color(NULL, pref_bg_picture);
		update_convo_color(FALSE);
	}
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
		aol_icon(fgcseld->window);
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
		aol_icon(c->fg_color_dialog->window);
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
		aol_icon(bgcseld->window);
		gtk_widget_show(bgcseld);
		gdk_window_raise(bgcseld->window);
		return;
	}

	if (!c->bg_color_dialog) {
		bgclr[0] = (gdouble)(c->bgcol.red) / 255;
		bgclr[1] = (gdouble)(c->bgcol.green) / 255;
		bgclr[2] = (gdouble)(c->bgcol.blue) / 255;

		c->bg_color_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
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
		aol_icon(c->bg_color_dialog->window);
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
	int i, j = 0, k = 0;
	char *fontname;
	struct conversation *c = gtk_object_get_user_data(GTK_OBJECT(fontsel));

	if (c) {
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

		set_font_face(fontname, c);
	} else {
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));

		for (i = 0; i < strlen(fontname); i++) {
			if (fontname[i] == '-') {
				if (++j > 2)
					break;
			} else if (j == 2)
				fontface[k++] = fontname[i];
		}
		fontface[k] = '\0';

		g_snprintf(fontxfld, sizeof(fontxfld), "%s", fontname);
	}

	cancel_font(NULL, c);
}

static GtkWidget *fontseld;

void destroy_fontsel(GtkWidget *w, gpointer d)
{
	gtk_widget_destroy(fontseld);
	fontseld = NULL;
}

void apply_font_dlg(GtkWidget *w, GtkWidget *f)
{
	int i, j = 0, k = 0;
	char *fontname;

	fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontseld));
	destroy_fontsel(0, 0);
	for (i = 0; i < strlen(fontname); i++) {
		if (fontname[i] == '-') {
			if (++j > 2)
				break;
		} else if (j == 2)
			fontface[k++] = fontname[i];
	}
	fontface[k] = '\0';
	g_snprintf(fontxfld, sizeof(fontxfld), "%s", fontname);
	update_convo_font();
}

void show_font_dialog(struct conversation *c, GtkWidget *font)
{

	if (!font) {		/* we came from the prefs dialog */
		if (fontseld)
			return;
		fontseld = gtk_font_selection_dialog_new(_("Select Font"));
		if (fontxfld[0]) {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								fontxfld);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld),
								DEFAULT_FONT_XFLD);
		}

		gtk_object_set_user_data(GTK_OBJECT(fontseld), NULL);
		gtk_signal_connect(GTK_OBJECT(fontseld), "delete_event",
				   GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->ok_button), "clicked",
				   GTK_SIGNAL_FUNC(apply_font_dlg), NULL);
		gtk_widget_realize(fontseld);
		aol_icon(fontseld->window);
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

		if (c->fontxfld[0]) {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(c->font_dialog),
								c->fontxfld);
		} else {
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(c->font_dialog),
								DEFAULT_FONT_XFLD);
		}

		gtk_signal_connect(GTK_OBJECT(c->font_dialog), "delete_event",
				   GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->ok_button),
				   "clicked", GTK_SIGNAL_FUNC(apply_font), c->font_dialog);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->cancel_button),
				   "clicked", GTK_SIGNAL_FUNC(cancel_font), c);

		gtk_widget_realize(c->font_dialog);

		aol_icon(c->font_dialog->window);
	}
	gtk_widget_show(c->font_dialog);
	gdk_window_raise(c->font_dialog->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for import/export                                          */
/*------------------------------------------------------------------------*/

static void do_import_dialog(GtkWidget *w, struct gaim_connection *gc)
{
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(importdialog));
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
		g_snprintf(buf, sizeof buf, "%s (%s)", c->username, c->prpl->name());
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

		g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("HOME"));

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
#ifdef USE_APPLET
		char *awayname = g_strdup_printf("away/%s", ca->mess->name);
		applet_widget_unregister_callback(APPLET_WIDGET(applet), awayname);
		g_free(awayname);
#endif
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
		do_error_dialog(_("You cannot create an away message with a blank title"),
				_("Gaim - Error"));
		return 0;
	}

	if ((gtk_text_get_length(GTK_TEXT(ca->text)) == 0) && (type <= 1)) {
		/* We shouldn't allow a blank message */
		do_error_dialog(_("You cannot create an empty away message"), _("Gaim - Error"));
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
	gtk_window_set_wmclass(GTK_WINDOW(ca->window), "away_mess", "Gaim");
	gtk_window_set_title(GTK_WINDOW(ca->window), _("Gaim - New away message"));
	gtk_signal_connect(GTK_OBJECT(ca->window), "delete_event",
			   GTK_SIGNAL_FUNC(destroy_dialog), ca->window);
	gtk_widget_realize(ca->window);
	aol_icon(ca->window->window);

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

	if (dummy && GTK_LIST(prefs_away_list)->selection) {
		GtkWidget *item = GTK_LIST(prefs_away_list)->selection->data;
		struct away_message *amt = gtk_object_get_user_data(GTK_OBJECT(item));
		int pos = 0;
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
	char *smiley_text;
	int i;

	smiley_text = strdup(current_smiley);

	/* surround(c->entry, smiley_text, ""); */

	if (GTK_OLD_EDITABLE(c->entry)->has_selection) {
		int finish = GTK_OLD_EDITABLE(c->entry)->selection_end_pos;
		gtk_editable_insert_text(GTK_EDITABLE(c->entry),
					 smiley_text, strlen(smiley_text), &finish);
	} else {
		int pos = GTK_OLD_EDITABLE(c->entry)->current_pos;
		gtk_editable_insert_text(GTK_EDITABLE(c->entry), smiley_text, strlen(smiley_text), &pos);
	}

	g_free(smiley_text);

	close_smiley_dialog(NULL, c);

	return;
}

static void toolbar_add_smiley(struct conversation *c, GtkWidget *bar, char **xpm, GtkWidget *win,
			       char *face)
{
	GtkWidget *tpm;
	GdkBitmap *mask;
	GdkPixmap *dpm;
	GtkWidget *button;

	dpm = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, xpm);
	tpm = gtk_pixmap_new(dpm, mask);
	gtk_widget_show(tpm);
	gdk_pixmap_unref(dpm);
	gdk_bitmap_unref(mask);
	button =
	    gtk_toolbar_append_element(GTK_TOOLBAR(bar), GTK_TOOLBAR_CHILD_BUTTON, NULL, NULL, NULL,
				       NULL, tpm, GTK_SIGNAL_FUNC(set_smiley), (char *)face);
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
	GSList *smilies;
	int smiley_count = 0;
	int total_count = 0;

	if (c->smiley_dialog)
		return;

	win = c->window;

	GAIM_DIALOG(dialog);
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	gtk_window_set_wmclass(GTK_WINDOW(dialog), "smiley_dialog", "Gaim");
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

	/* setup boxes */
	vbox = gtk_vbox_new(TRUE, 5);
	bbox = gtk_hbox_new(FALSE, 5);


	/* setup buttons */

	/* pack buttons */
	if (c->gc->prpl->smiley_list == NULL) {
		smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
		gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
		
		toolbar_add_smiley(c, smiley_box, angel_xpm, win, "O:-)");
		toolbar_add_smiley(c, smiley_box, bigsmile_xpm, win, ":-D");
		toolbar_add_smiley(c, smiley_box, burp_xpm, win, ":-!");
		toolbar_add_smiley(c, smiley_box, crossedlips_xpm, win, ":-X");

		smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
		gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);

		toolbar_add_smiley(c, smiley_box, cry_xpm, win, ":'(");
		toolbar_add_smiley(c, smiley_box, embarrassed_xpm, win, ":-[");
		toolbar_add_smiley(c, smiley_box, kiss_xpm, win, ":-*");
		toolbar_add_smiley(c, smiley_box, moneymouth_xpm, win, ":-$");

		smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
		gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);

		toolbar_add_smiley(c, smiley_box, sad_xpm, win, ":-(");
		toolbar_add_smiley(c, smiley_box, scream_xpm, win, "=-O");
		toolbar_add_smiley(c, smiley_box, smile_xpm, win, ":-)");
		toolbar_add_smiley(c, smiley_box, smile8_xpm, win, "8-)");

		smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
		gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);

		toolbar_add_smiley(c, smiley_box, think_xpm, win, ":-/");
		toolbar_add_smiley(c, smiley_box, tongue_xpm, win, ":-P");
		toolbar_add_smiley(c, smiley_box, wink_xpm, win, ";-)");
		toolbar_add_smiley(c, smiley_box, yell_xpm, win, ">:o");
	} else {

		smilies = c->gc->prpl->smiley_list();

		while (smilies) {
			struct _prpl_smiley *smile =
				(struct _prpl_smiley *)smilies->data;

			if (smile->show) {
				total_count++;
			}

			smilies = g_slist_next(smilies);
		}

		smilies = c->gc->prpl->smiley_list();

		smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
		gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);

		while (smilies) {
			struct _prpl_smiley *smile =
				(struct _prpl_smiley *)smilies->data;

			if (smile->show) {

				if ((!(smiley_count % ((int)(sqrt(total_count))))) && (smiley_count > 0)) {
					smiley_box = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
					gtk_box_pack_start(GTK_BOX(vbox), smiley_box, TRUE, TRUE, 0);
				}
			
				toolbar_add_smiley(c, smiley_box, smile->xpm, win, smile->key);
				smiley_count++;
			}

			smilies = g_slist_next(smilies);

		}
	}
	

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
	char *al = gtk_entry_get_text(GTK_ENTRY(aliasname));
	g_snprintf(b->show, sizeof(b->show), "%s", (al && strlen(al)) ? al : b->name);
	handle_buddy_rename(b, b->name);
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
	gtk_window_set_wmclass(GTK_WINDOW(aliasdlg), "alias_dialog", "Gaim");
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
	aol_icon(aliasdlg->window);

	gtk_widget_show(aliasdlg);
}


static gboolean dont_destroy(gpointer a, gpointer b, gpointer c)
{
	return TRUE;
}

static void do_save_log(GtkWidget *w, GtkWidget *filesel)
{
	char *file;
	char path[PATHSIZE];
	char buf[BUF_LONG];
	char error[BUF_LEN];
	FILE *fp_old, *fp_new;
	char filename[PATHSIZE];
	char *name;
	char *tmp;

	name = gtk_object_get_user_data(GTK_OBJECT(filesel));
	tmp = gaim_user_dir();
	g_snprintf(filename, PATHSIZE, "%s/logs/%s%s", tmp,
		   name ? normalize(name) : "system", name ? ".log" : "");
	g_free(tmp);

	file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	strncpy(path, file, PATHSIZE - 1);
	if (file_is_dir(path, filesel))
		return;

	if ((fp_new = fopen(path, "w")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   "Can't open file %s for writing - %s", path, strerror(errno));
		do_error_dialog(error, "Error");
		return;
	}

	if ((fp_old = fopen(filename, "r")) == NULL) {
		g_snprintf(error, BUF_LONG,
			   "Can't open file %s for reading - %s", filename, strerror(errno));
		do_error_dialog(error, "Error");
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

	g_snprintf(buf, BUF_LEN - 1, "%s/%s%s", getenv("HOME"),
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
	aol_icon(filesel->window);
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
	g_snprintf(filename, 256, "%s/logs/%s%s", tmp,
		   name ? normalize(name) : "system", name ? ".log" : "");
	g_free(tmp);

	if ((remove(filename)) == -1) {
		g_snprintf(buf, 256, _("Unable to remove file %s - %s"), filename, strerror(errno));
		do_error_dialog(buf, _("Error"));
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
	gtk_window_set_wmclass(GTK_WINDOW(window), "dialog", "Gaim");
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(destroy_dialog), window);
	gtk_widget_realize(window);
	aol_icon(window->window);

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
		g_snprintf(filename, 256, "%s/logs/%s.log", tmp, normalize(view->name));
		g_free(tmp);
	} else {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s/logs/system", tmp);
		g_free(tmp);
	}
	if ((fp = fopen(filename, "r")) == NULL) {
		g_snprintf(buf, BUF_LONG, "Unable to open log file %s", filename);
		do_error_dialog(buf, "Error!");
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
	gtk_window_set_wmclass(GTK_WINDOW(window), "log", "Gaim");
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
	aol_icon(window->window);

	layout = gtk_imhtml_new(NULL, NULL);
	bbox = gtk_hbox_new(FALSE, 0);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);

	if (name) {
		char *tmp = gaim_user_dir();
		g_snprintf(filename, 256, "%s/logs/%s.log", tmp, normalize(name));
		g_free(tmp);
		if ((fp = fopen(filename, "r")) == NULL) {
			g_snprintf(buf, BUF_LONG, "Unable to open log file %s", filename);
			do_error_dialog(buf, "Error!");
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
	GTK_LAYOUT(layout)->hadjustment->step_increment = 10.0;
	GTK_LAYOUT(layout)->vadjustment->step_increment = 10.0;
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
	char *new_name;
	struct group *g;
	struct group *orig;
	struct gaim_connection *gc;

	new_name = gtk_entry_get_text(GTK_ENTRY(entry));
	g = gtk_object_get_user_data(obj);

	if (new_name && (strlen(new_name) != 0) && strcmp(new_name, g->name)) {
		char *prevname;
		gc = g->gc;
		if ((orig = find_group(g->gc, new_name)) != NULL) {
			orig->members = g_slist_concat(orig->members, g->members);
			handle_group_rename(orig, g->name);
			g->gc->groups = g_slist_remove(g->gc->groups, g);
			/* FIXME, i don't like calling this. it's sloppy. */ build_edit_tree();
			g_free(g);
		} else {
			prevname = g_strdup(g->name);
			g_snprintf(g->name, sizeof(g->name), "%s", new_name);
			handle_group_rename(g, prevname);
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
		gtk_window_set_wmclass(GTK_WINDOW(rename_dialog), "rename_dialog", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(rename_dialog), FALSE, TRUE, TRUE);
		gtk_window_set_title(GTK_WINDOW(rename_dialog), _("Gaim - Rename Group"));
		gtk_signal_connect(GTK_OBJECT(rename_dialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_dialog);
		gtk_widget_realize(rename_dialog);
		aol_icon(rename_dialog->window);

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
	char *new_name;
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
		gtk_window_set_wmclass(GTK_WINDOW(rename_bud_dialog), "rename_bud_dialog", "Gaim");
		gtk_window_set_policy(GTK_WINDOW(rename_bud_dialog), FALSE, TRUE, TRUE);
		gtk_window_set_title(GTK_WINDOW(rename_bud_dialog), _("Gaim - Rename Buddy"));
		gtk_signal_connect(GTK_OBJECT(rename_bud_dialog), "destroy",
				   GTK_SIGNAL_FUNC(destroy_dialog), rename_bud_dialog);
		gtk_widget_realize(rename_bud_dialog);
		aol_icon(rename_bud_dialog->window);

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
		g_free(temp);
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

static GdkPixmap *icon_pm = NULL;
static GdkBitmap *icon_bm = NULL;

void aol_icon(GdkWindow *w)
{
#ifndef _WIN32
	if (icon_pm == NULL) {
		icon_pm = gdk_pixmap_create_from_xpm_d(w, &icon_bm, NULL, (gchar **)aimicon_xpm);
	}
	gdk_window_set_icon(w, NULL, icon_pm, icon_bm);
	if (mainwindow)
		gdk_window_set_group(w, mainwindow->window);
#endif
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
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

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
	if (misc_options & OPT_MISC_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

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
		gtk_text_insert(GTK_TEXT(mtd->textbox), NULL, NULL, NULL, mtd->text, -1);
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
 *           wmclass_name
 *           wmclass_class
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
	gtk_window_set_wmclass(GTK_WINDOW(b->window), b->wmclass_name, b->wmclass_class);
	gtk_window_set_title(GTK_WINDOW (b->window), b->title);

	/* Clean up if user dismisses window via window manager! */
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy", GTK_SIGNAL_FUNC(b->cancel), (gpointer) b);
	gtk_widget_realize(b->window);
	aol_icon(b->window->window);

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


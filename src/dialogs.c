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
#include "../config.h"
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <gtk/gtk.h>
#include "gaim.h"
#include "gtkhtml.h"

#include "pixmaps/cancel.xpm"
#include "pixmaps/save.xpm"
#include "pixmaps/ok.xpm"
#include "pixmaps/add.xpm"
#include "pixmaps/warn.xpm"
#include "pixmaps/gnome_remove.xpm"

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

#define DEFAULT_FONT_NAME "-adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1"

char *fontface;
char *fontname;
int smiley_array[FACE_TOTAL];

static GtkWidget *imdialog = NULL; /*I only want ONE of these :) */
static GList *dialogwindows = NULL;
static GtkWidget *exportdialog, *importdialog;

struct create_away {
        GtkWidget *window;
        GtkWidget *entry;
        GtkWidget *text;
        GtkWidget *checkbx;
};


struct warning {
        GtkWidget *window;
        GtkWidget *anon;
        char *who;
};

struct addbuddy {
        GtkWidget *window;
        GtkWidget *combo;
        GtkWidget *entry;
};

struct addperm {
        GtkWidget *window;
        GSList *buttons;
        GtkWidget *entry;
};

struct addbp {
        GtkWidget *window;
        GtkWidget *nameentry;
        GtkWidget *messentry;
	GtkWidget *sendim;
	GtkWidget *openwindow;
};

struct findbyemail {
	GtkWidget *window;
	GtkWidget *emailentry;
};

struct findbyinfo {
	GtkWidget *window;
	GtkWidget *firstentry;
	GtkWidget *middleentry;
	GtkWidget *lastentry;
	GtkWidget *maidenentry;
	GtkWidget *cityentry;
	GtkWidget *stateentry;
	GtkWidget *countryentry;
};

struct registerdlg {
	GtkWidget *window;
	GtkWidget *name;
	GtkWidget *email;
	GtkWidget *uname;
	GtkWidget *sname;
	GtkWidget *country;
};

struct info_dlg {
	GtkWidget *window;
	GtkWidget *text;
	GtkWidget *close;
};


struct set_info_dlg {
	GtkWidget *window;
	GtkWidget *text;
	GtkWidget *save;
	GtkWidget *cancel;
};

struct set_dir_dlg {
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
};

/*------------------------------------------------------------------------*/
/*  Function to Send an Email                                             */
/*------------------------------------------------------------------------*/

static int g_sendemail(char *name, char *email, int uname, int sname, char *country)
{
	static char email_data[2000];
	int sock;
	struct in_addr *host;
/*	char data[3]; */
	FILE *sockfile;
	char uname_output;
        FILE *tmpfile;
        char filename[128];
        char buf[256];
        int i=0, tmpfd=-1;

        while (i<10000 && tmpfd < 0) {
                g_snprintf(filename, 128, "/tmp/gaim_%s%d.tmp", current_user->username, i++);

                tmpfd = open(filename, O_RDWR|O_CREAT|O_EXCL, 0600);
        }

        if(tmpfd < 0) {
                return -1;
        }

        
	if (uname)
        {
                g_snprintf(buf, sizeof(buf), "uname -a > %s", filename);
		system(buf);
	}
	
	host = (struct in_addr *)get_address(REG_SRVR);
	if (!host) 
	{
		printf(_("Error Resolving Mail Server.\n"));
		return -1;
	}

	if ((sock = connect_address(host->s_addr, REG_PORT)) < 0)
	{
		printf(_("Error Connecting to Socket.\n"));
		return -1;
	}	 

	sockfile = fdopen(sock, "r+");

	g_snprintf(email_data, sizeof(email_data), "mail from: %s\n", REG_EMAIL_ADDR);
	fputs(email_data, sockfile);
	
	g_snprintf(email_data, sizeof(email_data), "rcpt to: %s\n", REG_EMAIL_ADDR);
	fputs(email_data, sockfile);

	g_snprintf(email_data, sizeof(email_data), "data\n");
	fputs(email_data, sockfile);
	g_snprintf(email_data, sizeof(email_data), "Subject: Registration Information\n\nBelow is the submitted Registration Information\n----------------------------------\nName: %s\nEmail: %s\nCountry: %s\nSName: %s\nGAIM: v%s\nUname: ", name, email, country, sname ? current_user->username : "N/A", VERSION);
	fputs(email_data, sockfile);

	if (uname)
	{
		tmpfile = fopen(filename, "r");
		while (!feof(tmpfile))
		{
			uname_output = fgetc(tmpfile);
			if (!feof(tmpfile))
				fputc(uname_output, sockfile);
		}
		fclose(tmpfile);
        }

        unlink(filename);
	
	g_snprintf(email_data, sizeof(email_data), "\n.\nquit\n\n");
	fputs(email_data, sockfile);

/*	while (fgets(data, 2, sockfile)) {
	}
        */
        /* I don't think the above is necessary... */
        
	close(sock);

	return 1;
}

/*------------------------------------------------------------------------*/
/*  Destroys                                                              */
/*------------------------------------------------------------------------*/

static gint delete_event_dialog(GtkWidget *w, GdkEventAny *e, struct conversation *c)
{
	gchar *object_data;
	dialogwindows = g_list_remove(dialogwindows, w);
	gtk_widget_destroy(w);
	
	object_data = gtk_object_get_user_data(GTK_OBJECT(w));

	if (GTK_IS_COLOR_SELECTION_DIALOG(w))
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->palette), FALSE);
		set_state_lock(0);
		c->color_dialog = NULL;
	}
	else if (GTK_IS_FONT_SELECTION_DIALOG(w))
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->font), FALSE);
		set_state_lock(0);
		c->font_dialog = NULL;
	}
	else if (!g_strcasecmp(object_data, "smiley dialog"))
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->smiley), FALSE);
		set_state_lock(0);
		c->smiley_dialog = NULL;
	}
	else if (!g_strcasecmp(object_data, "log dialog"))
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->wood), FALSE);
		set_state_lock(0);
		c->log_dialog = NULL;
	}
	
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

	if (dest == exportdialog)
		exportdialog = NULL;

	if (dest == importdialog)
		importdialog = NULL;

        dialogwindows = g_list_remove(dialogwindows, dest);
        gtk_widget_destroy(dest);

}


void destroy_all_dialogs()
{
        GList *d = dialogwindows;
        
        while(d) {
                destroy_dialog(NULL, d->data);
                d = d->next;
        }
        
        g_list_free(dialogwindows);
        dialogwindows = NULL;

	if (awaymessage)
		do_im_back(NULL, NULL);

        if (imdialog) {
                destroy_dialog(NULL, imdialog);
                imdialog = NULL;
        }
        
        if (exportdialog) {
                destroy_dialog(NULL, exportdialog);
                exportdialog = NULL;
        }

        if (importdialog) {
                destroy_dialog(NULL, importdialog);
                importdialog = NULL;
        }
}

static void do_warn(GtkWidget *widget, struct warning *w)
{
        serv_warn(w->who, (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->anon))) ?
                   1 : 0);
        
        destroy_dialog(NULL, w->window);
}


void show_warn_dialog(char *who)
{
	GtkWidget *cancel;
	GtkWidget *warn;
	GtkWidget *label;
	GtkWidget *vbox;
        GtkWidget *bbox;
	GtkWidget *frame;
	GtkWidget *fbox;

        struct warning *w = g_new0(struct warning, 1);
        
        char *buf = g_malloc(128);
        w->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_policy(GTK_WINDOW(w->window), FALSE, FALSE, TRUE);
	gtk_widget_show(w->window);
	dialogwindows = g_list_prepend(dialogwindows, w->window);
        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);
	fbox = gtk_vbox_new(FALSE, 5);

	frame = gtk_frame_new(_("Warn"));
	
	warn = picture_button(w->window, _("Warn"), warn_xpm);

	cancel = picture_button(w->window, _("Cancel"), cancel_xpm);

	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(warn), GTK_RELIEF_NONE);
	}
	
	/* Put the buttons in the box */

	gtk_box_pack_start(GTK_BOX(bbox), warn, FALSE, FALSE, 5);
        gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

        g_snprintf(buf, 127, _("Do you really want to warn %s?"), who);
        label = gtk_label_new(buf);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
        gtk_widget_show(label);
        w->anon = gtk_check_button_new_with_label(_("Warn anonymously?"));
        gtk_box_pack_start(GTK_BOX(vbox), w->anon, TRUE, TRUE, 5);

        label = gtk_label_new(_("Anonymous warnings are less harsh."));
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
        gtk_widget_show(label);

        w->who = who;
	
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(fbox), bbox, FALSE, FALSE, 5);

        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(w->window), "delete_event",
                           GTK_SIGNAL_FUNC(destroy_dialog), w->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), w->window);
        gtk_signal_connect(GTK_OBJECT(warn), "clicked",
                           GTK_SIGNAL_FUNC(do_warn), w);
        /* Finish up */
        gtk_widget_show(w->anon);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
	gtk_widget_show(frame);
	gtk_widget_show(fbox);

	gtk_window_set_title(GTK_WINDOW(w->window), _("Gaim - Warn user?"));
        gtk_container_add(GTK_CONTAINER(w->window), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(w->window), 5);
	gtk_widget_realize(w->window);
        aol_icon(w->window->window);

        gtk_widget_show(w->window);
	g_free(buf);
}


/*------------------------------------------------------------------------*/
/*  The dialog for getting an error                                       */
/*------------------------------------------------------------------------*/

void
do_error_dialog(char *message, char *title)
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
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),
		label, FALSE, FALSE, 5);
		
	close = picture_button(d, _("Close"), cancel_xpm);

	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
	
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area), 
		close, FALSE, FALSE, 5);

	gtk_container_set_border_width(GTK_CONTAINER(d), 5);
	gtk_window_set_title(GTK_WINDOW(d), title);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), d);
	aol_icon(d->window);

	gtk_widget_show(d);
}



void show_error_dialog(char *d)
{

	int no = atoi(d);
	char *w;
	char buf[256];
	char buf2[32];

#ifdef GAIM_PLUGINS
	GList *c = callbacks;
	struct gaim_callback *g;
	void (*function)(int, void *);
	while (c) {
		g = (struct gaim_callback *)c->data;
		if (g->event == event_error && g->function != NULL) {
			function = g->function;
			(*function)(no, g->data);
		}
		c = c->next;
	}
#endif

	if (USE_OSCAR)
		w = d + 4;
	else
		w = strtok(NULL, ":");
 	
	
        switch(no) {
        case 69:
                g_snprintf(buf, sizeof(buf), _("Unable to write file %s."), w);
                break;
        case 169:
                g_snprintf(buf, sizeof(buf), _("Unable to read file %s."), w);
                break;
        case 269:
                g_snprintf(buf, sizeof(buf), _("Message too long, last %s bytes truncated."), w);
                break;
        case 901:
                g_snprintf(buf, sizeof(buf), _("%s not currently logged in."), w);
                break;
        case 902:
                g_snprintf(buf, sizeof(buf), _("Warning of %s not allowed."), w);
                break;
        case 903:
                g_snprintf(buf, sizeof(buf), _("A message has been dropped, you are exceeding the server speed limit."));
                break;
        case 950:
                g_snprintf(buf, sizeof(buf), _("Chat in %s is not available."), w);
                break;
        case 960:
                g_snprintf(buf, sizeof(buf), _("You are sending messages too fast to %s."), w);
                break;
        case 961:
                g_snprintf(buf, sizeof(buf), _("You missed an IM from %s because it was too big."), w);
                break;
        case 962:
                g_snprintf(buf, sizeof(buf), _("You missed an IM from %s because it was sent too fast."), w);
                break;
        case 970:
                g_snprintf(buf, sizeof(buf), _("Failure."));
                break;
        case 971:
                g_snprintf(buf, sizeof(buf), _("Too many matches."));
                break;
        case 972:
                g_snprintf(buf, sizeof(buf), _("Need more qualifiers."));
                break;
        case 973:
                g_snprintf(buf, sizeof(buf), _("Dir service temporarily unavailable."));
                break;
        case 974:
                g_snprintf(buf, sizeof(buf), _("Email lookup restricted."));
                break;
        case 975:
                g_snprintf(buf, sizeof(buf), _("Keyword ignored."));
                break;
        case 976:
                g_snprintf(buf, sizeof(buf), _("No keywords."));
                break;
        case 977:
                g_snprintf(buf, sizeof(buf), _("User has no directory information."));
                /* g_snprintf(buf, sizeof(buf), "Language not supported."); */
                break;
        case 978:
                g_snprintf(buf, sizeof(buf), _("Country not supported."));
                break;
        case 979:
                g_snprintf(buf, sizeof(buf), _("Failure unknown: %s."), w);
                break;
        case 980:
                g_snprintf(buf, sizeof(buf), _("Incorrect nickname or password."));
                break;
        case 981:
                g_snprintf(buf, sizeof(buf), _("The service is temporarily unavailable."));
                break;
        case 982:
                g_snprintf(buf, sizeof(buf), _("Your warning level is currently too high to log in."));
                break;
        case 983:
                g_snprintf(buf, sizeof(buf), _("You have been connecting and disconnecting too frequently.  Wait ten minutes and try again.  If you continue to try, you will need to wait even longer."));
                break;
        case 989:
                g_snprintf(buf, sizeof(buf), _("An unknown signon error has occurred: %s."), w);
                break;
        default:
                g_snprintf(buf, sizeof(buf), _("An unknown error, %d, has occured.  Info: %s"), no, w);
	}
	
	g_snprintf(buf2, sizeof(buf2), _("Gaim - Error %d"), no);


        do_error_dialog(buf, buf2);
        return;
}

static void do_im(GtkWidget *widget, GtkWidget *imentry)
{
        char *who;
        struct conversation *c;
	
        who = g_strdup(normalize(gtk_entry_get_text(GTK_ENTRY(imentry))));
	destroy_dialog(NULL, imdialog);
        imdialog = NULL;
        
        if (!strcasecmp(who, "")) {
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

void show_ee_dialog(int ee)
{
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *eedialog;

	eedialog = gtk_window_new(GTK_WINDOW_DIALOG);
        ok = gtk_button_new_with_label(_("OK"));
        box = gtk_vbox_new(FALSE, 10);


	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(ok), GTK_RELIEF_NONE);
	
	if (ee == 0)
		label = gtk_label_new("Amazing!  Simply Amazing!");
	else if (ee == 1)
		label = gtk_label_new("Pimpin\' Penguin Style! *Waddle Waddle*");
	else if (ee == 2)
		label = gtk_label_new("You should be me.  I'm so cute!");
	else
		label = gtk_label_new("Now that's what I like!");

	gtk_widget_show(label);
	gtk_widget_show(ok);

	gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 10);
	gtk_box_pack_start(GTK_BOX(box), ok, FALSE, FALSE, 10);

	gtk_widget_show(box);

	gtk_container_add(GTK_CONTAINER(eedialog), box);
	gtk_window_set_title(GTK_WINDOW(eedialog), "Gaim - SUPRISE!");

	gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   GTK_SIGNAL_FUNC(destroy_dialog), eedialog);
	gtk_widget_realize(eedialog);
        aol_icon(eedialog->window);

	gtk_widget_show(eedialog);
}

void show_im_dialog(GtkWidget *w, GtkWidget *w2)
{
	GtkWidget *button;
	GtkWidget *imentry;
        GtkWidget *vbox;
        GtkWidget *ebox;
        GtkWidget *bbox;
        GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *fbox;

        if (!imdialog) {

                imdialog = gtk_window_new(GTK_WINDOW_DIALOG);
		gtk_widget_set_usize(imdialog, 255, 105);
		gtk_container_border_width(GTK_CONTAINER(imdialog), 5);
		gtk_window_set_policy(GTK_WINDOW(imdialog), FALSE, FALSE, TRUE);
		gtk_widget_show(imdialog);

		bbox = gtk_hbox_new(TRUE, 10);
                vbox = gtk_vbox_new(FALSE, 5);
                ebox = gtk_hbox_new(FALSE, 2);
		fbox = gtk_vbox_new(TRUE, 10);

		frame = gtk_frame_new(_("Send Instant Message"));
	
		imentry = gtk_entry_new();

		button = picture_button(imdialog, _("OK"), ok_xpm);
		gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(do_im), imentry);

		button = picture_button(imdialog, _("Cancel"), cancel_xpm);
		gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);

                label = gtk_label_new(_("IM who: "));
                gtk_box_pack_start(GTK_BOX(ebox), label, TRUE, TRUE, 10);
                gtk_widget_show(label);

                gtk_box_pack_start(GTK_BOX(ebox), imentry, TRUE, TRUE, 10);

                gtk_box_pack_start(GTK_BOX(vbox), ebox, FALSE, FALSE, 5);
                gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

                /* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(imentry), "activate",
				   GTK_SIGNAL_FUNC(do_im), imentry);
                gtk_signal_connect(GTK_OBJECT(imdialog), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);

		/* Finish up */
                gtk_widget_show(ebox);
                gtk_widget_show(imentry);
                gtk_widget_show(bbox);
                gtk_widget_show(vbox);
		gtk_widget_show(fbox);
		gtk_widget_show(frame);
		gtk_container_add(GTK_CONTAINER(frame), vbox);
		gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);
		gtk_window_set_title(GTK_WINDOW(imdialog), _("Gaim - IM user"));
                gtk_container_add(GTK_CONTAINER(imdialog), fbox);
                gtk_widget_grab_focus(imentry);
                gtk_widget_realize(imdialog);
		
		aol_icon(imdialog->window);

        }
        gtk_widget_show(imdialog);
}


/*------------------------------------------------------------------------*/
/*  The dialog for adding buddies                                         */
/*------------------------------------------------------------------------*/

extern void add_callback(GtkWidget *, struct conversation *);

void do_add_buddy(GtkWidget *w, struct addbuddy *a)
{
	char *grp, *who;
        struct conversation *c;
        
	who = gtk_entry_get_text(GTK_ENTRY(a->entry));
        grp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry));

        c = find_conversation(who);

        add_buddy(grp, who);

        if (c != NULL) {
		GtkWidget *parent = c->add_button->parent;
		gtk_widget_destroy(c->add_button);
		c->add_button = picture_button2(c->window, NULL, gnome_remove_xpm);
		gtk_signal_connect(GTK_OBJECT(c->add_button), "clicked", GTK_SIGNAL_FUNC(add_callback), c);
		gtk_box_pack_end(GTK_BOX(parent), c->add_button, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(parent), c->add_button, 1);
		gtk_widget_show(c->add_button);
	}
        
        build_edit_tree();

        serv_save_config();

        serv_add_buddy(who);

	do_export( (GtkWidget *) NULL, 0 );

        update_num_groups();

        destroy_dialog(NULL, a->window);
}

void do_add_group(GtkWidget *w, struct addbuddy *a)
{
	char *grp;

	grp = gtk_entry_get_text(GTK_ENTRY(a->entry));

	add_group(grp);

	build_edit_tree();

	serv_save_config();

	do_export( (GtkWidget *) NULL, 0 );

	update_num_groups();

	destroy_dialog(NULL, a->window);
}


static GList *groups_tree()
{
	GList *tmp=NULL;
        char *tmp2;
	struct group *g;
        GList *grp = groups;
        
	if (!grp) {
                tmp2 = g_strdup(_("Buddies"));
                tmp = g_list_append(tmp, tmp2);
	} else {
		while(grp) {
			g = (struct group *)grp->data;
                        tmp2 = g->name;
                        tmp=g_list_append(tmp, tmp2);
			grp = grp->next;
		}
	}
	return tmp;
}


void show_add_group()
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
        GtkWidget *topbox;
	GtkWidget *frame;

        struct addbuddy *a = g_new0(struct addbuddy, 1);
        
        a->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_policy(GTK_WINDOW(a->window), FALSE, FALSE, TRUE);
	gtk_widget_show(a->window);
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
        gtk_signal_connect(GTK_OBJECT(a->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), a->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), a->window);
        gtk_signal_connect(GTK_OBJECT(add), "clicked",
                           GTK_SIGNAL_FUNC(do_add_group), a);
        gtk_signal_connect(GTK_OBJECT(a->entry), "activate",
                           GTK_SIGNAL_FUNC(do_add_group), a);
        /* Finish up */
        gtk_widget_show(add);
        gtk_widget_show(cancel);
        gtk_widget_show(a->entry);
        gtk_widget_show(topbox);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
	gtk_widget_show(frame);
        gtk_window_set_title(GTK_WINDOW(a->window), _("Gaim - Add Buddy"));
        gtk_window_set_focus(GTK_WINDOW(a->window), a->entry);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_container_add(GTK_CONTAINER(a->window), frame);
	gtk_container_set_border_width(GTK_CONTAINER(a->window), 5);
        gtk_widget_realize(a->window);
        aol_icon(a->window->window);

	gtk_widget_show(a->window);
}

void show_add_buddy(char *buddy, char *group)
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
        GtkWidget *topbox;
	GtkWidget *frame;

        struct addbuddy *a = g_new0(struct addbuddy, 1);
        
        a->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(a->window, 480, 105);
	gtk_window_set_policy(GTK_WINDOW(a->window), FALSE, FALSE, TRUE);
	gtk_widget_show(a->window);
	dialogwindows = g_list_prepend(dialogwindows, a->window);

	bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);

        a->entry = gtk_entry_new();
        a->combo = gtk_combo_new();
        /* Fix the combo box */
        gtk_combo_set_popdown_strings(GTK_COMBO(a->combo), groups_tree());
        /* Put the buttons in the box */

	add = picture_button(a->window, _("Add"), add_xpm);
	cancel = picture_button(a->window, _("Cancel"), cancel_xpm);
	
        gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 5);
        gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_label(GTK_FRAME(frame), _("Add Buddy"));

        label = gtk_label_new(_("Buddy"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), a->entry, FALSE, FALSE, 5);
        if (buddy != NULL)
                gtk_entry_set_text(GTK_ENTRY(a->entry), buddy);

        label = gtk_label_new(_("Group"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), a->combo, FALSE, FALSE, 5);

        if (group != NULL)
                gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry), group);

        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(a->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), a->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), a->window);
        gtk_signal_connect(GTK_OBJECT(add), "clicked",
                           GTK_SIGNAL_FUNC(do_add_buddy), a);
        gtk_signal_connect(GTK_OBJECT(a->entry), "activate",
                           GTK_SIGNAL_FUNC(do_add_buddy), a);
        /* Finish up */
        gtk_widget_show(add);
        gtk_widget_show(cancel);
        gtk_widget_show(a->combo);
        gtk_widget_show(a->entry);
        gtk_widget_show(topbox);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
	gtk_widget_show(frame);
        gtk_window_set_title(GTK_WINDOW(a->window), _("Gaim - Add Buddy"));
        gtk_window_set_focus(GTK_WINDOW(a->window), a->entry);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_container_add(GTK_CONTAINER(a->window), frame);
	gtk_container_set_border_width(GTK_CONTAINER(a->window), 5);
        gtk_widget_realize(a->window);
        aol_icon(a->window->window);

	gtk_widget_show(a->window);
}


/*------------------------------------------------------------------------*/
/*  The dialog for new buddy pounces                                      */
/*------------------------------------------------------------------------*/


void do_new_bp(GtkWidget *w, struct addbp *b)
{
        struct buddy_pounce *bp = g_new0(struct buddy_pounce, 1);
	
	g_snprintf(bp->name, 80, "%s", gtk_entry_get_text(GTK_ENTRY(b->nameentry)));
	g_snprintf(bp->message, 2048, "%s", gtk_entry_get_text(GTK_ENTRY(b->messentry)));

	if (GTK_TOGGLE_BUTTON(b->openwindow)->active)
		bp->popup = 1;
	else
		bp->popup = 0;

	if (GTK_TOGGLE_BUTTON(b->sendim)->active)
		bp->sendim = 1;
	else
		bp->sendim = 0;

        buddy_pounces = g_list_append(buddy_pounces, bp);
	
        do_bp_menu();
        
        destroy_dialog(NULL, b->window);
        g_free(b);
}


void show_new_bp(char *name)
{
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
	GtkWidget *button;

        struct addbp *b = g_new0(struct addbp, 1);
        
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_widget_show(b->window);
        dialogwindows = g_list_prepend(dialogwindows, b->window);
        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);
        b->nameentry = gtk_entry_new();
        b->messentry = gtk_entry_new();

	/* Build OK Button */

	button = picture_button(b->window, _("OK"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(do_new_bp), b);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	

	button = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	

        /* Put the buttons in the box */
        label = gtk_label_new(_("Buddy To Pounce:"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), b->nameentry, FALSE, FALSE, 0);

	b->openwindow = gtk_check_button_new_with_label(_("Open IM Window on Buddy Logon"));
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->openwindow), FALSE);
	
	b->sendim = gtk_check_button_new_with_label(_("Send IM on Buddy Logon")); 
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sendim), TRUE);

	gtk_widget_show(b->openwindow);
	gtk_widget_show(b->sendim);
	gtk_box_pack_start(GTK_BOX(vbox), b->openwindow, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), b->sendim, FALSE, FALSE, 0);

        label = gtk_label_new(_("Message to send:"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), b->messentry, FALSE, FALSE, 0);


        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->messentry), "activate",
                           GTK_SIGNAL_FUNC(do_new_bp), b);

        
        /* Finish up */
        gtk_widget_show(b->nameentry);
        gtk_widget_show(b->messentry);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - New Buddy Pounce"));
        if (name != NULL) {
                gtk_entry_set_text(GTK_ENTRY(b->nameentry), name);
                gtk_window_set_focus(GTK_WINDOW(b->window), b->messentry);
        } else
                gtk_window_set_focus(GTK_WINDOW(b->window), b->nameentry);
        gtk_container_add(GTK_CONTAINER(b->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
        aol_icon(b->window->window);
}



/*------------------------------------------------------------------------*/
/*  The dialog for SET INFO / SET DIR INFO                                */
/*------------------------------------------------------------------------*/

void do_save_info(GtkWidget *widget, struct set_info_dlg *b)
{
	gchar *junk;
	char *buf;

	junk = gtk_editable_get_chars(GTK_EDITABLE(b->text), 0, -1);

	g_snprintf(current_user->user_info, sizeof(current_user->user_info), "%s", junk);
		
	save_prefs();

        buf = g_malloc(strlen(current_user->user_info) * 4);
	if (!buf) {
		buf = g_malloc(1);
		buf[0] = 0;
	}
        g_snprintf(buf, strlen(current_user->user_info) * 2, "%s", current_user->user_info);
        escape_text(buf);
        serv_set_info(buf);
        g_free(buf);
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


        serv_set_dir(first, middle, last, maiden, city, state, country, web);

        destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_set_dir()
{
	GtkWidget *label;
	GtkWidget *bot;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *fbox;

	struct set_dir_dlg *b = g_new0(struct set_dir_dlg, 1);

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(b->window, 300, 320);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_widget_show(b->window);

	dialogwindows = g_list_prepend(dialogwindows, b->window);

	vbox = gtk_vbox_new(FALSE, 5);
	fbox = gtk_vbox_new(FALSE, 5);

	frame = gtk_frame_new(_("Directory Info"));	

	/* Build Save Button */

	b->save = picture_button(b->window, _("Save"), save_xpm);

	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);
	
	bot = gtk_hbox_new(TRUE, 10);

	gtk_box_pack_start(GTK_BOX(bot), b->save, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(bot), b->cancel, FALSE, FALSE, 5);

	gtk_widget_show(bot);

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
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->first, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 2 */
        label = gtk_label_new(_("Middle Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->middle, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);


	/* Line 3 */
        label = gtk_label_new(_("Last Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->last, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 4 */
        label = gtk_label_new(_("Maiden Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->maiden, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 5 */
        label = gtk_label_new(_("City"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->city, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 6 */
	label = gtk_label_new(_("State"));        
	gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->state, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 7 */
        label = gtk_label_new(_("Country"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_box_pack_end(GTK_BOX(hbox), b->country, FALSE, FALSE, 2);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
	gtk_widget_show(hbox);

	/* Line 8 */

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), b->web, TRUE, TRUE, 2);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

	/* And add the buttons */
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(fbox), bot, FALSE, FALSE, 2);


	gtk_widget_show(vbox);
	gtk_widget_show(fbox);

	gtk_widget_show(frame);
        gtk_widget_show(b->first); 
        gtk_widget_show(b->middle);
        gtk_widget_show(b->last); 
        gtk_widget_show(b->maiden);
        gtk_widget_show(b->city);
	gtk_widget_show(b->state);
	gtk_widget_show(b->country);
	gtk_widget_show(b->web);

        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->save), "clicked",                                                      GTK_SIGNAL_FUNC(do_set_dir), b);   	
	
	gtk_container_add(GTK_CONTAINER(b->window), fbox);
	gtk_container_border_width(GTK_CONTAINER(b->window), 5);

	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set Dir Info"));
        gtk_window_set_focus(GTK_WINDOW(b->window), b->first);
        gtk_widget_realize(b->window);
	aol_icon(b->window->window);

	gtk_widget_show(b->window);	
}

void do_change_password(GtkWidget *widget, struct passwddlg *b)
{
	gchar *orig, *new1, *new2;

	orig = gtk_entry_get_text(GTK_ENTRY(b->original));
	new1 = gtk_entry_get_text(GTK_ENTRY(b->new1));
	new2 = gtk_entry_get_text(GTK_ENTRY(b->new2));

	if (strcasecmp(new1, new2)) {
		do_error_dialog(_("New Passwords Do Not Match"), _("Gaim - Change Password Error"));
		return ;
	}

	if ((strlen(orig) < 1) || (strlen(new1) < 1) || (strlen(new2) < 1)) {
		do_error_dialog(_("Fill out all fields completely"), _("Gaim - Change Password Error"));
		return;
	}

	serv_change_passwd(orig, new1);
	
	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_change_passwd()
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *fbox;
	GtkWidget *frame;

	struct passwddlg *b = g_new0(struct passwddlg, 1);

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(b->window, 325, 195);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_widget_show(b->window);

	dialogwindows = g_list_prepend(dialogwindows, b->window);

	frame = gtk_frame_new(_("Change Password"));
	fbox = gtk_vbox_new(FALSE, 5);

	/* Build OK Button */

	b->ok = picture_button(b->window, _("OK"), ok_xpm);
	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);

	/* Create our vbox */
	vbox = gtk_vbox_new(FALSE, 5);

	
	/* First Line */
	hbox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new(_("Original Password"));
	gtk_widget_show(label);

	b->original = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->original), FALSE);
	gtk_widget_show(b->original);

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->original, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	gtk_widget_show(hbox);

	/* Next Line */	
	hbox = gtk_hbox_new(FALSE, 5);
        label = gtk_label_new(_("New Password"));
        gtk_widget_show(label);
        b->new1 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new1), FALSE);
        gtk_widget_show(b->new1);

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->new1, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	/* Next Line */
	hbox = gtk_hbox_new(FALSE, 5);
	label = gtk_label_new(_("New Password (again)"));
        gtk_widget_show(label);
        b->new2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(b->new2), FALSE);
        gtk_widget_show(b->new2);

	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->new2, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	
	/* Now do our row of buttons */	
	hbox = gtk_hbox_new(TRUE, TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), b->ok, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), b->cancel, FALSE, FALSE, 5);

	gtk_widget_show(hbox);

	/* Pack our entries into a frame */
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* Add our frame to our frame box */
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);

	/* And add our row of buttons */
	gtk_box_pack_start(GTK_BOX(fbox), hbox, FALSE, FALSE, 5);


	gtk_widget_show(vbox);
	gtk_widget_show(frame);
	gtk_widget_show(fbox);
	
	gtk_container_add(GTK_CONTAINER(b->window), fbox);

	gtk_container_border_width(GTK_CONTAINER(b->window), 5);
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Password Change"));
	
        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->ok), "clicked",
                           GTK_SIGNAL_FUNC(do_change_password), b);


}

void show_set_info()
{
	GtkWidget *bot;
	GtkWidget *top;
	
	struct set_info_dlg *b = g_new0(struct set_info_dlg, 1);

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	gtk_widget_show(b->window);

	bot = gtk_hbox_new(TRUE, 10);
	top = gtk_vbox_new(FALSE, 10);

	/* Build OK Button */

	b->save = picture_button(b->window, _("Save"), save_xpm);
	b->cancel = picture_button(b->window, _("Cancel"), cancel_xpm);

	gtk_box_pack_start(GTK_BOX(bot), b->save, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(bot), b->cancel, FALSE, FALSE, 10);

	gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_signal_connect(GTK_OBJECT(b->save), "clicked",
			   GTK_SIGNAL_FUNC(do_save_info), b);

	gtk_widget_show(bot);


	b->text = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(b->text), TRUE);
	gtk_text_set_editable(GTK_TEXT(b->text), TRUE);
	gtk_widget_set_usize(b->text, 350, 100);
	gtk_text_insert(GTK_TEXT(b->text), NULL, NULL, NULL, current_user->user_info, -1);

	gtk_widget_show(b->text);

	gtk_box_pack_start(GTK_BOX(top), b->text, TRUE, TRUE, 10);
	gtk_widget_show(top);

	gtk_box_pack_start(GTK_BOX(top), bot, FALSE, FALSE, 10);

	gtk_container_add(GTK_CONTAINER(b->window), top);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
	aol_icon(b->window->window);
	
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Set User Info"));
	gtk_widget_show(b->window);

}

/*------------------------------------------------------------------------*/
/*  The dialog for registration information                               */
/*------------------------------------------------------------------------*/

void do_register_dialog(GtkWidget *widget, struct registerdlg *b)
{
	char *email = gtk_entry_get_text(GTK_ENTRY(b->email));
	char *name = gtk_entry_get_text(GTK_ENTRY(b->name));
	int uname = GTK_TOGGLE_BUTTON(b->uname)->active;
	int sname = GTK_TOGGLE_BUTTON(b->sname)->active;
	char *country = gtk_entry_get_text(GTK_ENTRY(b->country));

	general_options |= OPT_GEN_REGISTERED;
	save_prefs();
	
	destroy_dialog(NULL, b->window);

	g_free(b);

        g_sendemail(name, email, uname, sname, country);
}

void set_reg_flag(GtkWidget *widget, struct registerdlg *b)
{
	general_options |= OPT_GEN_REGISTERED;
	save_prefs();
	destroy_dialog(NULL, b->window);
	g_free(b);
}
 
void show_register_dialog()
{
	GtkWidget *ok;
	GtkWidget *cancel;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *bbox;

	struct registerdlg *b = g_new0(struct registerdlg, 1);
	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	dialogwindows = g_list_prepend(dialogwindows, b->window);

	cancel = gtk_button_new_with_label(_("Cancel"));
	ok = gtk_button_new_with_label(_("Send"));

	bbox = gtk_hbox_new(TRUE, 10);
	table = gtk_table_new(6, 2, TRUE);
	vbox = gtk_vbox_new(FALSE, 5);

	b->name = gtk_entry_new();
	b->email = gtk_entry_new();
	b->uname = gtk_check_button_new_with_label("Send the output of uname -a with registration");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->uname), TRUE);
	b->sname = gtk_check_button_new_with_label("Send my screenname with registration");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sname), TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 10);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

	label = gtk_label_new("This list will not, in any way, be distributed and\nwill be used for internal census purposes only.\nAll fields are completely optional.");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, 0, 1);

	label = gtk_label_new("Name");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), b->name, 1, 2, 1, 2);

	label = gtk_label_new("Email");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	gtk_table_attach_defaults(GTK_TABLE(table), b->email, 1, 2, 2, 3);
	
	label = gtk_label_new("Country");
	b->country = gtk_entry_new();
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
	gtk_table_attach_defaults(GTK_TABLE(table), b->country, 1, 2, 3, 4);

	gtk_table_attach_defaults(GTK_TABLE(table), b->sname, 0, 2, 4, 5);
	gtk_table_attach_defaults(GTK_TABLE(table), b->uname, 0, 2, 5, 6);

	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

	if (display_options & OPT_DISP_COOL_LOOK)
	{
		gtk_button_set_relief(GTK_BUTTON(ok), GTK_RELIEF_NONE);
		gtk_button_set_relief(GTK_BUTTON(cancel), GTK_RELIEF_NONE);
	}
	
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(set_reg_flag), b);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   GTK_SIGNAL_FUNC(do_register_dialog), b);

	gtk_widget_show(ok);
	gtk_widget_show(cancel);
	gtk_widget_show(b->name);
	gtk_widget_show(b->email);
	gtk_widget_show(b->uname);
	gtk_widget_show(b->sname);
	gtk_widget_show(b->country);
	gtk_widget_show(table);
	gtk_widget_show(bbox);
	gtk_widget_show(vbox);
	gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Registration");
	gtk_window_set_focus(GTK_WINDOW(b->window), b->name);
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
	aol_icon(b->window->window);

	gtk_widget_show(b->window);
}


/*------------------------------------------------------------------------*/
/*  The dialog for the info requests                                      */
/*------------------------------------------------------------------------*/

void g_show_info_text(char *info)
{
        GtkWidget *ok;
        GtkWidget *label;
	GtkWidget *text;
        GtkWidget *bbox;
        GtkWidget *button_box;
        GtkWidget *sw;

        struct info_dlg *b = g_new0(struct info_dlg, 1);

        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, b->window);
        gtk_container_border_width(GTK_CONTAINER(b->window), 5);
        bbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(b->window), bbox);
		button_box = gtk_hbox_new(FALSE, 5);
		gtk_widget_realize(GTK_WIDGET(b->window));
        ok = picture_button(b->window, _("OK"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);

	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(ok), GTK_RELIEF_NONE);

        label = gtk_label_new(_("Below are the results of your search: "));

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_ALWAYS);
	text = gtk_html_new(NULL, NULL);
	b->text = text;
	gtk_container_add(GTK_CONTAINER(sw), text);

	GTK_HTML (text)->hadj->step_increment = 10.0;
	GTK_HTML (text)->vadj->step_increment = 10.0;
	gtk_widget_set_usize(sw, 300, 250);

	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 0);
/*	gtk_box_pack_start(GTK_BOX(button_box), ok, FALSE, FALSE, 0);*/

	aol_icon(b->window->window);
	gtk_widget_show_all(b->window);

	if (display_options & OPT_DISP_SHOW_SMILEY)
		write_html_with_smileys(b->window, b->text, info);
	else
		gtk_html_append_text(GTK_HTML(b->text), info, 0);
	gtk_html_append_text(GTK_HTML(b->text), "</BODY>", 0);

	gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw)), 0);
}

void g_show_info(char *url) {
	char *url_text = grab_url(url);
	g_show_info_text(url_text);
	g_free(url_text);
}

/*------------------------------------------------------------------------*/
/*  The dialog for adding to permit/deny                                  */
/*------------------------------------------------------------------------*/


static void do_add_perm(GtkWidget *w, struct addperm *p)
{

        char *who;
        char *name;
        int d = 0;
        GSList *buttons = p->buttons;


        who = gtk_entry_get_text(GTK_ENTRY(p->entry));

        name = g_malloc(strlen(who) + 2);
        g_snprintf(name, strlen(who) + 2, "%s", who);
        
        while(buttons) {
                if((int)gtk_object_get_user_data(GTK_OBJECT(buttons->data)) == 1) {
                        if (GTK_TOGGLE_BUTTON(buttons->data)->active)
                                d = 1;
                }
                buttons = buttons->next;
        }

        if (d) {
                deny = g_list_append(deny, name);
                serv_add_deny(name);
        } else {
                permit = g_list_append(permit, name);
                serv_add_permit(name);
        }



        build_permit_tree();

        serv_save_config();
	do_export(0, 0);

        destroy_dialog(NULL, p->window);
}



void show_add_perm(char *who)
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
        GtkWidget *bbox;
        GtkWidget *vbox;
        GtkWidget *rbox;
        GtkWidget *topbox;
        GtkWidget *which;
	GtkWidget *frame;
	
	struct addperm *p = g_new0(struct addperm, 1);

        p->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER(p->window), 5);
	gtk_widget_set_usize(p->window, 310, 130);
	gtk_window_set_policy(GTK_WINDOW(p->window), FALSE, FALSE, TRUE);
	gtk_widget_show(p->window);

	dialogwindows = g_list_prepend(dialogwindows, p->window);

	bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);
        rbox = gtk_vbox_new(FALSE, 5);
        p->entry = gtk_entry_new();

	frame = gtk_frame_new(_("Permit / Deny"));

	/* Build Add Button */

	add = picture_button(p->window, _("Add"), add_xpm);
	cancel = picture_button(p->window, _("Cancel"), cancel_xpm);
	
	/* End of Cancel Button */
        if (who != NULL)
                gtk_entry_set_text(GTK_ENTRY(p->entry), who);

        which = gtk_radio_button_new_with_label(NULL, _("Deny"));
        gtk_box_pack_start(GTK_BOX(rbox), which, FALSE, FALSE, 0);
        gtk_object_set_user_data(GTK_OBJECT(which), (int *)1);
        gtk_widget_show(which);

        which = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(which)), _("Permit"));
        gtk_box_pack_start(GTK_BOX(rbox), which, FALSE, FALSE, 0);
        gtk_object_set_user_data(GTK_OBJECT(which), (int *)2);
        gtk_widget_show(which);
                
        /* Put the buttons in the box */
	
	gtk_box_pack_start(GTK_BOX(bbox), add, FALSE, FALSE, 5);
        gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 5);
		
        label = gtk_label_new(_("Add"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), p->entry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), rbox, FALSE, FALSE, 5);
        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);


        p->buttons = gtk_radio_button_group(GTK_RADIO_BUTTON(which));
        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(p->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), p->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), p->window);
        gtk_signal_connect(GTK_OBJECT(add), "clicked",
                           GTK_SIGNAL_FUNC(do_add_perm), p);
        gtk_signal_connect(GTK_OBJECT(p->entry), "activate",
                           GTK_SIGNAL_FUNC(do_add_perm), p);

        /* Finish up */
        gtk_widget_show(add);
        gtk_widget_show(cancel);
        gtk_widget_show(p->entry);
        gtk_widget_show(topbox);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_widget_show(rbox);
	gtk_widget_show(frame);
        gtk_window_set_title(GTK_WINDOW(p->window), _("Gaim - Add Permit/Deny"));
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
	if (c->wood)
	{
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

        if (!find_log_info(c->name)) {
                l = (struct log_conversation *)g_new0(struct log_conversation, 1);
                strcpy(l->name, c->name);
                strcpy(l->filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(c->log_dialog)));
                log_conversations = g_list_append(log_conversations, l);

                if (c != NULL)
                {
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
		
		g_snprintf(buf, BUF_LEN - 1, "%s/%s.log", getenv("HOME"), c->name);
		gtk_object_set_user_data(GTK_OBJECT(c->log_dialog), "log dialog");
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(c->log_dialog), buf);
		gtk_signal_connect(GTK_OBJECT(c->log_dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->log_dialog)->ok_button), "clicked", GTK_SIGNAL_FUNC(do_log), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(c->log_dialog)->cancel_button), "clicked", GTK_SIGNAL_FUNC(cancel_log), c);
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

        serv_dir_search(first, middle, last, maiden, city, state, country, "");
        destroy_dialog(NULL, b->window);
} 

void do_find_email(GtkWidget *w, struct findbyemail *b)
{
	char *email;

	email = gtk_entry_get_text(GTK_ENTRY(b->emailentry));
	
        serv_dir_search("","","","","","","", email);
 
	destroy_dialog(NULL, b->window);
}

void show_find_info()
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
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(b->window, 350, 320);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_widget_show(b->window);

	dialogwindows = g_list_prepend(dialogwindows, b->window);


	frame = gtk_frame_new(_("Search for Buddy"));
	fbox = gtk_vbox_new(FALSE, 5);

	/* Build OK Button */

	ok = picture_button(b->window, _("OK"), ok_xpm);
	cancel = picture_button(b->window, _("Cancel"), cancel_xpm);

        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 2);

        b->firstentry = gtk_entry_new();
	b->middleentry = gtk_entry_new();
	b->lastentry = gtk_entry_new();
	b->maidenentry = gtk_entry_new();
	b->cityentry = gtk_entry_new();
	b->stateentry = gtk_entry_new();
	b->countryentry = gtk_entry_new();

        gtk_box_pack_start(GTK_BOX(bbox), ok, FALSE, FALSE, 10);
        gtk_box_pack_end(GTK_BOX(bbox), cancel, FALSE, FALSE, 10);

	/* Line 1 */
        label = gtk_label_new(_("First Name"));
	gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->firstentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 2 */

        label = gtk_label_new(_("Middle Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->middleentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 3 */

	label = gtk_label_new(_("Last Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->lastentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 4 */

	label = gtk_label_new(_("Maiden Name"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->maidenentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 5 */
	
	label = gtk_label_new(_("City"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->cityentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 6 */
        label = gtk_label_new(_("State"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->stateentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Line 7 */
        label = gtk_label_new(_("Country"));
        gtk_widget_show(label);
	
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_box_pack_end(GTK_BOX(hbox), b->countryentry, FALSE, FALSE, 5);

	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	/* Merge The Boxes */	

	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_box_pack_start(GTK_BOX(fbox), frame, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), bbox, FALSE, FALSE, 5);

        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                         GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                           GTK_SIGNAL_FUNC(do_find_info), b);

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

void show_find_email()
{
        GtkWidget *label;
        GtkWidget *bbox;
        GtkWidget *vbox;
        GtkWidget *topbox;
	GtkWidget *frame;
	GtkWidget *button;

        struct findbyemail *b = g_new0(struct findbyemail, 1);
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(b->window, 240, 110);
	gtk_window_set_policy(GTK_WINDOW(b->window), FALSE, FALSE, TRUE);
	gtk_widget_show(b->window);
        dialogwindows = g_list_prepend(dialogwindows, b->window); 

	frame = gtk_frame_new(_("Search for Buddy"));

	bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);

        b->emailentry = gtk_entry_new();

	/* Build OK Button */

	button = picture_button(b->window, _("OK"), ok_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(do_find_email), b);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	

	button = picture_button(b->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 5);	

        label = gtk_label_new(_("Email"));
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), b->emailentry, FALSE, FALSE, 5);

        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->emailentry), "activate",
                           GTK_SIGNAL_FUNC(do_find_email), b);

	gtk_container_add(GTK_CONTAINER(frame), vbox);
        gtk_widget_show(b->emailentry);
	gtk_widget_show(frame);
	gtk_widget_show(topbox);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - Find Buddy By Email"));
        gtk_window_set_focus(GTK_WINDOW(b->window), b->emailentry);
        gtk_container_add(GTK_CONTAINER(b->window), frame);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
        aol_icon(b->window->window);

        gtk_widget_show(b->window);
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
		c->link_dialog = gtk_window_new(GTK_WINDOW_DIALOG);
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
		gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
				   GTK_SIGNAL_FUNC(cancel_link), c);
		gtk_signal_connect(GTK_OBJECT(b->ok), "clicked",
				   GTK_SIGNAL_FUNC(do_add_link), b);

		gtk_container_add(GTK_CONTAINER(c->link_dialog  ), fbox);
		gtk_container_border_width(GTK_CONTAINER(c->link_dialog  ), 10);
		gtk_window_set_title(GTK_WINDOW(c->link_dialog  ), _("GAIM - Add URL"));
		gtk_window_set_focus(GTK_WINDOW(c->link_dialog  ), b->url);
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

void cancel_color(GtkWidget *widget, struct conversation *c)
{
 	if (c->palette && widget)
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->palette), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->color_dialog);
	gtk_widget_destroy(c->color_dialog);
	c->color_dialog = NULL;
}

void do_color(GtkWidget *widget, GtkColorSelection *colorsel)
{
	gdouble color[3];
	GdkColor text_color;
	struct conversation *c;
	char *open_tag;

	open_tag = g_malloc(30);

	gtk_color_selection_get_color (colorsel, color);

	c = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	/* GTK_IS_EDITABLE(c->entry); huh? */

	text_color.red = ((guint16)(color[0]*65535))>>8;
	text_color.green = ((guint16)(color[1]*65535))>>8;
	text_color.blue = ((guint16)(color[2]*65535))>>8;
	
	g_snprintf(open_tag, 23, "<FONT COLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green, text_color.blue);
	surround(c->entry, open_tag, "</FONT>");
	sprintf(debug_buff,"#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	debug_print(debug_buff);
	g_free(open_tag);
	cancel_color(NULL, c);
}


void show_color_dialog(struct conversation *c, GtkWidget *color)
{
	GtkWidget *colorsel;

    if (!c->color_dialog)
	{
    	c->color_dialog = gtk_color_selection_dialog_new(_("Select Text Color"));
		colorsel = GTK_COLOR_SELECTION_DIALOG(c->color_dialog)->colorsel;

		gtk_object_set_user_data(GTK_OBJECT(colorsel), c);
		
		gtk_signal_connect(GTK_OBJECT(c->color_dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(c->color_dialog)->ok_button), "clicked", GTK_SIGNAL_FUNC(do_color), colorsel);
		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(c->color_dialog)->cancel_button), "clicked", GTK_SIGNAL_FUNC(cancel_color), c);

		gtk_widget_realize(c->color_dialog);
		aol_icon(c->color_dialog->window);
	}

	gtk_widget_show(c->color_dialog);
	gdk_window_raise(c->color_dialog->window);
}

/*------------------------------------------------------------------------*/
/*  Font Selection Dialog                                                 */
/*------------------------------------------------------------------------*/

void cancel_font(GtkWidget *widget, struct conversation *c)
{	
	if (c->font && widget)
	{
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
    struct conversation *c = gtk_object_get_user_data(GTK_OBJECT(fontsel));
	
	if (c)
	{
		char *tmp = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));
		strncpy(c->current_fontname, tmp, sizeof(c->current_fontname));

		for (i = 0; i < strlen(c->current_fontname); i++)
		{
			if (c->current_fontname[i] == '-')
			{
				if (++j > 2)
					break;	
			}		
			else if (j == 2)
				c->current_fontface[k++] = c->current_fontname[i];
		}
		c->current_fontface[k] = '\0';

		sprintf(debug_buff, "Setting font face %s\n", c->current_fontface);
		debug_print(debug_buff);

		set_font_face(NULL, c);
	}
	else
	{
		if (fontface)
			g_free(fontface);
		
		fontface = g_malloc(64);
		fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontsel));
	
		for (i = 0; i < strlen(fontname); i++)
		{
			if (fontname[i] == '-')
			{
				if (++j > 2)
					break;	
			}		
			else if (j == 2)
				fontface[k++] = fontname[i];
		}
		fontface[k] = '\0';
	}
	
	cancel_font(NULL, c);
}

static GtkWidget *fontseld;

void destroy_fontsel(GtkWidget *w, gpointer d) {
	gtk_widget_destroy(fontseld);
	fontseld = NULL;
}

void apply_font_dlg(GtkWidget *w, GtkWidget *f) {
	int i, j = 0, k = 0;
	if (fontface)
		g_free(fontface);

	fontface = g_malloc(64);
	fontname = gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(fontseld));
	destroy_fontsel(0, 0);
	for (i = 0; i < strlen(fontname); i++) {
		if (fontname[i] == '-') {
			if (++j > 2) break;
		} else if (j == 2)
			fontface[k++] = fontname[i];
	}
	fontface[k] = '\0';
}

void show_font_dialog(struct conversation *c, GtkWidget *font)
{

	if (!font) { /* we came from the prefs dialog */
		if (fontseld) return;
		fontseld = gtk_font_selection_dialog_new(_("Select Font"));
		if (fontname)
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld), fontname);
		else
			gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(fontseld), DEFAULT_FONT_NAME);

		gtk_object_set_user_data(GTK_OBJECT(fontseld), NULL);
		gtk_signal_connect(GTK_OBJECT(fontseld), "delete_event", GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->cancel_button), "clicked", GTK_SIGNAL_FUNC(destroy_fontsel), NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontseld)->ok_button), "clicked", GTK_SIGNAL_FUNC(apply_font_dlg), NULL);
		gtk_widget_realize(fontseld);
		aol_icon(fontseld->window);
		gtk_widget_show(fontseld);
		gdk_window_raise(fontseld->window);
		return;
	}
	
	if (!c->font_dialog)
	{
		c->font_dialog = gtk_font_selection_dialog_new(_("Select Font"));

		if (font)
			gtk_object_set_user_data(GTK_OBJECT(c->font_dialog), c);
		else
			gtk_object_set_user_data(GTK_OBJECT(c->font_dialog), NULL);
			
		gtk_font_selection_dialog_set_font_name((GtkFontSelectionDialog *)c->font_dialog, DEFAULT_FONT_NAME);	
		
		gtk_signal_connect(GTK_OBJECT(c->font_dialog), "delete_event", GTK_SIGNAL_FUNC(delete_event_dialog), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->ok_button), "clicked", GTK_SIGNAL_FUNC(apply_font), c->font_dialog);
		gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(c->font_dialog)->cancel_button), "clicked", GTK_SIGNAL_FUNC(cancel_font), c);
	
		if (fontname)
			gtk_font_selection_dialog_set_font_name((GtkFontSelectionDialog *)c->font_dialog, fontname);
		else
			gtk_font_selection_dialog_set_font_name((GtkFontSelectionDialog *)c->font_dialog, DEFAULT_FONT_NAME);		
		
		gtk_widget_realize(c->font_dialog);
	
		aol_icon(c->font_dialog->window);
	}	
	gtk_widget_show(c->font_dialog);
	gdk_window_raise(c->font_dialog->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for import/export                                          */
/*------------------------------------------------------------------------*/

#define PATHSIZE 1024

/* see if a buddy list cache file for this user exists */

gboolean
bud_list_cache_exists( void )
{
	gboolean ret = FALSE;
	char path[PATHSIZE];
	char *file;
	struct stat sbuf;
	extern char g_screenname[];

	file = getenv( "HOME" );
	if ( file != (char *) NULL ) {
	       	sprintf( path, "%s/.gaim/%s.blist", file, g_screenname );
		if ( !stat(path, &sbuf) ) 
			ret = TRUE;
	}
	return ret;
}

/* if dummy is 0, save to ~/.gaim/screenname.blist. Else, let user choose */

void do_export(GtkWidget *w, void *dummy)
{
        FILE *f;
	gint show_dialog = (int) dummy;
        char *buf = g_malloc(BUF_LONG);
        char *file;
	char path[PATHSIZE];
	extern char g_screenname[];

	if ( show_dialog == 1 ) {
		file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(exportdialog));
		strncpy( path, file, PATHSIZE - 1 );
	}
	else {
		file = getenv( "HOME" );
		if ( file != (char *) NULL ) {
			FILE *dir;
			sprintf(buf, "%s/.gaim/", file);
			dir = fopen(buf, "r");
			if (!dir)
				mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR);
			else
				fclose(dir);
                        sprintf( path, "%s/.gaim/%s.blist", file, g_screenname );
		} else
			return;
	}
        if ((f = fopen(path,"w"))) {
                serv_build_config(buf, 8192 - 1);
                fprintf(f, "%s\n", buf);
                fclose(f);
                chmod(buf, S_IRUSR | S_IWUSR);
        } else if ( show_dialog == 1 ) {
                g_snprintf(buf, BUF_LONG / 2, _("Error writing file %s"), file);
                do_error_dialog(buf, _("Error"));
        }
	if ( show_dialog == 1 ) {
        	destroy_dialog(NULL, exportdialog);
        	exportdialog = NULL;
	}
        
        g_free(buf);
        
}

	
void show_export_dialog()
{
        char *buf = g_malloc(BUF_LEN);
        if (!exportdialog) {
                exportdialog = gtk_file_selection_new(_("Gaim - Export Buddy List"));

                gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(exportdialog));

                g_snprintf(buf, BUF_LEN - 1, "%s/gaim.buddy", getenv("HOME"));
                
                gtk_file_selection_set_filename(GTK_FILE_SELECTION(exportdialog), buf);
                gtk_signal_connect(GTK_OBJECT(exportdialog), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_dialog), exportdialog);
                
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(exportdialog)->ok_button),
                                   "clicked", GTK_SIGNAL_FUNC(do_export), (void*)1);
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(exportdialog)->cancel_button),
                                   "clicked", GTK_SIGNAL_FUNC(destroy_dialog), exportdialog);
                

	}

	g_free(buf);

        gtk_widget_show(exportdialog);
        gdk_window_raise(exportdialog->window);

}

/* if dummy is 0, then import from ~/.gaim/screenname.blist, else let user
   choose */

void do_import(GtkWidget *w, void *dummy)
{
	gint show_dialog = (int) dummy;
        GList *grp, *grp2;
        char *buf = g_malloc(BUF_LONG);
        char *buf2;
        char *first = g_malloc(64);
	char *file;
	char path[PATHSIZE];
        FILE *f;
	extern char g_screenname[];

        if ( show_dialog == 1 ) {
        	file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(importdialog));
                strncpy( path, file, PATHSIZE - 1 );
        }
        else {
                file = getenv( "HOME" );
                if ( file != (char *) NULL )
                        sprintf( path, "%s/.gaim/%s.blist", file, g_screenname );
                else
			return;
        }

        if (!(f = fopen(path,"r"))) {
		if ( show_dialog == 1 ) {
                	g_snprintf(buf, BUF_LONG / 2, _("Error reading file %s"), file);
                	do_error_dialog(buf, _("Error"));
                	destroy_dialog(NULL, importdialog);
                	importdialog = NULL;
		}
                g_free(buf);
		g_free(first);
                return;
        }
                
        fgets(first, 64, f);

        if (!strcasecmp(first, "Config {\n")) {
		if ( show_dialog == 1 ) {
                	destroy_dialog(NULL, importdialog);
                	importdialog = NULL;
		}
		g_free(buf);
		g_free(first);
		fclose( f );
                return;
        } else if (buf[0] == 'm') {
                buf2 = buf;
                buf = g_malloc(8193);
                g_snprintf(buf, 8192, "toc_set_config {%s}\n", buf2);
                g_free(buf2);
        }

                
        fseek(f, 0, SEEK_SET);

        fread(buf, BUF_LONG, 1, f);

        grp = groups;
	
        while(grp) {
                grp2 = grp->next;
		remove_group((struct group *)grp->data);
		grp = grp2;
        }
        
        parse_toc_buddy_list(buf, 1);

        serv_save_config();
        
        build_edit_tree();
        build_permit_tree();
        
	fclose( f );

	if ( show_dialog == 1 ) {
		/* save what we just did to cache */

		do_export( (GtkWidget *) NULL, 0 );
               	destroy_dialog(NULL, importdialog);
               	importdialog = NULL;
	} 
        
        g_free(buf);
        g_free(first);
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
                                   "clicked", GTK_SIGNAL_FUNC(do_import), (void*)1);
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(importdialog)->cancel_button),
                                   "clicked", GTK_SIGNAL_FUNC(destroy_dialog), importdialog);
                

        }

	g_free(buf);
        gtk_widget_show(importdialog);
        gdk_window_raise(importdialog->window);
}


/*------------------------------------------------------------------------*/
/*  The dialog for new away messages                                      */
/*------------------------------------------------------------------------*/

void create_mess(GtkWidget *widget, struct create_away *ca)
{
        struct away_message *b;
	gchar  *away_message;
	guint  text_len;
	int    is_checked;

	/* Grab the appropriate data */       
	b = g_new0(struct away_message, 1);
	g_snprintf(b->name, sizeof(b->name), "%s", gtk_entry_get_text(GTK_ENTRY(ca->entry)));

	/* Get proper Length */
	text_len = gtk_text_get_length(GTK_TEXT(ca->text));
	away_message = gtk_editable_get_chars(GTK_EDITABLE(ca->text), 0, text_len);

	g_snprintf(b->message, sizeof(b->message), "%s", away_message);
	g_free(away_message);
	is_checked = GTK_TOGGLE_BUTTON(ca->checkbx)->active;
	
	if (is_checked) {
                away_messages = g_list_append(away_messages, b);
                save_prefs();
                do_away_menu();
                if (prefs_away_list != NULL)
                        gtk_list_select_item(GTK_LIST(prefs_away_list), g_list_index(away_messages, b));
	}

	/* stick it on the away list */
	if (!strlen(b->name))
		g_snprintf(b->name, sizeof(b->name), "I'm away!");
	do_away_message(NULL, b);
        
        destroy_dialog(NULL, ca->window);
}

void create_away_mess(GtkWidget *widget, void *dummy)
{
	GtkWidget *bbox;
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
	ca->window = gtk_window_new(GTK_WINDOW_DIALOG);
 	gtk_widget_set_usize(ca->window, 275, 200); 
	gtk_widget_show(ca->window);
	gtk_container_border_width(GTK_CONTAINER(ca->window), 5);
	gtk_window_set_policy(GTK_WINDOW(ca->window), FALSE, FALSE, TRUE);
	gtk_window_set_title(GTK_WINDOW(ca->window), _("Gaim - New away message"));
	gtk_signal_connect(GTK_OBJECT(ca->window),"delete_event",
		           GTK_SIGNAL_FUNC(destroy_dialog), ca->window);

	/* Set up our frame */

	frame = gtk_frame_new(_("New away message"));

	/* set up container boxes */
	bbox = gtk_hbox_new(FALSE, 0);
	fbox = gtk_vbox_new(FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	titlebox = gtk_hbox_new(FALSE, 0);
	tbox = gtk_vbox_new(FALSE, 0);

	/* Make a label for away entry */
	label = gtk_label_new(_("Away title: "));
	gtk_box_pack_start(GTK_BOX(titlebox), label, TRUE, TRUE, 5);
	gtk_widget_show(label);

	/* make away title entry */
	ca->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(titlebox), ca->entry, TRUE, TRUE, 5);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_show(sw);

	/* create and format text box */
	ca->text = gtk_text_new(NULL, NULL);
	gtk_text_set_word_wrap(GTK_TEXT(ca->text), TRUE);
	gtk_text_set_editable(GTK_TEXT(ca->text), TRUE );
	gtk_container_add(GTK_CONTAINER(sw), ca->text);
	gtk_widget_show(ca->text);
	gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 5);   

	button = picture_button(ca->window, _("Away"), save_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(create_mess), ca);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	button = picture_button(ca->window, _("Cancel"), cancel_xpm);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), ca->window);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);

	/* Checkbox for showing away msg */
	ca->checkbx = gtk_check_button_new_with_label(_("Save for later use"));

	/* pack boxes where they belong */
	gtk_box_pack_start(GTK_BOX(fbox), titlebox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), bbox, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(fbox), ca->checkbx, TRUE, TRUE, 5);

	gtk_container_add(GTK_CONTAINER(frame), fbox);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
	gtk_box_pack_start(GTK_BOX(tbox), frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(tbox), hbox, TRUE, FALSE, 5);
	
	gtk_container_add(GTK_CONTAINER(ca->window), tbox);

	/* let the world see what we have done. */
	gtk_widget_show(label);
	gtk_widget_show(ca->checkbx);
	gtk_widget_show(ca->entry);
	gtk_widget_show(titlebox);
	gtk_widget_show(hbox);
	gtk_widget_show(tbox);
	gtk_widget_show(bbox);
	gtk_widget_show(fbox);
	gtk_widget_show(frame);

        gtk_widget_realize(ca->window);
        aol_icon(ca->window->window);
}

/* smiley dialog */

void close_smiley_dialog(GtkWidget *widget, struct conversation *c)
{	
	if (c->smiley)
	{
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->smiley), FALSE);
		set_state_lock(0);
	}
	dialogwindows = g_list_remove(dialogwindows, c->smiley_dialog);
	gtk_widget_destroy(c->smiley_dialog);
	c->smiley_dialog = NULL;	
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
	
	for (i = 0; i < FACE_TOTAL; i++)
		if (smiley_array[i] == 1)
			break;

	switch (i)
	{
		case (FACE_ANGEL): smiley_text = g_strndup("O:-)", strlen("O:-)")); break;
		case (FACE_BIGSMILE): smiley_text = g_strndup(":-D", strlen(":-D")); break;
		case (FACE_BURP): smiley_text = g_strndup(":-!", strlen(":-!")); break;
		case (FACE_CROSSEDLIPS): smiley_text = g_strndup(":-X", strlen(":-X")); break;
		case (FACE_CRY): smiley_text = g_strndup(":'(", strlen(":'(")); break;
		case (FACE_EMBARRASSED): smiley_text = g_strndup(":-[", strlen(":-[")); break;
		case (FACE_KISS): smiley_text = g_strndup(":-*", strlen(":-*")); break;
		case (FACE_MONEYMOUTH): smiley_text = g_strndup(":-$", strlen(":-$")); break;
		case (FACE_SAD): smiley_text = g_strndup(":-(", strlen(":-(")); break;
		case (FACE_SCREAM): smiley_text = g_strndup("=-O", strlen("=-O")); break;
		case (FACE_SMILE): smiley_text = g_strndup(":-)", strlen(":-)")); break;
		case (FACE_SMILE8): smiley_text = g_strndup("8-)", strlen("8-)")); break;
		case (FACE_THINK): smiley_text = g_strndup(":-/", strlen(":-/")); break;
		case (FACE_TONGUE): smiley_text = g_strndup(":-p", strlen(":-p")); break;
		case (FACE_WINK): smiley_text = g_strndup(";-)", strlen(";-)")); break;
		case (FACE_YELL): smiley_text = g_strndup(">:o", strlen(">:o")); break;
		default: smiley_text = g_strndup(":-)", strlen(":-)")); break;
	}
	
	surround(c->entry, smiley_text, "");

	g_free(smiley_text);
	
	close_smiley_dialog(NULL, c);
		
	return;
}

static void toolbar_add_smiley(struct conversation *c, GtkWidget *bar, char **xpm, GtkWidget *win, int face)
{
	GtkWidget *tpm;
	GdkBitmap *mask;
	GdkPixmap *dpm;
	GtkWidget *button;

	dpm = gdk_pixmap_create_from_xpm_d(win->window, &mask, &win->style->white, xpm);
	tpm = gtk_pixmap_new(dpm, mask);
	gtk_widget_show(tpm);
	gdk_pixmap_unref(dpm);
	button = gtk_toolbar_append_element(GTK_TOOLBAR(bar), GTK_TOOLBAR_CHILD_BUTTON, NULL, NULL, NULL, NULL, tpm, GTK_SIGNAL_FUNC(set_smiley_array), (int *)face);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(insert_smiley_text), c);

	/* these look really weird with borders */
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
}
	
void show_smiley_dialog(struct conversation *c, GtkWidget *widget)
{
	GtkWidget *dialog;
	GtkWidget *vbox, *smiley_box_1, *smiley_box_2, *smiley_box_3, *smiley_box_4;
	GtkWidget *win;
	GtkWidget *bbox;

	if (c->smiley_dialog)
		return;

	win = c->window;
		
	dialog = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_policy(GTK_WINDOW(dialog), 0, 0, 1);

	/* setup boxes */
	vbox = gtk_vbox_new(TRUE, 5);
	bbox = gtk_hbox_new(FALSE, 5);

	smiley_box_1 = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	smiley_box_2 = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	smiley_box_3 = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);
	smiley_box_4 = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_TOOLBAR_ICONS);

	/* setup buttons */

	/* pack buttons */
	toolbar_add_smiley(c, smiley_box_1, angel_xpm, win, FACE_ANGEL);
	toolbar_add_smiley(c, smiley_box_1, bigsmile_xpm, win, FACE_BIGSMILE);
	toolbar_add_smiley(c, smiley_box_1, burp_xpm, win, FACE_BURP);
	toolbar_add_smiley(c, smiley_box_1, crossedlips_xpm, win, FACE_CROSSEDLIPS);

	toolbar_add_smiley(c, smiley_box_2, cry_xpm, win, FACE_CRY);
	toolbar_add_smiley(c, smiley_box_2, embarrassed_xpm, win, FACE_EMBARRASSED);
	toolbar_add_smiley(c, smiley_box_2, kiss_xpm, win, FACE_KISS);
	toolbar_add_smiley(c, smiley_box_2, moneymouth_xpm, win, FACE_MONEYMOUTH);

	toolbar_add_smiley(c, smiley_box_3, sad_xpm, win, FACE_SAD);
	toolbar_add_smiley(c, smiley_box_3, scream_xpm, win, FACE_SCREAM);
	toolbar_add_smiley(c, smiley_box_3, smile_xpm, win, FACE_SMILE);
	toolbar_add_smiley(c, smiley_box_3, smile8_xpm, win, FACE_SMILE8);

	toolbar_add_smiley(c, smiley_box_4, think_xpm, win, FACE_THINK);
	toolbar_add_smiley(c, smiley_box_4, tongue_xpm, win, FACE_TONGUE);
	toolbar_add_smiley(c, smiley_box_4, wink_xpm, win, FACE_WINK);
	toolbar_add_smiley(c, smiley_box_4, yell_xpm, win, FACE_YELL);

	/* pack containers */
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box_1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box_2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box_3, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), smiley_box_4, TRUE, TRUE, 0);

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

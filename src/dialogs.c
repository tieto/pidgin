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

static GtkWidget *imdialog = NULL; /*I only want ONE of these :) */
static GList *dialogwindows = NULL;
static GtkWidget *linkdialog, *colordialog, *exportdialog, *importdialog, *logdialog;

/*static void accept_callback(GtkWidget *widget, struct file_transfer *t);*/

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
	struct hostent *host;
	struct sockaddr_in site;
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
	
	host = gethostbyname(REG_SRVR);
	if (!host) 
	{
		printf("Error Resolving Mail Server.\n");
		return -1;
	}

	site.sin_family = AF_INET;
	site.sin_addr.s_addr = *(long *)(host->h_addr);
	site.sin_port = htons(REG_PORT);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		printf("Socket Error.\n");
		return -1;
	}
	
	if (connect(sock, (struct sockaddr *)&site, sizeof(site)))
	{
		printf("Error Connecting to Socket.\n");
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

	if (dest == logdialog)
		logdialog = NULL;

	if (dest == colordialog)
		colordialog = NULL;

	if (dest == linkdialog)
		linkdialog = NULL;

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

	do_im_back(NULL, NULL);

        if (imdialog) {
                destroy_dialog(NULL, imdialog);
                imdialog = NULL;
        }
        
	if (linkdialog) {
		destroy_dialog(NULL, linkdialog);
		linkdialog = NULL;
	}
	if (colordialog) {
		destroy_dialog(NULL, colordialog);
		colordialog = NULL;
	}

        if (exportdialog) {
                destroy_dialog(NULL, exportdialog);
                exportdialog = NULL;
        }

        if (importdialog) {
                destroy_dialog(NULL, exportdialog);
                importdialog = NULL;
        }
	
	if (logdialog) {
		destroy_dialog(NULL, logdialog);
		logdialog = NULL;
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
        struct warning *w = g_new0(struct warning, 1);
        
        char *buf = g_malloc(128);
        w->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, w->window);
        cancel = gtk_button_new_with_label("Cancel");
        warn = gtk_button_new_with_label("Warn");
        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);

        /* Put the buttons in the box */
        gtk_box_pack_start(GTK_BOX(bbox), warn, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        g_snprintf(buf, 127, "Do you really want to warn %s?", who);
        label = gtk_label_new(buf);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        gtk_widget_show(label);
        w->anon = gtk_check_button_new_with_label("Warn anonymously?");
        gtk_box_pack_start(GTK_BOX(vbox), w->anon, TRUE, TRUE, 0);

        label = gtk_label_new("Anonymous warnings are less harsh.");
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        gtk_widget_show(label);

        w->who = who;
		
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(w->window), "delete_event",
                           GTK_SIGNAL_FUNC(destroy_dialog), w->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), w->window);
        gtk_signal_connect(GTK_OBJECT(warn), "clicked",
                           GTK_SIGNAL_FUNC(do_warn), w);
        /* Finish up */
        gtk_widget_show(warn);
        gtk_widget_show(cancel);
        gtk_widget_show(w->anon);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(w->window), "Gaim - Warn user?");
        gtk_container_add(GTK_CONTAINER(w->window), vbox);
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

        label = gtk_label_new(message);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	close = gtk_button_new_with_label("Close");
	gtk_widget_show(close);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->vbox),
		label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(d)->action_area), 
		close, FALSE, FALSE, 5);
		

	gtk_window_set_title(GTK_WINDOW(d), title);
	gtk_signal_connect(GTK_OBJECT(close), "clicked", GTK_SIGNAL_FUNC(destroy_dialog), d);
        gtk_widget_realize(d);
	aol_icon(d->window);

	gtk_widget_show(d);
}



void show_error_dialog(char *c)
{

	int no = atoi(c);
	char *w = strtok(NULL, ":");
	char buf[256];
	char buf2[32];
 	
	
        switch(no) {
        case 69:
                g_snprintf(buf, sizeof(buf), "Unable to write file %s.", w);
                break;
        case 169:
                g_snprintf(buf, sizeof(buf), "Unable to read file %s.", w);
                break;
        case 269:
                g_snprintf(buf, sizeof(buf), "Message too long, last %s bytes truncated.", w);
                break;
        case 901:
                g_snprintf(buf, sizeof(buf), "%s not currently logged in.", w);
                break;
        case 902:
                g_snprintf(buf, sizeof(buf), "Warning of %s not allowed.", w);
                break;
        case 903:
                g_snprintf(buf, sizeof(buf), "A message has been dropped, you are exceeding the server speed limit.");
                break;
        case 950:
                g_snprintf(buf, sizeof(buf), "Chat in %s is not available.", w);
                break;
        case 960:
                g_snprintf(buf, sizeof(buf), "You are sending messages too fast to %s.", w);
                break;
        case 961:
                g_snprintf(buf, sizeof(buf), "You missed an IM from %s because it was too big.", w);
                break;
        case 962:
                g_snprintf(buf, sizeof(buf), "You missed an IM from %s because it was sent too fast.", w);
                break;
        case 970:
                g_snprintf(buf, sizeof(buf), "Failure.");
                break;
        case 971:
                g_snprintf(buf, sizeof(buf), "Too many matches.");
                break;
        case 972:
                g_snprintf(buf, sizeof(buf), "Need more qualifiers.");
                break;
        case 973:
                g_snprintf(buf, sizeof(buf), "Dir service temporarily unavailable.");
                break;
        case 974:
                g_snprintf(buf, sizeof(buf), "Email lookup restricted.");
                break;
        case 975:
                g_snprintf(buf, sizeof(buf), "Keyword ignored.");
                break;
        case 976:
                g_snprintf(buf, sizeof(buf), "No keywords.");
                break;
        case 977:
                g_snprintf(buf, sizeof(buf), "User has no directory information.");
                /* g_snprintf(buf, sizeof(buf), "Language not supported."); */
                break;
        case 978:
                g_snprintf(buf, sizeof(buf), "Country not supported.");
                break;
        case 979:
                g_snprintf(buf, sizeof(buf), "Failure unknown: %s.", w);
                break;
        case 980:
                g_snprintf(buf, sizeof(buf), "Incorrect nickname or password.");
                break;
        case 981:
                g_snprintf(buf, sizeof(buf), "The service is temporarily unavailable.");
                break;
        case 982:
                g_snprintf(buf, sizeof(buf), "Your warning level is currently too high to log in.");
                break;
        case 983:
                g_snprintf(buf, sizeof(buf), "You have been connecting and disconnecting too frequently.  Wait ten minutes and try again.  If you continue to try, you will need to wait even longer.");
                break;
        case 989:
                g_snprintf(buf, sizeof(buf), "An unknown signon error has occurred: %s.", w);
                break;
        default:
                g_snprintf(buf, sizeof(buf), "An unknown error, %d, has occured.  Info: %s", no, w);
	}
	
	g_snprintf(buf2, sizeof(buf2), "Gaim - Error %d", no);


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
        ok = gtk_button_new_with_label("OK");
        box = gtk_vbox_new(FALSE, 10);

	if (ee == 0)
		label = gtk_label_new("Amazing!  Simply Amazing!");
	else if (ee == 1)
		label = gtk_label_new("Pimpin\' Penguin Style! *Waddle Waddle*");
	else
		label = gtk_label_new("You should be me.  I'm so cute!");

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
        GtkWidget *cancel;
	GtkWidget *ok;
	GtkWidget *imentry;
        GtkWidget *vbox;
        GtkWidget *ebox;
        GtkWidget *bbox;
        GtkWidget *label;

        if (!imdialog) {

                imdialog = gtk_window_new(GTK_WINDOW_DIALOG);
                cancel = gtk_button_new_with_label("Cancel");
                ok = gtk_button_new_with_label("OK");
                bbox = gtk_hbox_new(TRUE, 10);
                vbox = gtk_vbox_new(FALSE, 5);
                ebox = gtk_hbox_new(FALSE, 2);

                /* Put the buttons in the box */
                gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 10);
                gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

                label = gtk_label_new("IM who: ");
                gtk_box_pack_start(GTK_BOX(ebox), label, TRUE, TRUE, 10);
                gtk_widget_show(label);

                imentry = gtk_entry_new();
                gtk_box_pack_start(GTK_BOX(ebox), imentry, TRUE, TRUE, 10);

                gtk_box_pack_start(GTK_BOX(vbox), ebox, FALSE, FALSE, 5);
                gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

                /* Handle closes right */
		gtk_signal_connect(GTK_OBJECT(imentry), "activate",
				   GTK_SIGNAL_FUNC(do_im), imentry);
                gtk_signal_connect(GTK_OBJECT(imdialog), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);
                gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                                   GTK_SIGNAL_FUNC(destroy_dialog), imdialog);
                gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                                   GTK_SIGNAL_FUNC(do_im), imentry);
                /* Finish up */
                gtk_widget_show(ok);
                gtk_widget_show(cancel);
                gtk_widget_show(ebox);
                gtk_widget_show(imentry);
                gtk_widget_show(bbox);
                gtk_widget_show(vbox);
                gtk_window_set_title(GTK_WINDOW(imdialog), "Gaim - IM user");
                gtk_container_add(GTK_CONTAINER(imdialog), vbox);
                gtk_widget_grab_focus(imentry);
                gtk_widget_realize(imdialog);
                aol_icon(imdialog->window);

        }
        gtk_widget_show(imdialog);
}


/*------------------------------------------------------------------------*/
/*  The dialog for adding buddies                                         */
/*------------------------------------------------------------------------*/

void do_add_buddy(GtkWidget *w, struct addbuddy *a)
{
	char *grp, *who;
        struct conversation *c;
        
	who = gtk_entry_get_text(GTK_ENTRY(a->entry));
        grp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry));

        c = find_conversation(who);

        add_buddy(grp, who);

        if (c != NULL)
		gtk_label_set_text(GTK_LABEL(GTK_BIN(c->add_button)->child), "Remove");
        
        build_edit_tree();

        serv_save_config();

        serv_add_buddy(who);

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
                tmp2 = g_strdup("Buddies");
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


void show_add_buddy(char *buddy, char *group)
{
	GtkWidget *cancel;
	GtkWidget *add;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;
        GtkWidget *topbox;
        struct addbuddy *a = g_new0(struct addbuddy, 1);
        
        a->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, a->window);
        cancel = gtk_button_new_with_label("Cancel");
        add = gtk_button_new_with_label("Add");
        bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);
        a->entry = gtk_entry_new();
        a->combo = gtk_combo_new();
        /* Fix the combo box */
        gtk_combo_set_popdown_strings(GTK_COMBO(a->combo), groups_tree());
        /* Put the buttons in the box */
        gtk_box_pack_start(GTK_BOX(bbox), add, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        label = gtk_label_new("Add");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), a->entry, FALSE, FALSE, 5);
        if (buddy != NULL)
                gtk_entry_set_text(GTK_ENTRY(a->entry), buddy);

        label = gtk_label_new("to group");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), a->combo, FALSE, FALSE, 5);

        if (group != NULL)
                gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(a->combo)->entry), group);

        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

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
        gtk_window_set_title(GTK_WINDOW(a->window), "Gaim - Add Buddy");
        gtk_window_set_focus(GTK_WINDOW(a->window), a->entry);
        gtk_container_add(GTK_CONTAINER(a->window), vbox);
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
	GtkWidget *cancel;
	GtkWidget *ok;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox;

        struct addbp *b = g_new0(struct addbp, 1);
        
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, b->window);
        cancel = gtk_button_new_with_label("Cancel");
        ok = gtk_button_new_with_label("OK");
        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);
        b->nameentry = gtk_entry_new();
        b->messentry = gtk_entry_new();
	
        /* Put the buttons in the box */
        gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        label = gtk_label_new("Buddy To Pounce:");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), b->nameentry, FALSE, FALSE, 0);

	b->openwindow = gtk_check_button_new_with_label("Open IM Window on Buddy Logon");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->openwindow), FALSE);
	
	b->sendim = gtk_check_button_new_with_label("Send IM on Buddy Logon"); 
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->sendim), TRUE);

	gtk_widget_show(b->openwindow);
	gtk_widget_show(b->sendim);
	gtk_box_pack_start(GTK_BOX(vbox), b->openwindow, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), b->sendim, FALSE, FALSE, 0);

        label = gtk_label_new("Message to send:");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), b->messentry, FALSE, FALSE, 0);


        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

        /* Handle closes right */
        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                           GTK_SIGNAL_FUNC(do_new_bp), b);
        gtk_signal_connect(GTK_OBJECT(b->messentry), "activate",
                           GTK_SIGNAL_FUNC(do_new_bp), b);

        
        /* Finish up */
        gtk_widget_show(ok);
        gtk_widget_show(cancel);
        gtk_widget_show(b->nameentry);
        gtk_widget_show(b->messentry);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - New Buddy Pounce");
        if (name != NULL) {
                gtk_entry_set_text(GTK_ENTRY(b->nameentry), name);
                gtk_window_set_focus(GTK_WINDOW(b->window), b->messentry);
        } else
                gtk_window_set_focus(GTK_WINDOW(b->window), b->nameentry);
        gtk_container_add(GTK_CONTAINER(b->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
        aol_icon(b->window->window);

	gtk_widget_show(b->window);
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
	GtkWidget *top;
	GtkWidget *table;

	struct set_dir_dlg *b = g_new0(struct set_dir_dlg, 1);

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	dialogwindows = g_list_prepend(dialogwindows, b->window);
	
	b->cancel = gtk_button_new_with_label("Cancel");
	b->save = gtk_button_new_with_label("Save");

	bot = gtk_hbox_new(TRUE, 10);
	top = gtk_vbox_new(FALSE, 10);

	gtk_widget_show(b->save);
	gtk_widget_show(b->cancel);

	gtk_box_pack_start(GTK_BOX(bot), b->save, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(bot), b->cancel, FALSE, FALSE, 5);

	gtk_widget_show(bot);

	table = gtk_table_new(10, 2, FALSE);
	
	b->first = gtk_entry_new();
	b->middle = gtk_entry_new();
	b->last = gtk_entry_new();
	b->maiden = gtk_entry_new();
	b->city = gtk_entry_new();
	b->state = gtk_entry_new();
	b->country = gtk_entry_new();
	b->web = gtk_check_button_new_with_label("Allow Web Searches To Find Your Info");
	
	label = gtk_label_new("First Name");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), b->first, 1, 2, 0, 1);
	
        label = gtk_label_new("Middle Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
        gtk_table_attach_defaults(GTK_TABLE(table), b->middle, 1, 2, 1, 2);  

        label = gtk_label_new("Last Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
        gtk_table_attach_defaults(GTK_TABLE(table), b->last, 1, 2, 2, 3);  

        label = gtk_label_new("Maiden Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);
        gtk_table_attach_defaults(GTK_TABLE(table), b->maiden, 1, 2, 3, 4);  

        label = gtk_label_new("City");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 4, 5);
        gtk_table_attach_defaults(GTK_TABLE(table), b->city, 1, 2, 4, 5);  

	label = gtk_label_new("State");        
	gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 5, 6);
        gtk_table_attach_defaults(GTK_TABLE(table), b->state, 1, 2, 5, 6);

        label = gtk_label_new("Country");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 6, 7);
        gtk_table_attach_defaults(GTK_TABLE(table), b->country, 1, 2, 6, 7);

	gtk_table_attach_defaults(GTK_TABLE(table), b->web, 0, 2, 8, 9);

	gtk_widget_show(table);
	gtk_box_pack_start(GTK_BOX(top), table, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(top), bot, FALSE, FALSE, 5);

        gtk_widget_show(b->first); 
        gtk_widget_show(b->middle);
        gtk_widget_show(b->last); 
        gtk_widget_show(b->maiden);
        gtk_widget_show(b->city);
	gtk_widget_show(b->state);
	gtk_widget_show(b->country);
	gtk_widget_show(b->web);

	gtk_widget_show(top);

        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(b->save), "clicked",                                                      GTK_SIGNAL_FUNC(do_set_dir), b);   	
	
	gtk_container_add(GTK_CONTAINER(b->window), top);
	gtk_container_border_width(GTK_CONTAINER(b->window), 10);
 	gtk_widget_set_usize(b->window, 530, 280); 
	gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Set Dir Info");
        gtk_window_set_focus(GTK_WINDOW(b->window), b->first);
        gtk_widget_realize(b->window);
	aol_icon(b->window->window);

	gtk_widget_show(b->window);	
}

void do_change_password(GtkWidget *widget, struct passwddlg *b)
{
	gchar *orig, *new1, *new2;
	gchar *buf;

	orig = gtk_entry_get_text(GTK_ENTRY(b->original));
	new1 = gtk_entry_get_text(GTK_ENTRY(b->new1));
	new2 = gtk_entry_get_text(GTK_ENTRY(b->new2));

	if (strcasecmp(new1, new2)) {
		do_error_dialog("New Passwords Do Not Match", "Gaim - Change Password Error");
		return ;
	}

	if ((strlen(orig) < 1) || (strlen(new1) < 1) || (strlen(new2) < 1)) {
		do_error_dialog("Fill out all fields completely", "Gaim - Change Password Error");
		return;
	}

	buf = g_malloc(BUF_LONG);
	g_snprintf(buf, BUF_LONG, "toc_change_passwd %s %s", orig, new1);
	sflap_send(buf, strlen(buf), TYPE_DATA);
	g_free(buf);
	
	destroy_dialog(NULL, b->window);
	g_free(b);
}

void show_change_passwd()
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *table;
	struct passwddlg *b = g_new0(struct passwddlg, 1);

	b->window = gtk_window_new(GTK_WINDOW_DIALOG);
	dialogwindows = g_list_prepend(dialogwindows, b->window);

	b->ok = gtk_button_new_with_label("Ok");
	b->cancel = gtk_button_new_with_label("Cancel");

	gtk_widget_show(b->ok);
	gtk_widget_show(b->cancel);

	table = gtk_table_new(3, 2, TRUE);
	


	label = gtk_label_new("Original Password");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	b->original = gtk_entry_new();
	gtk_widget_show(b->original);
	gtk_table_attach_defaults(GTK_TABLE(table), b->original, 1, 2, 0, 1);

        label = gtk_label_new("New Password");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
        b->new1 = gtk_entry_new();
        gtk_widget_show(b->new1);
        gtk_table_attach_defaults(GTK_TABLE(table), b->new1, 1, 2, 1, 2);

        label = gtk_label_new("New Password (again)");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
        b->new2 = gtk_entry_new();
        gtk_widget_show(b->new2);
        gtk_table_attach_defaults(GTK_TABLE(table), b->new2, 1, 2, 2, 3);

	gtk_widget_show(table);

	vbox = gtk_vbox_new(TRUE, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 5);
	
	hbox = gtk_hbox_new(TRUE, TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), b->ok, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), b->cancel, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);
	
	gtk_container_add(GTK_CONTAINER(b->window), vbox);
	gtk_widget_show(vbox);

	gtk_container_border_width(GTK_CONTAINER(b->window), 10);
	gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Password Change");
	gtk_widget_show(b->window);
	
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

	b->cancel = gtk_button_new_with_label("Cancel");
	b->save = gtk_button_new_with_label("Save");

	bot = gtk_hbox_new(TRUE, 10);
	top = gtk_vbox_new(FALSE, 10);

	gtk_widget_show(b->save);
	gtk_widget_show(b->cancel);

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

	gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Set User Info");
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

	cancel = gtk_button_new_with_label("Cancel");
	ok = gtk_button_new_with_label("Send");

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

void g_show_info(char *url)
{
        GtkWidget *ok;
        GtkWidget *label;
	GtkWidget *text;
        GtkWidget *bbox;
        GtkWidget *sw;
        char *url_text;

        struct info_dlg *b = g_new0(struct info_dlg, 1);

        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, b->window);
        gtk_container_border_width(GTK_CONTAINER(b->window), 5);
        bbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(b->window), bbox);

        ok = gtk_button_new_with_label("OK");
	gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   GTK_SIGNAL_FUNC(destroy_dialog), b->window);

        label = gtk_label_new("Below are the results of your search: ");

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

	gtk_widget_realize(b->window);
	aol_icon(b->window->window);
	gtk_widget_show_all(b->window);

	url_text = grab_url(url);
	gtk_html_append_text(GTK_HTML(b->text), url_text, 0);
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
        struct addperm *p = g_new0(struct addperm, 1);

        p->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, p->window);
        cancel = gtk_button_new_with_label("Cancel");
        add = gtk_button_new_with_label("Add");
        bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);
        rbox = gtk_vbox_new(FALSE, 5);
        p->entry = gtk_entry_new();

        if (who != NULL)
                gtk_entry_set_text(GTK_ENTRY(p->entry), who);

        which = gtk_radio_button_new_with_label(NULL, "Deny");
        gtk_box_pack_start(GTK_BOX(rbox), which, FALSE, FALSE, 0);
        gtk_object_set_user_data(GTK_OBJECT(which), (int *)1);
        gtk_widget_show(which);

        which = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(which)), "Permit");
        gtk_box_pack_start(GTK_BOX(rbox), which, FALSE, FALSE, 0);
        gtk_object_set_user_data(GTK_OBJECT(which), (int *)2);
        gtk_widget_show(which);
                
        /* Put the buttons in the box */
        gtk_box_pack_start(GTK_BOX(bbox), add, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);
		
        label = gtk_label_new("Add");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), p->entry, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), rbox, FALSE, FALSE, 5);
        /* And the boxes in the box */
        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);


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
        gtk_window_set_title(GTK_WINDOW(p->window), "Gaim - Add Permit/Deny");
        gtk_window_set_focus(GTK_WINDOW(p->window), p->entry);
        gtk_container_add(GTK_CONTAINER(p->window), vbox);
        gtk_widget_realize(p->window);
        aol_icon(p->window->window);

        gtk_widget_show(p->window);
}


/*------------------------------------------------------------------------*/
/*  Function Called To Add A Log                                          */
/*------------------------------------------------------------------------*/

void do_log(GtkWidget *w, char *name)
{
        struct log_conversation *l;
	struct conversation *c;
        char buf[128];

	c = find_conversation(name);
        if (!find_log_info(name)) {
                l = (struct log_conversation *)g_new0(struct log_conversation, 1);
                strcpy(l->name, name);
                strcpy(l->filename, gtk_file_selection_get_filename(GTK_FILE_SELECTION(logdialog)));
                log_conversations = g_list_append(log_conversations, l);

                if (c != NULL)
                {
                        g_snprintf(buf, sizeof(buf), LOG_CONVERSATION_TITLE, c->name);
                        gtk_window_set_title(GTK_WINDOW(c->window), buf);
                }
        }

        save_prefs();

        destroy_dialog(NULL, logdialog);
        logdialog = NULL;
} 

static void cancel_log(GtkWidget *w, char *name)
{
	
	struct conversation *c = gtk_object_get_user_data(GTK_OBJECT(logdialog));

	if (c != NULL)
        {
        set_state_lock(1);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(c->log_button), FALSE);
        set_state_lock(0);
        }
	destroy_dialog(NULL, logdialog);
}

void show_log_dialog(char *bname)
{
	char *buf = g_malloc(BUF_LEN);
        struct conversation *c = find_conversation(bname);

	
	if (!logdialog) {
		logdialog = gtk_file_selection_new("Gaim - Log Conversation");
		
		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(logdialog));	

                gtk_object_set_user_data(GTK_OBJECT(logdialog), c);
		
		g_snprintf(buf, BUF_LEN - 1, "%s/%s.log", getenv("HOME"), bname);
		
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(logdialog), buf);
		gtk_signal_connect(GTK_OBJECT(logdialog), "delete_event", GTK_SIGNAL_FUNC(cancel_log), c);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(logdialog)->ok_button), "clicked", GTK_SIGNAL_FUNC(do_log), bname);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(logdialog)->cancel_button), "clicked", GTK_SIGNAL_FUNC(cancel_log), bname);
	}

	g_free(buf);

	gtk_widget_show(logdialog);
	gdk_window_raise(logdialog->window);
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
        GtkWidget *topbox;

	struct findbyinfo *b = g_new0(struct findbyinfo, 1);
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, b->window);

        cancel = gtk_button_new_with_label("Cancel");
        ok = gtk_button_new_with_label("OK");

        bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_table_new(7, 2, TRUE);
        vbox = gtk_vbox_new(FALSE, 5);

        b->firstentry = gtk_entry_new();
	b->middleentry = gtk_entry_new();
	b->lastentry = gtk_entry_new();
	b->maidenentry = gtk_entry_new();
	b->cityentry = gtk_entry_new();
	b->stateentry = gtk_entry_new();
	b->countryentry = gtk_entry_new();

        gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        label = gtk_label_new("First Name");
	gtk_widget_show(label);
	gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(topbox), b->firstentry, 1, 2, 0, 1);

        label = gtk_label_new("Middle Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 1, 2);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->middleentry, 1, 2, 1, 2);

        label = gtk_label_new("Last Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 2, 3);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->lastentry, 1, 2, 2, 3);

        label = gtk_label_new("Maiden Name");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 3, 4);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->maidenentry, 1, 2, 3, 4);

        label = gtk_label_new("City");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 4, 5);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->cityentry, 1, 2, 4, 5);

        label = gtk_label_new("State");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 5, 6);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->stateentry, 1, 2, 5, 6);

        label = gtk_label_new("Country");
        gtk_widget_show(label);
        gtk_table_attach_defaults(GTK_TABLE(topbox), label, 0, 1, 6, 7);
        gtk_table_attach_defaults(GTK_TABLE(topbox), b->countryentry, 1, 2, 6, 7);

	gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

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
        gtk_widget_show(topbox);
        gtk_widget_show(bbox);                      
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Find Buddy By Info");
        gtk_window_set_focus(GTK_WINDOW(b->window), b->firstentry);
        gtk_container_add(GTK_CONTAINER(b->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
        aol_icon(b->window->window);

        gtk_widget_show(b->window);
}        

void show_find_email()
{
        GtkWidget *cancel;
        GtkWidget *ok;
        GtkWidget *label;
        GtkWidget *bbox;
        GtkWidget *vbox;
        GtkWidget *topbox;

        struct findbyemail *b = g_new0(struct findbyemail, 1);
        b->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, b->window); 

        cancel = gtk_button_new_with_label("Cancel");
        ok = gtk_button_new_with_label("OK");

        bbox = gtk_hbox_new(TRUE, 10);
        topbox = gtk_hbox_new(FALSE, 5);
        vbox = gtk_vbox_new(FALSE, 5);

        b->emailentry = gtk_entry_new();

        gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        label = gtk_label_new("Email");
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(topbox), label, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(topbox), b->emailentry, FALSE, FALSE, 5);

        gtk_box_pack_start(GTK_BOX(vbox), topbox, TRUE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 5);

        gtk_signal_connect(GTK_OBJECT(b->window), "destroy",
                           GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(cancel), "clicked", 
                         GTK_SIGNAL_FUNC(destroy_dialog), b->window);
        gtk_signal_connect(GTK_OBJECT(ok), "clicked",
                           GTK_SIGNAL_FUNC(do_find_email), b);
        gtk_signal_connect(GTK_OBJECT(b->emailentry), "activate",
                           GTK_SIGNAL_FUNC(do_find_email), b);

        gtk_widget_show(ok);
        gtk_widget_show(cancel);
        gtk_widget_show(b->emailentry);
        gtk_widget_show(topbox);
        gtk_widget_show(bbox);
        gtk_widget_show(vbox);
        gtk_window_set_title(GTK_WINDOW(b->window), "Gaim - Find Buddy By Email");
        gtk_window_set_focus(GTK_WINDOW(b->window), b->emailentry);
        gtk_container_add(GTK_CONTAINER(b->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(b->window), 10);
        gtk_widget_realize(b->window);
        aol_icon(b->window->window);

        gtk_widget_show(b->window);
}

/*------------------------------------------------------*/
/* Link Dialog                                          */
/*------------------------------------------------------*/

void cancel_link(GtkWidget *widget, struct linkdlg *b)
{
	if (b->toggle) {
		set_state_lock(1);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(b->toggle), FALSE);
		set_state_lock(0);
	}	
	destroy_dialog(NULL, b->window);
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


void show_add_link(GtkWidget *entry, GtkWidget *link)
{
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *table;
	GtkWidget *label;

	if (!linkdialog) {
		struct linkdlg *b = g_new0(struct linkdlg, 1);
		linkdialog = gtk_window_new(GTK_WINDOW_DIALOG);
		dialogwindows = g_list_prepend(dialogwindows, linkdialog);

		b->cancel = gtk_button_new_with_label("Cancel");
		b->ok = gtk_button_new_with_label("Ok");
	
		vbox = gtk_vbox_new(FALSE, 10);
		bbox = gtk_hbox_new(TRUE, 10);

		gtk_widget_show(b->ok);
		gtk_widget_show(b->cancel);

		gtk_box_pack_start(GTK_BOX(bbox), b->ok, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(bbox), b->cancel, FALSE, FALSE, 10);
		gtk_widget_show(bbox);

		table = gtk_table_new(2, 2, FALSE);
		b->url = gtk_entry_new();
		b->text = gtk_entry_new();

		label = gtk_label_new("URL");
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
		gtk_table_attach_defaults(GTK_TABLE(table), b->url, 1, 2, 0, 1);
		gtk_widget_show(label);
	
		label = gtk_label_new("Description");
		gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
		gtk_table_attach_defaults(GTK_TABLE(table), b->text, 1, 2, 1, 2);
		gtk_widget_show(label);

		gtk_widget_show(b->url);
		gtk_widget_show(b->text);
		gtk_widget_show(table);

		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 10);
		gtk_widget_show(vbox);

		gtk_signal_connect(GTK_OBJECT(linkdialog), "destroy",
				   GTK_SIGNAL_FUNC(cancel_link), b);
		gtk_signal_connect(GTK_OBJECT(b->cancel), "clicked",
				   GTK_SIGNAL_FUNC(cancel_link), b);
		gtk_signal_connect(GTK_OBJECT(b->ok), "clicked",
				   GTK_SIGNAL_FUNC(do_add_link), b);

		gtk_container_add(GTK_CONTAINER(linkdialog  ), vbox);
		gtk_container_border_width(GTK_CONTAINER(linkdialog  ), 10);
		gtk_window_set_title(GTK_WINDOW(linkdialog  ), "GAIM - Add URL");
		gtk_window_set_focus(GTK_WINDOW(linkdialog  ), b->url);
		b->window = linkdialog;
		b->toggle = link;
                b->entry = entry;
                gtk_widget_realize(linkdialog);
		aol_icon(linkdialog->window);

	}

	gtk_widget_show(linkdialog);
	gdk_window_raise(linkdialog->window);
}


/*------------------------------------------------------*/
/* Color Selection Dialog                               */
/*------------------------------------------------------*/

void cancel_color(GtkWidget *widget, GtkWidget *color)
{
 	if (color)
        {
        	set_state_lock(1);
        	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(color), FALSE);
        	set_state_lock(0);
	}
	destroy_dialog(NULL, colordialog);
}



void do_color(GtkWidget *widget, GtkColorSelection *colorsel)
{
        gdouble color[3];
	GdkColor text_color;
	GtkWidget *entry;
	char *open_tag;

	open_tag = g_malloc(30);

        gtk_color_selection_get_color (colorsel, color);

        entry = gtk_object_get_user_data(GTK_OBJECT(colorsel));
	
        text_color.red = ((guint16)(color[0]*65535))>>8;
        text_color.green = ((guint16)(color[1]*65535))>>8;
		text_color.blue = ((guint16)(color[2]*65535))>>8;
	
	g_snprintf(open_tag, 23, "<FONT COLOR=\"#%02X%02X%02X\">", text_color.red, text_color.green, text_color.blue);

	surround(entry, open_tag, "</FONT>");
	sprintf(debug_buff,"#%02X%02X%02X\n", text_color.red, text_color.green, text_color.blue);
	debug_print(debug_buff);
        g_free(open_tag);
	cancel_color(NULL, NULL);
}


void show_color_dialog(GtkWidget *entry, GtkWidget *color)
{
        GtkWidget *colorsel;

        if (!colordialog) {
     

		colordialog = gtk_color_selection_dialog_new("Select Text Color");
                colorsel = GTK_COLOR_SELECTION_DIALOG(colordialog)->colorsel;

                gtk_object_set_user_data(GTK_OBJECT(colorsel), entry);
		
                gtk_signal_connect(GTK_OBJECT(colordialog), "delete_event", GTK_SIGNAL_FUNC(cancel_color), color);

                gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(colordialog)->ok_button), "clicked", GTK_SIGNAL_FUNC(do_color), colorsel);

                gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(colordialog)->cancel_button), "clicked", GTK_SIGNAL_FUNC(cancel_color), color);
                gtk_widget_realize(colordialog);
		aol_icon(colordialog->window);

        }

        gtk_widget_show(colordialog);
        gdk_window_raise(colordialog->window);
}

/*------------------------------------------------------------------------*/
/*  The dialog for import/export                                          */
/*------------------------------------------------------------------------*/

void do_export(GtkWidget *w, void *dummy)
{
        FILE *f;
        char *buf = g_malloc(BUF_LONG);
        char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(exportdialog));

        if ((f = fopen(file,"w"))) {
                toc_build_config(buf, BUF_LONG - 1);
                fprintf(f, "%s\n", buf);
                fclose(f);
                chmod(buf, S_IRUSR | S_IWUSR);
        } else {
                g_snprintf(buf, BUF_LONG / 2, "Error writing file %s", file);
                do_error_dialog(buf, "Error");
        }
        destroy_dialog(NULL, exportdialog);
        exportdialog = NULL;
        
        g_free(buf);
        
        
}

	
void show_export_dialog()
{
        char *buf = g_malloc(BUF_LEN);
        if (!exportdialog) {
                exportdialog = gtk_file_selection_new("Gaim - Export Buddy List");

                gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(exportdialog));

                g_snprintf(buf, BUF_LEN - 1, "%s/gaim.buddy", getenv("HOME"));
                
                gtk_file_selection_set_filename(GTK_FILE_SELECTION(exportdialog), buf);
                gtk_signal_connect(GTK_OBJECT(exportdialog), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_dialog), exportdialog);
                
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(exportdialog)->ok_button),
                                   "clicked", GTK_SIGNAL_FUNC(do_export), NULL);
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(exportdialog)->cancel_button),
                                   "clicked", GTK_SIGNAL_FUNC(destroy_dialog), exportdialog);
                

	}

	g_free(buf);

        gtk_widget_show(exportdialog);
        gdk_window_raise(exportdialog->window);

}

void do_import(GtkWidget *w, void *dummy)
{
        GList *grp, *grp2;
        char *buf = g_malloc(BUF_LONG);
        char *buf2;
        char *first = g_malloc(64);
        char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(importdialog));
        FILE *f;
        
        if (!(f = fopen(file,"r"))) {
                g_snprintf(buf, BUF_LONG / 2, "Error reading file %s", file);
                do_error_dialog(buf, "Error");
                destroy_dialog(NULL, importdialog);
                importdialog = NULL;
                g_free(buf);
		g_free(first);
                return;
        }
                
        fgets(first, 64, f);

        if (!strcasecmp(first, "Config {\n")) {
                destroy_dialog(NULL, importdialog);
                importdialog = NULL;
		g_free(buf);
		g_free(first);
                return;
        } else if (buf[0] == 'm') {
                buf2 = buf;
                buf = g_malloc(1025);
                g_snprintf(buf, 1024, "toc_set_config {%s}\n", buf2);
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
        
        parse_toc_buddy_list(buf);

        serv_save_config();
        
        build_edit_tree();
        build_permit_tree();
        
        destroy_dialog(NULL, importdialog);
        importdialog = NULL;
        
        g_free(buf);
        g_free(first);
	
}

void show_import_dialog()
{
        char *buf = g_malloc(BUF_LEN);
        if (!importdialog) {
                importdialog = gtk_file_selection_new("Gaim - Import Buddy List");

                gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(importdialog));

                g_snprintf(buf, BUF_LEN - 1, "%s/", getenv("HOME"));
                
                gtk_file_selection_set_filename(GTK_FILE_SELECTION(importdialog), buf);
                gtk_signal_connect(GTK_OBJECT(importdialog), "destroy",
                                   GTK_SIGNAL_FUNC(destroy_dialog), importdialog);
                
                gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(importdialog)->ok_button),
                                   "clicked", GTK_SIGNAL_FUNC(do_import), NULL);
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
	
	if (is_checked) do_away_message(NULL, b);

	/* stick it on the away list */
	if (strlen(b->name)) {
                away_messages = g_list_append(away_messages, b);
                save_prefs();
                do_away_menu();
                if (pd != NULL)
                        gtk_list_select_item(GTK_LIST(pd->away_list), g_list_index(away_messages, b));
        }
        
        destroy_dialog(NULL, ca->window);
}

void create_away_mess(GtkWidget *widget, void *dummy)
{
	GtkWidget *bbox;
	GtkWidget *titlebox;
	GtkWidget *tbox;
	GtkWidget *create;
	GtkWidget *sw;
	GtkWidget *label;

        struct create_away *ca = g_new0(struct create_away, 1);
        
	/* Set up window */
	ca->window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_container_border_width(GTK_CONTAINER(ca->window), 10);
	gtk_window_set_title(GTK_WINDOW(ca->window), "Gaim - New away message");
	gtk_signal_connect(GTK_OBJECT(ca->window),"delete_event",
		           GTK_SIGNAL_FUNC(destroy_dialog), ca->window);

	/* set up container boxes */
	bbox = gtk_vbox_new(FALSE, 0);
	titlebox = gtk_hbox_new(FALSE, 0);
	tbox = gtk_vbox_new(FALSE, 0);

	/* Make a label for away entry */
	label = gtk_label_new("Away title: ");
	gtk_box_pack_start(GTK_BOX(titlebox), label, TRUE, TRUE, 0);

	/* make away title entry */
	ca->entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(titlebox), ca->entry, TRUE, TRUE, 0);

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
	gtk_box_pack_start(GTK_BOX(bbox), sw, TRUE, TRUE, 10);   

	/* make create button */
	create = gtk_button_new_with_label ("Create new message");
	gtk_box_pack_start(GTK_BOX(bbox), create, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(create), "clicked", GTK_SIGNAL_FUNC(create_mess), ca);

	/* Checkbox for showing away msg */
	ca->checkbx = gtk_check_button_new_with_label("Make away now");

	/* pack boxes where they belong */
	gtk_box_pack_start(GTK_BOX(tbox), titlebox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(tbox), bbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(tbox), ca->checkbx, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ca->window), tbox);

	/* let the world see what we have done. */
	gtk_widget_show(label);
	gtk_widget_show(create);
	gtk_widget_show(ca->checkbx);
	gtk_widget_show(ca->entry);
	gtk_widget_show(titlebox);
	gtk_widget_show(tbox);
	gtk_widget_show(bbox);


        gtk_widget_realize(ca->window);
        aol_icon(ca->window->window);

	gtk_widget_show(ca->window);


}

#if 0

/*------------------------------------------------------------------------*/
/*  The dialog for file requests                                          */
/*------------------------------------------------------------------------*/


static void cancel_callback(GtkWidget *widget, struct file_transfer *ft)
{
	char *send = g_malloc(256);

	if (ft->accepted) {
		g_free(send);
		return;
	}
	
	g_snprintf(send, 255, "toc_rvous_cancel %s %s %s", ft->user, ft->cookie, FILETRANS_UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);
	destroy_dialog(NULL, ft->window);
	g_free(ft->user);
	if (ft->message)
		g_free(ft->message);
	g_free(ft->filename);
	g_free(ft->cookie);
	g_free(ft->ip);
        g_free(ft);
}


static void warn_callback(GtkWidget *widget, struct file_transfer *ft)
{
        show_warn_dialog(ft->user);
}

static void info_callback(GtkWidget *widget, struct file_transfer *ft)
{
        serv_get_info(ft->user);
}

static char *put_16_int(gint i) {
        static char tmp[2];
        g_snprintf(tmp, 2, "%c%c", i >> 8,  i & 0xff);
        return tmp;
}

static char *put_32_int(gint i) {
        static char tmp[4];
        g_snprintf(tmp, 4, "%c%c%c%c", (i >> 24) & 0xff, (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
        return tmp;
}


static int get_16_int(char *text)
{
        int tmp = 0;
	tmp = ((*text << 8) & 0xff);
	text++;
	tmp |= (*text & 0xff);
        text++;
        return tmp;
}

static int get_32_int(char *text)
{
        int tmp = 0;
	tmp = ((*text << 24) & 0xff);
	text++;
	tmp |= ((*text << 16) & 0xff);
	text++;
	tmp |= ((*text << 8) & 0xff);
	text++;
	tmp |= (*text & 0xff);
        text++;
        return tmp;
}
	
static void do_accept(GtkWidget *w, struct file_transfer *ft)
{
	char *send = g_malloc(256);
	char *file = gtk_file_selection_get_filename(GTK_FILE_SELECTION(ft->window));
	char *buf;
	char *header;
	int hdrlen;
	int read_rv;
	char bmagic[5];
	struct sockaddr_in sin;
	int rcv;
        gint hdrtype, encrypt, compress, totfiles, filesleft;
        gint totparts, partsleft, totsize, size, modtime, checksum;
        gint rfrcsum, rfsize, cretime, rfcsum, nrecvd, recvcsum;
        char *bcookie, *idstring;
        char flags, lnameoffset, lsizeoffset, dummy;
        char *macfileinfo;
        gint nencode, nlanguage;
        char *name;
        char *c;

        
	if (!(ft->f = fopen(file,"w"))) {
		buf = g_malloc(BUF_LONG);
                g_snprintf(buf, BUF_LONG / 2, "Error writing file %s", file);
		do_error_dialog(buf, "Error");
		g_free(buf);
		ft->accepted = 0;
		accept_callback(NULL, ft);
		return;
	}

	ft->accepted = 1;
	
	destroy_dialog(NULL, ft->window);
	g_snprintf(send, 255, "toc_rvous_accept %s %s %s", ft->user, ft->cookie, FILETRANS_UID);
	sflap_send(send, strlen(send), TYPE_DATA);
	g_free(send);

	

        sin.sin_addr.s_addr = inet_addr(ft->ip);
        sin.sin_family = AF_INET;
	sin.sin_port = htons(ft->port);
	
	ft->fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (ft->fd <= -1 || connect(ft->fd, (struct sockaddr_in *)&sin, sizeof(sin))) {
	        g_free(buf);
		return;
		/*cancel */
	}

	rcv = 0;
	header = g_malloc(6);
	while (rcv != 6) {
		read_rv = read(ft->fd, header + rcv, 6 - rcv);
		if(read_rv < 0) {
			g_free(header);
			g_free(buf);
			return;
		}
		rcv += read_rv;
		while(gtk_events_pending())
			gtk_main_iteration();
	}

	strncpy(bmagic, header, 4);
        bmagic[4] = 0;

	hdrlen = ((header[4] << 8) & 0xff) | (header[5] & 0xff);

	g_free(header);
	header = g_malloc(hdrlen+1);

	rcv = 0;

	while (rcv != hdrlen) {
		read_rv = read(ft->fd, header + rcv, hdrlen - rcv);
		if(read_rv < 0) {
			g_free(header);
			g_free(buf);
			return;
		}
		rcv += read_rv;
		while(gtk_events_pending())
			gtk_main_iteration();
	}

	header[hdrlen] = 0;

        c = header;


        hdrtype = get_16_int(c);
        bcookie = g_malloc(9);
        strncpy(bcookie, c, 8);
        c+=8;
        bcookie[8] = 0;

        encrypt = get_16_int(c); c+=2;
        compress = get_16_int(c); c+=2;
        totfiles = get_16_int(c); c+=2;
        filesleft = get_16_int(c); c+=2;
        totparts = get_16_int(c); c+=2;
        partsleft = get_16_int(c); c+=2;
        totsize = get_32_int(c); c+=4;
        size = get_32_int(c); c+=4;
        modtime = get_32_int(c); c+=4;
        checksum = get_32_int(c); c+=4;
        rfrcsum = get_32_int(c); c+=4;
        rfsize = get_32_int(c); c+=4;
        cretime = get_32_int(c); c+=4;
        rfcsum = get_32_int(c); c+=4;
        nrecvd = get_32_int(c); c+=4;
        recvcsum = get_32_int(c); c+=4;
        idstring = g_malloc(33);
        strncpy(idstring, c, 32);
        c+=32;
        idstring[32] = 0;
        flags = *c; c++;
        lnameoffset = *c; c++;
        lsizeoffset = *c; c++;
        dummy = *c; c++;
        
        macfileinfo = g_malloc(70);
        strncpy(macfileinfo, c, 69);
        c+=69;
        macfileinfo[69] = 0;
        nencode = get_16_int(c); c+=2;
        nlanguage = get_16_int(c); c+=2;
        
        name = g_strdup(c);


        totparts = 1;
        partsleft = 1;
        rfsize = 0;


        printf("Header type: %d\n", hdrtype);
        printf("Encryption: %d\n", encrypt);
        printf("Compress: %d\n", compress);
        

        
	
}


static void accept_callback(GtkWidget *widget, struct file_transfer *ft)
{
	char *buf = g_malloc(BUF_LEN);
	char *fname = g_malloc(BUF_LEN);
	char *c;

	c = ft->filename + strlen(ft->filename);

	while (c != ft->filename) {
		if (*c == '/' || *c == '\\') {
			strcpy(fname, c+1);
			break;
		}
		c--;
	}

	if (c == ft->filename)
                strcpy(fname, ft->filename);

	
	destroy_dialog(NULL, ft->window);
	
	ft->window = gtk_file_selection_new("Gaim - Save As...");

	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(ft->window));

	g_snprintf(buf, BUF_LEN - 1, "%s/%s", getenv("HOME"), fname);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(ft->window), buf);
	gtk_signal_connect(GTK_OBJECT(ft->window), "destroy",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);
                
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(ft->window)->ok_button),
			   "clicked", GTK_SIGNAL_FUNC(do_accept), ft);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(ft->window)->cancel_button),
			   "clicked", GTK_SIGNAL_FUNC(cancel_callback), ft);

        dialogwindows = g_list_prepend(dialogwindows, ft->window);

	gtk_widget_show(ft->window);
	
	g_free(buf);
	g_free(fname);

}




void accept_file_dialog(struct file_transfer *ft)
{
        GtkWidget *accept, *info, *warn, *cancel;
        GtkWidget *text = NULL, *sw;
        GtkWidget *label;
        GtkWidget *vbox, *bbox;
        char buf[1024];

        
        ft->window = gtk_window_new(GTK_WINDOW_DIALOG);
        dialogwindows = g_list_prepend(dialogwindows, ft->window);

        accept = gtk_button_new_with_label("Accept");
        info = gtk_button_new_with_label("Info");
        warn = gtk_button_new_with_label("Warn");
        cancel = gtk_button_new_with_label("Cancel");

        bbox = gtk_hbox_new(TRUE, 10);
        vbox = gtk_vbox_new(FALSE, 5);

        gtk_widget_show(accept);
        gtk_widget_show(info);
        gtk_widget_show(warn);
        gtk_widget_show(cancel);

        gtk_box_pack_start(GTK_BOX(bbox), accept, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), info, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), warn, TRUE, TRUE, 10);
        gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 10);

        g_snprintf(buf, sizeof(buf), "%s requests you to accept the file: %s (%d bytes)",
                   ft->user, ft->filename, ft->size);
        label = gtk_label_new(buf);
        gtk_widget_show(label);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
        
        if (ft->message) {
                text = gaim_new_layout();
                sw = gtk_scrolled_window_new (NULL, NULL);
                gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                                GTK_POLICY_NEVER,
                                                GTK_POLICY_AUTOMATIC);
                gtk_widget_show(sw);
                gtk_container_add(GTK_CONTAINER(sw), text);
                gtk_widget_show(text);

                gtk_layout_set_size(GTK_LAYOUT(text), 250, 100);
                GTK_LAYOUT (text)->vadjustment->step_increment = 10.0;
                gtk_widget_set_usize(sw, 250, 100);

                gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 10);
        }
        gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 5);

        gtk_window_set_title(GTK_WINDOW(ft->window), "Gaim - Accept File?");
        gtk_window_set_focus(GTK_WINDOW(ft->window), accept);
        gtk_container_add(GTK_CONTAINER(ft->window), vbox);
        gtk_container_border_width(GTK_CONTAINER(ft->window), 10);
        gtk_widget_show(vbox);
        gtk_widget_show(bbox);
        gtk_widget_realize(ft->window);
        aol_icon(ft->window->window);

        gtk_widget_show(ft->window);


	gtk_signal_connect(GTK_OBJECT(accept), "clicked",
			   GTK_SIGNAL_FUNC(accept_callback), ft);
	gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
			   GTK_SIGNAL_FUNC(cancel_callback), ft);
	gtk_signal_connect(GTK_OBJECT(warn), "clicked",
			   GTK_SIGNAL_FUNC(warn_callback), ft);
	gtk_signal_connect(GTK_OBJECT(info), "clicked",
			   GTK_SIGNAL_FUNC(info_callback), ft);


	if (ft->message) {
		while(gtk_events_pending())
			gtk_main_iteration();
		html_print(text, ft->message);
	}



}
#endif

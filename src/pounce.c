/*
 * gaim
 *
 * Copyright (C) 1998-2003, Mark Spencer <markster@marko.net>
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "gaim.h"
#include "prpl.h"
#include "pounce.h"

GtkWidget *bpmenu = NULL;

void rem_bp(GtkWidget *w, struct buddy_pounce *b)
{
	buddy_pounces = g_list_remove(buddy_pounces, b);
	do_bp_menu();
	save_prefs();
}

void do_pounce(struct gaim_connection *gc, char *name, int when)
{
	char *who;

	struct buddy_pounce *b;
	struct gaim_conversation *c;
	struct gaim_account *account;

	GList *bp = buddy_pounces;

	who = g_strdup(normalize (name));

	while (bp) {
		b = (struct buddy_pounce *)bp->data;
		bp = bp->next;	/* increment the list here because rem_bp can make our handle bad */

		if (!(b->options & when))
			continue;

		account = gaim_account_find(b->pouncer, b->protocol);	/* find our user */
		if (account == NULL)
			continue;

		/* check and see if we're signed on as the pouncer */
		if (account->gc != gc)
			continue;

		if (!g_strcasecmp(who, normalize (b->name))) {	/* find someone to pounce */
			if (b->options & OPT_POUNCE_POPUP) {
				c = gaim_find_conversation(name);

				if (c == NULL)
					c = gaim_conversation_new(GAIM_CONV_IM, account, name);
				else
					gaim_conversation_set_account(c, account);
			}
			if (b->options & OPT_POUNCE_NOTIFY) {
				char tmp[1024];

				/* I know the line below is really ugly. I only did it this way
				 * because I thought it'd be funny :-) */

				g_snprintf(tmp, sizeof(tmp), 
					   (when & OPT_POUNCE_TYPING) ? _("%s has started typing to you") :
					   (when & OPT_POUNCE_SIGNON) ? _("%s has signed on") : 
					   (when & OPT_POUNCE_UNIDLE) ? _("%s has returned from being idle") : 
					   _("%s has returned from being away"), name);
				
				do_error_dialog(tmp, NULL, GAIM_INFO);
			}
			if (b->options & OPT_POUNCE_SEND_IM) {
				if (strlen(b->message) > 0) {
					c = gaim_find_conversation(name);

					if (c == NULL)
						c = gaim_conversation_new(GAIM_CONV_IM, account, name);
					else
						gaim_conversation_set_account(c, account);

					gaim_conversation_write(c, NULL, b->message, -1,
											WFLAG_SEND, time(NULL));

					serv_send_im(account->gc, name, b->message, -1, 0);
				}
			}
			if (b->options & OPT_POUNCE_COMMAND) {
#ifndef _WIN32
				int pid = fork();

				if (pid == 0) {
					char *args[4];
					args[0] = "sh";
					args[1] = "-c";
					args[2] = b->command;
					args[3] = NULL;
					execvp(args[0], args);
					_exit(0);
				}
#else
				STARTUPINFO si;
				PROCESS_INFORMATION pi;

				ZeroMemory( &si, sizeof(si) );
				si.cb = sizeof(si);
				ZeroMemory( &pi, sizeof(pi) );

				CreateProcess( NULL, b->command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
				CloseHandle( pi.hProcess );
				CloseHandle( pi.hThread );
#endif /*_WIN32*/
			}
			/*	if (b->options & OPT_POUNCE_SOUND) {
				if (strlen(b->sound))
					play_file(b->sound);
				else
					play_sound(SND_POUNCE_DEFAULT);
					}*/

			if (!(b->options & OPT_POUNCE_SAVE))
				rem_bp(NULL, b);

		}
	}
	g_free(who);
}

static void new_bp_callback(GtkWidget *w, struct buddy *b)
{
	if (b)
		show_new_bp(b->name, b->account->gc, b->idle, b->uc & UC_UNAVAILABLE, NULL);
	else
		show_new_bp(NULL, NULL, 0, 0, NULL);
}

static void edit_bp_callback(GtkWidget *w, struct buddy_pounce *b)
{
  show_new_bp(NULL, NULL, 0, 0, b);
}

static GtkTooltips *bp_tooltip = NULL;
void do_bp_menu()
{
	GtkWidget *menuitem, *mess, *messmenu;
	static GtkWidget *remmenu;
	GtkWidget *remitem;
	GtkWidget *sep;
	GList *l;
	struct buddy_pounce *b;
	GList *bp = buddy_pounces;

	/* Tooltip for editing bp's */
	if(!bp_tooltip)
		bp_tooltip = gtk_tooltips_new();

	l = gtk_container_get_children(GTK_CONTAINER(bpmenu));

	while (l) {
		gtk_widget_destroy(GTK_WIDGET(l->data));
		l = l->next;
	}

	remmenu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("New Buddy Pounce"));
	gtk_menu_shell_append(GTK_MENU_SHELL(bpmenu), menuitem);
	gtk_widget_show(menuitem);
	g_signal_connect(GTK_OBJECT(menuitem), "activate", G_CALLBACK(new_bp_callback), NULL);


	while (bp) {

		b = (struct buddy_pounce *)bp->data;
		remitem = gtk_menu_item_new_with_label(b->name);
		gtk_menu_shell_append(GTK_MENU_SHELL(remmenu), remitem);
		gtk_widget_show(remitem);
		g_signal_connect(GTK_OBJECT(remitem), "activate", G_CALLBACK(rem_bp), b);

		bp = bp->next;

	}

	menuitem = gtk_menu_item_new_with_label(_("Remove Buddy Pounce"));
	gtk_menu_shell_append(GTK_MENU_SHELL(bpmenu), menuitem);
	gtk_widget_show(menuitem);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), remmenu);
	gtk_widget_show(remmenu);

	sep = gtk_hseparator_new();
	menuitem = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(bpmenu), menuitem);
	gtk_container_add(GTK_CONTAINER(menuitem), sep);
	gtk_widget_set_sensitive(menuitem, FALSE);
	gtk_widget_show(menuitem);
	gtk_widget_show(sep);

	bp = buddy_pounces;

	while (bp) {

		b = (struct buddy_pounce *)bp->data;

		menuitem = gtk_menu_item_new_with_label(b->name);
		gtk_menu_shell_append(GTK_MENU_SHELL(bpmenu), menuitem);
		messmenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), messmenu);
		gtk_widget_show(menuitem);

		if (strlen(b->message))
			mess = gtk_menu_item_new_with_label(b->message);
		else
			mess = gtk_menu_item_new_with_label(_("[no message]"));
		gtk_menu_shell_append(GTK_MENU_SHELL(messmenu), mess);
		gtk_tooltips_set_tip(bp_tooltip, GTK_WIDGET(mess), _("[Click to edit]"), NULL);
		gtk_widget_show(mess);
		g_signal_connect(GTK_OBJECT(mess), "activate", G_CALLBACK(edit_bp_callback), b);
		bp = bp->next;

	}

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
	g_snprintf(bp->pouncer, 80, "%s", b->account->username);

	bp->protocol = b->account->protocol;

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

	gtk_widget_destroy(b->window);

	save_prefs();
	g_free(b);
}

static void pounce_choose(GtkWidget *opt, struct addbp *b)
{
	struct gaim_account *account = g_object_get_data(G_OBJECT(opt), "gaim_account");
	b->account = account;
}

static GtkWidget *pounce_user_menu(struct addbp *b, struct gaim_connection *gc)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GSList *u = gaim_accounts;
	struct gaim_account *account;
	struct prpl *p;
	int count = 0;
	int place = 0;
	char buf[2048];


	optmenu = gtk_option_menu_new();

	menu = gtk_menu_new();

	while (u) {
		account = (struct gaim_account *)u->data;
		p = (struct prpl *)find_prpl(account->protocol);
		g_snprintf(buf, sizeof buf, "%s (%s)", account->username, (p && p->name)?p->name:_("Unknown"));
		opt = gtk_menu_item_new_with_label(buf);
		g_object_set_data(G_OBJECT(opt), "gaim_account", account);
		g_signal_connect(GTK_OBJECT(opt), "activate", G_CALLBACK(pounce_choose), b);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
		gtk_widget_show(opt);

		if (b->account == account) {
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
		b->account = gaim_account_find(edit_bp->pouncer, edit_bp->protocol);
	} else {
		b->account = gc ? gc->account : gaim_accounts->data;
		b->buddy_pounce = NULL;
	}

	GAIM_DIALOG(b->window);
	gtk_window_set_resizable(GTK_WINDOW(b->window), TRUE);
	gtk_window_set_role(GTK_WINDOW(b->window), "new_bp");
	gtk_window_set_title(GTK_WINDOW(b->window), _("Gaim - New Buddy Pounce"));
	g_signal_connect(GTK_OBJECT(b->window), "destroy", G_CALLBACK(gtk_widget_destroy), b->window);
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
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_signon), 
		                           (edit_bp->options & OPT_POUNCE_SIGNON) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_signon), TRUE);
	gtk_table_attach(GTK_TABLE(table), b->p_signon, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->p_signon);

	b->p_unaway = gtk_check_button_new_with_label(_("Pounce on return from away"));
	if (away)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_unaway), TRUE);
	else if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_unaway), 
					   (edit_bp->options & OPT_POUNCE_UNAWAY) ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), b->p_unaway, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(b->p_unaway);

	b->p_unidle = gtk_check_button_new_with_label(_("Pounce on return from idle"));
	if (idle)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_unidle), TRUE);
	else if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_unidle), 
				           (edit_bp->options & OPT_POUNCE_UNIDLE) ? TRUE : FALSE);
	gtk_table_attach(GTK_TABLE(table), b->p_unidle, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->p_unidle);
	
	b->p_typing = gtk_check_button_new_with_label(_("Pounce when buddy is typing to you"));
	if (edit_bp)		
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->p_typing),
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
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->openwindow), 
			                   (edit_bp->options & OPT_POUNCE_POPUP) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->openwindow), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->openwindow, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->openwindow);
	
	b->popupnotify = gtk_check_button_new_with_label(_("Popup Notification"));
	if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->popupnotify), 
					   (edit_bp->options & OPT_POUNCE_NOTIFY) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->popupnotify), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->popupnotify, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->popupnotify);

	b->sendim = gtk_check_button_new_with_label(_("Send Message"));
	if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->sendim), 
					   (edit_bp->options & OPT_POUNCE_SEND_IM) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->sendim), TRUE);
	gtk_table_attach(GTK_TABLE(table), b->sendim, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->sendim);

	b->messentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->messentry, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	g_signal_connect(GTK_OBJECT(b->messentry), "activate", G_CALLBACK(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->messentry), 
					(edit_bp->options & OPT_POUNCE_SEND_IM) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->messentry), edit_bp->message);
	}
	gtk_widget_show(b->messentry);

	g_signal_connect(GTK_OBJECT(b->sendim), "clicked",
					 G_CALLBACK(gaim_gtk_toggle_sensitive), b->messentry);

	b->command = gtk_check_button_new_with_label(_("Execute command on pounce"));
	gtk_table_attach(GTK_TABLE(table), b->command, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->command),
					   (edit_bp->options & OPT_POUNCE_COMMAND) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->command), FALSE);
	gtk_widget_show(b->command);

	b->commentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->commentry, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	g_signal_connect(GTK_OBJECT(b->commentry), "activate", G_CALLBACK(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->commentry), 
					(edit_bp->options & OPT_POUNCE_COMMAND) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->commentry), edit_bp->command);
	}
	else
		gtk_widget_set_sensitive(GTK_WIDGET(b->commentry), FALSE);
	gtk_widget_show(b->commentry);
	g_signal_connect(GTK_OBJECT(b->command), "clicked",
					 G_CALLBACK(gaim_gtk_toggle_sensitive), b->commentry);
	
	b->sound = gtk_check_button_new_with_label(_("Play sound on pounce"));
	if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->sound), 
					   (edit_bp->options & OPT_POUNCE_SOUND) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->sound), FALSE);
	gtk_table_attach(GTK_TABLE(table), b->sound, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_widget_show(b->sound);

	b->soundentry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), b->soundentry, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	g_signal_connect(GTK_OBJECT(b->soundentry), "activate", G_CALLBACK(do_new_bp), b);
	if(edit_bp) {
		gtk_widget_set_sensitive(GTK_WIDGET(b->soundentry), 
					(edit_bp->options & OPT_POUNCE_SOUND) ? TRUE : FALSE);
		gtk_entry_set_text(GTK_ENTRY(b->soundentry), edit_bp->sound);
	} else 
		gtk_widget_set_sensitive(GTK_WIDGET(b->soundentry), FALSE);
	gtk_widget_show(b->soundentry);
	g_signal_connect(GTK_OBJECT(b->sound), "clicked",
					 G_CALLBACK(gaim_gtk_toggle_sensitive), b->soundentry);
	/* </pounce type="action"> */

	b->save = gtk_check_button_new_with_label(_("Save this pounce after activation"));
	gtk_container_set_border_width(GTK_CONTAINER(b->save), 7);
	if(edit_bp)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->save),
					   (edit_bp->options & OPT_POUNCE_SAVE) ? TRUE : FALSE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(b->save), FALSE);
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
	g_signal_connect(GTK_OBJECT(button), "clicked", G_CALLBACK(do_new_bp), b);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gaim_pixbuf_button_from_stock(_("C_ancel"), "gtk-cancel", GAIM_BUTTON_HORIZONTAL);
	gtk_size_group_add_widget(sg, button);
	g_signal_connect_swapped(GTK_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_destroy), b->window);
	gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);


	gtk_widget_show(b->window);
}


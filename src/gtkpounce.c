/**
 * @file gtkpounce.h GTK+ buddy pounce API
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include <unistd.h>
#include <string.h>
#include "gaim.h"
#include "gtkpounce.h"
#include "gtkblist.h"
#include "prpl.h"
#include "sound.h"
#include "notify.h"

struct gaim_gtkpounce_dialog
{
	/* Pounce data */
	struct gaim_pounce  *pounce;
	struct gaim_account *account;

	/* The window */
	GtkWidget *window;

	/* Pounce Who */
	GtkWidget *account_menu;
	GtkWidget *buddy_entry;

	/* Pounce When */
	GtkWidget *signon;
	GtkWidget *signoff;
	GtkWidget *away;
	GtkWidget *away_return;
	GtkWidget *idle;
	GtkWidget *idle_return;
	GtkWidget *typing;
	GtkWidget *stop_typing;

	/* Pounce Action */
	GtkWidget *open_win;
	GtkWidget *popup;
	GtkWidget *send_msg;
	GtkWidget *send_msg_entry;
	GtkWidget *exec_cmd;
	GtkWidget *exec_cmd_entry;
	GtkWidget *exec_cmd_browse;
	GtkWidget *play_sound;
	GtkWidget *play_sound_entry;
	GtkWidget *play_sound_browse;
	GtkWidget *play_sound_test;
	
	GtkWidget *save_pounce;
};

/**************************************************************************
 * Callbacks
 **************************************************************************/
static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e,
			  struct gaim_gtkpounce_dialog *dialog)
{
	gtk_widget_destroy(dialog->window);
	g_free(dialog);

	return TRUE;
}

static void
delete_cb(GtkWidget *w, struct gaim_gtkpounce_dialog *dialog)
{
	gaim_pounce_destroy(dialog->pounce);

	delete_win_cb(NULL, NULL, dialog);
}

static void
cancel_cb(GtkWidget *w, struct gaim_gtkpounce_dialog *dialog)
{
	delete_win_cb(NULL, NULL, dialog);
}

static void
pounce_update_entryfields(GtkWidget *w, gpointer data)
{
	const char *selected_filename;
	GHashTable *args;
		
	args = (GHashTable *) data;
	
	selected_filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(g_hash_table_lookup(args, "pounce_file_selector")));
	if (selected_filename != NULL)
		gtk_entry_set_text(GTK_ENTRY(g_hash_table_lookup(args, "entry")),selected_filename);

	g_free(args);
}

static void
pounce_file_selector(GtkWidget *w, gpointer data)
{
	GtkWidget *pounce_file_selector;
	GtkWidget *entry;
	GHashTable *args;
	
	entry = (GtkWidget *) data;	

	pounce_file_selector = gtk_file_selection_new(_("Select a file"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(pounce_file_selector), gtk_entry_get_text(GTK_ENTRY(entry)));
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(pounce_file_selector));
	gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(pounce_file_selector), FALSE);

	args = g_hash_table_new(g_str_hash,g_str_equal);
	g_hash_table_insert(args, "pounce_file_selector", (gpointer) pounce_file_selector);
	g_hash_table_insert(args, "entry", (gpointer) entry);
	
	g_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(pounce_file_selector)->ok_button),
					 "clicked", G_CALLBACK(pounce_update_entryfields), (gpointer) args);

	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(pounce_file_selector)->ok_button),
							 "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer) pounce_file_selector);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(pounce_file_selector)->cancel_button),
							 "clicked", G_CALLBACK(gtk_widget_destroy), (gpointer) pounce_file_selector);

	gtk_widget_show(pounce_file_selector);
}

static void
pounce_test_sound(GtkWidget *w, gpointer data)
{
	const char *filename;
	GtkWidget *entry;

	entry = (GtkWidget *) data;
	
	filename = gtk_entry_get_text(GTK_ENTRY(entry));
	if ((filename != NULL) && (strlen(filename) > 0))
		gaim_sound_play_file((char *) filename);
	else
		gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT);
}

static void
save_pounce_cb(GtkWidget *w, struct gaim_gtkpounce_dialog *dialog)
{
	const char *name;
	const char *message, *command, *sound;
	struct gaim_buddy_list *blist;
	struct gaim_gtk_buddy_list *gtkblist;
	GaimPounceEvent events = GAIM_POUNCE_NONE;
	GaimGtkPounceAction actions = GAIM_GTKPOUNCE_NONE;
	gboolean save;

	name = gtk_entry_get_text(GTK_ENTRY(dialog->buddy_entry));

	if (*name == '\0') {
		gaim_notify_error(NULL, NULL,
						  _("Please enter a buddy to pounce."), NULL);
		return;
	}

	/* Events */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->signon)))
		events |= GAIM_POUNCE_SIGNON;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->signoff)))
		events |= GAIM_POUNCE_SIGNOFF;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->away)))
		events |= GAIM_POUNCE_AWAY;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->away_return)))
		events |= GAIM_POUNCE_AWAY_RETURN;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->idle)))
		events |= GAIM_POUNCE_IDLE;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->idle_return)))
		events |= GAIM_POUNCE_IDLE_RETURN;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->typing)))
		events |= GAIM_POUNCE_TYPING;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->stop_typing)))
		events |= GAIM_POUNCE_TYPING_STOPPED;


	/* Actions */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->open_win)))
		actions |= GAIM_GTKPOUNCE_OPEN_WIN;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->popup)))
		actions |= GAIM_GTKPOUNCE_POPUP;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->send_msg)))
		actions |= GAIM_GTKPOUNCE_SEND_MSG;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd)))
		actions |= GAIM_GTKPOUNCE_EXEC_CMD;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->play_sound)))
		actions |= GAIM_GTKPOUNCE_PLAY_SOUND;

	save = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->save_pounce));

	/* Data fields */
	message = gtk_entry_get_text(GTK_ENTRY(dialog->send_msg_entry));
	command = gtk_entry_get_text(GTK_ENTRY(dialog->exec_cmd_entry));
	sound   = gtk_entry_get_text(GTK_ENTRY(dialog->play_sound_entry));

	if (*message == '\0') message = NULL;
	if (*command == '\0') command = NULL;
	if (*sound   == '\0') sound   = NULL;

	if (dialog->pounce == NULL)
	{
		gaim_gtkpounce_new(dialog->account, name, events, actions,
						   message, command, sound, save);
	}
	else
	{
		struct gaim_gtkpounce_data *pounce_data;

		gaim_pounce_set_events(dialog->pounce, events);
		gaim_pounce_set_pouncer(dialog->pounce, dialog->account);
		gaim_pounce_set_pouncee(dialog->pounce, name);

		pounce_data = GAIM_GTKPOUNCE(dialog->pounce);

		if (pounce_data->message != NULL) g_free(pounce_data->message);
		if (pounce_data->command != NULL) g_free(pounce_data->command);
		if (pounce_data->sound   != NULL) g_free(pounce_data->sound);

		pounce_data->message = (message == NULL ? NULL : g_strdup(message));
		pounce_data->command = (command == NULL ? NULL : g_strdup(command));
		pounce_data->sound   = (sound   == NULL ? NULL : g_strdup(sound));

		pounce_data->actions = actions;
		pounce_data->save    = save;
	}

	delete_win_cb(NULL, NULL, dialog);
	/* Rebuild the pounce menu */
	blist = gaim_get_blist();

	if (GAIM_IS_GTK_BLIST(blist))
	{
		gtkblist = GAIM_GTK_BLIST(blist);

		gaim_gtkpounce_menu_build(gtkblist->bpmenu);
	}

	save_prefs();
}

static void
pounce_choose_cb(GtkWidget *item, struct gaim_gtkpounce_dialog *dialog)
{
	dialog->account = g_object_get_data(G_OBJECT(item), "user_data");
}

static GtkWidget *
pounce_user_menu(struct gaim_gtkpounce_dialog *dialog)
{
	struct gaim_account *account;
	GaimPlugin *prpl;
	GtkWidget *opt_menu;
	GtkWidget *menu;
	GtkWidget *item;
	GSList *l;
	char buf[2048];
	int count, place = 0;

	opt_menu = gtk_option_menu_new();
	menu = gtk_menu_new();

	for (l = gaim_accounts, count = 0; l != NULL; l = l->next, count++) {
		account = (struct gaim_account *)l->data;

		prpl = gaim_find_prpl(account->protocol);

		g_snprintf(buf, sizeof(buf), "%s (%s)", account->username,
				   (prpl && prpl->info->name)
				   ? prpl->info->name : _("Unknown"));

		item = gtk_menu_item_new_with_label(buf);
		g_object_set_data(G_OBJECT(item), "user_data", account);

		g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(pounce_choose_cb), dialog);

		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		if (dialog->account == account) {
			gtk_menu_item_activate(GTK_MENU_ITEM(item));
			place = count;
		}
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt_menu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(opt_menu), place);

	return opt_menu;
}

static void
pounce_cb(struct gaim_pounce *pounce, GaimPounceEvent events, void *data)
{
	struct gaim_conversation *conv;
	struct gaim_account *account;
	struct gaim_gtkpounce_data *pounce_data;
	const char *pouncee;

	pounce_data = (struct gaim_gtkpounce_data *)data;
	pouncee     = gaim_pounce_get_pouncee(pounce);
	account     = gaim_pounce_get_pouncer(pounce);

	if (pounce_data->actions & GAIM_GTKPOUNCE_OPEN_WIN) {
		conv = gaim_find_conversation(pouncee);

		if (conv == NULL)
			conv = gaim_conversation_new(GAIM_CONV_IM, account, pouncee);
	}

	if (pounce_data->actions & GAIM_GTKPOUNCE_POPUP) {
		char tmp[1024];

		g_snprintf(tmp, sizeof(tmp),
				   (events & GAIM_POUNCE_TYPING) ? _("%s has started typing to you") :
				   (events & GAIM_POUNCE_SIGNON) ? _("%s has signed on") :
				   (events & GAIM_POUNCE_IDLE_RETURN) ? _("%s has returned from being idle") :
				   (events & GAIM_POUNCE_AWAY_RETURN) ? _("%s has returned from being away") :
				   (events & GAIM_POUNCE_TYPING_STOPPED) ? _("%s has stopped typing to you") :
				   (events & GAIM_POUNCE_SIGNOFF) ? _("%s has signed off") :
				   (events & GAIM_POUNCE_IDLE) ? _("%s has become idle") :
				   (events & GAIM_POUNCE_AWAY) ? _("%s has gone away.") :
				   _("Unknown pounce event. Please report this!"),
				   pouncee);

		gaim_notify_info(NULL, NULL, tmp, NULL);
	}

	if (pounce_data->actions & GAIM_GTKPOUNCE_SEND_MSG &&
		pounce_data->message != NULL) {

		conv = gaim_find_conversation(pouncee);

		if (conv == NULL)
			conv = gaim_conversation_new(GAIM_CONV_IM, account, pouncee);

		gaim_conversation_write(conv, NULL, pounce_data->message, -1,
								WFLAG_SEND, time(NULL));

		serv_send_im(account->gc, (char *)pouncee, pounce_data->message, -1, 0);
	}

	if (pounce_data->actions & GAIM_GTKPOUNCE_EXEC_CMD &&
		pounce_data->command != NULL) {
#ifndef _WIN32
		int pid = fork();

		if (pid == 0) {
			char *args[4];

			args[0] = "sh";
			args[1] = "-c";
			args[2] = pounce_data->command;
			args[3] = NULL;

			execvp(args[0], args);

			_exit(0);
		}
#endif /* _WIN32 */
	}

	if (pounce_data->actions & GAIM_GTKPOUNCE_PLAY_SOUND) {
		if (pounce_data->sound != NULL)
			gaim_sound_play_file(pounce_data->sound);
		else
			gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT);
	}

	if (!pounce_data->save)
		gaim_pounce_destroy(pounce);
}

static void
free_pounce(void *data)
{
	struct gaim_gtkpounce_data *pounce_data;
	struct gaim_buddy_list *blist;
	struct gaim_gtk_buddy_list *gtkblist;

	pounce_data = (struct gaim_gtkpounce_data *)data;

	if (pounce_data->message != NULL) g_free(pounce_data->message);
	if (pounce_data->command != NULL) g_free(pounce_data->command);
	if (pounce_data->sound   != NULL) g_free(pounce_data->sound);

	g_free(data);

	/* Rebuild the pounce menu */
	blist = gaim_get_blist();

	if (GAIM_IS_GTK_BLIST(blist))
	{
		gtkblist = GAIM_GTK_BLIST(blist);

		gaim_gtkpounce_menu_build(gtkblist->bpmenu);
	}

	save_prefs();
}

struct gaim_pounce *
gaim_gtkpounce_new(struct gaim_account *pouncer, const char *pouncee,
				   GaimPounceEvent events, GaimGtkPounceAction actions,
				   const char *message, const char *command,
				   const char *sound, gboolean save)
{
	struct gaim_gtkpounce_data *data;

	data = g_new0(struct gaim_gtkpounce_data, 1);

	data->actions = actions;

	if (message != NULL) data->message = g_strdup(message);
	if (command != NULL) data->command = g_strdup(command);
	if (sound   != NULL) data->sound   = g_strdup(sound);

	data->save = save;

	return gaim_pounce_new(pouncer, pouncee, events, pounce_cb, data,
						   free_pounce);
}

void
gaim_gtkpounce_dialog_show(struct buddy *buddy,
						   struct gaim_pounce *cur_pounce)
{
	struct gaim_gtkpounce_dialog *dialog;
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *sep;
	GtkSizeGroup *sg;
	GPtrArray *sound_widgets;
	GPtrArray *exec_widgets;
	
	dialog = g_new0(struct gaim_gtkpounce_dialog, 1);

	if (cur_pounce != NULL) {
		dialog->pounce  = cur_pounce;
		dialog->account = gaim_pounce_get_pouncer(cur_pounce);
	}
	else if (buddy != NULL) {
		dialog->pounce  = NULL;
		dialog->account = buddy->account;
	}
	else {
		dialog->pounce  = NULL;
		dialog->account = gaim_accounts->data;
	}

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Create the window. */
	dialog->window = window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_role(GTK_WINDOW(window), "buddy_pounce");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_title(GTK_WINDOW(window),
						 (cur_pounce == NULL
						  ? _("New Buddy Pounce") : _("Edit Buddy Pounce")));

	gtk_container_set_border_width(GTK_CONTAINER(window), 12);
	gtk_widget_realize(window);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox1 = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(window), vbox1);
	gtk_widget_show(vbox1);

	/* Create the vbox that will contain all the prefs stuff. */
	vbox2 = gtk_vbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, TRUE, TRUE, 0);

	/* Create the "Pounce Who" frame. */
	frame = make_frame(vbox2, _("Pounce Who"));

	/* Account: */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	dialog->account_menu = pounce_user_menu(dialog);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->account_menu, FALSE, FALSE, 0);
	gtk_widget_show(dialog->account_menu);

	/* Buddy: */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Buddy Name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	dialog->buddy_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), dialog->buddy_entry, TRUE, TRUE, 0);
	gtk_widget_show(dialog->buddy_entry);

	if (cur_pounce != NULL) {
		gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry),
						   gaim_pounce_get_pouncee(cur_pounce));
	}
	else if (buddy != NULL) {
		gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry), buddy->name);
	}

	/* Create the "Pounce When" frame. */
	frame = make_frame(vbox2, _("Pounce When"));

	table = gtk_table_new(2, 4, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 12);
	gtk_widget_show(table);

	dialog->signon =
		gtk_check_button_new_with_label(_("Sign on"));
	dialog->signoff =
		gtk_check_button_new_with_label(_("Sign off"));
	dialog->away =
		gtk_check_button_new_with_label(_("Away"));
	dialog->away_return =
		gtk_check_button_new_with_label(_("Return from away"));
	dialog->idle =
		gtk_check_button_new_with_label(_("Idle"));
	dialog->idle_return =
		gtk_check_button_new_with_label(_("Return from idle"));
	dialog->typing =
		gtk_check_button_new_with_label(_("Buddy starts typing"));
	dialog->stop_typing =
		gtk_check_button_new_with_label(_("Buddy stops typing"));

	gtk_table_attach(GTK_TABLE(table), dialog->signon,      0, 1, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->signoff,     1, 2, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->away,        0, 1, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->away_return, 1, 2, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->idle,        0, 1, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->idle_return, 1, 2, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->typing,      0, 1, 3, 4,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->stop_typing, 1, 2, 3, 5,
					 GTK_FILL, 0, 0, 0);

	gtk_widget_show(dialog->signon);
	gtk_widget_show(dialog->signoff);
	gtk_widget_show(dialog->away);
	gtk_widget_show(dialog->away_return);
	gtk_widget_show(dialog->idle);
	gtk_widget_show(dialog->idle_return);
	gtk_widget_show(dialog->typing);
	gtk_widget_show(dialog->stop_typing);

	/* Create the "Pounce Action" frame. */
	frame = make_frame(vbox2, _("Pounce Action"));

	table = gtk_table_new(3, 5, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), 12);
	gtk_widget_show(table);

	dialog->open_win = gtk_check_button_new_with_label(_("Open an IM window"));
	dialog->popup = gtk_check_button_new_with_label(_("Popup notification"));
	dialog->send_msg = gtk_check_button_new_with_label(_("Send a message"));
	dialog->exec_cmd = gtk_check_button_new_with_label(_("Execute a command"));
	dialog->play_sound = gtk_check_button_new_with_label(_("Play a sound"));
	
	dialog->send_msg_entry   = gtk_entry_new();
	dialog->exec_cmd_entry   = gtk_entry_new();
	dialog->exec_cmd_browse = gtk_button_new_with_label(_("Browse"));
	dialog->play_sound_entry = gtk_entry_new();
	dialog->play_sound_browse = gtk_button_new_with_label(_("Browse"));
	dialog->play_sound_test = gtk_button_new_with_label(_("Test"));
	
	gtk_widget_set_sensitive(dialog->send_msg_entry,   FALSE);
	gtk_widget_set_sensitive(dialog->exec_cmd_entry,   FALSE);
	gtk_widget_set_sensitive(dialog->exec_cmd_browse, FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_entry, FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_browse,   FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_test, FALSE);

	gtk_table_attach(GTK_TABLE(table), dialog->open_win,         0, 1, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->popup,            0, 1, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->send_msg,         0, 1, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->send_msg_entry,   1, 4, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->exec_cmd,         0, 1, 3, 4,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->exec_cmd_entry,   1, 2, 3, 4,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->exec_cmd_browse,   2, 3, 3, 4,
					 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->play_sound,       0, 1, 4, 5,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->play_sound_entry, 1, 2, 4, 5,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->play_sound_browse, 2, 3, 4, 5,
					 GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->play_sound_test, 3, 4, 4, 5,
					 GTK_FILL | GTK_EXPAND, 0, 0, 0);

	gtk_widget_show(dialog->open_win);
	gtk_widget_show(dialog->popup);
	gtk_widget_show(dialog->send_msg);
	gtk_widget_show(dialog->send_msg_entry);
	gtk_widget_show(dialog->exec_cmd);
	gtk_widget_show(dialog->exec_cmd_entry);
	gtk_widget_show(dialog->exec_cmd_browse);
	gtk_widget_show(dialog->play_sound);
	gtk_widget_show(dialog->play_sound_entry);
	gtk_widget_show(dialog->play_sound_browse);
	gtk_widget_show(dialog->play_sound_test);

	g_signal_connect(G_OBJECT(dialog->send_msg), "clicked",
					 G_CALLBACK(gaim_gtk_toggle_sensitive),
					 dialog->send_msg_entry);

	exec_widgets = g_ptr_array_new();
	g_ptr_array_add(exec_widgets,dialog->exec_cmd_entry);
	g_ptr_array_add(exec_widgets,dialog->exec_cmd_browse);
	
	g_signal_connect(G_OBJECT(dialog->exec_cmd), "clicked",
					 G_CALLBACK(gtk_toggle_sensitive_array),
					 exec_widgets);
	g_signal_connect(G_OBJECT(dialog->exec_cmd_browse), "clicked",
					 G_CALLBACK(pounce_file_selector),
					 dialog->exec_cmd_entry);

	sound_widgets = g_ptr_array_new();
	g_ptr_array_add(sound_widgets,dialog->play_sound_entry);
	g_ptr_array_add(sound_widgets,dialog->play_sound_browse);
	g_ptr_array_add(sound_widgets,dialog->play_sound_test);

	g_signal_connect(G_OBJECT(dialog->play_sound), "clicked",
					 G_CALLBACK(gtk_toggle_sensitive_array),
					 sound_widgets);
	g_signal_connect(G_OBJECT(dialog->play_sound_browse), "clicked",
					 G_CALLBACK(pounce_file_selector),
					 dialog->play_sound_entry);
	g_signal_connect(G_OBJECT(dialog->play_sound_test), "clicked",
					 G_CALLBACK(pounce_test_sound),
					 dialog->play_sound_entry);
	
	g_signal_connect(G_OBJECT(dialog->send_msg_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->exec_cmd_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->play_sound_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	
	/* Now the last part, where we have the Save checkbox */
	dialog->save_pounce = gtk_check_button_new_with_mnemonic(
		_("_Save this pounce after activation"));

	gtk_box_pack_start(GTK_BOX(vbox2), dialog->save_pounce, FALSE, FALSE, 0);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox1), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Now the button box! */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox1), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(delete_cb), dialog);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(cancel_cb), dialog);

	/* OK button */
	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(save_pounce_cb), dialog);

	/* Set the values of stuff. */
	if (cur_pounce != NULL) {
		GaimPounceEvent events;
		GaimGtkPounceAction actions;
		struct gaim_gtkpounce_data *pounce_data;

		pounce_data = GAIM_GTKPOUNCE(cur_pounce);
		events      = gaim_pounce_get_events(cur_pounce);
		actions     = pounce_data->actions;

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->signon),
									(events & GAIM_POUNCE_SIGNON));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->signoff),
									(events & GAIM_POUNCE_SIGNOFF));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->away),
									(events & GAIM_POUNCE_AWAY));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->away_return),
									(events & GAIM_POUNCE_AWAY_RETURN));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->idle),
									(events & GAIM_POUNCE_IDLE));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->idle_return),
									(events & GAIM_POUNCE_IDLE_RETURN));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->typing),
									(events & GAIM_POUNCE_TYPING));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->stop_typing),
									(events & GAIM_POUNCE_TYPING_STOPPED));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->open_win),
									(actions & GAIM_GTKPOUNCE_OPEN_WIN));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->popup),
									(actions & GAIM_GTKPOUNCE_POPUP));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->send_msg),
									(actions & GAIM_GTKPOUNCE_SEND_MSG));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd),
									(actions & GAIM_GTKPOUNCE_EXEC_CMD));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->play_sound),
									(actions & GAIM_GTKPOUNCE_PLAY_SOUND));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->save_pounce),
									 pounce_data->save);

		if (pounce_data->message != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->send_msg_entry),
							   pounce_data->message);
		if (pounce_data->command != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->exec_cmd_entry),
							   pounce_data->command);
		if (pounce_data->sound != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->play_sound_entry),
							   pounce_data->sound);
	}
	else {
		/* Set some defaults */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->send_msg), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->signon), TRUE);
	}

	gtk_widget_show_all(vbox2);
	gtk_widget_show(window);
}

static void
new_pounce_cb(GtkWidget *w, struct buddy *b)
{
	gaim_gtkpounce_dialog_show(b, NULL);
}

static void
delete_pounce_cb(GtkWidget *w, struct gaim_pounce *pounce)
{
	gaim_pounce_destroy(pounce);
}

static void
edit_pounce_cb(GtkWidget *w, struct gaim_pounce *pounce)
{
	struct buddy *buddy;

	buddy = gaim_find_buddy(gaim_pounce_get_pouncer(pounce),
							gaim_pounce_get_pouncee(pounce));

	gaim_gtkpounce_dialog_show(buddy, pounce);
}

static void
fill_menu(GtkWidget *menu, GCallback cb)
{
	GtkWidget *image;
	GtkWidget *item;
	GdkPixbuf *pixbuf, *scale;
	struct gaim_pounce *pounce;
	const char *buddy;
	GList *bp;

	for (bp = gaim_get_pounces(); bp != NULL; bp = bp->next) {
		pounce = (struct gaim_pounce *)bp->data;
		buddy = gaim_pounce_get_pouncee(pounce);

		/* Build the menu item */
		item = gtk_image_menu_item_new_with_label(buddy);

		/* Create a pixmap for the protocol icon. */
		pixbuf = create_prpl_icon(gaim_pounce_get_pouncer(pounce));
		if(pixbuf) {
			scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16,
					GDK_INTERP_BILINEAR);

			/* Now convert it to GtkImage */
			image = gtk_image_new_from_pixbuf(scale);
			g_object_unref(G_OBJECT(scale));
			g_object_unref(G_OBJECT(pixbuf));
			gtk_widget_show(image);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
		}

		/* Put the item in the menu */
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		/* Set our callbacks. */
		g_signal_connect(G_OBJECT(item), "activate", cb, pounce);
	}
}

void
gaim_gtkpounce_menu_build(GtkWidget *menu)
{
	GtkWidget *remmenu;
	GtkWidget *item;
	GList *l;

	for (l = gtk_container_get_children(GTK_CONTAINER(menu));
		 l != NULL;
		 l = l->next) {

		gtk_widget_destroy(GTK_WIDGET(l->data));
	}

	/* "New Buddy Pounce" */
	item = gtk_menu_item_new_with_label(_("New Buddy Pounce"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(new_pounce_cb), NULL);

	/* "Remove Buddy Pounce" */
	item = gtk_menu_item_new_with_label(_("Remove Buddy Pounce"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	/* "Remove Buddy Pounce" menu */
	remmenu = gtk_menu_new();

	fill_menu(remmenu, G_CALLBACK(delete_pounce_cb));

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), remmenu);
	gtk_widget_show(remmenu);
	gtk_widget_show(item);

	/* Separator */
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	fill_menu(menu, G_CALLBACK(edit_pounce_cb));
}


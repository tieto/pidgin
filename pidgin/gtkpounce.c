/**
 * @file gtkpounce.c GTK+ Buddy Pounce API
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
 *
 */
#include "internal.h"
#include "gtkgaim.h"

#include "account.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "sound.h"
#include "util.h"

#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkpounce.h"
#include "gaimstock.h"
#include "gtkutils.h"

/**
 * These are used for the GtkTreeView when you're scrolling through
 * all your saved pounces.
 */
enum
{
	/* Hidden column containing the GaimPounce */
	POUNCES_MANAGER_COLUMN_POUNCE,
	POUNCES_MANAGER_COLUMN_ICON,
	POUNCES_MANAGER_COLUMN_TARGET,
	POUNCES_MANAGER_COLUMN_ACCOUNT,
	POUNCES_MANAGER_COLUMN_RECURRING,
	POUNCES_MANAGER_NUM_COLUMNS
};

typedef struct
{
	/* Pounce data */
	GaimPounce  *pounce;
	GaimAccount *account;

	/* The window */
	GtkWidget *window;

	/* Pounce on Whom */
	GtkWidget *account_menu;
	GtkWidget *buddy_entry;

	/* Pounce options */
	GtkWidget *on_away;

	/* Pounce When Buddy... */
	GtkWidget *signon;
	GtkWidget *signoff;
	GtkWidget *away;
	GtkWidget *away_return;
	GtkWidget *idle;
	GtkWidget *idle_return;
	GtkWidget *typing;
	GtkWidget *typed;
	GtkWidget *stop_typing;
	GtkWidget *message_recv;

	/* Action */
	GtkWidget *open_win;
	GtkWidget *popup;
	GtkWidget *popup_entry;
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

	/* Buttons */
	GtkWidget *save_button;

} PidginPounceDialog;

typedef struct
{
	GtkWidget *window;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *modify_button;
	GtkWidget *delete_button;
} PouncesManager;

static PouncesManager *pounces_manager = NULL;

/**************************************************************************
 * Callbacks
 **************************************************************************/
static gint
delete_win_cb(GtkWidget *w, GdkEventAny *e, PidginPounceDialog *dialog)
{
	gtk_widget_destroy(dialog->window);
	g_free(dialog);

	return TRUE;
}

static void
cancel_cb(GtkWidget *w, PidginPounceDialog *dialog)
{
	delete_win_cb(NULL, NULL, dialog);
}

static void
pounce_update_entry_fields(void *user_data, const char *filename)
{
	GtkWidget *entry = (GtkWidget *)user_data;

	gtk_entry_set_text(GTK_ENTRY(entry), filename);
}

static void
filesel(GtkWidget *widget, gpointer data)
{
	GtkWidget *entry;
	const gchar *name;

	entry = (GtkWidget *)data;
	name = gtk_entry_get_text(GTK_ENTRY(entry));

	gaim_request_file(entry, _("Select a file"), name, FALSE,
					  G_CALLBACK(pounce_update_entry_fields), NULL, entry);
	g_signal_connect_swapped(G_OBJECT(entry), "destroy",
			G_CALLBACK(gaim_request_close_with_handle), entry);
}

static void
pounce_test_sound(GtkWidget *w, GtkWidget *entry)
{
	const char *filename;

	filename = gtk_entry_get_text(GTK_ENTRY(entry));

	if (filename != NULL && *filename != '\0')
		gaim_sound_play_file(filename, NULL);
	else
		gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT, NULL);
}

static void
add_pounce_to_treeview(GtkListStore *model, GaimPounce *pounce)
{
	GtkTreeIter iter;
	GaimAccount *account;
	GaimPounceEvent events;
	gboolean recurring;
	const char *pouncer;
	const char *pouncee;
	GdkPixbuf *pixbuf;

	account = gaim_pounce_get_pouncer(pounce);

	events = gaim_pounce_get_events(pounce);

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);

	pouncer = gaim_account_get_username(account);
	pouncee = gaim_pounce_get_pouncee(pounce);
	recurring = gaim_pounce_get_save(pounce);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
					   POUNCES_MANAGER_COLUMN_POUNCE, pounce,
					   POUNCES_MANAGER_COLUMN_ICON, pixbuf,
					   POUNCES_MANAGER_COLUMN_TARGET, pouncee,
					   POUNCES_MANAGER_COLUMN_ACCOUNT, pouncer,
					   POUNCES_MANAGER_COLUMN_RECURRING, recurring,
					   -1);

	if (pixbuf != NULL)
		g_object_unref(pixbuf);
}

static void
populate_pounces_list(PouncesManager *dialog)
{
	const GList *pounces;

	gtk_list_store_clear(dialog->model);

	for (pounces = gaim_pounces_get_all(); pounces != NULL;
			pounces = g_list_next(pounces))
	{
		add_pounce_to_treeview(dialog->model, pounces->data);
	}
}

static void
update_pounces(void)
{
	/* Rebuild the pounces list if the pounces manager is open */
	if (pounces_manager != NULL)
	{
		populate_pounces_list(pounces_manager);
	}
}

static void
signed_on_off_cb(GaimConnection *gc, gpointer user_data)
{
	update_pounces();
}

static void
save_pounce_cb(GtkWidget *w, PidginPounceDialog *dialog)
{
	const char *name;
	const char *message, *command, *sound, *reason;
	GaimPounceEvent events   = GAIM_POUNCE_NONE;
	GaimPounceOption options = GAIM_POUNCE_OPTION_NONE;

	name = gtk_entry_get_text(GTK_ENTRY(dialog->buddy_entry));

	if (*name == '\0')
	{
		gaim_notify_error(NULL, NULL,
						  _("Please enter a buddy to pounce."), NULL);
		return;
	}

	/* Options */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->on_away)))
		options |= GAIM_POUNCE_OPTION_AWAY;

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

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->typed)))
		events |= GAIM_POUNCE_TYPED;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->stop_typing)))
		events |= GAIM_POUNCE_TYPING_STOPPED;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->message_recv)))
		events |= GAIM_POUNCE_MESSAGE_RECEIVED;

	/* Data fields */
	message = gtk_entry_get_text(GTK_ENTRY(dialog->send_msg_entry));
	command = gtk_entry_get_text(GTK_ENTRY(dialog->exec_cmd_entry));
	sound   = gtk_entry_get_text(GTK_ENTRY(dialog->play_sound_entry));
	reason  = gtk_entry_get_text(GTK_ENTRY(dialog->popup_entry));

	if (*reason == '\0') reason = NULL;
	if (*message == '\0') message = NULL;
	if (*command == '\0') command = NULL;
	if (*sound   == '\0') sound   = NULL;

	if (dialog->pounce == NULL)
	{
		dialog->pounce = gaim_pounce_new(PIDGIN_UI, dialog->account,
										 name, events, options);
	}
	else {
		gaim_pounce_set_events(dialog->pounce, events);
		gaim_pounce_set_options(dialog->pounce, options);
		gaim_pounce_set_pouncer(dialog->pounce, dialog->account);
		gaim_pounce_set_pouncee(dialog->pounce, name);
	}

	/* Actions */
	gaim_pounce_action_set_enabled(dialog->pounce, "open-window",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->open_win)));
	gaim_pounce_action_set_enabled(dialog->pounce, "popup-notify",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->popup)));
	gaim_pounce_action_set_enabled(dialog->pounce, "send-message",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->send_msg)));
	gaim_pounce_action_set_enabled(dialog->pounce, "execute-command",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd)));
	gaim_pounce_action_set_enabled(dialog->pounce, "play-sound",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->play_sound)));

	gaim_pounce_action_set_attribute(dialog->pounce, "send-message",
									 "message", message);
	gaim_pounce_action_set_attribute(dialog->pounce, "execute-command",
									 "command", command);
	gaim_pounce_action_set_attribute(dialog->pounce, "play-sound",
									 "filename", sound);
	gaim_pounce_action_set_attribute(dialog->pounce, "popup-notify",
									 "reason", reason);

	/* Set the defaults for next time. */
	gaim_prefs_set_bool("/gaim/gtk/pounces/default_actions/open-window",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->open_win)));
	gaim_prefs_set_bool("/gaim/gtk/pounces/default_actions/popup-notify",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->popup)));
	gaim_prefs_set_bool("/gaim/gtk/pounces/default_actions/send-message",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->send_msg)));
	gaim_prefs_set_bool("/gaim/gtk/pounces/default_actions/execute-command",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd)));
	gaim_prefs_set_bool("/gaim/gtk/pounces/default_actions/play-sound",
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->play_sound)));

	gaim_pounce_set_save(dialog->pounce,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->save_pounce)));

	update_pounces();

	delete_win_cb(NULL, NULL, dialog);
}

static void
pounce_choose_cb(GtkWidget *item, GaimAccount *account,
				 PidginPounceDialog *dialog)
{
	dialog->account = account;
}

static void
buddy_changed_cb(GtkEntry *entry, PidginPounceDialog *dialog)
{
	if (dialog->save_button == NULL)
		return;

	gtk_widget_set_sensitive(dialog->save_button,
		*gtk_entry_get_text(entry) != '\0');
}

static void
message_recv_toggle(GtkButton *message_recv, GtkWidget *send_msg)
{
	gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(message_recv));

	gtk_widget_set_sensitive(send_msg, !active);
	if (active)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(send_msg), FALSE);
}

static void
pounce_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
				GtkSelectionData *sd, guint info, guint t, gpointer data)
{
	PidginPounceDialog *dialog;

	if (sd->target == gdk_atom_intern("GAIM_BLIST_NODE", FALSE))
	{
		GaimBlistNode *node = NULL;
		GaimBuddy *buddy;

		memcpy(&node, sd->data, sizeof(node));

		if (GAIM_BLIST_NODE_IS_CONTACT(node))
			buddy = gaim_contact_get_priority_buddy((GaimContact *)node);
		else if (GAIM_BLIST_NODE_IS_BUDDY(node))
			buddy = (GaimBuddy *)node;
		else
			return;

		dialog = (PidginPounceDialog *)data;

		gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry), buddy->name);
		dialog->account = buddy->account;
		pidgin_account_option_menu_set_selected(dialog->account_menu, buddy->account);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
	else if (sd->target == gdk_atom_intern("application/x-im-contact", FALSE))
	{
		char *protocol = NULL;
		char *username = NULL;
		GaimAccount *account;

		if (pidgin_parse_x_im_contact((const char *)sd->data, FALSE, &account,
										&protocol, &username, NULL))
		{
			if (account == NULL)
			{
				gaim_notify_error(NULL, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL);
			}
			else
			{
				dialog = (PidginPounceDialog *)data;

				gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry), username);
				dialog->account = account;
				pidgin_account_option_menu_set_selected(dialog->account_menu, account);
			}
		}

		g_free(username);
		g_free(protocol);

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	}
}

static const GtkTargetEntry dnd_targets[] =
{
	{"GAIM_BLIST_NODE", GTK_TARGET_SAME_APP, 0},
	{"application/x-im-contact", 0, 1}
};

void
pidgin_pounce_editor_show(GaimAccount *account, const char *name,
							GaimPounce *cur_pounce)
{
	PidginPounceDialog *dialog;
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *frame;
	GtkWidget *table;
	GtkSizeGroup *sg;
	GPtrArray *sound_widgets;
	GPtrArray *exec_widgets;

	g_return_if_fail((cur_pounce != NULL) ||
	                 (account != NULL) ||
	                 (gaim_accounts_get_all() != NULL));

	dialog = g_new0(PidginPounceDialog, 1);

	if (cur_pounce != NULL)
	{
		dialog->pounce  = cur_pounce;
		dialog->account = gaim_pounce_get_pouncer(cur_pounce);
	}
	else if (account != NULL)
	{
		dialog->pounce  = NULL;
		dialog->account = account;
	}
	else
	{
		GList *connections = gaim_connections_get_all();
		GaimConnection *gc;

		if (connections != NULL)
		{
			gc = (GaimConnection *)connections->data;
			dialog->account = gaim_connection_get_account(gc);
		}
		else
			dialog->account = gaim_accounts_get_all()->data;

		dialog->pounce  = NULL;
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

	gtk_container_set_border_width(GTK_CONTAINER(window), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(delete_win_cb), dialog);

	/* Create the parent vbox for everything. */
	vbox1 = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(window), vbox1);
	gtk_widget_show(vbox1);

	/* Create the vbox that will contain all the prefs stuff. */
	vbox2 = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, TRUE, TRUE, 0);

	/* Create the "Pounce on Whom" frame. */
	frame = pidgin_make_frame(vbox2, _("Pounce on Whom"));

	/* Account: */
	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Account:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	dialog->account_menu =
		pidgin_account_option_menu_new(dialog->account, TRUE,
										 G_CALLBACK(pounce_choose_cb),
										 NULL, dialog);

	gtk_box_pack_start(GTK_BOX(hbox), dialog->account_menu, FALSE, FALSE, 0);
	gtk_widget_show(dialog->account_menu);
	pidgin_set_accessible_label (dialog->account_menu, label);

	/* Buddy: */
	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(frame), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(_("_Buddy name:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_size_group_add_widget(sg, label);

	dialog->buddy_entry = gtk_entry_new();

	pidgin_setup_screenname_autocomplete(dialog->buddy_entry, dialog->account_menu, FALSE);

	gtk_box_pack_start(GTK_BOX(hbox), dialog->buddy_entry, TRUE, TRUE, 0);
	gtk_widget_show(dialog->buddy_entry);

	g_signal_connect(G_OBJECT(dialog->buddy_entry), "changed",
			 G_CALLBACK(buddy_changed_cb), dialog);
	pidgin_set_accessible_label (dialog->buddy_entry, label);

	if (cur_pounce != NULL) {
		gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry),
						   gaim_pounce_get_pouncee(cur_pounce));
	}
	else if (name != NULL) {
		gtk_entry_set_text(GTK_ENTRY(dialog->buddy_entry), name);
	}

	/* Create the "Pounce When Buddy..." frame. */
	frame = pidgin_make_frame(vbox2, _("Pounce When Buddy..."));

	table = gtk_table_new(5, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), GAIM_HIG_BORDER);
	gtk_widget_show(table);

	dialog->signon =
		gtk_check_button_new_with_mnemonic(_("Si_gns on"));
	dialog->signoff =
		gtk_check_button_new_with_mnemonic(_("Signs o_ff"));
	dialog->away =
		gtk_check_button_new_with_mnemonic(_("Goes a_way"));
	dialog->away_return =
		gtk_check_button_new_with_mnemonic(_("Ret_urns from away"));
	dialog->idle =
		gtk_check_button_new_with_mnemonic(_("Becomes _idle"));
	dialog->idle_return =
		gtk_check_button_new_with_mnemonic(_("Is no longer i_dle"));
	dialog->typing =
		gtk_check_button_new_with_mnemonic(_("Starts _typing"));
	dialog->typed =
		gtk_check_button_new_with_mnemonic(_("P_auses while typing"));
	dialog->stop_typing =
		gtk_check_button_new_with_mnemonic(_("Stops t_yping"));
	dialog->message_recv =
		gtk_check_button_new_with_mnemonic(_("Sends a _message"));

	gtk_table_attach(GTK_TABLE(table), dialog->message_recv, 0, 1, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->signon,       0, 1, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->signoff,      0, 1, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->away,         0, 1, 3, 4,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->away_return,  0, 1, 4, 5,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->idle,         1, 2, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->idle_return,  1, 2, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->typing,       1, 2, 2, 3,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->typed,        1, 2, 3, 4,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->stop_typing,  1, 2, 4, 5,
					 GTK_FILL, 0, 0, 0);

	gtk_widget_show(dialog->signon);
	gtk_widget_show(dialog->signoff);
	gtk_widget_show(dialog->away);
	gtk_widget_show(dialog->away_return);
	gtk_widget_show(dialog->idle);
	gtk_widget_show(dialog->idle_return);
	gtk_widget_show(dialog->typing);
	gtk_widget_show(dialog->typed);
	gtk_widget_show(dialog->stop_typing);
	gtk_widget_show(dialog->message_recv);

	/* Create the "Action" frame. */
	frame = pidgin_make_frame(vbox2, _("Action"));

	table = gtk_table_new(3, 5, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), GAIM_HIG_BORDER);
	gtk_widget_show(table);

	dialog->open_win
		= gtk_check_button_new_with_mnemonic(_("Ope_n an IM window"));
	dialog->popup
		= gtk_check_button_new_with_mnemonic(_("_Pop up a notification"));
	dialog->send_msg
		= gtk_check_button_new_with_mnemonic(_("Send a _message"));
	dialog->exec_cmd
		= gtk_check_button_new_with_mnemonic(_("E_xecute a command"));
	dialog->play_sound
		= gtk_check_button_new_with_mnemonic(_("P_lay a sound"));

	dialog->send_msg_entry    = gtk_entry_new();
	dialog->exec_cmd_entry    = gtk_entry_new();
	dialog->popup_entry       = gtk_entry_new();
	dialog->exec_cmd_browse   = gtk_button_new_with_mnemonic(_("Brows_e..."));
	dialog->play_sound_entry  = gtk_entry_new();
	dialog->play_sound_browse = gtk_button_new_with_mnemonic(_("Br_owse..."));
	dialog->play_sound_test   = gtk_button_new_with_mnemonic(_("Pre_view"));

	gtk_widget_set_sensitive(dialog->send_msg_entry,    FALSE);
	gtk_widget_set_sensitive(dialog->exec_cmd_entry,    FALSE);
	gtk_widget_set_sensitive(dialog->popup_entry,       FALSE);
	gtk_widget_set_sensitive(dialog->exec_cmd_browse,   FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_entry,  FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_browse, FALSE);
	gtk_widget_set_sensitive(dialog->play_sound_test,   FALSE);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);
	gtk_size_group_add_widget(sg, dialog->open_win);
	gtk_size_group_add_widget(sg, dialog->popup);
	gtk_size_group_add_widget(sg, dialog->popup_entry);
	gtk_size_group_add_widget(sg, dialog->send_msg);
	gtk_size_group_add_widget(sg, dialog->send_msg_entry);
	gtk_size_group_add_widget(sg, dialog->exec_cmd);
	gtk_size_group_add_widget(sg, dialog->exec_cmd_entry);
	gtk_size_group_add_widget(sg, dialog->exec_cmd_browse);
	gtk_size_group_add_widget(sg, dialog->play_sound);
	gtk_size_group_add_widget(sg, dialog->play_sound_entry);
	gtk_size_group_add_widget(sg, dialog->play_sound_browse);
	gtk_size_group_add_widget(sg, dialog->play_sound_test);

	gtk_table_attach(GTK_TABLE(table), dialog->open_win,         0, 1, 0, 1,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->popup,            0, 1, 1, 2,
					 GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dialog->popup_entry,      1, 4, 1, 2,
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

	gtk_table_set_row_spacings(GTK_TABLE(table), GAIM_HIG_BOX_SPACE / 2);

	gtk_widget_show(dialog->open_win);
	gtk_widget_show(dialog->popup);
	gtk_widget_show(dialog->popup_entry);
	gtk_widget_show(dialog->send_msg);
	gtk_widget_show(dialog->send_msg_entry);
	gtk_widget_show(dialog->exec_cmd);
	gtk_widget_show(dialog->exec_cmd_entry);
	gtk_widget_show(dialog->exec_cmd_browse);
	gtk_widget_show(dialog->play_sound);
	gtk_widget_show(dialog->play_sound_entry);
	gtk_widget_show(dialog->play_sound_browse);
	gtk_widget_show(dialog->play_sound_test);

	g_signal_connect(G_OBJECT(dialog->message_recv), "clicked",
					 G_CALLBACK(message_recv_toggle),
					 dialog->send_msg);

	g_signal_connect(G_OBJECT(dialog->send_msg), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive),
					 dialog->send_msg_entry);

	g_signal_connect(G_OBJECT(dialog->popup), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive),
					 dialog->popup_entry);

	exec_widgets = g_ptr_array_new();
	g_ptr_array_add(exec_widgets,dialog->exec_cmd_entry);
	g_ptr_array_add(exec_widgets,dialog->exec_cmd_browse);

	g_signal_connect(G_OBJECT(dialog->exec_cmd), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive_array),
					 exec_widgets);
	g_signal_connect(G_OBJECT(dialog->exec_cmd_browse), "clicked",
					 G_CALLBACK(filesel),
					 dialog->exec_cmd_entry);
	g_object_set_data_full(G_OBJECT(dialog->window), "exec-widgets",
				exec_widgets, (GDestroyNotify)g_ptr_array_free);

	sound_widgets = g_ptr_array_new();
	g_ptr_array_add(sound_widgets,dialog->play_sound_entry);
	g_ptr_array_add(sound_widgets,dialog->play_sound_browse);
	g_ptr_array_add(sound_widgets,dialog->play_sound_test);

	g_signal_connect(G_OBJECT(dialog->play_sound), "clicked",
					 G_CALLBACK(pidgin_toggle_sensitive_array),
					 sound_widgets);
	g_signal_connect(G_OBJECT(dialog->play_sound_browse), "clicked",
					 G_CALLBACK(filesel),
					 dialog->play_sound_entry);
	g_signal_connect(G_OBJECT(dialog->play_sound_test), "clicked",
					 G_CALLBACK(pounce_test_sound),
					 dialog->play_sound_entry);
	g_object_set_data_full(G_OBJECT(dialog->window), "sound-widgets",
				sound_widgets, (GDestroyNotify)g_ptr_array_free);

	g_signal_connect(G_OBJECT(dialog->send_msg_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->popup_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->exec_cmd_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);
	g_signal_connect(G_OBJECT(dialog->play_sound_entry), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);

	/* Create the "Options" frame. */
	frame = pidgin_make_frame(vbox2, _("Options"));

	table = gtk_table_new(2, 1, FALSE);
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_set_col_spacings(GTK_TABLE(table), GAIM_HIG_BORDER);
	gtk_widget_show(table);

	dialog->on_away =
		gtk_check_button_new_with_mnemonic(_("P_ounce only when my status is not available"));
	gtk_table_attach(GTK_TABLE(table), dialog->on_away, 0, 1, 0, 1,
					 GTK_FILL, 0, 0, 0);

	dialog->save_pounce = gtk_check_button_new_with_mnemonic(
		_("_Recurring"));
	gtk_table_attach(GTK_TABLE(table), dialog->save_pounce, 0, 1, 1, 2,
					 GTK_FILL, 0, 0, 0);

	gtk_widget_show(dialog->on_away);
	gtk_widget_show(dialog->save_pounce);

	/* Now the button box! */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox1), bbox, FALSE, FALSE, 0);
	gtk_widget_show(bbox);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(cancel_cb), dialog);

	/* Save button */
	dialog->save_button = button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(save_pounce_cb), dialog);

	if (*gtk_entry_get_text(GTK_ENTRY(dialog->buddy_entry)) == '\0')
		gtk_widget_set_sensitive(button, FALSE);

	/* Setup drag-and-drop */
	gtk_drag_dest_set(window,
					  GTK_DEST_DEFAULT_MOTION |
					  GTK_DEST_DEFAULT_DROP,
					  dnd_targets,
					  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
					  GDK_ACTION_COPY);
	gtk_drag_dest_set(dialog->buddy_entry,
					  GTK_DEST_DEFAULT_MOTION |
					  GTK_DEST_DEFAULT_DROP,
					  dnd_targets,
					  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
					  GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(window), "drag_data_received",
					 G_CALLBACK(pounce_dnd_recv), dialog);
	g_signal_connect(G_OBJECT(dialog->buddy_entry), "drag_data_received",
					 G_CALLBACK(pounce_dnd_recv), dialog);

	/* Set the values of stuff. */
	if (cur_pounce != NULL)
	{
		GaimPounceEvent events   = gaim_pounce_get_events(cur_pounce);
		GaimPounceOption options = gaim_pounce_get_options(cur_pounce);
		const char *value;

		/* Options */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->on_away),
									(options & GAIM_POUNCE_OPTION_AWAY));

		/* Events */
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
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->typed),
									(events & GAIM_POUNCE_TYPED));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->stop_typing),
									(events & GAIM_POUNCE_TYPING_STOPPED));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->message_recv),
									(events & GAIM_POUNCE_MESSAGE_RECEIVED));

		/* Actions */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->open_win),
			gaim_pounce_action_is_enabled(cur_pounce, "open-window"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->popup),
			gaim_pounce_action_is_enabled(cur_pounce, "popup-notify"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->send_msg),
			gaim_pounce_action_is_enabled(cur_pounce, "send-message"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd),
			gaim_pounce_action_is_enabled(cur_pounce, "execute-command"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->play_sound),
			gaim_pounce_action_is_enabled(cur_pounce, "play-sound"));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->save_pounce),
			gaim_pounce_get_save(cur_pounce));

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
													  "send-message",
													  "message")) != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dialog->send_msg_entry), value);
		}

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
													  "popup-notify",
													  "reason")) != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dialog->popup_entry), value);
		}

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
													  "execute-command",
													  "command")) != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dialog->exec_cmd_entry), value);
		}

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
													  "play-sound",
													  "filename")) != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dialog->play_sound_entry), value);
		}
	}
	else
	{
		GaimBuddy *buddy = NULL;

		if (name != NULL)
			buddy = gaim_find_buddy(account, name);

		/* Set some defaults */
		if (buddy == NULL)
		{
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->signon), TRUE);
		}
		else
		{
			if (!GAIM_BUDDY_IS_ONLINE(buddy))
			{
				gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(dialog->signon), TRUE);
			}
			else
			{
				gboolean default_set = FALSE;
				GaimPresence *presence = gaim_buddy_get_presence(buddy);

				if (gaim_presence_is_idle(presence))
				{
					gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(dialog->idle_return), TRUE);

					default_set = TRUE;
				}

				if (!gaim_presence_is_available(presence))
				{
					gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(dialog->away_return), TRUE);

					default_set = TRUE;
				}

				if (!default_set)
				{
					gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(dialog->signon), TRUE);
				}
			}
		}

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->open_win),
			gaim_prefs_get_bool("/gaim/gtk/pounces/default_actions/open-window"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->popup),
			gaim_prefs_get_bool("/gaim/gtk/pounces/default_actions/popup-notify"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->send_msg),
			gaim_prefs_get_bool("/gaim/gtk/pounces/default_actions/send-message"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->exec_cmd),
			gaim_prefs_get_bool("/gaim/gtk/pounces/default_actions/execute-command"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->play_sound),
			gaim_prefs_get_bool("/gaim/gtk/pounces/default_actions/play-sound"));
	}

	gtk_widget_show_all(vbox2);
	gtk_widget_show(window);
}

static gboolean
pounces_manager_configure_cb(GtkWidget *widget, GdkEventConfigure *event, PouncesManager *dialog)
{
	if (GTK_WIDGET_VISIBLE(widget)) {
		gaim_prefs_set_int("/gaim/gtk/pounces/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/pounces/dialog/height", event->height);
	}

	return FALSE;
}

static gboolean
pounces_manager_find_pounce(GtkTreeIter *iter, GaimPounce *pounce)
{
	GtkTreeModel *model = GTK_TREE_MODEL(pounces_manager->model);
	GaimPounce *p;

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	gtk_tree_model_get(model, iter, POUNCES_MANAGER_COLUMN_POUNCE, &p, -1);
	if (pounce == p)
		return TRUE;

	while (gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, POUNCES_MANAGER_COLUMN_POUNCE, &p, -1);
		if (pounce == p)
			return TRUE;
	}

	return FALSE;
}

static gboolean
pounces_manager_destroy_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	PouncesManager *dialog = user_data;

	dialog->window = NULL;
	pidgin_pounces_manager_hide();

	return FALSE;
}

#if !GTK_CHECK_VERSION(2,2,0)
static void
count_selected_helper(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	(*(gint *)user_data)++;
}
#endif

static void
pounces_manager_connection_cb(GaimConnection *gc, GtkWidget *add_button)
{
	gtk_widget_set_sensitive(add_button, (gaim_connections_get_all() != NULL));
}

static void
pounces_manager_add_cb(GtkButton *button, gpointer user_data)
{
	pidgin_pounce_editor_show(NULL, NULL, NULL);
}

static void
pounces_manager_modify_foreach(GtkTreeModel *model, GtkTreePath *path,
							 GtkTreeIter *iter, gpointer user_data)
{
	GaimPounce *pounce;

	gtk_tree_model_get(model, iter, POUNCES_MANAGER_COLUMN_POUNCE, &pounce, -1);
	pidgin_pounce_editor_show(NULL, NULL, pounce);
}

static void
pounces_manager_modify_cb(GtkButton *button, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, pounces_manager_modify_foreach, user_data);
}

static void
pounces_manager_delete_confirm_cb(GaimPounce *pounce)
{
	GtkTreeIter iter;

	if (pounces_manager && pounces_manager_find_pounce(&iter, pounce))
		gtk_list_store_remove(pounces_manager->model, &iter);

	gaim_request_close_with_handle(pounce);
	gaim_pounce_destroy(pounce);
}

static void
pounces_manager_delete_foreach(GtkTreeModel *model, GtkTreePath *path,
							   GtkTreeIter *iter, gpointer user_data)
{
	GaimPounce *pounce;
	GaimAccount *account;
	const char *pouncer, *pouncee;
	char *buf;

	gtk_tree_model_get(model, iter, POUNCES_MANAGER_COLUMN_POUNCE, &pounce, -1);
	account = gaim_pounce_get_pouncer(pounce);
	pouncer = gaim_account_get_username(account);
	pouncee = gaim_pounce_get_pouncee(pounce);

	buf = g_strdup_printf(_("Are you sure you want to delete the pounce on %s for %s?"), pouncee, pouncer);
	gaim_request_action(pounce, NULL, buf, NULL, 0, pounce, 2,
						_("Delete"), pounces_manager_delete_confirm_cb,
						_("Cancel"), NULL);
	g_free(buf);
}

static void
pounces_manager_delete_cb(GtkButton *button, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, pounces_manager_delete_foreach, user_data);
}

static void
pounces_manager_close_cb(GtkButton *button, gpointer user_data)
{
	pidgin_pounces_manager_hide();
}

static void
pounce_selected_cb(GtkTreeSelection *sel, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	int num_selected = 0;

#if GTK_CHECK_VERSION(2,2,0)
	num_selected = gtk_tree_selection_count_selected_rows(sel);
#else
	gtk_tree_selection_selected_foreach(sel, count_selected_helper, &num_selected);
#endif

	gtk_widget_set_sensitive(dialog->modify_button, (num_selected > 0));
	gtk_widget_set_sensitive(dialog->delete_button, (num_selected > 0));
}

static gboolean
pounce_double_click_cb(GtkTreeView *treeview, GdkEventButton *event, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GtkTreePath *path;
	GtkTreeIter iter;
	GaimPounce *pounce;

	/* Figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(dialog->treeview), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter, POUNCES_MANAGER_COLUMN_POUNCE, &pounce, -1);

	if ((pounce != NULL) && (event->button == 1) &&
		(event->type == GDK_2BUTTON_PRESS))
	{
		pidgin_pounce_editor_show(NULL, NULL, pounce);
		return TRUE;
	}

	return FALSE;
}

static void
pounces_manager_recurring_cb(GtkCellRendererToggle *renderer, gchar *path_str,
							gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GaimPounce *pounce;
	gboolean recurring;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter,
					   POUNCES_MANAGER_COLUMN_POUNCE, &pounce,
					   POUNCES_MANAGER_COLUMN_RECURRING, &recurring,
					   -1);

	gaim_pounce_set_save(pounce, !recurring);

	update_pounces();
}

static gboolean
search_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	gboolean result;
	char *haystack;

	gtk_tree_model_get(model, iter, column, &haystack, -1);

	result = (gaim_strcasestr(haystack, key) == NULL);

	g_free(haystack);

	return result;
}

static GtkWidget *
create_pounces_list(PouncesManager *dialog)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Create the scrolled window */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
						GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list model */
	dialog->model = gtk_list_store_new(POUNCES_MANAGER_NUM_COLUMNS,
									   G_TYPE_POINTER,
									   GDK_TYPE_PIXBUF,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_BOOLEAN
									   );

	/* Create the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(pounce_selected_cb), dialog);

	/* Handle double-clicking */
	g_signal_connect(G_OBJECT(treeview), "button_press_event",
					 G_CALLBACK(pounce_double_click_cb), dialog);


	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	/* Pouncee Column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Pounce Target"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 200);
	gtk_tree_view_column_set_sort_column_id(column,
											POUNCES_MANAGER_COLUMN_TARGET);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf",
									   POUNCES_MANAGER_COLUMN_ICON);

	/* Pouncee */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   POUNCES_MANAGER_COLUMN_TARGET);


	/* Account Column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Account"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 200);
	gtk_tree_view_column_set_sort_column_id(column,
											POUNCES_MANAGER_COLUMN_ACCOUNT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   POUNCES_MANAGER_COLUMN_ACCOUNT);

	/* Recurring Column */
	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(_("Recurring"), renderer,
						"active", POUNCES_MANAGER_COLUMN_RECURRING, NULL);
	gtk_tree_view_column_set_sort_column_id(column,
											POUNCES_MANAGER_COLUMN_RECURRING);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(pounces_manager_recurring_cb), dialog);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), POUNCES_MANAGER_COLUMN_TARGET);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(treeview), search_func, NULL, NULL);

	/* Sort the pouncee column by default */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->model),
										 POUNCES_MANAGER_COLUMN_TARGET,
										 GTK_SORT_ASCENDING);

	/* Populate list */
	populate_pounces_list(dialog);

	return sw;
}

void
pidgin_pounces_manager_show(void)
{
	PouncesManager *dialog;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *list;
	GtkWidget *vbox;
	GtkWidget *win;
	int width, height;

	if (pounces_manager != NULL) {
		gtk_window_present(GTK_WINDOW(pounces_manager->window));
		return;
	}

	pounces_manager = dialog = g_new0(PouncesManager, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/pounces/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/pounces/dialog/height");

	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "pounces");
	gtk_window_set_title(GTK_WINDOW(win), _("Buddy Pounces"));
	gtk_container_set_border_width(GTK_CONTAINER(win), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(pounces_manager_destroy_cb), dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(pounces_manager_configure_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* List of saved buddy pounces */
	list = create_pounces_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), list, TRUE, TRUE, 0);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, (gaim_accounts_get_all() != NULL));
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						pounces_manager, GAIM_CALLBACK(pounces_manager_connection_cb), button);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						pounces_manager, GAIM_CALLBACK(pounces_manager_connection_cb), button);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pounces_manager_add_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(PIDGIN_STOCK_MODIFY);
	dialog->modify_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pounces_manager_modify_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	dialog->delete_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pounces_manager_delete_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(pounces_manager_close_cb), dialog);

	gtk_widget_show(win);
}

void
pidgin_pounces_manager_hide(void)
{
	if (pounces_manager == NULL)
		return;

	if (pounces_manager->window != NULL)
		gtk_widget_destroy(pounces_manager->window);

	gaim_signals_disconnect_by_handle(pounces_manager);

	g_free(pounces_manager);
	pounces_manager = NULL;
}

static void
pounce_cb(GaimPounce *pounce, GaimPounceEvent events, void *data)
{
	GaimConversation *conv;
	GaimAccount *account;
	GaimBuddy *buddy;
	const char *pouncee;
	const char *alias;

	pouncee = gaim_pounce_get_pouncee(pounce);
	account = gaim_pounce_get_pouncer(pounce);

	buddy = gaim_find_buddy(account, pouncee);
	if (buddy != NULL)
	{
		alias = gaim_buddy_get_alias(buddy);
		if (alias == NULL)
			alias = pouncee;
	}
	else
		alias = pouncee;

	if (gaim_pounce_action_is_enabled(pounce, "open-window"))
	{
		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, pouncee, account);

		if (conv == NULL)
			conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, pouncee);
	}

	if (gaim_pounce_action_is_enabled(pounce, "popup-notify"))
	{
		char *tmp;
		const char *name_shown;
		const char *reason;
		reason = gaim_pounce_action_get_attribute(pounce, "popup-notify",
														  "reason");

		/*
		 * Here we place the protocol name in the pounce dialog to lessen
		 * confusion about what protocol a pounce is for.
		 */
		tmp = g_strdup_printf(
				   (events & GAIM_POUNCE_TYPING) ?
				   _("%s has started typing to you (%s)") :
				   (events & GAIM_POUNCE_TYPED) ?
				   _("%s has paused while typing to you (%s)") :
				   (events & GAIM_POUNCE_SIGNON) ?
				   _("%s has signed on (%s)") :
				   (events & GAIM_POUNCE_IDLE_RETURN) ?
				   _("%s has returned from being idle (%s)") :
				   (events & GAIM_POUNCE_AWAY_RETURN) ?
				   _("%s has returned from being away (%s)") :
				   (events & GAIM_POUNCE_TYPING_STOPPED) ?
				   _("%s has stopped typing to you (%s)") :
				   (events & GAIM_POUNCE_SIGNOFF) ?
				   _("%s has signed off (%s)") :
				   (events & GAIM_POUNCE_IDLE) ?
				   _("%s has become idle (%s)") :
				   (events & GAIM_POUNCE_AWAY) ?
				   _("%s has gone away. (%s)") :
				   (events & GAIM_POUNCE_MESSAGE_RECEIVED) ?
				   _("%s has sent you a message. (%s)") :
				   _("Unknown pounce event. Please report this!"),
				   alias, gaim_account_get_protocol_name(account));

		/*
		 * Ok here is where I change the second argument, title, from
		 * NULL to the account alias if we have it or the account
		 * name if that's all we have
		 */
		if ((name_shown = gaim_account_get_alias(account)) == NULL)
			name_shown = gaim_account_get_username(account);

		if (reason == NULL)
		{
			gaim_notify_info(NULL, name_shown, tmp, gaim_date_format_full(NULL));
		}
		else
		{
			char *tmp2 = g_strdup_printf("%s\n\n%s", reason, gaim_date_format_full(NULL));
			gaim_notify_info(NULL, name_shown, tmp, tmp2);
			g_free(tmp2);
		}
		g_free(tmp);
	}

	if (gaim_pounce_action_is_enabled(pounce, "send-message"))
	{
		const char *message;

		message = gaim_pounce_action_get_attribute(pounce, "send-message",
												   "message");

		if (message != NULL)
		{
			conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, pouncee, account);

			if (conv == NULL)
				conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, pouncee);

			gaim_conversation_write(conv, NULL, message,
									GAIM_MESSAGE_SEND, time(NULL));

			serv_send_im(account->gc, (char *)pouncee, (char *)message, 0);
		}
	}

	if (gaim_pounce_action_is_enabled(pounce, "execute-command"))
	{
		const char *command;

		command = gaim_pounce_action_get_attribute(pounce,
				"execute-command", "command");

		if (command != NULL)
		{
#ifndef _WIN32
			char *localecmd = g_locale_from_utf8(command, -1, NULL,
					NULL, NULL);

			if (localecmd != NULL)
			{
				int pid = fork();

				if (pid == 0) {
					char *args[4];

					args[0] = "sh";
					args[1] = "-c";
					args[2] = (char *)localecmd;
					args[3] = NULL;

					execvp(args[0], args);

					_exit(0);
				}
				g_free(localecmd);
			}
#else /* !_WIN32 */
			PROCESS_INFORMATION pi;
			BOOL retval;
			gchar *message = NULL;

			memset(&pi, 0, sizeof(pi));

			if (G_WIN32_HAVE_WIDECHAR_API ()) {
				STARTUPINFOW si;
				wchar_t *wc_cmd = g_utf8_to_utf16(command,
						-1, NULL, NULL, NULL);

				memset(&si, 0 , sizeof(si));
				si.cb = sizeof(si);

				retval = CreateProcessW(NULL, wc_cmd, NULL,
						NULL, 0, 0, NULL, NULL,
						&si, &pi);
				g_free(wc_cmd);
			} else {
				STARTUPINFOA si;
				char *l_cmd = g_locale_from_utf8(command,
						-1, NULL, NULL, NULL);

				memset(&si, 0 , sizeof(si));
				si.cb = sizeof(si);

				retval = CreateProcessA(NULL, l_cmd, NULL,
						NULL, 0, 0, NULL, NULL,
						&si, &pi);
				g_free(l_cmd);
			}

			if (retval) {
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			} else {
				message = g_win32_error_message(GetLastError());
			}

			gaim_debug_info("pounce",
					"Pounce execute command called for: "
					"%s\n%s%s%s",
						command,
						retval ? "" : "Error: ",
						retval ? "" : message,
						retval ? "" : "\n");
			g_free(message);
#endif /* !_WIN32 */
		}
	}

	if (gaim_pounce_action_is_enabled(pounce, "play-sound"))
	{
		const char *sound;

		sound = gaim_pounce_action_get_attribute(pounce,
												 "play-sound", "filename");

		if (sound != NULL)
			gaim_sound_play_file(sound, account);
		else
			gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT, account);
	}
}

static void
free_pounce(GaimPounce *pounce)
{
	update_pounces();
}

static void
new_pounce(GaimPounce *pounce)
{
	gaim_pounce_action_register(pounce, "open-window");
	gaim_pounce_action_register(pounce, "popup-notify");
	gaim_pounce_action_register(pounce, "send-message");
	gaim_pounce_action_register(pounce, "execute-command");
	gaim_pounce_action_register(pounce, "play-sound");

	update_pounces();
}

void *
pidgin_pounces_get_handle() {
	static int handle;

	return &handle;
}

void
pidgin_pounces_init(void)
{
	gaim_pounces_register_handler(PIDGIN_UI, pounce_cb, new_pounce,
								  free_pounce);

	gaim_prefs_add_none("/gaim/gtk/pounces");
	gaim_prefs_add_none("/gaim/gtk/pounces/default_actions");
	gaim_prefs_add_bool("/gaim/gtk/pounces/default_actions/open-window",
						FALSE);
	gaim_prefs_add_bool("/gaim/gtk/pounces/default_actions/popup-notify",
						TRUE);
	gaim_prefs_add_bool("/gaim/gtk/pounces/default_actions/send-message",
						FALSE);
	gaim_prefs_add_bool("/gaim/gtk/pounces/default_actions/execute-command",
						FALSE);
	gaim_prefs_add_bool("/gaim/gtk/pounces/default_actions/play-sound",
						FALSE);
	gaim_prefs_add_none("/gaim/gtk/pounces/dialog");
	gaim_prefs_add_int("/gaim/gtk/pounces/dialog/width",  520);
	gaim_prefs_add_int("/gaim/gtk/pounces/dialog/height", 321);

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						pidgin_pounces_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						pidgin_pounces_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
}

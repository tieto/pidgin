/**
 * @file gntpounce.c GNT Buddy Pounce API
 * @ingroup gntui
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
#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>
#include "internal.h"
#include "gntgaim.h"

#include "account.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "util.h"

#include "gntpounce.h"


typedef struct
{
	/* Pounce data */
	GaimPounce  *pounce;
	GaimAccount *account;

	/* The window */
	GntWidget *window;

	/* Pounce on Whom */
	GntWidget *account_menu;
	GntWidget *buddy_entry;

	/* Pounce options */
	GntWidget *on_away;

	/* Pounce When Buddy... */
	GntWidget *signon;
	GntWidget *signoff;
	GntWidget *away;
	GntWidget *away_return;
	GntWidget *idle;
	GntWidget *idle_return;
	GntWidget *typing;
	GntWidget *typed;
	GntWidget *stop_typing;
	GntWidget *message_recv;

	/* Action */
	GntWidget *open_win;
	GntWidget *popup;
	GntWidget *popup_entry;
	GntWidget *send_msg;
	GntWidget *send_msg_entry;
	GntWidget *exec_cmd;
	GntWidget *exec_cmd_entry;
	GntWidget *play_sound;

	GntWidget *save_pounce;

	/* Buttons */
	GntWidget *save_button;

} GaimGntPounceDialog;

typedef struct
{
	GntWidget *window;
	GntWidget *tree;
	GntWidget *modify_button;
	GntWidget *delete_button;
} PouncesManager;

static PouncesManager *pounces_manager = NULL;

/**************************************************************************
 * Callbacks
 **************************************************************************/
static gint
delete_win_cb(GntWidget *w, GaimGntPounceDialog *dialog)
{
	gnt_widget_destroy(dialog->window);
	g_free(dialog);

	return TRUE;
}

static void
cancel_cb(GntWidget *w, GaimGntPounceDialog *dialog)
{
	gnt_widget_destroy(dialog->window);
}

static void
add_pounce_to_treeview(GntTree *tree, GaimPounce *pounce)
{
	GaimAccount *account;
	const char *pouncer;
	const char *pouncee;

	account = gaim_pounce_get_pouncer(pounce);
	pouncer = gaim_account_get_username(account);
	pouncee = gaim_pounce_get_pouncee(pounce);
	gnt_tree_add_row_last(tree, pounce,
		gnt_tree_create_row(tree, pouncer, pouncee), NULL);
}

static void
populate_pounces_list(PouncesManager *dialog)
{
	const GList *pounces;

	gnt_tree_remove_all(GNT_TREE(dialog->tree));

	for (pounces = gaim_pounces_get_all(); pounces != NULL;
			pounces = g_list_next(pounces))
	{
		add_pounce_to_treeview(GNT_TREE(dialog->tree), pounces->data);
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
save_pounce_cb(GntWidget *w, GaimGntPounceDialog *dialog)
{
	const char *name;
	const char *message, *command, *reason;
	GaimPounceEvent events   = GAIM_POUNCE_NONE;
	GaimPounceOption options = GAIM_POUNCE_OPTION_NONE;

	name = gnt_entry_get_text(GNT_ENTRY(dialog->buddy_entry));

	if (*name == '\0')
	{
		gaim_notify_error(NULL, NULL,
						  _("Please enter a buddy to pounce."), NULL);
		return;
	}

	/* Options */
	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->on_away)))
		options |= GAIM_POUNCE_OPTION_AWAY;

	/* Events */
	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->signon)))
		events |= GAIM_POUNCE_SIGNON;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->signoff)))
		events |= GAIM_POUNCE_SIGNOFF;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->away)))
		events |= GAIM_POUNCE_AWAY;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->away_return)))
		events |= GAIM_POUNCE_AWAY_RETURN;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->idle)))
		events |= GAIM_POUNCE_IDLE;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->idle_return)))
		events |= GAIM_POUNCE_IDLE_RETURN;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->typing)))
		events |= GAIM_POUNCE_TYPING;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->typed)))
		events |= GAIM_POUNCE_TYPED;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->stop_typing)))
		events |= GAIM_POUNCE_TYPING_STOPPED;

	if (gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->message_recv)))
		events |= GAIM_POUNCE_MESSAGE_RECEIVED;

	/* Data fields */
	message = gnt_entry_get_text(GNT_ENTRY(dialog->send_msg_entry));
	command = gnt_entry_get_text(GNT_ENTRY(dialog->exec_cmd_entry));
	reason  = gnt_entry_get_text(GNT_ENTRY(dialog->popup_entry));

	if (*reason == '\0') reason = NULL;
	if (*message == '\0') message = NULL;
	if (*command == '\0') command = NULL;

	if (dialog->pounce == NULL) {
		dialog->pounce = gaim_pounce_new(GAIM_GNT_UI, dialog->account,
										 name, events, options);
	} else {
		gaim_pounce_set_events(dialog->pounce, events);
		gaim_pounce_set_options(dialog->pounce, options);
		gaim_pounce_set_pouncer(dialog->pounce, dialog->account);
		gaim_pounce_set_pouncee(dialog->pounce, name);
	}

	/* Actions */
	gaim_pounce_action_set_enabled(dialog->pounce, "open-window",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->open_win)));
	gaim_pounce_action_set_enabled(dialog->pounce, "popup-notify",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->popup)));
	gaim_pounce_action_set_enabled(dialog->pounce, "send-message",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->send_msg)));
	gaim_pounce_action_set_enabled(dialog->pounce, "execute-command",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->exec_cmd)));
	gaim_pounce_action_set_enabled(dialog->pounce, "play-beep",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->play_sound)));

	gaim_pounce_action_set_attribute(dialog->pounce, "send-message",
									 "message", message);
	gaim_pounce_action_set_attribute(dialog->pounce, "execute-command",
									 "command", command);
	gaim_pounce_action_set_attribute(dialog->pounce, "popup-notify",
									 "reason", reason);

	/* Set the defaults for next time. */
	gaim_prefs_set_bool("/gaim/gnt/pounces/default_actions/open-window",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->open_win)));
	gaim_prefs_set_bool("/gaim/gnt/pounces/default_actions/popup-notify",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->popup)));
	gaim_prefs_set_bool("/gaim/gnt/pounces/default_actions/send-message",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->send_msg)));
	gaim_prefs_set_bool("/gaim/gnt/pounces/default_actions/execute-command",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->exec_cmd)));
	gaim_prefs_set_bool("/gaim/gnt/pounces/default_actions/play-beep",
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->play_sound)));

	gaim_pounce_set_save(dialog->pounce,
		gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->save_pounce)));

	gaim_pounce_set_pouncer(dialog->pounce,
		(GaimAccount *)gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->account_menu)));

	update_pounces();

	gnt_widget_destroy(dialog->window);
}


void
gg_pounce_editor_show(GaimAccount *account, const char *name,
							GaimPounce *cur_pounce)
{
	GaimGntPounceDialog *dialog;
	GntWidget *window;
	GntWidget *bbox;
	GntWidget *hbox;
	GntWidget *button;
	GntWidget *combo;
	GList *list;

	g_return_if_fail((cur_pounce != NULL) ||
	                 (account != NULL) ||
	                 (gaim_accounts_get_all() != NULL));

	dialog = g_new0(GaimGntPounceDialog, 1);

	if (cur_pounce != NULL) {
		dialog->pounce  = cur_pounce;
		dialog->account = gaim_pounce_get_pouncer(cur_pounce);
	} else if (account != NULL) {
		dialog->pounce  = NULL;
		dialog->account = account;
	} else {
		GList *connections = gaim_connections_get_all();
		GaimConnection *gc;

		if (connections != NULL) {
			gc = (GaimConnection *)connections->data;
			dialog->account = gaim_connection_get_account(gc);
		} else
			dialog->account = gaim_accounts_get_all()->data;

		dialog->pounce  = NULL;
	}

	/* Create the window. */
	dialog->window = window = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_LEFT);
	gnt_box_set_title(GNT_BOX(window),
						 (cur_pounce == NULL
						  ? _("New Buddy Pounce") : _("Edit Buddy Pounce")));

	g_signal_connect(G_OBJECT(window), "destroy",
					 G_CALLBACK(delete_win_cb), dialog);

	gnt_box_add_widget(GNT_BOX(window), gnt_label_new_with_format(_("Pounce Who"), GNT_TEXT_FLAG_BOLD));

	/* Account: */
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new(_("Account:")));
	dialog->account_menu = combo = gnt_combo_box_new();
	list = gaim_accounts_get_all();
	for (; list; list = list->next)
	{
		GaimAccount *account;
		char *text;

		account = list->data;
		text = g_strdup_printf("%s (%s)",
				gaim_account_get_username(account),
				gaim_account_get_protocol_name(account));
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), account, text);
		g_free(text);
	}
	if (dialog->account)
		gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), dialog->account);

	gnt_box_add_widget(GNT_BOX(window), combo);

	/* Buddy: */
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Buddy name:")));

	dialog->buddy_entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->buddy_entry);

	gnt_box_add_widget(GNT_BOX(window), hbox);

	if (cur_pounce != NULL) {
		gnt_entry_set_text(GNT_ENTRY(dialog->buddy_entry),
						   gaim_pounce_get_pouncee(cur_pounce));
	} else if (name != NULL) {
		gnt_entry_set_text(GNT_ENTRY(dialog->buddy_entry), name);
	}

	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new_with_format(_("Pounce When Buddy..."), GNT_TEXT_FLAG_BOLD));

	dialog->signon = gnt_check_box_new(_("Signs on"));
	dialog->signoff = gnt_check_box_new(_("Signs off"));
	dialog->away = gnt_check_box_new(_("Goes away"));
	dialog->away_return = gnt_check_box_new(_("Returns from away"));
	dialog->idle = gnt_check_box_new(_("Becomes idle"));
	dialog->idle_return = gnt_check_box_new(_("Is no longer idle"));
	dialog->typing = gnt_check_box_new(_("Starts typing"));
	dialog->typed = gnt_check_box_new(_("Pauses while typing"));
	dialog->stop_typing = gnt_check_box_new(_("Stops typing"));
	dialog->message_recv = gnt_check_box_new(_("Sends a message"));

	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 2);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->signon);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->signoff);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 2);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->away);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->away_return);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 2);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->idle);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->idle_return);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 2);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->typing);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->typed);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(hbox), 2);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->stop_typing);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->message_recv);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	/* Create the "Action" frame. */
	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new_with_format(_("Action"), GNT_TEXT_FLAG_BOLD));

	dialog->open_win = gnt_check_box_new(_("Open an IM window"));
	dialog->popup = gnt_check_box_new(_("Pop up a notification"));
	dialog->send_msg = gnt_check_box_new(_("Send a message"));
	dialog->exec_cmd = gnt_check_box_new(_("Execute a command"));
	dialog->play_sound = gnt_check_box_new(_("Play a sound"));

	dialog->send_msg_entry    = gnt_entry_new(NULL);
	dialog->exec_cmd_entry    = gnt_entry_new(NULL);
	dialog->popup_entry       = gnt_entry_new(NULL);
	dialog->exec_cmd_entry   = gnt_entry_new(NULL);

	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->open_win);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->popup);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->popup_entry);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->send_msg);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->send_msg_entry);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->exec_cmd);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->exec_cmd_entry);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(hbox), dialog->play_sound);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));
	gnt_box_add_widget(GNT_BOX(window), gnt_label_new_with_format(_("Options"), GNT_TEXT_FLAG_BOLD));
	dialog->on_away = gnt_check_box_new(_("Pounce only when my status is not available"));
	gnt_box_add_widget(GNT_BOX(window), dialog->on_away);
	dialog->save_pounce = gnt_check_box_new(_("Recurring"));
	gnt_box_add_widget(GNT_BOX(window), dialog->save_pounce);


	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));
	/* Now the button box! */
	bbox = gnt_hbox_new(TRUE);

	/* Cancel button */
	button = gnt_button_new(_("Cancel"));
	gnt_box_add_widget(GNT_BOX(bbox), button);
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(cancel_cb), dialog);

	/* Save button */
	dialog->save_button = button = gnt_button_new(_("Save"));
	gnt_box_add_widget(GNT_BOX(bbox), button);
	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(save_pounce_cb), dialog);

	gnt_box_add_widget(GNT_BOX(window), bbox);


	/* Set the values of stuff. */
	if (cur_pounce != NULL)
	{
		GaimPounceEvent events   = gaim_pounce_get_events(cur_pounce);
		GaimPounceOption options = gaim_pounce_get_options(cur_pounce);
		const char *value;

		/* Options */
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->on_away),
									(options & GAIM_POUNCE_OPTION_AWAY));

		/* Events */
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->signon),
									(events & GAIM_POUNCE_SIGNON));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->signoff),
									(events & GAIM_POUNCE_SIGNOFF));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->away),
									(events & GAIM_POUNCE_AWAY));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->away_return),
									(events & GAIM_POUNCE_AWAY_RETURN));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->idle),
									(events & GAIM_POUNCE_IDLE));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->idle_return),
									(events & GAIM_POUNCE_IDLE_RETURN));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->typing),
									(events & GAIM_POUNCE_TYPING));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->typed),
									(events & GAIM_POUNCE_TYPED));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->stop_typing),
									(events & GAIM_POUNCE_TYPING_STOPPED));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->message_recv),
									(events & GAIM_POUNCE_MESSAGE_RECEIVED));

		/* Actions */
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->open_win),
			gaim_pounce_action_is_enabled(cur_pounce, "open-window"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->popup),
			gaim_pounce_action_is_enabled(cur_pounce, "popup-notify"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->send_msg),
			gaim_pounce_action_is_enabled(cur_pounce, "send-message"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->exec_cmd),
			gaim_pounce_action_is_enabled(cur_pounce, "execute-command"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->play_sound),
			gaim_pounce_action_is_enabled(cur_pounce, "play-beep"));

		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->save_pounce),
			gaim_pounce_get_save(cur_pounce));

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
		                                              "send-message",
		                                              "message")) != NULL)
		{
			gnt_entry_set_text(GNT_ENTRY(dialog->send_msg_entry), value);
		}

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
		                                              "popup-notify",
		                                              "reason")) != NULL)
		{
			gnt_entry_set_text(GNT_ENTRY(dialog->popup_entry), value);
		}

		if ((value = gaim_pounce_action_get_attribute(cur_pounce,
		                                              "execute-command",
		                                              "command")) != NULL)
		{
			gnt_entry_set_text(GNT_ENTRY(dialog->exec_cmd_entry), value);
		}
	}
	else
	{
		GaimBuddy *buddy = NULL;

		if (name != NULL)
			buddy = gaim_find_buddy(account, name);

		/* Set some defaults */
		if (buddy == NULL) {
			gnt_check_box_set_checked(
				GNT_CHECK_BOX(dialog->signon), TRUE);
		} else {
			if (!GAIM_BUDDY_IS_ONLINE(buddy)) {
				gnt_check_box_set_checked(
					GNT_CHECK_BOX(dialog->signon), TRUE);
			} else {
				gboolean default_set = FALSE;
				GaimPresence *presence = gaim_buddy_get_presence(buddy);

				if (gaim_presence_is_idle(presence))
				{
					gnt_check_box_set_checked(
						GNT_CHECK_BOX(dialog->idle_return), TRUE);

					default_set = TRUE;
				}

				if (!gaim_presence_is_available(presence))
				{
					gnt_check_box_set_checked(
						GNT_CHECK_BOX(dialog->away_return), TRUE);

					default_set = TRUE;
				}

				if (!default_set)
				{
					gnt_check_box_set_checked(
						GNT_CHECK_BOX(dialog->signon), TRUE);
				}
			}
		}

		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->open_win),
			gaim_prefs_get_bool("/gaim/gnt/pounces/default_actions/open-window"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->popup),
			gaim_prefs_get_bool("/gaim/gnt/pounces/default_actions/popup-notify"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->send_msg),
			gaim_prefs_get_bool("/gaim/gnt/pounces/default_actions/send-message"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->exec_cmd),
			gaim_prefs_get_bool("/gaim/gnt/pounces/default_actions/execute-command"));
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->play_sound),
			gaim_prefs_get_bool("/gaim/gnt/pounces/default_actions/play-beep"));
	}

	gnt_widget_show(window);
}



static gboolean
pounces_manager_destroy_cb(GntWidget *widget, gpointer user_data)
{
	PouncesManager *dialog = user_data;

	dialog->window = NULL;
	gg_pounces_manager_hide();

	return FALSE;
}


static void
pounces_manager_add_cb(GntButton *button, gpointer user_data)
{
	gg_pounce_editor_show(NULL, NULL, NULL);
}


static void
pounces_manager_modify_cb(GntButton *button, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GaimPounce *pounce = gnt_tree_get_selection_data(GNT_TREE(dialog->tree));
	gg_pounce_editor_show(NULL, NULL, pounce);
}

static void
pounces_manager_delete_confirm_cb(GaimPounce *pounce)
{
	gnt_tree_remove(GNT_TREE(pounces_manager->tree), pounce);

	gaim_request_close_with_handle(pounce);
	gaim_pounce_destroy(pounce);
}


static void
pounces_manager_delete_cb(GntButton *button, gpointer user_data)
{
	PouncesManager *dialog = user_data;
	GaimPounce *pounce;
	GaimAccount *account;
	const char *pouncer, *pouncee;
	char *buf;

	pounce = (GaimPounce *)gnt_tree_get_selection_data(GNT_TREE(dialog->tree));
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
pounces_manager_close_cb(GntButton *button, gpointer user_data)
{
	gg_pounces_manager_hide();
}


void
gg_pounces_manager_show(void)
{
	PouncesManager *dialog;
	GntWidget *bbox;
	GntWidget *button;
	GntWidget *tree;
	GntWidget *win;

	if (pounces_manager != NULL) {
		return;
	}

	pounces_manager = dialog = g_new0(PouncesManager, 1);

	dialog->window = win = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_title(GNT_BOX(win), _("Buddy Pounces"));
	gnt_box_set_pad(GNT_BOX(win), 0);

	g_signal_connect(G_OBJECT(win), "destroy",
					 G_CALLBACK(pounces_manager_destroy_cb), dialog);

	/* List of saved buddy pounces */
	dialog->tree = tree = GNT_WIDGET(gnt_tree_new_with_columns(2));
	gnt_tree_set_column_titles(GNT_TREE(tree), "Account", "Pouncee", NULL);
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);

	gnt_box_add_widget(GNT_BOX(win), tree);

	/* Button box. */
	bbox = gnt_hbox_new(TRUE);

	/* Add button */
	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(bbox), button);

	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(pounces_manager_add_cb), dialog);

	/* Modify button */
	button = gnt_button_new(_("Modify"));
	dialog->modify_button = button;
	gnt_box_add_widget(GNT_BOX(bbox), button);

	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(pounces_manager_modify_cb), dialog);

	/* Delete button */
	button = gnt_button_new(_("Delete"));
	dialog->delete_button = button;
	gnt_box_add_widget(GNT_BOX(bbox), button);

	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(pounces_manager_delete_cb), dialog);

	/* Close button */
	button = gnt_button_new(_("Close"));
	gnt_box_add_widget(GNT_BOX(bbox), button);
	gnt_widget_show(button);

	g_signal_connect(G_OBJECT(button), "activate",
					 G_CALLBACK(pounces_manager_close_cb), dialog);

	gnt_box_add_widget(GNT_BOX(win), bbox);

	gnt_widget_show(win);
	populate_pounces_list(pounces_manager);
}

void
gg_pounces_manager_hide(void)
{
	if (pounces_manager == NULL)
		return;

	if (pounces_manager->window != NULL)
		gnt_widget_destroy(pounces_manager->window);

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
		}
	}

	if (gaim_pounce_action_is_enabled(pounce, "play-beep"))
	{
		beep();
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
	gaim_pounce_action_register(pounce, "play-beep");

	update_pounces();
}

void *
gg_pounces_get_handle()
{
	static int handle;

	return &handle;
}

void
gg_pounces_init(void)
{
	gaim_pounces_register_handler(GAIM_GNT_UI, pounce_cb, new_pounce,
								  free_pounce);

	gaim_prefs_add_none("/gaim/gnt/pounces");
	gaim_prefs_add_none("/gaim/gnt/pounces/default_actions");
	gaim_prefs_add_bool("/gaim/gnt/pounces/default_actions/open-window",
						FALSE);
	gaim_prefs_add_bool("/gaim/gnt/pounces/default_actions/popup-notify",
						TRUE);
	gaim_prefs_add_bool("/gaim/gnt/pounces/default_actions/send-message",
						FALSE);
	gaim_prefs_add_bool("/gaim/gnt/pounces/default_actions/execute-command",
						FALSE);
	gaim_prefs_add_bool("/gaim/gnt/pounces/default_actions/play-beep",
						FALSE);
	gaim_prefs_add_none("/gaim/gnt/pounces/dialog");

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						gg_pounces_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						gg_pounces_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
}

/* XXX: There's no such thing in pidgin. Perhaps there should be? */
void gg_pounces_uninit()
{
	gaim_pounces_register_handler(GAIM_GNT_UI, NULL, NULL, NULL);

	gaim_signals_disconnect_by_handle(gg_pounces_get_handle());
}


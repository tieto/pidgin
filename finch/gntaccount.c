/**
 * @file gntaccount.c GNT Account API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
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
#include <gntutils.h>
#include <gntwindow.h>

#include "finch.h"

#include <account.h>
#include <accountopt.h>
#include <connection.h>
#include <notify.h>
#include <plugin.h>
#include <request.h>
#include <savedstatuses.h>

#include "gntaccount.h"
#include "gntblist.h"

#include <string.h>

typedef struct
{
	GntWidget *window;
	GntWidget *tree;
} FinchAccountList;

static FinchAccountList accounts;

typedef struct
{
	PurpleAccount *account;          /* NULL for a new account */

	GntWidget *window;

	GntWidget *protocol;
	GntWidget *screenname;
	GntWidget *password;
	GntWidget *alias;

	GntWidget *splits;
	GList *split_entries;

	GList *prpl_entries;
	GntWidget *prpls;

	GntWidget *newmail;
	GntWidget *remember;
	GntWidget *regserver;
} AccountEditDialog;

/* This is necessary to close an edit-dialog when an account is deleted */
static GList *accountdialogs;

static void
account_add(PurpleAccount *account)
{
	gnt_tree_add_choice(GNT_TREE(accounts.tree), account,
			gnt_tree_create_row(GNT_TREE(accounts.tree),
				purple_account_get_username(account),
				purple_account_get_protocol_name(account)),
			NULL, NULL);
	gnt_tree_set_choice(GNT_TREE(accounts.tree), account,
			purple_account_get_enabled(account, FINCH_UI));
}

static void
edit_dialog_destroy(AccountEditDialog *dialog)
{
	accountdialogs = g_list_remove(accountdialogs, dialog);
	g_list_free(dialog->prpl_entries);
	g_list_free(dialog->split_entries);
	g_free(dialog);
}

static void
save_account_cb(AccountEditDialog *dialog)
{
	PurpleAccount *account;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prplinfo;
	const char *value;
	GString *username;

	/* XXX: Do some error checking first. */

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	/* Screenname && user-splits */
	value = gnt_entry_get_text(GNT_ENTRY(dialog->screenname));

	if (value == NULL || *value == '\0')
	{
		purple_notify_error(NULL, _("Error"), _("Account was not added"),
				_("Username of an account must be non-empty."));
		return;
	}

	username = g_string_new(value);

	if (prplinfo != NULL)
	{
		GList *iter, *entries;
		for (iter = prplinfo->user_splits, entries = dialog->split_entries;
				iter && entries; iter = iter->next, entries = entries->next)
		{
			PurpleAccountUserSplit *split = iter->data;
			GntWidget *entry = entries->data;

			value = gnt_entry_get_text(GNT_ENTRY(entry));
			if (value == NULL || *value == '\0')
				value = purple_account_user_split_get_default_value(split);
			g_string_append_printf(username, "%c%s",
					purple_account_user_split_get_separator(split),
					value);
		}
	}

	if (dialog->account == NULL)
	{
		account = purple_account_new(username->str, purple_plugin_get_id(plugin));
		purple_accounts_add(account);
	}
	else
	{
		account = dialog->account;

		/* Protocol */
		purple_account_set_protocol_id(account, purple_plugin_get_id(plugin));
		purple_account_set_username(account, username->str);
	}
	g_string_free(username, TRUE);

	/* Alias */
	value = gnt_entry_get_text(GNT_ENTRY(dialog->alias));
	if (value && *value)
		purple_account_set_alias(account, value);

	/* Remember password and password */
	purple_account_set_remember_password(account,
			gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->remember)));
	value = gnt_entry_get_text(GNT_ENTRY(dialog->password));
	if (value && *value)
		purple_account_set_password(account, value);
	else
		purple_account_set_password(account, NULL);

	/* Mail notification */
	purple_account_set_check_mail(account,
			gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->newmail)));

	/* Protocol options */
	if (prplinfo)
	{
		GList *iter, *entries;

		for (iter = prplinfo->protocol_options, entries = dialog->prpl_entries;
				iter && entries; iter = iter->next, entries = entries->next)
		{
			PurpleAccountOption *option = iter->data;
			GntWidget *entry = entries->data;
			PurplePrefType type = purple_account_option_get_type(option);
			const char *setting = purple_account_option_get_setting(option);

			if (type == PURPLE_PREF_STRING)
			{
				const char *value = gnt_entry_get_text(GNT_ENTRY(entry));
				purple_account_set_string(account, setting, value);
			}
			else if (type == PURPLE_PREF_INT)
			{
				const char *str = gnt_entry_get_text(GNT_ENTRY(entry));
				int value = 0;
				if (str)
					value = atoi(str);
				purple_account_set_int(account, setting, value);
			}
			else if (type == PURPLE_PREF_BOOLEAN)
			{
				gboolean value = gnt_check_box_get_checked(GNT_CHECK_BOX(entry));
				purple_account_set_bool(account, setting, value);
			}
			else if (type == PURPLE_PREF_STRING_LIST)
			{
				/* TODO: */
			}
			else
			{
				g_assert_not_reached();
			}
		}
	}

	/* XXX: Proxy options */

	if (accounts.window && accounts.tree) {
		gnt_tree_set_selected(GNT_TREE(accounts.tree), account);
		gnt_box_give_focus_to_child(GNT_BOX(accounts.window), accounts.tree);
	}

	if (prplinfo && prplinfo->register_user &&
			gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->regserver))) {
		purple_account_register(account);
	} else if (dialog->account == NULL) {
		/* This is a new account. Set it to the current status. */
		/* Xerox from gtkaccount.c :D */
		const PurpleSavedStatus *saved_status;
		saved_status = purple_savedstatus_get_current();
		if (saved_status != NULL) {
			purple_savedstatus_activate_for_account(saved_status, account);
			purple_account_set_enabled(account, FINCH_UI, TRUE);
		}
	}

	gnt_widget_destroy(dialog->window);
}

static void
update_user_splits(AccountEditDialog *dialog)
{
	GntWidget *hbox;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prplinfo;
	GList *iter, *entries;
	char *username = NULL;

	if (dialog->splits)
	{
		gnt_box_remove_all(GNT_BOX(dialog->splits));
		g_list_free(dialog->split_entries);
	}
	else
	{
		dialog->splits = gnt_vbox_new(FALSE);
		gnt_box_set_pad(GNT_BOX(dialog->splits), 0);
		gnt_box_set_fill(GNT_BOX(dialog->splits), TRUE);
	}

	dialog->split_entries = NULL;

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	if (!plugin)
		return;
	prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);
	
	username = dialog->account ? g_strdup(purple_account_get_username(dialog->account)) : NULL;

	for (iter = prplinfo->user_splits; iter; iter = iter->next)
	{
		PurpleAccountUserSplit *split = iter->data;
		GntWidget *entry;
		char *buf;

		hbox = gnt_hbox_new(TRUE);
		gnt_box_add_widget(GNT_BOX(dialog->splits), hbox);

		buf = g_strdup_printf("%s:", purple_account_user_split_get_text(split));
		gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(buf));

		entry = gnt_entry_new(NULL);
		gnt_box_add_widget(GNT_BOX(hbox), entry);

		dialog->split_entries = g_list_append(dialog->split_entries, entry);
		g_free(buf);
	}

	for (iter = g_list_last(prplinfo->user_splits), entries = g_list_last(dialog->split_entries);
			iter && entries; iter = iter->prev, entries = entries->prev)
	{
		GntWidget *entry = entries->data;
		PurpleAccountUserSplit *split = iter->data;
		const char *value = NULL;
		char *s;

		if (dialog->account)
		{
			if(purple_account_user_split_get_reverse(split))
				s = strrchr(username, purple_account_user_split_get_separator(split));
			else
				s = strchr(username, purple_account_user_split_get_separator(split));

			if (s != NULL)
			{
				*s = '\0';
				s++;
				value = s;
			}
		}
		if (value == NULL)
			value = purple_account_user_split_get_default_value(split);

		if (value != NULL)
			gnt_entry_set_text(GNT_ENTRY(entry), value);
	}

	if (username != NULL)
		gnt_entry_set_text(GNT_ENTRY(dialog->screenname), username);

	g_free(username);
}

static void
add_protocol_options(AccountEditDialog *dialog)
{
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prplinfo;
	GList *iter;
	GntWidget *vbox, *box;
	PurpleAccount *account;

	if (dialog->prpls)
		gnt_box_remove_all(GNT_BOX(dialog->prpls));
	else
	{
		dialog->prpls = vbox = gnt_vbox_new(FALSE);
		gnt_box_set_pad(GNT_BOX(vbox), 0);
		gnt_box_set_alignment(GNT_BOX(vbox), GNT_ALIGN_LEFT);
		gnt_box_set_fill(GNT_BOX(vbox), TRUE);
	}

	if (dialog->prpl_entries)
	{
		g_list_free(dialog->prpl_entries);
		dialog->prpl_entries = NULL;
	}

	vbox = dialog->prpls;

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	if (!plugin)
		return;

	prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	account = dialog->account;

	for (iter = prplinfo->protocol_options; iter; iter = iter->next)
	{
		PurpleAccountOption *option = iter->data;
		PurplePrefType type = purple_account_option_get_type(option);

		box = gnt_hbox_new(TRUE);
		gnt_box_set_pad(GNT_BOX(box), 0);
		gnt_box_add_widget(GNT_BOX(vbox), box);

		if (type == PURPLE_PREF_BOOLEAN)
		{
			GntWidget *widget = gnt_check_box_new(purple_account_option_get_text(option));
			gnt_box_add_widget(GNT_BOX(box), widget);
			dialog->prpl_entries = g_list_append(dialog->prpl_entries, widget);

			if (account)
				gnt_check_box_set_checked(GNT_CHECK_BOX(widget),
						purple_account_get_bool(account,
							purple_account_option_get_setting(option),
							purple_account_option_get_default_bool(option)));
			else
				gnt_check_box_set_checked(GNT_CHECK_BOX(widget),
						purple_account_option_get_default_bool(option));
		}
		else
		{
			gnt_box_add_widget(GNT_BOX(box),
					gnt_label_new(purple_account_option_get_text(option)));

			if (type == PURPLE_PREF_STRING_LIST)
			{
				/* TODO: Use a combobox */
				/*       Don't forget to append the widget to prpl_entries */
			}
			else
			{
				GntWidget *entry = gnt_entry_new(NULL);
				gnt_box_add_widget(GNT_BOX(box), entry);
				dialog->prpl_entries = g_list_append(dialog->prpl_entries, entry);

				if (type == PURPLE_PREF_STRING)
				{
					const char *dv = purple_account_option_get_default_string(option);

					if (account)
						gnt_entry_set_text(GNT_ENTRY(entry),
								purple_account_get_string(account,
									purple_account_option_get_setting(option), dv));
					else
						gnt_entry_set_text(GNT_ENTRY(entry), dv);
				}
				else if (type == PURPLE_PREF_INT)
				{
					char str[32];
					int value = purple_account_option_get_default_int(option);
					if (account)
						value = purple_account_get_int(account,
								purple_account_option_get_setting(option), value);
					snprintf(str, sizeof(str), "%d", value);
					gnt_entry_set_flag(GNT_ENTRY(entry), GNT_ENTRY_FLAG_INT);
					gnt_entry_set_text(GNT_ENTRY(entry), str);
				}
				else
				{
					g_assert_not_reached();
				}
			}
		}
	}

	/* Show the registration checkbox only in a new account dialog,
	 * and when the selected prpl has the support for it. */
	gnt_widget_set_visible(dialog->regserver, account == NULL &&
			prplinfo->register_user != NULL);
}

static void
update_user_options(AccountEditDialog *dialog)
{
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prplinfo;

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	if (!plugin)
		return;

	prplinfo = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	if (dialog->newmail == NULL)
		dialog->newmail = gnt_check_box_new(_("New mail notifications"));
	if (dialog->account)
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->newmail),
				purple_account_get_check_mail(dialog->account));
	if (!prplinfo || !(prplinfo->options & OPT_PROTO_MAIL_CHECK))
		gnt_widget_set_visible(dialog->newmail, FALSE);
	else
		gnt_widget_set_visible(dialog->newmail, TRUE);

	if (dialog->remember == NULL)
		dialog->remember = gnt_check_box_new(_("Remember password"));
	if (dialog->account)
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->remember),
				purple_account_get_remember_password(dialog->account));
}

static void
prpl_changed_cb(GntWidget *combo, PurplePlugin *old, PurplePlugin *new, AccountEditDialog *dialog)
{
	update_user_splits(dialog);
	add_protocol_options(dialog);
	update_user_options(dialog);  /* This may not be necessary here */
	gnt_box_readjust(GNT_BOX(dialog->window));
	gnt_widget_draw(dialog->window);
}

static void
edit_account(PurpleAccount *account)
{
	GntWidget *window, *hbox;
	GntWidget *combo, *button, *entry;
	GList *list, *iter;
	AccountEditDialog *dialog;

	if (account)
	{
		GList *iter;
		for (iter = accountdialogs; iter; iter = iter->next)
		{
			AccountEditDialog *dlg = iter->data;
			if (dlg->account == account)
				return;
		}
	}

	list = purple_plugins_get_protocols();
	if (list == NULL) {
		purple_notify_error(NULL, _("Error"),
				_("There's no protocol plugins installed."),
				_("(You probably forgot to 'make install'.)"));
		return;
	}

	dialog = g_new0(AccountEditDialog, 1);
	accountdialogs = g_list_prepend(accountdialogs, dialog);

	dialog->window = window = gnt_vbox_new(FALSE);
	dialog->account = account;
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), account ? _("Modify Account") : _("New Account"));
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_widget_set_name(window, "edit-account");
	gnt_box_set_fill(GNT_BOX(window), TRUE);

	hbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->protocol = combo = gnt_combo_box_new();
	for (iter = list; iter; iter = iter->next)
	{
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), iter->data,
				((PurplePlugin*)iter->data)->info->name);
	}

	if (account)
		gnt_combo_box_set_selected(GNT_COMBO_BOX(combo),
				purple_plugins_find_with_id(purple_account_get_protocol_id(account)));
	else
		gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), list->data);

	g_signal_connect(G_OBJECT(combo), "selection-changed", G_CALLBACK(prpl_changed_cb), dialog);

	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Protocol:")));
	gnt_box_add_widget(GNT_BOX(hbox), combo);

	hbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->screenname = entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Username:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);

	/* User splits */
	update_user_splits(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->splits);

	hbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->password = entry = gnt_entry_new(NULL);
	gnt_entry_set_masked(GNT_ENTRY(entry), TRUE);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Password:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	if (account)
		gnt_entry_set_text(GNT_ENTRY(entry), purple_account_get_password(account));

	hbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(hbox), 0);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->alias = entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Alias:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	if (account)
		gnt_entry_set_text(GNT_ENTRY(entry), purple_account_get_alias(account));

	/* User options */
	update_user_options(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->remember);
	gnt_box_add_widget(GNT_BOX(window), dialog->newmail);

	/* Register checkbox */
	dialog->regserver = gnt_check_box_new(_("Create this account on the server"));
	gnt_box_add_widget(GNT_BOX(window), dialog->regserver);

	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));

	/* The advanced box */
	add_protocol_options(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->prpls);

	/* TODO: Add proxy options */

	/* The button box */
	hbox = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);

	button = gnt_button_new(_("Cancel"));
	gnt_box_add_widget(GNT_BOX(hbox), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), window);
	
	button = gnt_button_new(_("Save"));
	gnt_box_add_widget(GNT_BOX(hbox), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(save_account_cb), dialog);

	g_signal_connect_swapped(G_OBJECT(window), "destroy", G_CALLBACK(edit_dialog_destroy), dialog);

	gnt_widget_show(window);
	gnt_box_readjust(GNT_BOX(window));
	gnt_widget_draw(window);
}

static void
add_account_cb(GntWidget *widget, gpointer null)
{
	edit_account(NULL);
}

static void
modify_account_cb(GntWidget *widget, GntTree *tree)
{
	PurpleAccount *account = gnt_tree_get_selection_data(tree);
	if (!account)
		return;
	edit_account(account);
}

static void
really_delete_account(PurpleAccount *account)
{
	GList *iter;
	for (iter = accountdialogs; iter; iter = iter->next)
	{
		AccountEditDialog *dlg = iter->data;
		if (dlg->account == account)
		{
			gnt_widget_destroy(dlg->window);
			break;
		}
	}
	purple_request_close_with_handle(account); /* Close any other opened delete window */
	purple_accounts_delete(account);
}

static void
delete_account_cb(GntWidget *widget, GntTree *tree)
{
	PurpleAccount *account;
	char *prompt;

	account  = gnt_tree_get_selection_data(tree);
	if (!account)
		return;

	prompt = g_strdup_printf(_("Are you sure you want to delete %s?"),
			purple_account_get_username(account));

	purple_request_action(account, _("Delete Account"), prompt, NULL,
						  PURPLE_DEFAULT_ACTION_NONE,
						  account, NULL, NULL, account, 2,
						  _("Delete"), really_delete_account,
						  _("Cancel"), NULL);
	g_free(prompt);
}

static void
account_toggled(GntWidget *widget, void *key, gpointer null)
{
	PurpleAccount *account = key;

	purple_account_set_enabled(account, FINCH_UI, gnt_tree_get_choice(GNT_TREE(widget), key));
}

static void
reset_accounts_win(GntWidget *widget, gpointer null)
{
	accounts.window = NULL;
	accounts.tree = NULL;
}

void finch_accounts_show_all()
{
	GList *iter;
	GntWidget *box, *button;

	if (accounts.window) {
		gnt_window_present(accounts.window);
		return;
	}

	accounts.window = gnt_vbox_new(FALSE);
	gnt_box_set_toplevel(GNT_BOX(accounts.window), TRUE);
	gnt_box_set_title(GNT_BOX(accounts.window), _("Accounts"));
	gnt_box_set_pad(GNT_BOX(accounts.window), 0);
	gnt_box_set_alignment(GNT_BOX(accounts.window), GNT_ALIGN_MID);
	gnt_widget_set_name(accounts.window, "accounts");

	gnt_box_add_widget(GNT_BOX(accounts.window),
			gnt_label_new(_("You can enable/disable accounts from the following list.")));

	gnt_box_add_widget(GNT_BOX(accounts.window), gnt_line_new(FALSE));

	accounts.tree = gnt_tree_new_with_columns(2);
	GNT_WIDGET_SET_FLAGS(accounts.tree, GNT_WIDGET_NO_BORDER);

	for (iter = purple_accounts_get_all(); iter; iter = iter->next)
	{
		PurpleAccount *account = iter->data;
		account_add(account);
	}

	g_signal_connect(G_OBJECT(accounts.tree), "toggled", G_CALLBACK(account_toggled), NULL);

	gnt_tree_set_col_width(GNT_TREE(accounts.tree), 0, 40);
	gnt_tree_set_col_width(GNT_TREE(accounts.tree), 1, 10);
	gnt_box_add_widget(GNT_BOX(accounts.window), accounts.tree);

	gnt_box_add_widget(GNT_BOX(accounts.window), gnt_line_new(FALSE));

	box = gnt_hbox_new(FALSE);

	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(box), button);
	gnt_util_set_trigger_widget(GNT_WIDGET(accounts.tree), GNT_KEY_INS, button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(add_account_cb), NULL);

	button = gnt_button_new(_("Modify"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(modify_account_cb), accounts.tree);

	button = gnt_button_new(_("Delete"));
	gnt_box_add_widget(GNT_BOX(box), button);
	gnt_util_set_trigger_widget(GNT_WIDGET(accounts.tree), GNT_KEY_DEL, button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(delete_account_cb), accounts.tree);

	gnt_box_add_widget(GNT_BOX(accounts.window), box);

	g_signal_connect(G_OBJECT(accounts.window), "destroy", G_CALLBACK(reset_accounts_win), NULL);

	gnt_widget_show(accounts.window);
}

void finch_account_dialog_show(PurpleAccount *account)
{
	edit_account(account);
}

static gpointer
finch_accounts_get_handle(void)
{
	static int handle;

	return &handle;
}

static void
account_added_callback(PurpleAccount *account)
{
	if (accounts.window == NULL)
		return;
	account_add(account);
	gnt_widget_draw(accounts.tree);
}

static void
account_removed_callback(PurpleAccount *account)
{
	if (accounts.window == NULL)
		return;

	gnt_tree_remove(GNT_TREE(accounts.tree), account);
}

static void
account_abled_cb(PurpleAccount *account, gpointer user_data)
{
	if (accounts.window == NULL)
		return;
	gnt_tree_set_choice(GNT_TREE(accounts.tree), account,
			GPOINTER_TO_INT(user_data));
}

void finch_accounts_init()
{
	GList *iter;

	purple_signal_connect(purple_accounts_get_handle(), "account-added",
			finch_accounts_get_handle(), PURPLE_CALLBACK(account_added_callback),
			NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
			finch_accounts_get_handle(), PURPLE_CALLBACK(account_removed_callback),
			NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-disabled",
			finch_accounts_get_handle(),
			PURPLE_CALLBACK(account_abled_cb), GINT_TO_POINTER(FALSE));
	purple_signal_connect(purple_accounts_get_handle(), "account-enabled",
			finch_accounts_get_handle(),
			PURPLE_CALLBACK(account_abled_cb), GINT_TO_POINTER(TRUE));

	iter = purple_accounts_get_all();
	if (iter) {
		for (; iter; iter = iter->next) {
			if (purple_account_get_enabled(iter->data, FINCH_UI))
				break;
		}
		if (!iter)
			finch_accounts_show_all();
	} else {
		edit_account(NULL);
		finch_accounts_show_all();
	}
}

void finch_accounts_uninit()
{
	if (accounts.window)
		gnt_widget_destroy(accounts.window);
}

/* The following uiops stuff are copied from gtkaccount.c */
typedef struct
{
	PurpleAccount *account;
	char *username;
	char *alias;
} AddUserData;

static char *
make_info(PurpleAccount *account, PurpleConnection *gc, const char *remote_user,
          const char *id, const char *alias, const char *msg)
{
	if (msg != NULL && *msg == '\0')
		msg = NULL;

	return g_strdup_printf(_("%s%s%s%s has made %s his or her buddy%s%s"),
	                       remote_user,
	                       (alias != NULL ? " ("  : ""),
	                       (alias != NULL ? alias : ""),
	                       (alias != NULL ? ")"   : ""),
	                       (id != NULL
	                        ? id
	                        : (purple_connection_get_display_name(gc) != NULL
	                           ? purple_connection_get_display_name(gc)
	                           : purple_account_get_username(account))),
	                       (msg != NULL ? ": " : "."),
	                       (msg != NULL ? msg  : ""));
}

static void
notify_added(PurpleAccount *account, const char *remote_user,
			const char *id, const char *alias,
			const char *msg)
{
	char *buffer;
	PurpleConnection *gc;

	gc = purple_account_get_connection(account);

	buffer = make_info(account, gc, remote_user, id, alias, msg);

	purple_notify_info(NULL, NULL, buffer, NULL);

	g_free(buffer);
}

static void
free_add_user_data(AddUserData *data)
{
	g_free(data->username);

	if (data->alias != NULL)
		g_free(data->alias);

	g_free(data);
}

static void
add_user_cb(AddUserData *data)
{
	PurpleConnection *gc = purple_account_get_connection(data->account);

	if (g_list_find(purple_connections_get_all(), gc))
	{
		purple_blist_request_add_buddy(data->account, data->username,
									 NULL, data->alias);
	}

	free_add_user_data(data);
}

static void
request_add(PurpleAccount *account, const char *remote_user,
		  const char *id, const char *alias,
		  const char *msg)
{
	char *buffer;
	PurpleConnection *gc;
	AddUserData *data;

	gc = purple_account_get_connection(account);

	data = g_new0(AddUserData, 1);
	data->account  = account;
	data->username = g_strdup(remote_user);
	data->alias    = (alias != NULL ? g_strdup(alias) : NULL);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	purple_request_action(NULL, NULL, _("Add buddy to your list?"),
	                    buffer, PURPLE_DEFAULT_ACTION_NONE,
						account, remote_user, NULL,
						data, 2,
	                    _("Add"),    G_CALLBACK(add_user_cb),
	                    _("Cancel"), G_CALLBACK(free_add_user_data));
	g_free(buffer);
}

/* Copied from gtkaccount.c */
typedef struct {
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	void *data;
	char *username;
	char *alias;
	PurpleAccount *account;
} auth_and_add;

static void
free_auth_and_add(auth_and_add *aa)
{
	g_free(aa->username);
	g_free(aa->alias);
	g_free(aa);
}

static void
authorize_and_add_cb(auth_and_add *aa)
{
	aa->auth_cb(aa->data);
	purple_blist_request_add_buddy(aa->account, aa->username,
	 	                    NULL, aa->alias);
}

static void
deny_no_add_cb(auth_and_add *aa)
{
	aa->deny_cb(aa->data);
}

static void *
finch_request_authorize(PurpleAccount *account,
                        const char *remote_user,
                        const char *id,
                        const char *alias,
                        const char *message,
                        gboolean on_list,
                        PurpleAccountRequestAuthorizationCb auth_cb,
                        PurpleAccountRequestAuthorizationCb deny_cb,
                        void *user_data)
{
	char *buffer;
	PurpleConnection *gc;
	void *uihandle;

	gc = purple_account_get_connection(account);
	if (message != NULL && *message == '\0')
		message = NULL;

	buffer = g_strdup_printf(_("%s%s%s%s wants to add %s to his or her buddy list%s%s"),
				remote_user,
	 	                (alias != NULL ? " ("  : ""),
		                (alias != NULL ? alias : ""),
		                (alias != NULL ? ")"   : ""),
		                (id != NULL
		                ? id
		                : (purple_connection_get_display_name(gc) != NULL
		                ? purple_connection_get_display_name(gc)
		                : purple_account_get_username(account))),
		                (message != NULL ? ": " : "."),
		                (message != NULL ? message  : ""));
	if (!on_list) {
		GntWidget *widget;
		GList *iter;
		auth_and_add *aa = g_new(auth_and_add, 1);

		aa->auth_cb = auth_cb;
		aa->deny_cb = deny_cb;
		aa->data = user_data;
		aa->username = g_strdup(remote_user);
		aa->alias = g_strdup(alias);
		aa->account = account;

		uihandle = gnt_vwindow_new(FALSE);
		gnt_box_set_title(GNT_BOX(uihandle), _("Authorize buddy?"));
		gnt_box_set_pad(GNT_BOX(uihandle), 0);

		widget = purple_request_action(NULL, _("Authorize buddy?"), buffer, NULL,
			PURPLE_DEFAULT_ACTION_NONE,
			account, remote_user, NULL,
			aa, 2,
			_("Authorize"), authorize_and_add_cb,
			_("Deny"), deny_no_add_cb);
		gnt_screen_release(widget);
		gnt_box_set_toplevel(GNT_BOX(widget), FALSE);
		gnt_box_add_widget(GNT_BOX(uihandle), widget);

		gnt_box_add_widget(GNT_BOX(uihandle), gnt_hline_new());

		widget = finch_retrieve_user_info(purple_account_get_connection(account), remote_user);
		for (iter = GNT_BOX(widget)->list; iter; iter = iter->next) {
			if (GNT_IS_BUTTON(iter->data)) {
				gnt_widget_destroy(iter->data);
				gnt_box_remove(GNT_BOX(widget), iter->data);
				break;
			}
		}
		gnt_box_set_toplevel(GNT_BOX(widget), FALSE);
		gnt_screen_release(widget);
		gnt_box_add_widget(GNT_BOX(uihandle), widget);
		gnt_widget_show(uihandle);

		g_signal_connect_swapped(G_OBJECT(uihandle), "destroy", G_CALLBACK(free_auth_and_add), aa);
	} else {
		uihandle = purple_request_action(NULL, _("Authorize buddy?"), buffer, NULL,
			PURPLE_DEFAULT_ACTION_NONE,
			account, remote_user, NULL,
			user_data, 2,
			_("Authorize"), auth_cb,
			_("Deny"), deny_cb);
	}
	g_free(buffer);
	return uihandle;
}

static void
finch_request_close(void *uihandle)
{
	purple_request_close(PURPLE_REQUEST_ACTION, uihandle);
}

static PurpleAccountUiOps ui_ops =
{
	notify_added,
	NULL,
	request_add,
	finch_request_authorize,
	finch_request_close,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleAccountUiOps *finch_accounts_get_ui_ops()
{
	return &ui_ops;
}


#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntcheckbox.h>
#include <gntcombobox.h>
#include <gntentry.h>
#include <gntlabel.h>
#include <gntline.h>
#include <gnttree.h>

#include <account.h>
#include <accountopt.h>
#include <connection.h>
#include <notify.h>
#include <plugin.h>
#include <request.h>

#include "gntaccount.h"
#include "gntgaim.h"

#include <string.h>

typedef struct
{
	GntWidget *window;
	GntWidget *tree;
} GGAccountList;

static GGAccountList accounts;

typedef struct
{
	GaimAccount *account;          /* NULL for a new account */

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
} AccountEditDialog;

static void
account_add(GaimAccount *account)
{
	gnt_tree_add_choice(GNT_TREE(accounts.tree), account,
			gnt_tree_create_row(GNT_TREE(accounts.tree),
				gaim_account_get_username(account),
				gaim_account_get_protocol_name(account)),
			NULL, NULL);
	gnt_tree_set_choice(GNT_TREE(accounts.tree), account,
			gaim_account_get_enabled(account, GAIM_GNT_UI));
}

static void
edit_dialog_destroy(AccountEditDialog *dialog)
{
	g_list_free(dialog->prpl_entries);
	g_list_free(dialog->split_entries);
	g_free(dialog);
}

static void
save_account_cb(AccountEditDialog *dialog)
{
	GaimAccount *account;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prplinfo;
	const char *value;
	GString *username;

	/* XXX: Do some error checking first. */

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

	/* Screenname && user-splits */
	value = gnt_entry_get_text(GNT_ENTRY(dialog->screenname));

	if (value == NULL || *value == '\0')
	{
		gaim_notify_error(NULL, _("Error"), _("Account was not added"),
				_("Screenname of an account must be non-empty."));
		return;
	}
	
	username = g_string_new(value);

	if (prplinfo != NULL)
	{
		GList *iter, *entries;
		for (iter = prplinfo->user_splits, entries = dialog->split_entries;
				iter && entries; iter = iter->next, entries = entries->next)
		{
			GaimAccountUserSplit *split = iter->data;
			GntWidget *entry = entries->data;

			value = gnt_entry_get_text(GNT_ENTRY(entry));
			if (value == NULL || *value == '\0')
				value = gaim_account_user_split_get_default_value(split);
			g_string_append_printf(username, "%c%s",
					gaim_account_user_split_get_separator(split),
					value);
		}
	}

	if (dialog->account == NULL)
	{
		account = gaim_account_new(username->str, gaim_plugin_get_id(plugin));
		gaim_accounts_add(account);
	}
	else
	{
		account = dialog->account;

		/* Protocol */
		gaim_account_set_protocol_id(account, gaim_plugin_get_id(plugin));
		gaim_account_set_username(account, username->str);
	}
	g_string_free(username, TRUE);

	/* Alias */
	value = gnt_entry_get_text(GNT_ENTRY(dialog->alias));
	if (value && *value)
		gaim_account_set_alias(account, value);

	/* Remember password and password */
	gaim_account_set_remember_password(account,
			gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->remember)));
	value = gnt_entry_get_text(GNT_ENTRY(dialog->password));
	if (value && *value && gaim_account_get_remember_password(account))
		gaim_account_set_password(account, value);
	else
		gaim_account_set_password(account, NULL);

	/* Mail notification */
	/* XXX: Only if the protocol has anything to do with emails */
	gaim_account_set_check_mail(account,
			gnt_check_box_get_checked(GNT_CHECK_BOX(dialog->newmail)));

	/* Protocol options */
	if (prplinfo)
	{
		GList *iter, *entries;
		
		for (iter = prplinfo->protocol_options, entries = dialog->prpl_entries;
				iter && entries; iter = iter->next, entries = entries->next)
		{
			GaimAccountOption *option = iter->data;
			GntWidget *entry = entries->data;
			GaimPrefType type = gaim_account_option_get_type(option);
			const char *setting = gaim_account_option_get_setting(option);

			if (type == GAIM_PREF_STRING)
			{
				const char *value = gnt_entry_get_text(GNT_ENTRY(entry));
				gaim_account_set_string(account, setting, value);
			}
			else if (type == GAIM_PREF_INT)
			{
				const char *str = gnt_entry_get_text(GNT_ENTRY(entry));
				int value = 0;
				if (str)
					value = atoi(str);
				gaim_account_set_int(account, setting, value);
			}
			else if (type == GAIM_PREF_BOOLEAN)
			{
				gboolean value = gnt_check_box_get_checked(GNT_CHECK_BOX(entry));
				gaim_account_set_bool(account, setting, value);
			}
			else if (type == GAIM_PREF_STRING_LIST)
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

	gnt_widget_destroy(dialog->window);
}

static void
update_user_splits(AccountEditDialog *dialog)
{
	GntWidget *hbox;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prplinfo;
	GList *iter, *entries;
	char *username = NULL;

	if (dialog->splits)
	{
		gnt_box_remove_all(GNT_BOX(dialog->splits));
		g_list_free(dialog->split_entries);
	}
	else
	{
		dialog->splits = gnt_box_new(FALSE, TRUE);
		gnt_box_set_pad(GNT_BOX(dialog->splits), 0);
	}

	dialog->split_entries = NULL;

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	if (!plugin)
		return;
	prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plugin);
	
	username = g_strdup(gaim_account_get_username(dialog->account));

	for (iter = prplinfo->user_splits; iter; iter = iter->next)
	{
		GaimAccountUserSplit *split = iter->data;
		GntWidget *entry;
		char *buf;

		hbox = gnt_box_new(TRUE, FALSE);
		gnt_box_add_widget(GNT_BOX(dialog->splits), hbox);

		buf = g_strdup_printf("%s:", gaim_account_user_split_get_text(split));
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
		GaimAccountUserSplit *split = iter->data;
		const char *value = NULL;
		char *s;

		if (dialog->account)
		{
			s = strrchr(username, gaim_account_user_split_get_separator(split));
			if (s != NULL)
			{
				*s = '\0';
				s++;
				value = s;
			}
		}
		if (value == NULL)
			value = gaim_account_user_split_get_default_value(split);

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
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prplinfo;
	GList *iter;
	GntWidget *vbox, *box;
	GaimAccount *account;

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

	prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

	account = dialog->account;

	for (iter = prplinfo->protocol_options; iter; iter = iter->next)
	{
		GaimAccountOption *option = iter->data;
		GaimPrefType type = gaim_account_option_get_type(option);

		box = gnt_hbox_new(TRUE);
		gnt_box_set_pad(GNT_BOX(box), 0);
		gnt_box_add_widget(GNT_BOX(vbox), box);

		if (type == GAIM_PREF_BOOLEAN)
		{
			GntWidget *widget = gnt_check_box_new(gaim_account_option_get_text(option));
			gnt_box_add_widget(GNT_BOX(box), widget);
			dialog->prpl_entries = g_list_append(dialog->prpl_entries, widget);

			if (account)
				gnt_check_box_set_checked(GNT_CHECK_BOX(widget),
						gaim_account_get_bool(account,
							gaim_account_option_get_setting(option),
							gaim_account_option_get_default_bool(option)));
			else
				gnt_check_box_set_checked(GNT_CHECK_BOX(widget),
						gaim_account_option_get_default_bool(option));
		}
		else
		{
			gnt_box_add_widget(GNT_BOX(box),
					gnt_label_new(gaim_account_option_get_text(option)));

			if (type == GAIM_PREF_STRING_LIST)
			{
				/* TODO: Use a combobox */
				/*       Don't forget to append the widget to prpl_entries */
			}
			else
			{
				GntWidget *entry = gnt_entry_new(NULL);
				gnt_box_add_widget(GNT_BOX(box), entry);
				dialog->prpl_entries = g_list_append(dialog->prpl_entries, entry);

				if (type == GAIM_PREF_STRING)
				{
					const char *dv = gaim_account_option_get_default_string(option);

					if (account)
						gnt_entry_set_text(GNT_ENTRY(entry),
								gaim_account_get_string(account,
									gaim_account_option_get_setting(option), dv));
					else
						gnt_entry_set_text(GNT_ENTRY(entry), dv);
				}
				else if (type == GAIM_PREF_INT)
				{
					char str[32];
					int value = gaim_account_option_get_default_int(option);
					if (account)
						value = gaim_account_get_int(account,
								gaim_account_option_get_setting(option), value);
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
}

static void
update_user_options(AccountEditDialog *dialog)
{
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prplinfo;

	plugin = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(dialog->protocol));
	if (!plugin)
		return;

	prplinfo = GAIM_PLUGIN_PROTOCOL_INFO(plugin);

	if (dialog->newmail == NULL)
		dialog->newmail = gnt_check_box_new(_("New mail notifications"));
	if (dialog->account)
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->newmail),
				gaim_account_get_check_mail(dialog->account));
	if (!prplinfo || !(prplinfo->options & OPT_PROTO_MAIL_CHECK))
		gnt_widget_set_visible(dialog->newmail, FALSE);
	else
		gnt_widget_set_visible(dialog->newmail, TRUE);

	if (dialog->remember == NULL)
		dialog->remember = gnt_check_box_new(_("Remember password"));
	if (dialog->account)
		gnt_check_box_set_checked(GNT_CHECK_BOX(dialog->remember),
				gaim_account_get_remember_password(dialog->account));
}

static void
prpl_changed_cb(GntWidget *combo, GaimPlugin *old, GaimPlugin *new, AccountEditDialog *dialog)
{
	update_user_splits(dialog);
	add_protocol_options(dialog);
	update_user_options(dialog);  /* This may not be necessary here */
	gnt_box_readjust(GNT_BOX(dialog->window));
	gnt_widget_draw(dialog->window);
}

static void
edit_account(GaimAccount *account)
{
	GntWidget *window, *hbox;
	GntWidget *combo, *button, *entry;
	GList *list, *iter;
	AccountEditDialog *dialog;

	dialog = g_new0(AccountEditDialog, 1);

	dialog->window = window = gnt_box_new(FALSE, TRUE);
	dialog->account = account;
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), account ? _("Modify Account") : _("New Account"));
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_pad(GNT_BOX(window), 0);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);
	gnt_box_set_alignment(GNT_BOX(hbox), GNT_ALIGN_MID);

	dialog->protocol = combo = gnt_combo_box_new();
	list = gaim_plugins_get_protocols();
	for (iter = list; iter; iter = iter->next)
	{
		gnt_combo_box_add_data(GNT_COMBO_BOX(combo), iter->data,
				((GaimPlugin*)iter->data)->info->name);
	}
	if (account)
		gnt_combo_box_set_selected(GNT_COMBO_BOX(combo),
				gaim_plugins_find_with_id(gaim_account_get_protocol_id(account)));
	else
		gnt_combo_box_set_selected(GNT_COMBO_BOX(combo), list->data);

	g_signal_connect(G_OBJECT(combo), "selection-changed", G_CALLBACK(prpl_changed_cb), dialog);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Protocol:")));
	gnt_box_add_widget(GNT_BOX(hbox), combo);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->screenname = entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Screen name:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);

	/* User splits */
	update_user_splits(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->splits);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->password = entry = gnt_entry_new(NULL);
	gnt_entry_set_masked(GNT_ENTRY(entry), TRUE);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Password:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	if (account)
		gnt_entry_set_text(GNT_ENTRY(entry), gaim_account_get_password(account));

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->alias = entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Alias:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);
	if (account)
		gnt_entry_set_text(GNT_ENTRY(entry), gaim_account_get_alias(account));

	/* User options */
	update_user_options(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->remember);
	gnt_box_add_widget(GNT_BOX(window), dialog->newmail);

	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));

	/* The advanced box */
	add_protocol_options(dialog);
	gnt_box_add_widget(GNT_BOX(window), dialog->prpls);

	/* TODO: Add proxy options */

	/* The button box */
	hbox = gnt_box_new(FALSE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

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
	GaimAccount *account = gnt_tree_get_selection_data(tree);
	if (!account)
		return;
	edit_account(account);
}

static void
delete_account_cb(GntWidget *widget, GntTree *tree)
{
	/* XXX: After the request-api is complete */
	/* Note: remove the modify-dialog for the account */
}

static void
account_toggled(GntWidget *widget, void *key, gpointer null)
{
	GaimAccount *account = key;

	gaim_account_set_enabled(account, GAIM_GNT_UI, gnt_tree_get_choice(GNT_TREE(widget), key));
}

void gg_accounts_show_all()
{
	GList *iter;
	GntWidget *box, *button;

	accounts.window = gnt_box_new(FALSE, TRUE);
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

	for (iter = gaim_accounts_get_all(); iter; iter = iter->next)
	{
		GaimAccount *account = iter->data;
		account_add(account);
	}

	g_signal_connect(G_OBJECT(accounts.tree), "toggled", G_CALLBACK(account_toggled), NULL);
	
	gnt_tree_set_col_width(GNT_TREE(accounts.tree), 0, 40);
	gnt_tree_set_col_width(GNT_TREE(accounts.tree), 1, 10);
	gnt_box_add_widget(GNT_BOX(accounts.window), accounts.tree);

	gnt_box_add_widget(GNT_BOX(accounts.window), gnt_line_new(FALSE));

	box = gnt_box_new(FALSE, FALSE);

	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(add_account_cb), NULL);

	button = gnt_button_new(_("Modify"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(modify_account_cb), accounts.tree);

	button = gnt_button_new(_("Delete"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(delete_account_cb), accounts.tree);
	
	gnt_box_add_widget(GNT_BOX(accounts.window), box);
	
	gnt_widget_show(accounts.window);
}

static gpointer
gg_accounts_get_handle()
{
	static int handle;

	return &handle;
}

static void
account_added_callback(GaimAccount *account)
{
	if (accounts.window == NULL)
		return;
	account_add(account);
	gnt_widget_draw(accounts.tree);
}

static void
account_removed_callback(GaimAccount *account)
{
	if (accounts.window == NULL)
		return;

	gnt_tree_remove(GNT_TREE(accounts.tree), account);
}

void gg_accounts_init()
{
	gaim_signal_connect(gaim_accounts_get_handle(), "account-added",
			gg_accounts_get_handle(), GAIM_CALLBACK(account_added_callback),
			NULL);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
			gg_accounts_get_handle(), GAIM_CALLBACK(account_removed_callback),
			NULL);
	
	gg_accounts_show_all();
}

void gg_accounts_uninit()
{
	gnt_widget_destroy(accounts.window);
}

#if 0
/* The following uiops stuff are copied from gtkaccount.c */
/* Need to do some work on notify- and request-ui before this works */
typedef struct
{
	GaimAccount *account;
	char *username;
	char *alias;
} AddUserData;

static char *
make_info(GaimAccount *account, GaimConnection *gc, const char *remote_user,
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
	                        : (gaim_connection_get_display_name(gc) != NULL
	                           ? gaim_connection_get_display_name(gc)
	                           : gaim_account_get_username(account))),
	                       (msg != NULL ? ": " : "."),
	                       (msg != NULL ? msg  : ""));
}

static void
notify_added(GaimAccount *account, const char *remote_user,
			const char *id, const char *alias,
			const char *msg)
{
	char *buffer;
	GaimConnection *gc;

	gc = gaim_account_get_connection(account);

	buffer = make_info(account, gc, remote_user, id, alias, msg);

	gaim_notify_info(NULL, NULL, buffer, NULL);

	g_free(buffer);
}

static void
request_add(GaimAccount *account, const char *remote_user,
		  const char *id, const char *alias,
		  const char *msg)
{
	char *buffer;
	GaimConnection *gc;
	AddUserData *data;

	gc = gaim_account_get_connection(account);

	data = g_new0(AddUserData, 1);
	data->account  = account;
	data->username = g_strdup(remote_user);
	data->alias    = (alias != NULL ? g_strdup(alias) : NULL);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
#if 0
	gaim_request_action(NULL, NULL, _("Add buddy to your list?"),
	                    buffer, GAIM_DEFAULT_ACTION_NONE, data, 2,
	                    _("Add"),    G_CALLBACK(add_user_cb),
	                    _("Cancel"), G_CALLBACK(free_add_user_data));
#endif
	g_free(buffer);
}

static GaimAccountUiOps ui_ops = 
{
	.notify_added = notify_added,
	.status_changed = NULL,
	.request_add  = request_add
};
#else

static GaimAccountUiOps ui_ops = 
{
	.notify_added = NULL,
	.status_changed = NULL,
	.request_add  = NULL
};

#endif

GaimAccountUiOps *gg_accounts_get_ui_ops()
{
	return &ui_ops;
}


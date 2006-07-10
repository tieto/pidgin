#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
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
} AccountEditDialog;

static void
edit_dialog_destroy(AccountEditDialog *dialog)
{
	g_free(dialog);
}

static void
save_account_cb(AccountEditDialog *dialog)
{
}

static void
update_user_splits(AccountEditDialog *dialog)
{
	GntWidget *hbox;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prplinfo;
	GList *iter;
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

	/* XXX: Add default/custom values to the splits */
	g_free(username);
}

static void
prpl_changed_cb(GntWidget *combo, GaimPlugin *old, GaimPlugin *new, AccountEditDialog *dialog)
{
	update_user_splits(dialog);
	gnt_box_readjust(GNT_BOX(dialog->window));
	gnt_widget_draw(dialog->window);
}

static void
add_account(GntWidget *b, gpointer null)
{
	GntWidget *window, *hbox;
	GntWidget *combo, *button, *entry;
	GList *list, *iter;
	AccountEditDialog *dialog;

	dialog = g_new0(AccountEditDialog, 1);

	dialog->window = window = gnt_box_new(FALSE, TRUE);
	gnt_box_set_toplevel(GNT_BOX(window), TRUE);
	gnt_box_set_title(GNT_BOX(window), _("New Account"));
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
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Password:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);

	hbox = gnt_box_new(TRUE, FALSE);
	gnt_box_add_widget(GNT_BOX(window), hbox);

	dialog->alias = entry = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(hbox), gnt_label_new(_("Alias:")));
	gnt_box_add_widget(GNT_BOX(hbox), entry);

	gnt_box_add_widget(GNT_BOX(window), gnt_line_new(FALSE));
	
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
}

static void
account_toggled(GntWidget *widget, void *key, gpointer null)
{
	GaimAccount *account = key;

	gaim_account_set_enabled(account, GAIM_GNT_UI, gnt_tree_get_choice(GNT_TREE(widget), key));
}

void gg_accounts_init()
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

	accounts.tree = gnt_tree_new();
	GNT_WIDGET_SET_FLAGS(accounts.tree, GNT_WIDGET_NO_BORDER);

	for (iter = gaim_accounts_get_all(); iter; iter = iter->next)
	{
		GaimAccount *account = iter->data;
		char *str = g_strdup_printf("%s (%s)",
				gaim_account_get_username(account), gaim_account_get_protocol_id(account));

		gnt_tree_add_choice(GNT_TREE(accounts.tree), account,
				str, NULL, NULL);
		gnt_tree_set_choice(GNT_TREE(accounts.tree), account,
				gaim_account_get_enabled(account, GAIM_GNT_UI));
		g_free(str);
	}

	g_signal_connect(G_OBJECT(accounts.tree), "toggled", G_CALLBACK(account_toggled), NULL);
	
	gnt_widget_set_size(accounts.tree, 40, 10);
	gnt_box_add_widget(GNT_BOX(accounts.window), accounts.tree);

	gnt_box_add_widget(GNT_BOX(accounts.window), gnt_line_new(FALSE));

	box = gnt_box_new(FALSE, FALSE);

	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(add_account), NULL);

	button = gnt_button_new(_("Modify"));
	gnt_box_add_widget(GNT_BOX(box), button);

	button = gnt_button_new(_("Delete"));
	gnt_box_add_widget(GNT_BOX(box), button);
	
	gnt_box_add_widget(GNT_BOX(accounts.window), box);
	
	gnt_widget_show(accounts.window);
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


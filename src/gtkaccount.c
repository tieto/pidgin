/**
 * @file gtkaccount.c Account Editor dialog
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
 */
#include "gtkaccount.h"
#include "account.h"
#include "accountopt.h"
#include "event.h"
#include "prefs.h"
#include "stock.h"
#include "gtkblist.h"
#include "gaim-disclosure.h"

#ifdef _WIN32       
# include <gdk/gdkwin32.h>
#else
# include <unistd.h>
# include <gdk/gdkx.h>
#endif

#include <string.h>

enum
{
	COLUMN_ICON,
	COLUMN_PROTOCOL,
	COLUMN_SCREENNAME,
	COLUMN_ONLINE,
	COLUMN_AUTOLOGIN,
	COLUMN_FILLER,
	COLUMN_DATA,
	NUM_COLUMNS
};

typedef enum
{
	ADD_ACCOUNT_DIALOG,
	MODIFY_ACCOUNT_DIALOG

} AccountPrefsDialogType;

typedef struct
{
	GtkWidget *window;
	GtkWidget *treeview;

	GtkWidget *modify_button;
	GtkWidget *delete_button;

	GtkListStore *model;
	GtkTreeIter drag_iter;

	GtkTreeViewColumn *screenname_col;

} AccountsDialog;

typedef struct
{
	AccountPrefsDialogType type;

	AccountsDialog *accounts_dialog;

	GaimAccount *account;
	GaimProtocol protocol;
	GaimPlugin *plugin;
	GaimPluginProtocolInfo *prpl_info;

	GaimProxyType new_proxy_type;

	GList *user_split_entries;
	GList *protocol_opt_entries;

	GtkSizeGroup *sg;
	GtkWidget *window;

	GtkWidget *top_vbox;
	GtkWidget *bottom_vbox;
	GtkWidget *ok_button;

	/* Login Options */
	GtkWidget *login_frame;
	GtkWidget *protocol_menu;
	GtkWidget *password_box;
	GtkWidget *screenname_entry;
	GtkWidget *password_entry;
	GtkWidget *alias_entry;
	GtkWidget *remember_pass_check;
	GtkWidget *auto_login_check;

	/* User Options */
	GtkWidget *user_frame;
	GtkWidget *new_mail_check;
	GtkWidget *buddy_icon_hbox;
	GtkWidget *buddy_icon_entry;

	/* Protocol Options */
	GtkWidget *protocol_frame;
	GtkWidget *register_check;

	/* Proxy Options */
	GtkWidget *proxy_frame;
	GtkWidget *proxy_vbox;
	GtkWidget *proxy_dropdown;
	GtkWidget *proxy_menu;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_entry;
	GtkWidget *proxy_user_entry;
	GtkWidget *proxy_pass_entry;

} AccountPrefsDialog;


static AccountsDialog *accounts_dialog = NULL;

static void __add_account(AccountsDialog *dialog, GaimAccount *account);
static void __set_account(GtkListStore *store, GtkTreeIter *iter,
						  GaimAccount *account);

static char *
proto_name(int proto)
{
	GaimPlugin *p = gaim_find_prpl(proto);

	return ((p && p->info->name) ? _(p->info->name) : _("Unknown"));
}

/**************************************************************************
 * Add/Modify Account dialog
 **************************************************************************/
static void __add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void __add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void __add_protocol_options(AccountPrefsDialog *dialog,
								   GtkWidget *parent);
static void __add_proxy_options(AccountPrefsDialog *dialog, GtkWidget *parent);

static GtkWidget *
__add_pref_box(AccountPrefsDialog *dialog, GtkWidget *parent,
			   const char *text, GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(text);
	gtk_size_group_add_widget(dialog->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);
	gtk_widget_show(widget);

	return hbox;
}

static void
__set_account_protocol_cb(GtkWidget *item, GaimProtocol protocol,
						  AccountPrefsDialog *dialog)
{
	dialog->protocol = protocol;

	if ((dialog->plugin = gaim_find_prpl(dialog->protocol)) != NULL)
		dialog->prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(dialog->plugin);

	__add_login_options(dialog,    dialog->top_vbox);
	__add_user_options(dialog,     dialog->top_vbox);
	__add_protocol_options(dialog, dialog->bottom_vbox);
}

static void
__screenname_changed_cb(GtkEntry *entry, AccountPrefsDialog *dialog)
{
	if (dialog->ok_button == NULL)
		return;

	gtk_widget_set_sensitive(dialog->ok_button,
							 *gtk_entry_get_text(entry) != '\0');
}

static void
__add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *entry;
	GList *user_splits;
	GList *l, *l2;
	char *username = NULL;

	if (dialog->login_frame != NULL)
		gtk_widget_destroy(dialog->login_frame);

	/* Build the login options frame. */
	frame = gaim_gtk_make_frame(parent, _("Login Options"));

	/* cringe */
	dialog->login_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->login_frame, 0);
	gtk_widget_show(dialog->login_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Protocol */
	dialog->protocol_menu = gaim_gtk_protocol_option_menu_new(
			dialog->protocol, G_CALLBACK(__set_account_protocol_cb), dialog);

	__add_pref_box(dialog, vbox, _("Protocol:"), dialog->protocol_menu);

	/* Screen Name */
	dialog->screenname_entry = gtk_entry_new();

	__add_pref_box(dialog, vbox, _("Screenname:"), dialog->screenname_entry);

	g_signal_connect(G_OBJECT(dialog->screenname_entry), "changed",
					 G_CALLBACK(__screenname_changed_cb), dialog);

	/* Do the user split thang */
	if (dialog->plugin == NULL) /* Yeah right. */
		user_splits = NULL;
	else
		user_splits = dialog->prpl_info->user_splits;

	if (dialog->account != NULL)
		username = g_strdup(gaim_account_get_username(dialog->account));

	if (dialog->user_split_entries != NULL) {
		g_list_free(dialog->user_split_entries);
		dialog->user_split_entries = NULL;
	}

	for (l = user_splits; l != NULL; l = l->next) {
		GaimAccountUserSplit *split = l->data;
		char *buf;

		buf = g_strdup_printf("%s:", gaim_account_user_split_get_text(split));

		entry = gtk_entry_new();

		__add_pref_box(dialog, vbox, buf, entry);

		g_free(buf);

		dialog->user_split_entries =
			g_list_append(dialog->user_split_entries, entry);
	}

	for (l = g_list_last(dialog->user_split_entries),
		 l2 = g_list_last(user_splits);
		 l != NULL && l2 != NULL;
		 l = l->prev, l2 = l2->prev) {

		GtkWidget *entry = l->data;
		GaimAccountUserSplit *split = l2->data;
		const char *value = NULL;
		char *c;

		if (dialog->account != NULL) {
			c = strrchr(username,
						gaim_account_user_split_get_separator(split));

			if (c != NULL) {
				*c = '\0';
				c++;

				value = c;
			}
		}

		if (value == NULL)
			value = gaim_account_user_split_get_default_value(split);

		if (value != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), value);
	}

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(dialog->screenname_entry), username);

	g_free(username);


	/* Password */
	dialog->password_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(dialog->password_entry), FALSE);
	dialog->password_box = __add_pref_box(dialog, vbox, _("Password:"),
										  dialog->password_entry);

	/* Alias */
	dialog->alias_entry = gtk_entry_new();
	__add_pref_box(dialog, vbox, _("Alias:"), dialog->alias_entry);

	/* Remember Password */
	dialog->remember_pass_check =
		gtk_check_button_new_with_label(_("Remember password"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->remember_pass_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->remember_pass_check);

	/* Auto-Login */
	dialog->auto_login_check =
		gtk_check_button_new_with_label(_("Auto-login"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->auto_login_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->auto_login_check);

	/* Set the fields. */
	if (dialog->account != NULL) {
		if (gaim_account_get_password(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->password_entry),
							   gaim_account_get_password(dialog->account));

		if (gaim_account_get_alias(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->alias_entry),
							   gaim_account_get_alias(dialog->account));

		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->remember_pass_check),
				gaim_account_get_remember_password(dialog->account));

		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->auto_login_check), FALSE);
	}

	if (dialog->prpl_info != NULL &&
		(dialog->prpl_info->options & OPT_PROTO_NO_PASSWORD)) {

		gtk_widget_hide(dialog->password_box);
		gtk_widget_hide(dialog->remember_pass_check);
	}
}

static void
__add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *label;

	if (dialog->user_frame != NULL)
		gtk_widget_destroy(dialog->user_frame);

	/* Build the user options frame. */
	frame = gaim_gtk_make_frame(parent, _("User Options"));
	dialog->user_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->user_frame, 1);
	gtk_widget_show(dialog->user_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* New mail notifications */
	dialog->new_mail_check =
		gtk_check_button_new_with_label(_("New mail notifications"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->new_mail_check, FALSE, FALSE, 0);
	gtk_widget_show(dialog->new_mail_check);

	/* Buddy icon */
	dialog->buddy_icon_hbox = hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Buddy icon file:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	dialog->buddy_icon_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), dialog->buddy_icon_entry, TRUE, TRUE, 0);
	gtk_widget_show(dialog->buddy_icon_entry);

	button = gtk_button_new_with_label(_("Browse"));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	if (dialog->prpl_info != NULL) {
		if (!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK))
			gtk_widget_hide(dialog->new_mail_check);

		if (!(dialog->prpl_info->options & OPT_PROTO_BUDDY_ICON))
			gtk_widget_hide(dialog->buddy_icon_hbox);
	}

	if (dialog->account != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->new_mail_check),
				gaim_account_get_check_mail(dialog->account));

		if (gaim_account_get_buddy_icon(dialog->account) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->buddy_icon_entry),
					gaim_account_get_buddy_icon(dialog->account));
	}

	if (!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK) &&
		!(dialog->prpl_info->options & OPT_PROTO_BUDDY_ICON)) {

		/* Nothing to see :( aww. */
		gtk_widget_hide(dialog->user_frame);
	}
}

static void
__add_protocol_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GaimAccountOption *option;
	GaimAccount *account;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *check;
	GtkWidget *entry;
	GList *l;
	char buf[1024];
	char *title;
	const char *str_value;
	gboolean bool_value;
	int int_value;

	if (dialog->protocol_frame != NULL) {
		gtk_widget_destroy(dialog->protocol_frame);
		dialog->protocol_frame = NULL;
	}

	if (dialog->prpl_info == NULL ||
		dialog->prpl_info->protocol_options == NULL) {

		return;
	}

	account = dialog->account;

	/* Build the protocol options frame. */
	g_snprintf(buf, sizeof(buf), _("%s Options"), dialog->plugin->info->name);

	frame = gaim_gtk_make_frame(parent, buf);
	dialog->protocol_frame =
		gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->protocol_frame, 0);
	gtk_widget_show(dialog->protocol_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	if (dialog->protocol_opt_entries != NULL) {
		g_list_free(dialog->protocol_opt_entries);
		dialog->protocol_opt_entries = NULL;
	}

	for (l = dialog->prpl_info->protocol_options; l != NULL; l = l->next) {
		option = (GaimAccountOption *)l->data;

		switch (gaim_account_option_get_type(option)) {
			case GAIM_PREF_BOOLEAN:
				if (account == NULL ||
					gaim_account_get_protocol(account) != dialog->protocol) {

					bool_value = gaim_account_option_get_default_bool(option);
				}
				else
					bool_value = gaim_account_get_bool(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_bool(option));

				check = gtk_check_button_new_with_label(
					gaim_account_option_get_text(option));

				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
											 bool_value);

				gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);
				gtk_widget_show(check);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, check);

				break;

			case GAIM_PREF_INT:
				if (account == NULL ||
					gaim_account_get_protocol(account) != dialog->protocol) {

					int_value = gaim_account_option_get_default_int(option);
					}
				else
					int_value = gaim_account_get_int(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_int(option));

				g_snprintf(buf, sizeof(buf), "%d", int_value);

				entry = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(entry), buf);

				title = g_strdup_printf("%s:",
						gaim_account_option_get_text(option));

				__add_pref_box(dialog, vbox, title, entry);

				g_free(title);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, entry);

				break;

			case GAIM_PREF_STRING:
				if (account == NULL ||
					gaim_account_get_protocol(account) != dialog->protocol) {

					str_value = gaim_account_option_get_default_string(option);
				}
				else
					str_value = gaim_account_get_string(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_string(option));

				entry = gtk_entry_new();

				if (str_value != NULL)
					gtk_entry_set_text(GTK_ENTRY(entry), str_value);

				title = g_strdup_printf("%s:",
						gaim_account_option_get_text(option));

				__add_pref_box(dialog, vbox, title, entry);

				g_free(title);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, entry);

				break;

			default:
				break;
		}
	}

	/* Register user */
	if (dialog->prpl_info->register_user != NULL) {
		dialog->register_check =
			gtk_check_button_new_with_label(_("Register with your server"));

		gtk_box_pack_start(GTK_BOX(vbox), dialog->register_check,
						   FALSE, FALSE, 0);
		gtk_widget_show(dialog->register_check);
	}
}

static GtkWidget *
__make_proxy_dropdown(void)
{
	GtkWidget *dropdown;
	GtkWidget *menu;
	GtkWidget *item;

	dropdown = gtk_option_menu_new();
	menu = gtk_menu_new();

	/* Use Global Proxy Settings */
	item = gtk_menu_item_new_with_label(_("Use Global Proxy Settings"));
	g_object_set_data(G_OBJECT(item), "proxytype",
					  GINT_TO_POINTER(GAIM_PROXY_USE_GLOBAL));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* No Proxy */
	item = gtk_menu_item_new_with_label(_("No Proxy"));
	g_object_set_data(G_OBJECT(item), "proxytype",
					  GINT_TO_POINTER(GAIM_PROXY_NONE));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* SOCKS 4 */
	item = gtk_menu_item_new_with_label(_("SOCKS 4"));
	g_object_set_data(G_OBJECT(item), "proxytype",
					  GINT_TO_POINTER(GAIM_PROXY_SOCKS4));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* SOCKS 5 */
	item = gtk_menu_item_new_with_label(_("SOCKS 5"));
	g_object_set_data(G_OBJECT(item), "proxytype",
					  GINT_TO_POINTER(GAIM_PROXY_SOCKS5));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* HTTP */
	item = gtk_menu_item_new_with_label(_("HTTP"));
	g_object_set_data(G_OBJECT(item), "proxytype",
					  GINT_TO_POINTER(GAIM_PROXY_HTTP));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(dropdown), menu);

	return dropdown;
}

static void
__proxy_type_changed_cb(GtkWidget *optmenu, AccountPrefsDialog *dialog)
{
	dialog->new_proxy_type =
		gtk_option_menu_get_history(GTK_OPTION_MENU(optmenu)) - 1;

	if (dialog->new_proxy_type == GAIM_PROXY_USE_GLOBAL ||
		dialog->new_proxy_type == GAIM_PROXY_NONE) {

		gtk_widget_hide_all(dialog->proxy_vbox);
	}
	else
		gtk_widget_show_all(dialog->proxy_vbox);
}

static void
__port_popup_cb(GtkWidget *w, GtkMenu *menu, gpointer data)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label(
			_("you can see the butterflies mating"));
	gtk_widget_show(item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("If you look real closely"));
	gtk_widget_show(item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
}

static void
__add_proxy_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GaimProxyInfo *proxy_info;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *vbox2;

	if (dialog->proxy_frame != NULL)
		gtk_widget_destroy(dialog->proxy_frame);

	frame = gaim_gtk_make_frame(parent, _("Proxy Options"));
	dialog->proxy_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->proxy_frame, 1);
	gtk_widget_show(dialog->proxy_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Proxy Type drop-down. */
	dialog->proxy_dropdown = __make_proxy_dropdown();
	dialog->proxy_menu =
		gtk_option_menu_get_menu(GTK_OPTION_MENU(dialog->proxy_dropdown));

	__add_pref_box(dialog, vbox, _("Proxy _type:"), dialog->proxy_dropdown);

	/* Setup the second vbox, which may be hidden at times. */
	dialog->proxy_vbox = vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, 0);
	gtk_widget_show(vbox2);

	/* Host */
	dialog->proxy_host_entry = gtk_entry_new();
	__add_pref_box(dialog, vbox2, _("_Host:"), dialog->proxy_host_entry);

	/* Port */
	dialog->proxy_port_entry = gtk_entry_new();
	__add_pref_box(dialog, vbox2, _("_Port:"), dialog->proxy_port_entry);

	g_signal_connect(G_OBJECT(dialog->proxy_port_entry), "populate-popup",
					 G_CALLBACK(__port_popup_cb), NULL);

	/* User */
	dialog->proxy_user_entry = gtk_entry_new();

	__add_pref_box(dialog, vbox2, _("_Username:"), dialog->proxy_user_entry);

	/* Password */
	dialog->proxy_pass_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(dialog->proxy_pass_entry), FALSE);
	__add_pref_box(dialog, vbox2, _("Pa_ssword:"), dialog->proxy_pass_entry);

	if (dialog->account != NULL &&
		(proxy_info = gaim_account_get_proxy_info(dialog->account)) != NULL) {

		GaimProxyType type = gaim_proxy_info_get_type(proxy_info);

		/* Hah! */
		gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->proxy_dropdown),
									(int)type + 1);

		if (type == GAIM_PROXY_NONE || type == GAIM_PROXY_USE_GLOBAL) {
			gtk_widget_hide_all(vbox2);
		}
		else {
			const char *value;
			int int_val;

			if ((value = gaim_proxy_info_get_host(proxy_info)) != NULL)
				gtk_entry_set_text(GTK_ENTRY(dialog->proxy_host_entry), value);

			if ((int_val = gaim_proxy_info_get_port(proxy_info)) != 0) {
				char buf[32];

				g_snprintf(buf, sizeof(buf), "%d", int_val);

				gtk_entry_set_text(GTK_ENTRY(dialog->proxy_port_entry), buf);
			}

			if ((value = gaim_proxy_info_get_username(proxy_info)) != NULL)
				gtk_entry_set_text(GTK_ENTRY(dialog->proxy_user_entry), value);

			if ((value = gaim_proxy_info_get_password(proxy_info)) != NULL)
				gtk_entry_set_text(GTK_ENTRY(dialog->proxy_pass_entry), value);
		}
	}
	else
		gtk_widget_hide_all(vbox2);

	/* Connect signals. */
	g_signal_connect(G_OBJECT(dialog->proxy_dropdown), "changed",
					 G_CALLBACK(__proxy_type_changed_cb), dialog);
}

static void
__account_win_destroy_cb(GtkWidget *w, GdkEvent *event,
						 AccountPrefsDialog *dialog)
{
	if (dialog->user_split_entries != NULL)
		g_list_free(dialog->user_split_entries);

	if (dialog->protocol_opt_entries != NULL)
		g_list_free(dialog->protocol_opt_entries);

	g_free(dialog);
}

static void
__cancel_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	gtk_widget_destroy(dialog->window);

	__account_win_destroy_cb(NULL, NULL, dialog);
}

static void
__ok_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	GaimProxyInfo *proxy_info = NULL;
	GList *l, *l2;
	const char *value;
	char *username;
	char *tmp;
	size_t index;
	GtkTreeIter iter;

	if (dialog->account == NULL) {
		const char *screenname;

		screenname = gtk_entry_get_text(GTK_ENTRY(dialog->screenname_entry));

		dialog->account = gaim_account_new(screenname, dialog->protocol);
	}
	else {
		/* Protocol */
		gaim_account_set_protocol(dialog->account, dialog->protocol);
	}

	/* Alias */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->alias_entry));

	if (*value != '\0')
		gaim_account_set_alias(dialog->account, value);
	else
		gaim_account_set_alias(dialog->account, NULL);

	/* Buddy Icon */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->buddy_icon_entry));

	if ((dialog->prpl_info->options & OPT_PROTO_BUDDY_ICON) && *value != '\0')
		gaim_account_set_buddy_icon(dialog->account, value);
	else
		gaim_account_set_buddy_icon(dialog->account, NULL);

	/* Remember Password */
	gaim_account_set_remember_password(dialog->account,
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(dialog->remember_pass_check)));

	/* Check Mail */
	if (dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK)
		gaim_account_set_check_mail(dialog->account,
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(dialog->new_mail_check)));

	/* TODO: Do something about auto-login. */

	/* Password */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->password_entry));

	if (gaim_account_get_remember_password(dialog->account) && *value != '\0')
		gaim_account_set_password(dialog->account, value);
	else
		gaim_account_set_password(dialog->account, NULL);

	/* Build the username string. */
	username = g_strdup(gaim_account_get_username(dialog->account));

	for (l = dialog->prpl_info->user_splits, l2 = dialog->user_split_entries;
		 l != NULL && l2 != NULL;
		 l = l->next, l2 = l2->next) {

		GaimAccountUserSplit *split = l->data;
		GtkEntry *entry = l2->data;
		char sep[2] = " ";

		value = gtk_entry_get_text(entry);

		*sep = gaim_account_user_split_get_separator(split);

		tmp = g_strconcat(username, sep,
						  (*value ? value :
						   gaim_account_user_split_get_default_value(split)));

		g_free(username);
		username = tmp;
	}

	gaim_account_set_username(dialog->account, username);
	g_free(username);

	/* Add the protocol settings */
	gaim_account_clear_settings(dialog->account);

	for (l = dialog->prpl_info->protocol_options,
		 l2 = dialog->protocol_opt_entries;
		 l != NULL && l2 != NULL;
		 l = l->next, l2 = l2->next) {

		GaimPrefType type;
		GaimAccountOption *option = l->data;
		GtkWidget *widget = l2->data;
		const char *setting;
		int int_value;
		gboolean bool_value;

		type = gaim_account_option_get_type(option);

		setting = gaim_account_option_get_setting(option);

		switch (type) {
			case GAIM_PREF_STRING:
				value = gtk_entry_get_text(GTK_ENTRY(widget));
				gaim_account_set_string(dialog->account, setting, value);
				break;

			case GAIM_PREF_INT:
				int_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
				gaim_account_set_int(dialog->account, setting, int_value);
				break;

			case GAIM_PREF_BOOLEAN:
				bool_value =
					gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
				gaim_account_set_bool(dialog->account, setting, bool_value);
				break;

			default:
				break;
		}
	}

	/* Set the proxy stuff. */
	if (dialog->new_proxy_type == GAIM_PROXY_NONE) {
		gaim_account_set_proxy_info(dialog->account, NULL);
	}
	else {
		const char *port_str;

		proxy_info = gaim_account_get_proxy_info(dialog->account);

		if (proxy_info == NULL) {
			proxy_info = gaim_proxy_info_new();
			gaim_account_set_proxy_info(dialog->account, proxy_info);
		}

		gaim_proxy_info_set_type(proxy_info, dialog->new_proxy_type);

		gaim_proxy_info_set_host(proxy_info,
				gtk_entry_get_text(GTK_ENTRY(dialog->proxy_host_entry)));

		port_str = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_port_entry));

		if (port_str != NULL)
			gaim_proxy_info_set_port(proxy_info, atoi(port_str));
		else
			gaim_proxy_info_set_port(proxy_info, 0);

		gaim_proxy_info_set_username(proxy_info,
				gtk_entry_get_text(GTK_ENTRY(dialog->proxy_user_entry)));

		gaim_proxy_info_set_password(proxy_info,
				gtk_entry_get_text(GTK_ENTRY(dialog->proxy_pass_entry)));
	}

	/* Adds the account to the list, or modify the existing entry. */
	index = g_list_index(gaim_accounts_get_all(), dialog->account);

	if (index != -1 &&
		(gtk_tree_model_iter_nth_child(
				GTK_TREE_MODEL(dialog->accounts_dialog->model), &iter,
				NULL, index))) {

		__set_account(dialog->accounts_dialog->model, &iter, dialog->account);
	}
	else
		__add_account(dialog->accounts_dialog, dialog->account);

	/* See if we want to register with a server now. */
	if (dialog->prpl_info->register_user != NULL &&
		gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(dialog->register_check))) {

		dialog->prpl_info->register_user(dialog->account);
	}

	gtk_widget_destroy(dialog->window);

	__account_win_destroy_cb(NULL, NULL, dialog);

	gaim_accounts_sync();
}

static void
__show_account_prefs(AccountPrefsDialogType type,
					 AccountsDialog *accounts_dialog,
					 GaimAccount *account)
{
	AccountPrefsDialog *dialog;
	GtkWidget *win;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *dbox;
	GtkWidget *disclosure;
	GtkWidget *sep;
	GtkWidget *button;

	dialog = g_new0(AccountPrefsDialog, 1);

	dialog->accounts_dialog = accounts_dialog;
	dialog->account = account;
	dialog->type    = type;
	dialog->sg      = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if (dialog->account == NULL)
		dialog->protocol = GAIM_PROTO_OSCAR;
	else
		dialog->protocol = gaim_account_get_protocol(dialog->account);

	if ((dialog->plugin = gaim_find_prpl(dialog->protocol)) != NULL)
		dialog->prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(dialog->plugin);


	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(win), "account");

	if (type == ADD_ACCOUNT_DIALOG)
		gtk_window_set_title(GTK_WINDOW(win), _("Add Account"));
	else
		gtk_window_set_title(GTK_WINDOW(win), _("Modify Account"));

	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);

	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__account_win_destroy_cb), dialog);

	/* Setup the vbox */
	main_vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), main_vbox);
	gtk_widget_show(main_vbox);

	/* Setup the inner vbox */
	dialog->top_vbox = vbox = gtk_vbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(main_vbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);

	/* Setup the top frames. */
	__add_login_options(dialog, vbox);
	__add_user_options(dialog, vbox);

	/* Add the disclosure */
	disclosure = gaim_disclosure_new(_("Show more options"),
									 _("Show fewer options"));
	gtk_box_pack_start(GTK_BOX(vbox), disclosure, FALSE, FALSE, 0);
	gtk_widget_show(disclosure);

	/* Setup the box that the disclosure will cover. */
	dialog->bottom_vbox = dbox = gtk_vbox_new(FALSE, 18);
	gtk_box_pack_start(GTK_BOX(vbox), dbox, FALSE, FALSE, 0);

	gaim_disclosure_set_container(GAIM_DISCLOSURE(disclosure), dbox);

	/** Setup the bottom frames. */
	__add_protocol_options(dialog, dbox);
	__add_proxy_options(dialog, dbox);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Setup the button box */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(main_vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__cancel_account_prefs_cb), dialog);

	/* OK button */
	button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);

	gtk_widget_show(button);

	dialog->ok_button = button;

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__ok_account_prefs_cb), dialog);

	/* Show the window. */
	gtk_widget_show(win);
}

/**************************************************************************
 * Accounts Dialog
 **************************************************************************/

static void
__signed_on_off_cb(GaimConnection *gc, AccountsDialog *dialog)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	size_t index = g_list_index(gaim_accounts_get_all(), account);

	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index)) {
		gtk_list_store_set(dialog->model, &iter,
						   COLUMN_ONLINE, gaim_account_is_connected(account),
						   -1);
	}
}

static void
__drag_data_get_cb(GtkWidget *widget, GdkDragContext *ctx,
				   GtkSelectionData *data, guint info, guint time,
				   AccountsDialog *dialog)
{
	if (data->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE)) {
		GtkTreeRowReference *ref;
		GtkTreePath *source_row;
		GtkTreeIter iter;
		GaimAccount *account = NULL;
		GValue val = {0};

		ref = g_object_get_data(G_OBJECT(ctx), "gtk-tree-view-source-row");
		source_row = gtk_tree_row_reference_get_path(ref);

		if (source_row == NULL)
			return;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter,
								source_row);
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
								 COLUMN_DATA, &val);

		dialog->drag_iter = iter;

		account = g_value_get_pointer(&val);

		gtk_selection_data_set(data, gdk_atom_intern("GAIM_ACCOUNT", FALSE),
							   8, (void *)&account, sizeof(account));

		gtk_tree_path_free(source_row);
	}
}

static void
__drag_data_received_cb(GtkWidget *widget, GdkDragContext *ctx,
						guint x, guint y, GtkSelectionData *sd,
						guint info, guint t, AccountsDialog *dialog)
{
	if (sd->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE) && sd->data) {
		size_t dest_index;
		GaimAccount *a = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		memcpy(&a, sd->data, sizeof(a));

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y,
											  &path, &position)) {

			GtkTreeIter iter;
			GaimAccount *account;
			GValue val = {0};

			gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
			gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
									 COLUMN_DATA, &val);

			account = g_value_get_pointer(&val);

			switch (position) {
				case GTK_TREE_VIEW_DROP_AFTER:
				case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "after\n");
					gtk_list_store_move_after(dialog->model,
											  &dialog->drag_iter, &iter);
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account) + 1;
					break;

				case GTK_TREE_VIEW_DROP_BEFORE:
				case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "before\n");
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account);

					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "iter = %p\n", &iter);
					gaim_debug(GAIM_DEBUG_MISC, "gtkaccount",
							   "account = %s\n",
							   gaim_account_get_username(account));

					/*
					 * Somebody figure out why inserting before the first
					 * account sometimes moves to the end, please :(
					 */
					if (dest_index == 0) {
						gtk_list_store_move_after(dialog->model,
												  &dialog->drag_iter, &iter);
						gtk_list_store_swap(dialog->model, &iter,
											&dialog->drag_iter);
					}
					else {
						gtk_list_store_move_before(dialog->model,
												   &dialog->drag_iter, &iter);
					}

					break;

				default:
					return;
			}

			gaim_accounts_reorder(a, dest_index);
		}
	}
}

static gint
__accedit_win_destroy_cb(GtkWidget *w, GdkEvent *event, AccountsDialog *dialog)
{
	g_free(accounts_dialog);
	accounts_dialog = NULL;

	/* See if we're the main window here. */
	if (GAIM_GTK_BLIST(gaim_get_blist())->window == NULL &&
		mainwindow == NULL && gaim_connections_get_all() == NULL) {

		do_quit();
	}

	return FALSE;
}

static gboolean
__configure_cb(GtkWidget *w, GdkEventConfigure *event, AccountsDialog *dialog)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		int old_width = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
		int col_width;
		int difference;

		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/height", event->height);

		col_width = gtk_tree_view_column_get_width(dialog->screenname_col);

		if (col_width == 0)
			return FALSE;

		difference = (MAX(old_width, event->width) -
					  MIN(old_width, event->width));

		if (difference == 0)
			return FALSE;

		if (old_width < event->width)
			gtk_tree_view_column_set_min_width(dialog->screenname_col,
					col_width + difference);
		else
			gtk_tree_view_column_set_max_width(dialog->screenname_col,
					col_width - difference);
	}

	return FALSE;
}

static void
__add_account_cb(GtkWidget *w, AccountsDialog *dialog)
{
	__show_account_prefs(ADD_ACCOUNT_DIALOG, dialog, NULL);
}

static void
__modify_account_sel(GtkTreeModel *model, GtkTreePath *path,
					 GtkTreeIter *iter, gpointer data)
{
	GaimAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL)
		__show_account_prefs(MODIFY_ACCOUNT_DIALOG, data, account);
}

static void
__modify_account_cb(GtkWidget *w, AccountsDialog *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, __modify_account_sel,
										dialog);
}

static void
__delete_account_cb(GaimAccount *account)
{
	size_t index;
	GtkTreeIter iter;

	index = g_list_index(gaim_accounts_get_all(), account);

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(accounts_dialog->model),
									  &iter, NULL, index)) {

		gtk_list_store_remove(accounts_dialog->model, &iter);
	}

	gaim_account_destroy(account);
}

static void
__ask_delete_account_sel(GtkTreeModel *model, GtkTreePath *path,
						 GtkTreeIter *iter, gpointer data)
{
	GaimAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL) {
		char buf[8192];

		g_snprintf(buf, sizeof(buf),
				   _("Are you sure you want to delete %s?"),
				   gaim_account_get_username(account));

		gaim_request_action(NULL, NULL, buf, NULL, 1, account, 2,
							_("Delete"), __delete_account_cb,
							_("Cancel"), NULL);
	}
}

static void
__ask_delete_account_cb(GtkWidget *w, AccountsDialog *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, __ask_delete_account_sel,
										dialog);
}

static void
__close_accounts_cb(GtkWidget *w, AccountsDialog *dialog)
{
	gtk_widget_destroy(dialog->window);

	__accedit_win_destroy_cb(NULL, NULL, dialog);
}

static void
__online_cb(GtkCellRendererToggle *renderer, gchar *path_str, gpointer data)
{
	AccountsDialog *dialog = (AccountsDialog *)data;
	GaimAccount *account;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	gboolean online;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter, COLUMN_DATA, &account,
					   COLUMN_ONLINE, &online, -1);

	if (online)
		gaim_account_disconnect(account);
	else
		gaim_account_connect(account);
}

static void
__autologin_cb(GtkCellRendererToggle *renderer, gchar *path_str,
			   gpointer data)
{
	
}

static void
__add_columns(GtkWidget *treeview, AccountsDialog *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Protocol */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Protocol"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Icon text */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "pixbuf", COLUMN_ICON);

	/* Protocol name */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "text", COLUMN_PROTOCOL);

	/* Screennames */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Screenname"),
				renderer, "text", COLUMN_SCREENNAME, NULL);
	dialog->screenname_col = column;

	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);

	/* Online? */
	renderer = gtk_cell_renderer_toggle_new();
	
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(__online_cb), dialog);
	
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
						    -1, _("Online"),
						    renderer,
						    "active", COLUMN_ONLINE,
						    NULL);

	/* Auto-login? */
	renderer = gtk_cell_renderer_toggle_new();

	g_signal_connect(G_OBJECT(renderer), "toggled",
					 G_CALLBACK(__autologin_cb), dialog);

	column = gtk_tree_view_column_new_with_attributes(_("Auto-login"),
			renderer, "active", COLUMN_AUTOLOGIN, NULL);

	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);
	g_object_set(renderer, "xalign", 0.0, "xpad", 10, NULL);


}

static void
__set_account(GtkListStore *store, GtkTreeIter *iter, GaimAccount *account)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scale;

	scale = NULL;

	pixbuf = create_prpl_icon(account);

	if (pixbuf != NULL)
		scale = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);

	gtk_list_store_set(store, iter,
			COLUMN_ICON, scale,
			COLUMN_SCREENNAME, gaim_account_get_username(account),
			COLUMN_ONLINE, gaim_account_is_connected(account),
			COLUMN_AUTOLOGIN, FALSE,
			COLUMN_PROTOCOL, proto_name(gaim_account_get_protocol(account)),
			COLUMN_DATA, account,
			COLUMN_FILLER, FALSE,
			-1);

	if (pixbuf != NULL) g_object_unref(G_OBJECT(pixbuf));
	if (scale  != NULL) g_object_unref(G_OBJECT(scale));
}

static void
__add_account(AccountsDialog *dialog, GaimAccount *account)
{
	GtkTreeIter iter;

	gtk_list_store_append(dialog->model, &iter);

	__set_account(dialog->model, &iter, account);
}

static void
__populate_accounts_list(AccountsDialog *dialog)
{
	GList *l;

	gtk_list_store_clear(dialog->model);

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next)
		__add_account(dialog, (GaimAccount *)l->data);
}

static void
__account_selected_cb(GtkTreeSelection *sel, AccountsDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->modify_button, TRUE);
	gtk_widget_set_sensitive(dialog->delete_button, TRUE);
}

static GtkWidget *
__create_accounts_list(AccountsDialog *dialog)
{
	GtkWidget *sw;
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTargetEntry gte[] = {{"GAIM_ACCOUNT", GTK_TARGET_SAME_APP, 0}};

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
										GTK_SHADOW_IN);
	gtk_widget_show(sw);

	/* Create the list model. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF,
									   G_TYPE_STRING, G_TYPE_STRING,
									   G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
									   G_TYPE_BOOLEAN, G_TYPE_POINTER);

	/* And now the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_selection_set_mode(
			gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
			GTK_SELECTION_MULTIPLE);

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	__add_columns(treeview, dialog);

	__populate_accounts_list(dialog);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(__account_selected_cb), dialog);

	/* Setup DND. I wanna be an orc! */
	gtk_tree_view_enable_model_drag_source(
			GTK_TREE_VIEW(treeview), GDK_BUTTON1_MASK, gte,
			2, GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(
			GTK_TREE_VIEW(treeview), gte, 2,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
					 G_CALLBACK(__drag_data_received_cb), dialog);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
					 G_CALLBACK(__drag_data_get_cb), dialog);

	return sw;
}

void
gaim_gtk_account_dialog_show(void)
{
	AccountsDialog *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	int width, height;

	if (accounts_dialog != NULL)
		return;

	accounts_dialog = dialog = g_new0(AccountsDialog, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/height");

	GAIM_DIALOG(win);

	dialog->window = win;

	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "accounts");
	gtk_window_set_title(GTK_WINDOW(win), _("Accounts"));
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__accedit_win_destroy_cb), accounts_dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(__configure_cb), accounts_dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = __create_accounts_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__add_account_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(GAIM_STOCK_MODIFY);
	dialog->modify_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__modify_account_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	dialog->delete_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__ask_delete_account_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(__close_accounts_cb), dialog);

	/* Setup some gaim signal handlers. */
	gaim_signal_connect(dialog, event_signon,  __signed_on_off_cb, dialog);
	gaim_signal_connect(dialog, event_signoff, __signed_on_off_cb, dialog);

	gtk_widget_show(win);
}


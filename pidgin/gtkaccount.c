/**
 * @file gtkaccount.c GTK+ Account Editor UI
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#include "internal.h"
#include "pidgin.h"

#include "account.h"
#include "accountopt.h"
#include "core.h"
#include "debug.h"
#include "notify.h"
#include "plugin.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "signals.h"
#include "util.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "gtkstatusbox.h"
#include "pidginstock.h"
#include "minidialog.h"

#include "gtk3compat.h"

enum
{
	COLUMN_ICON,
	COLUMN_BUDDYICON,
	COLUMN_USERNAME,
	COLUMN_ENABLED,
	COLUMN_PROTOCOL,
	COLUMN_DATA,
	NUM_COLUMNS
};

typedef struct
{
	PurpleAccount *account;
	char *username;
	char *alias;

} PidginAccountAddUserData;

typedef struct
{
	GtkWidget *window;
	GtkWidget *treeview;

	GtkWidget *modify_button;
	GtkWidget *delete_button;
	GtkWidget *notebook;

	GtkListStore *model;
	GtkTreeIter drag_iter;

	GtkTreeViewColumn *username_col;

} AccountsWindow;

typedef struct
{
	GtkWidget *widget;
	gchar *setting;
	PurplePrefType type;
} ProtocolOptEntry;

typedef struct
{
	PidginAccountDialogType type;

	PurpleAccount *account;
	char *protocol_id;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prpl_info;

	PurpleProxyType new_proxy_type;

	GList *user_split_entries;
	GList *protocol_opt_entries;

	GtkSizeGroup *sg;
	GtkWidget *window;

	GtkWidget *notebook;
	GtkWidget *top_vbox;
	GtkWidget *ok_button;
	GtkWidget *register_button;

	/* Login Options */
	GtkWidget *login_frame;
	GtkWidget *protocol_menu;
	GtkWidget *password_box;
	GtkWidget *username_entry;
	GtkWidget *password_entry;
	GtkWidget *alias_entry;
	GtkWidget *remember_pass_check;

	/* User Options */
	GtkWidget *user_frame;
	GtkWidget *new_mail_check;
	GtkWidget *icon_hbox;
	GtkWidget *icon_check;
	GtkWidget *icon_entry;
	GtkWidget *icon_filesel;
	GtkWidget *icon_preview;
	GtkWidget *icon_text;
	PurpleStoredImage *icon_img;

	/* Protocol Options */
	GtkWidget *protocol_frame;

	/* Proxy Options */
	GtkWidget *proxy_frame;
	GtkWidget *proxy_vbox;
	GtkWidget *proxy_dropdown;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_entry;
	GtkWidget *proxy_user_entry;
	GtkWidget *proxy_pass_entry;

	/* Voice & Video Options*/
	GtkWidget *voice_frame;
	GtkWidget *suppression_check;

} AccountPrefsDialog;

static AccountsWindow *accounts_window = NULL;
static GHashTable *account_pref_wins;

static void add_account_to_liststore(PurpleAccount *account, gpointer user_data);
static void set_account(GtkListStore *store, GtkTreeIter *iter,
						  PurpleAccount *account, GdkPixbuf *global_buddyicon);

/**************************************************************************
 * Add/Modify Account dialog
 **************************************************************************/
static void add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_protocol_options(AccountPrefsDialog *dialog);
static void add_proxy_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_voice_options(AccountPrefsDialog *dialog);

static GtkWidget *
add_pref_box(AccountPrefsDialog *dialog, GtkWidget *parent,
			 const char *text, GtkWidget *widget)
{
	return pidgin_add_widget_to_vbox(GTK_BOX(parent), text, dialog->sg, widget, TRUE, NULL);
}

static void
set_dialog_icon(AccountPrefsDialog *dialog, gpointer data, size_t len, gchar *new_icon_path)
{
	GdkPixbuf *pixbuf = NULL;

	dialog->icon_img = purple_imgstore_unref(dialog->icon_img);
	if (data != NULL)
	{
		if (len > 0)
			dialog->icon_img = purple_imgstore_new(data, len, new_icon_path);
		else
			g_free(data);
	}

	if (dialog->icon_img != NULL) {
		pixbuf = pidgin_pixbuf_from_imgstore(dialog->icon_img);
	}

	if (pixbuf && dialog->prpl_info &&
	    (dialog->prpl_info->icon_spec.scale_rules & PURPLE_ICON_SCALE_DISPLAY))
	{
		/* Scale the icon to something reasonable */
		int width, height;
		GdkPixbuf *scale;

		pidgin_buddy_icon_get_scale_size(pixbuf, &dialog->prpl_info->icon_spec,
				PURPLE_ICON_SCALE_DISPLAY, &width, &height);
		scale = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);

		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = scale;
	}

	if (pixbuf == NULL)
	{
		/* Show a placeholder icon */
		GtkIconSize icon_size = gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		pixbuf = gtk_widget_render_icon(dialog->window, PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR,
		                                icon_size, "PidginAccount");
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_entry), pixbuf);
	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
}

static void
set_account_protocol_cb(GtkWidget *widget, const char *id,
						AccountPrefsDialog *dialog)
{
	PurplePlugin *new_plugin;

	new_plugin = purple_find_prpl(id);

	dialog->plugin = new_plugin;

	if (dialog->plugin != NULL)
	{
		dialog->prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(dialog->plugin);

		g_free(dialog->protocol_id);
		dialog->protocol_id = g_strdup(dialog->plugin->info->id);
	}

	if (dialog->account != NULL)
		purple_account_clear_settings(dialog->account);

	add_login_options(dialog,    dialog->top_vbox);
	add_user_options(dialog,     dialog->top_vbox);
	add_protocol_options(dialog);
	add_voice_options(dialog);

	gtk_widget_grab_focus(dialog->protocol_menu);

	if (!dialog->prpl_info || !dialog->prpl_info->register_user) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
			dialog->register_button), FALSE);
		gtk_widget_hide(dialog->register_button);
	} else {
		if (dialog->prpl_info != NULL &&
		   (dialog->prpl_info->options & OPT_PROTO_REGISTER_NOSCREENNAME)) {
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
				dialog->register_button), FALSE);
			gtk_widget_set_sensitive(dialog->register_button, FALSE);
		}
		gtk_widget_show(dialog->register_button);
	}
}

static gboolean
username_focus_cb(GtkWidget *widget, GdkEventFocus *event, AccountPrefsDialog *dialog)
{
	GHashTable *table;
	const char *label;

	if (!dialog->prpl_info || ! PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(
		dialog->prpl_info, get_account_text_table)) {
		return FALSE;
	}

	table = dialog->prpl_info->get_account_text_table(NULL);
	label = g_hash_table_lookup(table, "login_label");

	if(!strcmp(gtk_entry_get_text(GTK_ENTRY(widget)), label)) {
		gtk_entry_set_text(GTK_ENTRY(widget), "");
		gtk_widget_modify_text(widget, GTK_STATE_NORMAL,NULL);
	}

	g_hash_table_destroy(table);

	return FALSE;
}

static void
username_changed_cb(GtkEntry *entry, AccountPrefsDialog *dialog)
{
	gboolean opt_noscreenname = (dialog->prpl_info != NULL &&
		(dialog->prpl_info->options & OPT_PROTO_REGISTER_NOSCREENNAME));
	gboolean username_valid = purple_validate(dialog->plugin,
		gtk_entry_get_text(entry));

	if (dialog->ok_button) {
		if (opt_noscreenname && dialog->register_button &&
			gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(dialog->register_button)))
			gtk_widget_set_sensitive(dialog->ok_button, TRUE);
		else
			gtk_widget_set_sensitive(dialog->ok_button,
				username_valid);
	}

	if (dialog->register_button) {
		if (opt_noscreenname)
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		else
			gtk_widget_set_sensitive(dialog->register_button,
				username_valid);
	}
}

static gboolean
username_nofocus_cb(GtkWidget *widget, GdkEventFocus *event, AccountPrefsDialog *dialog)
{
	GdkColor color = {0, 34952, 35466, 34181};
	GHashTable *table = NULL;
	const char *label = NULL;

	if(PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(dialog->prpl_info, get_account_text_table)) {
		table = dialog->prpl_info->get_account_text_table(NULL);
		label = g_hash_table_lookup(table, "login_label");

		if (*gtk_entry_get_text(GTK_ENTRY(widget)) == '\0') {
			/* We have to avoid hitting the username_changed_cb function
			 * because it enables buttons we don't want enabled yet ;)
			 */
			g_signal_handlers_block_by_func(widget, G_CALLBACK(username_changed_cb), dialog);
			gtk_entry_set_text(GTK_ENTRY(widget), label);
			/* Make sure we can hit it again */
			g_signal_handlers_unblock_by_func(widget, G_CALLBACK(username_changed_cb), dialog);
			gtk_widget_modify_text(widget, GTK_STATE_NORMAL, &color);
		}

		g_hash_table_destroy(table);
	}

	return FALSE;
}

static void
register_button_cb(GtkWidget *checkbox, AccountPrefsDialog *dialog)
{
	int register_checked = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(dialog->register_button));
	int opt_noscreenname = (dialog->prpl_info != NULL &&
		(dialog->prpl_info->options & OPT_PROTO_REGISTER_NOSCREENNAME));
	int register_noscreenname = (opt_noscreenname && register_checked);

	/* get rid of login_label in username field */
	username_focus_cb(dialog->username_entry, NULL, dialog);

	if (register_noscreenname) {
		gtk_entry_set_text(GTK_ENTRY(dialog->username_entry), "");
		gtk_entry_set_text(GTK_ENTRY(dialog->password_entry), "");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->remember_pass_check), FALSE);
	}
	gtk_widget_set_sensitive(dialog->username_entry, !register_noscreenname);
	gtk_widget_set_sensitive(dialog->password_entry, !register_noscreenname);
	gtk_widget_set_sensitive(dialog->remember_pass_check, !register_noscreenname);

	if (dialog->ok_button) {
		gtk_widget_set_sensitive(dialog->ok_button,
			(opt_noscreenname && register_checked) ||
			*gtk_entry_get_text(GTK_ENTRY(dialog->username_entry))
				!= '\0');
	}

	username_nofocus_cb(dialog->username_entry, NULL, dialog);
}

static void
icon_filesel_choose_cb(const char *filename, gpointer data)
{
	AccountPrefsDialog *dialog = data;

	if (filename != NULL)
	{
		size_t len;
		gpointer data = pidgin_convert_buddy_icon(dialog->plugin, filename, &len);
		set_dialog_icon(dialog, data, len, g_strdup(filename));
	}

	dialog->icon_filesel = NULL;
}

static void
icon_select_cb(GtkWidget *button, AccountPrefsDialog *dialog)
{
	dialog->icon_filesel = pidgin_buddy_icon_chooser_new(GTK_WINDOW(dialog->window), icon_filesel_choose_cb, dialog);
	gtk_widget_show_all(dialog->icon_filesel);
}

static void
icon_reset_cb(GtkWidget *button, AccountPrefsDialog *dialog)
{
	set_dialog_icon(dialog, NULL, 0, NULL);
}

static void
account_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
		 GtkSelectionData *sd, guint info, guint t, AccountPrefsDialog *dialog)
{
	const gchar *name = (gchar *)gtk_selection_data_get_data(sd);
	gint length = gtk_selection_data_get_length(sd);
	gint format = gtk_selection_data_get_format(sd);

	if ((length >= 0) && (format == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp, *rtmp;
			gpointer data;
			size_t len;

			/* It looks like we're dealing with a local file. */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug(PURPLE_DEBUG_ERROR, "buddyicon", "%s\n",
					   (converr ? converr->message :
					    "g_filename_from_uri error"));
				return;
			}
			if ((rtmp = strchr(tmp, '\r')) || (rtmp = strchr(tmp, '\n')))
				*rtmp = '\0';

			data = pidgin_convert_buddy_icon(dialog->plugin, tmp, &len);
			/* This takes ownership of tmp */
			set_dialog_icon(dialog, data, len, tmp);
		}
		gtk_drag_finish(dc, TRUE, FALSE, t);
	}
	gtk_drag_finish(dc, FALSE, FALSE, t);
}

static void
update_editable(PurpleConnection *gc, AccountPrefsDialog *dialog)
{
	GtkStyle *style;
	gboolean set;
	GList *l;

	if (dialog->account == NULL)
		return;

	if (gc != NULL && dialog->account != purple_connection_get_account(gc))
		return;

	set = !(purple_account_is_connected(dialog->account) || purple_account_is_connecting(dialog->account));
	gtk_widget_set_sensitive(dialog->protocol_menu, set);
	gtk_editable_set_editable(GTK_EDITABLE(dialog->username_entry), set);
	style = set ? NULL : gtk_widget_get_style(dialog->username_entry);
	gtk_widget_modify_base(dialog->username_entry, GTK_STATE_NORMAL,
			style ? &style->base[GTK_STATE_INSENSITIVE] : NULL);

	for (l = dialog->user_split_entries ; l != NULL ; l = l->next) {
		if (GTK_IS_EDITABLE(l->data)) {
			gtk_editable_set_editable(GTK_EDITABLE(l->data), set);
			style = set ? NULL : gtk_widget_get_style(GTK_WIDGET(l->data));
			gtk_widget_modify_base(GTK_WIDGET(l->data), GTK_STATE_NORMAL,
					style ? &style->base[GTK_STATE_INSENSITIVE] : NULL);
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(l->data), set);
		}
	}
}

static void
add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *entry;
	GList *user_splits;
	GList *l, *l2;
	char *username = NULL;

	if (dialog->protocol_menu != NULL)
	{
		g_object_ref(G_OBJECT(dialog->protocol_menu));
		hbox = g_object_get_data(G_OBJECT(dialog->protocol_menu), "container");
		gtk_container_remove(GTK_CONTAINER(hbox), dialog->protocol_menu);
	}

	if (dialog->login_frame != NULL)
		gtk_widget_destroy(dialog->login_frame);

	/* Build the login options frame. */
	frame = pidgin_make_frame(parent, _("Login Options"));

	/* cringe */
	dialog->login_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->login_frame, 0);
	gtk_widget_show(dialog->login_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Protocol */
	if (dialog->protocol_menu == NULL)
	{
		dialog->protocol_menu = pidgin_protocol_option_menu_new(
				dialog->protocol_id, G_CALLBACK(set_account_protocol_cb), dialog);
		g_object_ref(G_OBJECT(dialog->protocol_menu));
	}

	hbox = add_pref_box(dialog, vbox, _("Pro_tocol:"), dialog->protocol_menu);
	g_object_set_data(G_OBJECT(dialog->protocol_menu), "container", hbox);

	g_object_unref(G_OBJECT(dialog->protocol_menu));

	/* Username */
	dialog->username_entry = gtk_entry_new();
	g_object_set(G_OBJECT(dialog->username_entry), "truncate-multiline", TRUE, NULL);

	add_pref_box(dialog, vbox, _("_Username:"), dialog->username_entry);

	if (dialog->account != NULL)
		username = g_strdup(purple_account_get_username(dialog->account));

	if (!username && dialog->prpl_info
			&& PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(dialog->prpl_info, get_account_text_table)) {
		GdkColor color = {0, 34952, 35466, 34181};
		GHashTable *table;
		const char *label;
		table = dialog->prpl_info->get_account_text_table(NULL);
		label = g_hash_table_lookup(table, "login_label");

		gtk_entry_set_text(GTK_ENTRY(dialog->username_entry), label);
		g_signal_connect(G_OBJECT(dialog->username_entry), "focus-in-event",
				G_CALLBACK(username_focus_cb), dialog);
		g_signal_connect(G_OBJECT(dialog->username_entry), "focus-out-event",
				G_CALLBACK(username_nofocus_cb), dialog);
		gtk_widget_modify_text(dialog->username_entry, GTK_STATE_NORMAL, &color);
		g_hash_table_destroy(table);
	}

	g_signal_connect(G_OBJECT(dialog->username_entry), "changed",
					 G_CALLBACK(username_changed_cb), dialog);

	/* Do the user split thang */
	if (dialog->prpl_info == NULL)
		user_splits = NULL;
	else
		user_splits = dialog->prpl_info->user_splits;

	if (dialog->user_split_entries != NULL) {
		g_list_free(dialog->user_split_entries);
		dialog->user_split_entries = NULL;
	}

	for (l = user_splits; l != NULL; l = l->next) {
		PurpleAccountUserSplit *split = l->data;
		char *buf;

		buf = g_strdup_printf("_%s:", purple_account_user_split_get_text(split));

		entry = gtk_entry_new();

		add_pref_box(dialog, vbox, buf, entry);

		g_free(buf);

		dialog->user_split_entries =
			g_list_append(dialog->user_split_entries, entry);
	}

	for (l = g_list_last(dialog->user_split_entries),
		 l2 = g_list_last(user_splits);
		 l != NULL && l2 != NULL;
		 l = l->prev, l2 = l2->prev) {

		GtkWidget *entry = l->data;
		PurpleAccountUserSplit *split = l2->data;
		const char *value = NULL;
		char *c;

		if (dialog->account != NULL) {
			if(purple_account_user_split_get_reverse(split))
				c = strrchr(username,
						purple_account_user_split_get_separator(split));
			else
				c = strchr(username,
						purple_account_user_split_get_separator(split));

			if (c != NULL) {
				*c = '\0';
				c++;

				value = c;
			}
		}
		if (value == NULL)
			value = purple_account_user_split_get_default_value(split);

		if (value != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), value);
	}

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(dialog->username_entry), username);

	g_free(username);


	/* Password */
	dialog->password_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(dialog->password_entry), FALSE);
#if !GTK_CHECK_VERSION(2,16,0)
	if (gtk_entry_get_invisible_char(GTK_ENTRY(dialog->password_entry)) == '*')
		gtk_entry_set_invisible_char(GTK_ENTRY(dialog->password_entry), PIDGIN_INVISIBLE_CHAR);
#endif /* Less than GTK+ 2.16 */
	dialog->password_box = add_pref_box(dialog, vbox, _("_Password:"),
										  dialog->password_entry);

	/* Remember Password */
	dialog->remember_pass_check =
		gtk_check_button_new_with_mnemonic(_("Remember pass_word"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->remember_pass_check),
								 FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->remember_pass_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->remember_pass_check);

	/* Set the fields. */
	if (dialog->account != NULL) {
		if (purple_account_get_password(dialog->account) &&
		    purple_account_get_remember_password(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->password_entry),
							   purple_account_get_password(dialog->account));

		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(dialog->remember_pass_check),
				purple_account_get_remember_password(dialog->account));
	}

	if (dialog->prpl_info != NULL &&
		(dialog->prpl_info->options & OPT_PROTO_NO_PASSWORD)) {

		gtk_widget_hide(dialog->password_box);
		gtk_widget_hide(dialog->remember_pass_check);
	}

	/* Do not let the user change the protocol/username while connected. */
	update_editable(NULL, dialog);
	purple_signal_connect(purple_connections_get_handle(), "signing-on", dialog,
					G_CALLBACK(update_editable), dialog);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", dialog,
					G_CALLBACK(update_editable), dialog);
}

static void
icon_check_cb(GtkWidget *checkbox, AccountPrefsDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->icon_hbox, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));
}

static void
add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *hbox2;
	GtkWidget *button;
	GtkWidget *label;

	if (dialog->user_frame != NULL)
		gtk_widget_destroy(dialog->user_frame);

	/* Build the user options frame. */
	frame = pidgin_make_frame(parent, _("User Options"));
	dialog->user_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->user_frame, 1);
	gtk_widget_show(dialog->user_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Alias */
	dialog->alias_entry = gtk_entry_new();
	add_pref_box(dialog, vbox, _("_Local alias:"), dialog->alias_entry);

	/* New mail notifications */
	dialog->new_mail_check =
		gtk_check_button_new_with_mnemonic(_("New _mail notifications"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->new_mail_check, FALSE, FALSE, 0);
	gtk_widget_show(dialog->new_mail_check);

	/* Buddy icon */
	dialog->icon_check = gtk_check_button_new_with_mnemonic(_("Use this buddy _icon for this account:"));
	g_signal_connect(G_OBJECT(dialog->icon_check), "toggled", G_CALLBACK(icon_check_cb), dialog);
	gtk_widget_show(dialog->icon_check);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->icon_check, FALSE, FALSE, 0);

	dialog->icon_hbox = hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_widget_set_sensitive(hbox, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	button = gtk_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(icon_select_cb), dialog);

	dialog->icon_entry = gtk_image_new();
	gtk_container_add(GTK_CONTAINER(button), dialog->icon_entry);
	gtk_widget_show(dialog->icon_entry);
	/* TODO: Uh, isn't this next line pretty useless? */
	pidgin_set_accessible_label (dialog->icon_entry, label);
	purple_imgstore_unref(dialog->icon_img);
	dialog->icon_img = NULL;

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	hbox2 = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, PIDGIN_HIG_BORDER);
	gtk_widget_show(hbox2);

	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(icon_reset_cb), dialog);
	gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	if (dialog->prpl_info != NULL) {
		if (!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK))
			gtk_widget_hide(dialog->new_mail_check);

		if (dialog->prpl_info->icon_spec.format == NULL) {
			gtk_widget_hide(dialog->icon_check);
			gtk_widget_hide(dialog->icon_hbox);
		}
	}

	if (dialog->account != NULL) {
		PurpleStoredImage *img;
		gpointer data = NULL;
		size_t len = 0;

		if (purple_account_get_alias(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->alias_entry),
							   purple_account_get_alias(dialog->account));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->new_mail_check),
					     purple_account_get_check_mail(dialog->account));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->icon_check),
					     !purple_account_get_bool(dialog->account, "use-global-buddyicon",
								       TRUE));

		img = purple_buddy_icons_find_account_icon(dialog->account);
		if (img)
		{
			len = purple_imgstore_get_size(img);
			data = g_memdup(purple_imgstore_get_data(img), len);
		}
		set_dialog_icon(dialog, data, len,
		                g_strdup(purple_account_get_buddy_icon_path(dialog->account)));
	} else {
		set_dialog_icon(dialog, NULL, 0, NULL);
	}

#if 0
	if (!dialog->prpl_info ||
			(!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK) &&
			 (dialog->prpl_info->icon_spec.format ==  NULL))) {

		/* Nothing to see :( aww. */
		gtk_widget_hide(dialog->user_frame);
	}
#endif
}

static void
add_protocol_options(AccountPrefsDialog *dialog)
{
	PurpleAccountOption *option;
	PurpleAccount *account;
	GtkWidget *vbox, *check, *entry, *combo;
	GList *list, *node;
	gint i, idx, int_value;
	GtkListStore *model;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	PurpleKeyValuePair *kvp;
	GList *l;
	char buf[1024];
	char *title, *tmp;
	const char *str_value;
	gboolean bool_value;
	ProtocolOptEntry *opt_entry;
	const GSList *str_hints;

	if (dialog->protocol_frame != NULL) {
		gtk_notebook_remove_page (GTK_NOTEBOOK(dialog->notebook), 1);
		dialog->protocol_frame = NULL;
	}

	while (dialog->protocol_opt_entries != NULL) {
		ProtocolOptEntry *opt_entry = dialog->protocol_opt_entries->data;
		g_free(opt_entry->setting);
		g_free(opt_entry);
		dialog->protocol_opt_entries = g_list_delete_link(dialog->protocol_opt_entries, dialog->protocol_opt_entries);
	}

	if (dialog->prpl_info == NULL ||
			dialog->prpl_info->protocol_options == NULL)
		return;

	account = dialog->account;

	/* Main vbox */
	dialog->protocol_frame = vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PIDGIN_HIG_BORDER);
	gtk_notebook_insert_page(GTK_NOTEBOOK(dialog->notebook), vbox,
			gtk_label_new_with_mnemonic(_("Ad_vanced")), 1);
	gtk_widget_show(vbox);

	for (l = dialog->prpl_info->protocol_options; l != NULL; l = l->next)
	{
		option = (PurpleAccountOption *)l->data;

		opt_entry = g_new0(ProtocolOptEntry, 1);
		opt_entry->type = purple_account_option_get_type(option);
		opt_entry->setting = g_strdup(purple_account_option_get_setting(option));

		switch (opt_entry->type)
		{
			case PURPLE_PREF_BOOLEAN:
				if (account == NULL ||
					strcmp(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					bool_value = purple_account_option_get_default_bool(option);
				}
				else
				{
					bool_value = purple_account_get_bool(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_bool(option));
				}

				tmp = g_strconcat("_", purple_account_option_get_text(option), NULL);
				opt_entry->widget = check = gtk_check_button_new_with_mnemonic(tmp);
				g_free(tmp);

				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
											 bool_value);

				gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);
				gtk_widget_show(check);
				break;

			case PURPLE_PREF_INT:
				if (account == NULL ||
					strcmp(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					int_value = purple_account_option_get_default_int(option);
				}
				else
				{
					int_value = purple_account_get_int(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_int(option));
				}

				g_snprintf(buf, sizeof(buf), "%d", int_value);

				opt_entry->widget = entry = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(entry), buf);

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, entry);
				g_free(title);
				break;

			case PURPLE_PREF_STRING:
				if (account == NULL ||
					strcmp(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = purple_account_option_get_default_string(option);
				}
				else
				{
					str_value = purple_account_get_string(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_string(option));
				}

				str_hints = purple_account_option_string_get_hints(option);
				if (str_hints)
				{
					const GSList *hint_it = str_hints;
					entry = gtk_combo_box_text_new_with_entry();
					while (hint_it)
					{
						const gchar *hint = hint_it->data;
						hint_it = g_slist_next(hint_it);
						gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(entry),
						                               hint);
					}
				}
				else
					entry = gtk_entry_new();
				
				opt_entry->widget = entry;
				if (purple_account_option_string_get_masked(option) && str_hints)
					g_warn_if_reached();
				else if (purple_account_option_string_get_masked(option))
				{
					gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
#if !GTK_CHECK_VERSION(2,16,0)
					if (gtk_entry_get_invisible_char(GTK_ENTRY(entry)) == '*')
						gtk_entry_set_invisible_char(GTK_ENTRY(entry), PIDGIN_INVISIBLE_CHAR);
#endif /* Less than GTK+ 2.16 */
				}

				if (str_value != NULL && str_hints)
					gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))),
					                   str_value);
				else
					gtk_entry_set_text(GTK_ENTRY(entry), str_value);

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, entry);
				g_free(title);
				break;

			case PURPLE_PREF_STRING_LIST:
				i = 0;
				idx = 0;

				if (account == NULL ||
					strcmp(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = purple_account_option_get_default_list_value(option);
				}
				else
				{
					str_value = purple_account_get_string(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_list_value(option));
				}

				list = purple_account_option_get_list(option);
				model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
				opt_entry->widget = combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));

				/* Loop through list of PurpleKeyValuePair items */
				for (node = list; node != NULL; node = node->next) {
					if (node->data != NULL) {
						kvp = (PurpleKeyValuePair *) node->data;
						if ((kvp->value != NULL) && (str_value != NULL) &&
						    !g_utf8_collate(kvp->value, str_value))
							idx = i;

						gtk_list_store_append(model, &iter);
						gtk_list_store_set(model, &iter,
								0, kvp->key,
								1, kvp->value,
								-1);
					}

					i++;
				}

				/* Set default */
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), idx);

				/* Define renderer */
				renderer = gtk_cell_renderer_text_new();
				gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer,
						TRUE);
				gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo),
						renderer, "text", 0, NULL);

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, combo);
				g_free(title);
				break;

			default:
				purple_debug_error("gtkaccount", "Invalid Account Option pref type (%d)\n",
						   opt_entry->type);
				g_free(opt_entry->setting);
				g_free(opt_entry);
				continue;
		}

		dialog->protocol_opt_entries =
			g_list_append(dialog->protocol_opt_entries, opt_entry);

	}
}

static GtkWidget *
make_proxy_dropdown(void)
{
	GtkWidget *dropdown;
	GtkListStore *model;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;

	model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, purple_running_gnome() ? _("Use GNOME Proxy Settings")
			:_("Use Global Proxy Settings"),
			1, PURPLE_PROXY_USE_GLOBAL,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("No Proxy"),
			1, PURPLE_PROXY_NONE,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("SOCKS 4"),
			1, PURPLE_PROXY_SOCKS4,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("SOCKS 5"),
			1, PURPLE_PROXY_SOCKS5,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("Tor/Privacy (SOCKS5)"),
			1, PURPLE_PROXY_TOR,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("HTTP"),
			1, PURPLE_PROXY_HTTP,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("Use Environmental Settings"),
			1, PURPLE_PROXY_USE_ENVVAR,
			-1);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dropdown), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dropdown), renderer,
			"text", 0, NULL);

	return dropdown;
}

static void
proxy_type_changed_cb(GtkWidget *menu, AccountPrefsDialog *dialog)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(menu), &iter)) {
		int int_value;
		gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(menu)), &iter,
			1, &int_value, -1);
		dialog->new_proxy_type = int_value;
	}

	if (dialog->new_proxy_type == PURPLE_PROXY_USE_GLOBAL ||
		dialog->new_proxy_type == PURPLE_PROXY_NONE ||
		dialog->new_proxy_type == PURPLE_PROXY_USE_ENVVAR) {

		gtk_widget_hide(dialog->proxy_vbox);
	}
	else
		gtk_widget_show_all(dialog->proxy_vbox);
}

static void
port_popup_cb(GtkWidget *w, GtkMenu *menu, gpointer data)
{
	GtkWidget *item1;
	GtkWidget *item2;

	/* This is an easter egg.
	   It means one of two things, both intended as humourus:
	   A) your network is really slow and you have nothing better to do than
	      look at butterflies.
	   B)You are looking really closely at something that shouldn't matter. */
	item1 = gtk_menu_item_new_with_label(_("If you look real closely"));

	/* This is an easter egg. See the comment on the previous line in the source. */
	item2 = gtk_menu_item_new_with_label(_("you can see the butterflies mating"));

	gtk_widget_show(item1);
	gtk_widget_show(item2);

	/* Prepend these in reverse order so they appear correctly. */
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item2);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item1);
}

static void
add_proxy_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	PurpleProxyInfo *proxy_info;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkTreeIter iter;
	GtkTreeModel *proxy_model;

	if (dialog->proxy_frame != NULL)
		gtk_widget_destroy(dialog->proxy_frame);

	/* Main vbox */
	dialog->proxy_frame = vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(parent), vbox);
	gtk_widget_show(vbox);

	/* Proxy Type drop-down. */
	dialog->proxy_dropdown = make_proxy_dropdown();

	add_pref_box(dialog, vbox, _("Proxy _type:"), dialog->proxy_dropdown);

	/* Setup the second vbox, which may be hidden at times. */
	dialog->proxy_vbox = vbox2 = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, PIDGIN_HIG_BORDER);
	gtk_widget_show(vbox2);

	/* Host */
	dialog->proxy_host_entry = gtk_entry_new();
	add_pref_box(dialog, vbox2, _("_Host:"), dialog->proxy_host_entry);

	/* Port */
	dialog->proxy_port_entry = gtk_entry_new();
	add_pref_box(dialog, vbox2, _("_Port:"), dialog->proxy_port_entry);

	g_signal_connect(G_OBJECT(dialog->proxy_port_entry), "populate-popup",
					 G_CALLBACK(port_popup_cb), NULL);

	/* User */
	dialog->proxy_user_entry = gtk_entry_new();

	add_pref_box(dialog, vbox2, _("_Username:"), dialog->proxy_user_entry);

	/* Password */
	dialog->proxy_pass_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(dialog->proxy_pass_entry), FALSE);
#if !GTK_CHECK_VERSION(2,16,0)
	if (gtk_entry_get_invisible_char(GTK_ENTRY(dialog->proxy_pass_entry)) == '*')
		gtk_entry_set_invisible_char(GTK_ENTRY(dialog->proxy_pass_entry), PIDGIN_INVISIBLE_CHAR);
#endif /* Less than GTK+ 2.16 */
	add_pref_box(dialog, vbox2, _("Pa_ssword:"), dialog->proxy_pass_entry);

	if (dialog->account != NULL &&
		(proxy_info = purple_account_get_proxy_info(dialog->account)) != NULL) {
		const char *value;
		int int_val;

		dialog->new_proxy_type = purple_proxy_info_get_type(proxy_info);

		if ((value = purple_proxy_info_get_host(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_host_entry), value);

		if ((int_val = purple_proxy_info_get_port(proxy_info)) != 0) {
			char buf[11];

			g_snprintf(buf, sizeof(buf), "%d", int_val);

			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_port_entry), buf);
		}

		if ((value = purple_proxy_info_get_username(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_user_entry), value);

		if ((value = purple_proxy_info_get_password(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_pass_entry), value);

	} else
		dialog->new_proxy_type = PURPLE_PROXY_USE_GLOBAL;

	proxy_model = gtk_combo_box_get_model(
		GTK_COMBO_BOX(dialog->proxy_dropdown));
	if (gtk_tree_model_get_iter_first(proxy_model, &iter)) {
		int int_val;
		do {
			gtk_tree_model_get(proxy_model, &iter, 1, &int_val, -1);
			if (int_val == dialog->new_proxy_type) {
				gtk_combo_box_set_active_iter(
					GTK_COMBO_BOX(dialog->proxy_dropdown), &iter);
				break;
			}
		} while(gtk_tree_model_iter_next(proxy_model, &iter));
	}

	proxy_type_changed_cb(dialog->proxy_dropdown, dialog);

	/* Connect signals. */
	g_signal_connect(G_OBJECT(dialog->proxy_dropdown), "changed",
					 G_CALLBACK(proxy_type_changed_cb), dialog);
}

static void
add_voice_options(AccountPrefsDialog *dialog)
{
#ifdef USE_VV
	if (!dialog->prpl_info || !PURPLE_PROTOCOL_PLUGIN_HAS_FUNC(dialog->prpl_info, initiate_media)) {
		if (dialog->voice_frame) {
			gtk_widget_destroy(dialog->voice_frame);
			dialog->voice_frame = NULL;
			dialog->suppression_check = NULL;
		}
		return;
	}

	if (!dialog->voice_frame) {
		dialog->voice_frame = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
		gtk_container_set_border_width(GTK_CONTAINER(dialog->voice_frame),
										PIDGIN_HIG_BORDER);

		dialog->suppression_check =
				gtk_check_button_new_with_mnemonic(_("Use _silence suppression"));
		gtk_box_pack_start(GTK_BOX(dialog->voice_frame), dialog->suppression_check,
				FALSE, FALSE, 0);

		gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook),
				dialog->voice_frame, gtk_label_new_with_mnemonic(_("_Voice and Video")));
		gtk_widget_show_all(dialog->voice_frame);
	}

	if (dialog->account) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->suppression_check),
		                             purple_account_get_silence_suppression(dialog->account));
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->suppression_check), FALSE);
	}
#endif
}

static gboolean
account_win_destroy_cb(GtkWidget *w, GdkEvent *event,
					   AccountPrefsDialog *dialog)
{
	g_hash_table_remove(account_pref_wins, dialog->account);

	gtk_widget_destroy(dialog->window);

	g_list_free(dialog->user_split_entries);
	while (dialog->protocol_opt_entries != NULL) {
		ProtocolOptEntry *opt_entry = dialog->protocol_opt_entries->data;
		g_free(opt_entry->setting);
		g_free(opt_entry);
		dialog->protocol_opt_entries = g_list_delete_link(dialog->protocol_opt_entries, dialog->protocol_opt_entries);
	}
	g_free(dialog->protocol_id);
	g_object_unref(dialog->sg);

	purple_imgstore_unref(dialog->icon_img);

	if (dialog->icon_filesel)
		gtk_widget_destroy(dialog->icon_filesel);

	purple_signals_disconnect_by_handle(dialog);

	g_free(dialog);
	return FALSE;
}

static void
cancel_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	account_win_destroy_cb(NULL, NULL, dialog);
}

static void
account_register_cb(PurpleAccount *account, gboolean succeeded, void *user_data)
{
	if (succeeded)
	{
		const PurpleSavedStatus *saved_status = purple_savedstatus_get_current();
		purple_signal_emit(pidgin_account_get_handle(), "account-modified", account);

		if (saved_status != NULL && purple_account_get_remember_password(account)) {
			purple_savedstatus_activate_for_account(saved_status, account);
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}
	else
		purple_accounts_delete(account);
}

static void
ok_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	PurpleProxyInfo *proxy_info = NULL;
	GList *l, *l2;
	const char *value;
	char *username;
	char *tmp;
	gboolean new_acct = FALSE, icon_change = FALSE;
	PurpleAccount *account;

	/* Build the username string. */
	username = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->username_entry)));

	if (dialog->prpl_info != NULL)
	{
		for (l = dialog->prpl_info->user_splits,
			 l2 = dialog->user_split_entries;
			 l != NULL && l2 != NULL;
			 l = l->next, l2 = l2->next)
		{
			PurpleAccountUserSplit *split = l->data;
			GtkEntry *entry = l2->data;
			char sep[2] = " ";

			value = gtk_entry_get_text(entry);

			*sep = purple_account_user_split_get_separator(split);

			tmp = g_strconcat(username, sep,
					(*value ? value :
					 purple_account_user_split_get_default_value(split)),
					NULL);

			g_free(username);
			username = tmp;
		}
	}

	if (dialog->account == NULL)
	{
		if (purple_accounts_find(username, dialog->protocol_id) != NULL) {
			purple_debug_warning("gtkaccount", "Trying to add a duplicate %s account (%s).\n",
				dialog->protocol_id, username);

			purple_notify_error(NULL, NULL, _("Unable to save new account"),
				_("An account already exists with the specified criteria."));

			g_free(username);
			return;
		}

		if (purple_accounts_get_all() == NULL) {
			/* We're adding our first account.  Be polite and show the buddy list */
			purple_blist_set_visible(TRUE);
		}

		account = purple_account_new(username, dialog->protocol_id);
		new_acct = TRUE;
	}
	else
	{
		account = dialog->account;

		/* Protocol */
		purple_account_set_protocol_id(account, dialog->protocol_id);
	}

	/* Alias */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->alias_entry));

	if (*value != '\0')
		purple_account_set_alias(account, value);
	else
		purple_account_set_alias(account, NULL);

	/* Buddy Icon */
	if (dialog->prpl_info != NULL && dialog->prpl_info->icon_spec.format != NULL)
	{
		const char *filename;

		if (new_acct || purple_account_get_bool(account, "use-global-buddyicon", TRUE) ==
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			icon_change = TRUE;
		}
		purple_account_set_bool(account, "use-global-buddyicon", !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			if (dialog->icon_img)
			{
				size_t len = purple_imgstore_get_size(dialog->icon_img);
				purple_buddy_icons_set_account_icon(account,
				                                    g_memdup(purple_imgstore_get_data(dialog->icon_img), len),
				                                    len);
				purple_account_set_buddy_icon_path(account, purple_imgstore_get_filename(dialog->icon_img));
			}
			else
			{
				purple_buddy_icons_set_account_icon(account, NULL, 0);
				purple_account_set_buddy_icon_path(account, NULL);
			}
		}
		else if ((filename = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon")) && icon_change)
		{
			size_t len;
			gpointer data = pidgin_convert_buddy_icon(dialog->plugin, filename, &len);
			purple_account_set_buddy_icon_path(account, filename);
			purple_buddy_icons_set_account_icon(account, data, len);
		}
	}


	/* Remember Password */
	purple_account_set_remember_password(account,
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(dialog->remember_pass_check)));

	/* Check Mail */
	if (dialog->prpl_info && dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK)
		purple_account_set_check_mail(account,
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(dialog->new_mail_check)));

	/* Password */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->password_entry));

	/*
	 * We set the password if this is a new account because new accounts
	 * will be set to online, and if the user has entered a password into
	 * the account editor (but has not checked the 'save' box), then we
	 * don't want to prompt them.
	 */
	if ((purple_account_get_remember_password(account) || new_acct) && (*value != '\0'))
		purple_account_set_password(account, value);
	else
		purple_account_set_password(account, NULL);

	purple_account_set_username(account, username);
	g_free(username);

	/* Add the protocol settings */
	if (dialog->prpl_info) {
		ProtocolOptEntry *opt_entry;
		GtkTreeIter iter;
		char *value2;
		int int_value;
		gboolean bool_value;

		for (l2 = dialog->protocol_opt_entries; l2; l2 = l2->next) {

			opt_entry = l2->data;

			switch (opt_entry->type) {
				case PURPLE_PREF_STRING:
					if (GTK_IS_COMBO_BOX(opt_entry->widget))
						value = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(opt_entry->widget));
					else
						value = gtk_entry_get_text(GTK_ENTRY(opt_entry->widget));
					purple_account_set_string(account, opt_entry->setting, value);
					break;

				case PURPLE_PREF_INT:
					int_value = atoi(gtk_entry_get_text(GTK_ENTRY(opt_entry->widget)));
					purple_account_set_int(account, opt_entry->setting, int_value);
					break;

				case PURPLE_PREF_BOOLEAN:
					bool_value =
						gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_entry->widget));
					purple_account_set_bool(account, opt_entry->setting, bool_value);
					break;

				case PURPLE_PREF_STRING_LIST:
					value2 = NULL;
					if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(opt_entry->widget), &iter))
						gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(opt_entry->widget)), &iter, 1, &value2, -1);
					purple_account_set_string(account, opt_entry->setting, value2);
					break;

				default:
					break;
			}
		}
	}

	/* Set the proxy stuff. */
	proxy_info = purple_account_get_proxy_info(account);

	/* Create the proxy info if it doesn't exist. */
	if (proxy_info == NULL) {
		proxy_info = purple_proxy_info_new();
		purple_account_set_proxy_info(account, proxy_info);
	}

	/* Set the proxy info type. */
	purple_proxy_info_set_type(proxy_info, dialog->new_proxy_type);

	/* Host */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_host_entry));

	if (*value != '\0')
		purple_proxy_info_set_host(proxy_info, value);
	else
		purple_proxy_info_set_host(proxy_info, NULL);

	/* Port */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_port_entry));

	if (*value != '\0')
		purple_proxy_info_set_port(proxy_info, atoi(value));
	else
		purple_proxy_info_set_port(proxy_info, 0);

	/* Username */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_user_entry));

	if (*value != '\0')
		purple_proxy_info_set_username(proxy_info, value);
	else
		purple_proxy_info_set_username(proxy_info, NULL);

	/* Password */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_pass_entry));

	if (*value != '\0')
		purple_proxy_info_set_password(proxy_info, value);
	else
		purple_proxy_info_set_password(proxy_info, NULL);

	/* If there are no values set then proxy_info NULL */
	if ((purple_proxy_info_get_type(proxy_info) == PURPLE_PROXY_USE_GLOBAL) &&
		(purple_proxy_info_get_host(proxy_info) == NULL) &&
		(purple_proxy_info_get_port(proxy_info) == 0) &&
		(purple_proxy_info_get_username(proxy_info) == NULL) &&
		(purple_proxy_info_get_password(proxy_info) == NULL))
	{
		purple_account_set_proxy_info(account, NULL);
		proxy_info = NULL;
	}

	/* Voice and Video settings */
	if (dialog->voice_frame) {
		purple_account_set_silence_suppression(account,
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->suppression_check)));
	}

	/* If this is a new account, add it to our list */
	if (new_acct)
		purple_accounts_add(account);
	else
		purple_signal_emit(pidgin_account_get_handle(), "account-modified", account);

	/* If this is a new account, then sign on! */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->register_button))) {
		purple_account_set_register_callback(account, account_register_cb, NULL);
		purple_account_register(account);
	} else if (new_acct) {
		const PurpleSavedStatus *saved_status;

		saved_status = purple_savedstatus_get_current();
		if (saved_status != NULL) {
			purple_savedstatus_activate_for_account(saved_status, account);
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}

	/* We no longer need the data from the dialog window */
	account_win_destroy_cb(NULL, NULL, dialog);

}

static const GtkTargetEntry dnd_targets[] = {
	{"text/plain", 0, 0},
	{"text/uri-list", 0, 1},
	{"STRING", 0, 2}
};

void
pidgin_account_dialog_show(PidginAccountDialogType type,
							 PurpleAccount *account)
{
	AccountPrefsDialog *dialog;
	GtkWidget *win;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *dbox;
	GtkWidget *notebook;
	GtkWidget *button;

	if (accounts_window != NULL && account != NULL &&
		(dialog = g_hash_table_lookup(account_pref_wins, account)) != NULL)
	{
		gtk_window_present(GTK_WINDOW(dialog->window));
		return;
	}

	dialog = g_new0(AccountPrefsDialog, 1);

	if (accounts_window != NULL && account != NULL)
	{
		g_hash_table_insert(account_pref_wins, account, dialog);
	}

	dialog->account = account;
	dialog->type    = type;
	dialog->sg      = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if (dialog->account == NULL) {
		/* Select the first prpl in the list*/
		GList *prpl_list = purple_plugins_get_protocols();
		if (prpl_list != NULL)
			dialog->protocol_id = g_strdup(((PurplePlugin *) prpl_list->data)->info->id);
	}
	else
	{
		dialog->protocol_id =
			g_strdup(purple_account_get_protocol_id(dialog->account));
	}

	if ((dialog->plugin = purple_find_prpl(dialog->protocol_id)) != NULL)
		dialog->prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(dialog->plugin);

	dialog->window = win = pidgin_create_dialog((type == PIDGIN_ADD_ACCOUNT_DIALOG) ? _("Add Account") : _("Modify Account"),
		PIDGIN_HIG_BOX_SPACE, "account", FALSE);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(account_win_destroy_cb), dialog);

	/* Setup the vbox */
	main_vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(win), FALSE, PIDGIN_HIG_BOX_SPACE);

	dialog->notebook = notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(notebook));

	/* Setup the inner vbox */
	dialog->top_vbox = vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), PIDGIN_HIG_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox,
			gtk_label_new_with_mnemonic(_("_Basic")));
	gtk_widget_show(vbox);

	/* Setup the top frames. */
	add_login_options(dialog, vbox);
	add_user_options(dialog, vbox);

	button = gtk_check_button_new_with_mnemonic(
		_("Create _this new account on the server"));
	gtk_box_pack_start(GTK_BOX(main_vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->register_button = button;
	g_signal_connect(G_OBJECT(dialog->register_button), "toggled", G_CALLBACK(register_button_cb), dialog);
	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);

	if (!dialog->prpl_info || !dialog->prpl_info->register_user)
		gtk_widget_hide(button);

	/* Setup the page with 'Advanced' (protocol options). */
	add_protocol_options(dialog);

	/* Setup the page with 'Proxy'. */
	dbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(dbox), PIDGIN_HIG_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), dbox,
			gtk_label_new_with_mnemonic(_("P_roxy")));
	gtk_widget_show(dbox);
	add_proxy_options(dialog, dbox);

	add_voice_options(dialog);

	/* Cancel button */
	pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_CANCEL, G_CALLBACK(cancel_account_prefs_cb), dialog);

	/* Save button */
	button = pidgin_dialog_add_button(GTK_DIALOG(win),
	                                  (type == PIDGIN_ADD_ACCOUNT_DIALOG) ? GTK_STOCK_ADD : GTK_STOCK_SAVE,
	                                  G_CALLBACK(ok_account_prefs_cb),
	                                  dialog);
	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);
	dialog->ok_button = button;

	/* Set up DND */
	gtk_drag_dest_set(dialog->window,
			  GTK_DEST_DEFAULT_MOTION |
			  GTK_DEST_DEFAULT_DROP,
			  dnd_targets,
			  sizeof(dnd_targets) / sizeof(GtkTargetEntry),
			  GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(dialog->window), "drag_data_received",
			 G_CALLBACK(account_dnd_recv), dialog);

	/* Show the window. */
	gtk_widget_show(win);
	if (!account)
		gtk_widget_grab_focus(dialog->protocol_menu);
}

/**************************************************************************
 * Accounts Dialog
 **************************************************************************/
static void
signed_on_off_cb(PurpleConnection *gc, gpointer user_data)
{
	PurpleAccount *account;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	size_t index;

	/* Don't need to do anything if the accounts window is not visible */
	if (accounts_window == NULL)
		return;

	account = purple_connection_get_account(gc);
	model = GTK_TREE_MODEL(accounts_window->model);
	index = g_list_index(purple_accounts_get_all(), account);

	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index))
	{
		pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);
		if ((pixbuf != NULL) && purple_account_is_disconnected(account))
			gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

		gtk_list_store_set(accounts_window->model, &iter,
				   COLUMN_ICON, pixbuf,
				   -1);

		if (pixbuf != NULL)
			g_object_unref(G_OBJECT(pixbuf));
	}
}

/*
 * Get the GtkTreeIter of the specified account in the
 * GtkListStore
 */
static gboolean
accounts_window_find_account_in_treemodel(GtkTreeIter *iter, PurpleAccount *account)
{
	GtkTreeModel *model;
	PurpleAccount *cur;

	g_return_val_if_fail(account != NULL, FALSE);
	g_return_val_if_fail(accounts_window != NULL, FALSE);

	model = GTK_TREE_MODEL(accounts_window->model);

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &cur, -1);
	if (cur == account)
		return TRUE;

	while (gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, COLUMN_DATA, &cur, -1);
		if (cur == account)
			return TRUE;
	}

	return FALSE;
}

static void
account_removed_cb(PurpleAccount *account, gpointer user_data)
{
	AccountPrefsDialog *dialog;
	GtkTreeIter iter;

	/* If the account was being modified, close the edit window */
	if ((dialog = g_hash_table_lookup(account_pref_wins, account)) != NULL)
		account_win_destroy_cb(NULL, NULL, dialog);

	if (accounts_window == NULL)
		return;

	/* Remove the account from the GtkListStore */
	if (accounts_window_find_account_in_treemodel(&iter, account))
		gtk_list_store_remove(accounts_window->model, &iter);

	if (purple_accounts_get_all() == NULL)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(accounts_window->notebook), 0);
}

static void
account_abled_cb(PurpleAccount *account, gpointer user_data)
{
	GtkTreeIter iter;

	if (accounts_window == NULL)
		return;

	/* update the account in the GtkListStore */
	if (accounts_window_find_account_in_treemodel(&iter, account))
		gtk_list_store_set(accounts_window->model, &iter,
						   COLUMN_ENABLED, GPOINTER_TO_INT(user_data),
						   -1);
}

static void
drag_data_get_cb(GtkWidget *widget, GdkDragContext *ctx,
				 GtkSelectionData *data, guint info, guint time,
				 AccountsWindow *dialog)
{
	GdkAtom target = gtk_selection_data_get_target(data);

	if (target == gdk_atom_intern("PURPLE_ACCOUNT", FALSE)) {
		GtkTreeRowReference *ref;
		GtkTreePath *source_row;
		GtkTreeIter iter;
		PurpleAccount *account = NULL;
		GValue val;

		ref = g_object_get_data(G_OBJECT(ctx), "gtk-tree-view-source-row");
		source_row = gtk_tree_row_reference_get_path(ref);

		if (source_row == NULL)
			return;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter,
								source_row);
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
								 COLUMN_DATA, &val);

		dialog->drag_iter = iter;

		account = g_value_get_pointer(&val);

		gtk_selection_data_set(data, gdk_atom_intern("PURPLE_ACCOUNT", FALSE),
							   8, (void *)&account, sizeof(account));

		gtk_tree_path_free(source_row);
	}
}

static void
move_account_after(GtkListStore *store, GtkTreeIter *iter,
				   GtkTreeIter *position)
{
	GtkTreeIter new_iter;
	PurpleAccount *account;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
					   COLUMN_DATA, &account,
					   -1);

	gtk_list_store_insert_after(store, &new_iter, position);

	set_account(store, &new_iter, account, NULL);

	gtk_list_store_remove(store, iter);
}

static void
move_account_before(GtkListStore *store, GtkTreeIter *iter,
					GtkTreeIter *position)
{
	GtkTreeIter new_iter;
	PurpleAccount *account;

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
					   COLUMN_DATA, &account,
					   -1);

	gtk_list_store_insert_before(store, &new_iter, position);

	set_account(store, &new_iter, account, NULL);

	gtk_list_store_remove(store, iter);
}

static void
drag_data_received_cb(GtkWidget *widget, GdkDragContext *ctx,
					  guint x, guint y, GtkSelectionData *sd,
					  guint info, guint t, AccountsWindow *dialog)
{
	GdkAtom target = gtk_selection_data_get_target(sd);
	const guchar *data = gtk_selection_data_get_data(sd);

	if (target == gdk_atom_intern("PURPLE_ACCOUNT", FALSE) && data) {
		gint dest_index;
		PurpleAccount *a = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		memcpy(&a, data, sizeof(a));

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y,
											  &path, &position)) {

			GtkTreeIter iter;
			PurpleAccount *account;
			GValue val;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
			val.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
									 COLUMN_DATA, &val);

			account = g_value_get_pointer(&val);

			switch (position) {
				case GTK_TREE_VIEW_DROP_AFTER:
				case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					move_account_after(dialog->model, &dialog->drag_iter,
									   &iter);
					dest_index = g_list_index(purple_accounts_get_all(),
											  account) + 1;
					break;

				case GTK_TREE_VIEW_DROP_BEFORE:
				case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					dest_index = g_list_index(purple_accounts_get_all(),
											  account);

					move_account_before(dialog->model, &dialog->drag_iter,
										&iter);
					break;

				default:
					return;
			}

			purple_accounts_reorder(a, dest_index);
		}
	}
}

static gboolean
accedit_win_destroy_cb(GtkWidget *w, GdkEvent *event, AccountsWindow *dialog)
{
	dialog->window = NULL;

	pidgin_accounts_window_hide();

	return FALSE;
}

static void
add_account_cb(GtkWidget *w, AccountsWindow *dialog)
{
	pidgin_account_dialog_show(PIDGIN_ADD_ACCOUNT_DIALOG, NULL);
}

static void
modify_account_sel(GtkTreeModel *model, GtkTreePath *path,
				   GtkTreeIter *iter, gpointer data)
{
	PurpleAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL)
		pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);
}

static void
modify_account_cb(GtkWidget *w, AccountsWindow *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, modify_account_sel, dialog);
}

static void
delete_account_cb(PurpleAccount *account)
{
	purple_accounts_delete(account);
}

static void
ask_delete_account_sel(GtkTreeModel *model, GtkTreePath *path,
					   GtkTreeIter *iter, gpointer data)
{
	PurpleAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL) {
		char *buf;

		buf = g_strdup_printf(_("Are you sure you want to delete %s?"),
							  purple_account_get_username(account));

		purple_request_close_with_handle(account);
		purple_request_action(account, NULL, buf, NULL,
							PURPLE_DEFAULT_ACTION_NONE,
							account, NULL, NULL,
							account, 2,
							_("Delete"), delete_account_cb,
							_("Cancel"), NULL);
		g_free(buf);
	}
}

static void
ask_delete_account_cb(GtkWidget *w, AccountsWindow *dialog)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection, ask_delete_account_sel,
										dialog);
}

static void
close_accounts_cb(GtkWidget *w, AccountsWindow *dialog)
{
	pidgin_accounts_window_hide();
}


static void
enabled_cb(GtkCellRendererToggle *renderer, gchar *path_str,
			   gpointer data)
{
	AccountsWindow *dialog = (AccountsWindow *)data;
	PurpleAccount *account;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	gboolean enabled;
	const PurpleSavedStatus *saved_status;

	gtk_tree_model_get_iter_from_string(model, &iter, path_str);
	gtk_tree_model_get(model, &iter,
					   COLUMN_DATA, &account,
					   COLUMN_ENABLED, &enabled,
					   -1);

	/*
	 * If we just enabled the account, then set the statuses
	 * to the current status.
	 */
	if (!enabled)
	{
		saved_status = purple_savedstatus_get_current();
		purple_savedstatus_activate_for_account(saved_status, account);
	}

	purple_account_set_enabled(account, PIDGIN_UI, !enabled);
}

static void
add_columns(GtkWidget *treeview, AccountsWindow *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Enabled */
	renderer = gtk_cell_renderer_toggle_new();

	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(enabled_cb), dialog);

	column = gtk_tree_view_column_new_with_attributes(_("Enabled"),
			 renderer, "active", COLUMN_ENABLED, NULL);

	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Username column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Username"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Buddy Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "pixbuf", COLUMN_BUDDYICON);

	/* Username */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "text", COLUMN_USERNAME);
	dialog->username_col = column;


	/* Protocol name */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Protocol"));
	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "pixbuf", COLUMN_ICON);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "text", COLUMN_PROTOCOL);
}

static void
set_account(GtkListStore *store, GtkTreeIter *iter, PurpleAccount *account, GdkPixbuf *global_buddyicon)
{
	GdkPixbuf *pixbuf, *buddyicon = NULL;
	PurpleStoredImage *img = NULL;
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info = NULL;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);
	if ((pixbuf != NULL) && purple_account_is_disconnected(account))
		gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

	prpl = purple_find_prpl(purple_account_get_protocol_id(account));
	if (prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info != NULL && prpl_info->icon_spec.format != NULL) {
		if (purple_account_get_bool(account, "use-global-buddyicon", TRUE)) {
			if (global_buddyicon != NULL)
				buddyicon = g_object_ref(G_OBJECT(global_buddyicon));
			else {
				/* This is for when set_account() is called for a single account */
				const char *path;
				path = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon");
				if ((path != NULL) && (*path != '\0')) {
					img = purple_imgstore_new_from_file(path);
				}
			}
		} else {
			img = purple_buddy_icons_find_account_icon(account);
		}
	}

	if (img != NULL) {
		GdkPixbuf *buddyicon_pixbuf;
		buddyicon_pixbuf = pidgin_pixbuf_from_imgstore(img);
		purple_imgstore_unref(img);

		if (buddyicon_pixbuf != NULL) {
			buddyicon = gdk_pixbuf_scale_simple(buddyicon_pixbuf, 22, 22, GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(buddyicon_pixbuf));
		}
	}

	gtk_list_store_set(store, iter,
			COLUMN_ICON, pixbuf,
			COLUMN_BUDDYICON, buddyicon,
			COLUMN_USERNAME, purple_account_get_username(account),
			COLUMN_ENABLED, purple_account_get_enabled(account, PIDGIN_UI),
			COLUMN_PROTOCOL, purple_account_get_protocol_name(account),
			COLUMN_DATA, account,
			-1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
	if (buddyicon != NULL)
		g_object_unref(G_OBJECT(buddyicon));
}

static void
add_account_to_liststore(PurpleAccount *account, gpointer user_data)
{
	GtkTreeIter iter;
	GdkPixbuf *global_buddyicon = user_data;

	if (accounts_window == NULL)
		return;

	gtk_list_store_append(accounts_window->model, &iter);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(accounts_window->notebook),1);

	set_account(accounts_window->model, &iter, account, global_buddyicon);
}

static gboolean
populate_accounts_list(AccountsWindow *dialog)
{
	GList *l;
	gboolean ret = FALSE;
	GdkPixbuf *global_buddyicon = NULL;
	const char *path;

	gtk_list_store_clear(dialog->model);

	if ((path = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon")) != NULL) {
		GdkPixbuf *pixbuf = pidgin_pixbuf_new_from_file(path);
		if (pixbuf != NULL) {
			global_buddyicon = gdk_pixbuf_scale_simple(pixbuf, 22, 22, GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(pixbuf));
		}
	}

	for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
		ret = TRUE;
		add_account_to_liststore((PurpleAccount *)l->data, global_buddyicon);
	}

	if (global_buddyicon != NULL)
		g_object_unref(G_OBJECT(global_buddyicon));

	return ret;
}

static void
account_selected_cb(GtkTreeSelection *sel, AccountsWindow *dialog)
{
	gboolean selected = FALSE;

	selected = (gtk_tree_selection_count_selected_rows(sel) > 0);

	gtk_widget_set_sensitive(dialog->modify_button, selected);
	gtk_widget_set_sensitive(dialog->delete_button, selected);
}

static gboolean
account_treeview_double_click_cb(GtkTreeView *treeview, GdkEventButton *event, gpointer user_data)
{
	AccountsWindow *dialog;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	PurpleAccount *account;

	dialog = (AccountsWindow *)user_data;

	if (event->window != gtk_tree_view_get_bin_window(treeview))
	    return FALSE;

	/* Figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(dialog->treeview), event->x, event->y, &path, &column, NULL, NULL))
		return FALSE;
	if (column == gtk_tree_view_get_column(treeview, 0)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter, COLUMN_DATA, &account, -1);

	if ((account != NULL) && (event->button == 1) &&
		(event->type == GDK_2BUTTON_PRESS))
	{
		pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *
create_accounts_list(AccountsWindow *dialog)
{
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTargetEntry gte[] = {{"PURPLE_ACCOUNT", GTK_TARGET_SAME_APP, 0}};
	char *pretty, *tmp;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	accounts_window->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(accounts_window->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(accounts_window->notebook), FALSE);
	gtk_container_add(GTK_CONTAINER(frame), accounts_window->notebook);

	/* Create a helpful first-time-use label */
	label = gtk_label_new(NULL);
	/* Translators: Please maintain the use of -> or <- to represent the menu heirarchy */
	tmp = g_strdup_printf(_(
						 "<span size='larger' weight='bold'>Welcome to %s!</span>\n\n"

						 "You have no IM accounts configured. To start connecting with %s "
						 "press the <b>Add...</b> button below and configure your first "
						 "account. If you want %s to connect to multiple IM accounts, "
						 "press <b>Add...</b> again to configure them all.\n\n"

						 "You can come back to this window to add, edit, or remove "
						 "accounts from <b>Accounts->Manage Accounts</b> in the Buddy "
						 "List window"), PIDGIN_NAME, PIDGIN_NAME, PIDGIN_NAME);
	pretty = pidgin_make_pretty_arrows(tmp);
	g_free(tmp);
	gtk_label_set_markup(GTK_LABEL(label), pretty);
	g_free(pretty);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);

	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_notebook_append_page(GTK_NOTEBOOK(accounts_window->notebook), label, NULL);

	/* Create the list model. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS,
					GDK_TYPE_PIXBUF,   /* COLUMN_ICON */
					GDK_TYPE_PIXBUF,   /* COLUMN_BUDDYICON */
					G_TYPE_STRING,     /* COLUMN_USERNAME */
					G_TYPE_BOOLEAN,    /* COLUMN_ENABLED */
					G_TYPE_STRING,     /* COLUMN_PROTOCOL */
					G_TYPE_POINTER     /* COLUMN_DATA */
					);

	/* And now the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	g_object_unref(G_OBJECT(dialog->model));

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(account_selected_cb), dialog);

	/* Handle double-clicking */
	g_signal_connect(G_OBJECT(treeview), "button_press_event",
					 G_CALLBACK(account_treeview_double_click_cb), dialog);

	gtk_notebook_append_page(GTK_NOTEBOOK(accounts_window->notebook),
		pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_NONE, -1, -1),
		NULL);

	add_columns(treeview, dialog);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(treeview));

	if (populate_accounts_list(dialog))
		gtk_notebook_set_current_page(GTK_NOTEBOOK(accounts_window->notebook), 1);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(accounts_window->notebook), 0);

	/* Setup DND. I wanna be an orc! */
	gtk_tree_view_enable_model_drag_source(
			GTK_TREE_VIEW(treeview), GDK_BUTTON1_MASK, gte,
			1, GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(
			GTK_TREE_VIEW(treeview), gte, 1,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(treeview), "drag-data-received",
					 G_CALLBACK(drag_data_received_cb), dialog);
	g_signal_connect(G_OBJECT(treeview), "drag-data-get",
					 G_CALLBACK(drag_data_get_cb), dialog);

	gtk_widget_show_all(frame);
	return frame;
}

static void
account_modified_cb(PurpleAccount *account, AccountsWindow *window)
{
	GtkTreeIter iter;

	if (!accounts_window_find_account_in_treemodel(&iter, account))
		return;

	set_account(window->model, &iter, account, NULL);
}

static void
global_buddyicon_changed(const char *name, PurplePrefType type,
			gconstpointer value, gpointer window)
{
	GList *list;
	for (list = purple_accounts_get_all(); list; list = list->next) {
		account_modified_cb(list->data, window);
	}
}

void
pidgin_accounts_window_show(void)
{
	AccountsWindow *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *button;
	int width, height;

	if (accounts_window != NULL) {
		gtk_window_present(GTK_WINDOW(accounts_window->window));
		return;
	}

	accounts_window = dialog = g_new0(AccountsWindow, 1);

	width  = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/accounts/dialog/width");
	height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/accounts/dialog/height");

#if GTK_CHECK_VERSION(3,0,0)
	dialog->window = win = pidgin_create_dialog(_("Accounts"), 0, "accounts", TRUE);
#else
	dialog->window = win = pidgin_create_dialog(_("Accounts"), PIDGIN_HIG_BORDER, "accounts", TRUE);
#endif
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(accedit_win_destroy_cb), accounts_window);

	/* Setup the vbox */
	vbox = pidgin_dialog_get_vbox_with_properties(GTK_DIALOG(win), FALSE, PIDGIN_HIG_BORDER);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = create_accounts_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Add button */
	pidgin_dialog_add_button(GTK_DIALOG(win), PIDGIN_STOCK_ADD, G_CALLBACK(add_account_cb), dialog);

	/* Modify button */
	button = pidgin_dialog_add_button(GTK_DIALOG(win), PIDGIN_STOCK_MODIFY, G_CALLBACK(modify_account_cb), dialog);
	dialog->modify_button = button;
	gtk_widget_set_sensitive(button, FALSE);

	/* Delete button */
	button = pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_DELETE, G_CALLBACK(ask_delete_account_cb), dialog);
	dialog->delete_button = button;
	gtk_widget_set_sensitive(button, FALSE);

	/* Close button */
	pidgin_dialog_add_button(GTK_DIALOG(win), GTK_STOCK_CLOSE, G_CALLBACK(close_accounts_cb), dialog);

	purple_signal_connect(pidgin_account_get_handle(), "account-modified",
	                    accounts_window,
	                    PURPLE_CALLBACK(account_modified_cb), accounts_window);
	purple_prefs_connect_callback(accounts_window,
	                    PIDGIN_PREFS_ROOT "/accounts/buddyicon",
	                    global_buddyicon_changed, accounts_window);

	gtk_widget_show(win);
}

void
pidgin_accounts_window_hide(void)
{
	if (accounts_window == NULL)
		return;

	if (accounts_window->window != NULL)
		gtk_widget_destroy(accounts_window->window);

	purple_signals_disconnect_by_handle(accounts_window);
	purple_prefs_disconnect_by_handle(accounts_window);

	g_free(accounts_window);
	accounts_window = NULL;
}

static void
free_add_user_data(PidginAccountAddUserData *data)
{
	g_free(data->username);
	g_free(data->alias);
	g_free(data);
}

static void
add_user_cb(PidginAccountAddUserData *data)
{
	PurpleConnection *gc = purple_account_get_connection(data->account);

	if (g_list_find(purple_connections_get_all(), gc))
	{
		purple_blist_request_add_buddy(data->account, data->username,
									 NULL, data->alias);
	}

	free_add_user_data(data);
}

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
pidgin_accounts_notify_added(PurpleAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *msg)
{
	char *buffer;
	PurpleConnection *gc;
	GtkWidget *alert;

	gc = purple_account_get_connection(account);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	alert = pidgin_make_mini_dialog(gc, PIDGIN_STOCK_DIALOG_INFO, buffer,
					  NULL, NULL, _("Close"), NULL, NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);
}

static void
pidgin_accounts_request_add(PurpleAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *msg)
{
	char *buffer;
	PurpleConnection *gc;
	PidginAccountAddUserData *data;
	GtkWidget *alert;

	gc = purple_account_get_connection(account);

	data = g_new0(PidginAccountAddUserData, 1);
	data->account  = account;
	data->username = g_strdup(remote_user);
	data->alias    = g_strdup(alias);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	alert = pidgin_make_mini_dialog(gc, PIDGIN_STOCK_DIALOG_QUESTION,
					  _("Add buddy to your list?"), buffer, data,
					  _("Add"), G_CALLBACK(add_user_cb),
					  _("Cancel"), G_CALLBACK(free_add_user_data), NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);
}

struct auth_request
{
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	void *data;
	char *username;
	char *alias;
	PurpleAccount *account;
	gboolean add_buddy_after_auth;
};

static void
free_auth_request(struct auth_request *ar)
{
	g_free(ar->username);
	g_free(ar->alias);
	g_free(ar);
}

static void
authorize_and_add_cb(struct auth_request *ar, const char *message)
{
	ar->auth_cb(message, ar->data);
	if (ar->add_buddy_after_auth) {
		purple_blist_request_add_buddy(ar->account, ar->username, NULL, ar->alias);
	}
}

static void
authorize_noreason_cb(struct auth_request *ar)
{
	authorize_and_add_cb(ar, NULL);
}

static void
authorize_reason_cb(struct auth_request *ar)
{
	const char *protocol_id;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prpl_info = NULL;

	protocol_id = purple_account_get_protocol_id(ar->account);
	if ((plugin = purple_find_prpl(protocol_id)) != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	if (prpl_info && (prpl_info->options & OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE)) {
		/* Duplicate information because ar is freed by closing minidialog */
		struct auth_request *aa = g_new0(struct auth_request, 1);
		aa->auth_cb = ar->auth_cb;
		aa->deny_cb = ar->deny_cb;
		aa->data = ar->data;
		aa->account = ar->account;
		aa->username = g_strdup(ar->username);
		aa->alias = g_strdup(ar->alias);
		aa->add_buddy_after_auth = ar->add_buddy_after_auth;
		purple_request_input(ar->account, NULL, _("Authorization acceptance message:"),
		                     NULL, _("No reason given."), TRUE, FALSE, NULL,
		                     _("OK"), G_CALLBACK(authorize_and_add_cb),
		                     _("Cancel"), G_CALLBACK(authorize_noreason_cb),
		                     ar->account, ar->username, NULL,
		                     aa);
		/* FIXME: aa is going to leak now. */
	} else {
		authorize_noreason_cb(ar);
	}
}

static void
deny_no_add_cb(struct auth_request *ar, const char *message)
{
	ar->deny_cb(message, ar->data);
}

static void
deny_noreason_cb(struct auth_request *ar)
{
	ar->deny_cb(NULL, ar->data);
}

static void
deny_reason_cb(struct auth_request *ar)
{
	const char *protocol_id;
	PurplePlugin *plugin;
	PurplePluginProtocolInfo *prpl_info = NULL;

	protocol_id = purple_account_get_protocol_id(ar->account);
	if ((plugin = purple_find_prpl(protocol_id)) != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(plugin);

	if (prpl_info && (prpl_info->options & OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE)) {
		/* Duplicate information because ar is freed by closing minidialog */
		struct auth_request *aa = g_new0(struct auth_request, 1);
		aa->auth_cb = ar->auth_cb;
		aa->deny_cb = ar->deny_cb;
		aa->data = ar->data;
		aa->add_buddy_after_auth = ar->add_buddy_after_auth;
		purple_request_input(ar->account, NULL, _("Authorization denied message:"),
		                     NULL, _("No reason given."), TRUE, FALSE, NULL,
		                     _("OK"), G_CALLBACK(deny_no_add_cb),
		                     _("Cancel"), G_CALLBACK(deny_noreason_cb),
		                     ar->account, ar->username, NULL,
		                     aa);
		/* FIXME: aa is going to leak now. */
	} else {
		deny_noreason_cb(ar);
	}
}

static gboolean
get_user_info_cb(GtkWidget   *label,
                 const gchar *uri,
                 gpointer     data)
{
	struct auth_request *ar = data;
	if (!strcmp(uri, "viewinfo")) {
		pidgin_retrieve_user_info(purple_account_get_connection(ar->account), ar->username);
		return TRUE;
	}
	return FALSE;
}

static void
send_im_cb(PidginMiniDialog *mini_dialog,
           GtkButton *button,
           gpointer data)
{
	struct auth_request *ar = data;
	pidgin_dialogs_im_with_user(ar->account, ar->username);
}

static void *
pidgin_accounts_request_authorization(PurpleAccount *account,
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
	GtkWidget *alert;
	PidginMiniDialog *dialog;
	GdkPixbuf *prpl_icon;
	struct auth_request *aa;
	const char *our_name;
	gboolean have_valid_alias = alias && *alias;

	gc = purple_account_get_connection(account);
	if (message != NULL && *message == '\0')
		message = NULL;

	our_name = (id != NULL) ? id :
			(purple_connection_get_display_name(gc) != NULL) ? purple_connection_get_display_name(gc) :
			purple_account_get_username(account);

	if (pidgin_mini_dialog_links_supported()) {
		char *escaped_remote_user = g_markup_escape_text(remote_user, -1);
		char *escaped_alias = alias != NULL ? g_markup_escape_text(alias, -1) : g_strdup("");
		char *escaped_our_name = g_markup_escape_text(our_name, -1);
		char *escaped_message = message != NULL ? g_markup_escape_text(message, -1) : g_strdup("");
		buffer = g_strdup_printf(_("<a href=\"viewinfo\">%s</a>%s%s%s wants to add you (%s) to his or her buddy list%s%s"),
					escaped_remote_user,
					(have_valid_alias ? " ("  : ""),
					escaped_alias,
					(have_valid_alias ? ")"   : ""),
					escaped_our_name,
					(have_valid_alias ? ": " : "."),
					escaped_message);
		g_free(escaped_remote_user);
		g_free(escaped_alias);
		g_free(escaped_our_name);
		g_free(escaped_message);
	} else {
		buffer = g_strdup_printf(_("%s%s%s%s wants to add you (%s) to his or her buddy list%s%s"),
					remote_user,
					(have_valid_alias ? " ("  : ""),
					(have_valid_alias ? alias : ""),
					(have_valid_alias ? ")"   : ""),
					our_name,
					(message != NULL ? ": " : "."),
					(message != NULL ? message  : ""));
	}

	prpl_icon = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_SMALL);

	aa = g_new0(struct auth_request, 1);
	aa->auth_cb = auth_cb;
	aa->deny_cb = deny_cb;
	aa->data = user_data;
	aa->username = g_strdup(remote_user);
	aa->alias = g_strdup(alias);
	aa->account = account;
	aa->add_buddy_after_auth = !on_list;

	alert = pidgin_make_mini_dialog_with_custom_icon(
		gc, prpl_icon,
		_("Authorize buddy?"), NULL, aa,
		_("Authorize"), authorize_reason_cb,
		_("Deny"), deny_reason_cb,
		NULL);

	dialog = PIDGIN_MINI_DIALOG(alert);
	if (pidgin_mini_dialog_links_supported()) {
		pidgin_mini_dialog_enable_description_markup(dialog);
		pidgin_mini_dialog_set_link_callback(dialog, G_CALLBACK(get_user_info_cb), aa);
	}
	pidgin_mini_dialog_set_description(dialog, buffer);
	pidgin_mini_dialog_add_non_closing_button(dialog, _("Send Instant Message"), send_im_cb, aa);

	g_signal_connect_swapped(G_OBJECT(alert), "destroy", G_CALLBACK(free_auth_request), aa);
	g_signal_connect(G_OBJECT(alert), "destroy", G_CALLBACK(purple_account_request_close), NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);

	return alert;
}

static void
pidgin_accounts_request_close(void *ui_handle)
{
	gtk_widget_destroy(GTK_WIDGET(ui_handle));
}

static PurpleAccountUiOps ui_ops =
{
	pidgin_accounts_notify_added,
	NULL,
	pidgin_accounts_request_add,
	pidgin_accounts_request_authorization,
	pidgin_accounts_request_close,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleAccountUiOps *
pidgin_accounts_get_ui_ops(void)
{
	return &ui_ops;
}

void *
pidgin_account_get_handle(void) {
	static int handle;

	return &handle;
}

void
pidgin_account_init(void)
{
	char *default_avatar = NULL;
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/accounts");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/accounts/dialog");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/accounts/dialog/width",  520);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/accounts/dialog/height", 321);
	default_avatar = g_build_filename(g_get_home_dir(), ".face.icon", NULL);
	if (!g_file_test(default_avatar, G_FILE_TEST_EXISTS)) {
		g_free(default_avatar);
		default_avatar = g_build_filename(g_get_home_dir(), ".face", NULL);
		if (!g_file_test(default_avatar, G_FILE_TEST_EXISTS)) {
			g_free(default_avatar);
			default_avatar = NULL;
		}
	}

	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon", default_avatar);
	g_free(default_avatar);

	purple_signal_register(pidgin_account_get_handle(), "account-modified",
						 purple_marshal_VOID__POINTER, NULL, 1,
						 purple_value_new(PURPLE_TYPE_SUBTYPE,
										PURPLE_SUBTYPE_ACCOUNT));

	/* Setup some purple signal handlers. */
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(signed_on_off_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(signed_on_off_cb), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-added",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(add_account_to_liststore), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(account_removed_cb), NULL);
	purple_signal_connect(purple_accounts_get_handle(), "account-disabled",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(account_abled_cb), GINT_TO_POINTER(FALSE));
	purple_signal_connect(purple_accounts_get_handle(), "account-enabled",
						pidgin_account_get_handle(),
						PURPLE_CALLBACK(account_abled_cb), GINT_TO_POINTER(TRUE));

	account_pref_wins =
		g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);
}

void
pidgin_account_uninit(void)
{
	/*
	 * TODO: Need to free all the dialogs in here.  Could probably create
	 *       a callback function to use for the free-some-data-function
	 *       parameter of g_hash_table_new_full, above.
	 */
	g_hash_table_destroy(account_pref_wins);

	purple_signals_disconnect_by_handle(pidgin_account_get_handle());
	purple_signals_unregister_by_instance(pidgin_account_get_handle());
}


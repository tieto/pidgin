/**
 * @file gtkaccount.c GTK+ Account Editor UI
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
#include "gaimstock.h"

enum
{
	COLUMN_ICON,
	COLUMN_BUDDYICON,
	COLUMN_SCREENNAME,
	COLUMN_ENABLED,
	COLUMN_PROTOCOL,
	COLUMN_DATA,
	COLUMN_PULSE_DATA,
	NUM_COLUMNS
};

typedef struct
{
	GaimAccount *account;
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

	GtkTreeViewColumn *screenname_col;

} AccountsWindow;

typedef struct
{
	PidginAccountDialogType type;

	GaimAccount *account;
	char *protocol_id;
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
	GtkWidget *register_button;

	/* Login Options */
	GtkWidget *login_frame;
	GtkWidget *protocol_menu;
	GtkWidget *password_box;
	GtkWidget *screenname_entry;
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
	char *cached_icon_path;
	char *icon_path;

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

	/* Are we registering? */
	gboolean   registering;

} AccountPrefsDialog;

typedef struct
{
	GdkPixbuf *online_pixbuf;
	gboolean pulse_to_grey;
	float pulse_value;
	int timeout;
	GaimAccount *account;
	GtkTreeModel *model;

} PidginPulseData;


static AccountsWindow *accounts_window = NULL;
static GHashTable *account_pref_wins;

static void add_account_to_liststore(GaimAccount *account, gpointer user_data);
static void set_account(GtkListStore *store, GtkTreeIter *iter,
						  GaimAccount *account, GdkPixbuf *global_buddyicon);

/**************************************************************************
 * Add/Modify Account dialog
 **************************************************************************/
static void add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_protocol_options(AccountPrefsDialog *dialog,
								 GtkWidget *parent);
static void add_proxy_options(AccountPrefsDialog *dialog, GtkWidget *parent);

static GtkWidget *
add_pref_box(AccountPrefsDialog *dialog, GtkWidget *parent,
			 const char *text, GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(parent), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new_with_mnemonic(text);
	gtk_size_group_add_widget(dialog->sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, GAIM_HIG_BORDER);
	gtk_widget_show(widget);
	pidgin_set_accessible_label (widget, label);

	return hbox;
}

static void
set_dialog_icon(AccountPrefsDialog *dialog, gchar *new_cached_icon_path, gchar *new_icon_path)
{
	char *filename;
	GdkPixbuf *pixbuf = NULL;

	g_free(dialog->cached_icon_path);
	g_free(dialog->icon_path);
	dialog->cached_icon_path = new_cached_icon_path;
	dialog->icon_path = new_icon_path;

	filename = gaim_buddy_icons_get_full_path(dialog->cached_icon_path);
	if (filename != NULL) {
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	if (pixbuf && dialog->prpl_info &&
	    (dialog->prpl_info->icon_spec.scale_rules & GAIM_ICON_SCALE_DISPLAY))
	{
		/* Scale the icon to something reasonable */
		int width, height;
		GdkPixbuf *scale;

		pidgin_buddy_icon_get_scale_size(pixbuf, &dialog->prpl_info->icon_spec,
				GAIM_ICON_SCALE_DISPLAY, &width, &height);
		scale = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);

		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = scale;
	}

	if (pixbuf == NULL)
	{
		/* Show a placeholder icon */
		gchar *filename;
		filename = g_build_filename(DATADIR, "pixmaps",
				"gaim", "insert-image.png", NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_entry), pixbuf);
	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
}

static void
set_account_protocol_cb(GtkWidget *item, const char *id,
						AccountPrefsDialog *dialog)
{
	GaimPlugin *new_plugin;

	new_plugin = gaim_find_prpl(id);

	if (new_plugin == dialog->plugin)
		return;

	dialog->plugin = new_plugin;

	if (dialog->plugin != NULL)
	{
		dialog->prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(dialog->plugin);

		g_free(dialog->protocol_id);
		dialog->protocol_id = g_strdup(dialog->plugin->info->id);
	}

	if (dialog->account != NULL)
		gaim_account_clear_settings(dialog->account);

	add_login_options(dialog,    dialog->top_vbox);
	add_user_options(dialog,     dialog->top_vbox);
	add_protocol_options(dialog, dialog->bottom_vbox);

	if (!dialog->prpl_info || !dialog->prpl_info->register_user) {
		gtk_widget_hide(dialog->register_button);
	} else {
		if (dialog->prpl_info != NULL &&
		   (dialog->prpl_info->options & OPT_PROTO_REGISTER_NOSCREENNAME)) {
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		} else {
			gtk_widget_set_sensitive(dialog->register_button, FALSE);
		}
		gtk_widget_show(dialog->register_button);
	}
}

static void
screenname_changed_cb(GtkEntry *entry, AccountPrefsDialog *dialog)
{
	if (dialog->ok_button)
		gtk_widget_set_sensitive(dialog->ok_button,
				*gtk_entry_get_text(entry) != '\0');
	if (dialog->register_button) {
		if (dialog->prpl_info != NULL && (dialog->prpl_info->options & OPT_PROTO_REGISTER_NOSCREENNAME))
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		else
			gtk_widget_set_sensitive(dialog->register_button,
					*gtk_entry_get_text(entry) != '\0');
	}
}

static void
icon_filesel_choose_cb(const char *filename, gpointer data)
{
	AccountPrefsDialog *dialog;

	dialog = data;

	if (filename != NULL)
		set_dialog_icon(dialog, pidgin_convert_buddy_icon(dialog->plugin, filename), g_strdup(filename));

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
	set_dialog_icon(dialog, NULL, NULL);
}

static void
account_dnd_recv(GtkWidget *widget, GdkDragContext *dc, gint x, gint y,
		 GtkSelectionData *sd, guint info, guint t, AccountPrefsDialog *dialog)
{
	gchar *name = (gchar *)sd->data;

	if ((sd->length >= 0) && (sd->format == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */
		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp, *rtmp;
			/* It looks like we're dealing with a local file. Let's
			 * just untar it in the right place */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				gaim_debug(GAIM_DEBUG_ERROR, "buddyicon", "%s\n",
					   (converr ? converr->message :
					    "g_filename_from_uri error"));
				return;
			}
			if ((rtmp = strchr(tmp, '\r')) || (rtmp = strchr(tmp, '\n')))
				*rtmp = '\0';
			set_dialog_icon(dialog, pidgin_convert_buddy_icon(dialog->plugin, tmp), g_strdup(tmp));
			g_free(tmp);
		}
		gtk_drag_finish(dc, TRUE, FALSE, t);
	}
	gtk_drag_finish(dc, FALSE, FALSE, t);
}

static void
update_editable(GaimConnection *gc, AccountPrefsDialog *dialog)
{
	gboolean set;
	GList *l;

	if (dialog->account == NULL)
		return;

	if (gc != NULL && dialog->account != gaim_connection_get_account(gc))
		return;

	set = !(gaim_account_is_connected(dialog->account) || gaim_account_is_connecting(dialog->account));
	gtk_widget_set_sensitive(dialog->protocol_menu, set);
	gtk_widget_set_sensitive(dialog->screenname_entry, set);

	for (l = dialog->user_split_entries ; l != NULL ; l = l->next)
		gtk_widget_set_sensitive((GtkWidget *)l->data, set);
}

static void
add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent)
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
	frame = pidgin_make_frame(parent, _("Login Options"));

	/* cringe */
	dialog->login_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->login_frame, 0);
	gtk_widget_show(dialog->login_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Protocol */
	dialog->protocol_menu = pidgin_protocol_option_menu_new(
			dialog->protocol_id, G_CALLBACK(set_account_protocol_cb), dialog);

	add_pref_box(dialog, vbox, _("Protocol:"), dialog->protocol_menu);

	/* Screen name */
	dialog->screenname_entry = gtk_entry_new();

	add_pref_box(dialog, vbox, _("Screen name:"), dialog->screenname_entry);

	g_signal_connect(G_OBJECT(dialog->screenname_entry), "changed",
					 G_CALLBACK(screenname_changed_cb), dialog);

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
	if (gtk_entry_get_invisible_char(GTK_ENTRY(dialog->password_entry)) == '*')
		gtk_entry_set_invisible_char(GTK_ENTRY(dialog->password_entry), GAIM_INVISIBLE_CHAR);
	dialog->password_box = add_pref_box(dialog, vbox, _("Password:"),
										  dialog->password_entry);

	/* Alias */
	dialog->alias_entry = gtk_entry_new();
	add_pref_box(dialog, vbox, _("Local alias:"), dialog->alias_entry);

	/* Remember Password */
	dialog->remember_pass_check =
		gtk_check_button_new_with_label(_("Remember password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->remember_pass_check),
								 FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->remember_pass_check,
					   FALSE, FALSE, 0);
	gtk_widget_show(dialog->remember_pass_check);

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
	}

	if (dialog->prpl_info != NULL &&
		(dialog->prpl_info->options & OPT_PROTO_NO_PASSWORD)) {

		gtk_widget_hide(dialog->password_box);
		gtk_widget_hide(dialog->remember_pass_check);
	}

	/* Do not let the user change the protocol/screenname while connected. */
	update_editable(NULL, dialog);
	gaim_signal_connect(gaim_connections_get_handle(), "signing-on", dialog,
					G_CALLBACK(update_editable), dialog);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off", dialog,
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
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* New mail notifications */
	dialog->new_mail_check =
		gtk_check_button_new_with_label(_("New mail notifications"));
	gtk_box_pack_start(GTK_BOX(vbox), dialog->new_mail_check, FALSE, FALSE, 0);
	gtk_widget_show(dialog->new_mail_check);

	/* Buddy icon */
	dialog->icon_check = gtk_check_button_new_with_label(_("Use this buddy icon for this account:"));
	g_signal_connect(G_OBJECT(dialog->icon_check), "toggled", G_CALLBACK(icon_check_cb), dialog);
	gtk_widget_show(dialog->icon_check);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->icon_check, FALSE, FALSE, 0);

	dialog->icon_hbox = hbox = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
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
	dialog->cached_icon_path = NULL;
	dialog->icon_path = NULL;

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	hbox2 = gtk_hbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, GAIM_HIG_BORDER);
	gtk_widget_show(hbox2);

	button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(icon_reset_cb), dialog);
	gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	if (dialog->prpl_info != NULL) {
		if (!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK))
			gtk_widget_hide(dialog->new_mail_check);

		if (dialog->prpl_info->icon_spec.format == NULL)
			gtk_widget_hide(dialog->icon_hbox);
	}

	if (dialog->account != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->new_mail_check),
					     gaim_account_get_check_mail(dialog->account));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->icon_check),
					     !gaim_account_get_bool(dialog->account, "use-global-buddyicon",
								       TRUE));
		set_dialog_icon(dialog,
				g_strdup(gaim_account_get_ui_string(dialog->account,
						PIDGIN_UI, "non-global-buddyicon-cached-path", NULL)),
				g_strdup(gaim_account_get_ui_string(dialog->account, 
						PIDGIN_UI, "non-global-buddyicon-path", NULL)));
	} else {
		set_dialog_icon(dialog, NULL, NULL);
	}

	if (!dialog->prpl_info ||
			(!(dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK) &&
			 (dialog->prpl_info->icon_spec.format ==  NULL))) {

		/* Nothing to see :( aww. */
		gtk_widget_hide(dialog->user_frame);
	}
}

static void
add_protocol_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GaimAccountOption *option;
	GaimAccount *account;
	GtkWidget *frame, *vbox, *check, *entry, *combo;
	const GList *list, *node;
	gint i, idx, int_value;
	GtkListStore *model;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GaimKeyValuePair *kvp;
	GList *l;
	char buf[1024];
	char *title;
	const char *str_value;
	gboolean bool_value;

	if (dialog->protocol_frame != NULL) {
		gtk_widget_destroy(dialog->protocol_frame);
		dialog->protocol_frame = NULL;
	}

	if (dialog->protocol_opt_entries != NULL) {
		g_list_free(dialog->protocol_opt_entries);
		dialog->protocol_opt_entries = NULL;
	}

	if (dialog->prpl_info == NULL ||
		dialog->prpl_info->protocol_options == NULL) {

		return;
	}

	account = dialog->account;

	/* Build the protocol options frame. */
	g_snprintf(buf, sizeof(buf), _("%s Options"), dialog->plugin->info->name);

	frame = pidgin_make_frame(parent, buf);
	dialog->protocol_frame =
		gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->protocol_frame, 0);
	gtk_widget_show(dialog->protocol_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	for (l = dialog->prpl_info->protocol_options; l != NULL; l = l->next)
	{
		option = (GaimAccountOption *)l->data;

		switch (gaim_account_option_get_type(option))
		{
			case GAIM_PREF_BOOLEAN:
				if (account == NULL ||
					strcmp(gaim_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					bool_value = gaim_account_option_get_default_bool(option);
				}
				else
				{
					bool_value = gaim_account_get_bool(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_bool(option));
				}

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
					strcmp(gaim_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					int_value = gaim_account_option_get_default_int(option);
				}
				else
				{
					int_value = gaim_account_get_int(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_int(option));
				}

				g_snprintf(buf, sizeof(buf), "%d", int_value);

				entry = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(entry), buf);

				title = g_strdup_printf("%s:",
						gaim_account_option_get_text(option));

				add_pref_box(dialog, vbox, title, entry);

				g_free(title);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, entry);

				break;

			case GAIM_PREF_STRING:
				if (account == NULL ||
					strcmp(gaim_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = gaim_account_option_get_default_string(option);
				}
				else
				{
					str_value = gaim_account_get_string(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_string(option));
				}

				entry = gtk_entry_new();
				if (gaim_account_option_get_masked(option))
				{
					gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
					if (gtk_entry_get_invisible_char(GTK_ENTRY(entry)) == '*')
						gtk_entry_set_invisible_char(GTK_ENTRY(entry), GAIM_INVISIBLE_CHAR);
				}

				if (str_value != NULL)
					gtk_entry_set_text(GTK_ENTRY(entry), str_value);

				title = g_strdup_printf("%s:",
						gaim_account_option_get_text(option));

				add_pref_box(dialog, vbox, title, entry);

				g_free(title);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, entry);

				break;

			case GAIM_PREF_STRING_LIST:
				i = 0;
				idx = 0;

				if (account == NULL ||
					strcmp(gaim_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = gaim_account_option_get_default_list_value(option);
				}
				else
				{
					str_value = gaim_account_get_string(account,
						gaim_account_option_get_setting(option),
						gaim_account_option_get_default_list_value(option));
				}

				list = gaim_account_option_get_list(option);
				model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
				combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));

				/* Loop through list of GaimKeyValuePair items */
				for (node = list; node != NULL; node = node->next) {
					if (node->data != NULL) {
						kvp = (GaimKeyValuePair *) node->data;
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

				title = g_strdup_printf("%s:",
						gaim_account_option_get_text(option));

				add_pref_box(dialog, vbox, title, combo);

				g_free(title);

				dialog->protocol_opt_entries =
					g_list_append(dialog->protocol_opt_entries, combo);

				break;


			default:
				break;
		}
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
			0, gaim_running_gnome() ? _("Use GNOME Proxy Settings")
			:_("Use Global Proxy Settings"),
			1, GAIM_PROXY_USE_GLOBAL,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("No Proxy"),
			1, GAIM_PROXY_NONE,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("HTTP"),
			1, GAIM_PROXY_HTTP,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("SOCKS 4"),
			1, GAIM_PROXY_SOCKS4,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("SOCKS 5"),
			1, GAIM_PROXY_SOCKS5,
			-1);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			0, _("Use Environmental Settings"),
			1, GAIM_PROXY_USE_ENVVAR,
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
	dialog->new_proxy_type =
		gtk_combo_box_get_active(GTK_COMBO_BOX(menu)) - 1;

	if (dialog->new_proxy_type == GAIM_PROXY_USE_GLOBAL ||
		dialog->new_proxy_type == GAIM_PROXY_NONE ||
		dialog->new_proxy_type == GAIM_PROXY_USE_ENVVAR) {

		gtk_widget_hide_all(dialog->proxy_vbox);
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
	GaimProxyInfo *proxy_info;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *vbox2;

	if (dialog->proxy_frame != NULL)
		gtk_widget_destroy(dialog->proxy_frame);

	frame = pidgin_make_frame(parent, _("Proxy Options"));
	dialog->proxy_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->proxy_frame, 1);
	gtk_widget_show(dialog->proxy_frame);

	/* Main vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Proxy Type drop-down. */
	dialog->proxy_dropdown = make_proxy_dropdown();

	add_pref_box(dialog, vbox, _("Proxy _type:"), dialog->proxy_dropdown);

	/* Setup the second vbox, which may be hidden at times. */
	dialog->proxy_vbox = vbox2 = gtk_vbox_new(FALSE, GAIM_HIG_BOX_SPACE);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, FALSE, FALSE, GAIM_HIG_BORDER);
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
	if (gtk_entry_get_invisible_char(GTK_ENTRY(dialog->proxy_pass_entry)) == '*')
		gtk_entry_set_invisible_char(GTK_ENTRY(dialog->proxy_pass_entry), GAIM_INVISIBLE_CHAR);
	add_pref_box(dialog, vbox2, _("Pa_ssword:"), dialog->proxy_pass_entry);

	if (dialog->account != NULL &&
		(proxy_info = gaim_account_get_proxy_info(dialog->account)) != NULL) {

		GaimProxyType type = gaim_proxy_info_get_type(proxy_info);
		const char *value;
		int int_val;

		/* Hah! */
		/* I dunno what you're laughing about, fuzz ball. */
		dialog->new_proxy_type = type;
		gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->proxy_dropdown),
				type + 1);

		if (type == GAIM_PROXY_USE_GLOBAL || type == GAIM_PROXY_NONE ||
				type == GAIM_PROXY_USE_ENVVAR)
			gtk_widget_hide_all(vbox2);


		if ((value = gaim_proxy_info_get_host(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_host_entry), value);

		if ((int_val = gaim_proxy_info_get_port(proxy_info)) != 0) {
			char buf[11];

			g_snprintf(buf, sizeof(buf), "%d", int_val);

			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_port_entry), buf);
		}

		if ((value = gaim_proxy_info_get_username(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_user_entry), value);

		if ((value = gaim_proxy_info_get_password(proxy_info)) != NULL)
			gtk_entry_set_text(GTK_ENTRY(dialog->proxy_pass_entry), value);
	}
	else {
		dialog->new_proxy_type = GAIM_PROXY_USE_GLOBAL;
		gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->proxy_dropdown),
				dialog->new_proxy_type + 1);
		gtk_widget_hide_all(vbox2);
	}

	/* Connect signals. */
	g_signal_connect(G_OBJECT(dialog->proxy_dropdown), "changed",
					 G_CALLBACK(proxy_type_changed_cb), dialog);
}

static void
account_win_destroy_cb(GtkWidget *w, GdkEvent *event,
					   AccountPrefsDialog *dialog)
{
	g_hash_table_remove(account_pref_wins, dialog->account);

	gtk_widget_destroy(dialog->window);

	g_list_free(dialog->user_split_entries);
	g_list_free(dialog->protocol_opt_entries);
	g_free(dialog->protocol_id);

	if (dialog->cached_icon_path != NULL)
	{
		const char *icon = gaim_account_get_ui_string(dialog->account, PIDGIN_UI, "non-global-buddyicon-cached-path", NULL);
		if (dialog->cached_icon_path != NULL && (icon == NULL || strcmp(dialog->cached_icon_path, icon)))
		{
			/* The user set an icon, which would've been cached by convert_buddy_icon,
			 * but didn't save the changes. Delete the cache file. */
			char *filename = g_build_filename(gaim_buddy_icons_get_cache_dir(), dialog->cached_icon_path, NULL);
			g_unlink(filename);
			g_free(filename);
		}

		g_free(dialog->cached_icon_path);
	}

	g_free(dialog->icon_path);

	if (dialog->icon_filesel)
		gtk_widget_destroy(dialog->icon_filesel);

	gaim_signals_disconnect_by_handle(dialog);

	g_free(dialog);
}

static void
cancel_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	account_win_destroy_cb(NULL, NULL, dialog);
}

static GaimAccount*
ok_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	GaimProxyInfo *proxy_info = NULL;
	GList *l, *l2;
	const char *value;
	char *username;
	char *tmp;
	gboolean new = FALSE, icon_change = FALSE;
	GaimAccount *account;
	GaimPluginProtocolInfo *prpl_info;

	if (dialog->account == NULL)
	{
		const char *screenname;

		screenname = gtk_entry_get_text(GTK_ENTRY(dialog->screenname_entry));
		account = gaim_account_new(screenname, dialog->protocol_id);
		new = TRUE;
	}
	else
	{
		account = dialog->account;

		/* Protocol */
		gaim_account_set_protocol_id(account, dialog->protocol_id);
	}

	/* Alias */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->alias_entry));

	if (*value != '\0')
		gaim_account_set_alias(account, value);
	else
		gaim_account_set_alias(account, NULL);

	/* Buddy Icon */
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(dialog->plugin);
	if (prpl_info != NULL && prpl_info->icon_spec.format != NULL)
	{
		if (new || gaim_account_get_bool(account, "use-global-buddyicon", TRUE) ==
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			icon_change = TRUE;
		}
		gaim_account_set_bool(account, "use-global-buddyicon", !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));
		gaim_account_set_ui_string(account, PIDGIN_UI, "non-global-buddyicon-cached-path", dialog->cached_icon_path);
		gaim_account_set_ui_string(account, PIDGIN_UI, "non-global-buddyicon-path", dialog->icon_path);
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			gaim_account_set_buddy_icon_path(account, dialog->icon_path);
			gaim_account_set_buddy_icon(account, dialog->cached_icon_path);
		}
		else if (gaim_prefs_get_path("/gaim/gtk/accounts/buddyicon") && icon_change)
		{
			const char *filename = gaim_prefs_get_path("/gaim/gtk/accounts/buddyicon");
			char *icon = pidgin_convert_buddy_icon(dialog->plugin, filename);
			gaim_account_set_buddy_icon_path(account, filename);
			gaim_account_set_buddy_icon(account, icon);
			g_free(icon);
		}
	}


	/* Remember Password */
	gaim_account_set_remember_password(account,
			gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(dialog->remember_pass_check)));

	/* Check Mail */
	if (dialog->prpl_info && dialog->prpl_info->options & OPT_PROTO_MAIL_CHECK)
		gaim_account_set_check_mail(account,
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
	if ((gaim_account_get_remember_password(account) || new) && (*value != '\0'))
		gaim_account_set_password(account, value);
	else
		gaim_account_set_password(account, NULL);

	/* Build the username string. */
	username =
		g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->screenname_entry)));

	if (dialog->prpl_info != NULL)
	{
		for (l = dialog->prpl_info->user_splits,
			 l2 = dialog->user_split_entries;
			 l != NULL && l2 != NULL;
			 l = l->next, l2 = l2->next)
		{
			GaimAccountUserSplit *split = l->data;
			GtkEntry *entry = l2->data;
			char sep[2] = " ";

			value = gtk_entry_get_text(entry);

			*sep = gaim_account_user_split_get_separator(split);

			tmp = g_strconcat(username, sep,
					(*value ? value :
					 gaim_account_user_split_get_default_value(split)),
					NULL);

			g_free(username);
			username = tmp;
		}
	}

	gaim_account_set_username(account, username);
	g_free(username);

	/* Add the protocol settings */
	if (dialog->prpl_info) {
		for (l = dialog->prpl_info->protocol_options,
				l2 = dialog->protocol_opt_entries;
				l != NULL && l2 != NULL;
				l = l->next, l2 = l2->next) {

			GaimPrefType type;
			GaimAccountOption *option = l->data;
			GtkWidget *widget = l2->data;
			GtkTreeIter iter;
			const char *setting;
			char *value2;
			int int_value;
			gboolean bool_value;

			type = gaim_account_option_get_type(option);

			setting = gaim_account_option_get_setting(option);

			switch (type) {
				case GAIM_PREF_STRING:
					value = gtk_entry_get_text(GTK_ENTRY(widget));
					gaim_account_set_string(account, setting, value);
					break;

				case GAIM_PREF_INT:
					int_value = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
					gaim_account_set_int(account, setting, int_value);
					break;

				case GAIM_PREF_BOOLEAN:
					bool_value =
						gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
					gaim_account_set_bool(account, setting, bool_value);
					break;

				case GAIM_PREF_STRING_LIST:
					gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter);
					gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(widget)), &iter, 1, &value2, -1);
					gaim_account_set_string(account, setting, value2);
					break;

				default:
					break;
			}
		}
	}

	/* Set the proxy stuff. */
	proxy_info = gaim_account_get_proxy_info(account);

	/* Create the proxy info if it doesn't exist. */
	if (proxy_info == NULL) {
		proxy_info = gaim_proxy_info_new();
		gaim_account_set_proxy_info(account, proxy_info);
	}

	/* Set the proxy info type. */
	gaim_proxy_info_set_type(proxy_info, dialog->new_proxy_type);

	/* Host */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_host_entry));

	if (*value != '\0')
		gaim_proxy_info_set_host(proxy_info, value);
	else
		gaim_proxy_info_set_host(proxy_info, NULL);

	/* Port */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_port_entry));

	if (*value != '\0')
		gaim_proxy_info_set_port(proxy_info, atoi(value));
	else
		gaim_proxy_info_set_port(proxy_info, 0);

	/* Username */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_user_entry));

	if (*value != '\0')
		gaim_proxy_info_set_username(proxy_info, value);
	else
		gaim_proxy_info_set_username(proxy_info, NULL);

	/* Password */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->proxy_pass_entry));

	if (*value != '\0')
		gaim_proxy_info_set_password(proxy_info, value);
	else
		gaim_proxy_info_set_password(proxy_info, NULL);

	/* If there are no values set then proxy_info NULL */
	if ((gaim_proxy_info_get_type(proxy_info) == GAIM_PROXY_USE_GLOBAL) &&
		(gaim_proxy_info_get_host(proxy_info) == NULL) &&
		(gaim_proxy_info_get_port(proxy_info) == 0) &&
		(gaim_proxy_info_get_username(proxy_info) == NULL) &&
		(gaim_proxy_info_get_password(proxy_info) == NULL))
	{
		gaim_account_set_proxy_info(account, NULL);
		proxy_info = NULL;
	}


	/* We no longer need the data from the dialog window */
	account_win_destroy_cb(NULL, NULL, dialog);

	/* If this is a new account, add it to our list */
	if (new)
		gaim_accounts_add(account);
	else
		gaim_signal_emit(pidgin_account_get_handle(), "account-modified", account);

	/* If this is a new account, then sign on! */
	if (new && !dialog->registering) {
		const GaimSavedStatus *saved_status;

		saved_status = gaim_savedstatus_get_current();
		if (saved_status != NULL) {
			gaim_savedstatus_activate_for_account(saved_status, account);
			gaim_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}

	return account;
}

static void
register_account_prefs_cb(GtkWidget *w, AccountPrefsDialog *dialog)
{
	GaimAccount *account;

	dialog->registering = TRUE;

	account = ok_account_prefs_cb(NULL, dialog);

	gaim_account_register(account);
}


static const GtkTargetEntry dnd_targets[] = {
	{"text/plain", 0, 0},
	{"text/uri-list", 0, 1},
	{"STRING", 0, 2}
};

void
pidgin_account_dialog_show(PidginAccountDialogType type,
							 GaimAccount *account)
{
	AccountPrefsDialog *dialog;
	GtkWidget *win;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *bbox;
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
		GList *prpl_list = gaim_plugins_get_protocols();
		if (prpl_list != NULL)
			dialog->protocol_id = g_strdup(((GaimPlugin *) prpl_list->data)->info->id);
	}
	else
	{
		dialog->protocol_id =
			g_strdup(gaim_account_get_protocol_id(dialog->account));
	}

	if ((dialog->plugin = gaim_find_prpl(dialog->protocol_id)) != NULL)
		dialog->prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(dialog->plugin);


	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_role(GTK_WINDOW(win), "account");

	if (type == PIDGIN_ADD_ACCOUNT_DIALOG)
		gtk_window_set_title(GTK_WINDOW(win), _("Add Account"));
	else
		gtk_window_set_title(GTK_WINDOW(win), _("Modify Account"));

	gtk_window_set_resizable(GTK_WINDOW(win), FALSE);

	gtk_container_set_border_width(GTK_CONTAINER(win), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(account_win_destroy_cb), dialog);

	/* Setup the vbox */
	main_vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(win), main_vbox);
	gtk_widget_show(main_vbox);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(notebook));

	/* Setup the inner vbox */
	dialog->top_vbox = vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), GAIM_HIG_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox,
			gtk_label_new_with_mnemonic(_("_Basic")));
	gtk_widget_show(vbox);

	/* Setup the top frames. */
	add_login_options(dialog, vbox);
	add_user_options(dialog, vbox);

	/* Setup the page with 'Advanced'. */
	dialog->bottom_vbox = dbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(dbox), GAIM_HIG_BORDER);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), dbox,
			gtk_label_new_with_mnemonic(_("_Advanced")));
	gtk_widget_show(dbox);

	/** Setup the bottom frames. */
	add_protocol_options(dialog, dbox);
	add_proxy_options(dialog, dbox);

	/* Setup the button box */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(main_vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Register button */
	button = gtk_button_new_with_label(_("Register"));
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(register_account_prefs_cb), dialog);

	dialog->register_button = button;

	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);

	if (!dialog->prpl_info || !dialog->prpl_info->register_user)
		gtk_widget_hide(button);

	/* Cancel button */
	button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(cancel_account_prefs_cb), dialog);

	/* Save button */
	button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);

	gtk_widget_show(button);

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

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(ok_account_prefs_cb), dialog);

	/* Show the window. */
	gtk_widget_show(win);
}

/**************************************************************************
 * Accounts Dialog
 **************************************************************************/
static void
signed_on_off_cb(GaimConnection *gc, gpointer user_data)
{
	GaimAccount *account;
	PidginPulseData *pulse_data;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	size_t index;

	/* Don't need to do anything if the accounts window is not visible */
	if (accounts_window == NULL)
		return;

	account = gaim_connection_get_account(gc);
	model = GTK_TREE_MODEL(accounts_window->model);
	index = g_list_index(gaim_accounts_get_all(), account);

	if (gtk_tree_model_iter_nth_child(model, &iter, NULL, index))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(accounts_window->model), &iter,
						   COLUMN_PULSE_DATA, &pulse_data, -1);

		if (pulse_data != NULL)
		{
			if (pulse_data->timeout > 0)
				g_source_remove(pulse_data->timeout);

			g_object_unref(G_OBJECT(pulse_data->online_pixbuf));

			g_free(pulse_data);
		}

		pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);
		if ((pixbuf != NULL) && gaim_account_is_disconnected(account))
			gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

		gtk_list_store_set(accounts_window->model, &iter,
				   COLUMN_ICON, pixbuf,
				   COLUMN_PULSE_DATA, NULL,
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
accounts_window_find_account_in_treemodel(GtkTreeIter *iter, GaimAccount *account)
{
	GtkTreeModel *model;
	GaimAccount *cur;

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
account_removed_cb(GaimAccount *account, gpointer user_data)
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

	if (gaim_accounts_get_all() == NULL)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(accounts_window->notebook), 0);
}

static void
account_abled_cb(GaimAccount *account, gpointer user_data)
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
	if (data->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE)) {
		GtkTreeRowReference *ref;
		GtkTreePath *source_row;
		GtkTreeIter iter;
		GaimAccount *account = NULL;
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

		gtk_selection_data_set(data, gdk_atom_intern("GAIM_ACCOUNT", FALSE),
							   8, (void *)&account, sizeof(account));

		gtk_tree_path_free(source_row);
	}
}

static void
move_account_after(GtkListStore *store, GtkTreeIter *iter,
				   GtkTreeIter *position)
{
	GtkTreeIter new_iter;
	GaimAccount *account;

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
	GaimAccount *account;

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
	if (sd->target == gdk_atom_intern("GAIM_ACCOUNT", FALSE) && sd->data) {
		gint dest_index;
		GaimAccount *a = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		memcpy(&a, sd->data, sizeof(a));

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y,
											  &path, &position)) {

			GtkTreeIter iter;
			GaimAccount *account;
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
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account) + 1;
					break;

				case GTK_TREE_VIEW_DROP_BEFORE:
				case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					dest_index = g_list_index(gaim_accounts_get_all(),
											  account);

					move_account_before(dialog->model, &dialog->drag_iter,
										&iter);
					break;

				default:
					return;
			}

			gaim_accounts_reorder(a, dest_index);
		}
	}
}

static gint
accedit_win_destroy_cb(GtkWidget *w, GdkEvent *event, AccountsWindow *dialog)
{
	pidgin_accounts_window_hide();

	return 0;
}

static gboolean
configure_cb(GtkWidget *w, GdkEventConfigure *event, AccountsWindow *dialog)
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
add_account_cb(GtkWidget *w, AccountsWindow *dialog)
{
	pidgin_account_dialog_show(PIDGIN_ADD_ACCOUNT_DIALOG, NULL);
}

static void
modify_account_sel(GtkTreeModel *model, GtkTreePath *path,
				   GtkTreeIter *iter, gpointer data)
{
	GaimAccount *account;

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
delete_account_cb(GaimAccount *account)
{
	gaim_accounts_delete(account);
}

static void
ask_delete_account_sel(GtkTreeModel *model, GtkTreePath *path,
					   GtkTreeIter *iter, gpointer data)
{
	GaimAccount *account;

	gtk_tree_model_get(model, iter, COLUMN_DATA, &account, -1);

	if (account != NULL) {
		char *buf;

		buf = g_strdup_printf(_("Are you sure you want to delete %s?"),
							  gaim_account_get_username(account));

		gaim_request_close_with_handle(account);
		gaim_request_action(account, NULL, buf, NULL, 0, account, 2,
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
	gtk_widget_destroy(dialog->window);

	pidgin_accounts_window_hide();
}


static void
enabled_cb(GtkCellRendererToggle *renderer, gchar *path_str,
			   gpointer data)
{
	AccountsWindow *dialog = (AccountsWindow *)data;
	GaimAccount *account;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	GtkTreeIter iter;
	gboolean enabled;
	const GaimSavedStatus *saved_status;

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
		saved_status = gaim_savedstatus_get_current();
		gaim_savedstatus_activate_for_account(saved_status, account);
	}

	gaim_account_set_enabled(account, PIDGIN_UI, !enabled);
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

	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);

	/* Screen Name column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Screen Name"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);

	/* Buddy Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "pixbuf", COLUMN_BUDDYICON);

	/* Screen Name */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer,
					   "text", COLUMN_SCREENNAME);
	dialog->screenname_col = column;


	/* Protocol name */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Protocol"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);

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
set_account(GtkListStore *store, GtkTreeIter *iter, GaimAccount *account, GdkPixbuf *global_buddyicon)
{
	GdkPixbuf *pixbuf, *buddyicon = NULL;
	const char *path = NULL;

	pixbuf = pidgin_create_prpl_icon(account, PIDGIN_PRPL_ICON_MEDIUM);
	if ((pixbuf != NULL) && gaim_account_is_disconnected(account))
		gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);

	if (gaim_account_get_bool(account, "use-global-buddyicon", TRUE)) {
		if (global_buddyicon != NULL)
			buddyicon = g_object_ref(G_OBJECT(global_buddyicon));
		/* This is for when set_account() is called for a single account */
		else
			path = gaim_prefs_get_path("/gaim/gtk/accounts/buddyicon");
	} else
		path = gaim_account_get_ui_string(account, PIDGIN_UI, "non-global-buddyicon-path", NULL);

	if (path != NULL) {
		GdkPixbuf *buddyicon_pixbuf = gdk_pixbuf_new_from_file(path, NULL);
		if (buddyicon_pixbuf != NULL) {
			buddyicon = gdk_pixbuf_scale_simple(buddyicon_pixbuf, 22, 22, GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(buddyicon_pixbuf));
		}
	}

	gtk_list_store_set(store, iter,
			COLUMN_ICON, pixbuf,
			COLUMN_BUDDYICON, buddyicon,
			COLUMN_SCREENNAME, gaim_account_get_username(account),
			COLUMN_ENABLED, gaim_account_get_enabled(account, PIDGIN_UI),
			COLUMN_PROTOCOL, gaim_account_get_protocol_name(account),
			COLUMN_DATA, account,
			-1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
	if (buddyicon != NULL)
		g_object_unref(G_OBJECT(buddyicon));
}

static void
add_account_to_liststore(GaimAccount *account, gpointer user_data)
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

	if ((path = gaim_prefs_get_path("/gaim/gtk/accounts/buddyicon")) != NULL) {
		GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, NULL);
		if (pixbuf != NULL) {
			global_buddyicon = gdk_pixbuf_scale_simple(pixbuf, 22, 22, GDK_INTERP_HYPER);
			g_object_unref(G_OBJECT(pixbuf));
		}
	}

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		ret = TRUE;
		add_account_to_liststore((GaimAccount *)l->data, global_buddyicon);
	}

	if (global_buddyicon != NULL)
		g_object_unref(G_OBJECT(global_buddyicon));

	return ret;
}

#if !GTK_CHECK_VERSION(2,2,0)
static void
get_selected_helper(GtkTreeModel *model, GtkTreePath *path,
					GtkTreeIter *iter, gpointer user_data)
{
	*((gboolean *)user_data) = TRUE;
}
#endif

static void
account_selected_cb(GtkTreeSelection *sel, AccountsWindow *dialog)
{
	gboolean selected = FALSE;

#if GTK_CHECK_VERSION(2,2,0)
	selected = (gtk_tree_selection_count_selected_rows(sel) > 0);
#else
	gtk_tree_selection_selected_foreach(sel, get_selected_helper, &selected);
#endif

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
	GaimAccount *account;
	const gchar *title;

	dialog = (AccountsWindow *)user_data;

	/* Figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(dialog->treeview), event->x, event->y, &path, &column, NULL, NULL))
		return FALSE;
	title = gtk_tree_view_column_get_title(column);
	/* The -1 is required because the first two columns of the list
	 * store are displayed as only one column in the tree view. */
	column = gtk_tree_view_get_column(treeview, COLUMN_ENABLED-1);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter, COLUMN_DATA, &account, -1);

	if ((account != NULL) && (event->button == 1) &&
		(event->type == GDK_2BUTTON_PRESS) &&
		(strcmp(gtk_tree_view_column_get_title(column), title)))
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
	GtkWidget *sw;
	GtkWidget *label;
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTargetEntry gte[] = {{"GAIM_ACCOUNT", GTK_TARGET_SAME_APP, 0}};
	char *pretty;

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

	accounts_window->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(accounts_window->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(accounts_window->notebook), FALSE);
	gtk_container_add(GTK_CONTAINER(frame), accounts_window->notebook);

	/* Create a helpful first-time-use label */
	label = gtk_label_new(NULL);
	/* Translators: Please maintain the use of -> or <- to represent the menu heirarchy */
	pretty = pidgin_make_pretty_arrows(_(
						 "<span size='larger' weight='bold'>Welcome to " PIDGIN_NAME "!</span>\n\n"
						 
						 "You have no IM accounts configured. To start connecting with " PIDGIN_NAME " "
						 "press the <b>Add</b> button below and configure your first "
						 "account. If you want " PIDGIN_NAME " to connect to multiple IM accounts, "
						 "press <b>Add</b> again to configure them all.\n\n"
						 
						 "You can come back to this window to add, edit, or remove "
						 "accounts from <b>Accounts->Add/Edit</b> in the Buddy "
						 "List window"));
	gtk_label_set_markup(GTK_LABEL(label), pretty);
	g_free(pretty);

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);

	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_notebook_append_page(GTK_NOTEBOOK(accounts_window->notebook), label, NULL);

	/* Create the scrolled window. */
	sw = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
					GTK_SHADOW_NONE);
	gtk_notebook_append_page(GTK_NOTEBOOK(accounts_window->notebook), sw, NULL);
	gtk_widget_show(sw);

	/* Create the list model. */
	dialog->model = gtk_list_store_new(NUM_COLUMNS,
					GDK_TYPE_PIXBUF,   /* COLUMN_ICON */
					GDK_TYPE_PIXBUF,   /* COLUMN_BUDDYICON */
					G_TYPE_STRING,     /* COLUMN_SCREENNAME */
					G_TYPE_BOOLEAN,    /* COLUMN_ENABLED */
					G_TYPE_STRING,     /* COLUMN_PROTOCOL */
					G_TYPE_POINTER,    /* COLUMN_DATA */
					G_TYPE_POINTER     /* COLUMN_PULSE_DATA */
					);

	/* And now the actual treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(account_selected_cb), dialog);

	/* Handle double-clicking */
	g_signal_connect(G_OBJECT(treeview), "button_press_event",
					 G_CALLBACK(account_treeview_double_click_cb), dialog);

	gtk_container_add(GTK_CONTAINER(sw), treeview);

	add_columns(treeview, dialog);

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
account_modified_cb(GaimAccount *account, AccountsWindow *window)
{
	GtkTreeIter iter;

	if (!accounts_window_find_account_in_treemodel(&iter, account))
		return;

	set_account(window->model, &iter, account, NULL);
}

static void
global_buddyicon_changed(const char *name, GaimPrefType type,
			gconstpointer value, gpointer window)
{
	GList *list;
	for (list = gaim_accounts_get_all(); list; list = list->next) {
		account_modified_cb(list->data, window);
	}
}

void
pidgin_accounts_window_show(void)
{
	AccountsWindow *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *button;
	int width, height;


	if (accounts_window != NULL) {
		gtk_window_present(GTK_WINDOW(accounts_window->window));
		return;
	}

	accounts_window = dialog = g_new0(AccountsWindow, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/height");

	dialog->window = win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "accounts");
	gtk_window_set_title(GTK_WINDOW(win), _("Accounts"));
	gtk_container_set_border_width(GTK_CONTAINER(win), GAIM_HIG_BORDER);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(accedit_win_destroy_cb), accounts_window);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(configure_cb), accounts_window);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, GAIM_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = create_accounts_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), GAIM_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(add_account_cb), dialog);

	/* Modify button */
	button = gtk_button_new_from_stock(PIDGIN_STOCK_MODIFY);
	dialog->modify_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(modify_account_cb), dialog);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	dialog->delete_button = button;
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(button, FALSE);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(ask_delete_account_cb), dialog);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(close_accounts_cb), dialog);

	gaim_signal_connect(pidgin_account_get_handle(), "account-modified",
	                    accounts_window,
	                    GAIM_CALLBACK(account_modified_cb), accounts_window);
	gaim_prefs_connect_callback(accounts_window,
	                    "/gaim/gtk/accounts/buddyicon",
	                    global_buddyicon_changed, accounts_window);

	gtk_widget_show(win);
}

void
pidgin_accounts_window_hide(void)
{
	if (accounts_window == NULL)
		return;

	gaim_signals_disconnect_by_handle(accounts_window);
	gaim_prefs_disconnect_by_handle(accounts_window);

	g_free(accounts_window);
	accounts_window = NULL;

	/* See if we're the main window here. */
	if (PIDGIN_BLIST(gaim_get_blist())->window == NULL &&
		gaim_connections_get_all() == NULL) {

		gaim_core_quit();
	}
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
	GaimConnection *gc = gaim_account_get_connection(data->account);

	if (g_list_find(gaim_connections_get_all(), gc))
	{
		gaim_blist_request_add_buddy(data->account, data->username,
									 NULL, data->alias);
	}

	free_add_user_data(data);
}

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
pidgin_accounts_notify_added(GaimAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *msg)
{
	char *buffer;
	GaimConnection *gc;
	GtkWidget *alert;

	gc = gaim_account_get_connection(account);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	alert = pidgin_make_mini_dialog(gc, PIDGIN_STOCK_DIALOG_INFO, buffer,
					  NULL, NULL, _("Close"), NULL, NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);
}

static void
pidgin_accounts_request_add(GaimAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *msg)
{
	char *buffer;
	GaimConnection *gc;
	PidginAccountAddUserData *data;
	GtkWidget *alert;

	gc = gaim_account_get_connection(account);

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

struct auth_and_add {
	GaimAccountRequestAuthorizationCb auth_cb;
	GaimAccountRequestAuthorizationCb deny_cb;
	void *data;
	char *username;
	char *alias;
	GaimAccount *account;
};

static void
authorize_and_add_cb(struct auth_and_add *aa)
{
	aa->auth_cb(aa->data);
	gaim_blist_request_add_buddy(aa->account, aa->username,
	 	                    NULL, aa->alias);

	g_free(aa->username);
	g_free(aa->alias);
	g_free(aa);
}

static void
deny_no_add_cb(struct auth_and_add *aa)
{
	aa->deny_cb(aa->data);

	g_free(aa->username);
	g_free(aa->alias);
	g_free(aa);
}

static void *
pidgin_accounts_request_authorization(GaimAccount *account, const char *remote_user,
					const char *id, const char *alias, const char *message, gboolean on_list,
					GCallback auth_cb, GCallback deny_cb, void *user_data)
{
	char *buffer;
	GaimConnection *gc;
	GtkWidget *alert;

	gc = gaim_account_get_connection(account);
	if (message != NULL && *message == '\0')
		message = NULL;

	buffer = g_strdup_printf(_("%s%s%s%s wants to add %s to his or her buddy list%s%s"),
				remote_user,
	 	                (alias != NULL ? " ("  : ""),
		                (alias != NULL ? alias : ""),
		                (alias != NULL ? ")"   : ""),
		                (id != NULL
		                ? id
		                : (gaim_connection_get_display_name(gc) != NULL
		                ? gaim_connection_get_display_name(gc)
		                : gaim_account_get_username(account))),
		                (message != NULL ? ": " : "."),
		                (message != NULL ? message  : ""));


	if (!on_list) {
		struct auth_and_add *aa = g_new0(struct auth_and_add, 1);
		aa->auth_cb = (GaimAccountRequestAuthorizationCb)auth_cb;
		aa->deny_cb = (GaimAccountRequestAuthorizationCb)deny_cb;
		aa->data = user_data;
		aa->username = g_strdup(remote_user);
		aa->alias = g_strdup(alias);
		aa->account = account;
		alert = pidgin_make_mini_dialog(gc, PIDGIN_STOCK_DIALOG_QUESTION,
						  _("Authorize buddy?"), buffer, aa,
						  _("Authorize"), authorize_and_add_cb, 
						  _("Deny"), deny_no_add_cb, 
						  NULL);
	} else {
		alert = pidgin_make_mini_dialog(gc, PIDGIN_STOCK_DIALOG_QUESTION,
						  _("Authorize buddy?"), buffer, user_data,
						  _("Authorize"), auth_cb, 
						  _("Deny"), deny_cb, 
						  NULL);
	}
	pidgin_blist_add_alert(alert);

	g_free(buffer);
	
	return NULL;
}

static void
pidgin_accounts_request_close(void *ui_handle)
{
	
}

static GaimAccountUiOps ui_ops =
{
	pidgin_accounts_notify_added,
	NULL,
	pidgin_accounts_request_add,
	pidgin_accounts_request_authorization,
	pidgin_accounts_request_close
};

GaimAccountUiOps *
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
	gaim_prefs_add_none("/gaim/gtk/accounts");
	gaim_prefs_add_none("/gaim/gtk/accounts/dialog");
	gaim_prefs_add_int("/gaim/gtk/accounts/dialog/width",  520);
	gaim_prefs_add_int("/gaim/gtk/accounts/dialog/height", 321);
	gaim_prefs_add_path("/gaim/gtk/accounts/buddyicon", NULL);

	gaim_signal_register(pidgin_account_get_handle(), "account-modified",
						 gaim_marshal_VOID__POINTER, NULL, 1,
						 gaim_value_new(GAIM_TYPE_SUBTYPE,
										GAIM_SUBTYPE_ACCOUNT));

	/* Setup some gaim signal handlers. */
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
	gaim_signal_connect(gaim_connections_get_handle(), "signed-off",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(signed_on_off_cb), NULL);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-added",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(add_account_to_liststore), NULL);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-removed",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(account_removed_cb), NULL);
	gaim_signal_connect(gaim_accounts_get_handle(), "account-disabled",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(account_abled_cb), GINT_TO_POINTER(FALSE));
	gaim_signal_connect(gaim_accounts_get_handle(), "account-enabled",
						pidgin_account_get_handle(),
						GAIM_CALLBACK(account_abled_cb), GINT_TO_POINTER(TRUE));

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

	gaim_signals_disconnect_by_handle(pidgin_account_get_handle());
	gaim_signals_unregister_by_instance(pidgin_account_get_handle());
}


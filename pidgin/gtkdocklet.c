/*
 * System tray icon (aka docklet) plugin for Purple
 *
 * Copyright (C) 2002-3 Robert McQueen <robot101@debian.org>
 * Copyright (C) 2003 Herman Bloggs <hermanator12002@yahoo.com>
 * Inspired by a similar plugin by:
 *  John (J5) Palmieri <johnp@martianrock.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"
#include "pidgin.h"

#include "core.h"
#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "sound.h"
#include "status.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtksavedstatuses.h"
#include "gtksound.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"
#include "pidginstock.h"
#include "gtkdocklet.h"
#include "gtkdialogs.h"
#include "gtknotify.h"

#include "gtk3compat.h"

#ifndef DOCKLET_TOOLTIP_LINE_LIMIT
#define DOCKLET_TOOLTIP_LINE_LIMIT 5
#endif

#define SHORT_EMBED_TIMEOUT 5
#define LONG_EMBED_TIMEOUT 15

/* globals */
static GtkStatusIcon *docklet = NULL;
static guint embed_timeout = 0;
static PurpleStatusPrimitive status = PURPLE_STATUS_OFFLINE;
static PidginDockletFlag flags = 0;
static gboolean enable_join_chat = FALSE;
static gboolean visible = FALSE;
static gboolean visibility_manager = FALSE;

/* protos */
static void docklet_gtk_status_create(gboolean);
static void docklet_gtk_status_destroy(void);

/**************************************************************************
 * docklet status and utility functions
 **************************************************************************/
static inline gboolean
docklet_is_blinking()
{
	return flags && !(flags & PIDGIN_DOCKLET_CONNECTING);
}

static void
docklet_gtk_status_update_icon(PurpleStatusPrimitive status, PidginDockletFlag newflag)
{
	const gchar *icon_name = NULL;

	switch (status) {
		case PURPLE_STATUS_OFFLINE:
			icon_name = PIDGIN_STOCK_TRAY_OFFLINE;
			break;
		case PURPLE_STATUS_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_AWAY;
			break;
		case PURPLE_STATUS_UNAVAILABLE:
			icon_name = PIDGIN_STOCK_TRAY_BUSY;
			break;
		case PURPLE_STATUS_EXTENDED_AWAY:
			icon_name = PIDGIN_STOCK_TRAY_XA;
			break;
		case PURPLE_STATUS_INVISIBLE:
			icon_name = PIDGIN_STOCK_TRAY_INVISIBLE;
			break;
		default:
			icon_name = PIDGIN_STOCK_TRAY_AVAILABLE;
			break;
	}

	if (newflag & PIDGIN_DOCKLET_EMAIL_PENDING)
		icon_name = PIDGIN_STOCK_TRAY_EMAIL;
	if (newflag & PIDGIN_DOCKLET_CONV_PENDING)
		icon_name = PIDGIN_STOCK_TRAY_PENDING;
	if (newflag & PIDGIN_DOCKLET_CONNECTING)
		icon_name = PIDGIN_STOCK_TRAY_CONNECT;

	if (icon_name) {
		gtk_status_icon_set_from_icon_name(docklet, icon_name);
	}

#if !GTK_CHECK_VERSION(3,0,0)
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/blink")) {
		gboolean pending = FALSE;
		pending |= (newflag & PIDGIN_DOCKLET_EMAIL_PENDING);
		pending |= (newflag & PIDGIN_DOCKLET_CONV_PENDING);
		gtk_status_icon_set_blinking(docklet, pending &&
			!(newflag & PIDGIN_DOCKLET_CONNECTING));
	} else if (gtk_status_icon_get_blinking(docklet)) {
		gtk_status_icon_set_blinking(docklet, FALSE);
	}
#endif
}

static GList *
get_pending_list(guint max)
{
	GList *l_im, *l_chat;

	l_im = pidgin_conversations_get_unseen_ims(PIDGIN_UNSEEN_TEXT, FALSE, max);

	/* Short circuit if we have our information already */
	if (max == 1 && l_im != NULL)
		return l_im;

	l_chat = pidgin_conversations_get_unseen_chats(
	    purple_prefs_get_int(PIDGIN_PREFS_ROOT "/conversations/notification_chat"),
							 FALSE, max);

	if (l_im != NULL && l_chat != NULL)
		return g_list_concat(l_im, l_chat);
	else if (l_im != NULL)
		return l_im;
	else
		return l_chat;
}

static gboolean
docklet_update_status(void)
{
	GList *convs, *l;
	int count;
	PurpleSavedStatus *saved_status;
	PurpleStatusPrimitive newstatus = PURPLE_STATUS_OFFLINE;
	PidginDockletFlag newflags = 0;

	/* get the current savedstatus */
	saved_status = purple_savedstatus_get_current();

	/* determine if any ims have unseen messages */
	convs = get_pending_list(DOCKLET_TOOLTIP_LINE_LIMIT);

	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "pending")) {
		if (convs && !visible) {
			g_list_free(convs);
			docklet_gtk_status_create(FALSE);
			return FALSE;
		} else if (!convs && visible) {
			docklet_gtk_status_destroy();
			return FALSE;
		}
	}

	if (!visible) {
		g_list_free(convs);
		return FALSE;
	}

	if (convs != NULL) {
		/* set tooltip if messages are pending */
		GString *tooltip_text = g_string_new("");
		newflags |= PIDGIN_DOCKLET_CONV_PENDING;

		for (l = convs, count = 0 ; l != NULL ; l = l->next, count++) {
			PurpleConversation *conv = (PurpleConversation *)l->data;
			PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);

			if (count == DOCKLET_TOOLTIP_LINE_LIMIT - 1) {
				g_string_append(tooltip_text, _("Right-click for more unread messages...\n"));
			} else if(gtkconv) {
				g_string_append_printf(tooltip_text,
					ngettext("%d unread message from %s\n", "%d unread messages from %s\n", gtkconv->unseen_count),
					gtkconv->unseen_count,
					purple_conversation_get_title(conv));
			} else {
				g_string_append_printf(tooltip_text,
					ngettext("%d unread message from %s\n", "%d unread messages from %s\n",
					GPOINTER_TO_INT(purple_conversation_get_data(conv, "unseen-count"))),
					GPOINTER_TO_INT(purple_conversation_get_data(conv, "unseen-count")),
					purple_conversation_get_title(conv));
			}
		}

		/* get rid of the last newline */
		if (tooltip_text->len > 0)
			tooltip_text = g_string_truncate(tooltip_text, tooltip_text->len - 1);

		gtk_status_icon_set_tooltip_text(docklet, tooltip_text->str);

		g_string_free(tooltip_text, TRUE);
		g_list_free(convs);

	} else {
		char *tooltip_text = g_strconcat(PIDGIN_NAME, " - ",
			purple_savedstatus_get_title(saved_status), NULL);
		gtk_status_icon_set_tooltip_text(docklet, tooltip_text);
		g_free(tooltip_text);
	}

	for(l = purple_accounts_get_all(); l != NULL; l = l->next) {

		PurpleAccount *account = (PurpleAccount*)l->data;

		if (!purple_account_get_enabled(account, PIDGIN_UI))
			continue;

		if (purple_account_is_disconnected(account))
			continue;

		if (purple_account_is_connecting(account))
			newflags |= PIDGIN_DOCKLET_CONNECTING;

		if (pidgin_notify_emails_pending())
			newflags |= PIDGIN_DOCKLET_EMAIL_PENDING;
	}

	newstatus = purple_savedstatus_get_type(saved_status);

	/* update the icon if we changed status */
	if (status != newstatus || flags != newflags) {
		status = newstatus;
		flags = newflags;

		docklet_gtk_status_update_icon(status, flags);
	}

	return FALSE; /* for when we're called by the glib idle handler */
}

static gboolean
online_account_supports_chat(void)
{
	GList *c = NULL;
	c = purple_connections_get_all();

	while(c != NULL) {
		PurpleConnection *gc = c->data;
		PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc));
		if (prpl_info != NULL && prpl_info->chat_info != NULL)
			return TRUE;
		c = c->next;
	}

	return FALSE;
}

/**************************************************************************
 * callbacks and signal handlers
 **************************************************************************/
#if 0
static void
pidgin_quit_cb()
{
	/* TODO: confirm quit while pending */
}
#endif

static void
docklet_update_status_cb(void *data)
{
	docklet_update_status();
}

static void
docklet_conv_updated_cb(PurpleConversation *conv, PurpleConversationUpdateType type)
{
	if (type == PURPLE_CONVERSATION_UPDATE_UNSEEN)
		docklet_update_status();
}

static void
docklet_signed_on_cb(PurpleConnection *gc)
{
	if (!enable_join_chat) {
		if (PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc))->chat_info != NULL)
			enable_join_chat = TRUE;
	}
	docklet_update_status();
}

static void
docklet_signed_off_cb(PurpleConnection *gc)
{
	if (enable_join_chat) {
		if (PURPLE_PLUGIN_PROTOCOL_INFO(purple_connection_get_prpl(gc))->chat_info != NULL)
			enable_join_chat = online_account_supports_chat();
	}
	docklet_update_status();
}

static void
docklet_show_pref_changed_cb(const char *name, PurplePrefType type,
			     gconstpointer value, gpointer data)
{
	const char *val = value;
	if (!strcmp(val, "always")) {
		if (!visible)
			docklet_gtk_status_create(FALSE);
		else if (!visibility_manager) {
			pidgin_blist_visibility_manager_add();
			visibility_manager = TRUE;
		}
	} else if (!strcmp(val, "never")) {
		if (visible)
			docklet_gtk_status_destroy();
	} else {
		if (visibility_manager) {
			pidgin_blist_visibility_manager_remove();
			visibility_manager = FALSE;
		}
		docklet_update_status();
	}
}

/**************************************************************************
 * docklet pop-up menu
 **************************************************************************/
static void
docklet_toggle_mute(GtkWidget *toggle, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/sound/mute",
	                      gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)));
}

#if !GTK_CHECK_VERSION(3,0,0)
static void
docklet_toggle_blink(GtkWidget *toggle, void *data)
{
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/blink",
	                      gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)));
}
#endif

static void
docklet_toggle_blist(GtkWidget *toggle, void *data)
{
	purple_blist_set_visible(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)));
}

#ifdef _WIN32
/* This is a workaround for a bug in windows GTK+. Clicking outside of the
   menu does not get rid of it, so instead we get rid of it as soon as the
   pointer leaves the menu. */
static gboolean
hide_docklet_menu(gpointer data)
{
	if (data != NULL) {
		gtk_menu_popdown(GTK_MENU(data));
	}
	return FALSE;
}

static gboolean
docklet_menu_leave_enter(GtkWidget *menu, GdkEventCrossing *event, void *data)
{
	static guint hide_docklet_timer = 0;

	if (event->type == GDK_LEAVE_NOTIFY && (event->detail == GDK_NOTIFY_ANCESTOR ||
			event->detail == GDK_NOTIFY_UNKNOWN)) {
		purple_debug(PURPLE_DEBUG_INFO, "docklet", "menu leave-notify-event\n");
		/* Add some slop so that the menu doesn't annoyingly disappear when mousing around */
		if (hide_docklet_timer == 0) {
			hide_docklet_timer = purple_timeout_add(500,
					hide_docklet_menu, menu);
		}
	} else if (event->type == GDK_ENTER_NOTIFY && event->detail == GDK_NOTIFY_ANCESTOR) {
		purple_debug(PURPLE_DEBUG_INFO, "docklet", "menu enter-notify-event\n");
		if (hide_docklet_timer != 0) {
			/* Cancel the hiding if we reenter */

			purple_timeout_remove(hide_docklet_timer);
			hide_docklet_timer = 0;
		}
	}
	return FALSE;
}
#endif

/* There is a lot of code here for handling the status submenu, much of
 * which is duplicated from the gtkstatusbox. It'd be nice to add API
 * somewhere to simplify this (either in the statusbox, or in libpurple).
 */
static void
show_custom_status_editor_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	PurpleSavedStatus *saved_status;
	saved_status = purple_savedstatus_get_current();

	if (purple_savedstatus_get_type(saved_status) == PURPLE_STATUS_AVAILABLE)
		saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AWAY);

	pidgin_status_editor_show(FALSE,
		purple_savedstatus_is_transient(saved_status) ? saved_status : NULL);
}

static PurpleSavedStatus *
create_transient_status(PurpleStatusPrimitive primitive, PurpleStatusType *status_type)
{
	PurpleSavedStatus *saved_status = purple_savedstatus_new(NULL, primitive);

	if(status_type != NULL) {
		GList *tmp, *active_accts = purple_accounts_get_all_active();
		for (tmp = active_accts; tmp != NULL; tmp = tmp->next) {
			purple_savedstatus_set_substatus(saved_status,
				(PurpleAccount*) tmp->data, status_type, NULL);
		}
		g_list_free(active_accts);
	}

	return saved_status;
}

static void
activate_status_account_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	PurpleStatusType *status_type;
	PurpleStatusPrimitive primitive;
	PurpleSavedStatus *saved_status = NULL;
	GList *iter = purple_savedstatuses_get_all();
	GList *tmp, *active_accts = purple_accounts_get_all_active();

	status_type = (PurpleStatusType *)user_data;
	primitive = purple_status_type_get_primitive(status_type);

	for (; iter != NULL; iter = iter->next) {
		PurpleSavedStatus *ss = iter->data;
		if ((purple_savedstatus_get_type(ss) == primitive) && purple_savedstatus_is_transient(ss) &&
			purple_savedstatus_has_substatuses(ss))
		{
			gboolean found = FALSE;
			/* The currently enabled accounts must have substatuses for all the active accts */
			for(tmp = active_accts; tmp != NULL; tmp = tmp->next) {
				PurpleAccount *acct = tmp->data;
				PurpleSavedStatusSub *sub = purple_savedstatus_get_substatus(ss, acct);
				if (sub) {
					const PurpleStatusType *sub_type = purple_savedstatus_substatus_get_type(sub);
					const char *subtype_status_id = purple_status_type_get_id(sub_type);
					if (subtype_status_id && !strcmp(subtype_status_id,
							purple_status_type_get_id(status_type)))
						found = TRUE;
				}
			}
			if (!found)
				continue;
			saved_status = ss;
			break;
		}
	}

	g_list_free(active_accts);

	/* Create a new transient saved status if we weren't able to find one */
	if (saved_status == NULL)
		saved_status = create_transient_status(primitive, status_type);

	/* Set the status for each account */
	purple_savedstatus_activate(saved_status);
}

static void
activate_status_primitive_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	PurpleStatusPrimitive primitive;
	PurpleSavedStatus *saved_status;

	primitive = GPOINTER_TO_INT(user_data);

	/* Try to lookup an already existing transient saved status */
	saved_status = purple_savedstatus_find_transient_by_type_and_message(primitive, NULL);

	/* Create a new transient saved status if we weren't able to find one */
	if (saved_status == NULL)
		saved_status = create_transient_status(primitive, NULL);

	/* Set the status for each account */
	purple_savedstatus_activate(saved_status);
}

static void
activate_saved_status_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	time_t creation_time;
	PurpleSavedStatus *saved_status;

	creation_time = GPOINTER_TO_INT(user_data);
	saved_status = purple_savedstatus_find_by_creation_time(creation_time);
	if (saved_status != NULL)
		purple_savedstatus_activate(saved_status);
}

static GtkWidget *
new_menu_item_with_status_icon(GtkWidget *menu, const char *str, PurpleStatusPrimitive primitive, GCallback cb, gpointer data, guint accel_key, guint accel_mods, char *mod)
{
	GtkWidget *menuitem;
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	menuitem = gtk_image_menu_item_new_with_label(str);

	if (menu)
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	if (cb)
		g_signal_connect(G_OBJECT(menuitem), "activate", cb, data);

	pixbuf = pidgin_create_status_icon(primitive, menu, PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem), image);

	gtk_widget_show_all(menuitem);

	return menuitem;
}

static void
add_account_statuses(GtkWidget *menu, PurpleAccount *account)
{
	GList *l;

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next) {
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;
		PurpleStatusPrimitive prim;

		if (!purple_status_type_is_user_settable(status_type))
			continue;

		prim = purple_status_type_get_primitive(status_type);

		new_menu_item_with_status_icon(menu,
			purple_status_type_get_name(status_type),
			prim, G_CALLBACK(activate_status_account_cb),
			GINT_TO_POINTER(status_type), 0, 0, NULL);
	}
}

static GtkWidget *
docklet_status_submenu(void)
{
	GtkWidget *submenu, *menuitem;
	GList *popular_statuses, *cur;
	PidginStatusBox *statusbox = NULL;

	submenu = gtk_menu_new();
	menuitem = gtk_menu_item_new_with_mnemonic(_("_Change Status"));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

	if(pidgin_blist_get_default_gtk_blist() != NULL) {
		statusbox = PIDGIN_STATUS_BOX(pidgin_blist_get_default_gtk_blist()->statusbox);
	}

	if(statusbox && statusbox->account != NULL) {
		add_account_statuses(submenu, statusbox->account);
	} else if(statusbox && statusbox->token_status_account != NULL) {
		add_account_statuses(submenu, statusbox->token_status_account);
	} else {
		new_menu_item_with_status_icon(submenu, _("Available"),
			PURPLE_STATUS_AVAILABLE, G_CALLBACK(activate_status_primitive_cb),
			GINT_TO_POINTER(PURPLE_STATUS_AVAILABLE), 0, 0, NULL);

		new_menu_item_with_status_icon(submenu, _("Away"),
			PURPLE_STATUS_AWAY, G_CALLBACK(activate_status_primitive_cb),
			GINT_TO_POINTER(PURPLE_STATUS_AWAY), 0, 0, NULL);

		new_menu_item_with_status_icon(submenu, _("Do not disturb"),
			PURPLE_STATUS_UNAVAILABLE, G_CALLBACK(activate_status_primitive_cb),
			GINT_TO_POINTER(PURPLE_STATUS_UNAVAILABLE), 0, 0, NULL);

		new_menu_item_with_status_icon(submenu, _("Invisible"),
			PURPLE_STATUS_INVISIBLE, G_CALLBACK(activate_status_primitive_cb),
			GINT_TO_POINTER(PURPLE_STATUS_INVISIBLE), 0, 0, NULL);

		new_menu_item_with_status_icon(submenu, _("Offline"),
			PURPLE_STATUS_OFFLINE, G_CALLBACK(activate_status_primitive_cb),
			GINT_TO_POINTER(PURPLE_STATUS_OFFLINE), 0, 0, NULL);
	}

	popular_statuses = purple_savedstatuses_get_popular(6);
	if (popular_statuses != NULL)
		pidgin_separator(submenu);
	for (cur = popular_statuses; cur != NULL; cur = cur->next)
	{
		PurpleSavedStatus *saved_status = cur->data;
		time_t creation_time = purple_savedstatus_get_creation_time(saved_status);
		new_menu_item_with_status_icon(submenu,
			purple_savedstatus_get_title(saved_status),
			purple_savedstatus_get_type(saved_status), G_CALLBACK(activate_saved_status_cb),
			GINT_TO_POINTER(creation_time), 0, 0, NULL);
	}
	g_list_free(popular_statuses);

	pidgin_separator(submenu);

	pidgin_new_item_from_stock(submenu, _("New..."), NULL, G_CALLBACK(show_custom_status_editor_cb), NULL, 0, 0, NULL);
	pidgin_new_item_from_stock(submenu, _("Saved..."), NULL, G_CALLBACK(pidgin_status_window_show), NULL, 0, 0, NULL);

	return menuitem;
}



static void
plugin_act(GtkWidget *widget, PurplePluginAction *pam)
{
	if (pam && pam->callback)
		pam->callback(pam);
}

static void
build_plugin_actions(GtkWidget *menu, PurplePlugin *plugin,
		gpointer context)
{
	GtkWidget *menuitem;
	PurplePluginAction *action = NULL;
	GList *actions, *l;

	actions = PURPLE_PLUGIN_ACTIONS(plugin, context);

	for (l = actions; l != NULL; l = l->next)
	{
		if (l->data)
		{
			action = (PurplePluginAction *) l->data;
			action->plugin = plugin;
			action->context = context;

			menuitem = gtk_menu_item_new_with_label(action->label);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

			g_signal_connect(G_OBJECT(menuitem), "activate",
					G_CALLBACK(plugin_act), action);
			g_object_set_data_full(G_OBJECT(menuitem), "plugin_action",
								   action,
								   (GDestroyNotify)purple_plugin_action_free);
			gtk_widget_show(menuitem);
		}
		else
			pidgin_separator(menu);
	}

	g_list_free(actions);
}


static void
docklet_plugin_actions(GtkWidget *menu)
{
	GtkWidget *menuitem, *submenu;
	PurplePlugin *plugin = NULL;
	GList *l;
	int c = 0;

	g_return_if_fail(menu != NULL);

	/* Add a submenu for each plugin with custom actions */
	for (l = purple_plugins_get_loaded(); l; l = l->next) {
		plugin = (PurplePlugin *) l->data;

		if (PURPLE_IS_PROTOCOL_PLUGIN(plugin))
			continue;

		if (!PURPLE_PLUGIN_HAS_ACTIONS(plugin))
			continue;

		menuitem = gtk_image_menu_item_new_with_label(_(plugin->info->name));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

		build_plugin_actions(submenu, plugin, NULL);

		c++;
	}
	if(c>0)
		pidgin_separator(menu);
}

static void
docklet_menu(void)
{
	static GtkWidget *menu = NULL;
	GtkWidget *menuitem;
	GtkMenuPositionFunc pos_func = gtk_status_icon_position_menu;

	if (menu) {
		gtk_widget_destroy(menu);
	}

	menu = gtk_menu_new();

	menuitem = gtk_check_menu_item_new_with_mnemonic(_("Show Buddy _List"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_blist), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Unread Messages"));

	if (flags & PIDGIN_DOCKLET_CONV_PENDING) {
		GtkWidget *submenu = gtk_menu_new();
		GList *l = get_pending_list(0);
		if (l == NULL) {
			gtk_widget_set_sensitive(menuitem, FALSE);
			purple_debug_warning("docklet",
				"status indicates messages pending, but no conversations with unseen messages were found.");
		} else {
			pidgin_conversations_fill_menu(submenu, l);
			g_list_free(l);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		}
	} else {
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	pidgin_separator(menu);

	menuitem = pidgin_new_item_from_stock(menu, _("New _Message..."), PIDGIN_STOCK_TOOLBAR_MESSAGE_NEW, G_CALLBACK(pidgin_dialogs_im), NULL, 0, 0, NULL);
	if (status == PURPLE_STATUS_OFFLINE)
		gtk_widget_set_sensitive(menuitem, FALSE);

	menuitem = pidgin_new_item_from_stock(menu, _("Join Chat..."), PIDGIN_STOCK_CHAT,
			G_CALLBACK(pidgin_blist_joinchat_show), NULL, 0, 0, NULL);
	if (status == PURPLE_STATUS_OFFLINE)
		gtk_widget_set_sensitive(menuitem, FALSE);

	menuitem = docklet_status_submenu();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	pidgin_separator(menu);

	pidgin_new_item_from_stock(menu, _("_Accounts"), NULL, G_CALLBACK(pidgin_accounts_window_show), NULL, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("Plu_gins"), PIDGIN_STOCK_TOOLBAR_PLUGINS, G_CALLBACK(pidgin_plugin_dialog_show), NULL, 0, 0, NULL);
	pidgin_new_item_from_stock(menu, _("Pr_eferences"), GTK_STOCK_PREFERENCES, G_CALLBACK(pidgin_prefs_show), NULL, 0, 0, NULL);

	pidgin_separator(menu);

	menuitem = gtk_check_menu_item_new_with_mnemonic(_("Mute _Sounds"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/sound/mute"));
	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/sound/method"), "none"))
		gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_mute), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

#if !GTK_CHECK_VERSION(3,0,0)
	menuitem = gtk_check_menu_item_new_with_mnemonic(_("_Blink on New Message"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/blink"));
	g_signal_connect(G_OBJECT(menuitem), "toggled", G_CALLBACK(docklet_toggle_blink), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
#endif

	pidgin_separator(menu);

	/* add plugin actions */
	docklet_plugin_actions(menu);

	pidgin_new_item_from_stock(menu, _("_Quit"), GTK_STOCK_QUIT, G_CALLBACK(purple_core_quit), NULL, 0, 0, NULL);

#ifdef _WIN32
	g_signal_connect(menu, "leave-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
	g_signal_connect(menu, "enter-notify-event", G_CALLBACK(docklet_menu_leave_enter), NULL);
	pos_func = NULL;
#endif
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       pos_func,
		       docklet, 0, gtk_get_current_event_time());
}

static void
pidgin_docklet_clicked(int button_type)
{
	switch (button_type) {
		case 1:
			if (flags & PIDGIN_DOCKLET_EMAIL_PENDING) {
				pidgin_notify_emails_present(NULL);
			} else if (flags & PIDGIN_DOCKLET_CONV_PENDING) {
				GList *l = get_pending_list(1);
				if (l != NULL) {
					pidgin_conv_present_conversation((PurpleConversation *)l->data);
					g_list_free(l);
				}
			} else {
				pidgin_blist_toggle_visibility();
			}
			break;
		case 3:
			docklet_menu();
			break;
	}
}

static void
pidgin_docklet_embedded(void)
{
	if (!visibility_manager
	    && strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "pending")) {
		pidgin_blist_visibility_manager_add();
		visibility_manager = TRUE;
	}
	visible = TRUE;
	docklet_update_status();
	docklet_gtk_status_update_icon(status, flags);
}

static void
pidgin_docklet_remove(void)
{
	if (visible) {
		if (visibility_manager) {
			pidgin_blist_visibility_manager_remove();
			visibility_manager = FALSE;
		}
		visible = FALSE;
		status = PURPLE_STATUS_OFFLINE;
	}
}

#ifndef _WIN32
static gboolean
docklet_gtk_embed_timeout_cb(gpointer data)
{
	/* The docklet was not embedded within the timeout.
	 * Remove it as a visibility manager, but leave the plugin
	 * loaded so that it can embed automatically if/when a notification
	 * area becomes available.
	 */
	purple_debug_info("docklet", "failed to embed within timeout\n");
	pidgin_docklet_remove();
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);

	embed_timeout = 0;
	return FALSE;
}
#endif

static gboolean
docklet_gtk_embedded_cb(GtkWidget *widget, gpointer data)
{
	if (embed_timeout) {
		purple_timeout_remove(embed_timeout);
		embed_timeout = 0;
	}

	if (gtk_status_icon_is_embedded(docklet)) {
		purple_debug_info("docklet", "embedded\n");

		pidgin_docklet_embedded();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", TRUE);
	} else {
		purple_debug_info("docklet", "detached\n");

		pidgin_docklet_remove();
		purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);
	}

	return TRUE;
}

static void
docklet_gtk_status_activated_cb(GtkStatusIcon *status_icon, gpointer user_data)
{
	pidgin_docklet_clicked(1);
}

static void
docklet_gtk_status_clicked_cb(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data)
{
	purple_debug_info("docklet", "The button is %u\n", button);
#ifdef GDK_WINDOWING_QUARTZ
	/* You can only click left mouse button on MacOSX native GTK. Let that be the menu */
	pidgin_docklet_clicked(3);
#else
	pidgin_docklet_clicked(button);
#endif
}

static void
docklet_gtk_status_destroy(void)
{
	g_return_if_fail(docklet != NULL);

	pidgin_docklet_remove();

	if (embed_timeout) {
		purple_timeout_remove(embed_timeout);
		embed_timeout = 0;
	}

	gtk_status_icon_set_visible(docklet, FALSE);
	g_object_unref(G_OBJECT(docklet));
	docklet = NULL;

	purple_debug_info("docklet", "GTK+ destroyed\n");
}

static void
docklet_gtk_status_create(gboolean recreate)
{
	if (docklet) {
		/* if this is being called when a tray icon exists, it's because
		   something messed up. try destroying it before we proceed,
		   although docklet_refcount may be all hosed. hopefully won't happen. */
		purple_debug_warning("docklet", "trying to create icon but it already exists?\n");
		docklet_gtk_status_destroy();
	}

	docklet = gtk_status_icon_new();
	g_return_if_fail(docklet != NULL);

	g_signal_connect(G_OBJECT(docklet), "activate", G_CALLBACK(docklet_gtk_status_activated_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "popup-menu", G_CALLBACK(docklet_gtk_status_clicked_cb), NULL);
	g_signal_connect(G_OBJECT(docklet), "notify::embedded", G_CALLBACK(docklet_gtk_embedded_cb), NULL);

	gtk_status_icon_set_visible(docklet, TRUE);

	/* This is a hack to avoid a race condition between the docklet getting
	 * embedded in the notification area and the gtkblist restoring its
	 * previous visibility state.  If the docklet does not get embedded within
	 * the timeout, it will be removed as a visibility manager until it does
	 * get embedded.  Ideally, we would only call docklet_embedded() when the
	 * icon was actually embedded. This only happens when the docklet is first
	 * created, not when being recreated.
	 *
	 * The gtk docklet tracks whether it successfully embedded in a pref and
	 * allows for a longer timeout period if it successfully embedded the last
	 * time it was run. This should hopefully solve problems with the buddy
	 * list not properly starting hidden when Pidgin is started on login.
	 */
	if (!recreate) {
		pidgin_docklet_embedded();
#ifndef _WIN32
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded")) {
			embed_timeout = purple_timeout_add_seconds(LONG_EMBED_TIMEOUT, docklet_gtk_embed_timeout_cb, NULL);
		} else {
			embed_timeout = purple_timeout_add_seconds(SHORT_EMBED_TIMEOUT, docklet_gtk_embed_timeout_cb, NULL);
		}
#endif
	}

	purple_debug_info("docklet", "GTK+ created\n");
}

/**************************************************************************
 * public api
 **************************************************************************/
 
void*
pidgin_docklet_get_handle()
{
	static int i;
	return &i;
}

GtkStatusIcon *
pidgin_docklet_get_status_icon(void)
{
	return docklet;
}

void
pidgin_docklet_init()
{
	void *conn_handle = purple_connections_get_handle();
	void *conv_handle = purple_conversations_get_handle();
	void *accounts_handle = purple_accounts_get_handle();
	void *status_handle = purple_savedstatuses_get_handle();
	void *docklet_handle = pidgin_docklet_get_handle();
	void *notify_handle = purple_notify_get_handle();
	gchar *tmp;

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/docklet");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/blink", FALSE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/docklet/show", "always");
	purple_prefs_connect_callback(docklet_handle, PIDGIN_PREFS_ROOT "/docklet/show",
				    docklet_show_pref_changed_cb, NULL);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/conversations/notification_chat", PIDGIN_UNSEEN_TEXT);

	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/docklet/gtk");
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/docklet/x11/embedded")) {
		purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", TRUE);
		purple_prefs_remove(PIDGIN_PREFS_ROOT "/docklet/x11/embedded");
	} else {
		purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/docklet/gtk/embedded", FALSE);
	}

	tmp = g_build_path(G_DIR_SEPARATOR_S, DATADIR, "pixmaps", "pidgin", "tray", NULL);
	gtk_icon_theme_append_search_path(gtk_icon_theme_get_default(), tmp);
	g_free(tmp);

	if (!strcmp(purple_prefs_get_string(PIDGIN_PREFS_ROOT "/docklet/show"), "always"))
		docklet_gtk_status_create(FALSE);

	purple_signal_connect(conn_handle, "signed-on",
			    docklet_handle, PURPLE_CALLBACK(docklet_signed_on_cb), NULL);
	purple_signal_connect(conn_handle, "signed-off",
			    docklet_handle, PURPLE_CALLBACK(docklet_signed_off_cb), NULL);
	purple_signal_connect(accounts_handle, "account-connecting",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "received-im-msg",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "conversation-created",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "deleting-conversation",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(conv_handle, "conversation-updated",
			    docklet_handle, PURPLE_CALLBACK(docklet_conv_updated_cb), NULL);
	purple_signal_connect(status_handle, "savedstatus-changed",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(notify_handle, "displaying-email-notification",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(notify_handle, "displaying-emails-notification",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
	purple_signal_connect(notify_handle, "displaying-emails-clear",
			    docklet_handle, PURPLE_CALLBACK(docklet_update_status_cb), NULL);
#if 0
	purple_signal_connect(purple_get_core(), "quitting",
			    docklet_handle, PURPLE_CALLBACK(purple_quit_cb), NULL);
#endif

	enable_join_chat = online_account_supports_chat();
}

void
pidgin_docklet_uninit()
{
	if (visible)
		docklet_gtk_status_destroy();
}


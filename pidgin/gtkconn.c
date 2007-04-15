/*
 * @file gtkconn.c GTK+ Connection API
 * @ingroup gtkui
 *
 * pidgin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "internal.h"
#include "pidgin.h"

#include "account.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkdialogs.h"
#include "gtkstatusbox.h"
#include "pidginstock.h"
#include "gtkutils.h"
#include "util.h"

#define INITIAL_RECON_DELAY_MIN  8000
#define INITIAL_RECON_DELAY_MAX 60000

#define MAX_RECON_DELAY 600000

typedef struct {
	int delay;
	guint timeout;
} PidginAutoRecon;

/**
 * Contains accounts that are auto-reconnecting.
 * The key is a pointer to the PurpleAccount and the
 * value is a pointer to a PidginAutoRecon.
 */
static GHashTable *hash = NULL;

static void
pidgin_connection_connect_progress(PurpleConnection *gc,
		const char *text, size_t step, size_t step_count)
{
	PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();
	if (!gtkblist)
		return;
	pidgin_status_box_set_connecting(PIDGIN_STATUS_BOX(gtkblist->statusbox),
					   (purple_connections_get_connecting() != NULL));
	pidgin_status_box_pulse_connecting(PIDGIN_STATUS_BOX(gtkblist->statusbox));
}

static void
pidgin_connection_connected(PurpleConnection *gc)
{
	PurpleAccount *account;
	PidginBuddyList *gtkblist;

	account  = purple_connection_get_account(gc);
	gtkblist = pidgin_blist_get_default_gtk_blist();

	if (gtkblist != NULL)
		pidgin_status_box_set_connecting(PIDGIN_STATUS_BOX(gtkblist->statusbox),
					   (purple_connections_get_connecting() != NULL));

	g_hash_table_remove(hash, account);

	pidgin_blist_update_account_error_state(account, NULL);
}

static void
pidgin_connection_disconnected(PurpleConnection *gc)
{
	PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();
	if (!gtkblist)
		return;
	pidgin_status_box_set_connecting(PIDGIN_STATUS_BOX(gtkblist->statusbox),
					   (purple_connections_get_connecting() != NULL));

	if (purple_connections_get_all() != NULL)
		return;

	pidgindialogs_destroy_all();
}

static void
free_auto_recon(gpointer data)
{
	PidginAutoRecon *info = data;

	if (info->timeout != 0)
		g_source_remove(info->timeout);

	g_free(info);
}

static gboolean
do_signon(gpointer data)
{
	PurpleAccount *account = data;
	PidginAutoRecon *info;
	PurpleStatus *status;

	purple_debug_info("autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(hash, account);

	if (info)
		info->timeout = 0;

	status = purple_account_get_active_status(account);
	if (purple_status_is_online(status))
	{
		purple_debug_info("autorecon", "calling purple_account_connect\n");
		purple_account_connect(account);
		purple_debug_info("autorecon", "done calling purple_account_connect\n");
	}

	return FALSE;
}

static void
pidgin_connection_report_disconnect(PurpleConnection *gc, const char *text)
{
	PurpleAccount *account = NULL;
	PidginAutoRecon *info;

	account = purple_connection_get_account(gc);
	info = g_hash_table_lookup(hash, account);

	pidgin_blist_update_account_error_state(account, text);
	if (!gc->wants_to_die) {
		if (info == NULL) {
			info = g_new0(PidginAutoRecon, 1);
			g_hash_table_insert(hash, account, info);
			info->delay = g_random_int_range(INITIAL_RECON_DELAY_MIN, INITIAL_RECON_DELAY_MAX);
		} else {
			info->delay = MIN(2 * info->delay, MAX_RECON_DELAY);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add(info->delay, do_signon, account);
	} else {
		char *p, *s, *n=NULL ;
		if (info != NULL)
			g_hash_table_remove(hash, account);

		if (purple_account_get_alias(account))
		{
			n = g_strdup_printf("%s (%s) (%s)",
					purple_account_get_username(account),
					purple_account_get_alias(account),
					purple_account_get_protocol_name(account));
		}
		else
		{
			n = g_strdup_printf("%s (%s)",
					purple_account_get_username(account),
					purple_account_get_protocol_name(account));
		}

		p = g_strdup_printf(_("%s disconnected"), n);
		s = g_strdup_printf(_("%s\n\n"
				"%s will not attempt to reconnect the account until you "
				"correct the error and re-enable the account."), text, PIDGIN_NAME);
		purple_notify_error(NULL, NULL, p, s);
		g_free(p);
		g_free(s);
		g_free(n);

		/*
		 * TODO: Do we really want to disable the account when it's
		 * disconnected by wants_to_die?  This happens when you sign
		 * on from somewhere else, or when you enter an invalid password.
		 */
		purple_account_set_enabled(account, PIDGIN_UI, FALSE);
	}
}

static void pidgin_connection_network_connected ()
{
	GList *list = purple_accounts_get_all_active();
	PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();

	if(gtkblist)
		pidgin_status_box_set_network_available(PIDGIN_STATUS_BOX(gtkblist->statusbox), TRUE);

	while (list) {
		PurpleAccount *account = (PurpleAccount*)list->data;
		g_hash_table_remove(hash, account);
		if (purple_account_is_disconnected(account))
			do_signon(account);
		list = list->next;
	}
}

static void pidgin_connection_network_disconnected ()
{
	GList *l = purple_accounts_get_all_active();
	PidginBuddyList *gtkblist = pidgin_blist_get_default_gtk_blist();
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConnection *gc = NULL;
	
	if(gtkblist)
		pidgin_status_box_set_network_available(PIDGIN_STATUS_BOX(gtkblist->statusbox), FALSE);

	while (l) {
		PurpleAccount *a = (PurpleAccount*)l->data;
		if (!purple_account_is_disconnected(a)) {
			gc = purple_account_get_connection(a);
			if (gc && gc->prpl) 
				prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);
			if (prpl_info) {
				if (prpl_info->keepalive)
					prpl_info->keepalive(gc);
				else
					purple_account_disconnect(a);
			}
		}
		l = l->next;
	}
}

static void pidgin_connection_notice(PurpleConnection *gc, const char *text)
{ }

static PurpleConnectionUiOps conn_ui_ops =
{
	pidgin_connection_connect_progress,
	pidgin_connection_connected,
	pidgin_connection_disconnected,
	pidgin_connection_notice,
	pidgin_connection_report_disconnect,
	pidgin_connection_network_connected,
	pidgin_connection_network_disconnected
};

PurpleConnectionUiOps *
pidgin_connections_get_ui_ops(void)
{
	return &conn_ui_ops;
}

static void
account_removed_cb(PurpleAccount *account, gpointer user_data)
{
	g_hash_table_remove(hash, account);

	pidgin_blist_update_account_error_state(account, NULL);
}


/**************************************************************************
* GTK+ connection glue
**************************************************************************/

void *
pidgin_connection_get_handle(void)
{
	static int handle;

	return &handle;
}

void
pidgin_connection_init(void)
{
	hash = g_hash_table_new_full(
							g_direct_hash, g_direct_equal,
							NULL, free_auto_recon);

	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						pidgin_connection_get_handle(),
						PURPLE_CALLBACK(account_removed_cb), NULL);
}

void
pidgin_connection_uninit(void)
{
	purple_signals_disconnect_by_handle(pidgin_connection_get_handle());

	g_hash_table_destroy(hash);
}

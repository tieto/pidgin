/**
 * @file gntconn.c GNT Connection API
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
#include <internal.h>
#include "finch.h"

#include "account.h"
#include "core.h"
#include "connection.h"
#include "debug.h"
#include "request.h"

#include "gntaccount.h"
#include "gntconn.h"

#define INITIAL_RECON_DELAY_MIN  8000
#define INITIAL_RECON_DELAY_MAX 60000

#define MAX_RECON_DELAY 600000

typedef struct {
	int delay;
	guint timeout;
} FinchAutoRecon;

/**
 * Contains accounts that are auto-reconnecting.
 * The key is a pointer to the PurpleAccount and the
 * value is a pointer to a FinchAutoRecon.
 */
static GHashTable *hash = NULL;

static void
free_auto_recon(gpointer data)
{
	FinchAutoRecon *info = data;

	if (info->timeout != 0)
		g_source_remove(info->timeout);

	g_free(info);
}


static gboolean
do_signon(gpointer data)
{
	PurpleAccount *account = data;
	FinchAutoRecon *info;
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
ce_modify_account_cb(PurpleAccount *account)
{
	finch_account_dialog_show(account);
}

static void
ce_enable_account_cb(PurpleAccount *account)
{
	purple_account_set_enabled(account, FINCH_UI, TRUE);
}

static void
finch_connection_report_disconnect(PurpleConnection *gc, PurpleConnectionError reason,
		const char *text)
{
	FinchAutoRecon *info;
	PurpleAccount *account = purple_connection_get_account(gc);

	if (!purple_connection_error_is_fatal(reason)) {
		info = g_hash_table_lookup(hash, account);

		if (info == NULL) {
			info = g_new0(FinchAutoRecon, 1);
			g_hash_table_insert(hash, account, info);
			info->delay = g_random_int_range(INITIAL_RECON_DELAY_MIN, INITIAL_RECON_DELAY_MAX);
		} else {
			info->delay = MIN(2 * info->delay, MAX_RECON_DELAY);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add(info->delay, do_signon, account);
	} else {
		char *act, *primary, *secondary;
		act = g_strdup_printf(_("%s (%s)"), purple_account_get_username(account),
				purple_account_get_protocol_name(account));

		primary = g_strdup_printf(_("%s disconnected."), act);
		secondary = g_strdup_printf(_("%s\n\n"
				"Finch will not attempt to reconnect the account until you "
				"correct the error and re-enable the account."), text);

		purple_request_action(account, NULL, primary, secondary, 2,
							account, NULL, NULL,
							account, 3,
							_("OK"), NULL,
							_("Modify Account"), PURPLE_CALLBACK(ce_modify_account_cb),
							_("Re-enable Account"), PURPLE_CALLBACK(ce_enable_account_cb));

		g_free(act);
		g_free(primary);
		g_free(secondary);
		purple_account_set_enabled(account, FINCH_UI, FALSE);
	}
}

static void
account_removed_cb(PurpleAccount *account, gpointer user_data)
{
	g_hash_table_remove(hash, account);
}

static void *
finch_connection_get_handle(void)
{
	static int handle;

	return &handle;
}

static PurpleConnectionUiOps ops = 
{
	NULL, /* connect_progress */
	NULL, /* connected */
	NULL, /* disconnected */
	NULL, /* notice */
	NULL,
	NULL, /* network_connected */
	NULL, /* network_disconnected */
	finch_connection_report_disconnect,
	NULL,
	NULL,
	NULL
};

PurpleConnectionUiOps *finch_connections_get_ui_ops()
{
	return &ops;
}

void finch_connections_init()
{
	hash = g_hash_table_new_full(
							g_direct_hash, g_direct_equal,
							NULL, free_auto_recon);

	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						finch_connection_get_handle(),
						PURPLE_CALLBACK(account_removed_cb), NULL);
}

void finch_connections_uninit()
{
	purple_signals_disconnect_by_handle(finch_connection_get_handle());
	g_hash_table_destroy(hash);
}

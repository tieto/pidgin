/*
 * @file gtkconn.c GTK+ Connection API
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
#include "gtkgaim.h"

#include "account.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkdialogs.h"
#include "gtkstatusbox.h"
#include "gtkstock.h"
#include "gtkutils.h"
#include "util.h"

#define INITIAL_RECON_DELAY 8000
#define MAX_RECON_DELAY 600000

typedef struct {
	int delay;
	guint timeout;
} GaimAutoRecon;

static GHashTable *hash = NULL;
static GSList *accountReconnecting = NULL;

static void gaim_gtk_connection_connect_progress(GaimConnection *gc,
		const char *text, size_t step, size_t step_count)
{
	GaimGtkBuddyList *gtkblist = gaim_gtk_blist_get_default_gtk_blist();
	GtkGaimStatusBox *box;
	if (!gtkblist)
		return;
	gtk_gaim_status_box_set_connecting(GTK_GAIM_STATUS_BOX(gtkblist->statusbox),
					   (gaim_connections_get_connecting() != NULL));
	gtk_gaim_status_box_pulse_connecting(GTK_GAIM_STATUS_BOX(gtkblist->statusbox));
}

static void gaim_gtk_connection_connected(GaimConnection *gc)
{
	GaimGtkBuddyList *gtkblist = gaim_gtk_blist_get_default_gtk_blist();
	GtkGaimStatusBox *box;
	GaimAccount *account = NULL;
	if (!gtkblist)
		return;
	gtk_gaim_status_box_set_connecting(GTK_GAIM_STATUS_BOX(gtkblist->statusbox),
					   (gaim_connections_get_connecting() != NULL));
	account = gaim_connection_get_account(gc);
	
	if (hash != NULL)
		g_hash_table_remove(hash, account);
	if (accountReconnecting == NULL)
		return;
	accountReconnecting = g_slist_remove(accountReconnecting, account);
	if (accountReconnecting == NULL)
		gtk_gaim_status_box_set_error(GTK_GAIM_STATUS_BOX(gtkblist->statusbox), NULL);
}

static void gaim_gtk_connection_disconnected(GaimConnection *gc)
{
	GaimGtkBuddyList *gtkblist = gaim_gtk_blist_get_default_gtk_blist();
	if (!gtkblist)
		return;
	gtk_gaim_status_box_set_connecting(GTK_GAIM_STATUS_BOX(gtkblist->statusbox),
					   (gaim_connections_get_connecting() != NULL));

	if (gaim_connections_get_all() != NULL)
		return;

	gaim_gtkdialogs_destroy_all();
}

static void gaim_gtk_connection_notice(GaimConnection *gc,
		const char *text)
{
}


static void
free_auto_recon(gpointer data)
{
	GaimAutoRecon *info = data;

	if (info->timeout != 0)
		g_source_remove(info->timeout);

	g_free(info);
}

static gboolean
do_signon(gpointer data)
{
	GaimAccount *account = data;
	GaimAutoRecon *info;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(hash, account);

	if (g_list_index(gaim_accounts_get_all(), account) < 0)
		return FALSE;

	if (info)
		info->timeout = 0;

	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "calling gaim_account_connect\n");
	gaim_account_connect(account);
	gaim_debug(GAIM_DEBUG_INFO, "autorecon", "done calling gaim_account_connect\n");

	return FALSE;
}

static void gaim_gtk_connection_report_disconnect(GaimConnection *gc, const char *text)
{
	GaimGtkBuddyList *gtkblist = gaim_gtk_blist_get_default_gtk_blist();
	GaimAccount *account = NULL;
	GaimAutoRecon *info;
	GSList* listAccount;

	if (hash == NULL) {
		hash = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
		free_auto_recon);
	}
	account = gaim_connection_get_account(gc);
	info = g_hash_table_lookup(hash, account);
	if (accountReconnecting)
		listAccount = g_slist_find(accountReconnecting, account);
	else
		listAccount = NULL;

	if (!gc->wants_to_die) {
		if (gtkblist != NULL)
			gtk_gaim_status_box_set_error(GTK_GAIM_STATUS_BOX(gtkblist->statusbox), text);
		
		if (info == NULL) {
			info = g_new0(GaimAutoRecon, 1);
			g_hash_table_insert(hash, account, info);
			info->delay = INITIAL_RECON_DELAY;
		} else {
			info->delay = MIN(2 * info->delay, MAX_RECON_DELAY);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add(info->delay, do_signon, account);

		if (!listAccount)
			accountReconnecting = g_slist_prepend(accountReconnecting, account);
	} else {
	  char *p, *s, *n=NULL ;
	  if (info != NULL)
	    g_hash_table_remove(hash, account);

	  if (listAccount)
	      accountReconnecting = g_slist_delete_link(accountReconnecting, listAccount);

	  if (gaim_account_get_alias(account)) {
	    n = g_strdup_printf("%s (%s) (%s)",
				  gaim_account_get_username(account),
				  gaim_account_get_alias(account),
				  gaim_account_get_protocol_name(account));
	    } else {
	      n = g_strdup_printf("%s (%s)",
				  gaim_account_get_username(account),
				  gaim_account_get_protocol_name(account));
	    }

	    p = g_strdup_printf(_("%s disconnected"), n);
	    s = g_strdup_printf(_("%s was disconnected due to an error. %s The account has been disabled. "
				  "Correct the error and reenable the account to connect."), n, text);
	    gaim_notify_error(NULL, NULL, p, s);
	    g_free(p);
	    g_free(s);
	    g_free(n);
		/* XXX: do we really want to disable the account when it's disconnected by wants_to_die?
		 *      This normally happens when you sign on from somewhere else. */
	    gaim_account_set_enabled(account, GAIM_GTK_UI, FALSE);
	}
}

static GaimConnectionUiOps conn_ui_ops =
{
	gaim_gtk_connection_connect_progress,
	gaim_gtk_connection_connected,
	gaim_gtk_connection_disconnected,
	gaim_gtk_connection_notice,
	gaim_gtk_connection_report_disconnect,
};

GaimConnectionUiOps *
gaim_gtk_connections_get_ui_ops(void)
{
	return &conn_ui_ops;
}

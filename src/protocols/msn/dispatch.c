/**
 * @file dispatch.c Dispatch server functions
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"
#include "dispatch.h"
#include "notification.h"
#include "error.h"

static GHashTable *dispatch_commands = NULL;

static gboolean
ver_cmd(MsnServConn *servconn, const char *command, const char **params,
		size_t param_count)
{
	GaimConnection *gc = servconn->session->account->gc;
	size_t i;
	gboolean msnp5_found = FALSE;

	for (i = 1; i < param_count; i++) {
		if (!strcmp(params[i], "MSNP5")) {
			msnp5_found = TRUE;
			break;
		}
	}

	if (!msnp5_found) {
		gaim_connection_error(gc, _("Protocol not supported"));

		return FALSE;
	}

	if (!msn_servconn_send_command(servconn, "INF", NULL)) {
		gaim_connection_error(gc, _("Unable to request INF\n"));

		return FALSE;
	}

	return TRUE;
}

static gboolean
inf_cmd(MsnServConn *servconn, const char *command, const char **params,
		size_t param_count)
{
	GaimAccount *account = servconn->session->account;
	GaimConnection *gc = gaim_account_get_connection(account);
	char outparams[MSN_BUF_LEN];

	if (strcmp(params[1], "MD5")) {
		gaim_connection_error(gc, _("Unable to login using MD5"));

		return FALSE;
	}

	g_snprintf(outparams, sizeof(outparams), "MD5 I %s",
			   gaim_account_get_username(account));

	if (!msn_servconn_send_command(servconn, "USR", outparams)) {
		gaim_connection_error(gc, _("Unable to send USR\n"));

		return FALSE;
	}

	gaim_connection_update_progress(gc, _("Requesting to send password"),
									3, MSN_CONNECT_STEPS);

	return TRUE;
}

static gboolean
xfr_cmd(MsnServConn *servconn, const char *command, const char **params,
		size_t param_count)
{
	MsnSession *session = servconn->session;
	GaimConnection *gc = servconn->session->account->gc;
	char *host;
	int port;
	char *c;

	if (param_count < 2 || strcmp(params[1], "NS")) {
		gaim_connection_error(gc, _("Got invalid XFR\n"));

		return FALSE;
	}

	host = g_strdup(params[2]);

	if ((c = strchr(host, ':')) != NULL) {
		*c = '\0';

		port = atoi(c + 1);
	}
	else
		port = 1863;

	session->passport_info.sl = time(NULL);

	/* Disconnect from here. */
	msn_servconn_destroy(servconn);
	session->dispatch_conn = NULL;

	/* Now connect to the switchboard. */
	session->notification_conn = msn_notification_new(session, host, port);

	g_free(host);

	if (!msn_servconn_connect(session->notification_conn)) {
		gaim_connection_error(gc, _("Unable to transfer"));
	}

	return FALSE;
}

static gboolean
unknown_cmd(MsnServConn *servconn, const char *command, const char **params,
			size_t param_count)
{
	GaimConnection *gc = servconn->session->account->gc;

	if (isdigit(*command)) {
		char buf[4];

		strncpy(buf, command, 4);
		buf[4] = '\0';

		gaim_connection_error(gc, (char *)msn_error_get_text(atoi(buf)));
	}
	else
		gaim_connection_error(gc, _("Unable to parse message."));

	return FALSE;
}

static gboolean
connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *dispatch = data;
	MsnSession *session = dispatch->session;
	GaimConnection *gc = session->account->gc;

	if (source == -1) {
		gaim_connection_error(session->account->gc, _("Unable to connect"));
		return FALSE;
	}

	gaim_connection_update_progress(gc, _("Connecting"), 1, MSN_CONNECT_STEPS);

	if (dispatch->fd != source)
		dispatch->fd = source;

	if (!msn_servconn_send_command(dispatch, "VER",
								   "MSNP7 MSNP6 MSNP5 MSNP4 CVR0")) {
		gaim_connection_error(gc, _("Unable to write to server"));
		return FALSE;
	}

	gaim_connection_update_progress(gc, _("Syncing with server"),
									2, MSN_CONNECT_STEPS);

	return TRUE;
}

static void
failed_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *dispatch = data;
	GaimConnection *gc;

	gc = dispatch->session->account->gc;

	gaim_connection_error(gc, _("Error reading from server"));
}

MsnServConn *
msn_dispatch_new(MsnSession *session, const char *server, int port)
{
	MsnServConn *dispatch;

	dispatch = msn_servconn_new(session);
	
	msn_servconn_set_server(dispatch, server, port);
	msn_servconn_set_connect_cb(dispatch, connect_cb);
	msn_servconn_set_failed_read_cb(dispatch, failed_read_cb);

	if (dispatch_commands == NULL) {
		/* Register the command callbacks. */
		msn_servconn_register_command(dispatch, "VER",       ver_cmd);
		msn_servconn_register_command(dispatch, "INF",       inf_cmd);
		msn_servconn_register_command(dispatch, "XFR",       xfr_cmd);
		msn_servconn_register_command(dispatch, "_unknown_", unknown_cmd);

		/* Save this for future use. */
		dispatch_commands = dispatch->commands;
	}
	else {
		g_hash_table_destroy(dispatch->commands);

		dispatch->commands = dispatch_commands;
	}

	return dispatch;
}


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
cvr_cmd(MsnServConn *servconn, const char *command, const char **params,
		size_t param_count)
{
	GaimAccount *account = servconn->session->account;
	GaimConnection *gc = gaim_account_get_connection(account);
	char outparams[MSN_BUF_LEN];

	g_snprintf(outparams, sizeof(outparams),
			   "TWN I %s", gaim_account_get_username(account));

	if (!msn_servconn_send_command(servconn, "USR", outparams))
	{
		gaim_connection_error(gc, _("Unable to request USR\n"));

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
		gaim_connection_error(gc, _("Unable to send USR"));

		return FALSE;
	}

	gaim_connection_update_progress(gc, _("Requesting to send password"),
									3, MSN_CONNECT_STEPS);

	return TRUE;
}

static gboolean
ver_cmd(MsnServConn *servconn, const char *command, const char **params,
		size_t param_count)
{
	MsnSession *session = servconn->session;
	GaimAccount *account = session->account;
	GaimConnection *gc = gaim_account_get_connection(account);
	gboolean protocol_supported = FALSE;
	char outparams[MSN_BUF_LEN];
	char proto_str[8];
	size_t i;

	g_snprintf(proto_str, sizeof(proto_str), "MSNP%d", session->protocol_ver);

	for (i = 1; i < param_count; i++)
	{
		if (!strcmp(params[i], proto_str))
		{
			protocol_supported = TRUE;
			break;
		}
	}

	if (!protocol_supported)
	{
		gaim_connection_error(gc, _("Protocol version not supported"));

		return FALSE;
	}

	if (session->protocol_ver >= 8)
	{
		g_snprintf(outparams, sizeof(outparams),
				   "0x0409 winnt 5.1 i386 MSNMSGR 6.0.0602 MSMSGS %s",
				   gaim_account_get_username(account));

		if (!msn_servconn_send_command(servconn, "CVR", outparams))
		{
			gaim_connection_error(gc, _("Unable to request CVR\n"));

			return FALSE;
		}
	}
	else
	{
		if (!msn_servconn_send_command(servconn, "INF", NULL))
		{
			gaim_connection_error(gc, _("Unable to request INF\n"));

			return FALSE;
		}
	}

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

	if (param_count < 2 || strcmp(params[1], "NS"))
	{
		gaim_connection_error(gc, _("Got invalid XFR"));

		return FALSE;
	}

	host = g_strdup(params[2]);

	if ((c = strchr(host, ':')) != NULL)
	{
		*c = '\0';

		port = atoi(c + 1);
	}
	else
		port = 1863;

	session->passport_info.sl = time(NULL);

	/* Disconnect from here. */
	msn_servconn_destroy(servconn);
	session->dispatch_conn = NULL;

	/* Reset our transaction ID. */
	session->trId = 0;

	/* Now connect to the switchboard. */
	session->notification_conn = msn_notification_new(session, host, port);

	g_free(host);

	if (!msn_servconn_connect(session->notification_conn))
		gaim_connection_error(gc, _("Unable to transfer"));

	return FALSE;
}

static gboolean
unknown_cmd(MsnServConn *servconn, const char *command, const char **params,
			size_t param_count)
{
	GaimConnection *gc = servconn->session->account->gc;

	if (isdigit(*command))
		gaim_connection_error(gc, msn_error_get_text(atoi(command)));
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
	char proto_vers[256];
	size_t i;

	if (source == -1)
	{
		gaim_connection_error(session->account->gc, _("Unable to connect."));
		return FALSE;
	}

	gaim_connection_update_progress(gc, _("Connecting"), 1, MSN_CONNECT_STEPS);

	if (dispatch->fd != source)
		dispatch->fd = source;

	proto_vers[0] = '\0';

	for (i = 7; i <= session->protocol_ver; i++)
	{
		char old_buf[256];

		strcpy(old_buf, proto_vers);

		g_snprintf(proto_vers, sizeof(proto_vers), "MSNP%d %s", (int)i, old_buf);
	}

	strncat(proto_vers, "CVR0", sizeof(proto_vers));

	if (!msn_servconn_send_command(dispatch, "VER", proto_vers))
	{
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

		msn_servconn_register_command(dispatch, "CVR",       cvr_cmd);
		msn_servconn_register_command(dispatch, "INF",       inf_cmd);
		msn_servconn_register_command(dispatch, "VER",       ver_cmd);
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


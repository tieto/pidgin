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
__ver_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	size_t i;
	gboolean msnp5_found = FALSE;

	for (i = 1; i < param_count; i++) {
		if (!strcmp(params[i], "MSNP5")) {
			msnp5_found = TRUE;
			break;
		}
	}

	if (!msnp5_found) {
		hide_login_progress(gc, _("Protocol not supported"));
		signoff(gc);

		return FALSE;
	}

	if (!msn_servconn_send_command(servconn, "INF", NULL)) {
		hide_login_progress(gc, _("Unable to request INF\n"));
		signoff(gc);

		return FALSE;
	}

	return TRUE;
}

static gboolean
__inf_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;
	char outparams[MSN_BUF_LEN];

	if (strcmp(params[1], "MD5")) {
		hide_login_progress(gc, _("Unable to login using MD5"));
		signoff(gc);

		return FALSE;
	}

	g_snprintf(outparams, sizeof(outparams), "MD5 I %s", gc->username);

	if (!msn_servconn_send_command(servconn, "USR", outparams)) {
		hide_login_progress(gc, _("Unable to send USR\n"));
		signoff(gc);

		return FALSE;
	}

	set_login_progress(gc, 3, _("Requesting to send password"));

	return TRUE;
}

static gboolean
__xfr_cmd(MsnServConn *servconn, const char *command, const char **params,
		  size_t param_count)
{
	MsnSession *session = servconn->session;
	struct gaim_connection *gc = servconn->session->account->gc;
	char *host;
	int port;
	char *c;

	if (param_count < 2 || strcmp(params[1], "NS")) {
		hide_login_progress(gc, _("Got invalid XFR\n"));
		signoff(gc);

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
		hide_login_progress(gc, _("Unable to transfer"));
		signoff(gc);
	}

	return FALSE;
}

static gboolean
__unknown_cmd(MsnServConn *servconn, const char *command, const char **params,
			  size_t param_count)
{
	struct gaim_connection *gc = servconn->session->account->gc;

	if (isdigit(*command)) {
		char buf[4];

		strncpy(buf, command, 4);
		buf[4] = '\0';

		hide_login_progress(gc, (char *)msn_error_get_text(atoi(buf)));
	}
	else
		hide_login_progress(gc, _("Unable to parse message."));

	signoff(gc);

	return FALSE;
}

static gboolean
__connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *dispatch = data;
	MsnSession *session = dispatch->session;
	struct gaim_connection *gc = session->account->gc;

	if (source == -1) {
		hide_login_progress(session->account->gc, _("Unable to connect"));
		signoff(session->account->gc);
		return FALSE;
	}

	set_login_progress(gc, 1, _("Connecting"));

	if (dispatch->fd != source)
		dispatch->fd = source;

	if (!msn_servconn_send_command(dispatch, "VER",
								   "MSNP7 MSNP6 MSNP5 MSNP4 CVR0")) {
		hide_login_progress(gc, _("Unable to write to server"));
		signoff(gc);
		return FALSE;
	}

	set_login_progress(session->account->gc, 2, _("Syncing with server"));

	return TRUE;
}

static void
__failed_read_cb(gpointer data, gint source, GaimInputCondition cond)
{
	MsnServConn *dispatch = data;
	struct gaim_connection *gc;

	gc = dispatch->session->account->gc;

	hide_login_progress(gc, _("Error reading from server"));
	signoff(gc);
}

MsnServConn *
msn_dispatch_new(MsnSession *session, const char *server, int port)
{
	MsnServConn *dispatch;

	dispatch = msn_servconn_new(session);
	
	msn_servconn_set_server(dispatch, server, port);
	msn_servconn_set_connect_cb(dispatch, __connect_cb);
	msn_servconn_set_failed_read_cb(dispatch, __failed_read_cb);

	if (dispatch_commands == NULL) {
		/* Register the command callbacks. */
		msn_servconn_register_command(dispatch, "VER",       __ver_cmd);
		msn_servconn_register_command(dispatch, "INF",       __inf_cmd);
		msn_servconn_register_command(dispatch, "XFR",       __xfr_cmd);
		msn_servconn_register_command(dispatch, "_unknown_", __unknown_cmd);

		/* Save this for future use. */
		dispatch_commands = dispatch->commands;
	}
	else {
		g_hash_table_destroy(dispatch->commands);

		dispatch->commands = dispatch_commands;
	}

	return dispatch;
}


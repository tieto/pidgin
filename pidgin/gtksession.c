/*
 * @file gtksession.c X Windows session management API
 * @ingroup pidgin
 */

/* Pidgin is the legal property of its developers, whose names are too numerous
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
 *
 */
#include "internal.h"

#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "gtksession.h"

#ifdef USE_SM

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <gdk/gdkx.h>
#include <unistd.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#define ERROR_LENGTH 512

static IceIOErrorHandler ice_installed_io_error_handler;
static SmcConn session = NULL;
static gchar *myself = NULL;
static gboolean had_first_save = FALSE;
static gboolean session_managed = FALSE;

/* ICE belt'n'braces stuff */

struct ice_connection_info {
	IceConn connection;
	guint input_id;
};

static void ice_process_messages(gpointer data, gint fd,
								 PurpleInputCondition condition) {
	struct ice_connection_info *conninfo = (struct ice_connection_info*) data;
	IceProcessMessagesStatus status;

	/* please don't block... please! */
	status = IceProcessMessages(conninfo->connection, NULL, NULL);

	if (status == IceProcessMessagesIOError) {
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "ICE IO error, closing connection... ");

		/* IO error, please disconnect */
		IceSetShutdownNegotiation(conninfo->connection, False);
		IceCloseConnection(conninfo->connection);

		purple_debug(PURPLE_DEBUG_INFO, NULL, "done.\n");

		/* cancel the handler */
		purple_input_remove(conninfo->input_id);
	}
}

static void ice_connection_watch(IceConn connection, IcePointer client_data,
	      Bool opening, IcePointer *watch_data) {
	struct ice_connection_info *conninfo = NULL;

	if (opening) {
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "Handling new ICE connection... \n");

		/* ensure ICE connection is not passed to child processes */
		if (fcntl(IceConnectionNumber(connection), F_SETFD, FD_CLOEXEC) != 0)
			purple_debug_warning("gtksession", "couldn't set FD_CLOEXEC\n");

		conninfo = g_new(struct ice_connection_info, 1);
		conninfo->connection = connection;

		/* watch the connection */
		conninfo->input_id = purple_input_add(IceConnectionNumber(connection), PURPLE_INPUT_READ,
											ice_process_messages, conninfo);
		*watch_data = conninfo;
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "Handling closed ICE connection... \n");

		/* get the input ID back and stop watching it */
		conninfo = (struct ice_connection_info*) *watch_data;
		purple_input_remove(conninfo->input_id);
		g_free(conninfo);
	}

	purple_debug(PURPLE_DEBUG_INFO, NULL, "done.\n");
}

/* We call any handler installed before (or after) ice_init but
 * avoid calling the default libICE handler which does an exit().
 *
 * This means we do nothing by default, which is probably correct,
 * the connection will get closed by libICE
 */

static void ice_io_error_handler(IceConn connection) {
	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Handling ICE IO error... ");

	if (ice_installed_io_error_handler)
		(*ice_installed_io_error_handler)(connection);

	purple_debug(PURPLE_DEBUG_INFO, NULL, "done.\n");
}

static void ice_init(void) {
	IceIOErrorHandler default_handler;

	ice_installed_io_error_handler = IceSetIOErrorHandler(NULL);
	default_handler = IceSetIOErrorHandler(ice_io_error_handler);

	if (ice_installed_io_error_handler == default_handler)
		ice_installed_io_error_handler = NULL;

	IceAddConnectionWatch(ice_connection_watch, NULL);

	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "ICE initialized.\n");
}

/* my magic utility function */

static gchar **session_make_command(gchar *client_id, gchar *config_dir) {
	gint i = 4;
	gint j = 0;
	gchar **ret;

	if (client_id) i += 2;
	if (config_dir)	i += 2; /* we will specify purple's user dir */

	ret = g_new(gchar *, i);
	ret[j++] = g_strdup(myself);

	if (client_id) {
		ret[j++] = g_strdup("--session");
		ret[j++] = g_strdup(client_id);
	}

	if (config_dir) {
		ret[j++] = g_strdup("--config");
		ret[j++] = g_strdup(config_dir);
	}

	ret[j++] = g_strdup("--display");
	ret[j++] = g_strdup((gchar *)gdk_display_get_name(gdk_display_get_default()));

	ret[j] = NULL;

	return ret;
}

/* SM callback handlers */

static void session_save_yourself(SmcConn conn, SmPointer data, int save_type,
       Bool shutdown, int interact_style, Bool fast) {
	if (had_first_save == FALSE && save_type == SmSaveLocal &&
	      interact_style == SmInteractStyleNone && !shutdown &&
	      !fast) {
		/* this is just a dry run, spit it back */
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "Received first save_yourself\n");
		SmcSaveYourselfDone(conn, True);
		had_first_save = TRUE;
		return;
	}

	/* tum ti tum... don't add anything else here without *
         * reading SMlib.PS from an X.org ftp server near you */

	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Received save_yourself\n");

	if (save_type == SmSaveGlobal || save_type == SmSaveBoth) {
		/* may as well do something ... */
		/* or not -- save_prefs(); */
	}

	SmcSaveYourselfDone(conn, True);
}

static void session_die(SmcConn conn, SmPointer data) {
	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Received die\n");
	purple_core_quit();
}

static void session_save_complete(SmcConn conn, SmPointer data) {
	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Received save_complete\n");
}

static void session_shutdown_cancelled(SmcConn conn, SmPointer data) {
	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Received shutdown_cancelled\n");
}

/* utility functions stolen from Gnome-client */

static void session_set_value(SmcConn conn, gchar *name, char *type,
	      int num_vals, SmPropValue *vals) {
	SmProp *proplist[1];
	SmProp prop;

	g_return_if_fail(conn);

	prop.name = name;
	prop.type = type;
	prop.num_vals = num_vals;
	prop.vals = vals;

	proplist[0] = &prop;
	SmcSetProperties(conn, 1, proplist);
}

static void session_set_string(SmcConn conn, gchar *name, gchar *value) {
	SmPropValue val;

	g_return_if_fail(name);

	val.length = strlen (value)+1;
	val.value  = value;

	session_set_value(conn, name, SmARRAY8, 1, &val);
}

static void session_set_gchar(SmcConn conn, gchar *name, gchar value) {
	SmPropValue val;

	g_return_if_fail(name);

	val.length = 1;
	val.value  = &value;

	session_set_value(conn, name, SmCARD8, 1, &val);
}

static void session_set_array(SmcConn conn, gchar *name, gchar *array[]) {
	gint    argc;
	gchar **ptr;
	gint    i;

	SmPropValue *vals;

	g_return_if_fail (name);

	/* We count the number of elements in our array.  */
	for (ptr = array, argc = 0; *ptr ; ptr++, argc++) /* LOOP */;

	/* Now initialize the 'vals' array.  */
	vals = g_new (SmPropValue, argc);
	for (ptr = array, i = 0 ; i < argc ; ptr++, i++) {
		vals[i].length = strlen (*ptr);
		vals[i].value  = *ptr;
	}

	session_set_value(conn, name, SmLISTofARRAY8, argc, vals);

	g_free (vals);
}

#endif /* USE_SM */

/* setup functions */

void
pidgin_session_init(gchar *argv0, gchar *previous_id, gchar *config_dir)
{
#ifdef USE_SM
	SmcCallbacks callbacks;
	gchar *client_id = NULL;
	gchar error[ERROR_LENGTH] = "";
	gchar *tmp = NULL;
	gchar **cmd = NULL;

	if (session != NULL) {
		/* session is already established, what the hell is going on? */
		purple_debug(PURPLE_DEBUG_WARNING, "Session Management",
				   "Duplicated call to pidgin_session_init!\n");
		return;
	}

	if (g_getenv("SESSION_MANAGER") == NULL) {
		purple_debug(PURPLE_DEBUG_ERROR, "Session Management",
				   "No SESSION_MANAGER found, aborting.\n");
		return;
	}

	ice_init();

	callbacks.save_yourself.callback         = session_save_yourself;
	callbacks.die.callback                   = session_die;
	callbacks.save_complete.callback         = session_save_complete;
	callbacks.shutdown_cancelled.callback    = session_shutdown_cancelled;

	callbacks.save_yourself.client_data      = NULL;
	callbacks.die.client_data                = NULL;
	callbacks.save_complete.client_data      = NULL;
	callbacks.shutdown_cancelled.client_data = NULL;

	if (previous_id) {
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "Connecting with previous ID %s\n", previous_id);
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "Session Management",
				   "Connecting with no previous ID\n");
	}

	session = SmcOpenConnection(NULL, "session", SmProtoMajor, SmProtoMinor, SmcSaveYourselfProcMask |
		    SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
		    &callbacks, previous_id, &client_id, ERROR_LENGTH, error);

	if (session == NULL) {
		if (error[0] != '\0') {
			purple_debug(PURPLE_DEBUG_ERROR, "Session Management",
					   "Connection failed with error: %s\n", error);
		} else {
			purple_debug(PURPLE_DEBUG_ERROR, "Session Management",
					   "Connetion failed with unknown error.\n");
		}
		return;
	}

	tmp = SmcVendor(session);
	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Connected to manager (%s) with client ID %s\n",
			   tmp, client_id);
	free(tmp);

	session_managed = TRUE;
	gdk_set_sm_client_id(client_id);

	tmp = g_get_current_dir();
	session_set_string(session, SmCurrentDirectory, tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", (int) getpid());
	session_set_string(session, SmProcessID, tmp);
	g_free(tmp);

	tmp = g_strdup(g_get_user_name());
	session_set_string(session, SmUserID, tmp);
	g_free(tmp);

	session_set_gchar(session, SmRestartStyleHint, (gchar) SmRestartIfRunning);
	session_set_string(session, SmProgram, (gchar *) g_get_prgname());

	myself = g_strdup(argv0);
	purple_debug(PURPLE_DEBUG_MISC, "Session Management",
			   "Using %s as command\n", myself);

	cmd = session_make_command(NULL, config_dir);
	session_set_array(session, SmCloneCommand, cmd);
	g_strfreev(cmd);

	/* this is currently useless, but gnome-session warns 'the following applications will not
	   save their current status' bla bla if we don't have it and the user checks 'Save Session'
	   when they log out */
	cmd = g_new(gchar *, 2);
	cmd[0] = g_strdup("/bin/true");
	cmd[1] = NULL;
	session_set_array(session, SmDiscardCommand, cmd);
	g_strfreev(cmd);

	cmd = session_make_command(client_id, config_dir);
	session_set_array(session, SmRestartCommand, cmd);
	g_strfreev(cmd);

	g_free(client_id);
#endif /* USE_SM */
}

void
pidgin_session_end()
{
#ifdef USE_SM
	if (session == NULL) /* no session to close */
		return;

	SmcCloseConnection(session, 0, NULL);

	purple_debug(PURPLE_DEBUG_INFO, "Session Management",
			   "Connection closed.\n");
#endif /* USE_SM */
}

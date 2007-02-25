/*
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
 *
 */
#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "eventloop.h"
#include "ft.h"
#include "log.h"
#include "notify.h"
#include "prefix.h"
#include "prefs.h"
#include "prpl.h"
#include "pounce.h"
#include "savedstatuses.h"
#include "sound.h"
#include "status.h"
#include "util.h"
#include "whiteboard.h"

#include <glib.h>

#include <string.h>
#include <unistd.h>

#include "defines.h"

/**
 * The following eventloop functions are used in both pidgin and gaim-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define GAIM_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _GaimGLibIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;
} GaimGLibIOClosure;

static void gaim_glib_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimGLibIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_GLIB_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_GLIB_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  gaim_cond);

	return TRUE;
}

static guint glib_input_add(gint fd, GaimInputCondition condition, GaimInputFunction function,
							   gpointer data)
{
	GaimGLibIOClosure *closure = g_new0(GaimGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_GLIB_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_GLIB_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_glib_io_invoke, closure, gaim_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static GaimEventLoopUiOps glib_eventloops = 
{
	g_timeout_add,
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL
};
/*** End of the eventloop functions. ***/

/*** Conversation uiops ***/
static void
null_write_conv(GaimConversation *conv, const char *who, const char *alias,
			const char *message, GaimMessageFlags flags, time_t mtime)
{
	const char *name;
	if (alias && *alias)
		name = alias;
	else if (who && *who)
		name = who;
	else
		name = NULL;

	printf("(%s) %s %s: %s\n", gaim_conversation_get_name(conv),
			gaim_utf8_strftime("(%H:%M:%S)", localtime(&mtime)),
			name, message);
}

static GaimConversationUiOps null_conv_uiops = 
{
	.write_conv = null_write_conv
};

static void
null_ui_init()
{
	/**
	 * This should initialize the UI components for all the modules. Here we
	 * just initialize the UI for conversations.
	 */
	gaim_conversations_set_ui_ops(&null_conv_uiops);
}

static GaimCoreUiOps null_core_uiops = 
{
	NULL,
	NULL,
	null_ui_init,
	NULL
};

static void
init_libpurple()
{
	/* Set a custom user directory (optional) */
	gaim_util_set_user_dir(CUSTOM_USER_DIRECTORY);

	/* We do not want any debugging for now to keep the noise to a minimum. */
	gaim_debug_set_enabled(FALSE);

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	gaim_core_set_ui_ops(&null_core_uiops);

	/* Set the uiops for the eventloop. If your client is glib-based, you can safely
	 * copy this verbatim. */
	gaim_eventloop_set_ui_ops(&glib_eventloops);

	/* Set path to search for plugins. The core (libpurple) takes care of loading the
	 * core-plugins, which includes the protocol-plugins. So it is not essential to add
	 * any path here, but it might be desired, especially for ui-specific plugins. */
	gaim_plugins_add_search_path(CUSTOM_PLUGIN_PATH);

	/* Now that all the essential stuff has been set, let's try to init the core. It's
	 * necessary to provide a non-NULL name for the current ui to the core. This name
	 * is used by stuff that depends on this ui, for example the ui-specific plugins. */
	if (!gaim_core_init(UI_ID)) {
		/* Initializing the core failed. Terminate. */
		fprintf(stderr,
				"libpurple initialization failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}

	/* Create and load the buddylist. */
	gaim_set_blist(gaim_blist_new());
	gaim_blist_load();

	/* Load the preferences. */
	gaim_prefs_load();

	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using gaim_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	gaim_plugins_load_saved(PLUGIN_SAVE_PREF);

	/* Load the pounces. */
	gaim_pounces_load();
}

static void
signed_on(GaimConnection *gc, gpointer null)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	printf("Account connected: %s %s\n", account->username, account->protocol_id);
}

static void
connect_to_signals_for_demonstration_purposes_only()
{
	static int handle;
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on", &handle,
				GAIM_CALLBACK(signed_on), NULL);
}

int main()
{
	GList *iter;
	int i, num;
	GList *names = NULL;
	const char *prpl;
	char name[128];
	char *password;
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	GaimAccount *account;
	GaimSavedStatus *status;

	init_libpurple();

	printf("libpurple initialized.\n");

	iter = gaim_plugins_get_protocols();
	for (i = 0; iter; iter = iter->next) {
		GaimPlugin *plugin = iter->data;
		GaimPluginInfo *info = plugin->info;
		if (info && info->name) {
			printf("\t%d: %s\n", i++, info->name);
			names = g_list_append(names, info->id);
		}
	}
	printf("Select the protocol [0-%d]: ", i-1);
	fgets(name, sizeof(name), stdin);
	sscanf(name, "%d", &num);
	prpl = g_list_nth_data(names, num);

	printf("Username: ");
	fgets(name, sizeof(name), stdin);
	name[strlen(name) - 1] = 0;  /* strip the \n at the end */

	/* Create the account */
	account = gaim_account_new(name, prpl);

	/* Get the password for the account */
	password = getpass("Password: ");
	gaim_account_set_password(account, password);

	/* It's necessary to enable the account first. */
	gaim_account_set_enabled(account, UI_ID, TRUE);

	/* Now, to connect the account(s), create a status and activate it. */
	status = gaim_savedstatus_new(NULL, GAIM_STATUS_AVAILABLE);
	gaim_savedstatus_activate(status);

	connect_to_signals_for_demonstration_purposes_only();

	g_main_loop_run(loop);

	return 0;
}


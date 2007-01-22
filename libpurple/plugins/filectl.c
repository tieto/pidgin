/**
 * Send commands to Gaim via ~/.gaim/control
 *
 * Originally by Eric Warmenhoven <eric@warmenhoven.org>
 * Compile fixes/mini hacks Alex Bennee <alex@bennee.com>
 * and Brian Tarricone <bjt23@users.sourceforge.net>
 */

/* system includes */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

#include "account.h"
#include "config.h"
#include "core.h"
#include "conversation.h"
#include "debug.h"
#include "eventloop.h"
#include "internal.h"
#include "util.h"
#include "version.h"

#define FILECTL_PLUGIN_ID "core-filectl"
static int check;
static time_t mtime;

static void init_file(void);
static gboolean check_file(void);

/* parse char * as if were word array */
char *getarg(char *, int, int);

/* go through file and run any commands */
void
run_commands()
{
	struct stat finfo;
	char filename[256];
	char buffer[1024];
	char *command, *arg1, *arg2;
	FILE *file;

	sprintf(filename, "%s" G_DIR_SEPARATOR_S "control", gaim_user_dir());

	file = g_fopen(filename, "r+");
	while (fgets(buffer, sizeof(buffer), file)) {

		/* Read the next command */
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = 0;
		gaim_debug_misc("filectl", "read: %s\n", buffer);
		command = getarg(buffer, 0, 0);

		if (!strncasecmp(command, "login", 6)) {
			GaimAccount *account;

			arg1 = getarg(buffer, 1, 0);
			arg2 = getarg(buffer, 2, 1);

			account = gaim_accounts_find(arg1, arg2);
			if (account != NULL) /* username found */
				gaim_account_connect(account);

			free(arg1);
			free(arg2);

		} else if (!strncasecmp(command, "logout", 7)) {
			GaimAccount *account;

			arg1 = getarg(buffer, 1, 1);
			arg2 = getarg(buffer, 2, 1);

			account = gaim_accounts_find(arg1, arg2);
			if (account != NULL)
			{
				gaim_account_disconnect(account);
			}
			else if (arg1 == NULL)
				gaim_connections_disconnect_all();

			free(arg1);
			free(arg2);

/* gaim_find_conversation() is gone in 2.0.0. */
#if 0
		} else if (!strncasecmp(command, "send", 4)) {
			GaimConversation *conv;

			arg1 = getarg(buffer, 1, 0);
			arg2 = getarg(buffer, 2, 1);

			conv = gaim_find_conversation(GAIM_CONV_TYPE_ANY, arg1);
			if (conv != NULL)
			{
				/*
				gaim_conversation_write(conv, arg2, WFLAG_SEND, NULL, time(NULL), -1);
				serv_send_im(conv->gc, arg1, arg2, 0);
				*/
			}

			free(arg1);
			free(arg2);
#endif

		} else if (!strncasecmp(command, "away", 4)) {
			arg1 = getarg(buffer, 1, 1);
			/* serv_set_away_all(arg1); */
			free(arg1);

		} else if (!strncasecmp(command, "hide", 4)) {
			gaim_blist_set_visible(FALSE);

		} else if (!strncasecmp(command, "unhide", 6)) {
			gaim_blist_set_visible(TRUE);

		} else if (!strncasecmp(command, "back", 4)) {
			/* do_im_back(); */

		} else if (!strncasecmp(command, "quit", 4)) {
			gaim_core_quit();

		}

		free(command);
	}

	fclose(file);

	if (g_stat(filename, &finfo) != 0)
		return;
	mtime = finfo.st_mtime;
}

/**
 * Check to see if the size of the file is > 0. if so, run commands.
 */
void
init_file()
{
	/* most of this was taken from Bash v2.04 by the FSF */
	struct stat finfo;
	char filename[256];

	sprintf(filename, "%s" G_DIR_SEPARATOR_S "control", gaim_user_dir());

	if ((g_stat(filename, &finfo) == 0) && (finfo.st_size > 0))
		run_commands();
}

/**
 * Check to see if we need to run commands from the file.
 */
gboolean
check_file()
{
	/* most of this was taken from Bash v2.04 by the FSF */
	struct stat finfo;
	char filename[256];

	sprintf(filename, "%s" G_DIR_SEPARATOR_S "control", gaim_user_dir());

	if ((g_stat(filename, &finfo) == 0) && (finfo.st_size > 0))
	{
		if (mtime != finfo.st_mtime) {
			gaim_debug_info("filectl", "control changed, checking\n");
			run_commands();
		}
	}

	return TRUE;
}

char *
getarg(char *line, int which, int remain)
{
	char *arr;
	char *val;
	int count = -1;
	int i;
	int state = 0;

	for (i = 0; i < strlen(line) && count < which; i++) {
		switch (state) {
		case 0: /* in whitespace, expecting word */
			if (isalnum(line[i])) {
				count++;
				state = 1;
			}
			break;
		case 1: /* inside word, waiting for whitespace */
			if (isspace(line[i])) {
				state = 0;
			}
			break;
		}
	}

	arr = strdup(&line[i - 1]);
	if (remain)
		return arr;

	for (i = 0; i < strlen(arr) && isalnum(arr[i]); i++);
	arr[i] = 0;
	val = strdup(arr);
	arr[i] = ' ';
	free(arr);
	return val;
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	init_file();
	check = gaim_timeout_add(5000, (GSourceFunc)check_file, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_timeout_remove(check);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	FILECTL_PLUGIN_ID,                                /**< id             */
	N_("File Control"),                               /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Allows control by entering commands in a file."),
	                                                  /**  description    */
	N_("Allows control by entering commands in a file."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	GAIM_WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(filectl, init_plugin, info)

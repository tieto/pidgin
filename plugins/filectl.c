/*
** Send commands to gaim via ~/.gaim/control
**
** By Eric Warmenhoven <eric@warmenhoven.org>
** compile fixes/mini hacks Alex Bennee <alex@bennee.com>
*/

/* system includes */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

/* gaim includes */
#include "internal.h"

#include "config.h"
#include "gaim.h"
#include "debug.h"
#include "account.h"
#include "conversation.h"
#include "version.h"


#define FILECTL_PLUGIN_ID "core-filectl"
static int check;
static time_t mtime;

static void init_file();
static gboolean check_file();

/* parse char * as if were word array */
char *getarg(char *, int, int);

/* go through file and run any commands */
void run_commands() {
	struct stat finfo;
	char filename[256];
	char buffer[1024];
	char *command, *arg1, *arg2;
	FILE *file;

	sprintf(filename, "%s/.gaim/control", getenv("HOME"));

	file = fopen(filename, "r+");
	while (fgets(buffer, sizeof buffer, file)) {
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = 0;
		gaim_debug(GAIM_DEBUG_MISC, "filectl", "read: %s\n", buffer);
		command = getarg(buffer, 0, 0);
		if (!strncasecmp(command, "signon", 6)) {
			GaimAccount *account = NULL;
			GList *accts = gaim_accounts_get_all();
			arg1 = getarg(buffer, 1, 1);
			if (arg1) {
				while (accts) {
					GaimAccount *a = accts->data;
					if (!strcmp(a->username, arg1)) {
						account = a;
						break;
					}
					accts = accts->next;
				}
				free(arg1);
			}
			if (account) /* username found */
				gaim_account_connect(account);
		} else if (!strncasecmp(command, "signoff", 7)) {
			GaimConnection *gc = NULL;
			GList *c = gaim_connections_get_all();
			arg1 = getarg(buffer, 1, 1);
			while (arg1 && c) {
				gc = c->data;
				if (!strcmp(gc->account->username, arg1)) {
					break;
				}
				gc = NULL;
				c = c->next;
			}
			if (gc)
				gaim_connection_disconnect(gc);
			else if (!arg1)
				gaim_connections_disconnect_all();
			free(arg1);
		} else if (!strncasecmp(command, "send", 4)) {
			GaimConversation *c;
			arg1 = getarg(buffer, 1, 0);
			arg2 = getarg(buffer, 2, 1);
			c = gaim_find_conversation(GAIM_CONV_ANY, arg1);
			if (c)
			{
				/* disable for now
				gaim_conversation_write(c, arg2, WFLAG_SEND, NULL, time(NULL), -1);
				serv_send_im(c->gc, arg1, arg2, 0);
				*/
			}
			free(arg1);
			free(arg2);
		} else if (!strncasecmp(command, "away", 4)) {
			arg1 = getarg(buffer, 1, 1);
			serv_set_away_all(arg1);
			free(arg1);
		} else if (!strncasecmp(command, "hide", 4)) {
			/* hide_buddy_list(); */
		} else if (!strncasecmp(command, "unhide", 6)) {
			/* unhide_buddy_list(); */
		} else if (!strncasecmp(command, "back", 4)) {
			/* do_im_back(); */
		} else if (!strncasecmp(command, "quit", 4)) {
			/* gaim_core_quit(); */
		}
		free(command);
	}

	fclose(file);

	if (stat (filename, &finfo) != 0)
		return;
	mtime = finfo.st_mtime;
}

/* check to see if the size of the file is > 0. if so, run commands */
void init_file() {
	/* most of this was taken from Bash v2.04 by the FSF */
	struct stat finfo;
	char file[256];

	sprintf(file, "%s/.gaim/control", getenv("HOME"));

	if ((stat (file, &finfo) == 0) && (finfo.st_size > 0))
		run_commands();
}

/* check to see if we need to run commands from the file */
gboolean check_file() {
	/* most of this was taken from Bash v2.04 by the FSF */
	struct stat finfo;
	char file[256];

	sprintf(file, "%s/.gaim/control", getenv("HOME"));

	if ((stat (file, &finfo) == 0) && (finfo.st_size > 0))
	{
		if (mtime != finfo.st_mtime) {
			gaim_debug(GAIM_DEBUG_INFO, "filectl",
					   "control changed, checking\n");
			run_commands();
		}
	}

	return TRUE;
}

char *getarg(char *line, int which, int remain) {
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
	check = g_timeout_add(5000, (GSourceFunc) check_file, NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	g_source_remove(check);

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
	N_("Gaim File Control"),                          /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Allows you to control Gaim by entering commands in a file."),
	                                                  /**  description    */
	N_("Allows you to control Gaim by entering commands in a file."),
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

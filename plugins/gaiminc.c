//#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "gaim.h"

#define GAIMINC_PLUGIN_ID "core-gaiminc"

void echo_hi(void *m) {
	/* this doesn't do much, just lets you know who we are :) */
	show_about(NULL, NULL);
}

void reverse(struct gaim_connection *gc, char **who, char **message, void *m) {
	/* this will drive you insane. whenever you receive a message,
	 * the text of the message (HTML and all) will be reversed. */
	int i, l;
	char tmp;

	/* this check is necessary in case bad plugins do bad things */
	if (message == NULL || *message == NULL)
		return;

	l = strlen(*message);

	if (!strcmp(*who, gc->username))
		return;

	for (i = 0; i < l/2; i++) {
		tmp = (*message)[i];
		(*message)[i] = (*message)[l - i - 1];
		(*message)[l - i - 1] = tmp;
	}
}

void bud(struct gaim_connection *gc, char *who, void *m) {
	/* whenever someone comes online, it sends them a message. if i
	 * cared more, i'd make it so it popped up on your screen too */
	serv_send_im(gc, who, "Hello!", -1, 0);
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	/* this is for doing something fun when we sign on */
	gaim_signal_connect(plugin, event_signon, echo_hi, NULL);

	/* this is for doing something fun when we get a message */
	gaim_signal_connect(plugin, event_im_recv, reverse, NULL);

	/* this is for doing something fun when a buddy comes online */
	gaim_signal_connect(plugin, event_buddy_signon, bud, NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	GAIMINC_PLUGIN_ID,                                /**< id             */
	N_("Gaim Demonstration Plugin"),                  /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("An example plugin that does stuff - see the description."),
	                                                  /**  description    */
	N_("This is a really cool plugin that does a lot of stuff:\n"
	   "- It tells you who wrote the program when you log in\n"
	   "- It reverses all incoming text\n"
	   "- It sends a message to people on your list immediately"
	   " when they sign on"),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(gaiminc, init_plugin, info)

/* events.c
 *
 * test every callback, print to stdout
 *
 * by EW
 *
 * GPL and all that jazz
 *
 */

#define EVENTTEST_PLUGIN_ID "core-eventtest"
#include "gaim.h"

static void evt_signon(struct gaim_connection *gc, void *data)
{
	printf("event_signon\n");
}

static void evt_signoff(struct gaim_connection *gc, void *data)
{
	printf("event_signoff\n");
}

static void evt_away(struct gaim_connection *gc, char *state, char *message, void *data)
{
	printf("event_away: %s %s %s\n", gc->username, state, message);
}

static void evt_back(void *data)
{
	printf("event_back\n");
}

static void evt_im_recv(struct gaim_connection *gc, char **who, char **what, guint *flags, void *data)
{
	printf("event_im_recv: %s %s\n", *who, *what);
}

static void evt_im_send(struct gaim_connection *gc, char *who, char **what, void *data)
{
	printf("event_im_send: %s %s\n", who, *what);
}

static void evt_buddy_signon(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_signon: %s\n", who);
}

static void evt_buddy_signoff(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_signoff: %s\n", who);
}

static void evt_buddy_away(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_away: %s\n", who);
}

static void evt_buddy_back(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_back: %s\n", who);
}

static void evt_buddy_idle(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_idle: %s\n", who);
}

static void evt_buddy_unidle(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_buddy_unidle: %s\n", who);
}

static void evt_blist_update(void *data)
{
	printf("event_blist_update\n");
}

static void evt_chat_invited(struct gaim_connection *gc, char *who, char *room, char *message, void *data)
{
	printf("event_chat_invited: %s %s %s\n", who, room, message);
}

static void evt_chat_join(struct gaim_connection *gc, int id, void *data)
{
	printf("event_chat_join: %d\n", id);
}

static void evt_chat_leave(struct gaim_connection *gc, int id, void *data)
{
	printf("event_chat_leave: %d\n", id);
}

static void evt_chat_buddy_join(struct gaim_connection *gc, int id, char *who, void *data)
{
	printf("event_chat_buddy_join: %d %s\n", id, who);
}

static void evt_chat_buddy_leave(struct gaim_connection *gc, int id, char *who, void *data)
{
	printf("event_chat_buddy_leave: %d %s\n", id, who);
}

static void evt_chat_recv(struct gaim_connection *gc, int id, char *who, char *text, void *data)
{
	printf("event_chat_recv: %d %s %s\n", id, who, text);
}

static void evt_chat_send(struct gaim_connection *gc, int id, char **what, void *data)
{
	printf("event_chat_send: %d %s\n", id, *what);
}

static void evt_warned(struct gaim_connection *gc, char *who, int level, void *data)
{
	printf("event_warned: %s %d\n", who, level);
}

static void evt_error(int error, void *data)
{
	printf("event_error: %d\n", error);
}

static void evt_quit(void *data)
{
	printf("event_quit\n");
}

static void evt_new_conversation(char *who, void *data)
{
	printf("event_new_conversation: %s\n", who);
}

static void evt_set_info(struct gaim_connection *gc, char *info, void *data)
{
	printf("event_set_info: %s\n", info);
}

static void evt_draw_menu(GtkWidget *menu, char *name, void *data)
{
	printf("event_draw_menu: %s\n", name);
}

static void evt_im_displayed_sent(struct gaim_connection *gc, char *who, char **what, void *data)
{
	printf("event_im_displayed_sent: %s %s\n", who, *what);
}

static void evt_im_displayed_rcvd(struct gaim_connection *gc, char *who, char *what, guint32 flags, time_t time, void *data)
{
	printf("event_im_displayed_rcvd: %s %s %u %u\n", who, what, flags, time);
}

static void evt_chat_send_invite(struct gaim_connection *gc, int id, char *who, char **msg, void *data)
{
	printf("event_chat_send_invite: %d %s %s\n", id, who, *msg);
}

static evt_got_typing(struct gaim_connection *gc, char *who, void *data)
{
	printf("event_got_typing: %s\n", who);
}

static evt_del_conversation(struct gaim_conversation *c, void *data)
{
	printf("event_del_conversation\n");
}

static evt_connecting(struct gaim_account *u, void *data)
{
	printf("event_connecting\n");
}


/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(plugin, event_signon,		evt_signon, NULL);
	gaim_signal_connect(plugin, event_signoff,		evt_signoff, NULL);
	gaim_signal_connect(plugin, event_away,		evt_away, NULL);
	gaim_signal_connect(plugin, event_back,		evt_back, NULL);
	gaim_signal_connect(plugin, event_im_recv,		evt_im_recv, NULL);
	gaim_signal_connect(plugin, event_im_send,		evt_im_send, NULL);
	gaim_signal_connect(plugin, event_buddy_signon,	evt_buddy_signon, NULL);
	gaim_signal_connect(plugin, event_buddy_signoff,	evt_buddy_signoff, NULL);
	gaim_signal_connect(plugin, event_buddy_away,	evt_buddy_away, NULL);
	gaim_signal_connect(plugin, event_buddy_back,	evt_buddy_back, NULL);
	gaim_signal_connect(plugin, event_chat_invited,	evt_chat_invited, NULL);
	gaim_signal_connect(plugin, event_chat_join,		evt_chat_join, NULL);
	gaim_signal_connect(plugin, event_chat_leave,	evt_chat_leave, NULL);
	gaim_signal_connect(plugin, event_chat_buddy_join,	evt_chat_buddy_join, NULL);
	gaim_signal_connect(plugin, event_chat_buddy_leave,	evt_chat_buddy_leave, NULL);
	gaim_signal_connect(plugin, event_chat_recv,		evt_chat_recv, NULL);
	gaim_signal_connect(plugin, event_chat_send,		evt_chat_send, NULL);
	gaim_signal_connect(plugin, event_warned,		evt_warned, NULL);
	gaim_signal_connect(plugin, event_error,		evt_error, NULL);
	gaim_signal_connect(plugin, event_quit,		evt_quit, NULL);
	gaim_signal_connect(plugin, event_new_conversation,	evt_new_conversation, NULL);
	gaim_signal_connect(plugin, event_set_info,		evt_set_info, NULL);
	gaim_signal_connect(plugin, event_draw_menu,		evt_draw_menu, NULL);
	gaim_signal_connect(plugin, event_im_displayed_sent,	evt_im_displayed_sent, NULL);
	gaim_signal_connect(plugin, event_im_displayed_rcvd, evt_im_displayed_rcvd, NULL);
	gaim_signal_connect(plugin, event_chat_send_invite,	evt_chat_send_invite, NULL);
	gaim_signal_connect(plugin, event_got_typing, 	evt_got_typing, NULL);
	gaim_signal_connect(plugin, event_del_conversation,	evt_del_conversation, NULL);
	gaim_signal_connect(plugin, event_connecting,	evt_connecting, NULL);

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

	EVENTTEST_PLUGIN_ID,                              /**< id             */
	N_("Event Test"),                                 /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Test to see that all events are working properly."),
	                                                  /**  description    */
	N_("Test to see that all events are working properly."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(eventtester, __init_plugin, info);

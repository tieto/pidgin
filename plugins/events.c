/* tester.c
 *
 * test every callback, print to stdout
 *
 * by EW
 *
 * GPL and all that jazz
 *
 */

#define GAIM_PLUGINS
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

static void evt_im_recv(struct gaim_connection *gc, char **who, char **what, void *data)
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

static void evt_chat_invited(struct gaim_connection *gc, char *who, char *room, char *message, void *data)
{
	printf("event_chat_invited: %s %s %s\n", who, room, message);
}

static void evt_chat_join(struct gaim_connection *gc, char *room, void *data)
{
	printf("event_chat_join: %s\n", room);
}

static void evt_chat_leave(struct gaim_connection *gc, char *room, void *data)
{
	printf("event_chat_leave: %s\n", room);
}

static void evt_chat_buddy_join(struct gaim_connection *gc, char *room, char *who, void *data)
{
	printf("event_chat_buddy_join: %s %s\n", room, who);
}

static void evt_chat_buddy_leave(struct gaim_connection *gc, char *room, char *who, void *data)
{
	printf("event_chat_buddy_leave: %s %s\n", room, who);
}

static void evt_chat_recv(struct gaim_connection *gc, char *room, char *who, char *text, void *data)
{
	printf("event_chat_recv: %s %s %s\n", room, who, text);
}

static void evt_chat_send(struct gaim_connection *gc, char *room, char **what, void *data)
{
	printf("event_chat_send: %s %s\n", room, *what);
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

char *gaim_plugin_init(GModule *h)
{
	gaim_signal_connect(h, event_signon,           evt_signon, NULL);
	gaim_signal_connect(h, event_signoff,          evt_signoff, NULL);
	gaim_signal_connect(h, event_away,             evt_away, NULL);
	gaim_signal_connect(h, event_back,             evt_back, NULL);
	gaim_signal_connect(h, event_im_recv,          evt_im_recv, NULL);
	gaim_signal_connect(h, event_im_send,          evt_im_send, NULL);
	gaim_signal_connect(h, event_buddy_signon,     evt_buddy_signon, NULL);
	gaim_signal_connect(h, event_buddy_signoff,    evt_buddy_signoff, NULL);
	gaim_signal_connect(h, event_buddy_away,       evt_buddy_away, NULL);
	gaim_signal_connect(h, event_buddy_back,       evt_buddy_back, NULL);
	gaim_signal_connect(h, event_chat_invited,     evt_chat_invited, NULL);
	gaim_signal_connect(h, event_chat_join,        evt_chat_join, NULL);
	gaim_signal_connect(h, event_chat_leave,       evt_chat_leave, NULL);
	gaim_signal_connect(h, event_chat_buddy_join,  evt_chat_buddy_join, NULL);
	gaim_signal_connect(h, event_chat_buddy_leave, evt_chat_buddy_leave, NULL);
	gaim_signal_connect(h, event_chat_recv,        evt_chat_recv, NULL);
	gaim_signal_connect(h, event_chat_send,        evt_chat_send, NULL);
	gaim_signal_connect(h, event_warned,           evt_warned, NULL);
	gaim_signal_connect(h, event_error,            evt_error, NULL);
	gaim_signal_connect(h, event_quit,             evt_quit, NULL);
	gaim_signal_connect(h, event_new_conversation, evt_new_conversation, NULL);
	return NULL;
}

char *name()
{
	return "Event Test";
}

char *description()
{
	return "Test to see that all events are working properly.";
}

/* tester.c
 *
 * test every callback, print to stdout
 *
 * by EWarmenhoven
 *
 * GPL and all that jazz
 *
 */

#define GAIM_PLUGINS
#include "gaim.h"

void evt_signon(void *data)
{
	printf("event_signon\n");
}

void evt_signoff(void *data)
{
	printf("event_signoff\n");
}

void evt_away(void *data)
{
	printf("event_away\n");
}

void evt_back(void *data)
{
	printf("event_back\n");
}

void evt_im_recv(char **who, char **what, void *data)
{
	printf("event_im_recv: %s %s\n", *who, *what);
}

void evt_im_send(char *who, char **what, void *data)
{
	printf("event_im_send: %s %s\n", who, *what);
}

void evt_buddy_signon(char *who, void *data)
{
	printf("event_buddy_signon: %s\n", who);
}

void evt_buddy_signoff(char *who, void *data)
{
	printf("event_buddy_signoff: %s\n", who);
}

void evt_buddy_away(char *who, void *data)
{
	printf("event_buddy_away: %s\n", who);
}

void evt_buddy_back(char *who, void *data)
{
	printf("event_buddy_back: %s\n", who);
}

void evt_blist_update(void *data)
{
	printf("event_blist_update\n");
}

void evt_chat_invited(char *who, char *room, char *message, void *data)
{
	printf("event_chat_invited: %s %s %s\n", who, room, message);
}

void evt_chat_join(char *room, void *data)
{
	printf("event_chat_join: %s\n", room);
}

void evt_chat_leave(char *room, void *data)
{
	printf("event_chat_leave: %s\n", room);
}

void evt_chat_buddy_join(char *room, char *who, void *data)
{
	printf("event_chat_buddy_join: %s %s\n", room, who);
}

void evt_chat_buddy_leave(char *room, char *who, void *data)
{
	printf("event_chat_buddy_leave: %s %s\n", room, who);
}

void evt_chat_recv(char *room, char *who, char *text, void *data)
{
	printf("event_chat_recv: %s %s %s\n", room, who, text);
}

void evt_chat_send(char *room, char **what, void *data)
{
	printf("event_chat_send: %s %s\n", room, *what);
}

void evt_warned(char *who, int level, void *data)
{
	printf("event_warned: %s %d\n", who, level);
}

void evt_error(int error, void *data)
{
	printf("event_error: %d\n", error);
}

void evt_quit(void *data)
{
	printf("event_quit\n");
}

int gaim_plugin_init(void *h)
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
	gaim_signal_connect(h, event_blist_update,     evt_blist_update, NULL);
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
	return 0;
}

char *name()
{
	return "Event Test";
}

char *description()
{
	return "Test to see that all events are working properly.";
}

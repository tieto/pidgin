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

void one_arg(void *m)
{
	printf("%d\n", m);
}

void two_arg(char *n, void *m)
{
	printf("%d\n", m);
}

void three_arg(char *n, char *o, void *m)
{
	printf("%d\n", m);
}

void four_arg(char *n, char *o, char *p, void *m)
{
	printf("%d\n", m);
}

int gaim_plugin_init(void *h)
{
	gaim_signal_connect(h, event_signon,           one_arg,   (void *)0);
	gaim_signal_connect(h, event_signoff,          one_arg,   (void *)1);
	gaim_signal_connect(h, event_away,             one_arg,   (void *)2);
	gaim_signal_connect(h, event_back,             one_arg,   (void *)3);
	gaim_signal_connect(h, event_im_recv,          three_arg, (void *)4);
	gaim_signal_connect(h, event_im_send,          three_arg, (void *)5);
	gaim_signal_connect(h, event_buddy_signon,     two_arg,   (void *)6);
	gaim_signal_connect(h, event_buddy_signoff,    two_arg,   (void *)7);
	gaim_signal_connect(h, event_buddy_away,       two_arg,   (void *)8);
	gaim_signal_connect(h, event_buddy_back,       two_arg,   (void *)9);
	gaim_signal_connect(h, event_blist_update,     one_arg,   (void *)10);
	gaim_signal_connect(h, event_chat_invited,     four_arg,  (void *)11);
	gaim_signal_connect(h, event_chat_join,        two_arg,   (void *)12);
	gaim_signal_connect(h, event_chat_leave,       two_arg,   (void *)13);
	gaim_signal_connect(h, event_chat_buddy_join,  three_arg, (void *)14);
	gaim_signal_connect(h, event_chat_buddy_leave, three_arg, (void *)15);
	gaim_signal_connect(h, event_chat_recv,        four_arg,  (void *)16);
	gaim_signal_connect(h, event_chat_send,        three_arg, (void *)17);
	gaim_signal_connect(h, event_warned,           three_arg, (void *)18);
	gaim_signal_connect(h, event_error,            two_arg,   (void *)19);
	gaim_signal_connect(h, event_quit,             one_arg,   (void *)20);
}

char *name()
{
	return "Event Test";
}

char *description()
{
	return "Test to see that all events are working properly.";
}

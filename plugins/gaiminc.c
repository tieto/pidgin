#define GAIM_PLUGINS

#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "gaim.h"

void echo_hi(void *m) {
	/* this doesn't do much, just lets you know who we are :) */
	show_about(NULL, NULL);
}

void reverse(char **who, char **message, void *m) {
	/* this will drive you insane. whenever you receive a message,
	 * the text of the message (HTML and all) will be reversed. */
	int i, l;
	char tmp;

	/* this check is necessary in case bad plugins do bad things */
	if (message == NULL || *message == NULL)
		return;

	l = strlen(*message);

	if (!strcmp(*who, current_user->username))
		return;

	for (i = 0; i < l/2; i++) {
		tmp = (*message)[i];
		(*message)[i] = (*message)[l - i - 1];
		(*message)[l - i - 1] = tmp;
	}
}

void bud(char *who, void *m) {
	/* whenever someone comes online, it sends them a message. if i
	 * cared more, i'd make it so it popped up on your screen too */
	serv_send_im(who, "Hello!", 0);
}

void gaim_plugin_init(void *handle) {
	/* this is for doing something fun when we sign on */
	gaim_signal_connect(handle, event_signon, echo_hi, NULL);

	/* this is for doing something fun when we get a message */
	gaim_signal_connect(handle, event_im_recv, reverse, NULL);

	/* this is for doing something fun when a buddy comes online */
	gaim_signal_connect(handle, event_buddy_signon, bud, NULL);
}

char *name() {
	return "Gaim Demonstration Plugin";
}

char *description() {
	return "This is a really cool plugin that does a lot of stuff:\n"
		"- It tells you who wrote the program when you log in\n"
		"- It reverses all incoming text\n"
		"- It sends a message to people on your list immediately"
		" when they sign on";
}

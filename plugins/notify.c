#define GAIM_PLUGINS
#include "gaim.h"

#include <gtk/gtk.h>
#include <string.h>

void *handle;

void received_im(struct gaim_connection *gc, char **who, char **what, void *m) {
	char buf[256];
	struct conversation *cnv = find_conversation(*who);
	GtkWindow *win;
	char *me = g_strdup(normalize(gc->username));

	if (!strcmp(me, normalize(*who))) {
		g_free(me);
		return;
	}
	g_free(me);

	if (cnv == NULL)
		cnv = new_conversation(*who);

	win = (GtkWindow *)cnv->window;

	g_snprintf(buf, sizeof(buf), "%s", win->title);
	if (!strstr(buf, "(*) ")) {
		g_snprintf(buf, sizeof(buf), "(*) %s", win->title);
		gtk_window_set_title(win, buf);
	}
}

void sent_im(struct gaim_connection *gc, char *who, char **what, void *m) {
	char buf[256];
	struct conversation *c = find_conversation(who);
	GtkWindow *win = (GtkWindow *)c->window;

	g_snprintf(buf, sizeof(buf), "%s", win->title);
	if (strstr(buf, "(*) ")) {
		g_snprintf(buf, sizeof(buf), "%s", &win->title[4]);
		gtk_window_set_title(win, buf);
	}
}

char *gaim_plugin_init(GModule *hndl) {
	handle = hndl;

	gaim_signal_connect(handle, event_im_recv, received_im, NULL);
	gaim_signal_connect(handle, event_im_send, sent_im, NULL);

	return NULL;
}

char *name() {
	return "Visual Notification";
}

char *description() {
	return "Puts an asterisk in the title bar of all conversations"
		" where you have not responded to a message yet.";
}

#include "config.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include "gaim.h"

#include <gtk/gtk.h>

#ifdef _WIN32
#include "win32dep.h"
#endif

void *handle;

G_MODULE_IMPORT GtkWidget *imaway;
/*G_MODULE_IMPORT GtkWidget *blist;*/
/* XXX G_MODULE_IMPORT GtkWidget *all_chats; */
/*G_MODULE_IMPORT GtkWidget *all_convos;*/

#ifdef USE_APPLET
extern void applet_destroy_buddy();
#endif

void iconify_windows(struct gaim_connection *gc, char *state,
					 char *message, void *data) {
	struct gaim_window *win;
	GList *windows;

	if (!imaway || !gc->away)
		return;

	gtk_window_iconify(GTK_WINDOW(imaway));
	gaim_blist_set_visible(FALSE);

	for (windows = gaim_get_windows();
		 windows != NULL;
		 windows = windows->next) {

		win = (struct gaim_window *)windows->data;

		if (GAIM_IS_GTK_WINDOW(win)) {
			struct gaim_gtk_window *gtkwin;

			gtkwin = GAIM_GTK_WINDOW(win);

			gtk_window_iconify(GTK_WINDOW(gtkwin->window));
		}
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

G_MODULE_EXPORT char *gaim_plugin_init(GModule *h) {
	handle = h;

	gaim_signal_connect(handle, event_away, iconify_windows, NULL);

	return NULL;
}

struct gaim_plugin_description desc; 
G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("Iconify on away"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Iconifies the away box and the buddy list when you go away."));
	desc.authors = g_strdup("Eric Warmenhoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}
 
G_MODULE_EXPORT char *name() {
	return _("Iconify on away");
}

G_MODULE_EXPORT char *description() {
	return _("Iconifies the away box and the buddy list when you go away.");
}

/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "config.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include "gaim.h"
#include "gtkimhtml.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define HISTORY_SIZE (4 * 1024)

GModule *handle;

void historize (char *name, void *data)
{
	struct gaim_conversation *c = gaim_find_conversation(name);
	struct gaim_gtk_conversation *gtkconv;
	struct stat st;
	FILE *fd;
	char *userdir = g_strdup(gaim_user_dir());
	char *logfile = g_strdup_printf("%s.log", normalize(name));
	char *path = g_build_filename(userdir, "logs", logfile, NULL);
	char buf[HISTORY_SIZE+1];
	char *tmp;
	int size;
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
	
	if (stat(path, &st) || S_ISDIR(st.st_mode) || st.st_size == 0 || 
	    !(fd = fopen(path, "r"))) {
		g_free(userdir);
		g_free(logfile);
		g_free(path);
		return;
	}
      
	fseek(fd, st.st_size > HISTORY_SIZE ? st.st_size - HISTORY_SIZE : 0, SEEK_SET);
	size = fread(buf, 1, HISTORY_SIZE, fd);
	tmp = buf;
	tmp[size] = 0;
	
	/* start the history at a newline */
	while (*tmp && *tmp != '\n')
		tmp++;

	if (*tmp) tmp++;

	if(*tmp == '<')
		options |= GTK_IMHTML_NO_NEWLINE;

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), tmp, strlen(tmp), options);

	g_free(userdir);
	g_free(logfile);
	g_free(path);
}

G_MODULE_EXPORT char *gaim_plugin_init(GModule *h) {
	handle = h;

	gaim_signal_connect(handle, event_new_conversation, historize, NULL);

	return NULL;
}

struct gaim_plugin_description desc; 
G_MODULE_EXPORT struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("History"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Shows recently logged conversations in new conversations "));
	desc.authors = g_strdup("Sean Egan &lt;bj91704@binghamton.edu>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

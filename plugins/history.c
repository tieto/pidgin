/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#define GAIM_PLUGINS
#include "gaim.h"
#include "gtkimhtml.h"
#include <sys/stat.h>
#include <unistd.h>

#define HISTORY_SIZE (4 * 1024)

GModule *handle;

void historize (char *name, void *data)
{
	struct conversation *c = find_conversation(name);
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

	gtk_imhtml_append_text(GTK_IMHTML(c->text), tmp, strlen(tmp), options);

	g_free(userdir);
	g_free(logfile);
	g_free(path);
}

char *gaim_plugin_init(GModule *h) {
	handle = h;

	gaim_signal_connect(handle, event_new_conversation, historize, NULL);

	return NULL;
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup(_("History"));
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup(_("Shows recently logged conversations in new conversations "));
	desc.authors = g_strdup("Sean Egan &lt;bj91704@binghamton.edu>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "gtkinternal.h"

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define HISTORY_PLUGIN_ID "gtk-history"

#define HISTORY_SIZE (4 * 1024)

static void historize(GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	const char *name = gaim_conversation_get_name(c);
	struct stat st;
	FILE *fd;
	char *userdir = g_strdup(gaim_user_dir());
	char *logfile = g_strdup_printf("%s.log", gaim_normalize(name));
	char *path = g_build_filename(userdir, "logs", logfile, NULL);
	char buf[HISTORY_SIZE+1];
	char *tmp, *tmp2;
	int size;
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
	GtkTextIter end;

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

	if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_urls_as_links"))
		tmp2 = gaim_markup_linkify(tmp);
	else
		tmp2 = g_strdup(tmp);

	gtkconv = GAIM_GTK_CONVERSATION(c);

	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), tmp2, options);
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<br>", options);
	gtk_text_buffer_get_end_iter(GTK_IMHTML(gtkconv->imhtml)->text_buffer,
			&end);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(gtkconv->imhtml), &end, 0,
			TRUE, 0, 0);

	g_free(tmp2);
	g_free(userdir);
	g_free(logfile);
	g_free(path);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_conversations_get_handle(),
						"conversation-created",
						plugin, GAIM_CALLBACK(historize), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	2,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("History"),
	VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert the last XXX of the last conversation into the current conversation."),
	"Sean Egan <bj91704@binghamton.edu>",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)

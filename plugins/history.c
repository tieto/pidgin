/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "gtkinternal.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
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
	char *history = NULL;
	guint flags;
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
	GtkTextIter end;
	GList *logs = gaim_log_get_logs(gaim_conversation_get_name(c),
			gaim_conversation_get_account(c));

	if (!logs)
		return;
	history = gaim_log_read((GaimLog*)logs->data, &flags);
	gtkconv = GAIM_GTK_CONVERSATION(c);
	if (flags & GAIM_LOG_READ_NO_NEWLINE)
		options |= GTK_IMHTML_NO_NEWLINE;
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), history, options);
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<hr>", options);
	gtk_text_buffer_get_end_iter(GTK_IMHTML(gtkconv->imhtml)->text_buffer,
			&end);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(gtkconv->imhtml), &end, 0,
			TRUE, 0, 0);
	g_free(history);
	while (logs) {
		GaimLog *log = logs->data;
		GList *logs2;
		g_free(log->name);
		g_free(log);
		logs2 = logs->next;
		g_list_free_1(logs);
		logs = logs2;
	}
	
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

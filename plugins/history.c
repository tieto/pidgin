/* Puts last 4k of log in new conversations a la Everybuddy (and then
 * stolen by Trillian "Pro") */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"

#define HISTORY_PLUGIN_ID "gtk-history"

#define HISTORY_SIZE (4 * 1024)

static gboolean _scroll_imhtml_to_end(gpointer data)
{
	GtkIMHtml *imhtml = data;
	gtk_imhtml_scroll_to_end(GTK_IMHTML(imhtml));
	g_object_unref(G_OBJECT(imhtml));
	return FALSE;
}

static void historize(GaimConversation *c)
{
	GaimGtkConversation *gtkconv;
	GaimConversationType convtype;
	char *history = NULL;
	guint flags;
	GtkIMHtmlOptions options = GTK_IMHTML_NO_COLOURS;
	GList *logs = NULL;

	convtype = gaim_conversation_get_type(c);
	if (convtype == GAIM_CONV_IM)
		logs = gaim_log_get_logs(GAIM_LOG_IM,
				gaim_conversation_get_name(c), gaim_conversation_get_account(c));
	else if (convtype == GAIM_CONV_CHAT)
		logs = gaim_log_get_logs(GAIM_LOG_CHAT,
				gaim_conversation_get_name(c), gaim_conversation_get_account(c));

	if (!logs)
		return;

	history = gaim_log_read((GaimLog*)logs->data, &flags);
	gtkconv = GAIM_GTK_CONVERSATION(c);
	if (flags & GAIM_LOG_READ_NO_NEWLINE)
		options |= GTK_IMHTML_NO_NEWLINE;
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<br>", options);
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), history, options);
	gtk_imhtml_append_text(GTK_IMHTML(gtkconv->imhtml), "<hr>", options);
	g_object_ref(G_OBJECT(gtkconv->imhtml));
	g_idle_add(_scroll_imhtml_to_end, gtkconv->imhtml);
	g_free(history);

	while (logs) {
		GaimLog *log = logs->data;
		GList *logs2;
	    gaim_log_free(log);
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
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	HISTORY_PLUGIN_ID,
	N_("History"),
	VERSION,
	N_("Shows recently logged conversations in new conversations."),
	N_("When a new conversation is opened this plugin will insert the last conversation into the current conversation."),
	"Sean Egan <bj91704@binghamton.edu>",
	GAIM_WEBSITE,
	plugin_load,
	NULL,
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

#include "gtkinternal.h"

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "gtkplugin.h"

#ifdef MAX
# undef MAX
# undef MIN
#endif

#include "protocols/jabber/jabber.h"
#include "protocols/msn/session.h"

#define RAW_PLUGIN_ID "gtk-raw"

static GtkWidget *window = NULL;
static GtkWidget *optmenu = NULL;
static GaimConnection *gc = NULL;
static GaimPlugin *me = NULL;

static int goodbye()
{
	gaim_plugin_unload(me);
	return FALSE;
}

static void send_it(GtkEntry *entry)
{
	const char *txt;
	if (!gc) return;
	txt = gtk_entry_get_text(entry);
	switch (gaim_connection_get_protocol(gc)) {
		case GAIM_PROTO_TOC:
			{
				int *a = (int *)gc->proto_data;
				unsigned short seqno = htons(a[1]++ & 0xffff);
				unsigned short len = htons(strlen(txt) + 1);
				write(*a, "*\002", 2);
				write(*a, &seqno, 2);
				write(*a, &len, 2);
				write(*a, txt, ntohs(len));
				gaim_debug(GAIM_DEBUG_MISC, "raw", "TOC C: %s\n", txt);
			}
			break;
		case GAIM_PROTO_MSN:
			{
				MsnSession *session = gc->proto_data;
				char buf[strlen(txt) + 3];

				g_snprintf(buf, sizeof(buf), "%s\r\n", txt);
				msn_servconn_write(session->notification_conn, buf, strlen(buf));
			}
			break;
		case GAIM_PROTO_IRC:
			write(*(int *)gc->proto_data, txt, strlen(txt));
			write(*(int *)gc->proto_data, "\r\n", 2);
			gaim_debug(GAIM_DEBUG_MISC, "raw", "IRC C: %s\n", txt);
			break;
		case GAIM_PROTO_JABBER:
			jab_send_raw(*(jconn *)gc->proto_data, txt);
			break;
	}
	gtk_entry_set_text(entry, "");
}

static void set_gc(gpointer d, GaimConnection *c)
{
	gc = c;
}

static void redo_optmenu(GaimConnection *arg, gpointer x)
{
	GtkWidget *menu;
	GList *g = gaim_connections_get_all();
	GaimConnection *c;
	GaimAccount *account;
	GaimPlugin *plugin;

	menu = gtk_menu_new();
	gc = NULL;

	while (g) {
		char buf[256];
		GtkWidget *opt;
		c = (GaimConnection *)g->data;
		g = g->next;
		if (x && c == arg)
			continue;
		if (gaim_connection_get_protocol(c) != GAIM_PROTO_TOC &&
			gaim_connection_get_protocol(c) != GAIM_PROTO_MSN &&
		    gaim_connection_get_protocol(c) != GAIM_PROTO_IRC &&
			gaim_connection_get_protocol(c) != GAIM_PROTO_JABBER)
			continue;
		if (!gc)
			gc = c;

		account = gaim_connection_get_account(c);

		plugin = gaim_find_prpl(gaim_account_get_protocol(account));

		g_snprintf(buf, sizeof buf, "%s (%s)",
				   gaim_account_get_username(account),
				   plugin->info->name);

		opt = gtk_menu_item_new_with_label(buf);
		g_signal_connect(G_OBJECT(opt), "activate", G_CALLBACK(set_gc), c);
		gtk_widget_show(opt);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), opt);
	}

	gtk_option_menu_remove_menu(GTK_OPTION_MENU(optmenu));
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GtkWidget *hbox;
	GtkWidget *entry;

	gaim_signal_connect(plugin, event_signon, redo_optmenu, NULL);
	gaim_signal_connect(plugin, event_signoff, redo_optmenu, me);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(goodbye), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 5);

	redo_optmenu(NULL, NULL);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(send_it), NULL);

	gtk_widget_show_all(window);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (window)
		gtk_widget_destroy(window);

	window = NULL;

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
	RAW_PLUGIN_ID,
	N_("Raw"),
	VERSION,
	N_("Lets you send raw input to text-based protocols."),
	N_("Lets you send raw input to text-based protocols (Jabber, MSN, IRC, "
	   "TOC). Hit 'Enter' in the entry box to send. Watch the debug window."),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	GAIM_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	me = plugin;
}

GAIM_INIT_PLUGIN(raw, init_plugin, info)

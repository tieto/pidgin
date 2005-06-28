#define GAIM_PLUGINS
#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "version.h"

#include "gtkgaim.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#ifdef MAX
# undef MAX
# undef MIN
#endif

#include "protocols/jabber/jabber.h"
#undef GAIM_PLUGINS
#include "protocols/msn/session.h"

#define RAW_PLUGIN_ID "gtk-raw"

static GtkWidget *window = NULL;
static GaimAccount *account = NULL;
static GaimPlugin *my_plugin = NULL;

static int
window_closed_cb()
{
	gaim_plugin_unload(my_plugin);

	return FALSE;
}

static void
text_sent_cb(GtkEntry *entry)
{
	const char *txt;
	GaimConnection *gc;
	const char *prpl_id;

	if (account == NULL)
		return;

	gc = gaim_account_get_connection(account);

	txt = gtk_entry_get_text(entry);

	prpl_id = gaim_account_get_protocol_id(account);
	
	gaim_debug_misc("raw", "prpl_id = %s\n", prpl_id);
	
	if (strcmp(prpl_id, "prpl-toc") == 0) {
		int *a = (int *)gc->proto_data;
		unsigned short seqno = htons(a[1]++ & 0xffff);
		unsigned short len = htons(strlen(txt) + 1);
		write(*a, "*\002", 2);
		write(*a, &seqno, 2);
		write(*a, &len, 2);
		write(*a, txt, ntohs(len));
		gaim_debug(GAIM_DEBUG_MISC, "raw", "TOC C: %s\n", txt);
		
	} else if (strcmp(prpl_id, "prpl-msn") == 0) {
		MsnSession *session = gc->proto_data;
		char buf[strlen(txt) + 3];

		g_snprintf(buf, sizeof(buf), "%s\r\n", txt);
		msn_servconn_write(session->notification->servconn, buf, strlen(buf));
		
	} else if (strcmp(prpl_id, "prpl-irc") == 0) {
		write(*(int *)gc->proto_data, txt, strlen(txt));
		write(*(int *)gc->proto_data, "\r\n", 2);
		gaim_debug(GAIM_DEBUG_MISC, "raw", "IRC C: %s\n", txt);

	} else if (strcmp(prpl_id, "prpl-jabber") == 0) {
		jabber_send_raw((JabberStream *)gc->proto_data, txt, -1);
	} else {
		gaim_debug_error("raw", "Unknown protocol ID %s\n", prpl_id);
	}

	gtk_entry_set_text(entry, "");
}

static void
account_changed_cb(GtkWidget *dropdown, GaimAccount *new_account,
				   void *user_data)
{
	account = new_account;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *dropdown;

	gaim_debug_register_category("raw");

	/* Setup the window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 6);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(window_closed_cb), NULL);

	/* Main hbox */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	/* Account drop-down menu. */
	dropdown = gaim_gtk_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(account_changed_cb), NULL, NULL);

	if (gaim_connections_get_all())
		account = (GaimAccount *)gaim_connections_get_all()->data;

	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);

	/* Entry box */
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(text_sent_cb), NULL);

	gtk_widget_show_all(window);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (window)
		gtk_widget_destroy(window);

	window = NULL;

	gaim_debug_register_category("raw");

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
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	my_plugin = plugin;
}

GAIM_INIT_PLUGIN(raw, init_plugin, info)

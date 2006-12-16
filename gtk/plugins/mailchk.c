#include "internal.h"

#include "debug.h"
#include "sound.h"
#include "version.h"

#include "gtkblist.h"
#include "gtkplugin.h"

#define MAILCHK_PLUGIN_ID "gtk-mailchk"

#define ANY_MAIL    0x01
#define UNREAD_MAIL 0x02
#define NEW_MAIL    0x04

static guint32 timer = 0;
static GtkWidget *mail = NULL;

static gint
check_mail()
{
	static off_t oldsize = 0;
	gchar *filename;
	off_t newsize;
	struct stat s;
	gint ret = 0;

	filename = g_strdup(g_getenv("MAIL"));
	if (!filename)
		filename = g_strconcat("/var/spool/mail/", g_get_user_name(), NULL);

	if (g_stat(filename, &s) < 0) {
		g_free(filename);
		return -1;
	}

	newsize = s.st_size;
	if (newsize) ret |= ANY_MAIL;
	if (s.st_mtime > s.st_atime && newsize) ret |= UNREAD_MAIL;
	if (newsize != oldsize && (ret & UNREAD_MAIL)) ret |= NEW_MAIL;
	oldsize = newsize;

	g_free(filename);

	return ret;
}

static void
destroy_cb()
{
	mail = NULL;
}

static gboolean
check_timeout(gpointer data)
{
	gint count = check_mail();
	GaimBuddyList *list = gaim_get_blist();

	if (count == -1)
		return FALSE;

	if (!list || !GAIM_IS_GTK_BLIST(list) || !(GAIM_GTK_BLIST(list)->vbox))
		return TRUE;

	if (!mail) {
		/* guess we better build it then :P */
		GtkWidget *vbox = GAIM_GTK_BLIST(list)->vbox;

		mail = gtk_label_new("No mail messages.");
		gtk_box_pack_start(GTK_BOX(vbox), mail, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(vbox), mail, 1);
		g_signal_connect(G_OBJECT(mail), "destroy", G_CALLBACK(destroy_cb), NULL);
		gtk_widget_show(mail);
	}

	if (count & NEW_MAIL)
		gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT, NULL);

	if (count & UNREAD_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have new mail!");
	else if (count & ANY_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have mail.");
	else
		gtk_label_set_text(GTK_LABEL(mail), "No mail messages.");

	return TRUE;
}

static void
signon_cb(GaimConnection *gc)
{
	GaimBuddyList *list = gaim_get_blist();
	if (list && GAIM_IS_GTK_BLIST(list) && !timer) {
		check_timeout(NULL); /* we want the box to be drawn immediately */
		timer = g_timeout_add(2000, check_timeout, NULL);
	}
}

static void
signoff_cb(GaimConnection *gc)
{
	GaimBuddyList *list = gaim_get_blist();
	if ((!list || !GAIM_IS_GTK_BLIST(list) || !GAIM_GTK_BLIST(list)->vbox) && timer) {
		g_source_remove(timer);
		timer = 0;
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GaimBuddyList *list = gaim_get_blist();
	void *conn_handle = gaim_connections_get_handle();

	if (!check_timeout(NULL)) {
		gaim_debug_warning("mailchk", "Could not read $MAIL or /var/spool/mail/$USER");
		return FALSE;
	}

	if (list && GAIM_IS_GTK_BLIST(list) && GAIM_GTK_BLIST(list)->vbox)
		timer = g_timeout_add(2000, check_timeout, NULL);

	gaim_signal_connect(conn_handle, "signed-on",
						plugin, GAIM_CALLBACK(signon_cb), NULL);
	gaim_signal_connect(conn_handle, "signed-off",
						plugin, GAIM_CALLBACK(signoff_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (timer)
		g_source_remove(timer);
	timer = 0;
	if (mail)
		gtk_widget_destroy(mail);
	mail = NULL;

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
	MAILCHK_PLUGIN_ID,
	N_("Mail Checker"),
	VERSION,
	N_("Checks for new local mail."),
	N_("Adds a small box to the buddy list that"
	   " shows if you have new mail."),
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
}

GAIM_INIT_PLUGIN(mailchk, init_plugin, info)

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

static guint timer = 0;
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
	PurpleBuddyList *list = purple_get_blist();

	if (count == -1)
		return FALSE;

	if (!list || !PURPLE_IS_GTK_BLIST(list) || !(PIDGIN_BLIST(list)->vbox))
		return TRUE;

	if (!mail) {
		/* guess we better build it then :P */
		GtkWidget *vbox = PIDGIN_BLIST(list)->vbox;

		mail = gtk_label_new("No mail messages.");
		gtk_box_pack_start(GTK_BOX(vbox), mail, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(vbox), mail, 1);
		g_signal_connect(G_OBJECT(mail), "destroy", G_CALLBACK(destroy_cb), NULL);
		gtk_widget_show(mail);
	}

	if (count & NEW_MAIL)
		purple_sound_play_event(PURPLE_SOUND_POUNCE_DEFAULT, NULL);

	if (count & UNREAD_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have new mail!");
	else if (count & ANY_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have mail.");
	else
		gtk_label_set_text(GTK_LABEL(mail), "No mail messages.");

	return TRUE;
}

static void
signon_cb(PurpleConnection *gc)
{
	PurpleBuddyList *list = purple_get_blist();
	if (list && PURPLE_IS_GTK_BLIST(list) && !timer) {
		check_timeout(NULL); /* we want the box to be drawn immediately */
		timer = purple_timeout_add_seconds(2, check_timeout, NULL);
	}
}

static void
signoff_cb(PurpleConnection *gc)
{
	PurpleBuddyList *list = purple_get_blist();
	if ((!list || !PURPLE_IS_GTK_BLIST(list) || !PIDGIN_BLIST(list)->vbox) && timer) {
		purple_timeout_remove(timer);
		timer = 0;
	}
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurpleBuddyList *list = purple_get_blist();
	void *conn_handle = purple_connections_get_handle();

	if (!check_timeout(NULL)) {
		purple_debug_warning("mailchk", "Could not read $MAIL or /var/spool/mail/$USER\n");
		return FALSE;
	}

	if (list && PURPLE_IS_GTK_BLIST(list) && PIDGIN_BLIST(list)->vbox)
		timer = purple_timeout_add_seconds(2, check_timeout, NULL);

	purple_signal_connect(conn_handle, "signed-on",
						plugin, PURPLE_CALLBACK(signon_cb), NULL);
	purple_signal_connect(conn_handle, "signed-off",
						plugin, PURPLE_CALLBACK(signoff_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	if (timer)
		purple_timeout_remove(timer);
	timer = 0;
	if (mail)
		gtk_widget_destroy(mail);
	mail = NULL;

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_PLUGIN_TYPE,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	MAILCHK_PLUGIN_ID,
	N_("Mail Checker"),
	DISPLAY_VERSION,
	N_("Checks for new local mail."),
	N_("Adds a small box to the buddy list that"
	   " shows if you have new mail."),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(mailchk, init_plugin, info)

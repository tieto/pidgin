#include "gaim.h"
#include "gtkplugin.h"
#include "blist.h"
#include "gtkblist.h"
#include "sound.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAILCHK_PLUGIN_ID "gtk-mailchk"

#define ANY_MAIL    0x01
#define UNREAD_MAIL 0x02
#define NEW_MAIL    0x04

static guint32 timer = 0;
static GtkWidget *mail = NULL;

static gint check_mail()
{
	static off_t oldsize = 0;
	gchar *filename;
	off_t newsize;
	struct stat s;
	gint ret = 0;

	filename = g_strdup(g_getenv("MAIL"));
	if (!filename)
		filename = g_strconcat("/var/spool/mail/", g_get_user_name(), NULL);

	if (stat(filename, &s) < 0) {
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

static void destroy_cb()
{
	mail = NULL;
}

static gboolean check_timeout(gpointer data)
{
	gint count = check_mail();
	struct gaim_buddy_list *list = gaim_get_blist();
	if (count == -1)
		return FALSE;

	if (!list || !GAIM_GTK_BLIST(list))
		return TRUE;

	if (!mail) {
		/* guess we better build it then :P */
		//GList *tmp = gtk_container_get_children(GTK_CONTAINER(GAIM_GTK_BLIST(list)));
		//GtkWidget *vbox2 = (GtkWidget *)tmp->data;
		GtkWidget *vbox = (GtkWidget *)(GAIM_GTK_BLIST(list)->vbox);

		mail = gtk_label_new("No mail messages.");
		gtk_box_pack_start(GTK_BOX(vbox), mail, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(vbox), mail, 1);
		g_signal_connect(G_OBJECT(mail), "destroy", G_CALLBACK(destroy_cb), NULL);
		gtk_widget_show(mail);
	}

	if (count & NEW_MAIL)
		gaim_sound_play_event(GAIM_SOUND_POUNCE_DEFAULT);

	if (count & UNREAD_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have new mail!");
	else if (count & ANY_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have mail.");
	else
		gtk_label_set_text(GTK_LABEL(mail), "No mail messages.");

	return TRUE;
}

static void signon_cb(struct gaim_connection *gc)
{
	struct gaim_buddy_list *list = gaim_get_blist();
	if (list && GAIM_GTK_BLIST(list) && !timer)
		timer = g_timeout_add(2000, check_timeout, NULL);
}

static void signoff_cb(struct gaim_connection *gc)
{
	struct gaim_buddy_list *list = gaim_get_blist();
	if ((!list || !GAIM_GTK_BLIST(list)) && timer) {
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
	struct gaim_buddy_list *list = gaim_get_blist();
	if (!check_timeout(NULL)) {
		gaim_debug(GAIM_DEBUG_WARNING, "mailchk", "Could not read $MAIL or /var/spool/mail/$USER");
		return FALSE;
	}

	if (list && GAIM_GTK_BLIST(list))
		timer = g_timeout_add(2000, check_timeout, NULL);

	gaim_signal_connect(plugin, event_signon, signon_cb, NULL);
	gaim_signal_connect(plugin, event_signoff, signoff_cb, NULL);

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
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	GAIM_GTK_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	MAILCHK_PLUGIN_ID,                                /**< id             */
	N_("Mail Checker"),                               /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Checks for new local mail."),
	                                                  /**  description    */
	N_("Checks for new local mail."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
__init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(mailchk, __init_plugin, info);

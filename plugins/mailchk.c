#include "config.h"
#include "sound.h"

#ifndef GAIM_PLUGINS
#define GAIM_PLUGINS
#endif

#include "gaim.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

	filename = g_getenv("MAIL");
	if (!filename)
		filename = g_strconcat("/var/spool/mail/", g_get_user_name(), NULL);
	else
		filename = g_strdup(filename);

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

static void maildes()
{
	mail = NULL;
}

static gboolean check_timeout(gpointer data)
{
	gint count = check_mail();

	if (count == -1)
		return FALSE;

	if (!blist)
		return TRUE;

	if (!mail) {
		/* guess we better build it then :P */
		GList *tmp = gtk_container_children(GTK_CONTAINER(blist));
		GtkWidget *vbox2 = (GtkWidget *)tmp->data;

		mail = gtk_label_new("No mail messages.");
		gtk_box_pack_start(GTK_BOX(vbox2), mail, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(vbox2), mail, 1);
		g_signal_connect(GTK_OBJECT(mail), "destroy", G_CALLBACK(maildes), NULL);
		gtk_widget_show(mail);
	}

	if (count & NEW_MAIL)
		gaim_sound_play(SND_POUNCE_DEFAULT);

	if (count & UNREAD_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have new mail!");
	else if (count & ANY_MAIL)
		gtk_label_set_text(GTK_LABEL(mail), "You have mail.");
	else
		gtk_label_set_text(GTK_LABEL(mail), "No mail messages.");

	return TRUE;
}

static void mail_signon(struct gaim_connection *gc)
{
	if (blist && !timer)
		timer = g_timeout_add(2000, check_timeout, NULL);
}

static void mail_signoff(struct gaim_connection *gc)
{
	if (!blist && timer) {
		g_source_remove(timer);
		timer = 0;
	}
}

char *gaim_plugin_init(GModule *m)
{
	if (!check_timeout(NULL))
		return "Could not read $MAIL or /var/spool/mail/$USER";
	if (blist)
		timer = g_timeout_add(2000, check_timeout, NULL);
	gaim_signal_connect(m, event_signon, mail_signon, NULL);
	gaim_signal_connect(m, event_signoff, mail_signoff, NULL);
	return NULL;
}

void gaim_plugin_remove()
{
	if (timer)
		g_source_remove(timer);
	timer = 0;
	if (mail)
		gtk_widget_destroy(mail);
	mail = NULL;
}

struct gaim_plugin_description desc; 
struct gaim_plugin_description *gaim_plugin_desc() {
	desc.api_version = PLUGIN_API_VERSION;
	desc.name = g_strdup("Mail Checker");
	desc.version = g_strdup(VERSION);
	desc.description = g_strdup("Checks for new local mail.");
	desc.authors = g_strdup("Eric Warmehoven &lt;eric@warmenhoven.org>");
	desc.url = g_strdup(WEBSITE);
	return &desc;
}

char *name()
{
	return "Mail Check";
}

char *description()
{
	return "Checks for new local mail";
}

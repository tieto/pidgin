#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "notify.h"
#include "version.h"

#define MUSICMESSAGIN_PLUGIN_ID "gtk-hazure-musicmessaging"

static gboolean start_session(void);
static void run_editor(void);
static void add_button (GaimConversation *conv);

typedef struct {
	GaimBuddy *buddy;
	GtkWidget *window;
	/* Something for the ly file? */
	
	/* Anything else needed for a session? */
	
} MMSession;

/* List of sessions */
GList *sessions;

/* Pointer to this plugin */
GaimPlugin *plugin_pointer;

static gboolean
plugin_load(GaimPlugin *plugin) {
    gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Welcome",
                        "Welcome to music messaging.", NULL, NULL, NULL);
	/* Keep the plugin for reference (needed for notify's) */
	plugin_pointer = plugin;
	
	/* Add the button to all the current conversations */
	gaim_conversation_foreach (add_button);
	
	/* Listen for any new conversations */
	void *conv_list_handle = gaim_conversations_get_handle();
	
	gaim_signal_connect(conv_list_handle, "conversation-created", 
					plugin, GAIM_CALLBACK(add_button), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin) {
	
	gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Unloaded",
						gaim_prefs_get_string ("/plugins/gtk/musicmessaging/editor_path"), NULL, NULL, NULL);
	
	
	return TRUE;
}

static gboolean
start_session(void)
{
	run_editor();
	return TRUE;
}

static void set_editor_path (GtkWidget *button, GtkWidget *text_field)
{
	const char * path = gtk_entry_get_text((GtkEntry*)text_field);
	gaim_prefs_set_string("/plugins/gtk/musicmessaging/editor_path", path);
	
	/*Testing*
	start_session();
	*/
	
}

static void run_editor (void)
{
	GError *spawn_error = NULL;
	gchar * args[2];
	args[0] = (gchar *)gaim_prefs_get_string("/plugins/gtk/musicmessaging/editor_path");
	args[1] = NULL;
	if (!(g_spawn_async (".", args, NULL, 0, NULL, NULL, NULL, &spawn_error)))
	{
		gaim_notify_error(plugin_pointer, "Error Running Editor",
						"The following error has occured:", spawn_error->message);
	}
}

static void add_button (GaimConversation *conv)
{
	GtkWidget *button, *image, *bbox;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(start_session), NULL);

	bbox = gtk_vbox_new(FALSE, 0);

	gtk_container_add (GTK_CONTAINER(button), bbox);

	gchar *file_path = g_build_filename (DATADIR, "pixmaps", "gaim", "buttons", "music.png", NULL);
	image = gtk_image_new_from_file(file_path);

	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 0);

	gtk_widget_show_all(bbox);

	gtk_box_pack_start(GTK_BOX(GAIM_GTK_CONVERSATION(conv)->toolbar), button, FALSE, FALSE, 0);
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	
	GtkWidget *editor_path;
	GtkWidget *editor_path_label;
	GtkWidget *editor_path_button;
	
	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

	/* Configuration frame */
	vbox = gaim_gtk_make_frame(ret, _("Music Messaging Configuration"));
	
	/* Path to the score editor */
	editor_path = gtk_entry_new();
	editor_path_label = gtk_label_new("Score Editor Path");
	editor_path_button = gtk_button_new_with_mnemonic(_("_Apply"));
	
	gtk_entry_set_text((GtkEntry*)editor_path, "/usr/local/bin/gscore");
	
	g_signal_connect(G_OBJECT(editor_path_button), "clicked",
					 G_CALLBACK(set_editor_path), editor_path);
					 
	gtk_box_pack_start(GTK_BOX(vbox), editor_path_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), editor_path, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), editor_path_button, FALSE, FALSE, 0);
	
	gtk_widget_show_all(ret);

	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,

    MUSICMESSAGIN_PLUGIN_ID,
    "Music Messaging",
    VERSION,
    "Music Messaging Plugin for collabrative composition.",
    "The Music Messaging Plugin allows a number of users to simultaniously work on a piece of music by editting a common score in real-time.",
    "Christian Muise <christian.muise@gmail.com>",
    GAIM_WEBSITE,
    plugin_load,
    plugin_unload,
    NULL,
    &ui_info,
    NULL,
    NULL,
    NULL
};

static void
init_plugin(GaimPlugin *plugin) {
	gaim_prefs_add_none("/plugins/gtk/musicmessaging");
	gaim_prefs_add_string("/plugins/gtk/musicmessaging/editor_path", "/usr/local/bin/gscore");
}

GAIM_INIT_PLUGIN(musicmessaging, init_plugin, info);

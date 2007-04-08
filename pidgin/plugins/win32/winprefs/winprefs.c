/*
 * purple - WinPurple Options Plugin
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#include "internal.h"

#include "gtkwin32dep.h"

#include "core.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

#include "gtkappbar.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkprefs.h"
#include "gtkutils.h"

/*
 *  MACROS & DEFINES
 */
#define WINPREFS_PLUGIN_ID "gtk-win-prefs"

#define RUNKEY "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"

/*
 *  LOCALS
 */
static const char *PREF_DBLIST_DOCKABLE = "/plugins/gtk/win32/winprefs/dblist_dockable";
static const char *PREF_DBLIST_DOCKED = "/plugins/gtk/win32/winprefs/dblist_docked";
static const char *PREF_DBLIST_HEIGHT = "/plugins/gtk/win32/winprefs/dblist_height";
static const char *PREF_DBLIST_SIDE = "/plugins/gtk/win32/winprefs/dblist_side";
static const char *PREF_BLIST_ON_TOP = "/plugins/gtk/win32/winprefs/blist_on_top";
static const char *PREF_CHAT_BLINK = "/plugins/gtk/win32/winprefs/chat_blink";

/* Deprecated */
static const char *PREF_DBLIST_ON_TOP = "/plugins/gtk/win32/winprefs/dblist_on_top";

static PurplePlugin *handle = NULL;
static GtkAppBar *blist_ab = NULL;
static GtkWidget *blist = NULL;
static guint blist_visible_cb_id = 0;

enum {
	BLIST_TOP_NEVER = 0,
	BLIST_TOP_ALWAYS,
	BLIST_TOP_DOCKED,
};

/*
 *  CODE
 */

/* BLIST DOCKING */

static void blist_save_state() {
	if(blist_ab) {
		if(purple_prefs_get_bool(PREF_DBLIST_DOCKABLE) && blist_ab->docked) {
			purple_prefs_set_int(PREF_DBLIST_HEIGHT, blist_ab->undocked_height);
			purple_prefs_set_int(PREF_DBLIST_SIDE, blist_ab->side);
			purple_prefs_set_bool(PREF_DBLIST_DOCKED, blist_ab->docked);
		} else
			purple_prefs_set_bool(PREF_DBLIST_DOCKED, FALSE);
	}
}

static void blist_set_ontop(gboolean val) {
	if(!blist)
		return;

	if(val)
		SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	else
		SetWindowPos(GDK_WINDOW_HWND(blist->window), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

}

static void blist_dock_cb(gboolean val) {
	if(val) {
		purple_debug_info(WINPREFS_PLUGIN_ID, "Blist Docking...\n");
		if(purple_prefs_get_int(PREF_BLIST_ON_TOP) != BLIST_TOP_NEVER)
			blist_set_ontop(TRUE);
	} else {
		purple_debug_info(WINPREFS_PLUGIN_ID, "Blist Undocking...\n");
		if(purple_prefs_get_int(PREF_BLIST_ON_TOP) == BLIST_TOP_ALWAYS)
			blist_set_ontop(TRUE);
		else
			blist_set_ontop(FALSE);
	}
}

static void blist_set_dockable(gboolean val) {
	if(val) {
		if(blist_ab == NULL && blist != NULL) {
			blist_ab = gtk_appbar_add(blist);
			gtk_appbar_add_dock_cb(blist_ab, blist_dock_cb);
		}
	} else {
		if(blist_ab != NULL) {
			gtk_appbar_remove(blist_ab);
			blist_ab = NULL;
		}

		if(purple_prefs_get_int(PREF_BLIST_ON_TOP) == BLIST_TOP_ALWAYS)
			blist_set_ontop(TRUE);
		else
			blist_set_ontop(FALSE);
	}
}

/* PLUGIN CALLBACKS */

/* We need this because the blist destroy cb won't be called before the
   plugin is unloaded, when quitting */
static void purple_quit_cb() {
	purple_debug_info(WINPREFS_PLUGIN_ID, "purple_quit_cb: removing appbar\n");
	blist_save_state();
	blist_set_dockable(FALSE);
}

/* Listen for the first time the window stops being withdrawn */
static void blist_visible_cb(const char *pref, PurplePrefType type,
		gconstpointer value, gpointer user_data) {
	if(purple_prefs_get_bool(pref)) {
		gtk_appbar_dock(blist_ab,
			purple_prefs_get_int(PREF_DBLIST_SIDE));

		if(purple_prefs_get_int(PREF_BLIST_ON_TOP)
				== BLIST_TOP_DOCKED)
			blist_set_ontop(TRUE);

		/* We only need to be notified about this once */
		purple_prefs_disconnect_callback(blist_visible_cb_id);
	}
}

/* This needs to be delayed otherwise, when the blist is originally created and
 * hidden, it'll trigger the blist_visible_cb */
static gboolean listen_for_blist_visible_cb(gpointer data) {
	if (handle != NULL)
		blist_visible_cb_id =
			purple_prefs_connect_callback(handle,
				"/purple/gtk/blist/list_visible",
				blist_visible_cb, NULL);

	return FALSE;
}

static void blist_create_cb(PurpleBuddyList *purple_blist, void *data) {
	purple_debug_info(WINPREFS_PLUGIN_ID, "buddy list created\n");

	blist = PIDGIN_BLIST(purple_blist)->window;

	if(purple_prefs_get_bool(PREF_DBLIST_DOCKABLE)) {
		blist_set_dockable(TRUE);
		if(purple_prefs_get_bool(PREF_DBLIST_DOCKED)) {
			blist_ab->undocked_height = purple_prefs_get_int(PREF_DBLIST_HEIGHT);
			if(!(gdk_window_get_state(blist->window)
					& GDK_WINDOW_STATE_WITHDRAWN)) {
				gtk_appbar_dock(blist_ab,
					purple_prefs_get_int(PREF_DBLIST_SIDE));
				if(purple_prefs_get_int(PREF_BLIST_ON_TOP)
						== BLIST_TOP_DOCKED)
					blist_set_ontop(TRUE);
			} else {
				g_idle_add(listen_for_blist_visible_cb, NULL);
			}
		}
	}

	if(purple_prefs_get_int(PREF_BLIST_ON_TOP) == BLIST_TOP_ALWAYS)
		blist_set_ontop(TRUE);

}

/* WIN PREFS GENERAL */

static void
winprefs_set_autostart(GtkWidget *w) {
	char *runval = NULL;

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		runval = g_strdup_printf("%s" G_DIR_SEPARATOR_S "purple.exe", wpurple_install_dir());

	if(!wpurple_write_reg_string(HKEY_CURRENT_USER, RUNKEY, "Purple", runval)
		/* For Win98 */
		&& !wpurple_write_reg_string(HKEY_LOCAL_MACHINE, RUNKEY, "Purple", runval))
			purple_debug_error(WINPREFS_PLUGIN_ID, "Could not set registry key value\n");

	g_free(runval);
}

static void
winprefs_set_blist_dockable(const char *pref, PurplePrefType type,
		gconstpointer value, gpointer user_data)
{
	blist_set_dockable(GPOINTER_TO_INT(value));
}

static void
winprefs_set_blist_ontop(const char *pref, PurplePrefType type,
		gconstpointer value, gpointer user_data)
{
	gint setting = purple_prefs_get_int(PREF_BLIST_ON_TOP);
	if((setting == BLIST_TOP_DOCKED && blist_ab && blist_ab->docked)
		|| setting == BLIST_TOP_ALWAYS)
		blist_set_ontop(TRUE);
	else
		blist_set_ontop(FALSE);
}

static gboolean
winpidgin_conv_chat_blink(PurpleAccount *account, const char *who, char **message,
		PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	if(purple_prefs_get_bool(PREF_CHAT_BLINK))
		winpidgin_conv_blink(conv, flags);

	return FALSE;
}


/*
 *  EXPORTED FUNCTIONS
 */

static gboolean plugin_load(PurplePlugin *plugin) {
	handle = plugin;

	/* blist docking init */
	if(purple_get_blist() && PIDGIN_BLIST(purple_get_blist())
			&& PIDGIN_BLIST(purple_get_blist())->window) {
		blist_create_cb(purple_get_blist(), NULL);
	}

	/* This really shouldn't happen anymore generally, but if for some strange
	   reason, the blist is recreated, we need to set it up again. */
	purple_signal_connect(pidgin_blist_get_handle(), "gtkblist-created",
		plugin, PURPLE_CALLBACK(blist_create_cb), NULL);

	purple_signal_connect(pidgin_conversations_get_handle(),
		"displaying-chat-msg", plugin, PURPLE_CALLBACK(winpidgin_conv_chat_blink),
		NULL);

	purple_signal_connect((void*)purple_get_core(), "quitting", plugin,
		PURPLE_CALLBACK(purple_quit_cb), NULL);

	purple_prefs_connect_callback(handle, PREF_BLIST_ON_TOP,
		winprefs_set_blist_ontop, NULL);
	purple_prefs_connect_callback(handle, PREF_DBLIST_DOCKABLE,
		winprefs_set_blist_dockable, NULL);

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
	blist_set_dockable(FALSE);
	blist_set_ontop(FALSE);

	handle = NULL;

	return TRUE;
}

static GtkWidget* get_config_frame(PurplePlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *button;
	char *gtk_version = NULL;
	char *run_key_val;
	char *tmp;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	gtk_version = g_strdup_printf("GTK+\t%u.%u.%u\nGlib\t%u.%u.%u",
		gtk_major_version, gtk_minor_version, gtk_micro_version,
		glib_major_version, glib_minor_version, glib_micro_version);

	/* Display Installed GTK+ Runtime Version */
	if(gtk_version) {
		GtkWidget *label;
		vbox = pidgin_make_frame(ret, _("GTK+ Runtime Version"));
		label = gtk_label_new(gtk_version);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_widget_show(label);
		g_free(gtk_version);
	}

	/* Autostart */
	vbox = pidgin_make_frame(ret, _("Startup"));
	tmp = g_strdup_printf(_("_Start %s on Windows startup"), PIDGIN_NAME);
	button = gtk_check_button_new_with_mnemonic(tmp);
	g_free(tmp);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	if ((run_key_val = wpurple_read_reg_string(HKEY_CURRENT_USER, RUNKEY, "Purple"))
			|| (run_key_val = wpurple_read_reg_string(HKEY_LOCAL_MACHINE, RUNKEY, "Purple"))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_free(run_key_val);
	}
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(winprefs_set_autostart), NULL);
	gtk_widget_show(button);

	/* Buddy List */
	vbox = pidgin_make_frame(ret, _("Buddy List"));
	pidgin_prefs_checkbox(_("_Dockable Buddy List"),
							PREF_DBLIST_DOCKABLE, vbox);

	/* Blist On Top */
	pidgin_prefs_dropdown(vbox, _("_Keep Buddy List window on top:"),
		PURPLE_PREF_INT, PREF_BLIST_ON_TOP,
		_("Never"), BLIST_TOP_NEVER,
		_("Always"), BLIST_TOP_ALWAYS,
		/* XXX: Did this ever work? */
		_("Only when docked"), BLIST_TOP_DOCKED,
		NULL);

	/* Conversations */
	vbox = pidgin_make_frame(ret, _("Conversations"));
	pidgin_prefs_checkbox(_("_Flash window when chat messages are received"),
							PREF_CHAT_BLINK, vbox);

	gtk_widget_show_all(ret);
	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0
};

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
	WINPREFS_PLUGIN_ID,
	N_("Pidgwin Options"),
	VERSION,
	N_("Options specific to Pidgin for Windows."),
	N_("Provides options specific to Pidgin for Windows , such as buddy list docking."),
	"Herman Bloggs <hermanator12002@yahoo.com>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/win32");
	purple_prefs_add_none("/plugins/gtk/win32/winprefs");
	purple_prefs_add_bool(PREF_DBLIST_DOCKABLE, FALSE);
	purple_prefs_add_bool(PREF_DBLIST_DOCKED, FALSE);
	purple_prefs_add_int(PREF_DBLIST_HEIGHT, 0);
	purple_prefs_add_int(PREF_DBLIST_SIDE, 0);
	purple_prefs_add_bool(PREF_CHAT_BLINK, FALSE);

	/* Convert old preferences */
	if(purple_prefs_exists(PREF_DBLIST_ON_TOP)) {
		gint blist_top = BLIST_TOP_NEVER;
		if(purple_prefs_get_bool(PREF_BLIST_ON_TOP))
			blist_top = BLIST_TOP_ALWAYS;
		else if(purple_prefs_get_bool(PREF_DBLIST_ON_TOP))
			blist_top = BLIST_TOP_DOCKED;
		purple_prefs_remove(PREF_BLIST_ON_TOP);
		purple_prefs_remove(PREF_DBLIST_ON_TOP);
		purple_prefs_add_int(PREF_BLIST_ON_TOP, blist_top);
	} else
		purple_prefs_add_int(PREF_BLIST_ON_TOP, BLIST_TOP_NEVER);
}

PURPLE_INIT_PLUGIN(winprefs, init_plugin, info)


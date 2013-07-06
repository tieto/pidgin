/*
 * pidgin - Windows Pidgin Options Plugin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "gtkwin32dep.h"

#include "core.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

#include "gtkappbar.h"
#include "gtkbuddylist.h"
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
/* Deprecated */
static const char *PREF_CHAT_BLINK = "/plugins/gtk/win32/winprefs/chat_blink";
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

	gtk_window_set_keep_above(GTK_WINDOW(PIDGIN_BLIST(purple_get_blist())->window), val);
}

static void blist_dock_cb(gboolean val) {
	if(val) {
		purple_debug_info(WINPREFS_PLUGIN_ID, "Blist Docking...\n");
		if(purple_prefs_get_int(PREF_BLIST_ON_TOP) != BLIST_TOP_NEVER)
			blist_set_ontop(TRUE);
	} else {
		purple_debug_info(WINPREFS_PLUGIN_ID, "Blist Undocking...\n");
		blist_set_ontop(purple_prefs_get_int(PREF_BLIST_ON_TOP) == BLIST_TOP_ALWAYS);
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

		blist_set_ontop(purple_prefs_get_int(PREF_BLIST_ON_TOP) == BLIST_TOP_ALWAYS);
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
				PIDGIN_PREFS_ROOT "/blist/list_visible",
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
		runval = g_strdup_printf("\"%s" G_DIR_SEPARATOR_S "pidgin.exe\"", wpurple_install_dir());

	if(!wpurple_write_reg_string(HKEY_CURRENT_USER, RUNKEY, "Pidgin", runval)
		/* For Win98 */
		&& !wpurple_write_reg_string(HKEY_LOCAL_MACHINE, RUNKEY, "Pidgin", runval))
			purple_debug_error(WINPREFS_PLUGIN_ID, "Could not set registry key value\n");

	g_free(runval);
}

static void
winprefs_set_multiple_instances(GtkWidget *w) {
	wpurple_write_reg_string(HKEY_CURRENT_USER, "Environment", "PIDGIN_MULTI_INST",
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)) ? "1" : NULL);
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
	char *run_key_val;
	char *tmp;

	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	/* Autostart */
	vbox = pidgin_make_frame(ret, _("Startup"));
	tmp = g_strdup_printf(_("_Start %s on Windows startup"), PIDGIN_NAME);
	button = gtk_check_button_new_with_mnemonic(tmp);
	g_free(tmp);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	if ((run_key_val = wpurple_read_reg_string(HKEY_CURRENT_USER, RUNKEY, "Pidgin"))
			|| (run_key_val = wpurple_read_reg_string(HKEY_LOCAL_MACHINE, RUNKEY, "Pidgin"))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_free(run_key_val);
	}
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(winprefs_set_autostart), NULL);
	gtk_widget_show(button);

	button = gtk_check_button_new_with_mnemonic(_("Allow multiple instances"));
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
	if ((run_key_val = wpurple_read_reg_string(HKEY_CURRENT_USER, "Environment", "PIDGIN_MULTI_INST"))) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_free(run_key_val);
	}
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(winprefs_set_multiple_instances), NULL);
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

	gtk_widget_show_all(ret);
	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
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
	N_("Windows Pidgin Options"),
	DISPLAY_VERSION,
	N_("Options specific to Pidgin for Windows."),
	N_("Provides options specific to Pidgin for Windows, such as buddy list docking."),
	"Herman Bloggs <hermanator12002@yahoo.com>",
	PURPLE_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	&ui_info,
	NULL,
	NULL,
	NULL,

	/* padding */
	NULL,
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
	purple_prefs_remove(PREF_CHAT_BLINK);
}

PURPLE_INIT_PLUGIN(winprefs, init_plugin, info)


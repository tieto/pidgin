/***************************************************************************
 *            vibes.c
 *
 *  Thu Oct 30 23:32:34 2008
 *  Copyright  2008  marcus
 *  <ml@update.uu.se>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "internal.h"

#include "version.h"

#include "pidgin.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "debug.h"

static void
move_window_sync(GtkWindow *win, guint x, guint y)
{
	GdkDisplay *disp = gdk_display_get_default();
	gtk_window_move(win, x, y);
	/* synchronize display */
	gdk_error_trap_pop();
	gdk_display_sync(disp);
}

static void
vibrate_conv_window(PurpleConversation *conv)
{
	PidginConversation *pconv = PIDGIN_CONVERSATION(conv);
	PidginWindow *pwin = pidgin_conv_get_window(pconv);
	GtkWindow *win = pwin->window;
	int x, y, i;

	gtk_window_get_position(win, &x, &y);
	
	for (i = 0 ; i < 20 ; i++) {
		move_window_sync(win, x, y + 10);
		usleep(10);
		move_window_sync(win, x, y - 10);
		usleep(10);
		move_window_sync(win, x - 10, y);
		usleep(10);
		move_window_sync(win, x, y);
		usleep(10);
	}
}

static void
sent_attention_cb(PurpleAccount *account, const char *who, 
	PurpleConversation *conv, guint type)
{
	purple_debug_info("vibes", "sent_attention_cb called\n");
	vibrate_conv_window(conv);
}

static void
got_attention_cb(PurpleAccount *account, const char *who, 
	PurpleConversation *conv, guint type)
{
	purple_debug_info("vibes", "got_attention_cb called\n");
	vibrate_conv_window(conv);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(),
					"sent-attention", plugin,
					PURPLE_CALLBACK(sent_attention_cb), NULL);
	purple_signal_connect(purple_conversations_get_handle(),
					"got-attention", plugin,
					PURPLE_CALLBACK(got_attention_cb), NULL);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_signal_disconnect(purple_conversations_get_handle(),
		"sent-attention", plugin, PURPLE_CALLBACK(sent_attention_cb));
	purple_signal_disconnect(purple_conversations_get_handle(),
		"got-attention", plugin, PURPLE_CALLBACK(got_attention_cb));
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,                           /**< major version */
	PURPLE_MINOR_VERSION,                           /**< minor version */
	PURPLE_PLUGIN_STANDARD,                         /**< type */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                              /**< flags */
	NULL,                                           /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,                        /**< priority */

	"gtk-vibes",                                /**< id */
	N_("Vibes"),                              /**< name */
	DISPLAY_VERSION,                                /**< version */
	N_("Shake the Conversation Window on Attentions."),         /**< summary */
	N_("Enables shaking of the conversation window "
	   "when sending and receiving attentions "
	   "(buzz, nudge etc."),         /**< description */
	"Marcus Lundblad <ml@update.uu.se>",              /**< author */
	PURPLE_WEBSITE,                                 /**< homepage */
	plugin_load,                                    /**< load */
	plugin_unload,                                  /**< unload */
	NULL,                                           /**< destroy */
	NULL,                                           /**< ui_info */
	NULL,                                           /**< extra_info */
	NULL,                                           /**< prefs_info */
	NULL,                                           /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	/* register prefs */
	
}

PURPLE_INIT_PLUGIN(sendbutton, init_plugin, info)

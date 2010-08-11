/*
 * Mouse gestures plugin for Purple
 *
 * Copyright (C) 2003 Christian Hammond.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */
#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "gstroke.h"

#define GESTURES_PLUGIN_ID "gtk-x11-gestures"

static void
stroke_close(GtkWidget *widget, void *data)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;

	conv = (PurpleConversation *)data;

	/* Double-check */
	if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		return;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gstroke_cleanup(gtkconv->imhtml);
	purple_conversation_destroy(conv);
}

static void
switch_page(PidginWindow *win, GtkDirectionType dir)
{
	int count, current;

#if GTK_CHECK_VERSION(2,2,0)
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
#else
	count = g_list_length(GTK_NOTEBOOK(win->notebook)->children);
#endif
	current = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));

	if (dir == GTK_DIR_LEFT)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), current - 1);
	}
	else if (dir == GTK_DIR_RIGHT)
	{
		if (current == count - 1)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
		else
			gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), current + 1);
	}
}

static void
stroke_prev_tab(GtkWidget *widget, void *data)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	PidginWindow *win;

	conv  = (PurpleConversation *)data;
	gtkconv = PIDGIN_CONVERSATION(conv);
	win   = gtkconv->win;

	switch_page(win, GTK_DIR_LEFT);
}

static void
stroke_next_tab(GtkWidget *widget, void *data)
{
	PurpleConversation *conv;
	PidginWindow *win;

	conv  = (PurpleConversation *)data;
	win   = PIDGIN_CONVERSATION(conv)->win;

	switch_page(win, GTK_DIR_RIGHT);
}

static void
stroke_new_win(GtkWidget *widget, void *data)
{
	PidginWindow *new_win, *old_win;
	PurpleConversation *conv;

	conv    = (PurpleConversation *)data;
	old_win = PIDGIN_CONVERSATION(conv)->win;

	if (pidgin_conv_window_get_gtkconv_count(old_win) <= 1)
		return;

	new_win = pidgin_conv_window_new();

	pidgin_conv_window_remove_gtkconv(old_win, PIDGIN_CONVERSATION(conv));
	pidgin_conv_window_add_gtkconv(new_win, PIDGIN_CONVERSATION(conv));

	pidgin_conv_window_show(new_win);
}

static void
attach_signals(PurpleConversation *conv)
{
	PidginConversation *gtkconv;

	gtkconv = PIDGIN_CONVERSATION(conv);

	gstroke_enable(gtkconv->imhtml);
	gstroke_signal_connect(gtkconv->imhtml, "14789",  stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "1456",   stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "1489",   stroke_close,    conv);
	gstroke_signal_connect(gtkconv->imhtml, "74123",  stroke_next_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "7456",   stroke_next_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "96321",  stroke_prev_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "9654",   stroke_prev_tab, conv);
	gstroke_signal_connect(gtkconv->imhtml, "25852",  stroke_new_win,  conv);
}

static void
new_conv_cb(PurpleConversation *conv)
{
	if (PIDGIN_IS_PIDGIN_CONVERSATION(conv))
		attach_signals(conv);
}

#if 0
static void
mouse_button_menu_cb(GtkMenuItem *item, gpointer data)
{
	int button = (int)data;

	gstroke_set_mouse_button(button + 2);
}
#endif

static void
toggle_draw_cb(GtkToggleButton *toggle, gpointer data)
{
	purple_prefs_set_bool("/plugins/gtk/X11/gestures/visual",
		gtk_toggle_button_get_active(toggle));
}

static void
visual_pref_cb(const char *name, PurplePrefType type, gconstpointer value,
			   gpointer data)
{
	gstroke_set_draw_strokes((gboolean) GPOINTER_TO_INT(value) );
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	PurpleConversation *conv;
	GList *l;

	for (l = purple_get_conversations(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		attach_signals(conv);
	}

	purple_signal_connect(purple_conversations_get_handle(),
						"conversation-created",
						plugin, PURPLE_CALLBACK(new_conv_cb), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	GList *l;

	for (l = purple_get_conversations(); l != NULL; l = l->next) {
		conv = (PurpleConversation *)l->data;

		if (!PIDGIN_IS_PIDGIN_CONVERSATION(conv))
			continue;

		gtkconv = PIDGIN_CONVERSATION(conv);

		gstroke_cleanup(gtkconv->imhtml);
		gstroke_disable(gtkconv->imhtml);
	}

	return TRUE;
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	GtkWidget *toggle;
#if 0
	GtkWidget *opt;
	GtkWidget *menu, *item;
#endif

	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	/* Configuration frame */
	vbox = pidgin_make_frame(ret, _("Mouse Gestures Configuration"));

#if 0
	/* Mouse button drop-down menu */
	menu = gtk_menu_new();
	opt = gtk_option_menu_new();

	item = gtk_menu_item_new_with_label(_("Middle mouse button"));
	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(mouse_button_menu_cb), opt);
	gtk_menu_append(menu, item);

	item = gtk_menu_item_new_with_label(_("Right mouse button"));
	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(mouse_button_menu_cb), opt);
	gtk_menu_append(menu, item);

	gtk_box_pack_start(GTK_BOX(vbox), opt, FALSE, FALSE, 0);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(opt),
								gstroke_get_mouse_button() - 2);
#endif

	/* "Visual gesture display" checkbox */
	toggle = gtk_check_button_new_with_mnemonic(_("_Visual gesture display"));
	gtk_box_pack_start(GTK_BOX(vbox), toggle, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
			purple_prefs_get_bool("/plugins/gtk/X11/gestures/visual"));
	g_signal_connect(G_OBJECT(toggle), "toggled",
					 G_CALLBACK(toggle_draw_cb), NULL);

	gtk_widget_show_all(ret);

	return ret;
}

static PidginPluginUiInfo ui_info =
{
	get_config_frame,
	0, /* page_num (Reserved) */

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
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	PIDGIN_PLUGIN_TYPE,                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	GESTURES_PLUGIN_ID,                               /**< id             */
	N_("Mouse Gestures"),                             /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Provides support for mouse gestures"),
	                                                  /**  description    */
	N_("Allows support for mouse gestures in conversation windows.\n"
	   "Drag the middle mouse button to perform certain actions:\n\n"
	   "Drag down and then to the right to close a conversation.\n"
	   "Drag up and then to the left to switch to the previous "
	   "conversation.\n"
	   "Drag up and then to the right to switch to the next "
	   "conversation."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	&ui_info,                                         /**< ui_info        */
	NULL,                                             /**< extra_info     */
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
	purple_prefs_add_none("/plugins/gtk/X11");
	purple_prefs_add_none("/plugins/gtk/X11/gestures");
	purple_prefs_add_bool("/plugins/gtk/X11/gestures/visual", FALSE);

	purple_prefs_connect_callback(plugin, "/plugins/gtk/X11/gestures/visual",
								visual_pref_cb, NULL);
}

PURPLE_INIT_PLUGIN(gestures, init_plugin, info)

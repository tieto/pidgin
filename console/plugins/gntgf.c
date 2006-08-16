/**
 * @file gntgf.c Minimal toaster plugin in Gnt.
 *
 * Copyright (C) 2006 Sadrul Habib Chowdhury <sadrul@users.sourceforge.net>
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
 */

#define GAIM_PLUGINS

#define PLUGIN_STATIC_NAME	"GntGf"

#define PREFS_EVENT           "/plugins/gnt/gntgf/events"
#define PREFS_EVENT_SIGNONF   PREFS_EVENT "/signonf"
#define PREFS_EVENT_IM_MSG    PREFS_EVENT "/immsg"
#define PREFS_EVENT_CHAT_MSG  PREFS_EVENT "/chatmsg"
#define PREFS_EVENT_CHAT_NICK PREFS_EVENT "/chatnick"

#include <glib.h>

#include <plugin.h>
#include <version.h>
#include <blist.h>
#include <conversation.h>

#include <gnt.h>
#include <gntbox.h>
#include <gntbutton.h>
#include <gntlabel.h>
#include <gnttree.h>

#include <gntplugin.h>

#define _(X) X

typedef struct
{
	GntWidget *window;
	int timer;
} GntToast;

static GList *toasters;
static int gpsy;

static void
destroy_toaster(GntToast *toast)
{
	toasters = g_list_remove(toasters, toast);
	gnt_widget_destroy(toast->window);
	g_source_remove(toast->timer);
	g_free(toast);
}

static gboolean
remove_toaster(GntToast *toast)
{
	GList *iter;
	int h;

	gnt_widget_get_size(toast->window, NULL, &h);
	gpsy -= h;

	destroy_toaster(toast);

	for (iter = toasters; iter; iter = iter->next)
	{
		int x, y;
		toast = iter->data;
		gnt_widget_get_position(toast->window, &x, &y);
		y += h;
		gnt_screen_move_widget(toast->window, x, y);
	}

	return FALSE;
}

static void
notify(const char *fmt, ...)
{
	GntWidget *window;
	GntToast *toast = g_new0(GntToast, 1);
	char *str;
	int h, w;
	va_list args;

	toast->window = window = gnt_vbox_new(FALSE);
	GNT_WIDGET_SET_FLAGS(window, GNT_WIDGET_TRANSIENT);
	GNT_WIDGET_UNSET_FLAGS(window, GNT_WIDGET_NO_BORDER);

	va_start(args, fmt);
	str = g_strdup_vprintf(fmt, args);
	va_end(args);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new_with_format(str, GNT_TEXT_FLAG_HIGHLIGHT));

	g_free(str);
	gnt_widget_size_request(window);
	gnt_widget_get_size(window, &w, &h);
	gpsy += h;
	gnt_widget_set_position(window, getmaxx(stdscr) - w - 1,
			getmaxy(stdscr) - gpsy - 1);
	gnt_widget_draw(window);

	toast->timer = g_timeout_add(4000, (GSourceFunc)remove_toaster, toast);
	toasters = g_list_prepend(toasters, toast);
}

static void
buddy_signed_on(GaimBuddy *buddy, gpointer null)
{
	if (gaim_prefs_get_bool(PREFS_EVENT_SIGNONF))
		notify(_("%s just signed on"), gaim_buddy_get_alias(buddy));
}

static void
buddy_signed_off(GaimBuddy *buddy, gpointer null)
{
	if (gaim_prefs_get_bool(PREFS_EVENT_SIGNONF))
		notify(_("%s just signed off"), gaim_buddy_get_alias(buddy));
}

static void
received_im_msg(GaimAccount *account, const char *sender, const char *msg,
		GaimConversation *conv, GaimMessageFlags flags, gpointer null)
{
	if (gaim_prefs_get_bool(PREFS_EVENT_IM_MSG))
		notify(_("%s sent you a message"), sender);
}

static void
received_chat_msg(GaimAccount *account, const char *sender, const char *msg,
		GaimConversation *conv, GaimMessageFlags flags, gpointer null)
{
	if (gaim_prefs_get_bool(PREFS_EVENT_CHAT_NICK) && (flags & GAIM_MESSAGE_NICK))
		notify(_("%s said your nick in %s"), sender, gaim_conversation_get_name(conv));
	else if (gaim_prefs_get_bool(PREFS_EVENT_CHAT_MSG))
		notify(_("%s sent a message in %s"), sender, gaim_conversation_get_name(conv));
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on", plugin,
			GAIM_CALLBACK(buddy_signed_on), NULL);
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-off", plugin,
			GAIM_CALLBACK(buddy_signed_off), NULL);
	gaim_signal_connect(gaim_conversations_get_handle(), "received-im-msg", plugin,
			GAIM_CALLBACK(received_im_msg), NULL);
	gaim_signal_connect(gaim_conversations_get_handle(), "received-chat-msg", plugin,
			GAIM_CALLBACK(received_chat_msg), NULL);

	gpsy = 0;

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	while (toasters)
	{
		GntToast *toast = toasters->data;
		destroy_toaster(toast);
	}
	return TRUE;
}

static struct
{
	char *pref;
	char *display;
} prefs[] =
{
	{PREFS_EVENT_SIGNONF, _("Buddy signs on/off")},
	{PREFS_EVENT_IM_MSG, _("You receive an IMs")},
	{PREFS_EVENT_CHAT_MSG, _("Someone speaks in a chat")},
	{PREFS_EVENT_CHAT_NICK, _("Someone says your name in a chat")},
	{NULL, NULL}
};

static void
pref_toggled(GntTree *tree, char *key, gpointer null)
{
	gaim_prefs_set_bool(key, gnt_tree_get_choice(tree, key));
}

static GntWidget *
config_frame()
{
	GntWidget *window, *tree;
	int i;

	window = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(window), 0);
	gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_MID);
	gnt_box_set_fill(GNT_BOX(window), TRUE);

	gnt_box_add_widget(GNT_BOX(window),
			gnt_label_new(_("Notify with a toaster when")));

	tree = gnt_tree_new();
	gnt_box_add_widget(GNT_BOX(window), tree);

	for (i = 0; prefs[i].pref; i++)
	{
		gnt_tree_add_choice(GNT_TREE(tree), prefs[i].pref,
				gnt_tree_create_row(GNT_TREE(tree), prefs[i].display), NULL, NULL);
		gnt_tree_set_choice(GNT_TREE(tree), prefs[i].pref,
				gaim_prefs_get_bool(prefs[i].pref));
	}
	gnt_tree_set_col_width(GNT_TREE(tree), 0, 40);
	g_signal_connect(G_OBJECT(tree), "toggled", G_CALLBACK(pref_toggled), NULL);

	return window;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,
	GAIM_GNT_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	"gntgf",
	N_("GntGf"),
	VERSION,
	N_("Toaster plugin for GntGaim."),
	N_("Toaster plugin for GntGaim."),
	"Sadrul H Chowdhury <sadrul@users.sourceforge.net>",
	"http://gaim.sourceforge.net",
	plugin_load,
	plugin_unload,
	NULL,
	config_frame,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins");
	gaim_prefs_add_none("/plugins/gnt");
	
	gaim_prefs_add_none("/plugins/gnt/gntgf");
	gaim_prefs_add_none(PREFS_EVENT);

	gaim_prefs_add_bool(PREFS_EVENT_SIGNONF, TRUE);
	gaim_prefs_add_bool(PREFS_EVENT_IM_MSG, TRUE);
	gaim_prefs_add_bool(PREFS_EVENT_CHAT_MSG, TRUE);
	gaim_prefs_add_bool(PREFS_EVENT_CHAT_NICK, TRUE);
}

GAIM_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)

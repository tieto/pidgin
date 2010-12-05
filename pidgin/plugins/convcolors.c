/*
 * Conversation Colors
 * Copyright (C) 2006
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

#define PLUGIN_ID			"gtk-plugin_pack-convcolors"
#define PLUGIN_NAME			N_("Conversation Colors")
#define PLUGIN_STATIC_NAME	ConversationColors
#define PLUGIN_SUMMARY		N_("Customize colors in the conversation window")
#define PLUGIN_DESCRIPTION	N_("Customize colors in the conversation window")
#define PLUGIN_AUTHOR		"Sadrul H Chowdhury <sadrul@users.sourceforge.net>"

/* System headers */
#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>

/* Purple headers */
#include <gtkplugin.h>
#include <version.h>

#include <conversation.h>
#include <gtkconv.h>
#include <gtkprefs.h>
#include <gtkutils.h>

#define	PREF_PREFIX	"/plugins/gtk/" PLUGIN_ID
#define PREF_IGNORE	PREF_PREFIX "/ignore_incoming"
#define PREF_CHATS	PREF_PREFIX "/chats"
#define PREF_IMS	PREF_PREFIX "/ims"

#define	PREF_SEND	PREF_PREFIX "/send"
#define	PREF_SEND_C	PREF_SEND "/color"
#define	PREF_SEND_F	PREF_SEND "/format"
#define	PREF_SEND_E	PREF_SEND "/enabled"

#define PREF_RECV	PREF_PREFIX "/recv"
#define	PREF_RECV_C	PREF_RECV "/color"
#define	PREF_RECV_F	PREF_RECV "/format"
#define	PREF_RECV_E	PREF_RECV "/enabled"

#define PREF_SYSTEM	PREF_PREFIX "/system"
#define	PREF_SYSTEM_C	PREF_SYSTEM "/color"
#define	PREF_SYSTEM_F	PREF_SYSTEM "/format"
#define	PREF_SYSTEM_E	PREF_SYSTEM "/enabled"

#define PREF_ERROR	PREF_PREFIX "/error"
#define	PREF_ERROR_C	PREF_ERROR "/color"
#define	PREF_ERROR_F	PREF_ERROR "/format"
#define	PREF_ERROR_E	PREF_ERROR "/enabled"

#define PREF_NICK	PREF_PREFIX "/nick"
#define	PREF_NICK_C	PREF_NICK "/color"
#define	PREF_NICK_F	PREF_NICK "/format"
#define	PREF_NICK_E	PREF_NICK "/enabled"

enum
{
	FONT_BOLD		= 1 << 0,
	FONT_ITALIC		= 1 << 1,
	FONT_UNDERLINE	= 1 << 2
};

static struct
{
	PurpleMessageFlags flag;
	char *prefix;
	const char *text;
} formats[] = 
{
	{PURPLE_MESSAGE_ERROR, PREF_ERROR, N_("Error Messages")},
	{PURPLE_MESSAGE_NICK, PREF_NICK, N_("Highlighted Messages")},
	{PURPLE_MESSAGE_SYSTEM, PREF_SYSTEM, N_("System Messages")},
	{PURPLE_MESSAGE_SEND, PREF_SEND, N_("Sent Messages")},
	{PURPLE_MESSAGE_RECV, PREF_RECV, N_("Received Messages")},
	{0, NULL, NULL}
};

static gboolean
displaying_msg(PurpleAccount *account, const char *who, char **displaying,
				PurpleConversation *conv, PurpleMessageFlags flags)
{
	int i;
	char tmp[128], *t;
	gboolean bold, italic, underline;
	int f;
	const char *color;
	gboolean rtl = FALSE;

	for (i = 0; formats[i].prefix; i++)
		if (flags & formats[i].flag)
			break;

	if (!formats[i].prefix)
		return FALSE;

	g_snprintf(tmp, sizeof(tmp), "%s/enabled", formats[i].prefix);

	if (!purple_prefs_get_bool(tmp) ||
			(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM &&
			!purple_prefs_get_bool(PREF_IMS)) ||
			(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT &&
			!purple_prefs_get_bool(PREF_CHATS)))
		return FALSE;

	g_snprintf(tmp, sizeof(tmp), "%s/color", formats[i].prefix);
	color = purple_prefs_get_string(tmp);

	g_snprintf(tmp, sizeof(tmp), "%s/format", formats[i].prefix);
	f = purple_prefs_get_int(tmp);
	bold = (f & FONT_BOLD);
	italic = (f & FONT_ITALIC);
	underline = (f & FONT_UNDERLINE);
	rtl = purple_markup_is_rtl(*displaying);

	if (purple_prefs_get_bool(PREF_IGNORE))
	{
		/* This seems to be necessary, especially for received messages. */
		t = *displaying;
		*displaying = purple_strreplace(t, "\n", "<br>");
		g_free(t);

		t = *displaying;
		*displaying = purple_markup_strip_html(t);
		g_free(t);

		t = *displaying;
		*displaying = g_markup_escape_text(t, -1);
		g_free(t);

		/* Restore the links */
		t = *displaying;
		*displaying = purple_markup_linkify(t);
		g_free(t);
	}

	if (color && *color)
	{
		t = *displaying;
		*displaying = g_strdup_printf("<FONT COLOR=\"%s\">%s</FONT>", color, t);
		g_free(t);
	}

	t = *displaying;
	*displaying = g_strdup_printf("%s%s%s%s%s%s%s%s%s",
						bold ? "<B>" : "</B>",
						italic ? "<I>" : "</I>",
						underline ? "<U>" : "</U>",
						rtl ? "<SPAN style=\"direction:rtl;text-align:right;\">" : "",
						t,
						rtl ? "</SPAN>" : "",
						bold ? "</B>" : "<B>",
						italic ? "</I>" : "<I>",
						underline ? "</U>" : "<U>"
			);
	g_free(t);

	return FALSE;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(pidgin_conversations_get_handle(),
					"displaying-im-msg", plugin,
					PURPLE_CALLBACK(displaying_msg), NULL);
	purple_signal_connect(pidgin_conversations_get_handle(),
					"displaying-chat-msg", plugin,
					PURPLE_CALLBACK(displaying_msg), NULL);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

/* Ripped from PurpleRC */
static void
color_response(GtkDialog *color_dialog, gint response, const char *data)
{
	if (response == GTK_RESPONSE_OK)
	{
#if GTK_CHECK_VERSION(2,14,0)
		GtkWidget *colorsel =
			gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(color_dialog));
#else
		GtkWidget *colorsel = GTK_COLOR_SELECTION_DIALOG(color_dialog)->colorsel;
#endif
		GdkColor color;
		char colorstr[8];
		char tmp[128];

		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(colorsel), &color);

		g_snprintf(colorstr, sizeof(colorstr), "#%02X%02X%02X",
		           color.red/256, color.green/256, color.blue/256);

		g_snprintf(tmp, sizeof(tmp), "%s/color", data);

		purple_prefs_set_string(tmp, colorstr);
	}

	gtk_widget_destroy(GTK_WIDGET(color_dialog));
}

static void
set_color(GtkWidget *widget, const char *data)
{
	GtkWidget *color_dialog = NULL;
	GdkColor color;
	char title[128];
	char tmp[128];

	g_snprintf(title, sizeof(title), _("Select Color for %s"), _(data));
	color_dialog = gtk_color_selection_dialog_new(title);
	g_signal_connect(G_OBJECT(color_dialog), "response",
	                 G_CALLBACK(color_response), (gpointer)data);

	g_snprintf(tmp, sizeof(tmp), "%s/color", data);
	if (gdk_color_parse(purple_prefs_get_string(tmp), &color))
	{
#if GTK_CHECK_VERSION(2,14,0)
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(
			gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(color_dialog))),
			&color);
#else
		gtk_color_selection_set_current_color(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(color_dialog)->colorsel),
			&color);
#endif
	}

	gtk_window_present(GTK_WINDOW(color_dialog));
}

static void
toggle_enabled(GtkWidget *widget, gpointer data)
{
	const char *prefix = (char *)data;
	gboolean e;
	char tmp[128];

	g_snprintf(tmp, sizeof(tmp), "%s/enabled", prefix);
	e = purple_prefs_get_bool(tmp);
	purple_prefs_set_bool(tmp, !e);
}

static void
toggle_something(const char *prefix, int format)
{
	int f;
	char tmp[128];

	g_snprintf(tmp, sizeof(tmp), "%s/format", prefix);
	f = purple_prefs_get_int(tmp);
	f ^= format;
	purple_prefs_set_int(tmp, f);
}

static void
toggle_bold(GtkWidget *widget, gpointer data)
{
	toggle_something(data, FONT_BOLD);
}

static void
toggle_italic(GtkWidget *widget, gpointer data)
{
	toggle_something(data, FONT_ITALIC);
}

static void
toggle_underline(GtkWidget *widget, gpointer data)
{
	toggle_something(data, FONT_UNDERLINE);
}

static void
enable_toggled(const char *name, PurplePrefType type, gconstpointer val, gpointer data)
{
	GtkWidget *widget = (GtkWidget *)data;

	gtk_widget_set_sensitive(widget, GPOINTER_TO_INT(val));
}

static void
disconnect_prefs_callbacks(GtkWidget *widget, gpointer data)
{
	PurplePlugin *plugin = (PurplePlugin *)data;

	purple_prefs_disconnect_by_handle(plugin);
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *frame;
	int i;

	ret = gtk_vbox_new(FALSE, PIDGIN_HIG_CAT_SPACE);
	gtk_container_set_border_width(GTK_CONTAINER(ret), PIDGIN_HIG_BORDER);

	for (i = 0; formats[i].prefix; i++)
	{
		char tmp[128];
		char tmp2[128];
		int f;
		gboolean e;
		GtkWidget *vbox, *hbox, *button;

		g_snprintf(tmp2, sizeof(tmp2), "%s/enabled", formats[i].prefix);
		e = purple_prefs_get_bool(tmp2);

		g_snprintf(tmp, sizeof(tmp), "%s/format", formats[i].prefix);
		f = purple_prefs_get_int(tmp);

		frame = pidgin_make_frame(ret, _(formats[i].text));
		vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(frame), vbox, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

		button = gtk_check_button_new_with_label(_("Enabled"));
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		if (e)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(toggle_enabled),
				formats[i].prefix);

		button = pidgin_pixbuf_button_from_stock(" Color", GTK_STOCK_SELECT_COLOR,
				PIDGIN_BUTTON_HORIZONTAL);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(set_color),
				formats[i].prefix);
		gtk_widget_set_sensitive(button, e);
		purple_prefs_connect_callback(plugin, tmp2, enable_toggled, button);

		button = gtk_check_button_new_with_label(_("Bold"));
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		if (f & FONT_BOLD)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(toggle_bold),
				formats[i].prefix);
		gtk_widget_set_sensitive(button, e);
		purple_prefs_connect_callback(plugin, tmp2, enable_toggled, button);

		button = gtk_check_button_new_with_label(_("Italic"));
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		if (f & FONT_ITALIC)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(toggle_italic),
				formats[i].prefix);
		gtk_widget_set_sensitive(button, e);
		purple_prefs_connect_callback(plugin, tmp2, enable_toggled, button);

		button = gtk_check_button_new_with_label(_("Underline"));
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		if (f & FONT_UNDERLINE)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
		g_signal_connect(G_OBJECT(button), "clicked",
				G_CALLBACK(toggle_underline), formats[i].prefix);
		gtk_widget_set_sensitive(button, e);
		purple_prefs_connect_callback(plugin, tmp2, enable_toggled, button);
	}

	g_signal_connect(ret, "destroy", G_CALLBACK(disconnect_prefs_callbacks), plugin);
	frame = pidgin_make_frame(ret, _("General"));
	pidgin_prefs_checkbox(_("Ignore incoming format"), PREF_IGNORE, frame);
	pidgin_prefs_checkbox(_("Apply in Chats"), PREF_CHATS, frame);
	pidgin_prefs_checkbox(_("Apply in IMs"), PREF_IMS, frame);

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
	PURPLE_PLUGIN_MAGIC,            /* Magic              */
	PURPLE_MAJOR_VERSION,           /* Purple Major Version */
	PURPLE_MINOR_VERSION,           /* Purple Minor Version */
	PURPLE_PLUGIN_STANDARD,         /* plugin type        */
	PIDGIN_PLUGIN_TYPE,         /* ui requirement     */
	0,                            /* flags              */
	NULL,                         /* dependencies       */
	PURPLE_PRIORITY_DEFAULT,        /* priority           */

	PLUGIN_ID,                    /* plugin id          */
	PLUGIN_NAME,                  /* name               */
	DISPLAY_VERSION,              /* version            */
	PLUGIN_SUMMARY,               /* summary            */
	PLUGIN_DESCRIPTION,           /* description        */
	PLUGIN_AUTHOR,                /* author             */
	PURPLE_WEBSITE,                 /* website            */

	plugin_load,                  /* load               */
	plugin_unload,                /* unload             */
	NULL,                         /* destroy            */

	&ui_info,                     /* ui_info            */
	NULL,                         /* extra_info         */
	NULL,                         /* prefs_info         */
	NULL,                         /* actions            */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none(PREF_PREFIX);

	purple_prefs_add_bool(PREF_IGNORE, TRUE);
	purple_prefs_add_bool(PREF_CHATS, TRUE);
	purple_prefs_add_bool(PREF_IMS, TRUE);

	purple_prefs_add_none(PREF_SEND);
	purple_prefs_add_none(PREF_RECV);
	purple_prefs_add_none(PREF_SYSTEM);
	purple_prefs_add_none(PREF_ERROR);
	purple_prefs_add_none(PREF_NICK);

	purple_prefs_add_string(PREF_SEND_C, "#909090");
	purple_prefs_add_string(PREF_RECV_C, "#000000");
	purple_prefs_add_string(PREF_SYSTEM_C, "#50a050");
	purple_prefs_add_string(PREF_ERROR_C, "#ff0000");
	purple_prefs_add_string(PREF_NICK_C, "#0000dd");

	purple_prefs_add_int(PREF_SEND_F, 0);
	purple_prefs_add_int(PREF_RECV_F, 0);
	purple_prefs_add_int(PREF_SYSTEM_F, FONT_ITALIC);
	purple_prefs_add_int(PREF_ERROR_F, FONT_BOLD | FONT_UNDERLINE);
	purple_prefs_add_int(PREF_NICK_F, FONT_BOLD);

	purple_prefs_add_bool(PREF_SEND_E, TRUE);
	purple_prefs_add_bool(PREF_RECV_E, TRUE);
	purple_prefs_add_bool(PREF_SYSTEM_E, TRUE);
	purple_prefs_add_bool(PREF_ERROR_E, TRUE);
	purple_prefs_add_bool(PREF_NICK_E, TRUE);
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)

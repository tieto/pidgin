/*
 * @file gtksound.h GTK+ Sound
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#include "internal.h"
#include "gtkgaim.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef USE_GSTREAMER
# include <gst/gst.h>
#endif /* USE_GSTREAMER */

#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "sound.h"
#include "util.h"

#include "gtkconv.h"
#include "gtksound.h"

struct gaim_sound_event {
	char *label;
	char *pref;
	char *def;
};

#define PLAY_SOUND_TIMEOUT 15000

static guint mute_login_sounds_timeout = 0;
static gboolean mute_login_sounds = FALSE;

static struct gaim_sound_event sounds[GAIM_NUM_SOUNDS] = {
	{N_("Buddy logs in"), "login", "login.wav"},
	{N_("Buddy logs out"), "logout", "logout.wav"},
	{N_("Message received"), "im_recv", "receive.wav"},
	{N_("Message received begins conversation"), "first_im_recv", "receive.wav"},
	{N_("Message sent"), "send_im", "send.wav"},
	{N_("Person enters chat"), "join_chat", "login.wav"},
	{N_("Person leaves chat"), "left_chat", "logout.wav"},
	{N_("You talk in chat"), "send_chat_msg", "send.wav"},
	{N_("Others talk in chat"), "chat_msg_recv", "receive.wav"},
	/* this isn't a terminator, it's the buddy pounce default sound event ;-) */
	{NULL, "pounce_default", "alert.wav"},
	{N_("Someone says your screen name in chat"), "nick_said", "alert.wav"}
};

static gboolean
unmute_login_sounds_cb(gpointer data)
{
	mute_login_sounds = FALSE;
	mute_login_sounds_timeout = 0;
	return FALSE;
}

static gboolean
chat_nick_matches_name(GaimConversation *conv, const char *aname)
{
	GaimConvChat *chat = NULL;
	char *nick = NULL;
	char *name = NULL;
	gboolean ret = FALSE;
	chat = gaim_conversation_get_chat_data(conv);

	if (chat==NULL)
		return ret;

	nick = g_strdup(gaim_normalize(conv->account, chat->nick));
	name = g_strdup(gaim_normalize(conv->account, aname));

	if (g_utf8_collate(nick, name) == 0)
		ret = TRUE;

	g_free(nick);
	g_free(name);

	return ret;
}

/*
 * play a sound event for a conversation, honoring make_sound flag
 * of conversation and checking for focus if conv_focus pref is set
 */
static void
play_conv_event(GaimConversation *conv, GaimSoundEventID event)
{
	/* If we should not play the sound for some reason, then exit early */
	if (conv != NULL)
	{
		GaimGtkConversation *gtkconv;
		GaimGtkWindow *win;
		gboolean has_focus;

		gtkconv = GAIM_GTK_CONVERSATION(conv);
		win = gtkconv->win;

		has_focus = gaim_conversation_has_focus(conv);

		if (!gtkconv->make_sound ||
			(has_focus && !gaim_prefs_get_bool("/gaim/gtk/sound/conv_focus")))
		{
			return;
		}
	}

	gaim_sound_play_event(event, conv ? gaim_conversation_get_account(conv) : NULL);
}

static void
buddy_state_cb(GaimBuddy *buddy, GaimSoundEventID event)
{
	gaim_sound_play_event(event, gaim_buddy_get_account(buddy));
}

static void
im_msg_received_cb(GaimAccount *account, char *sender,
				   char *message, GaimConversation *conv,
				   int flags, GaimSoundEventID event)
{
	if (flags & GAIM_MESSAGE_DELAYED)
		return;

	if (conv==NULL)
		gaim_sound_play_event(GAIM_SOUND_FIRST_RECEIVE, account);
	else
		play_conv_event(conv, event);
}

static void
im_msg_sent_cb(GaimAccount *account, const char *receiver,
			   const char *message, GaimSoundEventID event)
{
	GaimConversation *conv = gaim_find_conversation_with_account(
		GAIM_CONV_TYPE_ANY, receiver, account);
	play_conv_event(conv, event);
}

static void
chat_buddy_join_cb(GaimConversation *conv, const char *name,
				   GaimConvChatBuddyFlags flags, gboolean new_arrival,
				   GaimSoundEventID event)
{
	if (new_arrival && !chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_buddy_left_cb(GaimConversation *conv, const char *name,
				   const char *reason, GaimSoundEventID event)
{
	if (!chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_msg_sent_cb(GaimAccount *account, const char *message,
				 int id, GaimSoundEventID event)
{
	GaimConnection *conn = gaim_account_get_connection(account);
	GaimConversation *conv = NULL;

	if (conn!=NULL)
		conv = gaim_find_chat(conn,id);

	play_conv_event(conv, event);
}

static void
chat_msg_received_cb(GaimAccount *account, char *sender,
					 char *message, GaimConversation *conv,
					 GaimMessageFlags flags, GaimSoundEventID event)
{
	GaimConvChat *chat;

	if (flags & GAIM_MESSAGE_DELAYED)
		return;

	chat = gaim_conversation_get_chat_data(conv);
	g_return_if_fail(chat != NULL);

	if (gaim_conv_chat_is_user_ignored(chat, sender))
		return;

	if (chat_nick_matches_name(conv, sender))
		return;

	if (flags & GAIM_MESSAGE_NICK || gaim_utf8_has_word(message, chat->nick))
		play_conv_event(conv, GAIM_SOUND_CHAT_NICK);
	else
		play_conv_event(conv, event);
}

/*
 * We mute sounds for the 10 seconds after you log in so that
 * you don't get flooded with sounds when the blist shows all
 * your buddies logging in.
 */
static void
account_signon_cb(GaimConnection *gc, gpointer data)
{
	if (mute_login_sounds_timeout != 0)
		g_source_remove(mute_login_sounds_timeout);
	mute_login_sounds = TRUE;
	mute_login_sounds_timeout = gaim_timeout_add(10000, unmute_login_sounds_cb, NULL);
}

const char *
gaim_gtk_sound_get_event_option(GaimSoundEventID event)
{
	if(event >= GAIM_NUM_SOUNDS)
		return 0;

	return sounds[event].pref;
}

char *
gaim_gtk_sound_get_event_label(GaimSoundEventID event)
{
	if(event >= GAIM_NUM_SOUNDS)
		return NULL;

	return sounds[event].label;
}

void *
gaim_gtk_sound_get_handle()
{
	static int handle;

	return &handle;
}

static void
gaim_gtk_sound_init(void)
{
	void *gtk_sound_handle = gaim_gtk_sound_get_handle();
	void *blist_handle = gaim_blist_get_handle();
	void *conv_handle = gaim_conversations_get_handle();

	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						gtk_sound_handle, GAIM_CALLBACK(account_signon_cb),
						NULL);

	gaim_prefs_add_none("/gaim/gtk/sound");
	gaim_prefs_add_none("/gaim/gtk/sound/enabled");
	gaim_prefs_add_none("/gaim/gtk/sound/file");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/login", TRUE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/login", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/logout", TRUE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/logout", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/im_recv", TRUE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/im_recv", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/first_im_recv", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/first_im_recv", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/send_im", TRUE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/send_im", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/join_chat", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/join_chat", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/left_chat", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/left_chat", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/send_chat_msg", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/send_chat_msg", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/chat_msg_recv", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/chat_msg_recv", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/nick_said", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/nick_said", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/enabled/pounce_default", TRUE);
	gaim_prefs_add_string("/gaim/gtk/sound/file/pounce_default", "");
	gaim_prefs_add_bool("/gaim/gtk/sound/conv_focus", TRUE);
	gaim_prefs_add_bool("/gaim/gtk/sound/mute", FALSE);
	gaim_prefs_add_string("/gaim/gtk/sound/command", "");
	gaim_prefs_add_string("/gaim/gtk/sound/method", "automatic");
	gaim_prefs_add_int("/gaim/gtk/sound/volume", 50);

#ifdef USE_GSTREAMER
	gaim_debug_info("sound", "Initializing sound output drivers.\n");
	gst_init(NULL, NULL);
#endif /* USE_GSTREAMER */

	gaim_signal_connect(blist_handle, "buddy-signed-on",
						gtk_sound_handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_SOUND_BUDDY_ARRIVE));
	gaim_signal_connect(blist_handle, "buddy-signed-off",
						gtk_sound_handle, GAIM_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(GAIM_SOUND_BUDDY_LEAVE));
	gaim_signal_connect(conv_handle, "received-im-msg",
						gtk_sound_handle, GAIM_CALLBACK(im_msg_received_cb),
						GINT_TO_POINTER(GAIM_SOUND_RECEIVE));
	gaim_signal_connect(conv_handle, "sent-im-msg",
						gtk_sound_handle, GAIM_CALLBACK(im_msg_sent_cb),
						GINT_TO_POINTER(GAIM_SOUND_SEND));
	gaim_signal_connect(conv_handle, "chat-buddy-joined",
						gtk_sound_handle, GAIM_CALLBACK(chat_buddy_join_cb),
						GINT_TO_POINTER(GAIM_SOUND_CHAT_JOIN));
	gaim_signal_connect(conv_handle, "chat-buddy-left",
						gtk_sound_handle, GAIM_CALLBACK(chat_buddy_left_cb),
						GINT_TO_POINTER(GAIM_SOUND_CHAT_LEAVE));
	gaim_signal_connect(conv_handle, "sent-chat-msg",
						gtk_sound_handle, GAIM_CALLBACK(chat_msg_sent_cb),
						GINT_TO_POINTER(GAIM_SOUND_CHAT_YOU_SAY));
	gaim_signal_connect(conv_handle, "received-chat-msg",
						gtk_sound_handle, GAIM_CALLBACK(chat_msg_received_cb),
						GINT_TO_POINTER(GAIM_SOUND_CHAT_SAY));
}

static void
gaim_gtk_sound_uninit(void)
{
#ifdef USE_GSTREAMER
	gst_deinit();
#endif

	gaim_signals_disconnect_by_handle(gaim_gtk_sound_get_handle());
}

#ifdef USE_GSTREAMER
static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
  	  gpointer    data)
{
	GstElement *play = data;
	GError *err;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		gst_element_set_state(play, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(play));
		break;
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, NULL);
		gaim_debug_error("gstreamer", err->message);
		g_error_free(err);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &err, NULL);
		gaim_debug_warning("gstreamer", err->message);
		g_error_free(err);
		break;
	default:
		break;
	}
	return TRUE;
}
#endif

static void
gaim_gtk_sound_play_file(const char *filename)
{
	const char *method;
#ifdef USE_GSTREAMER
	float volume;
	char *uri;
	GstElement *sink = NULL;
	GstElement *play = NULL;
#endif

	if (gaim_prefs_get_bool("/gaim/gtk/sound/mute"))
		return;

	method = gaim_prefs_get_string("/gaim/gtk/sound/method");

	if (!strcmp(method, "none")) {
		return;
	} else if (!strcmp(method, "beep")) {
		gdk_beep();
		return;
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		char *tmp = g_strdup_printf(_("Unable to play sound because the chosen file (%s) does not exist."), filename);
		gaim_notify_error(NULL, NULL, tmp, NULL);
		g_free(tmp);
		return;
	}

#ifndef _WIN32
	if (!strcmp(method, "custom")) {
		const char *sound_cmd;
		char *command;
		GError *error = NULL;

		sound_cmd = gaim_prefs_get_string("/gaim/gtk/sound/command");

		if (!sound_cmd || *sound_cmd == '\0') {
			gaim_notify_error(NULL, NULL,
							  _("Unable to play sound because the "
								"'Command' sound method has been chosen, "
								"but no command has been set."), NULL);
			return;
		}

		if(strstr(sound_cmd, "%s"))
			command = gaim_strreplace(sound_cmd, "%s", filename);
		else
			command = g_strdup_printf("%s %s", sound_cmd, filename);

		if(!g_spawn_command_line_async(command, &error)) {
			char *tmp = g_strdup_printf(_("Unable to play sound because the configured sound command could not be launched: %s"), error->message);
			gaim_notify_error(NULL, NULL, tmp, NULL);
			g_free(tmp);
			g_error_free(error);
		}

		g_free(command);
		return;
	}
#ifdef USE_GSTREAMER
	volume = (float)(CLAMP(gaim_prefs_get_int("/gaim/gtk/sound/volume"),0,100)) / 50;
	if (!strcmp(method, "automatic")) {
		if (gaim_running_gnome()) {
			sink = gst_element_factory_make("gconfaudiosink", "sink");
		}
	} else if (!strcmp(method, "esd")) {
		sink = gst_element_factory_make("esdsink", "sink");
	} else if (!strcmp(method, "arts")) {
		sink = gst_element_factory_make("artssink", "sink");
	} else if (!strcmp(method, "nas")) {
		sink = gst_element_factory_make("nassink", "sink");
	}

	uri = g_strdup_printf("file://%s", filename);
	play = gst_element_factory_make("playbin", "play");
	
	g_object_set(G_OBJECT(play), "uri", uri,
		                     "volume", volume, 
		                     "audio-sink", sink, NULL);

	gst_bus_add_watch(gst_pipeline_get_bus(GST_PIPELINE(play)),
			  bus_call, play);
	gst_element_set_state(play, GST_STATE_PLAYING);
	
	g_free(uri);
	
#else /* USE_GSTREAMER */
	gdk_beep();
	return;
#endif /* USE_GSTREAMER */
#else /* _WIN32 */
	gaim_debug_info("sound", "Playing %s\n", filename);

	if (G_WIN32_HAVE_WIDECHAR_API ()) {
		wchar_t *wc_filename = g_utf8_to_utf16(filename,
				-1, NULL, NULL, NULL);
		if (!PlaySoundW(wc_filename, NULL, SND_ASYNC | SND_FILENAME))
			gaim_debug(GAIM_DEBUG_ERROR, "sound", "Error playing sound.\n");
		g_free(wc_filename);
	} else {
		char *l_filename = g_locale_from_utf8(filename,
				-1, NULL, NULL, NULL);
		if (!PlaySoundA(l_filename, NULL, SND_ASYNC | SND_FILENAME))
			gaim_debug(GAIM_DEBUG_ERROR, "sound", "Error playing sound.\n");
		g_free(l_filename);
	}
#endif /* _WIN32 */
}

static void
gaim_gtk_sound_play_event(GaimSoundEventID event)
{
	char *enable_pref;
	char *file_pref;

	if ((event == GAIM_SOUND_BUDDY_ARRIVE) && mute_login_sounds)
		return;

	if (event >= GAIM_NUM_SOUNDS) {
		gaim_debug_error("sound", "got request for unknown sound: %d\n", event);
		return;
	}

	enable_pref = g_strdup_printf("/gaim/gtk/sound/enabled/%s",
			sounds[event].pref);
	file_pref = g_strdup_printf("/gaim/gtk/sound/file/%s", sounds[event].pref);

	/* check NULL for sounds that don't have an option, ie buddy pounce */
	if (gaim_prefs_get_bool(enable_pref)) {
		char *filename = g_strdup(gaim_prefs_get_string(file_pref));
		if(!filename || !strlen(filename)) {
			if(filename) g_free(filename);
			filename = g_build_filename(DATADIR, "sounds", "gaim", sounds[event].def, NULL);
		}

		gaim_sound_play_file(filename, NULL);
		g_free(filename);
	}

	g_free(enable_pref);
	g_free(file_pref);
}

static GaimSoundUiOps sound_ui_ops =
{
	gaim_gtk_sound_init,
	gaim_gtk_sound_uninit,
	gaim_gtk_sound_play_file,
	gaim_gtk_sound_play_event
};

GaimSoundUiOps *
gaim_gtk_sound_get_ui_ops(void)
{
	return &sound_ui_ops;
}

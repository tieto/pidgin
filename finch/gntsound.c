/**
 * @file gntsound.c GNT Sound API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 */
#include "finch.h"
#include <internal.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef USE_GSTREAMER
#include <gst/gst.h>
#endif /* USE_GSTREAMER */

#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "sound.h"
#include "util.h"

#include "gntconv.h"

#include "gntbox.h"
#include "gntwindow.h"
#include "gntcombobox.h"
#include "gntlabel.h"
#include "gntconv.h"
#include "gntsound.h"
#include "gntwidget.h"
#include "gntentry.h"
#include "gntcheckbox.h"
#include "gntline.h"
#include "gntslider.h"
#include "gnttree.h"
#include "gntfilesel.h"

typedef struct {
	PurpleSoundEventID id;
	char *label;
	char *pref;
	char *def;
	char *file;
} FinchSoundEvent;

typedef struct {
	GntWidget *method;
	GntWidget *command;
	GntWidget *conv_focus;
	GntWidget *while_status;
	GntWidget *volume;
	GntWidget *events;
	GntWidget *window;
	GntWidget *selector;

	GntWidget *profiles;
	GntWidget *new_profile;
	gchar * original_profile;
} SoundPrefDialog;

#define DEFAULT_PROFILE "default"

static SoundPrefDialog *pref_dialog;

#define PLAY_SOUND_TIMEOUT 15000

static guint mute_login_sounds_timeout = 0;
static gboolean mute_login_sounds = FALSE;

#ifdef USE_GSTREAMER
static gboolean gst_init_failed;
#endif /* USE_GSTREAMER */

static FinchSoundEvent sounds[PURPLE_NUM_SOUNDS] = {
	{PURPLE_SOUND_BUDDY_ARRIVE, N_("Buddy logs in"), "login", "login.wav", NULL},
	{PURPLE_SOUND_BUDDY_LEAVE,  N_("Buddy logs out"), "logout", "logout.wav", NULL},
	{PURPLE_SOUND_RECEIVE,      N_("Message received"), "im_recv", "receive.wav", NULL},
	{PURPLE_SOUND_FIRST_RECEIVE, N_("Message received begins conversation"), "first_im_recv", "receive.wav", NULL},
	{PURPLE_SOUND_SEND,         N_("Message sent"), "send_im", "send.wav", NULL},
	{PURPLE_SOUND_CHAT_JOIN,    N_("Person enters chat"), "join_chat", "login.wav", NULL},
	{PURPLE_SOUND_CHAT_LEAVE,   N_("Person leaves chat"), "left_chat", "logout.wav", NULL},
	{PURPLE_SOUND_CHAT_YOU_SAY, N_("You talk in chat"), "send_chat_msg", "send.wav", NULL},
	{PURPLE_SOUND_CHAT_SAY,     N_("Others talk in chat"), "chat_msg_recv", "receive.wav", NULL},
	{PURPLE_SOUND_POUNCE_DEFAULT, NULL, "pounce_default", "alert.wav", NULL},
	{PURPLE_SOUND_CHAT_NICK,    N_("Someone says your username in chat"), "nick_said", "alert.wav", NULL}
};

const char *
finch_sound_get_active_profile()
{
	return purple_prefs_get_string(FINCH_PREFS_ROOT "/sound/actprofile");
}

/* This method creates a pref name based on the current active profile.
 * So if "Home" is the current active profile the pref name
 * [FINCH_PREFS_ROOT "/sound/profiles/Home/$NAME"] is created.
 */
static gchar *
make_pref(const char *name)
{
	static char pref_string[512];
	g_snprintf(pref_string, sizeof(pref_string),
			FINCH_PREFS_ROOT "/sound/profiles/%s%s", finch_sound_get_active_profile(), name);
	return pref_string;
}


static gboolean
unmute_login_sounds_cb(gpointer data)
{
	mute_login_sounds = FALSE;
	mute_login_sounds_timeout = 0;
	return FALSE;
}

static gboolean
chat_nick_matches_name(PurpleConversation *conv, const char *aname)
{
	PurpleConvChat *chat = NULL;
	char *nick = NULL;
	char *name = NULL;
	gboolean ret = FALSE;
	PurpleAccount *account;

	chat = purple_conversation_get_chat_data(conv);
	if (chat == NULL)
		return ret;

	account = purple_conversation_get_account(conv);
	nick = g_strdup(purple_normalize(account, chat->nick));
	name = g_strdup(purple_normalize(account, aname));

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
play_conv_event(PurpleConversation *conv, PurpleSoundEventID event)
{
	/* If we should not play the sound for some reason, then exit early */
	if (conv != NULL)
	{
		FinchConv *gntconv;
		gboolean has_focus;

		gntconv = FINCH_CONV(conv);

		has_focus = purple_conversation_has_focus(conv);

		if ((gntconv->flags & FINCH_CONV_NO_SOUND) ||
			(has_focus && !purple_prefs_get_bool(make_pref("/conv_focus"))))
		{
			return;
		}
	}

	purple_sound_play_event(event, conv ? purple_conversation_get_account(conv) : NULL);
}

static void
buddy_state_cb(PurpleBuddy *buddy, PurpleSoundEventID event)
{
	purple_sound_play_event(event, purple_buddy_get_account(buddy));
}

static void
im_msg_received_cb(PurpleAccount *account, char *sender,
				   char *message, PurpleConversation *conv,
				   PurpleMessageFlags flags, PurpleSoundEventID event)
{
	if (flags & PURPLE_MESSAGE_DELAYED)
		return;

	if (conv == NULL) {
		purple_sound_play_event(PURPLE_SOUND_FIRST_RECEIVE, account);
	} else {
		play_conv_event(conv, event);
	}
}

static void
im_msg_sent_cb(PurpleAccount *account, const char *receiver,
			   const char *message, PurpleSoundEventID event)
{
	PurpleConversation *conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_IM, receiver, account);
	play_conv_event(conv, event);
}

static void
chat_buddy_join_cb(PurpleConversation *conv, const char *name,
				   PurpleConvChatBuddyFlags flags, gboolean new_arrival,
				   PurpleSoundEventID event)
{
	if (new_arrival && !chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_buddy_left_cb(PurpleConversation *conv, const char *name,
				   const char *reason, PurpleSoundEventID event)
{
	if (!chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_msg_sent_cb(PurpleAccount *account, const char *message,
				 int id, PurpleSoundEventID event)
{
	PurpleConnection *conn = purple_account_get_connection(account);
	PurpleConversation *conv = NULL;

	if (conn!=NULL)
		conv = purple_find_chat(conn, id);

	play_conv_event(conv, event);
}

static void
chat_msg_received_cb(PurpleAccount *account, char *sender,
					 char *message, PurpleConversation *conv,
					 PurpleMessageFlags flags, PurpleSoundEventID event)
{
	PurpleConvChat *chat;

	if (flags & PURPLE_MESSAGE_DELAYED)
		return;

	chat = purple_conversation_get_chat_data(conv);
	g_return_if_fail(chat != NULL);

	if (purple_conv_chat_is_user_ignored(chat, sender))
		return;

	if (chat_nick_matches_name(conv, sender))
		return;

	if (flags & PURPLE_MESSAGE_NICK || purple_utf8_has_word(message, chat->nick))
		play_conv_event(conv, PURPLE_SOUND_CHAT_NICK);
	else
		play_conv_event(conv, event);
}

/*
 * We mute sounds for the 10 seconds after you log in so that
 * you don't get flooded with sounds when the blist shows all
 * your buddies logging in.
 */
static void
account_signon_cb(PurpleConnection *gc, gpointer data)
{
	if (mute_login_sounds_timeout != 0)
		g_source_remove(mute_login_sounds_timeout);
	mute_login_sounds = TRUE;
	mute_login_sounds_timeout = purple_timeout_add_seconds(10, unmute_login_sounds_cb, NULL);
}

static void *
finch_sound_get_handle(void)
{
	static int handle;

	return &handle;
}


/* This gets called when the active profile changes */
static void
initialize_profile(const char *name, PurplePrefType type, gconstpointer val, gpointer null)
{
	if (purple_prefs_exists(make_pref("")))
		return;

	purple_prefs_add_none(make_pref(""));
	purple_prefs_add_none(make_pref("/enabled"));
	purple_prefs_add_none(make_pref("/file"));
	purple_prefs_add_bool(make_pref("/enabled/login"), FALSE);
	purple_prefs_add_path(make_pref("/file/login"), "");
	purple_prefs_add_bool(make_pref("/enabled/logout"), FALSE);
	purple_prefs_add_path(make_pref("/file/logout"), "");
	purple_prefs_add_bool(make_pref("/enabled/im_recv"), FALSE);
	purple_prefs_add_path(make_pref("/file/im_recv"), "");
	purple_prefs_add_bool(make_pref("/enabled/first_im_recv"), FALSE);
	purple_prefs_add_path(make_pref("/file/first_im_recv"), "");
	purple_prefs_add_bool(make_pref("/enabled/send_im"), FALSE);
	purple_prefs_add_path(make_pref("/file/send_im"), "");
	purple_prefs_add_bool(make_pref("/enabled/join_chat"), FALSE);
	purple_prefs_add_path(make_pref("/file/join_chat"), "");
	purple_prefs_add_bool(make_pref("/enabled/left_chat"), FALSE);
	purple_prefs_add_path(make_pref("/file/left_chat"), "");
	purple_prefs_add_bool(make_pref("/enabled/send_chat_msg"), FALSE);
	purple_prefs_add_path(make_pref("/file/send_chat_msg"), "");
	purple_prefs_add_bool(make_pref("/enabled/chat_msg_recv"), FALSE);
	purple_prefs_add_path(make_pref("/file/chat_msg_recv"), "");
	purple_prefs_add_bool(make_pref("/enabled/nick_said"), FALSE);
	purple_prefs_add_path(make_pref("/file/nick_said"), "");
	purple_prefs_add_bool(make_pref("/enabled/pounce_default"), FALSE);
	purple_prefs_add_path(make_pref("/file/pounce_default"), "");
	purple_prefs_add_bool(make_pref("/conv_focus"), FALSE);
	purple_prefs_add_bool(make_pref("/mute"), FALSE);
	purple_prefs_add_path(make_pref("/command"), "");
	purple_prefs_add_string(make_pref("/method"), "automatic");
	purple_prefs_add_int(make_pref("/volume"), 50);
}

static void
finch_sound_init(void)
{
	void *gnt_sound_handle = finch_sound_get_handle();
	void *blist_handle = purple_blist_get_handle();
	void *conv_handle = purple_conversations_get_handle();
#ifdef USE_GSTREAMER
	GError *error = NULL;
#endif

	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						gnt_sound_handle, PURPLE_CALLBACK(account_signon_cb),
						NULL);

	purple_prefs_add_none(FINCH_PREFS_ROOT "/sound");
	purple_prefs_add_string(FINCH_PREFS_ROOT "/sound/actprofile", DEFAULT_PROFILE);
	purple_prefs_add_none(FINCH_PREFS_ROOT "/sound/profiles");

	purple_prefs_connect_callback(gnt_sound_handle, FINCH_PREFS_ROOT "/sound/actprofile", initialize_profile, NULL);
	purple_prefs_trigger_callback(FINCH_PREFS_ROOT "/sound/actprofile");

	
#ifdef USE_GSTREAMER
	purple_debug_info("sound", "Initializing sound output drivers.\n");
#if (GST_VERSION_MAJOR > 0 || \
	(GST_VERSION_MAJOR == 0 && GST_VERSION_MINOR > 10) || \
	 (GST_VERSION_MAJOR == 0 && GST_VERSION_MINOR == 10 && GST_VERSION_MICRO >= 10))
	gst_registry_fork_set_enabled(FALSE);
#endif
	if ((gst_init_failed = !gst_init_check(NULL, NULL, &error))) {
		purple_notify_error(NULL, _("GStreamer Failure"),
					_("GStreamer failed to initialize."),
					error ? error->message : "");
		if (error) {
			g_error_free(error);
			error = NULL;
		}
	}
#endif /* USE_GSTREAMER */

	purple_signal_connect(blist_handle, "buddy-signed-on",
						gnt_sound_handle, PURPLE_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(PURPLE_SOUND_BUDDY_ARRIVE));
	purple_signal_connect(blist_handle, "buddy-signed-off",
						gnt_sound_handle, PURPLE_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(PURPLE_SOUND_BUDDY_LEAVE));
	purple_signal_connect(conv_handle, "received-im-msg",
						gnt_sound_handle, PURPLE_CALLBACK(im_msg_received_cb),
						GINT_TO_POINTER(PURPLE_SOUND_RECEIVE));
	purple_signal_connect(conv_handle, "sent-im-msg",
						gnt_sound_handle, PURPLE_CALLBACK(im_msg_sent_cb),
						GINT_TO_POINTER(PURPLE_SOUND_SEND));
	purple_signal_connect(conv_handle, "chat-buddy-joined",
						gnt_sound_handle, PURPLE_CALLBACK(chat_buddy_join_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_JOIN));
	purple_signal_connect(conv_handle, "chat-buddy-left",
						gnt_sound_handle, PURPLE_CALLBACK(chat_buddy_left_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_LEAVE));
	purple_signal_connect(conv_handle, "sent-chat-msg",
						gnt_sound_handle, PURPLE_CALLBACK(chat_msg_sent_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_YOU_SAY));
	purple_signal_connect(conv_handle, "received-chat-msg",
						gnt_sound_handle, PURPLE_CALLBACK(chat_msg_received_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_SAY));
}

static void
finch_sound_uninit(void)
{
#ifdef USE_GSTREAMER
	if (!gst_init_failed)
		gst_deinit();
#endif

	purple_signals_disconnect_by_handle(finch_sound_get_handle());
}

#ifdef USE_GSTREAMER
static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	GstElement *play = data;
	GError *err = NULL;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, NULL);
		purple_debug_error("gstreamer", "%s\n", err->message);
		g_error_free(err);
		/* fall-through and clean up */
	case GST_MESSAGE_EOS:
		gst_element_set_state(play, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(play));
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &err, NULL);
		purple_debug_warning("gstreamer", "%s\n", err->message);
		g_error_free(err);
		break;
	default:
		break;
	}
	return TRUE;
}
#endif

static void
finch_sound_play_file(const char *filename)
{
	const char *method;
#ifdef USE_GSTREAMER
	float volume;
	char *uri;
	GstElement *sink = NULL;
	GstElement *play = NULL;
	GstBus *bus = NULL;
#endif
	if (purple_prefs_get_bool(make_pref("/mute")))
		return;

	method = purple_prefs_get_string(make_pref("/method"));

	if (!strcmp(method, "nosound")) {
		return;
	} else if (!strcmp(method, "beep")) {
		beep();
		return;
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		purple_debug_error("gntsound", "sound file (%s) does not exist.\n", filename);
		return;
	}

#ifndef _WIN32
	if (!strcmp(method, "custom")) {
		const char *sound_cmd;
		char *command;
		char *esc_filename;
		GError *error = NULL;

		sound_cmd = purple_prefs_get_path(make_pref("/command"));

		if (!sound_cmd || *sound_cmd == '\0') {
			purple_debug_error("gntsound",
					 "'Command' sound method has been chosen, "
					 "but no command has been set.");
			return;
		}

		esc_filename = g_shell_quote(filename);

		if (strstr(sound_cmd, "%s"))
			command = purple_strreplace(sound_cmd, "%s", esc_filename);
		else
			command = g_strdup_printf("%s %s", sound_cmd, esc_filename);

		if (!g_spawn_command_line_async(command, &error)) {
			purple_debug_error("gntsound", "sound command could not be launched: %s\n", error->message);
			g_error_free(error);
		}

		g_free(esc_filename);
		g_free(command);
		return;
	}
#ifdef USE_GSTREAMER
	if (gst_init_failed)  /* Perhaps do beep instead? */
		return;
	volume = (float)(CLAMP(purple_prefs_get_int(make_pref("/volume")), 0, 100)) / 50;
	if (!strcmp(method, "automatic")) {
		if (purple_running_gnome()) {
			sink = gst_element_factory_make("gconfaudiosink", "sink");
		}
		if (!sink)
			sink = gst_element_factory_make("autoaudiosink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else if (!strcmp(method, "esd")) {
		sink = gst_element_factory_make("esdsink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else if (!strcmp(method, "alsa")) {
		sink = gst_element_factory_make("alsasink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else {
		purple_debug_error("sound", "Unknown sound method '%s'\n", method);
		return;
	}

	play = gst_element_factory_make("playbin", "play");
	
	if (play == NULL) {
		return;
	}
	
	uri = g_strdup_printf("file://%s", filename);

	g_object_set(G_OBJECT(play), "uri", uri,
		                     "volume", volume,
		                     "audio-sink", sink, NULL);

	bus = gst_pipeline_get_bus(GST_PIPELINE(play));
	gst_bus_add_watch(bus, bus_call, play);

	gst_element_set_state(play, GST_STATE_PLAYING);

	gst_object_unref(bus);
	g_free(uri);

#else /* USE_GSTREAMER */
	beep();
	return;
#endif /* USE_GSTREAMER */
#else /* _WIN32 */
	purple_debug_info("sound", "Playing %s\n", filename);

	{
		wchar_t *wc_filename = g_utf8_to_utf16(filename,
				-1, NULL, NULL, NULL);
		if (!PlaySoundW(wc_filename, NULL, SND_ASYNC | SND_FILENAME))
			purple_debug(PURPLE_DEBUG_ERROR, "sound", "Error playing sound.\n");
		g_free(wc_filename);
	}
#endif /* _WIN32 */
}

static void
finch_sound_play_event(PurpleSoundEventID event)
{
	char *enable_pref;
	char *file_pref;
	if ((event == PURPLE_SOUND_BUDDY_ARRIVE) && mute_login_sounds)
		return;

	if (event >= PURPLE_NUM_SOUNDS ||
			event >= G_N_ELEMENTS(sounds)) {
		purple_debug_error("sound", "got request for unknown sound: %d\n", event);
		return;
	}

	enable_pref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/enabled/%s",
			finch_sound_get_active_profile(),
			sounds[event].pref);
	file_pref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/file/%s", finch_sound_get_active_profile(), sounds[event].pref);

	/* check NULL for sounds that don't have an option, ie buddy pounce */
	if (purple_prefs_get_bool(enable_pref)) {
		char *filename = g_strdup(purple_prefs_get_path(file_pref));
		if (!filename || !strlen(filename)) {
			g_free(filename);
			/* XXX Consider creating a constant for "sounds/purple" to be shared with Pidgin */
			filename = g_build_filename(DATADIR, "sounds", "purple", sounds[event].def, NULL);
		}

		purple_sound_play_file(filename, NULL);
		g_free(filename);
	}

	g_free(enable_pref);
	g_free(file_pref);
}

GList *
finch_sound_get_profiles()
{
	GList *list = NULL, *iter;
	iter = purple_prefs_get_children_names(FINCH_PREFS_ROOT "/sound/profiles");
	while (iter) {
		list = g_list_append(list, g_strdup(strrchr(iter->data, '/') + 1));
		g_free(iter->data);
		iter = g_list_delete_link(iter, iter);
	}
	return list;
}

/* This will also create it if it doesn't exist */
void
finch_sound_set_active_profile(const char *name)
{
	purple_prefs_set_string(FINCH_PREFS_ROOT "/sound/actprofile", name);
}

static gboolean
finch_sound_profile_exists(const char *name)
{
	gchar * tmp;
	gboolean ret = purple_prefs_exists(tmp = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s", name));
	g_free(tmp);
	return ret;
}

static void
save_cb(GntWidget *button, gpointer win)
{
	GList * itr;

	purple_prefs_set_string(make_pref("/method"), gnt_combo_box_get_selected_data(GNT_COMBO_BOX(pref_dialog->method)));
	purple_prefs_set_path(make_pref("/command"), gnt_entry_get_text(GNT_ENTRY(pref_dialog->command)));
	purple_prefs_set_bool(make_pref("/conv_focus"), gnt_check_box_get_checked(GNT_CHECK_BOX(pref_dialog->conv_focus)));
	purple_prefs_set_int("/purple/sound/while_status", GPOINTER_TO_INT(gnt_combo_box_get_selected_data(GNT_COMBO_BOX(pref_dialog->while_status))));
	purple_prefs_set_int(make_pref("/volume"), gnt_slider_get_value(GNT_SLIDER(pref_dialog->volume)));

	for (itr = gnt_tree_get_rows(GNT_TREE(pref_dialog->events)); itr; itr = itr->next) {
		FinchSoundEvent * event = &sounds[GPOINTER_TO_INT(itr->data)];
		char * filepref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/file/%s", finch_sound_get_active_profile(), event->pref);
		char * boolpref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/enabled/%s", finch_sound_get_active_profile(), event->pref);
		purple_prefs_set_bool(boolpref, gnt_tree_get_choice(GNT_TREE(pref_dialog->events), itr->data));
		purple_prefs_set_path(filepref, event->file ? event->file : "");
		g_free(filepref);
		g_free(boolpref);
	}
	gnt_widget_destroy(GNT_WIDGET(win));
}

static void
file_cb(GntFileSel *w, const char *path, const char *file, gpointer data)
{
	FinchSoundEvent *event = data;

	g_free(event->file);
	event->file = g_strdup(path);

	gnt_tree_change_text(GNT_TREE(pref_dialog->events), GINT_TO_POINTER(event->id), 1, file);
	gnt_tree_set_choice(GNT_TREE(pref_dialog->events), GINT_TO_POINTER(event->id), TRUE);
	
	gnt_widget_destroy(GNT_WIDGET(w));
}

static void
test_cb(GntWidget *button, gpointer null)
{
	PurpleSoundEventID id = GPOINTER_TO_INT(gnt_tree_get_selection_data(GNT_TREE(pref_dialog->events)));
	FinchSoundEvent * event = &sounds[id];
	char *enabled, *file, *tmpfile, *volpref;
	gboolean temp_value;
	int volume;

	enabled = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/enabled/%s",
			finch_sound_get_active_profile(), event->pref);
	file = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/file/%s",
			finch_sound_get_active_profile(), event->pref);
	volpref = g_strdup(make_pref("/volume"));

	temp_value = purple_prefs_get_bool(enabled);
	tmpfile = g_strdup(purple_prefs_get_path(file));
	volume = purple_prefs_get_int(volpref);

	purple_prefs_set_path(file, event->file);
	if (!temp_value) purple_prefs_set_bool(enabled, TRUE);
	purple_prefs_set_int(volpref, gnt_slider_get_value(GNT_SLIDER(pref_dialog->volume)));

	purple_sound_play_event(id, NULL);

	if (!temp_value) purple_prefs_set_bool(enabled, FALSE);
	purple_prefs_set_path(file, tmpfile);
	purple_prefs_set_int(volpref, volume);

	g_free(enabled);
	g_free(file);
	g_free(tmpfile);
	g_free(volpref);
}

static void
reset_cb(GntWidget *button, gpointer null)
{
	/* Don't dereference this pointer ! */
	gpointer key = gnt_tree_get_selection_data(GNT_TREE(pref_dialog->events)); 

	FinchSoundEvent * event = &sounds[GPOINTER_TO_INT(key)];
	g_free(event->file);
	event->file = NULL;
	gnt_tree_change_text(GNT_TREE(pref_dialog->events), key, 1, _("(default)"));
}


static void
choose_cb(GntWidget *button, gpointer null)
{
	GntWidget *w = gnt_file_sel_new();
	GntFileSel *sel = GNT_FILE_SEL(w);
	PurpleSoundEventID id = GPOINTER_TO_INT(gnt_tree_get_selection_data(GNT_TREE(pref_dialog->events)));
	FinchSoundEvent * event = &sounds[id];
	char *path = NULL;

	gnt_box_set_title(GNT_BOX(w), _("Select Sound File ..."));
	gnt_file_sel_set_current_location(sel,
			(event && event->file) ? (path = g_path_get_dirname(event->file))
				: purple_home_dir());

	g_signal_connect_swapped(G_OBJECT(sel->cancel), "activate", G_CALLBACK(gnt_widget_destroy), sel);
	g_signal_connect(G_OBJECT(sel), "file_selected", G_CALLBACK(file_cb), event);
	g_signal_connect_swapped(G_OBJECT(sel), "destroy", G_CALLBACK(g_nullify_pointer), &pref_dialog->selector);

	/* If there's an already open file-selector, close that one. */
	if (pref_dialog->selector)
		gnt_widget_destroy(pref_dialog->selector);

	pref_dialog->selector = w;

	gnt_widget_show(w);
	g_free(path);
}

static void
release_pref_dialog(GntBindable *data, gpointer null)
{
	GList * itr;
	for (itr = gnt_tree_get_rows(GNT_TREE(pref_dialog->events)); itr; itr = itr->next) {
		PurpleSoundEventID id = GPOINTER_TO_INT(itr->data);
		FinchSoundEvent * e = &sounds[id];
		g_free(e->file);
		e->file = NULL;
	}
	if (pref_dialog->selector)
		gnt_widget_destroy(pref_dialog->selector);
	g_free(pref_dialog->original_profile);
	g_free(pref_dialog);
	pref_dialog = NULL;
}

static void
load_pref_window(const char * profile)
{
	gint i;

	finch_sound_set_active_profile(profile);

	gnt_combo_box_set_selected(GNT_COMBO_BOX(pref_dialog->method), (gchar *)purple_prefs_get_string(make_pref("/method")));

	gnt_entry_set_text(GNT_ENTRY(pref_dialog->command), purple_prefs_get_path(make_pref("/command")));

	gnt_check_box_set_checked(GNT_CHECK_BOX(pref_dialog->conv_focus), purple_prefs_get_bool(make_pref("/conv_focus")));

	gnt_combo_box_set_selected(GNT_COMBO_BOX(pref_dialog->while_status), GINT_TO_POINTER(purple_prefs_get_int("/purple" "/sound/while_status")));

	gnt_slider_set_value(GNT_SLIDER(pref_dialog->volume), CLAMP(purple_prefs_get_int(make_pref("/volume")), 0, 100));

	for (i = 0; i < PURPLE_NUM_SOUNDS; i++) {
		FinchSoundEvent * event = &sounds[i];
		gchar *boolpref;
		gchar *filepref, *basename = NULL;
		const char * profile = finch_sound_get_active_profile();

		filepref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/file/%s", profile, event->pref);

		g_free(event->file);
		event->file = g_strdup(purple_prefs_get_path(filepref));

		g_free(filepref);
		if (event->label == NULL) {
			continue;
		}

		boolpref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s/enabled/%s", profile, event->pref);

		gnt_tree_change_text(GNT_TREE(pref_dialog->events), GINT_TO_POINTER(i), 0, event->label);
		gnt_tree_change_text(GNT_TREE(pref_dialog->events), GINT_TO_POINTER(i), 1,
				event->file[0] ? (basename = g_path_get_basename(event->file)) : _("(default)"));
		g_free(basename);

		gnt_tree_set_choice(GNT_TREE(pref_dialog->events), GINT_TO_POINTER(i), purple_prefs_get_bool(boolpref));

		g_free(boolpref);
	}

	gnt_tree_set_selected(GNT_TREE(pref_dialog->profiles), (gchar *)finch_sound_get_active_profile());

	gnt_widget_draw(pref_dialog->window);
}

static void
reload_pref_window(const char *profile)
{
	if (!strcmp(profile, finch_sound_get_active_profile()))
		return;
	load_pref_window(profile);
}

static void
prof_del_cb(GntWidget *button, gpointer null)
{
	const char * profile = gnt_entry_get_text(GNT_ENTRY(pref_dialog->new_profile));
	gchar * pref;

	if (!strcmp(profile, DEFAULT_PROFILE))
		return;

	pref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/profiles/%s", profile);
	purple_prefs_remove(pref);
	g_free(pref);

	if (!strcmp(pref_dialog->original_profile, profile)) {
		g_free(pref_dialog->original_profile);
		pref_dialog->original_profile = g_strdup(DEFAULT_PROFILE);
	}

	if(!strcmp(profile, finch_sound_get_active_profile()))
		reload_pref_window(DEFAULT_PROFILE);

	gnt_tree_remove(GNT_TREE(pref_dialog->profiles), (gchar *) profile);
}

static void
prof_add_cb(GntButton *button, GntEntry * entry)
{
	const char * profile = gnt_entry_get_text(entry);
	GntTreeRow * row;
	if (!finch_sound_profile_exists(profile)) {
		gpointer key = g_strdup(profile);
		row = gnt_tree_create_row(GNT_TREE(pref_dialog->profiles), profile);
		gnt_tree_add_row_after(GNT_TREE(pref_dialog->profiles), key,
				row,
				NULL, NULL);
		gnt_entry_set_text(entry, "");
		gnt_tree_set_selected(GNT_TREE(pref_dialog->profiles), key);
		finch_sound_set_active_profile(key);
	} else 
		reload_pref_window(profile);
}

static void
prof_load_cb(GntTree *tree, gpointer oldkey, gpointer newkey, gpointer null)
{
	reload_pref_window(newkey);
}

static void
cancel_cb(GntButton *button, gpointer win)
{
	finch_sound_set_active_profile(pref_dialog->original_profile);
	gnt_widget_destroy(GNT_WIDGET(win));
}

void
finch_sounds_show_all(void)
{
	GntWidget *box, *tmpbox, *splitbox, *cmbox, *slider;
	GntWidget *entry;
	GntWidget *chkbox;
	GntWidget *button;
	GntWidget *label;
	GntWidget *tree;
	GntWidget *win;

	gint i;
	GList *itr, *list;

	if (pref_dialog) {
		gnt_window_present(pref_dialog->window);
		return;
	}

	pref_dialog = g_new0(SoundPrefDialog, 1);

	pref_dialog->original_profile = g_strdup(finch_sound_get_active_profile());

	pref_dialog->window = win = gnt_window_box_new(FALSE, TRUE);
	gnt_box_set_pad(GNT_BOX(win), 0);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_title(GNT_BOX(win), _("Sound Preferences"));
	gnt_box_set_fill(GNT_BOX(win), TRUE);
	gnt_box_set_alignment(GNT_BOX(win), GNT_ALIGN_MID);

	/* Profiles */
	splitbox = gnt_hbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(splitbox), 0);
	gnt_box_set_alignment(GNT_BOX(splitbox), GNT_ALIGN_TOP);

	box = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(box), 0);
	gnt_box_add_widget(GNT_BOX(box), gnt_label_new_with_format(_("Profiles"), GNT_TEXT_FLAG_BOLD));
	pref_dialog->profiles = tree = gnt_tree_new();
	gnt_tree_set_hash_fns(GNT_TREE(tree), g_str_hash, g_str_equal, g_free);
	gnt_tree_set_compare_func(GNT_TREE(tree), (GCompareFunc)g_ascii_strcasecmp);
	g_signal_connect(G_OBJECT(tree), "selection-changed", G_CALLBACK(prof_load_cb), NULL);

	itr = list = finch_sound_get_profiles();
	for (; itr; itr = itr->next) {
		/* Do not free itr->data. It's the stored as a key for the tree, and will
		 * be freed when the tree is destroyed. */
		gnt_tree_add_row_after(GNT_TREE(tree), itr->data,
				gnt_tree_create_row(GNT_TREE(tree), itr->data), NULL, NULL);
	}
	g_list_free(list);

	gnt_box_add_widget(GNT_BOX(box), tree);

	pref_dialog->new_profile = entry = gnt_entry_new("");
	gnt_box_add_widget(GNT_BOX(box), entry);

	tmpbox = gnt_hbox_new(FALSE);
	button = gnt_button_new("Add");
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(prof_add_cb), entry);
	gnt_box_add_widget(GNT_BOX(tmpbox), button);
	button = gnt_button_new("Delete");
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(prof_del_cb), NULL);
	gnt_box_add_widget(GNT_BOX(tmpbox), button);
	gnt_box_add_widget(GNT_BOX(box), tmpbox);
	gnt_box_add_widget(GNT_BOX(splitbox), box);

	gnt_box_add_widget(GNT_BOX(splitbox), gnt_vline_new());

	/* Sound method */

	box = gnt_vbox_new(FALSE);
	gnt_box_set_pad(GNT_BOX(box), 0);

	pref_dialog->method = cmbox = gnt_combo_box_new();
	gnt_tree_set_hash_fns(GNT_TREE(GNT_COMBO_BOX(cmbox)->dropdown), g_str_hash, g_str_equal, NULL);
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "automatic", _("Automatic"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "alsa", "ALSA");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "esd", "ESD");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "beep", _("Console Beep"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "custom", _("Command"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), "nosound", _("No Sound"));

	label = gnt_label_new_with_format(_("Sound Method"), GNT_TEXT_FLAG_BOLD);
	gnt_box_add_widget(GNT_BOX(box), label); 
	tmpbox = gnt_hbox_new(TRUE);
	gnt_box_set_fill(GNT_BOX(tmpbox), FALSE);
	gnt_box_set_pad(GNT_BOX(tmpbox), 0);
	gnt_box_add_widget(GNT_BOX(tmpbox), gnt_label_new(_("Method: ")));
	gnt_box_add_widget(GNT_BOX(tmpbox), cmbox);
	gnt_box_add_widget(GNT_BOX(box), tmpbox); 

	tmpbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(tmpbox), 0);
	gnt_box_set_fill(GNT_BOX(tmpbox), FALSE);
	gnt_box_add_widget(GNT_BOX(tmpbox), gnt_label_new(_("Sound Command\n(%s for filename)")));
	pref_dialog->command = entry = gnt_entry_new("");
	gnt_box_add_widget(GNT_BOX(tmpbox), entry);
	gnt_box_add_widget(GNT_BOX(box), tmpbox);

	gnt_box_add_widget(GNT_BOX(box), gnt_line_new(FALSE));

	/* Sound options */
	gnt_box_add_widget(GNT_BOX(box), gnt_label_new_with_format(_("Sound Options"), GNT_TEXT_FLAG_BOLD)); 
	pref_dialog->conv_focus = chkbox = gnt_check_box_new(_("Sounds when conversation has focus"));
	gnt_box_add_widget(GNT_BOX(box), chkbox);

	tmpbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(tmpbox), 0);
	gnt_box_set_fill(GNT_BOX(tmpbox), FALSE);
	gnt_box_add_widget(GNT_BOX(tmpbox), gnt_label_new("Enable Sounds:"));
	pref_dialog->while_status = cmbox = gnt_combo_box_new();
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), GINT_TO_POINTER(3), _("Always"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), GINT_TO_POINTER(1), _("Only when available"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox), GINT_TO_POINTER(2), _("Only when not available"));
	gnt_box_add_widget(GNT_BOX(tmpbox), cmbox);
	gnt_box_add_widget(GNT_BOX(box), tmpbox);

	tmpbox = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(tmpbox), 0);
	gnt_box_set_fill(GNT_BOX(tmpbox), FALSE);
	gnt_box_add_widget(GNT_BOX(tmpbox), gnt_label_new(_("Volume(0-100):")));

	pref_dialog->volume = slider = gnt_slider_new(FALSE, 100, 0);
	gnt_slider_set_step(GNT_SLIDER(slider), 5);
	gnt_slider_set_small_step(GNT_SLIDER(slider), 1);
	gnt_slider_set_large_step(GNT_SLIDER(slider), 20);
	label = gnt_label_new("");
	gnt_slider_reflect_label(GNT_SLIDER(slider), GNT_LABEL(label));
	gnt_box_set_pad(GNT_BOX(tmpbox), 1);
	gnt_box_add_widget(GNT_BOX(tmpbox), slider);
	gnt_box_add_widget(GNT_BOX(tmpbox), label);
	gnt_box_add_widget(GNT_BOX(box), tmpbox);
	gnt_box_add_widget(GNT_BOX(splitbox), box);

	gnt_box_add_widget(GNT_BOX(win), splitbox);

	gnt_box_add_widget(GNT_BOX(win), gnt_hline_new());

	/* Sound events */
	gnt_box_add_widget(GNT_BOX(win), gnt_label_new_with_format(_("Sound Events"), GNT_TEXT_FLAG_BOLD)); 
	pref_dialog->events = tree = gnt_tree_new_with_columns(2);
	gnt_tree_set_column_titles(GNT_TREE(tree), _("Event"), _("File"));
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);

	for (i = 0; i < PURPLE_NUM_SOUNDS; i++) {
		FinchSoundEvent * event = &sounds[i];

		if (event->label == NULL) {
			continue;
		}

		gnt_tree_add_choice(GNT_TREE(tree), GINT_TO_POINTER(i),
			gnt_tree_create_row(GNT_TREE(tree), event->label, event->def),
			NULL, NULL);
	}

	gnt_tree_adjust_columns(GNT_TREE(tree));
	gnt_box_add_widget(GNT_BOX(win), tree);

	box = gnt_hbox_new(FALSE);
	button = gnt_button_new(_("Test"));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(test_cb), NULL);
	gnt_box_add_widget(GNT_BOX(box), button);
	button = gnt_button_new(_("Reset"));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(reset_cb), NULL);
	gnt_box_add_widget(GNT_BOX(box), button);
	button = gnt_button_new(_("Choose..."));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(choose_cb), NULL);
	gnt_box_add_widget(GNT_BOX(box), button);
	gnt_box_add_widget(GNT_BOX(win), box);

	gnt_box_add_widget(GNT_BOX(win), gnt_line_new(FALSE));

	/* Add new stuff before this */
	box = gnt_hbox_new(FALSE);
	gnt_box_set_fill(GNT_BOX(box), TRUE);
	button = gnt_button_new(_("Save"));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(save_cb), win);
	gnt_box_add_widget(GNT_BOX(box), button);
	button = gnt_button_new(_("Cancel"));
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(cancel_cb), win);
	gnt_box_add_widget(GNT_BOX(box), button);
	gnt_box_add_widget(GNT_BOX(win), box);

	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(release_pref_dialog), NULL);

	load_pref_window(finch_sound_get_active_profile());

	gnt_widget_show(win);
}

gboolean finch_sound_is_enabled(void)
{
	const char *pref = make_pref("/method");
	const char *method = purple_prefs_get_string(pref);

	if (!method)
		return FALSE;
	if (strcmp(method, "nosound") == 0)
		return FALSE;
	if (purple_prefs_get_int(make_pref("/volume")) <= 0)
		return FALSE;

	return TRUE;
}

static PurpleSoundUiOps sound_ui_ops =
{
	finch_sound_init,
	finch_sound_uninit,
	finch_sound_play_file,
	finch_sound_play_event,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleSoundUiOps *
finch_sound_get_ui_ops(void)
{
	return &sound_ui_ops;
}


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

#ifdef USE_AO
# include <ao/ao.h>
# include <audiofile.h>
#endif /* USE_AO */

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
static gboolean sound_initialized = FALSE;

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
	{N_("Someone says your name in chat"), "nick_said", "alert.wav"}
};

#ifdef USE_AO
static int ao_driver = -1;
#endif /* USE_AO */

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
				   GaimConvChatBuddyFlags flags, GaimSoundEventID event)
{
	if (!chat_nick_matches_name(conv, name))
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

	chat = gaim_conversation_get_chat_data(conv);

	if (chat!=NULL && gaim_conv_chat_is_user_ignored(chat, sender))
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

static void
_pref_sound_method_changed(const char *name, GaimPrefType type,
						   gconstpointer val, gpointer data)
{
	if(type != GAIM_PREF_STRING || strcmp(name, "/gaim/gtk/sound/method"))
		return;

	sound_initialized = TRUE;

#ifdef USE_AO
	ao_driver = -1;

	if(!strcmp(val, "esd"))
		ao_driver = ao_driver_id("esd");
	else if(!strcmp(val, "arts"))
		ao_driver = ao_driver_id("arts");
	else if(!strcmp(val, "nas"))
		ao_driver = ao_driver_id("nas");
	else if(!strcmp(val, "automatic"))
		ao_driver = ao_default_driver_id();

	if(ao_driver != -1) {
		ao_info *info = ao_driver_info(ao_driver);
		gaim_debug_info("sound",
						"Sound output driver loaded: %s\n", info->name);
	}
#endif /* USE_AO */
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

#ifdef USE_AO
	gaim_debug_info("sound", "Initializing sound output drivers.\n");
	ao_initialize();
#endif /* USE_AO */

	gaim_prefs_connect_callback(gaim_gtk_sound_get_handle(), "/gaim/gtk/sound/method",
			_pref_sound_method_changed, NULL);

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
#ifdef USE_AO
	ao_shutdown();
#endif
	sound_initialized = FALSE;

	gaim_signals_disconnect_by_handle(gaim_gtk_sound_get_handle());
}

#ifdef USE_AO
static gboolean
expire_old_child(gpointer data)
{
	int ret;
	pid_t pid = GPOINTER_TO_INT(data);

	ret = waitpid(pid, NULL, WNOHANG | WUNTRACED);

	if(ret == 0) {
		if(kill(pid, SIGKILL) < 0)
			gaim_debug_error("gtksound", "Killing process %d failed (%s)\n",
							 pid, strerror(errno));
	}

    return FALSE; /* do not run again */
}

static void
scale_pcm_data(char *data, int nframes, ao_sample_format *format,
			   double intercept, double minclip, double maxclip,
			   float scale)
{
	int i;
	float v;
	gint16 *data16 = (gint16*)data;
	gint32 *data32 = (gint32*)data;
#ifdef G_HAVE_GINT64
	gint64 *data64 = (gint64*)data;
#endif

	switch(format->bits) {
		case 16:
			for(i = 0; i < nframes * format->channels; i++) {
				v = ((data16[i] - intercept) * scale) + intercept;
				v = CLAMP(v, minclip, maxclip);
				data16[i]=(gint16)v;
			}
			break;
		case 32:
			for(i = 0; i < nframes * format->channels; i++) {
				v = ((data32[i] - intercept) * scale) + intercept;
				v = CLAMP(v, minclip, maxclip);
				data32[i]=(gint32)v;
			}
			break;
#ifdef G_HAVE_GINT64
		case 64:
			for(i = 0; i < nframes * format->channels; i++) {
				v = ((data64[i] - intercept) * scale) + intercept;
				v = CLAMP(v, minclip, maxclip);
				data64[i]=(gint64)v;
			}
			break;
#endif
		default:
			gaim_debug_warning("gtksound", "Cannot scale %d bit pcm data.\n", format->bits);
			break;
	}
}
#endif /* USE_AO */

static void
gaim_gtk_sound_play_file(const char *filename)
{
	const char *method;
#ifdef USE_AO
	pid_t pid;
	AFfilehandle file;
	int volume = 50;
#endif

	if (!sound_initialized)
		gaim_prefs_trigger_callback("/gaim/gtk/sound/method");

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
#ifdef USE_AO
	volume = gaim_prefs_get_int("/gaim/gtk/sound/volume");
	volume = CLAMP(volume, 0, 100);

	pid = fork();
	if (pid < 0)
		return;
	else if (pid == 0) {
		/* Child process */
		float scale = ((float) volume * volume) / 2500;
		file = afOpenFile(filename, "rb", NULL);
		if(file) {
			ao_device *device;
			ao_sample_format format;
			int in_fmt;
			int bytes_per_frame;
			double slope, intercept, minclip, maxclip;

			format.rate = afGetRate(file, AF_DEFAULT_TRACK);
			format.channels = afGetChannels(file, AF_DEFAULT_TRACK);
			afGetSampleFormat(file, AF_DEFAULT_TRACK, &in_fmt,
					&format.bits);

			afGetPCMMapping(file, AF_DEFAULT_TRACK, &slope,
					&intercept, &minclip, &maxclip);

			/* XXX: libao doesn't seem to like 8-bit sounds, so we'll
			 * let libaudiofile make them a bit better for us */
			if(format.bits == 8)
				format.bits = 16;

			afSetVirtualSampleFormat(file, AF_DEFAULT_TRACK,
					AF_SAMPFMT_TWOSCOMP, format.bits);

#if G_BYTE_ORDER == G_BIG_ENDIAN
			format.byte_format = AO_FMT_BIG;
			afSetVirtualByteOrder(file, AF_DEFAULT_TRACK,
					AF_BYTEORDER_BIGENDIAN);
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
			format.byte_format = AO_FMT_LITTLE;
			afSetVirtualByteOrder(file, AF_DEFAULT_TRACK,
					AF_BYTEORDER_LITTLEENDIAN);
#else
#warning Unknown endianness
#endif

			bytes_per_frame = format.bits * format.channels / 8;

			device = ao_open_live(ao_driver, &format, NULL);

			if(device) {
				int frames_read;
				char buf[4096];
				int buf_frames = sizeof(buf) / bytes_per_frame;

				while((frames_read = afReadFrames(file, AF_DEFAULT_TRACK,
								buf, buf_frames))) {
					if(volume != 50)
						scale_pcm_data(buf, frames_read, &format, intercept,
									   minclip, maxclip, scale);
					if(!ao_play(device, buf, frames_read * bytes_per_frame))
						break;
				}
				ao_close(device);
			}
			afCloseFile(file);
		}
		ao_shutdown();
		_exit(0);
	} else {
		/* Parent process */
		gaim_timeout_add(PLAY_SOUND_TIMEOUT, expire_old_child, GINT_TO_POINTER(pid));
	}
#else /* USE_AO */
	gdk_beep();
	return;
#endif /* USE_AO */
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

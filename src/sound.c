/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#ifdef USE_AO
#include <ao/ao.h>
#include <audiofile.h>
#endif /* USE_AO */
#ifdef USE_NAS_AUDIO
#include <audio/audiolib.h>
#include <audio/soundlib.h>
#endif /* USE_NAS_AUDIO */

#include "gaim.h"
#include "sound.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

struct gaim_sound_event {
	char *label;
	guint opt;
	char *def;
};

#ifdef USE_AO
static gboolean ao_initialized=FALSE;
static int ao_driver = -1;
#endif /* USE_AO */


static gboolean mute_login_sounds = FALSE;
static gboolean mute_sounds = FALSE;
static char *sound_cmd = NULL;

/* description, option bit, default sound file             *
 * set the option bit to 0 to have it not display in prefs *
 * the order here has to match the defines in gaim.h.      *
 *                                               -Robot101 */
static struct gaim_sound_event sounds[GAIM_NUM_SOUNDS] = {
	{N_("Buddy logs in"), OPT_SOUND_LOGIN, "arrive.wav"},
	{N_("Buddy logs out"), OPT_SOUND_LOGOUT, "leave.wav"},
	{N_("Message received"), OPT_SOUND_RECV, "receive.wav"},
	{N_("Message received begins conversation"), OPT_SOUND_FIRST_RCV, "receive.wav"},
	{N_("Message sent"), OPT_SOUND_SEND, "send.wav"},
	{N_("Person enters chat"), OPT_SOUND_CHAT_JOIN, "arrive.wav"},
	{N_("Person leaves chat"), OPT_SOUND_CHAT_PART, "leave.wav"},
	{N_("You talk in chat"), OPT_SOUND_CHAT_YOU_SAY, "send.wav"},
	{N_("Others talk in chat"), OPT_SOUND_CHAT_SAY, "receive.wav"},
	/* this isn't a terminator, it's the buddy pounce default sound event ;-) */
	{NULL, 0, "redalert.wav"},
	{N_("Someone says your name in chat"), OPT_SOUND_CHAT_NICK, "redalert.wav"}
};

static char *sound_file[GAIM_NUM_SOUNDS];


#ifdef USE_AO
static void check_ao_init()
{
	if(!ao_initialized) {
		gaim_debug(GAIM_DEBUG_INFO, "sound",
				   "Initializing sound output drivers.\n");
		ao_initialize();
		ao_initialized = TRUE;
	}
}
#endif /* USE_AO */

void gaim_sound_change_output_method() {
#ifdef USE_AO
	ao_driver = -1;

	if ((sound_options & OPT_SOUND_ESD) || (sound_options & OPT_SOUND_ARTS) ||
			(sound_options & OPT_SOUND_NORMAL)) {
		check_ao_init();
		if (ao_driver == -1 && (sound_options & OPT_SOUND_ESD)) {
			ao_driver = ao_driver_id("esd");
		}
		if(ao_driver == -1 && (sound_options & OPT_SOUND_ARTS)) {
			ao_driver = ao_driver_id("arts");
		}
		if (ao_driver == -1) {
			ao_driver = ao_default_driver_id();
		}
	}
	if(ao_driver != -1) {
		ao_info *info = ao_driver_info(ao_driver);
		gaim_debug(GAIM_DEBUG_INFO, "sound",
				   "Sound output driver loaded: %s\n", info->name);
	}
#endif /* USE_AO */
#ifdef USE_NAS
	if((sound_options & OPT_SOUND_NAS))
		gaim_debug(GAIM_DEBUG_INFO, "sound",
				   "Sound output driver loaded: NAS output\n");
#endif /* USE_NAS */
}

void gaim_sound_quit()
{
#ifdef USE_AO
	if(ao_initialized)
		ao_shutdown();
#endif
}


#ifdef USE_NAS_AUDIO
static gboolean play_file_nas(const char *filename)
{
	AuServer *nas_serv;
	gboolean ret = FALSE;

	if((nas_serv = AuOpenServer(NULL, 0, NULL, 0, NULL, NULL))) {
		ret = AuSoundPlaySynchronousFromFile(nas_serv, filename, 100);
		AuCloseServer(nas_serv);
	}

	return ret;
}

#endif /* USE_NAS_AUDIO */

void gaim_sound_play_file(char *filename)
{
#if defined(USE_NAS_AUDIO) || defined(USE_AO)
	pid_t pid;
#ifdef USE_AO
	AFfilehandle file;
#endif
#endif

	if (mute_sounds)
		return;

	if (awaymessage && !(sound_options & OPT_SOUND_WHEN_AWAY))
		return; /* check here in case a buddy pounce plays a file while away */

	if (sound_options & OPT_SOUND_BEEP) {
		gdk_beep();
		return;
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		char *tmp = g_strdup_printf(_("Unable to play sound because the chosen filename (%s) does not exist."), filename);
		do_error_dialog(tmp, NULL, GAIM_ERROR);
		g_free(tmp);
		return;
	}

#ifndef _WIN32
	if ((sound_options & OPT_SOUND_CMD)) {
		char *command;
		GError *error = NULL;

		if(!sound_cmd) {
			do_error_dialog(_("Unable to play sound because the 'Command' sound method has been chosen, but no command has been set."), NULL, GAIM_ERROR);
			return;
		}

		command = g_strdup_printf(sound_cmd, filename);

		if(!g_spawn_command_line_async(command, &error)) {
			char *tmp = g_strdup_printf(_("Unable to play sound because the configured sound command could not be launched: %s"), error->message);
			do_error_dialog(tmp, NULL, GAIM_ERROR);
			g_free(tmp);
			g_error_free(error);
		}

		g_free(command);
		return;
	}
#if defined(USE_NAS_AUDIO) || defined(USE_AO)
	pid = fork();
	if (pid < 0)
		return;
	else if (pid == 0) {
#ifdef USE_NAS_AUDIO
		if ((sound_options & OPT_SOUND_NAS)) {
			if (play_file_nas(filename))
				_exit(0);
		}
#endif /* USE_NAS_AUDIO */

#ifdef USE_AO
		file = afOpenFile(filename, "rb", NULL);
		if(file) {
			ao_device *device;
			ao_sample_format format;
			int in_fmt;
			int bytes_per_frame;

			format.rate = afGetRate(file, AF_DEFAULT_TRACK);
			format.channels = afGetChannels(file, AF_DEFAULT_TRACK);
			afGetSampleFormat(file, AF_DEFAULT_TRACK, &in_fmt,
					&format.bits);

			/* XXX: libao doesn't seem to like 8-bit sounds, so we'll
			 * let libaudiofile make them a bit better for us */
			if(format.bits == 8)
				format.bits = 16;

			afSetVirtualSampleFormat(file, AF_DEFAULT_TRACK,
					AF_SAMPFMT_TWOSCOMP, format.bits);

#if __BYTE_ORDER == __BIG_ENDIAN
			format.byte_format = AO_FMT_BIG;
			afSetVirtualByteOrder(file, AF_DEFAULT_TRACK,
					AF_BYTEORDER_BIGENDIAN);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
			format.byte_format = AO_FMT_LITTLE;
			afSetVirtualByteOrder(file, AF_DEFAULT_TRACK,
					AF_BYTEORDER_LITTLEENDIAN);
#endif

			bytes_per_frame = format.bits * format.channels / 8;

			device = ao_open_live(ao_driver, &format, NULL);

			if(device) {
				int frames_read;
				char buf[4096];
				int buf_frames = sizeof(buf) / bytes_per_frame;

				while((frames_read = afReadFrames(file, AF_DEFAULT_TRACK,
								buf, buf_frames))) {
					if(!ao_play(device, buf, frames_read * bytes_per_frame))
						break;
				}
				ao_close(device);
			}
			afCloseFile(file);
		}
		ao_shutdown();
#endif /* USE_AO */
		_exit(0);
	}
#else /* USE_NAS_AUDIO || USE_AO */
	gdk_beep();
	return;
#endif /* USE_NAS_AUDIO || USE_AO */
#else /* _WIN32 */
	gaim_debug(GAIM_DEBUG_INFO, "sound", "Playing %s\n", filename);

	if (!PlaySound(filename, 0, SND_ASYNC | SND_FILENAME))
		gaim_debug(GAIM_DEBUG_ERROR, "sound", "Error playing sound.\n");
#endif /* _WIN32 */
}

void gaim_sound_play_event(GaimSoundEventID event)
{
	if ((event == GAIM_SOUND_BUDDY_ARRIVE) && mute_login_sounds)
		return;

	if (event >= GAIM_NUM_SOUNDS) {
		gaim_debug(GAIM_DEBUG_MISC, "sound",
				   "got request for unknown sound: %d\n", event);
		return;
	}

	/* check NULL for sounds that don't have an option, ie buddy pounce */
	if ((sound_options & sounds[event].opt) || (sounds[event].opt == 0)) {
		if (sound_file[event]) {
			gaim_sound_play_file(sound_file[event]);
		} else {
			gchar *filename = NULL;

			filename = g_build_filename(DATADIR, "sounds", "gaim", sounds[event].def, NULL);
			gaim_sound_play_file(filename);
			g_free(filename);
		}
	}
}

void gaim_sound_set_mute(gboolean mute)
{
	mute_sounds = mute;
}

gboolean gaim_sound_get_mute()
{
	return mute_sounds;
}

void gaim_sound_set_login_mute(gboolean mute)
{
	mute_login_sounds = mute;
}

void gaim_sound_set_event_file(GaimSoundEventID event, const char *filename)
{
	if(event >= GAIM_NUM_SOUNDS)
		return;

	if(sound_file[event])
		g_free(sound_file[event]);

	sound_file[event] = g_strdup(filename);
}


char *gaim_sound_get_event_file(GaimSoundEventID event)
{
	if(event >= GAIM_NUM_SOUNDS)
		return NULL;

	return sound_file[event];
}

guint gaim_sound_get_event_option(GaimSoundEventID event)
{
	if(event >= GAIM_NUM_SOUNDS)
		return 0;

	return sounds[event].opt;
}

char *gaim_sound_get_event_label(GaimSoundEventID event)
{
	if(event >= GAIM_NUM_SOUNDS)
		return NULL;

	return sounds[event].label;
}


void gaim_sound_set_command(const char *cmd)
{
	if(sound_cmd)
		g_free(sound_cmd);
	if(strlen(cmd) > 0)
		sound_cmd = g_strdup(cmd);
	else
		sound_cmd = NULL;
}

char *gaim_sound_get_command()
{
	return sound_cmd;
}


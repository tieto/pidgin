/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <windows.h>
#include <mmsystem.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef USE_AO
#include <ao/ao.h>
#include <audiofile.h>
#endif /* USE_AO */

#include "gaim.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#ifdef USE_AO
static gboolean ao_initialized = FALSE;
#endif /* USE_AO */

gboolean mute_sounds = 0;

/* description, option bit, default sound file             *
 * set the option bit to 0 to have it not display in prefs *
 * the order here has to match the defines in gaim.h.      *
 *                                               -Robot101 */
struct sound_struct sounds[NUM_SOUNDS] = {
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

void play_file(char *filename)
{
#ifndef _WIN32
#ifdef USE_AO
	int ao_driver;
#endif /* USE_AO */
	pid_t pid;
#endif

	if (awaymessage && !(sound_options & OPT_SOUND_WHEN_AWAY))
		return; /* check here in case a buddy pounce plays a file while away */

	if (sound_options & OPT_SOUND_BEEP) {
		gdk_beep();
		return;
	}

#ifndef _WIN32
	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		gchar *tmp = g_strdup_printf(_("Unable to play sound because the chosen filename (%s) does not exist."), filename);
		do_error_dialog(tmp, NULL, GAIM_ERROR);
		g_free(tmp);
		return;
	}

	if (sound_options & OPT_SOUND_CMD) {
		gchar *command = NULL;
		GError *error = NULL;

		if (!sound_cmd[0]) {
			do_error_dialog(_("Unable to play sound because the 'Command' sound method has been chosen, but no command has been set."), NULL, GAIM_ERROR);
			return;
		}

		command = g_strdup_printf(sound_cmd, filename);

		if (g_spawn_command_line_async(command, &error) == FALSE) {
			gchar *tmp = g_strdup_printf(_("Unable to play sound because the configured sound command could not be launched: %s"), error->message);
			do_error_dialog(tmp, NULL, GAIM_ERROR);
			g_free(tmp);
			g_error_free(error);
		}

		g_free(command);
		return;
	}

#ifdef USE_AO
	if (!ao_initialized) {
		ao_initialize();
	}

	ao_driver = ao_default_driver_id();

	if (ao_driver == -1) {
		do_error_dialog(_("Unable to play sound because no suitable driver could be found."), NULL, GAIM_ERROR);
		return;
	}

	pid = fork();

	if (pid < 0)
		return;
	else if (pid == 0) {
		AFfilehandle file = afOpenFile(filename, "rb", NULL);
		if(file) {
			ao_device *device;
			ao_sample_format format;

			int in_fmt;
			int bytes_per_frame;

			format.rate = afGetRate(file, AF_DEFAULT_TRACK);
			format.channels = afGetChannels(file, AF_DEFAULT_TRACK);
			afGetSampleFormat(file, AF_DEFAULT_TRACK, &in_fmt,
					&format.bits);

			bytes_per_frame = format.bits * format.channels / 8;

			device = ao_open_live(ao_driver, &format, NULL);

			if (device) {
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

			ao_shutdown();
			afCloseFile(file);
		}
		_exit(0);
	}
#else /* USE_AO */
	gdk_beep();
#endif /* USE_AO */

#else /* _WIN32 */
	debug_printf("Playing %s\n", filename);
	if (!PlaySound(filename, 0, SND_ASYNC | SND_FILENAME))
		debug_printf("Error playing sound.");
#endif
}

void sound_quit() {
#ifdef USE_AO
	if (ao_initialized) {
		ao_shutdown();
	}
#endif
}

extern int logins_not_muted;

void play_sound(int sound)
{
	if (mute_sounds)
		return;

	if ((sound == SND_BUDDY_ARRIVE) && !logins_not_muted)
		return;

	if (sound >= NUM_SOUNDS) {
		debug_printf("got request for unknown sound: %d\n", sound);
		return;
	}

	/* check NULL for sounds that don't have an option, ie buddy pounce */
	if ((sound_options & sounds[sound].opt) || (sounds[sound].opt == 0)) {
		if (sound_file[sound]) {
			play_file(sound_file[sound]);
		} else {
			gchar *filename = NULL;

			filename = g_build_filename(DATADIR, "sounds", "gaim", sounds[sound].def, NULL);
			play_file(filename);
			g_free(filename);
		}
	}
}

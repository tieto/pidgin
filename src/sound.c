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

#ifdef ESD_SOUND
#include <esd.h>
#endif

#ifdef NAS_SOUND
#include <audio/audiolib.h>
#endif

#include "gaim.h"

gboolean mute_sounds = 0;

/* description, option bit, default sound file                    *
 * if you want it to get displayed in the prefs dialog, it needs  *
 * to be added to the sound_order array in prefs.c, if not, and   *
 * it has no option bit, set it to 0. the order here has to match *
 * the defines in gaim.h.                               -Robot101 */
struct sound_struct sounds[NUM_SOUNDS] = {
	{N_("Buddy logs in"), OPT_SOUND_LOGIN, "BuddyArrive.wav"},
	{N_("Buddy logs out"), OPT_SOUND_LOGOUT, "BuddyLeave.wav"},
	{N_("Message received"), OPT_SOUND_RECV, "Receive.wav"},
	{N_("Message received begins conversation"), OPT_SOUND_FIRST_RCV, "Receive.wav"},
	{N_("Message sent"), OPT_SOUND_SEND, "Send.wav"},
	{N_("Person enters chat"), OPT_SOUND_CHAT_JOIN, "BuddyArrive.wav"},
	{N_("Person leaves chat"), OPT_SOUND_CHAT_PART, "BuddyLeave.wav"},
	{N_("You talk in chat"), OPT_SOUND_CHAT_YOU_SAY, "Send.wav"},
	{N_("Others talk in chat"), OPT_SOUND_CHAT_SAY, "Receive.wav"},
	/* this isn't a terminator, it's the buddy pounce default sound event ;-) */
	{NULL, 0, "RedAlert.wav"},
	{N_("Someone says your name in chat"), OPT_SOUND_CHAT_NICK, "RedAlert.wav"}
};
int sound_order[] = {
	SND_BUDDY_ARRIVE, SND_BUDDY_LEAVE,
	SND_FIRST_RECEIVE, SND_RECEIVE, SND_SEND,
	SND_CHAT_JOIN, SND_CHAT_LEAVE,
	SND_CHAT_YOU_SAY, SND_CHAT_SAY, SND_CHAT_NICK, 0
};

#ifndef _WIN32
static int check_dev(char *dev)
{
	struct stat stat_buf;
	uid_t user = getuid();
	gid_t group = getgid(), other_groups[32];
	int i, numgroups;

	if ((numgroups = getgroups(32, other_groups)) == -1)
		return 0;
	if (stat(dev, &stat_buf))
		return 0;
	if (user == stat_buf.st_uid && stat_buf.st_mode & S_IWUSR)
		return 1;
	if (stat_buf.st_mode & S_IWGRP) {
		if (group == stat_buf.st_gid)
			return 1;
		for (i = 0; i < numgroups; i++)
			if (other_groups[i] == stat_buf.st_gid)
				return 1;
	}
	if (stat_buf.st_mode & S_IWOTH)
		return 1;
	return 0;
}

static void play_audio_file(char *file)
{
	/* here we can assume that we can write to /dev/audio */
	char *buf;
	struct stat info;
	int fd = open(file, O_RDONLY);
	if (fd <= 0) {
		return;
	}
	fstat(fd, &info);
	if (info.st_size < 24)
		return;
	buf = malloc(info.st_size + 1);
	read(fd, buf, 24);
	read(fd, buf, info.st_size - 24);
	close(fd);

	fd = open("/dev/audio", O_WRONLY | O_EXCL | O_NDELAY);
	if (fd < 0) {
		free(buf);
		return;
	}
	write(fd, buf, info.st_size - 24);
	free(buf);
	close(fd);
}

static int can_play_audio()
{
	return check_dev("/dev/audio");
}

#ifdef ESD_SOUND

int esd_fd;

static int can_play_esd()
{
	esd_format_t format = ESD_BITS16 | ESD_STREAM | ESD_PLAY | ESD_MONO;

	esd_fd = esd_play_stream(format, 8012, NULL, "gaim");

	if (esd_fd < 0) {
		return 0;
	}

	return 1;
}

#endif

#ifdef NAS_SOUND

char nas_server[] = "localhost";
AuServer *nas_serv = NULL;

static AuBool NasEventHandler(AuServer * aud, AuEvent * ev, AuEventHandlerRec * handler)
{
	AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;

	if (ev->type == AuEventTypeElementNotify) {
		switch (event->kind) {
		case AuElementNotifyKindState:
			switch (event->cur_state) {
			case AuStateStop:
				_exit(0);
			}
			break;
		}
	}
	return AuTrue;
}

static int can_play_nas()
{
	if ((nas_serv = AuOpenServer(NULL, 0, NULL, 0, NULL, NULL)))
		return 1;
	return 0;
}

static int play_nas_file(char *file)
{
	struct stat stat_buf;
	char *buf;
	int ret;
	int fd = open(file, O_RDONLY);
	if (fd <= 0)
		return 0;

	if (!can_play_nas())
		return 0;

	if (stat(file, &stat_buf))
		return 0;

	if (!stat_buf.st_size)
		return 0;

	buf = malloc(stat_buf.st_size);
	read(fd, buf, stat_buf.st_size);
	ret = play_nas(buf, stat_buf.st_size);
	free(buf);
	return ret;
}

#endif
#endif /* !_WIN32 */

void play_file(char *filename)
{
#ifndef _WIN32
	int pid;
#endif
	if (awaymessage && !(sound_options & OPT_SOUND_WHEN_AWAY))
		return; /* check here in case a buddy pounce plays a file while away */
	
	if (sound_options & OPT_SOUND_BEEP) {
		gdk_beep();
		return;
	}

	else if (sound_options & OPT_SOUND_NORMAL) {
		debug_printf("attempting to play audio file with internal method -- this is unlikely to work\n");
	}
#ifndef _WIN32
	pid = fork();

	if (pid < 0)
		return;
	else if (pid == 0) {
		alarm(30);
		
		if ((sound_options & OPT_SOUND_CMD) && sound_cmd[0]) {
			char *args[4];
			char command[4096];

			g_snprintf(command, sizeof(command), sound_cmd, filename);

			args[0] = "sh";
			args[1] = "-c";
			args[2] = command;
			args[3] = NULL;
			execvp(args[0], args);
			_exit(0);
		}
#ifdef ESD_SOUND
		else if (sound_options & OPT_SOUND_ESD) {
			if (esd_play_file(NULL, filename, 1))
				_exit(0);
		}
#endif

		else if (sound_options & OPT_SOUND_ARTSC) {
			char *args[3];
			args[0] = "artsplay";
			args[1] = filename;
			args[2] = NULL;
			execvp(args[0], args);
			_exit(0);
		}

#ifdef NAS_SOUND
		else if (sound_options & OPT_SOUND_NAS) {
			if (play_nas_file(filename))
				_exit(0);
		}
#endif
		else if ((sound_options & OPT_SOUND_NORMAL) && 
			 can_play_audio()) {
			play_audio_file(filename);
			_exit(0);
		}

		_exit(0);
	}
#else /* _WIN32 */
	debug_printf("Playing %s\n", filename);
	if (!PlaySound(filename, 0, SND_ASYNC | SND_FILENAME))
		debug_printf("Error playing sound.");
#endif
}

extern int logins_not_muted;

void play_sound(int sound)
{
	if (mute_sounds)
		return;
	
	if (awaymessage && !(sound_options & OPT_SOUND_WHEN_AWAY))
		return;

	if ((sound == SND_BUDDY_ARRIVE) && !logins_not_muted)
		return;

	if (sound >= NUM_SOUNDS) {
		debug_printf("sorry old fruit... can't say I know that sound: ", sound, "\n");
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

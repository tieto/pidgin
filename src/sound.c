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
#include "../config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef ESD_SOUND
#include <esd.h>
#endif

#ifdef NAS_SOUND
#include <audio/audiolib.h>
#endif

#include "gaim.h"
#include "sounds/BuddyArrive.h"
#include "sounds/BuddyLeave.h"
#include "sounds/Send.h"
#include "sounds/Receive.h"

static int check_dev(char *dev)
{
	struct stat stat_buf;
	uid_t user = getuid();
	gid_t group = getgid(), 
	      other_groups[32];
	int i, numgroups;

	if ((numgroups = getgroups (32, other_groups)) == -1)
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


static void play_audio(char *data, int size)
{
        int fd;
        
        fd = open("/dev/audio", O_WRONLY | O_EXCL);
        if (fd < 0)
                return;
        write(fd, data, size);
        close(fd);
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

	fd = open("/dev/audio", O_WRONLY | O_EXCL);
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
/*
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
** Z-note -- this is from libaudiofile.  Thanks guys!
*/

int _af_ulaw2linear (unsigned char ulawbyte)
{
  static int exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
  int sign, exponent, mantissa, sample;

  ulawbyte = ~ulawbyte;
  sign = (ulawbyte & 0x80);
  exponent = (ulawbyte >> 4) & 0x07;
  mantissa = ulawbyte & 0x0F;
  sample = exp_lut[exponent] + (mantissa << (exponent + 3));
  if (sign != 0) sample = -sample;

  return(sample);
}


int esd_fd;

static int play_esd(unsigned char *data, int size)
{
        int i;
        guint16 *lineardata;
	
        lineardata = g_malloc(size * 2);

	for (i=0; i<size; i++)
		lineardata[i] = _af_ulaw2linear(data[i]);
	
	write(esd_fd, lineardata, size * 2);

	close(esd_fd);
	g_free(lineardata);

        return 1;

}

static int play_esd_file(char *file)
{
	int esd_stat;
	int fd = open(file, O_RDONLY);
	if (fd <= 0)
		return 0;
	esd_stat = esd_play_file(NULL, file, 1);
	return esd_stat;
}

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

static AuBool
NasEventHandler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *handler)
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


static int play_nas(unsigned char *data, int size)
{
        AuDeviceID device = AuNone;
        AuFlowID flow;
        AuElement elements[3];
        int i, n, w;

        /* look for an output device */
        for (i = 0; i < AuServerNumDevices(nas_serv); i++) {
                if ((AuDeviceKind(AuServerDevice(nas_serv, i)) ==
                     AuComponentKindPhysicalOutput) &&
                    AuDeviceNumTracks(AuServerDevice(nas_serv, i)) == 1) {
                        device = AuDeviceIdentifier(AuServerDevice(nas_serv, i));
                        break;
                }
        }

        if (device == AuNone)
                return 0;

        if (!(flow = AuCreateFlow(nas_serv, NULL)))
                return 0;


        AuMakeElementImportClient(&elements[0], 8012, AuFormatULAW8,
                                  1, AuTrue, size, size/2, 0, NULL);
        AuMakeElementExportDevice(&elements[1], 0, device, 8012,
                                  AuUnlimitedSamples, 0, NULL);
        AuSetElements(nas_serv, flow, AuTrue, 2, elements, NULL);

        AuStartFlow(nas_serv, flow, NULL);

        AuWriteElement(nas_serv, flow, 0, size, data, AuTrue, NULL);

        AuRegisterEventHandler(nas_serv, AuEventHandlerIDMask, 0, flow,
                               NasEventHandler, NULL);
        
        while(1) {
                AuHandleEvents(nas_serv);
        }

        return 1;
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

void play_file(char *filename) {
	int pid;

#ifdef _WIN32
	return;
#endif

	pid = fork();

	if (pid < 0)
		return;
	else if (pid == 0) {
		if (sound_options & OPT_SOUND_BEEP) {
			printf("\a");
			fflush(stdout);
			_exit(0);
		}

		if (sound_cmd[0]) {
			char *args[4];
			char command[4096];
			g_snprintf(command, sizeof command, sound_cmd, filename);
			args[0] = "sh";
			args[1] = "-c";
			args[2] = command;
			args[3] = NULL;
			execvp(args[0], args);
			_exit(0);
		}

#ifdef ESD_SOUND
		if (play_esd_file(filename))
			_exit(0);
#endif

#ifdef NAS_SOUND
		if (play_nas_file(filenae))
			_exit(0);
#endif

		if (can_play_audio()) {
			play_audio_file(filename);
			_exit(0);
		}

		_exit(0);
	} else {
		gtk_timeout_add(100, (GtkFunction)clean_pid, NULL);
	}
}

void play(unsigned char *data, int size)
{
        int pid;

#ifdef _WIN32
        return;
#endif
        
	pid = fork();
	
	if (pid < 0)
		return;
        else if (pid == 0) {
		if (sound_options & OPT_SOUND_BEEP) {
			printf("\a");
			fflush(stdout);
			_exit(0);
		}

#ifdef ESD_SOUND
                /* ESD is our player of choice.  Are we OK to
                 * go there? */
                if (can_play_esd()) {
                        if (play_esd(data, size))
                                _exit(0);
                }
#endif

#ifdef NAS_SOUND
                /* NAS is our second choice setup. */
                if (can_play_nas()) {
                        if (play_nas(data, size))
                                _exit(0);
                }
#endif

                /* Lastly, we can try just plain old /dev/audio */
                if (can_play_audio()) {
                        play_audio(data, size);
                        _exit(0);
                }

		_exit(0);
        } else {
                gtk_timeout_add(100, (GtkFunction)clean_pid, NULL);
        }
}

extern int logins_not_muted;

void play_sound(int sound)
{

	switch(sound) {
	case BUDDY_ARRIVE:
		if ((sound_options & OPT_SOUND_LOGIN) && logins_not_muted) {
			if (sound_file[BUDDY_ARRIVE]) {
				play_file(sound_file[BUDDY_ARRIVE]);
			} else {
				play(BuddyArrive, sizeof(BuddyArrive));
			}
		}
		break;
	case BUDDY_LEAVE:
		if (sound_options & OPT_SOUND_LOGOUT) {
			if (sound_file[BUDDY_LEAVE]) {
				play_file(sound_file[BUDDY_LEAVE]);
			} else {
				play(BuddyLeave, sizeof(BuddyLeave));
			}
		}
		break;
        case FIRST_RECEIVE:
		if (sound_options & OPT_SOUND_FIRST_RCV) {
			if (sound_file[FIRST_RECEIVE]) {
				play_file(sound_file[FIRST_RECEIVE]);
			} else {
				play(Receive, sizeof(Receive));
			}
		}
		break;
        case RECEIVE:
		if (sound_options & OPT_SOUND_RECV) {
			if (sound_file[RECEIVE]) {
				play_file(sound_file[RECEIVE]);
			} else {
				play(Receive, sizeof(Receive));
			}
		}
		break;
	case SEND:
		if (sound_options & OPT_SOUND_SEND) {
			if (sound_file[SEND]) {
				play_file(sound_file[SEND]);
			} else {
				play(Send, sizeof(Send));
			}
		}
		break;
        case CHAT_JOIN:
		if (sound_options & OPT_SOUND_CHAT_JOIN) {
			if (sound_file[CHAT_JOIN]) {
				play_file(sound_file[CHAT_JOIN]);
			} else {
				play(BuddyArrive, sizeof(BuddyArrive));
			}
		}
		break;
        case CHAT_LEAVE:
		if (sound_options & OPT_SOUND_CHAT_PART) {
			if (sound_file[CHAT_LEAVE]) {
				play_file(sound_file[CHAT_LEAVE]);
			} else {
				play(BuddyLeave, sizeof(BuddyLeave));
			}
		}
		break;
        case CHAT_YOU_SAY:
		if (sound_options & OPT_SOUND_CHAT_YOU_SAY) {
			if (sound_file[CHAT_YOU_SAY]) {
				play_file(sound_file[CHAT_YOU_SAY]);
			} else {
				play(Send, sizeof(Send));
			}
		}
		break;
        case CHAT_SAY:
		if (sound_options & OPT_SOUND_CHAT_SAY) {
			if (sound_file[CHAT_SAY]) {
				play_file(sound_file[CHAT_SAY]);
			} else {
				play(Receive, sizeof(Receive));
			}
		}
		break;
       }
}


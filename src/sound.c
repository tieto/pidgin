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


static void play_audio(char *data, int size)
{
        int fd;
        
        fd = open("/dev/audio", O_WRONLY | O_EXCL);
        if (fd < 0)
                return;
        write(fd, data, size);
        close(fd);
}

static int can_play_audio()
{
	struct stat stat_buf;
	uid_t user = getuid();
	gid_t group = getgid(); 
	if (stat("/dev/audio", &stat_buf))
		return 0;
	if (user == stat_buf.st_uid && stat_buf.st_mode & S_IWUSR)
		return 1;
	if (group == stat_buf.st_gid && stat_buf.st_mode & S_IWGRP)
		return 1;
	if (stat_buf.st_mode & S_IWOTH)
		return 1;
	return 0;
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



static int play_esd(unsigned char *data, int size)
{
        int fd, i;
	esd_format_t format = ESD_BITS16 | ESD_STREAM | ESD_PLAY | ESD_MONO;
        guint16 *lineardata;
	
	
        fd = esd_play_stream_fallback(format, 8012, NULL, "gaim");

        if (fd < 0) {
                return 0;
	}

        lineardata = g_malloc(size * 2);

	for (i=0; i<size; i++)
		lineardata[i] = _af_ulaw2linear(data[i]);
	
	write(fd, lineardata, size * 2);

	close(fd);
	g_free(lineardata);

        return 1;

}

static int can_play_esd()
{
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

#endif

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
        } else {
                gtk_timeout_add(100, (GtkFunction)clean_pid, NULL);
        }
}

extern int logins_not_muted;

void play_sound(int sound)
{

	switch(sound) {
	case BUDDY_ARRIVE:
		if ((sound_options & OPT_SOUND_LOGIN) && logins_not_muted)
			play(BuddyArrive, sizeof(BuddyArrive));
		break;
	case BUDDY_LEAVE:
		if (sound_options & OPT_SOUND_LOGOUT)
			play(BuddyLeave, sizeof(BuddyLeave));
		break;
	case SEND:
		if (sound_options & OPT_SOUND_SEND)
			play(Send, sizeof(Send));
		break;
        case FIRST_RECEIVE:
		if (sound_options & OPT_SOUND_FIRST_RCV)
			play(Receive, sizeof(Receive));
		break;
        case RECEIVE:
		if (sound_options & OPT_SOUND_RECV)
			play(Receive, sizeof(Receive));
		break;
	case AWAY:
		if (sound_options & OPT_SOUND_WHEN_AWAY)
			play(Receive, sizeof(Receive));
		break;
       }
}

#ifdef USE_APPLET

#include <gnome.h>
void applet_play_sound(int sound)
{

	if (!(sound_options & OPT_SOUND_THROUGH_GNOME)) {
		play_sound(sound);
		return;
	}

	switch(sound) {
	case BUDDY_ARRIVE:
		if ((sound_options & OPT_SOUND_LOGIN) && logins_not_muted)
			gnome_triggers_do("", "program", "gaim_applet", "login", NULL);
		break;
	case BUDDY_LEAVE:
		if (sound_options & OPT_SOUND_LOGOUT)
			gnome_triggers_do("", "program", "gaim_applet", "leave", NULL);
		break;
	case SEND:
		if (sound_options & OPT_SOUND_SEND)
			gnome_triggers_do("", "program", "gaim_applet", "send", NULL);
		break;
        case FIRST_RECEIVE:
		if (sound_options & OPT_SOUND_FIRST_RCV)
			gnome_triggers_do("", "program", "gaim_applet", "recv", NULL);
		break;
        case RECEIVE:
		if (sound_options & OPT_SOUND_RECV)
			gnome_triggers_do("", "program", "gaim_applet", "recv", NULL);
		break;
	case AWAY:
		if (sound_options & OPT_SOUND_WHEN_AWAY)
			gnome_triggers_do("", "program", "gaim_applet", "recv", NULL);
		break;
       }
}

#endif /* USE_APPLET */

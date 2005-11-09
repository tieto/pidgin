/*
  The mediastreamer library aims at providing modular media processing and I/O
	for linphone, but also for any telephony application.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org
  										
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "mediastream.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cond=1;

void stop_handler(int signum)
{
	cond--;
	if (cond<0) exit(-1);
}

void parse_addr(gchar *addr, char **ip, int *port)
{
	char *semicolon;
	gint iplen;
	
	*ip=NULL;
	*port=0;
	semicolon=strchr(addr,':');
	if (semicolon==NULL) return;
	iplen=semicolon-addr;
	*ip=g_malloc(iplen+1);
	strncpy(*ip,addr,iplen);
	(*ip)[iplen]='\0';
	*port=atoi(semicolon+1);
}

char *usage="mediastream --local <port> --remote <ip:port> --payload <payload type>\n";
void run_media_streams(gint localport, gchar *remote_ip, gint remoteport, gint payload);

int main(int argc, char * argv[])
{
	gint i;
	gint localport=0,remoteport=0,payload=0;
	gchar *ip;
	gchar *tmp;
	
	/*create the rtp session */
	ortp_init();
	ortp_set_debug_file("oRTP",NULL);
	rtp_profile_set_payload(&av_profile,115,&lpc1015);
	rtp_profile_set_payload(&av_profile,110,&speex_nb);
	
	if (argc<4) {
		printf(usage);
		return -1;
	}
	for (i=1;i<argc;i++){
		if (strcmp(argv[i],"--local")==0){
			i++;
			localport=atoi(argv[i]);
		}else if (strcmp(argv[i],"--remote")==0){
			i++;
			parse_addr(argv[i],&ip,&remoteport);
			if (ip==NULL) {
				printf(usage);
				return -1;
			}
			printf("Remote addr: ip=%s port=%i\n",ip,remoteport);
		}else if (strcmp(argv[i],"--payload")==0){
			i++;
			payload=atoi(argv[i]);
		}
	}
	tmp=getenv("defcard");
	if (tmp!=NULL) audio_stream_set_default_card(atoi(tmp));
	run_media_streams(localport,ip,remoteport,payload);
	return 0;
}

void run_media_streams(gint localport, gchar *remote_ip, gint remoteport, gint payload)
{
	AudioStream *audio;
	ms_init();
	ms_speex_codec_init();
	signal(SIGINT,stop_handler);
	
	audio=audio_stream_start(&av_profile,localport,remote_ip,remoteport,payload,250);
	while(cond)
	{
		/* sleep until we receive SIGINT */
		sleep(1);
	}
	
	printf("stoping all...\n");
	
	audio_stream_stop(audio);
}

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

#include <netdb.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include "gaim.h"
#include "proxy.h"
#include "gnome_applet_mgr.h"

unsigned int *get_address(char *hostname)
{
	struct hostent *hp;
	unsigned int *sin=NULL;
	if ((hp = proxy_gethostbyname(hostname))) {
		sin = (unsigned int *)g_new0(struct sockaddr_in, 1);
		memcpy(sin, hp->h_addr, hp->h_length);
	}
	return sin;
}

int connect_address(unsigned int addy, unsigned short port)
{
        int fd;
	struct sockaddr_in sin;

	sin.sin_addr.s_addr = addy;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (fd > -1) {
		quad_addr=strdup(inet_ntoa(sin.sin_addr));
		if (proxy_connect(fd, (struct sockaddr *)&sin, sizeof(sin)) > -1) {
			return fd;
		}
	}
	return -1;
}


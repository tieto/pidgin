/*
 * gaim-remote
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
 * Copyright (C) 2002, Sean Egan <bj91704@binghamton.edu>
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
 */
#ifndef _GAIM_SOCKET_H_
#define _GAIM_SOCKET_H_

#include <glib.h>

typedef struct
{
	unsigned char type;
	unsigned char subtype;
	unsigned long length;
	char *data;

} GaimRemotePacket;

void gaim_remote_session_send_packet(int fd, GaimRemotePacket *packet);
int gaim_remote_session_connect(int session);
gboolean gaim_remote_session_exists(int sess);
GaimRemotePacket *gaim_remote_session_read_packet(int fd);

GaimRemotePacket *gaim_remote_packet_new(guchar type, guchar subtype);
void gaim_remote_packet_free(GaimRemotePacket *p);
void gaim_remote_packet_append_string(GaimRemotePacket *p, char *str);
void gaim_remote_packet_append_char(GaimRemotePacket *p, char c);
void gaim_remote_packet_append_raw(GaimRemotePacket *p, char *str, int len);

#endif /* _GAIM_SOCKET_H_ */

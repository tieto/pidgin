/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Copyright (C) 1998-2001, Denis V. Dmitrienko <denis@null.net> and
 *                          Bill Soudan <soudan@kde.org>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _SOCKETMANAGER_H
#define _SOCKETMANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/socket.h>

#include "icq.h"
#include "list.h"

typedef struct icq_Socket_s icq_Socket;
typedef void (*icq_SocketHandler)(void *data);

struct icq_Socket_s
{
  int socket;

  icq_SocketHandler handlers[ICQ_SOCKET_MAX];
  void *data[ICQ_SOCKET_MAX];
};

int icq_SocketNew(int domain, int type, int protocol);
int icq_SocketAccept(int listens, struct sockaddr *addr, socklen_t *addrlen);
void icq_SocketAlloc(int socket_fd);
int icq_SocketDelete(int socket_fd);
void icq_SocketSetHandler(int socket_fd, int type, icq_SocketHandler handler,
  void *data);
void icq_SocketReady(icq_Socket *s, int type);
void icq_SocketBuildFdSets(void);
void icq_SocketPoll();
icq_Socket *icq_FindSocket(int socket_fd);

extern icq_List *icq_SocketList;

#endif /* _SOCKETMANAGER_H */

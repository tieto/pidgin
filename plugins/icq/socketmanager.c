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

/**
 * The icqlib socket manager is a simple socket abstraction layer, which
 * supports opening and closing sockets as well as installing handler
 * functions for read ready and write ready events.  Its purpose is to
 * both unify socket handling in icqlib and expose icqlib's socket
 * requirements so the library client can assist with socket housekeeping.
 *
 * Library clients have two options to support icqlib:
 *
 * 1. Periodically call icq_Main.  This will handle all select logic
 * internally.  Advantage is implementation ease, disadvantage is wasted 
 * CPU cycles because of polling and poor TCP file transfer performance.
 * 
 * 2. Install a icq_SocketNotify callback, perform your own socket
 * management, and notify icqlib using the icq_SocketReady method when
 * a socket is ready for reading or writing.  Advantage is efficiency,
 * disadvantage is extra code.
 *
 */

#include <stdlib.h>

#ifdef _WIN32
#include <winsock.h>
#endif

#include "socketmanager.h"

icq_List *icq_SocketList = NULL;
fd_set icq_FdSets[ICQ_SOCKET_MAX];
int icq_MaxSocket;

void (*icq_SocketNotify)(int socket_fd, int type, int status);

/**
 * Creates a new socket using the operating system's socket creation
 * facility.
 */
int icq_SocketNew(int domain, int type, int protocol)
{
  int s = socket(domain, type, protocol);

  icq_SocketAlloc(s);

  return s;
}


/**
 * Creates a new socket by accepting a connection from a listening
 * socket.
 */
int icq_SocketAccept(int listens, struct sockaddr *addr, socklen_t *addrlen)
{
  int s = accept(listens, addr, addrlen);

  icq_SocketAlloc(s);

  return s;
}

/**
 * Creates a new icq_Socket structure, and appends it to the 
 * socketmanager's global socket list.
 */
void icq_SocketAlloc(int s)
{
  if (s != -1)
  {
    icq_Socket *psocket = (icq_Socket *)malloc(sizeof(icq_Socket));
    int i;
    psocket->socket = s;

    for (i=0; i<ICQ_SOCKET_MAX; i++)
      psocket->handlers[i] = NULL;

    icq_ListEnqueue(icq_SocketList, psocket);
  }
}  

/**
 * Closes a socket.  This function will notify the library client
 * through the icq_SocketNotify callback if the socket had an installed
 * read or write handler.
 */
int icq_SocketDelete(int socket_fd)
{
#ifdef _WIN32
  int result = closesocket(socket_fd);
#else
  int result = close(socket_fd);
#endif

  if (result != -1)
  {
    icq_Socket *s = icq_FindSocket(socket_fd);
    int i;

    /* uninstall all handlers - this will take care of notifing library
     * client */
    for (i=0; i<ICQ_SOCKET_MAX; i++)
    {
      if (s->handlers[i])
        icq_SocketSetHandler(s->socket, i, NULL, NULL);
    }

    icq_ListRemove(icq_SocketList, s);
    free(s);
  }

  return result;
}

/**
 * Installs a socket event handler.  The handler will be called when
 * the socket is ready for reading or writing, depending on the type
 * which should be either ICQ_SOCKET_READ or ICQ_SOCKET_WRITE.  In 
 * addition, user data can be passed to the callback function through
 * the data member.
 */
void icq_SocketSetHandler(int socket_fd, int type, icq_SocketHandler handler, 
  void *data)
{
  icq_Socket *s = icq_FindSocket(socket_fd);
  if (s)
  {
    s->data[type] = data;
    s->handlers[type] = handler;
    if (icq_SocketNotify)
      (*icq_SocketNotify)(socket_fd, type, handler ? 1 : 0);
  }
}

/**
 * Handles a socket ready event by calling the installed callback 
 * function, if any.
 */
void icq_SocketReady(icq_Socket *s, int type)
{
  if (s && s->handlers[type])
  {
    (*s->handlers[type])(s->data[type]);
  }
}

void icq_HandleReadySocket(int socket_fd, int type)
{
  icq_SocketReady(icq_FindSocket(socket_fd), type);
}
  
int _icq_SocketBuildFdSets(void *p, va_list data)
{
  icq_Socket *s = p;
  int i;
  (void)data;
  
  for (i=0; i<ICQ_SOCKET_MAX; i++)
    if (s->handlers[i]) {
      FD_SET(s->socket, &(icq_FdSets[i]));
      if (s->socket > icq_MaxSocket)
        icq_MaxSocket = s->socket;
    }

  return 0; /* traverse entire icq_List */
}

void icq_SocketBuildFdSets()
{
  int i;

  /* clear fdsets */
  for (i=0; i<ICQ_SOCKET_MAX; i++)
    FD_ZERO(&(icq_FdSets[i]));

  icq_MaxSocket = 0;
  
  /* build fd lists for open sockets */
  (void)icq_ListTraverse(icq_SocketList, _icq_SocketBuildFdSets);
}

int _icq_SocketHandleReady(void *p, va_list data)
{
  icq_Socket *s = p;
  int i;
  (void)data;
  
  for (i=0; i<ICQ_SOCKET_MAX; i++)
    if (FD_ISSET(s->socket, &(icq_FdSets[i]))) {
      icq_SocketReady(s, i);
    }

  return 0; /* traverse entire icq_List */
}
      
void icq_SocketPoll()
{
  struct timeval tv;

  icq_SocketBuildFdSets();
  
  tv.tv_sec = 0; tv.tv_usec = 0;
    
  /* determine which sockets require maintenance */
  select(icq_MaxSocket+1, &(icq_FdSets[ICQ_SOCKET_READ]),
    &(icq_FdSets[ICQ_SOCKET_WRITE]), NULL, &tv);

  /* handle ready sockets */
  (void)icq_ListTraverse(icq_SocketList, _icq_SocketHandleReady);
}

int _icq_FindSocket(void *p, va_list data)
{
  int socket_fd = va_arg(data, int);
  return (((icq_Socket *)p)->socket == socket_fd);
}

icq_Socket *icq_FindSocket(int socket_fd)
{
  return icq_ListTraverse(icq_SocketList, _icq_FindSocket, socket_fd);
}



/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _SOCKETMANAGER_H
#define _SOCKETMANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
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
void icq_SocketAlloc(int s);
int icq_SocketDelete(int socket);
void icq_SocketSetHandler(int socket, int type, icq_SocketHandler handler,
  void *data);
void icq_SocketReady(icq_Socket *s, int type);
void icq_SocketBuildFdSets(void);
void icq_SocketPoll();
icq_Socket *icq_FindSocket(int socket);

extern list *icq_SocketList;

#endif /* _SOCKETMANAGER_H */

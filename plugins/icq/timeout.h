/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef _TIMEOUTMANAGER_H
#define _TIMEOUTMANAGER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>

#include "list.h"

typedef struct icq_Timeout_s icq_Timeout;
typedef void (*icq_TimeoutHandler)(void *data);

struct icq_Timeout_s
{
  time_t expire_time;
  int length;
  int single_shot;

  icq_TimeoutHandler handler;
  void *data;
};

int icq_TimeoutCompare(icq_Timeout *t1, icq_Timeout *t2);
icq_Timeout *icq_TimeoutNew(int length, icq_TimeoutHandler handler, 
  void *data);
void icq_TimeoutDelete(icq_Timeout *timeout);

void icq_HandleTimeout();
void icq_TimeoutDoNotify();

extern list *icq_TimeoutList;

#endif /* _TIMEOUTMANAGER_H */

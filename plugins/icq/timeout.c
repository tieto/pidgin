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

#include <stdlib.h>

#include "timeout.h"

icq_Timeout *icq_CurrentTimeout = NULL;
icq_List *icq_TimeoutList = NULL;

void (*icq_SetTimeout)(long length);

int icq_TimeoutCompare(icq_Timeout *t1, icq_Timeout *t2)
{
  return (t1->expire_time - t2->expire_time);
}

icq_Timeout *icq_TimeoutNew(int length, icq_TimeoutHandler handler, 
  void *data)
{
  icq_Timeout *t = (icq_Timeout *)malloc(sizeof(icq_Timeout));

  if (t)
  {
    int count = icq_TimeoutList->count;

    t->length = length;
    t->handler = handler;
    t->data = data;
    t->expire_time = time(NULL) + length;
    t->single_shot = 1;

    icq_ListInsertSorted(icq_TimeoutList, t);

    if (count == 0)
      icq_TimeoutDoNotify();
  }

  return t;
}

void icq_TimeoutDelete(icq_Timeout *timeout)
{
  icq_ListRemove(icq_TimeoutList, timeout);

  /* if this was the timeout we were currently waiting on, move on
   * to the next */
  if (icq_CurrentTimeout == timeout)
  {
    icq_CurrentTimeout = NULL;
    icq_TimeoutDoNotify();
  }

  free(timeout);
}

int _icq_HandleTimeout1(void *p, va_list data)
{
  icq_Timeout *t = p;
  int complete = 0;
  time_t current_time = va_arg(data, time_t);
  icq_List *expired_timeouts = va_arg(data, icq_List *);
  (void)data;

  if (t->expire_time <= current_time)
    icq_ListEnqueue(expired_timeouts, t);
  else
    /* traversal is complete when we reach an expire time in the future */
    complete = 1;

  return complete;
}

int _icq_HandleTimeout2(void *p, va_list data)
{
  icq_Timeout *t = p;
  (void)data;

  /* maybe a previously executed timeout caused us to be deleted, so
   * make sure we're still around */
  if (icq_ListFind(icq_TimeoutList, t))
    (t->handler)(t->data);

  return 0; /* traverse entire list */
}

int _icq_HandleTimeout3(void *p, va_list data)
{
  icq_Timeout *t = p;
  int complete = 0;
  time_t current_time = va_arg(data, time_t);

  if (t->expire_time <= current_time)
  {
    if (t->single_shot)
      icq_TimeoutDelete(t);
    else
      t->expire_time = current_time + t->length;
  }
  else
    /* traversal is complete when we reach an expire time in the future */
    complete = 1;

  return complete;
}

void icq_HandleTimeout()
{
  time_t current_time = time(NULL);
  icq_List *expired_timeouts = icq_ListNew();

  icq_CurrentTimeout = NULL;

  /* these three operations must be split up for the case where a
   * timeout function causes timers to be deleted - this ensures
   * we don't try to free any timers that have already been removed
   * or corrupt the list traversal process */

  /* determine which timeouts that have expired */
  icq_ListTraverse(icq_TimeoutList, _icq_HandleTimeout1, current_time,
    expired_timeouts);

  /* call handler function for expired timeouts */
  icq_ListTraverse(expired_timeouts, _icq_HandleTimeout2);

  /* delete any expired timeouts */
  icq_ListTraverse(icq_TimeoutList, _icq_HandleTimeout3, current_time);

  /* if there's any timeouts left, notify the library client */
  if (icq_TimeoutList->count)
    icq_TimeoutDoNotify();

  icq_ListDelete(expired_timeouts, NULL);
}

void icq_TimeoutDoNotify()
{
  time_t length, current_time = time(NULL);

  if (!icq_TimeoutList->count)
  {
    if (icq_SetTimeout)
      (*icq_SetTimeout)(0);
    return;
  }

  icq_CurrentTimeout = (icq_Timeout *)icq_ListFirst(icq_TimeoutList);
  length = icq_CurrentTimeout->expire_time - current_time;

  if (icq_SetTimeout)
    (*icq_SetTimeout)(length);
}

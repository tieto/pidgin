/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "timeout.h"

icq_Timeout *icq_CurrentTimeout = NULL;
list *icq_TimeoutList = NULL;

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

    list_insert_sorted(icq_TimeoutList, t);

    if (count == 0)
      icq_TimeoutDoNotify();
  }

  return t;
}

void icq_TimeoutDelete(icq_Timeout *timeout)
{
  list_remove(icq_TimeoutList, timeout);

  /* if this was the timeout we were currently waiting on, move on
   * to the next */
  if (icq_CurrentTimeout = timeout)
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
  list *expired_timeouts = va_arg(data, list *);

  if (t->expire_time <= current_time)
    list_enqueue(expired_timeouts, t);
  else
    /* traversal is complete when we reach an expire time in the future */
    complete = 1;

  return complete;
}

int _icq_HandleTimeout2(void *p, va_list data)
{
  icq_Timeout *t = p;

  /* maybe a previously executed timeout caused us to be deleted, so
   * make sure we're still around */
  if (list_find(icq_TimeoutList, t))
    (t->handler)(t->data);
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
  list *expired_timeouts = list_new();

  icq_CurrentTimeout = NULL;

  /* these three operations must be split up, in the case where a
   * timeout function causes timers to be deleted - this ensures
   * we don't try to free any timers that have already been removed
   * or corrupt the list traversal process */

  /* determine which timeouts that have expired */
  list_traverse(icq_TimeoutList, _icq_HandleTimeout1, current_time,
    expired_timeouts);

  /* call handler function for expired timeouts */
  list_traverse(expired_timeouts, _icq_HandleTimeout2);

  /* delete any expired timeouts */
  list_traverse(icq_TimeoutList, _icq_HandleTimeout3, current_time);

  if (icq_TimeoutList->count)
    icq_TimeoutDoNotify();
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

  icq_CurrentTimeout = (icq_Timeout *)list_first(icq_TimeoutList);
  length = icq_CurrentTimeout->expire_time - current_time;

  if (icq_SetTimeout)
    (*icq_SetTimeout)(length);
}

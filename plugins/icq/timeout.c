/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "timeout.h"

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
  free(timeout);
}

int _icq_HandleTimeout(void *p, va_list data)
{
  icq_Timeout *t = p;
  time_t current_time = va_arg(data, time_t);
  int complete = 0;

  if (t->expire_time <= current_time)
    (*t->handler)(t->data);
  else
    /* traversal is complete when we reach an expire time in the future */
    complete = 1;

  if (t->single_shot)
    icq_TimeoutDelete(t);
  else
    t->expire_time = current_time + t->length;

  return complete;
}

void icq_HandleTimeout()
{
  time_t current_time = time(NULL);

  /* call handler functions for all timeouts that have expired */
  list_traverse(icq_TimeoutList, _icq_HandleTimeout, current_time);

  if (icq_TimeoutList->count)
    icq_TimeoutDoNotify();
}  

void icq_TimeoutDoNotify()
{
  time_t current_time = time(NULL);

  icq_Timeout *t = (icq_Timeout *)list_first(icq_TimeoutList);
  long length = t->expire_time - current_time;
  if (icq_SetTimeout)
    (*icq_SetTimeout)(length);
}

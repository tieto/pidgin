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

extern icq_List *icq_TimeoutList;

#endif /* _TIMEOUTMANAGER_H */

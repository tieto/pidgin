/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: list.h 1987 2001-06-09 14:46:51Z warmenhoven $
 *
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

#ifndef _LIST_H
#define _LIST_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#define icq_ListEnqueue(plist, p) \
   icq_ListInsert(plist, 0, p)

#define icq_ListDequeue(plist) \
  icq_ListRemoveNode(plist, plist->head)

typedef struct icq_ListNode_s icq_ListNode;
typedef struct icq_List_s icq_List;
typedef int (*icq_ListCompareFunc)(void *o1, void *o2);

struct icq_ListNode_s
{
  icq_ListNode *next;
  icq_ListNode *previous;
  void *item;
};

struct icq_List_s
{
  icq_ListNode *head;
  icq_ListNode *tail;
  int count;
  icq_ListCompareFunc compare_function;
};

icq_List *icq_ListNew(void);
void icq_ListDelete(icq_List *plist, void (*item_free_f)(void *));
void icq_ListFree(icq_List *plist, void (*item_free_f)(void *));
void icq_ListInsertSorted(icq_List *plist, void *pitem);
void icq_ListInsert(icq_List *plist, icq_ListNode *pnode, void *pitem);
void *icq_ListRemove(icq_List *plist, void *pitem);
void *icq_ListTraverse(icq_List *plist, int (*item_f)(void *, va_list), ...);
int icq_ListDump(icq_List *plist);
void *icq_ListFirst(icq_List *plist);
void *icq_ListLast(icq_List *plist);
void *icq_ListAt(icq_List *plist, int num);
icq_ListNode *icq_ListFind(icq_List *plist, void *pitem);
void *icq_ListRemoveNode(icq_List *plist, icq_ListNode *p);

#endif /* _LIST_H */

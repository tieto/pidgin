/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * $Id: list.c 2096 2001-07-31 01:00:39Z warmenhoven $
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

/*
 * linked list functions
 */

#include <stdlib.h>
#include <stdio.h>

#include "list.h"

icq_List *icq_ListNew()
{
  icq_List *plist=(icq_List *)malloc(sizeof(icq_List));

  plist->head=0;
  plist->tail=0;
  plist->count=0;

  return plist;
}

/* Frees all list nodes and list itself */
void icq_ListDelete(icq_List *plist, void (*item_free_f)(void *))
{
  if (item_free_f)
    icq_ListFree(plist, item_free_f);
  free(plist);
}

/* Only frees the list nodes */
void icq_ListFree(icq_List *plist, void (*item_free_f)(void *))
{
  icq_ListNode *p=plist->head;

#ifdef LIST_TRACE
  printf("icq_ListFree(%p)\n", plist);
  icq_ListDump(plist);
#endif

  while(p)
  {
    icq_ListNode *ptemp=p;

    p=p->next;
    (*item_free_f)((void *)ptemp->item);
    icq_ListRemoveNode(plist, ptemp);
  }
}

void icq_ListInsertSorted(icq_List *plist, void *pitem)
{
  icq_ListNode *i=plist->head;
  int done = 0;

  while (i && !done)
  {
    if ((*plist->compare_function)(pitem, i->item)<0)
      done = 1;
    else
      i=i->next;
  }

  icq_ListInsert(plist, i, pitem);
}

void icq_ListInsert(icq_List *plist, icq_ListNode *pnode, void *pitem)
{
  icq_ListNode *pnew=(icq_ListNode *)malloc(sizeof(icq_ListNode));
  pnew->item=pitem;

#ifdef LIST_TRACE
  printf("inserting %x (node=%x) into list %x\n", pitem, pnew, plist);
#endif
 
  plist->count++;

  /* null source node signifies insert at end of icq_List */
  if(!pnode) 
  {
    pnew->previous=plist->tail;
    pnew->next=0;
    if(plist->tail)
      plist->tail->next=pnew;
    plist->tail=pnew;

    if(!plist->head)
      plist->head=pnew;
  }
  else
  {
    pnew->previous=pnode->previous;
    pnew->next=pnode;

    if(pnew->previous)
      pnew->previous->next=pnew;

    if(pnew->next)
      pnode->previous=pnew;
        
    if(plist->head==pnode)
      plist->head=pnew;
  }

#ifdef LIST_TRACE
  icq_ListDump(plist);
#endif
}

void *icq_ListRemoveNode(icq_List *plist, icq_ListNode *p)
{
  void *pitem;

  if(!p)
    return 0;

#ifdef LIST_TRACE
  printf("removing %x (node=%x) from list %x\n", p->item, p, plist);
#endif

  plist->count--;

  if(p->next)
    p->next->previous=p->previous;

  if(p->previous)
    p->previous->next=p->next;

  if(plist->head==p)
    plist->head=p->next;

  if(plist->tail==p)
    plist->tail=p->previous;

  p->next=0;
  p->previous=0;

#ifdef LIST_TRACE
  icq_ListDump(plist);
#endif

  pitem=p->item;

  free(p);

  return pitem;
}

void *icq_ListTraverse(icq_List *plist, int (*item_f)(void *, va_list), ...)
{
  icq_ListNode *i=plist->head;
  int f=0;
  va_list ap;

#ifdef LIST_TRACE
  printf("icq_ListTraverse(%p)\n", plist);
  icq_ListDump(plist);
#endif
  va_start(ap, item_f);

  /* call item_f for each item in list until end of list or item 
   * function returns 0 */
  while(i && !f)
  {
    icq_ListNode *pnext=i->next;

    if(!(f=(*item_f)(i->item, ap)))
      i=pnext;
  }

  va_end(ap);

  if (i)
    return i->item;
  else
    return 0;
}

int icq_ListDump(icq_List *plist)
{
  icq_ListNode *p=plist->head;

  printf("list %lx { head=%lx, tail=%lx, count=%d }\ncontents: ",
         (long)plist, (long)plist->head, (long)plist->tail, plist->count);

  while(p)
  {
    printf("%lx, ", (long)p->item);
    p=p->next;
  }
  printf("end\n");

  return 0;
}

void *icq_ListFirst(icq_List *plist)
{
  if(plist->head)
    return plist->head->item;
  else
    return 0;
}

void *icq_ListLast(icq_List *plist)
{
  if(plist->tail)
    return plist->tail->item;
  else
    return 0;
}

void *icq_ListAt(icq_List *plist, int num)
{
  icq_ListNode *ptr = plist->head;
  while(ptr && num)
  {
    num--;
    ptr = ptr->next;
  }
  if(!num)
    return ptr->item;
  else
    return 0L;
}

icq_ListNode *icq_ListFind(icq_List *plist, void *pitem)
{
  icq_ListNode *p=plist->head;

  while(p)
  {
    if(p->item==pitem)
      return p;
    p=p->next;
  }
  return 0;
}

void *icq_ListRemove(icq_List *plist, void *pitem)
{
  icq_ListNode *p=icq_ListFind(plist, pitem);

  if(p)
    return icq_ListRemoveNode(plist, p);
  else
    return 0;
}

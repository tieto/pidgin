/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: list.c 1319 2000-12-19 10:08:29Z warmenhoven $
$Log$
Revision 1.2  2000/12/19 10:08:29  warmenhoven
Yay, new icqlib

Revision 1.14  2000/07/10 01:44:20  bills
i really don't learn - removed LIST_TRACE define

Revision 1.13  2000/07/10 01:43:48  bills
argh - last list buglet fixed, removed free(node) call from list_free

Revision 1.12  2000/07/10 01:31:17  bills
oops - removed #define LIST_TRACE and #define QUEUE_DEBUG

Revision 1.11  2000/07/10 01:26:30  bills
added more trace messages, added list_remove_node call in list_free...
fixes list corruption bug introduced during last commit

Revision 1.10  2000/07/09 22:04:45  bills
recoded list_free function - this was working very incorrectly!  it was
only freeing the first node of the list, and then ending.  fixes a memory
leak.

Revision 1.9  2000/05/10 18:48:56  denis
list_free() was added to free but do not dispose the list.
Memory leak with destroying the list was fixed.

Revision 1.8  2000/05/03 18:19:15  denis
Bug with empty contact list was fixed.

Revision 1.7  2000/01/16 21:26:54  bills
fixed serious bug in list_remove

Revision 1.6  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.5  1999/09/29 19:59:30  bills
cleanups

Revision 1.4  1999/07/16 11:59:46  denis
list_first(), list_last(), list_at() added.
Cleaned up.

*/
/*
 * linked list functions
 */

#include <stdlib.h>
#include <stdio.h>

#include "list.h"

list *list_new()
{
  list *plist=(list *)malloc(sizeof(list));

  plist->head=0;
  plist->tail=0;
  plist->count=0;

  return plist;
}

/* Frees all list nodes and list itself */
void list_delete(list *plist, void (*item_free_f)(void *))
{
  list_free(plist, item_free_f);
  free(plist);
}

/* Only frees the list nodes */
void list_free(list *plist, void (*item_free_f)(void *))
{
  list_node *p=plist->head;

#ifdef LIST_TRACE
  printf("list_free(%p)\n", plist);
  list_dump(plist);
#endif

  while(p)
  {
    list_node *ptemp=p;

    p=p->next;
    (*item_free_f)((void *)ptemp->item);
    list_remove_node(plist, ptemp);
  }
}

void list_insert(list *plist, list_node *pnode, void *pitem)
{
  list_node *pnew=(list_node *)malloc(sizeof(list_node));
  pnew->item=pitem;

#ifdef LIST_TRACE
  printf("inserting %x (node=%x) into list %x\n", pitem, pnew, plist);
#endif
 
  plist->count++;

  /* null source node signifies insert at end of list */
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
  list_dump(plist);
#endif
}

void *list_remove_node(list *plist, list_node *p)
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
  list_dump(plist);
#endif

  pitem=p->item;

  free(p);

  return pitem;
}

void *list_traverse(list *plist, int (*item_f)(void *, va_list), ...)
{
  list_node *i=plist->head;
  int f=0;
  va_list ap;

#ifdef LIST_TRACE
  printf("list_traverse(%p)\n", plist);
  list_dump(plist);
#endif
  va_start(ap, item_f);

  /* call item_f for each item in list until end of list or item 
   * function returns 0 */
  while(i && !f)
  {
    list_node *pnext=i->next;

    if(!(f=(*item_f)(i->item, ap)))
      i=pnext;
  }
  va_end(ap);
  if (i)
    return i->item;
  else
    return 0;
}

int list_dump(list *plist)
{
  list_node *p=plist->head;

  printf("list %x { head=%x, tail=%x, count=%d }\ncontents: ",
         (int)plist, (int)plist->head, (int)plist->tail, plist->count);

  while(p)
  {
    printf("%x, ", (int)p->item);
    p=p->next;
  }
  printf("end\n");

  return 0;
}

void *list_first(list *plist)
{
  if(plist->head)
    return plist->head->item;
  else
    return 0;
}

void *list_last(list *plist)
{
  if(plist->tail)
    return plist->tail->item;
  else
    return 0;
}

void *list_at(list *plist, int num)
{
  list_node *ptr = plist->head;
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

list_node *list_find(list *plist, void *pitem)
{
  list_node *p=plist->head;

  while(p)
  {
    if(p->item==pitem)
      return p;
    p=p->next;
  }
  return 0;
}

void *list_remove(list *plist, void *pitem)
{
  list_node *p=list_find(plist, pitem);

  if(p)
    return list_remove_node(plist, p);
  else
    return 0;
}

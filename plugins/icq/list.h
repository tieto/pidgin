/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _LIST_H
#define _LIST_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#define list_enqueue(plist, p) \
   list_insert(plist, 0, p)

#define list_dequeue(plist) \
  list_remove_node(plist, plist->head)

typedef struct list_node_s list_node;
typedef struct list_s list;
typedef int (*icq_ListCompareFunc)(void *o1, void *o2);

struct list_node_s
{
  list_node *next;
  list_node *previous;
  void *item;
};

struct list_s
{
  list_node *head;
  list_node *tail;
  int count;
  icq_ListCompareFunc compare_function;
};

list *list_new(void);
void list_delete(list *plist, void (*item_free_f)(void *));
void list_free(list *plist, void (*item_free_f)(void *));
void list_insert_sorted(list *plist, void *pitem);
void list_insert(list *plist, list_node *pnode, void *pitem);
void *list_remove(list *plist, void *pitem);
void *list_traverse(list *plist, int (*item_f)(void *, va_list), ...);
int list_dump(list *plist);
void *list_first(list *plist);
void *list_last(list *plist);
void *list_at(list *plist, int num);
list_node *list_find(list *plist, void *pitem);
void *list_remove_node(list *plist, list_node *p);

#endif /* _LIST_H */

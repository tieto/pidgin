/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: contacts.c 1442 2001-01-28 01:52:27Z warmenhoven $
$Log$
Revision 1.3  2001/01/28 01:52:27  warmenhoven
icqlib 1.1.5

Revision 1.6  2001/01/16 00:10:13  denis
Invisible list has been finished.

Revision 1.5  2000/12/19 06:00:07  bills
moved members from ICQLINK to ICQLINK_private struct

Revision 1.4  2000/06/17 16:38:45  denis
New parameter was added in icq_ContactSetVis() for setting/resetting
'Visible to User' status.
Port's type was changed to unsigned short in icq_UserOnline callback.

Revision 1.3  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.2  1999/07/23 12:28:00  denis
Cleaned up.

Revision 1.1  1999/07/18 20:11:48  bills
added

*/

#include <stdlib.h>
#include <stdarg.h>

#include "icq.h"
#include "icqlib.h"
#include "icqtypes.h"
#include "util.h"
#include "list.h"
#include "contacts.h"

icq_ContactItem *icq_ContactNew(ICQLINK *link)
{
  icq_ContactItem *pcontact=
    (icq_ContactItem *)malloc(sizeof(icq_ContactItem));

  if(pcontact)
    pcontact->icqlink=link;

  return pcontact;
}

void icq_ContactDelete(void *p)
{
  free(p);
}

void icq_ContactAdd(ICQLINK *link, DWORD cuin)
{
  icq_ContactItem *p = icq_ContactNew(link);
  p->uin = cuin;
  p->vis_list = FALSE;

  list_enqueue(link->d->icq_ContactList, p);
}

void icq_ContactRemove(ICQLINK *link, DWORD cuin)
{
  icq_ContactItem *pcontact=icq_ContactFind(link, cuin);

  if (pcontact)
  {
    list_remove(link->d->icq_ContactList, pcontact);
    icq_ContactDelete(pcontact);
  }
}

void icq_ContactClear(ICQLINK *link)
{
  list_delete(link->d->icq_ContactList, icq_ContactDelete);
  link->d->icq_ContactList=list_new();
}

int _icq_ContactFind(void *p, va_list data)
{
  DWORD uin=va_arg(data, DWORD);

  return (((icq_ContactItem *)p)->uin == uin);
}

icq_ContactItem *icq_ContactFind(ICQLINK *link, DWORD cuin)
{
  return list_traverse(link->d->icq_ContactList, _icq_ContactFind, cuin);
}

void icq_ContactSetVis(ICQLINK *link, DWORD cuin, BYTE vu)
{
  icq_ContactItem *p = icq_ContactFind(link, cuin);
  if(p)
    p->vis_list = vu;
}

void icq_ContactSetInvis(ICQLINK *link, DWORD cuin, BYTE vu)
{
  icq_ContactItem *p = icq_ContactFind(link, cuin);
  if(p)
    p->invis_list = vu;
}

icq_ContactItem *icq_ContactGetFirst(ICQLINK *link)
{
  return list_first(link->d->icq_ContactList);
}

icq_ContactItem *icq_ContactGetNext(icq_ContactItem *pcontact)
{
  list_node *p=list_find(pcontact->icqlink->d->icq_ContactList, pcontact);

  if (p && p->next)
    return p->next->item;
  else
    return 0L;
}

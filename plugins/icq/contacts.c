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

#include "icqlib.h"

#include "contacts.h"

icq_ContactItem *icq_ContactNew(icq_Link *icqlink)
{
  icq_ContactItem *pcontact=
    (icq_ContactItem *)malloc(sizeof(icq_ContactItem));

  if(pcontact)
    pcontact->icqlink=icqlink;

  return pcontact;
}

void icq_ContactDelete(void *p)
{
  free(p);
}

void icq_ContactAdd(icq_Link *icqlink, DWORD cuin)
{
  icq_ContactItem *p = icq_ContactNew(icqlink);
  p->uin = cuin;
  p->vis_list = FALSE;

  icq_ListEnqueue(icqlink->d->icq_ContactList, p);
}

void icq_ContactRemove(icq_Link *icqlink, DWORD cuin)
{
  icq_ContactItem *pcontact=icq_ContactFind(icqlink, cuin);

  if (pcontact)
  {
    icq_ListRemove(icqlink->d->icq_ContactList, pcontact);
    icq_ContactDelete(pcontact);
  }
}

void icq_ContactClear(icq_Link *icqlink)
{
  icq_ListDelete(icqlink->d->icq_ContactList, icq_ContactDelete);
  icqlink->d->icq_ContactList=icq_ListNew();
}

int _icq_ContactFind(void *p, va_list data)
{
  DWORD uin=va_arg(data, DWORD);

  return (((icq_ContactItem *)p)->uin == uin);
}

icq_ContactItem *icq_ContactFind(icq_Link *icqlink, DWORD cuin)
{
  return icq_ListTraverse(icqlink->d->icq_ContactList, _icq_ContactFind, cuin);
}

void icq_ContactSetVis(icq_Link *icqlink, DWORD cuin, BYTE vu)
{
  icq_ContactItem *p = icq_ContactFind(icqlink, cuin);
  if(p)
    p->vis_list = vu;
}

void icq_ContactSetInvis(icq_Link *icqlink, DWORD cuin, BYTE vu)
{
  icq_ContactItem *p = icq_ContactFind(icqlink, cuin);
  if(p)
    p->invis_list = vu;
}

icq_ContactItem *icq_ContactGetFirst(icq_Link *icqlink)
{
  return icq_ListFirst(icqlink->d->icq_ContactList);
}

icq_ContactItem *icq_ContactGetNext(icq_ContactItem *pcontact)
{
  icq_ListNode *p=icq_ListFind(pcontact->icqlink->d->icq_ContactList, pcontact);

  if (p && p->next)
    return p->next->item;
  else
    return 0L;
}

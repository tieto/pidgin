/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _CONTACTS_H
#define _CONTACTS_H

#include "icq.h"
#include "icqtypes.h"

typedef struct icq_ContactItem_s
{
  ICQLINK *icqlink;
  unsigned long uin;
  int vis_list;
  int invis_list;
  unsigned long remote_ip;
  unsigned long remote_real_ip;
  unsigned long remote_port;
  unsigned char tcp_flag;
} icq_ContactItem;  

icq_ContactItem *icq_ContactNew(ICQLINK *link);
void icq_ContactDelete(void *pcontact);
icq_ContactItem *icq_ContactFind(ICQLINK *link, DWORD cuin);
icq_ContactItem *icq_ContactGetFirst(ICQLINK *link);
icq_ContactItem *icq_ContactGetNext(icq_ContactItem *pcontact);

#endif /* _CONTACTS_H */

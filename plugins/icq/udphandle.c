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
#include "udp.h"
#include "queue.h"
#include "icqbyteorder.h"
#include "contacts.h"

void icq_DoMsg(icq_Link *icqlink, DWORD type, WORD len, char *data, DWORD uin, BYTE hour,
               BYTE minute, BYTE day, BYTE month, WORD year)
{
  icq_List *strList;
  int fieldCount;
  int i, k, nr;
  const char **contact_uin;
  const char **contact_nick;
  (void)len;

  strList = icq_ListNew();
  switch(type)
  {
    case TYPE_ADDED:
      /* Format: Nick, 0xFE, FName, 0xFE, LName, 0xFE, EMail */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 4 && fieldCount != 5)
      {
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad TYPE_ADDED packet (expected 4/5 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", icq_ListAt(strList, 0)); /* Nick */
      icq_RusConv("wk", icq_ListAt(strList, 1)); /* FName */
      icq_RusConv("wk", icq_ListAt(strList, 2)); /* LName */
      icq_RusConv("wk", icq_ListAt(strList, 3)); /* EMail */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "%lu has added you to their contact list, "
                 "Nick: %s, First Name: %s, Last Name: %s, EMail: %s\n",
                 uin, icq_ListAt(strList, 0), icq_ListAt(strList, 1),
                 icq_ListAt(strList, 2), icq_ListAt(strList, 3));
      invoke_callback(icqlink,icq_RecvAdded)(icqlink, uin, hour, minute, day, month, year,
                                             icq_ListAt(strList, 0), icq_ListAt(strList, 1),
                                             icq_ListAt(strList, 2), icq_ListAt(strList, 3));
      break;
    case TYPE_AUTH_REQ:
      /* Format: Nick, 0xFE, FName, 0xFE, LName, 0xFE, EMail, 0xFE, 0, 0xFE, Reason */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad TYPE_AUTH_REQ packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", icq_ListAt(strList, 0)); /* Nick */
      icq_RusConv("wk", icq_ListAt(strList, 1)); /* FName */
      icq_RusConv("wk", icq_ListAt(strList, 2)); /* LName */
      icq_RusConv("wk", icq_ListAt(strList, 3)); /* EMail */
      icq_RusConv("wk", icq_ListAt(strList, 5)); /* Reason */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "%lu has requested your authorization to be added to "
                 "their contact list, Nick: %s, First Name: %s, Last Name: %s, "
                 "EMail: %s, Reason: %s\n", uin, icq_ListAt(strList, 0), icq_ListAt(strList, 1),
                 icq_ListAt(strList, 2), icq_ListAt(strList, 3), icq_ListAt(strList, 4));
      invoke_callback(icqlink,icq_RecvAuthReq)(icqlink, uin, hour, minute, day, month, year,
                                               icq_ListAt(strList, 0), icq_ListAt(strList, 1),
                                               icq_ListAt(strList, 2), icq_ListAt(strList, 3),
                                               icq_ListAt(strList, 5));
      break;
    case TYPE_URL:
      /* Format: Description, 0xFE, URL */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 2)
      {
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad TYPE_URL packet (expected 2 args, recived %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", icq_ListAt(strList, 0)); /* Description */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "URL received from %lu, URL: %s, Description: %s\n",
                 uin, icq_ListAt(strList, 1), icq_ListAt(strList, 0));
      invoke_callback(icqlink,icq_RecvURL)(icqlink, uin, hour, minute, day, month, year,
                                           icq_ListAt(strList, 1), icq_ListAt(strList, 0));
      break;
    case TYPE_WEBPAGER:
      /* Format: Nick, 0xFE, Empty-FName, 0xFE, Empty-LName, 0xFE, EMail, 0xFE,
       *         Reason(3), 0xFE, Message with IP & Subject */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad TYPE_WEBPAGER packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", icq_ListAt(strList, 0)); /* Nick */
      icq_RusConv("wk", icq_ListAt(strList, 5)); /* Message */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "WebPager message received, Nick: %s, EMail: %s, "
                 "Message:\n%s\n", icq_ListAt(strList, 0), icq_ListAt(strList, 3),
                 icq_ListAt(strList, 5));
      invoke_callback(icqlink,icq_RecvWebPager)(icqlink, hour, minute, day, month, year,
                                                icq_ListAt(strList, 0), icq_ListAt(strList, 3),
                                                icq_ListAt(strList, 5));
      break;
    case TYPE_EXPRESS:
      /* Format: Nick, 0xFE, Empty-FName, 0xFE, Empty-LName, 0xFE, EMail, 0xFE,
       *         Reason(3), 0xFE, Message Subject */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Bad TYPE_EXPRESS packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", icq_ListAt(strList, 0)); /* Nick */
      icq_RusConv("wk", icq_ListAt(strList, 5)); /* Message */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "MailExpress message received, Nick: %s, EMail: %s, "
                 "Message:\n%s\n", icq_ListAt(strList, 0), icq_ListAt(strList, 3),
                 icq_ListAt(strList, 5));
      invoke_callback(icqlink, icq_RecvMailExpress)(icqlink, hour, minute, day, month, year,
                                                   icq_ListAt(strList, 0), icq_ListAt(strList, 3),
                                                   icq_ListAt(strList, 5));
      break;
    case TYPE_CONTACT:
      /* Format: Number of contacts, 0xFE, UIN, 0xFE, Nick, 0xFE, ... */
      nr = icq_SplitFields(strList, data);
      contact_uin = (const char**)malloc((nr - 1) / 2);
      contact_nick = (const char**)malloc((nr - 1) / 2);

      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Contact List received from %lu (%i):\n", uin,
                 atoi(icq_ListAt(strList, 0)));

      for(i = 1, k = 0; i < (nr - 1); k++)
      {
        contact_uin[k] = icq_ListAt(strList, i);
        contact_nick[k] = icq_ListAt(strList, i + 1);
        i += 2;
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "\t%s - %s\n", contact_uin[k], contact_nick[k]);
      }
      invoke_callback(icqlink, icq_RecvContactList)(icqlink, uin, hour, minute, day, month, year,
                                                 k, contact_uin, contact_nick);
      free(contact_uin);
      free(contact_nick);
      break;
    default:
      icq_RusConv("wk", data); /* Entire message */
      icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Instant message type %i from %lu:\n%s\n", type, uin, data);
      invoke_callback(icqlink, icq_RecvMessage)(icqlink, uin, hour, minute, day, month, year, data);
  }
  icq_ListDelete(strList, free);
}

void icq_HandleInfoReply(icq_Link *icqlink, icq_Packet *p)
{
  char *ptr1, *ptr2, *ptr3, *ptr4;
  DWORD uin;
  icq_PacketGotoUDPInData(p, 0);
  uin = icq_PacketRead32(p);
  ptr1 = icq_PacketReadStringNew(p);
  ptr2 = icq_PacketReadStringNew(p);
  ptr3 = icq_PacketReadStringNew(p);
  ptr4 = icq_PacketReadStringNew(p);
  icq_RusConv("wk", ptr1);
  icq_RusConv("wk", ptr2);
  icq_RusConv("wk", ptr3);
  icq_RusConv("wk", ptr4);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Info reply for %lu\n", uin);
  invoke_callback(icqlink,icq_InfoReply)(icqlink, uin, ptr1, ptr2, ptr3, ptr4, icq_PacketRead8(p));
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
}

void icq_HandleExtInfoReply(icq_Link *icqlink, icq_Packet *p)
{
  char *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;
  DWORD uin;
  WORD cnt_code, age;
  char cnt_stat, gender;

  icq_PacketGotoUDPInData(p, 0);
  uin = icq_PacketRead32(p);
  ptr1 = icq_PacketReadStringNew(p);
  cnt_code = icq_PacketRead16(p);
  cnt_stat = icq_PacketRead8(p);
  ptr2 = icq_PacketReadStringNew(p);
  age = icq_PacketRead16(p);
  gender = icq_PacketRead8(p);
  ptr3 = icq_PacketReadStringNew(p);
  ptr4 = icq_PacketReadStringNew(p);
  ptr5 = icq_PacketReadStringNew(p);
  icq_RusConv("wk", ptr1);
  icq_RusConv("wk", ptr2);
  icq_RusConv("wk", ptr3);
  icq_RusConv("wk", ptr4);
  icq_RusConv("wk", ptr5);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Extended info reply for %lu\n", uin);
  invoke_callback(icqlink,icq_ExtInfoReply)(icqlink, uin, (char*)ptr1, cnt_code, cnt_stat, (char*)ptr2,
                                            age, gender, (char*)ptr3, (char*)ptr4, (char*)ptr5);
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
  free(ptr5);
}

void icq_HandleSearchReply(icq_Link *icqlink, icq_Packet *p)
{
  char *ptr1, *ptr2, *ptr3, *ptr4, auth;
  DWORD uin;
  icq_PacketGotoUDPInData(p, 0);
  uin = icq_PacketRead32(p);
  ptr1 = icq_PacketReadStringNew(p);
  ptr2 = icq_PacketReadStringNew(p);
  ptr3 = icq_PacketReadStringNew(p);
  ptr4 = icq_PacketReadStringNew(p);
  icq_RusConv("wk", ptr1);
  icq_RusConv("wk", ptr2);
  icq_RusConv("wk", ptr3);
  icq_RusConv("wk", ptr4);
  auth = icq_PacketRead8(p);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "User found %lu, Nick: %s, First Name: %s, Last Name: %s, "
             "EMail: %s, Auth: %s\n", uin, ptr1, ptr2, ptr3, ptr4, auth==1?"no":"yes");
  invoke_callback(icqlink,icq_UserFound)(icqlink, uin, (char*)ptr1, (char*)ptr2,
                                         (char*)ptr3, (char*)ptr4, auth);
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
}

/************************************************
This is called when a user goes offline
*************************************************/
void icq_HandleUserOffline(icq_Link *icqlink, icq_Packet *p)
{
  DWORD remote_uin;
  icq_ContactItem *ptr;

  icq_PacketGotoUDPInData(p, 0);
  remote_uin = icq_PacketRead32(p);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "User %lu logged off\n", remote_uin);
  invoke_callback(icqlink,icq_UserOffline)(icqlink, remote_uin);

  ptr=icq_ContactFind(icqlink, remote_uin);
  if(ptr)
  {
    ptr->remote_ip = 0;
    ptr->remote_port = 0;
  }
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleUserOnline(icq_Link *icqlink, icq_Packet *p)
{
  DWORD remote_uin, new_status, remote_ip, remote_real_ip;
  DWORD remote_port; /* Why Mirabilis used 4 bytes for port? */
  BYTE tcp_flag;
  icq_ContactItem *ptr;

  icq_PacketGotoUDPInData(p, 0);
  remote_uin = icq_PacketRead32(p);
  remote_ip = ntohl(icq_PacketRead32n(p));  /* icqtohl() */
  remote_port = icqtohl(icq_PacketRead32n(p));
  remote_real_ip = ntohl(icq_PacketRead32n(p)); /* icqtohl() */
  tcp_flag = icq_PacketRead8(p);
  new_status = icq_PacketRead32(p);

  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE,
             "User %lu (%s = 0x%X) logged on. tcp_flag=0x%X IP=%08X, real IP=%08X, port=%d\n",
             remote_uin, icq_ConvertStatus2Str(new_status), new_status, tcp_flag, remote_ip,
             remote_real_ip, remote_port);
  invoke_callback(icqlink,icq_UserOnline)(icqlink, remote_uin, new_status, remote_ip,
                                          remote_port, remote_real_ip, tcp_flag);

  ptr=icq_ContactFind(icqlink, remote_uin);
  if(ptr)
  {
    ptr->remote_ip=remote_ip;
    ptr->remote_real_ip=remote_real_ip;
    ptr->remote_port = remote_port;
    ptr->tcp_flag = tcp_flag;
  }
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleStatusChange(icq_Link *icqlink, icq_Packet *p)
{
  unsigned long remote_uin, new_status;

  icq_PacketGotoUDPInData(p, 0);
  remote_uin = icq_PacketRead32(p);
  new_status = icq_PacketRead32(p);
  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "%lu changed status to %s (0x%X)\n", remote_uin,
             icq_ConvertStatus2Str(new_status), new_status);
  invoke_callback(icqlink,icq_UserStatusUpdate)(icqlink, remote_uin, new_status);
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleMetaUserInfo(icq_Link *icqlink, icq_Packet *p)
{
  unsigned short subcmd, country, seq2, age, occupation, wcountry;
  char res, auth, timezone, webaware, hideip, gender;
  char *nick, *first, *last, *email, *about, *city;
  char *pri_eml, *sec_eml, *old_eml;
  char *phone, *fax, *street, *cellular, *state;
  char *wcity, *wstate, *wphone, *wfax, *waddress;
  char *company, *department, *job, *whomepage;
  char *homepage;
  char byear, bmonth, bday, lang1, lang2, lang3, inum;
  int i;
  char anum, bnum, hnum;
  unsigned long uin, zip, wzip;
  char *empty = NULL;
  char *interests[4] = {0, 0, 0, 0};
  unsigned short icategory[4] = {0, 0, 0, 0};
  char *affiliations[4] = {0, 0, 0, 0};
  unsigned short acategory[4] = {0, 0, 0, 0};
  char *backgrounds[4] = {0, 0, 0, 0};
  unsigned short bcategory[4] = {0, 0, 0, 0};
  char *hpcat[4] = {0, 0, 0, 0};
  unsigned short hcategory[4] = {0, 0, 0, 0};

  seq2 = icq_PacketReadUDPInSeq2(p);
  icq_PacketGotoUDPInData(p, 0);
  subcmd = icq_PacketRead16(p);
  res = icq_PacketRead8(p);
  if(res == META_SRV_FAILURE)
  {
    icq_FmtLog(icqlink, ICQ_LOG_WARNING, "META failure\n");
    invoke_callback(icqlink,icq_RequestNotify)(icqlink, seq2, ICQ_NOTIFY_FAILED,
                                               sizeof(subcmd), &subcmd);
  }
  else
    switch(subcmd)
    {
      case META_SRV_USER_FOUND:
        uin = icq_PacketRead32(p);
        nick = icq_PacketReadStringNew(p);
        first = icq_PacketReadStringNew(p);
        last = icq_PacketReadStringNew(p);
        email = icq_PacketReadStringNew(p);
        auth = icq_PacketRead8(p);
        icq_PacketRead16(p); /* ??? */
        icq_PacketRead32(p); /* ??? */
        icq_RusConv("wk", nick);
        icq_RusConv("wk", first);
        icq_RusConv("wk", last);
        icq_RusConv("wk", email);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Found %lu, Nick: %s, First Name: %s, "\
                   "Last Name: %s, EMail: %s, Auth: %s\n", seq2, uin, nick, first, last,
                   email, auth==1?"no":"yes");
        invoke_callback(icqlink,icq_MetaUserFound)(icqlink, seq2, uin, nick, first, last, email, auth);
        free(nick);
        free(first);
        free(last);
        free(email);
        break;
      case META_SRV_USER_INFO: /* finished! */
        nick = icq_PacketReadStringNew(p);
        first = icq_PacketReadStringNew(p);
        last = icq_PacketReadStringNew(p);
        pri_eml = icq_PacketReadStringNew(p);
        sec_eml = icq_PacketReadStringNew(p);
        old_eml = icq_PacketReadStringNew(p);
        city = icq_PacketReadStringNew(p);
        state = icq_PacketReadStringNew(p);
        phone = icq_PacketReadStringNew(p);
        fax = icq_PacketReadStringNew(p);
        street = icq_PacketReadStringNew(p);
        cellular = icq_PacketReadStringNew(p);
        zip = icq_PacketRead32(p);
        country = icq_PacketRead16(p);
        timezone = icq_PacketRead8(p);         /* +1 = -30min, -1 = +30min (-4 = GMT+0200) */
        auth = icq_PacketRead8(p);             /* 1 - no auth required, 0 - required */
        webaware = icq_PacketRead8(p);         /* 1 - yes, 0 - no */
        hideip = icq_PacketRead8(p);           /* 1 - yes, 0 - no */
        icq_RusConv("wk", nick);
        icq_RusConv("wk", first);
        icq_RusConv("wk", last);
        icq_RusConv("wk", pri_eml);
        icq_RusConv("wk", sec_eml);
        icq_RusConv("wk", old_eml);
        icq_RusConv("wk", city);
        icq_RusConv("wk", state);
        icq_RusConv("wk", phone);
        icq_RusConv("wk", fax);
        icq_RusConv("wk", street);
        icq_RusConv("wk", cellular);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Info: %s, %s, %s, "\
                   "%s, %s, %s, %s, %s, %s, %s, %s, %s, %lu, %s, %i, %s, %s, %s\n",
                   nick, first, last, pri_eml, sec_eml, old_eml, city, state, phone,
                   fax, street, cellular, zip, icq_GetCountryName(country), timezone,
                   auth?"false":"true", webaware?"true":"false", hideip?"true":"false");
        invoke_callback(icqlink,icq_MetaUserInfo)(icqlink, seq2, nick, first, last, pri_eml,
                                                  sec_eml, old_eml, city, state, phone, fax,
                                                  street, cellular, zip, country, timezone,
                                                  auth, webaware, hideip);
        free(nick);
        free(first);
        free(last);
        free(pri_eml);
        free(sec_eml);
        free(old_eml);
        free(city);
        free(state);
        free(phone);
        free(fax);
        free(street);
        free(cellular);
        break;
      case META_SRV_USER_WORK: /* finished! */
        wcity = icq_PacketReadStringNew(p);
        wstate = icq_PacketReadStringNew(p);
        wphone = icq_PacketReadStringNew(p);
        wfax = icq_PacketReadStringNew(p);
        waddress = icq_PacketReadStringNew(p);
        wzip = icq_PacketRead32(p);
        wcountry = icq_PacketRead16(p);          /* icq_GetCountryName() */
        company = icq_PacketReadStringNew(p);
        department = icq_PacketReadStringNew(p);
        job = icq_PacketReadStringNew(p);
        occupation = icq_PacketRead16(p);        /* icq_GetMetaOccupationName() */
        whomepage = icq_PacketReadStringNew(p);
        icq_RusConv("wk", wcity);
        icq_RusConv("wk", wstate);
        icq_RusConv("wk", wphone);
        icq_RusConv("wk", wfax);
        icq_RusConv("wk", waddress);
        icq_RusConv("wk", company);
        icq_RusConv("wk", department);
        icq_RusConv("wk", job);
        icq_RusConv("wk", whomepage);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Work: %s, %s, %s, "\
                   "%s, %s,   %lu, %s, %s, %s, %s, %s, %s\n", wcity, wstate,
                   wphone, wfax, waddress, wzip, icq_GetCountryName(wcountry),
                   company, department, job, icq_GetMetaOccupationName(occupation),
                   whomepage);
        invoke_callback(icqlink, icq_MetaUserWork)(icqlink, seq2, wcity, wstate, wphone,
                                                   wfax, waddress, wzip, wcountry, company,
                                                   department, job, occupation, whomepage);
        free(wcity);
        free(wstate);
        free(wphone);
        free(wfax);
        free(waddress);
        free(company);
        free(department);
        free(job);
        free(whomepage);
        break;
      case META_SRV_USER_MORE: /* finished! */
        age = icq_PacketRead16(p);    /* 0xFFFF - not entered */
        gender = icq_PacketRead8(p);  /* 1 - female, 2 - male */
        homepage = icq_PacketReadStringNew(p);
        byear = icq_PacketRead8(p);   /* starting from 1900 */
        bmonth = icq_PacketRead8(p);
        bday = icq_PacketRead8(p);
        lang1 = icq_PacketRead8(p);   /* icq_GetMetaLanguageName() */
        lang2 = icq_PacketRead8(p);   /* icq_GetMetaLanguageName() */
        lang3 = icq_PacketRead8(p);   /* icq_GetMetaLanguageName() */
        icq_RusConv("wk", homepage);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User More: %i, %s, %s, "\
                   "%02i/%02i/%04i, %s, %s, %s\n", age,
                   gender==1?"female":gender==2?"male":"not entered",
                   homepage, bday, bmonth, byear+1900, icq_GetMetaLanguageName(lang1),
                   icq_GetMetaLanguageName(lang2), icq_GetMetaLanguageName(lang3));
        if(icqlink->icq_MetaUserMore)
          (*icqlink->icq_MetaUserMore)(icqlink, seq2, age, gender, homepage, byear,
                                    bmonth, bday, lang1, lang2, lang3);
        free(homepage);
        break;
      case META_SRV_USER_ABOUT: /* finished! */
        about = icq_PacketReadStringNew(p);
        icq_RusConv("wk", about);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User About: %s\n", about);
        invoke_callback(icqlink,icq_MetaUserAbout)(icqlink, seq2, about);
        free(about);
        break;
      case META_SRV_USER_INTERESTS: /* finished! */
        inum = icq_PacketRead8(p);
        for(i=0; i<inum && i<4; i++)
        {
          icategory[i] = icq_PacketRead16(p);
          interests[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", interests[i]);
        }
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Interests: %i, %i - %s, "\
                   "%i - %s, %i - %s, %i - %s\n", inum, icategory[0],
                   interests[0]?interests[0]:empty, icategory[1], interests[1]?interests[1]:empty,
                   icategory[2], interests[2]?interests[2]:empty, icategory[3],
                   interests[3]?interests[3]:empty);
        invoke_callback(icqlink, icq_MetaUserInterests)(icqlink, seq2, inum,
                                                        icategory[0], interests[0], icategory[1], 
                                                        interests[1], icategory[2], interests[2],
                                                        icategory[3], interests[3]);
        for(i=0; i<inum && i<4; i++)
          free(interests[i]);
        break;
      case META_SRV_USER_AFFILIATIONS: /* finished! */
        bnum = icq_PacketRead8(p);
        for(i=0; i<bnum && i<4; i++)
        {
          bcategory[i] = icq_PacketRead16(p);           /* icq_GetMetaBackgroundName() */
          backgrounds[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", backgrounds[i]);
        }
        anum = icq_PacketRead8(p);
        for(i=0; i<anum && i<4; i++)
        {
          acategory[i] = icq_PacketRead16(p);           /* icq_GetMetaAffiliationName() */
          affiliations[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", affiliations[i]);
        }
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Affiliations: %i, %s - %s, "\
                   "%s - %s, %s - %s, %s - %s; Backgrounds: %i, %s - %s, %s - %s, "\
                   "%s - %s, %s - %s\n", anum,
                   icq_GetMetaAffiliationName(acategory[0]), affiliations[0]?affiliations[0]:empty,
                   icq_GetMetaAffiliationName(acategory[1]), affiliations[1]?affiliations[1]:empty,
                   icq_GetMetaAffiliationName(acategory[2]), affiliations[2]?affiliations[2]:empty,
                   icq_GetMetaAffiliationName(acategory[3]), affiliations[3]?affiliations[3]:empty,
                   bnum, icq_GetMetaBackgroundName(bcategory[0]), backgrounds[0]?backgrounds[0]:empty,
                   icq_GetMetaBackgroundName(bcategory[1]), backgrounds[1]?backgrounds[1]:empty,
                   icq_GetMetaBackgroundName(bcategory[2]), backgrounds[2]?backgrounds[2]:empty,
                   icq_GetMetaBackgroundName(bcategory[3]), backgrounds[3]?backgrounds[3]:empty);
        invoke_callback(icqlink, icq_MetaUserAffiliations)(icqlink, seq2, anum, acategory[0],
                                                           affiliations[0], acategory[1], affiliations[1],
                                                           acategory[2], affiliations[2], acategory[3],
                                                           affiliations[3], bnum, bcategory[0],
                                                           backgrounds[0], bcategory[1], backgrounds[1],
                                                           bcategory[2], backgrounds[2], bcategory[3],
                                                           backgrounds[3]);
        for(i=0; i<bnum && i<4; i++)
          free(backgrounds[i]);
        for(i=0; i<anum && i<4; i++)
          free(affiliations[i]);
        break;
      case META_SRV_USER_HPCATEGORY: /* finished! */
        hnum = icq_PacketRead8(p);
        for(i=0; i<hnum && i<1; i++)
        {
          hcategory[i] = icq_PacketRead16(p);
          hpcat[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", hpcat[i]);
        }
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User Homepage Category: %i, %i - %s\n",
                   hnum, hcategory[0], hpcat[0]);
        invoke_callback(icqlink,icq_MetaUserHomePageCategory)(icqlink, seq2,
                                                              hnum, hcategory[0], hpcat[0]?hpcat[0]:empty);
        for(i=0; i<hnum && i<1; i++)
          free(hpcat[i]);
        break;
      case META_SRV_RES_INFO:
      case META_SRV_RES_HOMEPAGE:
      case META_SRV_RES_ABOUT:
      case META_SRV_RES_SECURE:
      case META_SRV_RES_PASS:
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META success\n");
        invoke_callback(icqlink,icq_RequestNotify)(icqlink, seq2, ICQ_NOTIFY_SUCCESS,
                                                   sizeof(subcmd), &subcmd);
        break;
      default:
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "META User - 0x%04X\n", subcmd);
        icq_PacketUDPDump(p);
        break;
    }
  icq_UDPAck(icqlink, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleMultiPacket(icq_Link *icqlink, icq_Packet *p)
{
  icq_Packet *tmp;
  int num, i;
  icq_PacketGotoUDPInData(p, 0);
  num = icq_PacketRead8(p);

  icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "MultiPacket: %i packets\n", num);

  for(i = 0; i < num; i++)
  {
    tmp = icq_PacketNew();
    tmp->length = icq_PacketRead16(p);
    memcpy(tmp->data, &(p->data[p->cursor]), tmp->length);
    icq_PacketAdvance(p, tmp->length);
    icq_ServerResponse(icqlink, tmp);
    icq_PacketDelete(tmp);
  }
}

void icq_ServerResponse(icq_Link *icqlink, icq_Packet *p)
{
  time_t cur_time;
  struct tm *tm_str;
  int len;
  struct in_addr in_a;
  DWORD uin;
  WORD year, type, seq, cmd;
  BYTE month, day, hour, minute;

  seq = icq_PacketReadUDPInSeq1(p);
  cmd = icq_PacketReadUDPInCmd(p);

  if(icq_PacketReadUDPInVer(p) == 5) /* We understand only V5 packets! */
  {
    switch(cmd)
    {
      case UDP_SRV_ACK:
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "The server acknowledged the command\n");
        invoke_callback(icqlink,icq_RequestNotify)(icqlink, seq, ICQ_NOTIFY_ACK, 0, 0);
        invoke_callback(icqlink,icq_RequestNotify)(icqlink, seq, ICQ_NOTIFY_SUCCESS, 0, 0);
        icq_UDPQueueDelSeq(icqlink, seq);
        break;
      case UDP_SRV_MULTI_PACKET:
        icq_HandleMultiPacket(icqlink, p);
        break;
      case UDP_SRV_NEW_UIN:
        uin = icq_PacketReadUDPInUIN(p);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "The new uin is %lu\n", uin);
        icq_UDPAck(icqlink, seq);
        invoke_callback(icqlink,icq_NewUIN)(icqlink, uin);
        break;
      case UDP_SRV_LOGIN_REPLY:
        icq_PacketGotoUDPInData(p, 0);
        icqlink->icq_OurIP = ntohl(icq_PacketRead32n(p));
/*       icq_OurIp = icq_PacketRead32(p); */
        in_a.s_addr = htonl(icqlink->icq_OurIP);
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Login successful, UIN: %lu, IP: %s\n",
                   icqlink->icq_Uin, inet_ntoa(in_a));
        icq_UDPAck(icqlink, seq);
        icq_SendLogin1(icqlink);
        icq_SendContactList(icqlink);
        icq_SendVisibleList(icqlink);
        invoke_callback(icqlink,icq_Logged)(icqlink);
        break;
      case UDP_SRV_OFFLINE_MESSAGE: /* Offline message through the server */
        icq_PacketGotoUDPInData(p, 0);
        uin = icq_PacketRead32(p);
        year = icq_PacketRead16(p);
        month = icq_PacketRead8(p);
        day = icq_PacketRead8(p);
        hour = icq_PacketRead8(p);
        minute = icq_PacketRead8(p);
        type = icq_PacketRead16(p);
        len = icq_PacketRead16(p);
        icq_DoMsg(icqlink, type, len, (char*)&p->data[p->cursor], uin, hour, minute, day, month, year);
        icq_UDPAck(icqlink, seq);
        break;
      case UDP_SRV_X1: /* unknown message sent after login*/
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Acknowleged UDP_SRV_X1 (Begin messages)\n");
        icq_UDPAck(icqlink, seq);
        break;
      case UDP_SRV_X2: /* unknown message sent after login*/
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Acknowleged UDP_SRV_X2 (Done old messages)\n");
        icq_UDPAck(icqlink, seq);
        icq_SendGotMessages(icqlink);
        break;
      case UDP_SRV_INFO_REPLY:
        icq_HandleInfoReply(icqlink, p);
        break;
      case UDP_SRV_EXT_INFO_REPLY:
        icq_HandleExtInfoReply(icqlink, p);
        break;
      case UDP_SRV_USER_ONLINE:
        icq_HandleUserOnline(icqlink, p);
        break;
      case UDP_SRV_USER_OFFLINE:
        icq_HandleUserOffline(icqlink, p);
        break;
      case UDP_SRV_TRY_AGAIN:
        icq_FmtLog(icqlink, ICQ_LOG_WARNING, "Server is busy, please try again\n");
        icq_Login(icqlink, icqlink->icq_Status);
        break;
      case UDP_SRV_STATUS_UPDATE:
        icq_HandleStatusChange(icqlink, p);
        break;
      case UDP_SRV_GO_AWAY:
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Server has forced us to disconnect\n");
        if(icqlink->icq_Disconnected)
          (*icqlink->icq_Disconnected)(icqlink);
        break;
      case UDP_SRV_END_OF_SEARCH:
        icq_FmtLog(icqlink, ICQ_LOG_MESSAGE, "Search done\n");
        invoke_callback(icqlink,icq_SearchDone)(icqlink);
        icq_UDPAck(icqlink, seq);
        break;
      case UDP_SRV_USER_FOUND:
        icq_HandleSearchReply(icqlink, p);
        break;
      case UDP_SRV_ONLINE_MESSAGE: /* Online message through the server */
        cur_time = time(0L);
        tm_str = localtime(&cur_time);
        icq_PacketGotoUDPInData(p, 0);
        uin = icq_PacketRead32(p);
        type = icq_PacketRead16(p);
        len = icq_PacketRead16(p);
        icq_DoMsg(icqlink, type, len, (char*)&p->data[p->cursor], uin, tm_str->tm_hour,
                  tm_str->tm_min, tm_str->tm_mday, tm_str->tm_mon+1, tm_str->tm_year+1900);
        icq_UDPAck(icqlink, seq);
        break;
      case UDP_SRV_WRONG_PASSWORD:
        icq_FmtLog(icqlink, ICQ_LOG_ERROR, "Wrong password\n");
        icq_UDPAck(icqlink, seq);
        invoke_callback(icqlink,icq_WrongPassword)(icqlink);
        break;
      case UDP_SRV_INVALID_UIN:
        icq_FmtLog(icqlink, ICQ_LOG_WARNING, "Invalid UIN\n");
        icq_UDPAck(icqlink, seq);
        invoke_callback(icqlink,icq_InvalidUIN)(icqlink);
        break;
      case UDP_SRV_META_USER:
        icq_HandleMetaUserInfo(icqlink, p);
        break;
      default: /* commands we dont handle yet */
        icq_FmtLog(icqlink, ICQ_LOG_WARNING, "Unhandled message %04x, Version: %x, "
                   "Sequence: %04x, Size: %d\n", cmd, icq_PacketReadUDPInVer(p),
                   seq, p->length);
        icq_UDPAck(icqlink, seq); /* fake like we know what we're doing */
        break;
    }
  }
  else
  {
    icq_FmtLog(icqlink, ICQ_LOG_WARNING, "Unhandled protocol version! Message %04x, Version: %x, "
               "Sequence: %04x, Size: %d\n", cmd, icq_PacketReadUDPInVer(p),
               seq, p->length);
/*    icq_UDPAck(icqlink, seq);  DO NOT ACK unhandled protocol version! */
  }
}

/******************************************
Handles packets that the server sends to us.
*******************************************/
void icq_HandleServerResponse(icq_Link *icqlink)
{
  WORD seq, cmd;
  int s;
  icq_Packet *p;

  p = icq_PacketNew();
  s = icq_UDPSockRead(icqlink, p);
  p->length = s;
  if(s<=0)
  {
    icq_FmtLog(icqlink, ICQ_LOG_FATAL, "Connection terminated\n");
    icq_Disconnect(icqlink);
    invoke_callback(icqlink,icq_Disconnected)(icqlink);
  }
  seq = icq_PacketReadUDPInSeq1(p);
  cmd = icq_PacketReadUDPInCmd(p);
  if(icq_GetServMess(icqlink, seq) && cmd != UDP_SRV_NEW_UIN && cmd != UDP_SRV_GO_AWAY && cmd != UDP_SRV_ACK)
  {
    icq_FmtLog(icqlink, ICQ_LOG_WARNING, "Ignored a message cmd %04x, seq %04x\n", cmd, seq);
    icq_UDPAck(icqlink, seq); /* LAGGGGG!! */
    icq_PacketDelete(p);
    return;
  }
  if(cmd != UDP_SRV_ACK)
    icq_SetServMess(icqlink, seq);

  icq_ServerResponse(icqlink, p);

  icq_PacketDelete(p);
}

/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
$Id: udphandle.c 1871 2001-05-19 17:57:49Z warmenhoven $
$Log$
Revision 1.5  2001/05/19 17:57:49  warmenhoven
this is not an "official" fix

Revision 1.4  2001/02/22 23:07:34  warmenhoven
updating icqlib

Revision 1.31  2001/02/22 05:40:04  bills
port tcp connect timeout code and UDP queue to new timeout manager

Revision 1.30  2001/01/15 06:17:35  denis
Applied patch from Andrey Chernomyrdin <andrey@excom.spb.su> to
handle icq2000 specific "You've been added" packet.

Revision 1.29  2000/11/02 07:28:30  denis
Do not ack unhandled protocol version.

Revision 1.28  2000/06/25 17:25:52  denis
icq_HandleMetaUserInfo() handles all (!) available in original icq2k
Meta User Info packets along with other useful Meta* packets. (WOW! ;)

Revision 1.27  2000/05/10 18:54:43  denis
icq_Disconnect() now called before icq_Disconnected callback to
prevent high CPU usage in kicq's "reconnect on disconnect" code.

Revision 1.26  2000/05/03 18:29:15  denis
Callbacks have been moved to the ICQLINK structure.

Revision 1.25  2000/02/07 02:48:15  bills
changed log message in HandleUserOnline

Revision 1.24  2000/01/16 03:59:10  bills
reworked list code so list_nodes don't need to be inside item structures,
removed strlist code and replaced with generic list calls

Revision 1.23  1999/12/27 11:12:37  denis
icq_UpdateMetaInfoSecurity() added for setting "My authorization is
required", "Web Aware" and "IP Publishing".

Revision 1.22  1999/12/14 03:37:06  bills
removed old real_ip->ip masq hack, added store to remote_real_ip in
icq_ContactItem

Revision 1.21  1999/12/12 18:03:58  denis
Authorization Request packet handling fixed.

Revision 1.20  1999/11/29 17:18:31  denis
icq_DoMsg() redone using string lists.

Revision 1.18  1999/11/11 15:10:32  guruz
- Added Base for Webpager Messages. Please type "make fixme"
- Removed Segfault when kicq is started the first time

Revision 1.17  1999/10/14 11:44:04  denis
Cleanups.

Revision 1.16  1999/09/29 17:16:08  denis
MailExpress message handler started.

Revision 1.15  1999/07/18 20:23:54  bills
fixed tcp port bug in icq_HandleUserOnline, changed to use new byte-order
& contact list functions

Revision 1.14  1999/07/16 15:46:02  denis
Cleaned up.

Revision 1.13  1999/07/16 12:40:55  denis
ICQ UDP v5 implemented.
Encription for ICQ UDP v5 implemented.
icq_Packet* unified interface for UDP packets implemented.
Multipacket support of ICQ UDP v5 support added.

Revision 1.12  1999/07/12 15:13:45  cproch
- added definition of ICQLINK to hold session-specific global variabled
  applications which have more than one connection are now possible
- changed nearly every function defintion to support ICQLINK parameter

Revision 1.11  1999/07/03 06:46:54  lord
converting icq_userOnline callback parameters to correct
byte order.

Revision 1.10  1999/07/03 02:29:46  bills
added code to HandleUserOnline to update tcp_flag

Revision 1.9  1999/04/29 09:40:54  denis
Unsuccessful attempt to implement web presence (webaware) feature

Revision 1.8  1999/04/18 01:58:37  bills
changed icq_SrvAck call to icq_RequestNotify

Revision 1.7  1999/04/17 19:26:49  bills
removed *_link entries from icq_ContactItem, including cleaup/init code

Revision 1.6  1999/04/14 15:05:39  denis
Cleanups for "strict" compiling (-ansi -pedantic)
Switched from icq_Log callback to icq_Fmt function.

Revision 1.5  1999/04/05 18:47:23  bills
initial chat support implemented

Revision 1.4  1999/03/31 01:39:50  bills
added handling of tcp_flag in HandleLogin

Revision 1.3  1999/03/28 03:35:17  bills
fixed function names so icqlib compiles, fixed bug in HandleUserOnline
(remote_ip and remote_real_ip were not evaluating correctly), added
hack so I can test using local network

Revision 1.2  1999/03/25 22:25:02  bills
modified icq_HandleUserOnline & Offline for new message_link

Revision 1.1  1999/03/24 11:37:38  denis
Underscored files with TCP stuff renamed.
TCP stuff cleaned up
Function names changed to corresponding names.
icqlib.c splitted to many small files by subject.
C++ comments changed to ANSI C comments.

*/

#include <stdlib.h>

#include "icqtypes.h"
#include "icq.h"
#include "icqlib.h"
#include "udp.h"
#include "icqpacket.h"
#include "queue.h"
#include "icqbyteorder.h"
#include "list.h"

int icq_SplitFields(list *strList, const char *str)
{
  char *tmpBuf, *tmp, *ptr;
  int count = 0;

  tmpBuf = (char*)malloc(strlen(str)+1);
  strcpy(tmpBuf, str);
  ptr = tmpBuf;

  while(ptr)
  {
    char *p;
    tmp = strchr(ptr, 0xFE);
    if(tmp != 0L)
    {
      *tmp = 0;
      tmp++;
    }
    count++;
    p = (char *)malloc(strlen(ptr)+1);
    strcpy(p, ptr);
    list_enqueue(strList, p);
    ptr = tmp;
  }

  free(tmpBuf);
  return count;
}

void icq_DoMsg(ICQLINK *link, DWORD type, WORD len, char *data, DWORD uin, BYTE hour,
               BYTE minute, BYTE day, BYTE month, WORD year)
{
  list *strList;
  int fieldCount;

  strList = list_new();
  switch(type)
  {
    case TYPE_ADDED:
      /* Format: Nick, 0xFE, FName, 0xFE, LName, 0xFE, EMail */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 4 && fieldCount != 5)
      {
        icq_FmtLog(link, ICQ_LOG_ERROR, "Bad TYPE_ADDED packet (expected 4/5 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", list_at(strList, 0)); /* Nick */
      icq_RusConv("wk", list_at(strList, 1)); /* FName */
      icq_RusConv("wk", list_at(strList, 2)); /* LName */
      icq_RusConv("wk", list_at(strList, 3)); /* EMail */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "%lu has added you to their contact list, "
                 "Nick: %s, First Name: %s, Last Name: %s, EMail: %s\n",
                 uin, list_at(strList, 0), list_at(strList, 1),
                 list_at(strList, 2), list_at(strList, 3));
      if(link->icq_RecvAdded)
        (*link->icq_RecvAdded)(link, uin, hour, minute, day, month, year,
                         list_at(strList, 0), list_at(strList, 1),
                         list_at(strList, 2), list_at(strList, 3));
      break;
    case TYPE_AUTH_REQ:
      /* Format: Nick, 0xFE, FName, 0xFE, LName, 0xFE, EMail, 0xFE, 0, 0xFE, Reason */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(link, ICQ_LOG_ERROR, "Bad TYPE_AUTH_REQ packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", list_at(strList, 0)); /* Nick */
      icq_RusConv("wk", list_at(strList, 1)); /* FName */
      icq_RusConv("wk", list_at(strList, 2)); /* LName */
      icq_RusConv("wk", list_at(strList, 3)); /* EMail */
      icq_RusConv("wk", list_at(strList, 5)); /* Reason */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "%lu has requested your authorization to be added to "
                 "their contact list, Nick: %s, First Name: %s, Last Name: %s, "
                 "EMail: %s, Reason: %s\n", uin, list_at(strList, 0), list_at(strList, 1),
                 list_at(strList, 2), list_at(strList, 3), list_at(strList, 4));
      if(link->icq_RecvAuthReq)
        (*link->icq_RecvAuthReq)(link, uin, hour, minute, day, month, year, list_at(strList, 0),
                           list_at(strList, 1), list_at(strList, 2),
                           list_at(strList, 3), list_at(strList, 5));
      break;
    case TYPE_URL:
      /* Format: Description, 0xFE, URL */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 2)
      {
        icq_FmtLog(link, ICQ_LOG_ERROR, "Bad TYPE_URL packet (expected 2 args, recived %i)!\n", fieldCount);
        return;
      }
      icq_RusConv("wk", list_at(strList, 0)); /* Description */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "URL received from %lu, URL: %s, Description: %s\n",
                 uin, list_at(strList, 1), list_at(strList, 0));
      if(link->icq_RecvURL)
        (*link->icq_RecvURL)(link, uin, hour, minute, day, month, year, list_at(strList, 1),
                       list_at(strList, 0));
      break;
    case TYPE_WEBPAGER:
      /* Format: Nick, 0xFE, Empty-FName, 0xFE, Empty-LName, 0xFE, EMail, 0xFE,
       *         Reason(3), 0xFE, Message with IP & Subject */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(link, ICQ_LOG_ERROR, "Bad TYPE_WEBPAGER packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", list_at(strList, 0)); /* Nick */
      icq_RusConv("wk", list_at(strList, 5)); /* Message */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "WebPager message received, Nick: %s, EMail: %s, "
                 "Message:\n%s\n", list_at(strList, 0), list_at(strList, 3),
                 list_at(strList, 5));
      if(link->icq_RecvWebPager)
        (*link->icq_RecvWebPager)(link, hour, minute, day, month, year, list_at(strList, 0),
                                   list_at(strList, 3), list_at(strList, 5));
      break;
    case TYPE_EXPRESS:
      /* Format: Nick, 0xFE, Empty-FName, 0xFE, Empty-LName, 0xFE, EMail, 0xFE,
       *         Reason(3), 0xFE, Message Subject */
      fieldCount = icq_SplitFields(strList, data);
      if(fieldCount != 6)
      {
        icq_FmtLog(link, ICQ_LOG_ERROR, "Bad TYPE_EXPRESS packet (expected 6 args, received %i)!\n",
                   fieldCount);
        return;
      }
      icq_RusConv("wk", list_at(strList, 0)); /* Nick */
      icq_RusConv("wk", list_at(strList, 5)); /* Message */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "MailExpress message received, Nick: %s, EMail: %s, "
                 "Message:\n%s\n", list_at(strList, 0), list_at(strList, 3),
                 list_at(strList, 5));
      if(link->icq_RecvMailExpress)
        (*link->icq_RecvMailExpress)(link, hour, minute, day, month, year, list_at(strList, 0),
                               list_at(strList, 3), list_at(strList, 5));
      break;
    default:
      icq_RusConv("wk", data); /* Entire message */
      icq_FmtLog(link, ICQ_LOG_MESSAGE, "Instant message type %i from %lu:\n%s\n", type, uin, data);
      if(link->icq_RecvMessage)
        (*link->icq_RecvMessage)(link, uin, hour, minute, day, month, year, data);
  }
  list_delete(strList, free);
}

void icq_HandleInfoReply(ICQLINK *link, icq_Packet *p)
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
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Info reply for %lu\n", uin);
  if(link->icq_InfoReply)
    (*link->icq_InfoReply)(link, uin, ptr1, ptr2, ptr3, ptr4, icq_PacketRead8(p));
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
}

void icq_HandleExtInfoReply(ICQLINK *link, icq_Packet *p)
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
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "Extended info reply for %lu\n", uin);
  if(link->icq_ExtInfoReply)
    (*link->icq_ExtInfoReply)(link, uin, (char*)ptr1, cnt_code, cnt_stat, (char*)ptr2, age,
                              gender, (char*)ptr3, (char*)ptr4, (char*)ptr5);
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
  free(ptr5);
}

void icq_HandleSearchReply(ICQLINK *link, icq_Packet *p)
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
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "User found %lu, Nick: %s, First Name: %s, Last Name: %s, "
             "EMail: %s, Auth: %s\n", uin, ptr1, ptr2, ptr3, ptr4, auth==1?"no":"yes");
  if(link->icq_UserFound)
    (*link->icq_UserFound)(link, uin, (char*)ptr1, (char*)ptr2, (char*)ptr3, (char*)ptr4, auth);
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
  free(ptr1);
  free(ptr2);
  free(ptr3);
  free(ptr4);
}

/************************************************
This is called when a user goes offline
*************************************************/
void icq_HandleUserOffline(ICQLINK *link, icq_Packet *p)
{
  DWORD remote_uin;
  icq_ContactItem *ptr;

  icq_PacketGotoUDPInData(p, 0);
  remote_uin = icq_PacketRead32(p);
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "User %lu logged off\n", remote_uin);
  if(link->icq_UserOffline)
    (*link->icq_UserOffline)(link, remote_uin);

  ptr=icq_ContactFind(link, remote_uin);
  if(ptr)
  {
    ptr->remote_ip = 0;
    ptr->remote_port = 0;
  }
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleUserOnline(ICQLINK *link, icq_Packet *p)
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

  icq_FmtLog(link, ICQ_LOG_MESSAGE,
             "User %lu (%s = 0x%X) logged on. tcp_flag=0x%X IP=%08X, real IP=%08X, port=%d\n",
             remote_uin, icq_ConvertStatus2Str(new_status), new_status, tcp_flag, remote_ip,
             remote_real_ip, remote_port);
  if(link->icq_UserOnline)
    (*link->icq_UserOnline)(link, remote_uin, new_status, remote_ip, remote_port, remote_real_ip, tcp_flag);

  ptr=icq_ContactFind(link, remote_uin);
  if(ptr)
  {
    ptr->remote_ip=remote_ip;
    ptr->remote_real_ip=remote_real_ip;
    ptr->remote_port = remote_port;
    ptr->tcp_flag = tcp_flag;
  }
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleStatusChange(ICQLINK *link, icq_Packet *p)
{
  unsigned long remote_uin, new_status;

  icq_PacketGotoUDPInData(p, 0);
  remote_uin = icq_PacketRead32(p);
  new_status = icq_PacketRead32(p);
  icq_FmtLog(link, ICQ_LOG_MESSAGE, "%lu changed status to %s (0x%X)\n", remote_uin,
             icq_ConvertStatus2Str(new_status), new_status);
  if(link->icq_UserStatusUpdate)
    (*link->icq_UserStatusUpdate)(link, remote_uin, new_status);
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleMetaUserInfo(ICQLINK *link, icq_Packet *p)
{
  unsigned short subcmd, country, seq2, age, occupation, wcountry;
  unsigned char res, auth, timezone, webaware, hideip, gender;
  unsigned char *nick, *first, *last, *email, *about, *city;
  unsigned char *pri_eml, *sec_eml, *old_eml;
  unsigned char *phone, *fax, *street, *cellular, *state;
  unsigned char *wcity, *wstate, *wphone, *wfax, *waddress;
  unsigned char *company, *department, *job, *whomepage;
  unsigned char *homepage;
  unsigned char byear, bmonth, bday, lang1, lang2, lang3, inum, i;
  unsigned char anum, bnum, hnum;
  unsigned long uin, zip, wzip;
  unsigned char *empty = "";
  unsigned char *interests[4] = {0, 0, 0, 0};
  unsigned short icategory[4] = {0, 0, 0, 0};
  unsigned char *affiliations[4] = {0, 0, 0, 0};
  unsigned short acategory[4] = {0, 0, 0, 0};
  unsigned char *backgrounds[4] = {0, 0, 0, 0};
  unsigned short bcategory[4] = {0, 0, 0, 0};
  unsigned char *hpcat[4] = {0, 0, 0, 0};
  unsigned short hcategory[4] = {0, 0, 0, 0};

  seq2 = icq_PacketReadUDPInSeq2(p);
  icq_PacketGotoUDPInData(p, 0);
  subcmd = icq_PacketRead16(p);
  res = icq_PacketRead8(p);
  if(res == META_SRV_FAILURE)
  {
    icq_FmtLog(link, ICQ_LOG_WARNING, "META failure\n");
    if(link->icq_RequestNotify)
      (*link->icq_RequestNotify)(link, seq2, ICQ_NOTIFY_FAILED, sizeof(subcmd), &subcmd);
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
        icq_PacketRead16(p); // ???
        icq_PacketRead32(p); // ???
        icq_RusConv("wk", nick);
        icq_RusConv("wk", first);
        icq_RusConv("wk", last);
        icq_RusConv("wk", email);
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Found %lu, Nick: %s, First Name: %s, "\
                   "Last Name: %s, EMail: %s, Auth: %s\n", seq2, uin, nick, first, last,
                   email, auth==1?"no":"yes");
        if(link->icq_MetaUserFound)
          (*link->icq_MetaUserFound)(link, seq2, uin, nick, first, last, email, auth);
        free(nick);
        free(first);
        free(last);
        free(email);
        break;
      case META_SRV_USER_INFO: // finished!
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
        timezone = icq_PacketRead8(p);         // +1 = -30min, -1 = +30min (-4 = GMT+0200)
        auth = icq_PacketRead8(p);             // 1 - no auth required, 0 - required
        webaware = icq_PacketRead8(p);         // 1 - yes, 0 - no
        hideip = icq_PacketRead8(p);           // 1 - yes, 0 - no
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
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Info: %s, %s, %s, "\
                   "%s, %s, %s, %s, %s, %s, %s, %s, %s, %lu, %s, %i, %s, %s, %s\n",
                   nick, first, last, pri_eml, sec_eml, old_eml, city, state, phone,
                   fax, street, cellular, zip, icq_GetCountryName(country), timezone,
                   auth?"false":"true", webaware?"true":"false", hideip?"true":"false");
        if(link->icq_MetaUserInfo)
          (*link->icq_MetaUserInfo)(link, seq2, nick, first, last, pri_eml, sec_eml,
                                    old_eml, city, state, phone, fax, street, cellular,
                                    zip, country, timezone, auth, webaware, hideip);
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
      case META_SRV_USER_WORK: // finished!
        wcity = icq_PacketReadStringNew(p);
        wstate = icq_PacketReadStringNew(p);
        wphone = icq_PacketReadStringNew(p);
        wfax = icq_PacketReadStringNew(p);
        waddress = icq_PacketReadStringNew(p);
        wzip = icq_PacketRead32(p);
        wcountry = icq_PacketRead16(p);          // icq_GetCountryName()
        company = icq_PacketReadStringNew(p);
        department = icq_PacketReadStringNew(p);
        job = icq_PacketReadStringNew(p);
        occupation = icq_PacketRead16(p);        // icq_GetMetaOccupationName()
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
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Work: %s, %s, %s, "\
                   "%s, %s,   %lu, %s, %s, %s, %s, %s, %s\n", wcity, wstate,
                   wphone, wfax, waddress, wzip, icq_GetCountryName(wcountry),
                   company, department, job, icq_GetMetaOccupationName(occupation),
                   whomepage);
        if(link->icq_MetaUserWork)
          (*link->icq_MetaUserWork)(link, seq2, wcity, wstate, wphone, wfax,
                                    waddress, wzip, wcountry, company, department,
                                    job, occupation, whomepage);
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
      case META_SRV_USER_MORE: // finished!
        age = icq_PacketRead16(p);    // 0xFFFF - not entered
        gender = icq_PacketRead8(p);  // 1 - female, 2 - male
        homepage = icq_PacketReadStringNew(p);
        byear = icq_PacketRead8(p);   // starting from 1900
        bmonth = icq_PacketRead8(p);
        bday = icq_PacketRead8(p);
        lang1 = icq_PacketRead8(p);   // icq_GetMetaLanguageName()
        lang2 = icq_PacketRead8(p);   // icq_GetMetaLanguageName()
        lang3 = icq_PacketRead8(p);   // icq_GetMetaLanguageName()
        icq_RusConv("wk", homepage);
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User More: %i, %s, %s, "\
                   "%02i/%02i/%04i, %s, %s, %s\n", age,
                   gender==1?"female":gender==2?"male":"not entered",
                   homepage, bday, bmonth, byear+1900, icq_GetMetaLanguageName(lang1),
                   icq_GetMetaLanguageName(lang2), icq_GetMetaLanguageName(lang3));
        if(link->icq_MetaUserMore)
          (*link->icq_MetaUserMore)(link, seq2, age, gender, homepage, byear,
                                    bmonth, bday, lang1, lang2, lang3);
        free(homepage);
        break;
      case META_SRV_USER_ABOUT: // finished!
        about = icq_PacketReadStringNew(p);
        icq_RusConv("wk", about);
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User About: %s\n", about);
        if(link->icq_MetaUserAbout)
          (*link->icq_MetaUserAbout)(link, seq2, about);
        free(about);
        break;
      case META_SRV_USER_INTERESTS: // finished!
        inum = icq_PacketRead8(p);
        for(i=0; i<inum && i<4; i++)
        {
          icategory[i] = icq_PacketRead16(p);
          interests[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", interests[i]);
        }
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Interests: %i, %i - %s, "\
                   "%i - %s, %i - %s, %i - %s\n", inum, icategory[0],
                   interests[0]?interests[0]:empty, icategory[1], interests[1]?interests[1]:empty,
                   icategory[2], interests[2]?interests[2]:empty, icategory[3],
                   interests[3]?interests[3]:empty);
        if(link->icq_MetaUserInterests)
          (*link->icq_MetaUserInterests)(link, seq2, inum, icategory[0], interests[0],
                                         icategory[1], interests[1], icategory[2],
                                         interests[2], icategory[3], interests[3]);
        for(i=0; i<inum && i<4; i++)
          free(interests[i]);
        break;
      case META_SRV_USER_AFFILIATIONS: // finished!
        bnum = icq_PacketRead8(p);
        for(i=0; i<bnum && i<4; i++)
        {
          bcategory[i] = icq_PacketRead16(p);           // icq_GetMetaBackgroundName()
          backgrounds[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", backgrounds[i]);
        }
        anum = icq_PacketRead8(p);
        for(i=0; i<anum && i<4; i++)
        {
          acategory[i] = icq_PacketRead16(p);           // icq_GetMetaAffiliationName()
          affiliations[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", affiliations[i]);
        }
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Affiliations: %i, %s - %s, "\
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
        if(link->icq_MetaUserAffiliations)
          (*link->icq_MetaUserAffiliations)(link, seq2, anum, acategory[0],
                  affiliations[0], acategory[1], affiliations[1], acategory[2],
                  affiliations[2], acategory[3], affiliations[3], bnum,
                  bcategory[0], backgrounds[0], bcategory[1], backgrounds[1],
                  bcategory[2], backgrounds[2], bcategory[3], backgrounds[3]);
        for(i=0; i<bnum && i<4; i++)
          free(backgrounds[i]);
        for(i=0; i<anum && i<4; i++)
          free(affiliations[i]);
        break;
      case META_SRV_USER_HPCATEGORY: // finished!
        hnum = icq_PacketRead8(p);
        for(i=0; i<hnum && i<1; i++)
        {
          hcategory[i] = icq_PacketRead16(p);
          hpcat[i] = icq_PacketReadStringNew(p);
          icq_RusConv("wk", hpcat[i]);
        }
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User Homepage Category: %i, %i - %s\n",
                   hnum, hcategory[0], hpcat[0]);
        if(link->icq_MetaUserHomePageCategory)
          (*link->icq_MetaUserHomePageCategory)(link, seq2, hnum, hcategory[0], hpcat[0]?hpcat[0]:empty);
        for(i=0; i<hnum && i<1; i++)
          free(hpcat[i]);
        break;
      case META_SRV_RES_INFO:
      case META_SRV_RES_HOMEPAGE:
      case META_SRV_RES_ABOUT:
      case META_SRV_RES_SECURE:
      case META_SRV_RES_PASS:
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META success\n");
        if(link->icq_RequestNotify)
          (*link->icq_RequestNotify)(link, seq2, ICQ_NOTIFY_SUCCESS, sizeof(subcmd), &subcmd);
        break;
      default:
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "META User - 0x%04X\n", subcmd);
        icq_PacketUDPDump(p);
        break;
    }
  icq_UDPAck(link, icq_PacketReadUDPInSeq1(p));
}

void icq_HandleMultiPacket(ICQLINK *link, icq_Packet *p)
{
  icq_Packet *tmp;
  int num, i;
  icq_PacketGotoUDPInData(p, 0);
  num = icq_PacketRead8(p);

  icq_FmtLog(link, ICQ_LOG_MESSAGE, "MultiPacket: %i packets\n", num);

  for(i = 0; i < num; i++)
  {
    tmp = icq_PacketNew();
    tmp->length = icq_PacketRead16(p);
    memcpy(tmp->data, &(p->data[p->cursor]), tmp->length);
    icq_PacketAdvance(p, tmp->length);
    icq_ServerResponse(link, tmp);
    icq_PacketDelete(tmp);
  }
}

void icq_ServerResponse(ICQLINK *link, icq_Packet *p)
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
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "The server acknowledged the command\n");
        if(link->icq_RequestNotify)
        {
          (*link->icq_RequestNotify)(link, seq, ICQ_NOTIFY_ACK, 0, 0);
          (*link->icq_RequestNotify)(link, seq, ICQ_NOTIFY_SUCCESS, 0, 0);
        }
        icq_UDPQueueDelSeq(link, seq);
        break;
      case UDP_SRV_MULTI_PACKET:
        icq_HandleMultiPacket(link, p);
        break;
      case UDP_SRV_NEW_UIN:
        uin = icq_PacketReadUDPInUIN(p);
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "The new uin is %lu\n", uin);
        icq_UDPAck(link, seq);
        if(link->icq_NewUIN)
          (*link->icq_NewUIN)(link, uin);
        break;
      case UDP_SRV_LOGIN_REPLY:
        icq_PacketGotoUDPInData(p, 0);
        link->icq_OurIP = ntohl(icq_PacketRead32n(p));
/*       icq_OurIp = icq_PacketRead32(p); */
        in_a.s_addr = htonl(link->icq_OurIP);
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "Login successful, UIN: %lu, IP: %s\n",
                   link->icq_Uin, inet_ntoa(in_a));
        icq_UDPAck(link, seq);
        icq_SendLogin1(link);
        icq_SendContactList(link);
        icq_SendVisibleList(link);
        if(link->icq_Logged)
          (*link->icq_Logged)(link);
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
        icq_DoMsg(link, type, len, (char*)&p->data[p->cursor], uin, hour, minute, day, month, year);
        icq_UDPAck(link, seq);
        break;
      case UDP_SRV_X1: /* unknown message sent after login*/
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "Acknowleged UDP_SRV_X1 (Begin messages)\n");
        icq_UDPAck(link, seq);
        break;
      case UDP_SRV_X2: /* unknown message sent after login*/
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "Acknowleged UDP_SRV_X2 (Done old messages)\n");
        icq_UDPAck(link, seq);
        icq_SendGotMessages(link);
        break;
      case UDP_SRV_INFO_REPLY:
        icq_HandleInfoReply(link, p);
        break;
      case UDP_SRV_EXT_INFO_REPLY:
        icq_HandleExtInfoReply(link, p);
        break;
      case UDP_SRV_USER_ONLINE:
        icq_HandleUserOnline(link, p);
        break;
      case UDP_SRV_USER_OFFLINE:
        icq_HandleUserOffline(link, p);
        break;
      case UDP_SRV_TRY_AGAIN:
        icq_FmtLog(link, ICQ_LOG_WARNING, "Server is busy, please try again\n");
        icq_Login(link, link->icq_Status);
        break;
      case UDP_SRV_STATUS_UPDATE:
        icq_HandleStatusChange(link, p);
        break;
      case UDP_SRV_GO_AWAY:
        icq_FmtLog(link, ICQ_LOG_ERROR, "Server has forced us to disconnect\n");
        if(link->icq_Disconnected)
          (*link->icq_Disconnected)(link);
        break;
      case UDP_SRV_END_OF_SEARCH:
        icq_FmtLog(link, ICQ_LOG_MESSAGE, "Search done\n");
        if(link->icq_SearchDone)
          (*link->icq_SearchDone)(link);
        icq_UDPAck(link, seq);
        break;
      case UDP_SRV_USER_FOUND:
        icq_HandleSearchReply(link, p);
        break;
      case UDP_SRV_ONLINE_MESSAGE: /* Online message through the server */
        cur_time = time(0L);
        tm_str = localtime(&cur_time);
        icq_PacketGotoUDPInData(p, 0);
        uin = icq_PacketRead32(p);
        type = icq_PacketRead16(p);
        len = icq_PacketRead16(p);
        icq_DoMsg(link, type, len, (char*)&p->data[p->cursor], uin, tm_str->tm_hour,
                  tm_str->tm_min, tm_str->tm_mday, tm_str->tm_mon+1, tm_str->tm_year+1900);
        icq_UDPAck(link, seq);
        break;
      case UDP_SRV_WRONG_PASSWORD:
        icq_FmtLog(link, ICQ_LOG_ERROR, "Wrong password\n");
        icq_UDPAck(link, seq);
        if(link->icq_WrongPassword)
          (*link->icq_WrongPassword)(link);
        break;
      case UDP_SRV_INVALID_UIN:
        icq_FmtLog(link, ICQ_LOG_WARNING, "Invalid UIN\n");
        icq_UDPAck(link, seq);
        if(link->icq_InvalidUIN)
          (*link->icq_InvalidUIN)(link);
        break;
      case UDP_SRV_META_USER:
        icq_HandleMetaUserInfo(link, p);
        break;
      default: /* commands we dont handle yet */
        icq_FmtLog(link, ICQ_LOG_WARNING, "Unhandled message %04x, Version: %x, "
                   "Sequence: %04x, Size: %d\n", cmd, icq_PacketReadUDPInVer(p),
                   seq, p->length);
        icq_UDPAck(link, seq); /* fake like we know what we're doing */
        break;
    }
  }
  else
  {
    icq_FmtLog(link, ICQ_LOG_WARNING, "Unhandled protocol version! Message %04x, Version: %x, "
               "Sequence: %04x, Size: %d\n", cmd, icq_PacketReadUDPInVer(p),
               seq, p->length);
/*    icq_UDPAck(link, seq);  DO NOT ACK unhandled protocol version! */
  }
}

/******************************************
Handles packets that the server sends to us.
*******************************************/
void icq_HandleServerResponse(ICQLINK *link)
{
  WORD seq, cmd;
  int s;
  icq_Packet *p;

  p = icq_PacketNew();
  s = icq_UDPSockRead(link, p);
  p->length = s;
  if(s<=0)
  {
    icq_FmtLog(link, ICQ_LOG_FATAL, "Connection terminated\n");
    icq_Disconnect(link);
    if(link->icq_Disconnected)
      (*link->icq_Disconnected)(link);
  }
  seq = icq_PacketReadUDPInSeq1(p);
  cmd = icq_PacketReadUDPInCmd(p);
  if(icq_GetServMess(link, seq) && cmd != UDP_SRV_NEW_UIN && cmd != UDP_SRV_GO_AWAY && cmd != UDP_SRV_ACK)
  {
    icq_FmtLog(link, ICQ_LOG_WARNING, "Ignored a message cmd %04x, seq %04x\n", cmd, seq);
    icq_UDPAck(link, seq); /* LAGGGGG!! */
    icq_PacketDelete(p);
    return;
  }
  if(cmd != UDP_SRV_ACK)
    icq_SetServMess(link, seq);

  icq_ServerResponse(link, p);

  icq_PacketDelete(p);
}

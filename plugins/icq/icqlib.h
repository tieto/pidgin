/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _ICQLIB_H_
#define _ICQLIB_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* for strdup(), bzero() and snprintf() declarations */
#ifndef __USE_BSD
#define __USE_BSD 1
#define __need_bsd_undef 1
#endif
#include <string.h>
#include <stdio.h>
#ifdef __need_bsd_undef
#undef __USE_BSD
#endif

#include <sys/types.h>

#include "util.h"
#include "contacts.h"

BOOL icq_GetServMess(ICQLINK *link, WORD num);
void icq_SetServMess(ICQLINK *link, WORD num);
void icq_RusConv(const char to[4], char *t_in);
void icq_RusConv_n(const char to[4], char *t_in, int len);

#ifndef _WIN32
#ifndef inet_addr
extern unsigned long inet_addr(const char *cp);
#endif /* inet_addr */
#ifndef inet_aton
extern int inet_aton(const char *cp, struct in_addr *inp);
#endif /* inet_aton */
#ifndef inet_ntoa
extern char *inet_ntoa(struct in_addr in);
#endif /* inet_ntoa */
#ifndef strdup
extern char *strdup(const char *s);
#endif /* strdup */
#endif /* _WIN32 */

/** Private ICQLINK data.  These are members that are internal to
 * icqlib, but must be contained in the per-connection ICQLINK
 * struct.
 */
typedef struct icq_link_private {

  void *icq_ContactList;

  /* 65536 seqs max, 1 bit per seq -> 65536/8 = 8192 */
  unsigned char icq_UDPServMess[8192]; 

  unsigned short icq_UDPSeqNum1, icq_UDPSeqNum2;
  unsigned long icq_UDPSession;
  
  void *icq_UDPQueue;

  int icq_TCPSequence;
  void *icq_TCPLinks;
  void *icq_ChatSessions;
  void *icq_FileSessions;

} ICQLINK_private;

#define invoke_callback(plink, callback) \
   if (plink->callback) (*(plink->callback))

#endif /* _ICQLIB_H_ */

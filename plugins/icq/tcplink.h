/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _TCP_LINK_H_
#define _TCP_LINK_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <time.h>

#include "icq.h"
#include "icqpacket.h"
#include "list.h"

/* link mode bitfield values */
#define TCP_LINK_MODE_RAW             1
#define TCP_LINK_MODE_HELLOWAIT       2
#define TCP_LINK_MODE_LISTEN          4
#define TCP_LINK_MODE_CONNECTING      8
#define TCP_LINK_SOCKS_CONNECTING     16
#define TCP_LINK_SOCKS_AUTHORIZATION  32
#define TCP_LINK_SOCKS_AUTHSTATUS     64
#define TCP_LINK_SOCKS_NOAUTHSTATUS   128
#define TCP_LINK_SOCKS_CROSSCONNECT   256
#define TCP_LINK_SOCKS_CONNSTATUS     512

/* link types */
#define TCP_LINK_MESSAGE              1
#define TCP_LINK_CHAT                 2
#define TCP_LINK_FILE                 3

#define icq_TCPLinkBufferSize 4096
#define TCP_LINK_CONNECT_TIMEOUT 30

struct icq_TCPLink_s {

   /* icq_TCPLink ICQLINK, type, mode, and session */
   ICQLINK *icqlink;
   int type;
   int mode;
   int proxy_status;
   void *session;
	 
   /* socket parameters */
   int socket;
   struct sockaddr_in socket_address;
   struct sockaddr_in remote_address;

   /* data buffer for receive calls */
   char buffer[icq_TCPLinkBufferSize];
   int buffer_count;

   /* packet queues */
   list *received_queue;
   list *send_queue;

   /* icq specific data, initialized by hello packet */
   unsigned long id;
   unsigned long remote_version;
   unsigned long remote_uin;
   char flags;

   /* connect timer */
   time_t connect_time;

};

icq_TCPLink *icq_TCPLinkNew(ICQLINK *link);
void icq_TCPLinkDelete(void *p);
void icq_TCPLinkClose(icq_TCPLink *p);
void icq_TCPLinkNodeDelete(list_node *p);

int icq_TCPLinkConnect(icq_TCPLink *plink, DWORD uin, int port);
icq_TCPLink *icq_TCPLinkAccept(icq_TCPLink *plink);
int icq_TCPLinkListen(icq_TCPLink *plink);

void icq_TCPLinkOnDataReceived(icq_TCPLink *plink);
void icq_TCPLinkOnPacketReceived(icq_TCPLink *plink, icq_Packet *p);
void icq_TCPLinkOnConnect(icq_TCPLink *plink);

unsigned long icq_TCPLinkSendSeq(icq_TCPLink *plink, icq_Packet *p,
  unsigned long sequence);
void icq_TCPLinkSend(icq_TCPLink *plink, icq_Packet *p);

void icq_TCPLinkProcessReceived(icq_TCPLink *plink);

icq_TCPLink *icq_FindTCPLink(ICQLINK *link, unsigned long uin, int type);

void icq_ChatRusConv_n(const char to[4], char *t_in, int t_len);

#endif /* _TCP_LINK_H_ */

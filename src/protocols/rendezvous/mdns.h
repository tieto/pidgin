/**
 * @file mdns.h Multicast DNS connection code used by rendezvous.
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * TODO: Need to document a lot of these.
 */

#ifndef _MDNS_H_
#define _MDNS_H_

#include "internal.h"
#include "debug.h"

/*
 * Some #define's stolen from libfaim.  Used to put
 * binary data (bytes, shorts and ints) into an array.
 */
#define util_put8(buf, data) ((*(buf) = (unsigned char)(data)&0xff),1)
#define util_put16(buf, data) ( \
		(*(buf) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+1) = (unsigned char)(data)&0xff),  \
		2)
#define util_put32(buf, data) ( \
		(*((buf)) = (unsigned char)((data)>>24)&0xff), \
		(*((buf)+1) = (unsigned char)((data)>>16)&0xff), \
		(*((buf)+2) = (unsigned char)((data)>>8)&0xff), \
		(*((buf)+3) = (unsigned char)(data)&0xff), \
		4)
#define util_get8(buf) ((*(buf))&0xff)
#define util_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define util_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))

/*
 * Merriam-Webster's
 */
#define RENDEZVOUS_RRTYPE_A		1
#define RENDEZVOUS_RRTYPE_NS	2
#define RENDEZVOUS_RRTYPE_CNAME	5
#define RENDEZVOUS_RRTYPE_NULL	10
#define RENDEZVOUS_RRTYPE_PTR	12
#define RENDEZVOUS_RRTYPE_TXT	16
#define RENDEZVOUS_RRTYPE_AAAA	28
#define RENDEZVOUS_RRTYPE_SRV	33
#define RENDEZVOUS_RRTYPE_ALL	255

/*
 * Express for Men's
 */
typedef struct _Header {
	unsigned short id;
	unsigned short flags;
	unsigned short numquestions;
	unsigned short numanswers;
	unsigned short numauthority;
	unsigned short numadditional;
} Header;

typedef struct _Question {
	gchar *name;
	unsigned short type;
	unsigned short class;
} Question;

typedef struct _ResourceRecord {
	gchar *name;
	unsigned short type;
	unsigned short class;
	int ttl;
	unsigned short rdlength;
	void *rdata;
} ResourceRecord;

typedef unsigned char ResourceRecordRDataA;

typedef struct _ResourceRecordRDataTXTNode {
	char *name;
	char *value;
} ResourceRecordRDataTXTNode;

typedef GSList ResourceRecordRDataTXT;

typedef unsigned char ResourceRecordRDataAAAA;

typedef struct _ResourceRecordRDataSRV {
	unsigned int priority;
	unsigned int weight;
	unsigned int port;
	gchar *target;
} ResourceRecordRDataSRV;

typedef struct _DNSPacket {
	Header header;
	GSList *questions;
	GSList *answers;
	GSList *authority;
	GSList *additional;
} DNSPacket;

/*
 * Bring in 'Da Noise, Bring in 'Da Functions
 */

/**
 * Create a multicast socket that can be used for sending and 
 * receiving multicast DNS packets.  The socket joins the
 * link-local multicast group (224.0.0.251).
 *
 * @return The file descriptor of the new socket, or -1 if
 *         there was an error establishing the socket.
 */
int mdns_socket_establish();

/**
 * Close a multicast socket.  This also clears the MDNS
 * cache.
 *
 * @param The file descriptor of the multicast socket.
 */
void mdns_socket_close(int fd);

/**
 * Sends a multicast DNS datagram.  Generally this is called
 * by other convenience functions such as mdns_query(), however
 * a client CAN construct its own DNSPacket if it wishes.
 *
 * @param fd The file descriptor of a pre-established socket to
 *        be used for sending the outgoing mDNS datagram.
 * @param dns The DNS datagram you wish to send.
 * @return 0 on success, otherwise return the error number.
 */
int mdns_send_dns(int fd, const DNSPacket *dns);

/**
 * Send a multicast DNS query for the given domain across the given
 * socket.
 *
 * @param fd The file descriptor of a pre-established socket to
 *        be used for sending the outgoing mDNS datagram.
 * @param domain This is the domain name you wish to query.  It should 
 *        be of the format "_presence._tcp.local" for example.
 * @return 0 if successful.
 */
int mdns_query(int fd, const char *domain, unsigned short type);

int mdns_send_rr(int fd, ResourceRecord *rr);
int mdns_advertise_a(int fd, const char *name, const unsigned char *ip);
int mdns_advertise_null(int fd, const char *name, const char *data, unsigned short rdlength);
int mdns_advertise_ptr(int fd, const char *name, const char *domain);
int mdns_advertise_ptr_with_ttl(int fd, const char *name, const char *domain, int ttl);
int mdns_advertise_txt(int fd, const char *name, const GSList *txt);
int mdns_advertise_aaaa(int fd, const char *name, const unsigned char *ip);
int mdns_advertise_srv(int fd, const char *name, unsigned short port, const char *target);

/**
 * Read a UDP packet from the given file descriptor and parse it
 * into a DNSPacket.
 *
 * @param fd A UDP listening socket to read from.
 * @return A newly allocated DNSPacket.  This should be freed with
 *         mdns_free() when no longer needed.
 */
DNSPacket *mdns_read(int fd);

/**
 * Free a DNSPacket structure.
 *
 * @param dns The DNSPacket that you want to free.
 */
void mdns_free(DNSPacket *dns);
void mdns_free_rr(ResourceRecord *rr);
void mdns_free_rrs(GSList *rrs);

ResourceRecord *mdns_copy_rr(const ResourceRecord *rr);

ResourceRecordRDataTXTNode *mdns_txt_find(const GSList *ret, const char *name);

GSList *mdns_txt_add(GSList *ret, const char *name, const char *value, gboolean replace);


#endif /* _MDNS_H_ */

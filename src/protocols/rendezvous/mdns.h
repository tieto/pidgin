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

#ifndef _MDNS_H_
#define _MDNS_H_

#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

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

typedef struct ResourceRecord {
	gchar *name;
	unsigned short type;
	unsigned short class;
	int ttl;
	unsigned short rdlength;
	void *rdata;
} ResourceRecord;

typedef struct _DNSPacket {
	Header header;
	Question *questions;
	ResourceRecord *answers;
	ResourceRecord *authority;
	ResourceRecord *additional;
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
int mdns_establish_socket();

/**
 * Send a multicast DNS query for the given domain across the given
 * socket.
 *
 * @param fd The file descriptor of a pre-established socket to
 *        be used for sending the outgoing mDNS query.
 * @param domain This is the domain name you wish to query.  It should 
 *        be of the format "_presence._tcp.local" for example.
 * @return 0 if sucessful.
 */
int mdns_query(int fd, const char *domain);

/**
 *
 *
 */
DNSPacket *mdns_read(int fd);

/**
 * Free a DNSPacket structure.
 *
 * @param dns The DNSPacket that you want to free.
 */
void mdns_free(DNSPacket *dns);

#endif /* _MDNS_H_ */

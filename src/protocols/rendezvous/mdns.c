/**
 * @file mdns.c Multicast DNS connection code used by rendezvous.
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
 * If you want to understand this, read RFC1035 and 
 * draft-cheshire-dnsext-multicastdns.txt
 */

/*
 * XXX - THIS DOESN'T DO BOUNDS CHECKING!!!  DON'T USE IT ON AN UNTRUSTED
 * NETWORK UNTIL IT DOES!!!  THERE ARE POSSIBLE REMOTE ACCESS VIA BUFFER
 * OVERFLOW SECURITY HOLES!!!
 */

#include "internal.h"
#include "debug.h"

#include "mdns.h"
#include "util.h"

int
mdns_establish_socket()
{
	int fd = -1;
	struct sockaddr_in addr;
	struct ip_mreq mreq;
	unsigned char loop;
	unsigned char ttl;
	int reuseaddr;

	gaim_debug_info("mdns", "Establishing multicast socket\n");

	/* What's the difference between AF_INET and PF_INET? */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		gaim_debug_error("mdns", "Unable to create socket: %s\n", strerror(errno));
		return -1;
	}

	/* Make the socket non-blocking (although it shouldn't matter) */
	fcntl(fd, F_SETFL, O_NONBLOCK);

	/* Bind the socket to a local IP and port */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5353);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
		gaim_debug_error("mdns", "Unable to bind socket to interface.\n");
		close(fd);
		return -1;
	}

	/* Ensure loopback is enabled (it should be enabled by default, by let's be sure) */
	loop = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(unsigned char)) == -1) {
		gaim_debug_error("mdns", "Error calling setsockopt for IP_MULTICAST_LOOP\n");
	}

	/* Set TTL to 255--required by mDNS */
	ttl = 255;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(unsigned char)) == -1) {
		gaim_debug_error("mdns", "Error calling setsockopt for IP_MULTICAST_TTL\n");
		close(fd);
		return -1;
	}

	/* Join the .local multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(struct ip_mreq)) == -1) {
		gaim_debug_error("mdns", "Error calling setsockopt for IP_ADD_MEMBERSHIP\n");
		close(fd);
		return -1;
	}

	/* Make the local IP re-usable */
	reuseaddr = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
		gaim_debug_error("mdns", "Error calling setsockopt for SO_REUSEADDR: %s\n", strerror(errno));
	}

	return fd;
}

int
mdns_query(int fd, const char *domain)
{
	struct sockaddr_in addr;
	unsigned int querylen;
	unsigned char *query;
	char *b, *c;
	int i, n;

	if (strlen(domain) > 255) {
		return -EINVAL;
	}

	/*
	 * Build the outgoing query packet.  It is made of the header with a
	 * query made up of the given domain.  The header is 12 bytes.
	 */
	querylen = 12 + strlen(domain) + 2 + 4;
	if (!(query = (unsigned char *)g_malloc(querylen))) {
		return -ENOMEM;
	}

	/* The header section */
	util_put32(&query[0], 0); /* The first 32 bits of the header are all 0's in mDNS */
	util_put16(&query[4], 1); /* QDCOUNT */
	util_put16(&query[6], 0); /* ANCOUNT */
	util_put16(&query[8], 0); /* NSCOUNT */
	util_put16(&query[10], 0); /* ARCOUNT */

	/* The question section */
	i = 12; /* Destination in query */
	b = (char *)domain;
	while ((c = strchr(b, '.'))) {
		i += util_put8(&query[i], c - b); /* Length of domain-name segment */
		memcpy(&query[i], b, c - b); /* Domain-name segment */
		i += c - b; /* Increment the destination pointer */
		b = c + 1;
	}
	i += util_put8(&query[i], strlen(b)); /* Length of domain-name segment */
	strcpy(&query[i], b); /* Domain-name segment */
	i += strlen(b) + 1; /* Increment the destination pointer */
	i += util_put16(&query[i], 0x000c); /* QTYPE */
	i += util_put16(&query[i], 0x8001); /* QCLASS */

	/* Actually send the DNS query */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5353);
	addr.sin_addr.s_addr = inet_addr("224.0.0.251");
	n = sendto(fd, query, querylen, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	g_free(query);

	if (n == -1) {
		gaim_debug_error("mdns", "Error sending packet: %d\n", errno);
		return -1;
	} else if (n != querylen) {
		gaim_debug_error("mdns", "Only sent %d of %d bytes of query.\n", n, querylen);
		return -1;
	}

	return 0;
}

/*
 * XXX - Needs bounds checking!
 *
 * Read in a domain name from the given buffer starting at the given
 * offset.  This handles using domain name compression to jump around
 * the data buffer, if needed.
 *
 * @return A null-terminated string representation of the domain name.
 *         This should be g_free'd when no longer needed.
 */
static gchar *
mdns_read_name(const char *data, int datalen, int dataoffset)
{
	GString *ret = g_string_new("");
	unsigned char tmp;

	while ((tmp = util_get8(&data[dataoffset])) != 0) {
		dataoffset++;

		if ((tmp & 0xc0) == 0) { /* First two bits are 00 */
			if (*ret->str)
				g_string_append_c(ret, '.');
			g_string_append_len(ret, &data[dataoffset], tmp);
			dataoffset += tmp;

		} else if ((tmp & 0x40) == 0) { /* First two bits are 10 */
			/* Reserved for future use */

		} else if ((tmp & 0x80) == 1) { /* First two bits are 01 */
			/* Reserved for future use */

		} else { /* First two bits are 11 */
			/* Jump to another position in the data */
			dataoffset = util_get8(&data[dataoffset]);

		}
	}

	return g_string_free(ret, FALSE);
}

/*
 * XXX - Needs bounds checking!
 *
 * Determine how many bytes long a portion of the domain name is
 * at the given offset.  This does NOT jump around the data array
 * in the case of domain name compression.
 *
 * @return The length of the portion of the domain name.
 */
static int
mdns_read_name_len(const char *data, int datalen, int dataoffset)
{
	int startoffset = dataoffset;
	unsigned char tmp;

	while ((tmp = util_get8(&data[dataoffset++])) != 0) {

		if ((tmp & 0xc0) == 0) { /* First two bits are 00 */
			dataoffset += tmp;

		} else if ((tmp & 0x40) == 0) { /* First two bits are 10 */
			/* Reserved for future use */

		} else if ((tmp & 0x80) == 1) { /* First two bits are 01 */
			/* Reserved for future use */

		} else { /* First two bits are 11 */
			/* End of this portion of the domain name */
			dataoffset++;
			break;

		}
	}

	return dataoffset - startoffset;
}

/*
 * XXX - Needs bounds checking!
 *
 */
static Question *
mdns_read_questions(int numquestions, const char *data, int datalen, int *offset)
{
	Question *ret;
	int i;

	ret = (Question *)g_malloc0(numquestions * sizeof(Question));
	for (i = 0; i < numquestions; i++) {
		ret[i].name = mdns_read_name(data, 0, *offset);
		*offset += mdns_read_name_len(data, 0, *offset);
		ret[i].type = util_get16(&data[*offset]); /* QTYPE */
		*offset += 2;
		ret[i].class = util_get16(&data[*offset]); /* QCLASS */
		*offset += 2;
	}

	return ret;
}

/*
 * Read in a chunk of data, probably a buddy icon.
 *
 */
static unsigned char *
mdns_read_rr_rdata_null(const char *data, int datalen, int offset, unsigned short rdlength)
{
	unsigned char *ret = NULL;

	if (offset + rdlength > datalen)
		return NULL;

	ret = (unsigned char *)g_malloc(rdlength);
	memcpy(ret, &data[offset], rdlength);

	return ret;
}

/*
 * XXX - Needs bounds checking!
 *
 */
static char *
mdns_read_rr_rdata_ptr(const char *data, int datalen, int offset)
{
	char *ret = NULL;

	ret = mdns_read_name(data, datalen, offset);

	return ret;
}

/*
 *
 *
 */
static GHashTable *
mdns_read_rr_rdata_txt(const char *data, int datalen, int offset, unsigned short rdlength)
{
	GHashTable *ret = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	int endoffset = offset + rdlength;
	unsigned char tmp;
	char buf[256], *key, *value;

	while (offset < endoffset) {
		/* Read in the length of the next name/value pair */
		tmp = util_get8(&data[offset]);
		offset++;

		/* Ensure packet is valid */
		if (offset + tmp > endoffset)
			break;

		/* Read in the next name/value pair */
		strncpy(buf, &data[offset], tmp);
		offset += tmp;

		if (buf[0] == '=') {
			/* Name/value pairs beginning with = are silently ignored */
			continue;
		}

		/* The value is a substring of buf, starting just after the = */
		buf[tmp] = '\0';
		value = strchr(buf, '=');
		if (value != NULL) {
			value[0] = '\0';
			value++;
		}

		/* Make the key all lowercase */
		key = g_utf8_strdown(buf, -1);
		if (!g_hash_table_lookup(ret, key))
			g_hash_table_insert(ret, key, g_strdup(value));
		else
			g_free(key);
	}

	return ret;
}

/*
 * XXX - Needs bounds checking!
 *
 */
static ResourceRecord *
mdns_read_rr(int numrecords, const char *data, int datalen, int *offset)
{
	ResourceRecord *ret;
	int i;

	ret = (ResourceRecord *)g_malloc0(numrecords * sizeof(ResourceRecord));
	for (i = 0; i < numrecords; i++) {
		ret[i].name = mdns_read_name(data, 0, *offset); /* NAME */
		*offset += mdns_read_name_len(data, 0, *offset);
		ret[i].type = util_get16(&data[*offset]); /* TYPE */
		*offset += 2;
		ret[i].class = util_get16(&data[*offset]); /* CLASS */
		*offset += 2;
		ret[i].ttl = util_get32(&data[*offset]); /* TTL */
		*offset += 4;
		ret[i].rdlength = util_get16(&data[*offset]); /* RDLENGTH */
		*offset += 2;

		/* RDATA */
		switch (ret[i].type) {
			case RENDEZVOUS_RRTYPE_NULL:
				ret[i].rdata = mdns_read_rr_rdata_null(data, datalen, *offset, ret[i].rdlength);
			break;

			case RENDEZVOUS_RRTYPE_PTR:
				ret[i].rdata = mdns_read_rr_rdata_ptr(data, datalen, *offset);
			break;

			case RENDEZVOUS_RRTYPE_TXT:
				ret[i].rdata = mdns_read_rr_rdata_txt(data, datalen, *offset, ret[i].rdlength);
			break;

			default:
				ret[i].rdata = NULL;
			break;
		}
		*offset += ret[i].rdlength;
	}

	return ret;
}

/*
 * XXX - Needs bounds checking!
 *
 */
DNSPacket *
mdns_read(int fd)
{
	DNSPacket *ret = NULL;
	int i; /* Current position in datagram */
	//char data[512];
	char data[10096];
	int datalen;
	struct sockaddr_in addr;
	socklen_t addrlen;

	/* Read in an mDNS packet */
	addrlen = sizeof(struct sockaddr_in);
	if ((datalen = recvfrom(fd, data, sizeof(data), 0, (struct sockaddr *)&addr, &addrlen)) == -1) {
		gaim_debug_error("mdns", "Error reading packet: %d\n", errno);
		return NULL;
	}

	ret = (DNSPacket *)g_malloc0(sizeof(DNSPacket));

	/* Parse the incoming packet, starting from 0 */
	i = 0;

	/* The header section */
	ret->header.id = util_get16(&data[i]); /* ID */
	i += 2;

	/* For the flags, some bits must be 0 and some must be 1, the rest are ignored */
	ret->header.flags = util_get16(&data[i]); /* Flags (QR, OPCODE, AA, TC, RD, RA, Z, AD, CD, and RCODE */
	i += 2;
	if ((ret->header.flags & 0x8000) == 0) {
		/* QR should be 1 */
		g_free(ret);
		return NULL;
	}
	if ((ret->header.flags & 0x7800) != 0) {
		/* OPCODE should be all 0's */
		g_free(ret);
		return NULL;
	}

	/* Read in the number of other things in the packet */
	ret->header.numquestions = util_get16(&data[i]);
	i += 2;
	ret->header.numanswers = util_get16(&data[i]);
	i += 2;
	ret->header.numauthority = util_get16(&data[i]);
	i += 2;
	ret->header.numadditional = util_get16(&data[i]);
	i += 2;

	/* Read in all the questions */
	ret->questions = mdns_read_questions(ret->header.numquestions, data, datalen, &i);

	/* Read in all resource records */
	ret->answers = mdns_read_rr(ret->header.numanswers, data, datalen, &i);

	/* Read in all authority records */
	ret->authority = mdns_read_rr(ret->header.numauthority, data, datalen, &i);

	/* Read in all additional records */
	ret->additional = mdns_read_rr(ret->header.numadditional, data, datalen, &i);

	/* We should be at the end of the packet */
	if (i != datalen) {
		gaim_debug_error("mdns", "Finished parsing before end of DNS packet!  Only parsed %d of %d bytes.", i, datalen);
		g_free(ret);
		return NULL;
	}

	return ret;
}

/**
 * Free the rdata associated with a given resource record.
 */
static void
mdns_free_rr_rdata(unsigned short type, void *rdata)
{
	switch (type) {
			case RENDEZVOUS_RRTYPE_NULL:
			case RENDEZVOUS_RRTYPE_PTR:
				g_free(rdata);
			break;

			case RENDEZVOUS_RRTYPE_TXT:
				g_hash_table_destroy(rdata);
			break;
	}
}

/**
 * Free a given question
 */
static void
mdns_free_q(Question *q)
{
	g_free(q->name);
}

/**
 * Free a given resource record.
 */
static void
mdns_free_rr(ResourceRecord *rr)
{
	g_free(rr->name);
	mdns_free_rr_rdata(rr->type, rr->rdata);
}

void
mdns_free(DNSPacket *dns)
{
	int i;

	for (i = 0; i < dns->header.numquestions; i++)
		mdns_free_q(&dns->questions[i]);
	for (i = 0; i < dns->header.numanswers; i++)
		mdns_free_rr(&dns->answers[i]);
	for (i = 0; i < dns->header.numauthority; i++)
		mdns_free_rr(&dns->authority[i]);
	for (i = 0; i < dns->header.numadditional; i++)
		mdns_free_rr(&dns->additional[i]);

	g_free(dns->questions);
	g_free(dns->answers);
	g_free(dns->authority);
	g_free(dns->additional);
	g_free(dns);
}

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
 * draft-cheshire-dnsext-multicastdns.txt, and buy
 * me a doughnut.  thx k bye.
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

/******************************************/
/* Functions for connection establishment */
/******************************************/

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

static int
mdns_send_raw(int fd, unsigned int datalen, unsigned char *data)
{
	struct sockaddr_in addr;
	int n;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(5353);
	addr.sin_addr.s_addr = inet_addr("224.0.0.251");
	n = sendto(fd, data, datalen, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (n == -1) {
		gaim_debug_error("mdns", "Error sending packet: %d\n", errno);
		return -1;
	} else if (n != datalen) {
		gaim_debug_error("mdns", "Only sent %d of %d bytes of data.\n", n, datalen);
		return -1;
	}

	return 0;
}

/***************************************/
/* Functions for sending mDNS messages */
/***************************************/

static int
mdns_getlength_RR_txt(void *rdata)
{
	GSList *cur;
	ResourceRecordTXTRDataNode *node;
	int rdlength = 0;

	for (cur = (GSList *)rdata; cur != NULL; cur = cur->next) {
		node = (ResourceRecordTXTRDataNode *)cur->data;
		rdlength += 1 + strlen(node->name);
		if (node->value != NULL)
			rdlength += 1 + strlen(node->value);
	}

	return rdlength;
}

static int
mdns_getlength_RR(const ResourceRecord *rr)
{
	int ret = 0;

	ret += strlen(rr->name) + 2;
	ret += 10;

	switch (rr->type) {
		case RENDEZVOUS_RRTYPE_PTR:
			ret += strlen((const char *)rr->rdata) + 2;
		break;

		case RENDEZVOUS_RRTYPE_TXT:
			ret += mdns_getlength_RR_txt(rr->rdata);
		break;
	}

	return ret;
}

static int
mdns_put_name(char *data, int datalen, int offset, const char *name)
{
	int i = 0;
	char *b, *c;

	b = (char *)name;
	while ((c = strchr(b, '.'))) {
		i += util_put8(&data[offset + i], c - b); /* Length of domain-name segment */
		memcpy(&data[offset + i], b, c - b); /* Domain-name segment */
		i += c - b; /* Increment the destination pointer */
		b = c + 1;
	}
	i += util_put8(&data[offset + i], strlen(b)); /* Length of domain-name segment */
	strcpy(&data[offset + i], b); /* Domain-name segment */
	i += strlen(b) + 1; /* Increment the destination pointer */

	return i;
}

static int
mdns_put_RR(char *data, int datalen, int offset, const ResourceRecord *rr)
{
	int i = 0;

	i += mdns_put_name(data, datalen, offset + i, rr->name);
	i += util_put16(&data[offset + i], rr->type);
	i += util_put16(&data[offset + i], rr->class);
	i += util_put32(&data[offset + i], rr->ttl);

	switch (rr->type) {
		case RENDEZVOUS_RRTYPE_PTR:
			i += util_put16(&data[offset + i], strlen((const char *)rr->rdata) + 2);
			i += mdns_put_name(data, datalen, offset + i, (const char *)rr->rdata);
		break;

		case RENDEZVOUS_RRTYPE_TXT: {
			GSList *cur;
			ResourceRecordTXTRDataNode *node;
			int rdlength = mdns_getlength_RR_txt(rr->rdata);
			int mylength;

			i += util_put16(&data[offset + i], rdlength);
			for (cur = (GSList *)rr->rdata; cur != NULL; cur = cur->next) {
				node = (ResourceRecordTXTRDataNode *)cur->data;
				mylength = 1 + strlen(node->name);
				if (node->value)
					mylength += 1 + strlen(node->value);
				i += util_put8(&data[offset + i], mylength - 1);
				memcpy(&data[offset + i], node->name, strlen(node->name));
				i += strlen(node->name);
				if (node->value) {
					data[offset + i] = '=';
					i++;
					memcpy(&data[offset + i], node->value, strlen(node->value));
					i += strlen(node->value);
				}
			}
		} break;
	}

	return i;
}

int
mdns_send_dns(int fd, const DNSPacket *dns)
{
	int ret;
	unsigned int datalen;
	unsigned char *data;
	int offset;
	int i;

	/* Calculate the length of the buffer we'll need to hold the DNS packet */
	datalen = 0;

	/* Header */
	datalen += 12;

	/* Questions */
	for (i = 0; i < dns->header.numquestions; i++)
		datalen += strlen(dns->questions[i].name) + 2 + 4;

	/* Resource records */
	for (i = 0; i < dns->header.numanswers; i++)
		datalen += mdns_getlength_RR(&dns->answers[i]);
	for (i = 0; i < dns->header.numauthority; i++)
		datalen += mdns_getlength_RR(&dns->authority[i]);
	for (i = 0; i < dns->header.numadditional; i++)
		datalen += mdns_getlength_RR(&dns->additional[i]);

	/* Allocate a buffer */
	if (!(data = (unsigned char *)g_malloc(datalen))) {
		return -ENOMEM;
	}

	/* Construct the datagram */
	/* Header */
	offset = 0;
	offset += util_put16(&data[offset], dns->header.id); /* ID */
	offset += util_put16(&data[offset], dns->header.flags);
	offset += util_put16(&data[offset], dns->header.numquestions); /* QDCOUNT */
	offset += util_put16(&data[offset], dns->header.numanswers); /* ANCOUNT */
	offset += util_put16(&data[offset], dns->header.numauthority); /* NSCOUNT */
	offset += util_put16(&data[offset], dns->header.numadditional); /* ARCOUNT */

	/* Questions */
	for (i = 0; i < dns->header.numquestions; i++) {
		offset += mdns_put_name(data, datalen, offset, dns->questions[i].name); /* QNAME */
		offset += util_put16(&data[offset], dns->questions[i].type); /* QTYPE */
		offset += util_put16(&data[offset], dns->questions[i].class); /* QCLASS */
	}

	/* Resource records */
	for (i = 0; i < dns->header.numanswers; i++)
		offset += mdns_put_RR(data, datalen, offset, &dns->answers[i]);
	for (i = 0; i < dns->header.numauthority; i++)
		offset += mdns_put_RR(data, datalen, offset, &dns->authority[i]);
	for (i = 0; i < dns->header.numadditional; i++)
		offset += mdns_put_RR(data, datalen, offset, &dns->additional[i]);

	/* Send the datagram */
	ret = mdns_send_raw(fd, datalen, data);

	g_free(data);

	return ret;
}

int
mdns_query(int fd, const char *domain)
{
	int ret;
	DNSPacket *dns;

	if (strlen(domain) > 255) {
		return -EINVAL;
	}

	dns = (DNSPacket *)g_malloc(sizeof(DNSPacket));
	dns->header.id = 0x0000;
	dns->header.flags = 0x0000;
	dns->header.numquestions = 0x0001;
	dns->header.numanswers = 0x0000;
	dns->header.numauthority = 0x0000;
	dns->header.numadditional = 0x0000;

	dns->questions = (Question *)g_malloc(1 * sizeof(Question));
	dns->questions[0].name = g_strdup(domain);
	dns->questions[0].type = RENDEZVOUS_RRTYPE_PTR;
	dns->questions[0].class = 0x8001;

	dns->answers = NULL;
	dns->authority = NULL;
	dns->additional = NULL;

	mdns_send_dns(fd, dns);

	mdns_free(dns);

	return ret;
}

int
mdns_advertise_ptr(int fd, const char *name, const char *domain)
{
	int ret;
	DNSPacket *dns;

	if ((strlen(name) > 255) || (strlen(domain) > 255)) {
		return -EINVAL;
	}

	dns = (DNSPacket *)g_malloc(sizeof(DNSPacket));
	dns->header.id = 0x0000;
	dns->header.flags = 0x8400;
	dns->header.numquestions = 0x0000;
	dns->header.numanswers = 0x0001;
	dns->header.numauthority = 0x0000;
	dns->header.numadditional = 0x0000;
	dns->questions = NULL;

	dns->answers = (ResourceRecord *)g_malloc(1 * sizeof(ResourceRecord));
	dns->answers[0].name = g_strdup(name);
	dns->answers[0].type = RENDEZVOUS_RRTYPE_PTR;
	dns->answers[0].class = 0x0001;
	dns->answers[0].ttl = 0x00001c20;
	dns->answers[0].rdlength = 0x0000; /* Set automatically */
	dns->answers[0].rdata = (void *)g_strdup(domain);

	dns->authority = NULL;
	dns->additional = NULL;

	mdns_send_dns(fd, dns);

	mdns_free(dns);

	return ret;
}

int
mdns_advertise_txt(int fd, const char *name, const GSList *rdata)
{
	int ret;
	DNSPacket *dns;

	if ((strlen(name) > 255)) {
		return -EINVAL;
	}

	dns = (DNSPacket *)g_malloc(sizeof(DNSPacket));
	dns->header.id = 0x0000;
	dns->header.flags = 0x8400;
	dns->header.numquestions = 0x0000;
	dns->header.numanswers = 0x0001;
	dns->header.numauthority = 0x0000;
	dns->header.numadditional = 0x0000;
	dns->questions = NULL;

	dns->answers = (ResourceRecord *)g_malloc(1 * sizeof(ResourceRecord));
	dns->answers[0].name = g_strdup(name);
	dns->answers[0].type = RENDEZVOUS_RRTYPE_TXT;
	dns->answers[0].class = 0x0001;
	dns->answers[0].ttl = 0x00001c20;
	dns->answers[0].rdlength = 0x0000; /* Set automatically */
	dns->answers[0].rdata = (void *)rdata;

	dns->authority = NULL;
	dns->additional = NULL;

	mdns_send_dns(fd, dns);

	/* The hash table should be freed by the caller of this function */
	dns->answers[0].rdata = NULL;

	mdns_free(dns);

	return ret;
}

/***************************************/
/* Functions for parsing mDNS messages */
/***************************************/

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
 *
 *
 */
static ResourceRecordSRV *
mdns_read_rr_rdata_srv(const char *data, int datalen, int offset, unsigned short rdlength)
{
	ResourceRecordSRV *ret = NULL;
	int endoffset = offset + rdlength;

	if (offset + 7 > endoffset)
		return NULL;

	ret = g_malloc(sizeof(ResourceRecordSRV));

	/* Read in the priority */
	ret->priority = util_get16(&data[offset]);
	offset += 2;

	/* Read in the weight */
	ret->weight = util_get16(&data[offset]);
	offset += 2;

	/* Read in the port */
	ret->port = util_get16(&data[offset]);
	offset += 2;

	/* Read in the target name */
	/*
	 * XXX - RFC2782 says it's not supposed to be an alias...
	 * but it was in the packet capture I looked at from iChat.
	 */
	ret->target = mdns_read_name(data, datalen, offset);

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

			case RENDEZVOUS_RRTYPE_SRV:
				ret[i].rdata = mdns_read_rr_rdata_srv(data, datalen, *offset, ret[i].rdlength);
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
	/* XXX - Find out what to use as a maximum incoming UDP packet size */
	/* char data[512]; */
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

			case RENDEZVOUS_RRTYPE_SRV:
				g_free(((ResourceRecordSRV *)rdata)->target);
				g_free(rdata);
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

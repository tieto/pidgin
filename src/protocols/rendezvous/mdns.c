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
 * XXX - This entire file could use another pair of eyes to audit for 
 * any possible buffer overflow exploits.  It doesn't even HAVE to be
 * a pair, neither--one eye would suffice.  Oh, snap, somebody call
 * One Eyed Willie.
 */

/*
 * XXX - Store data for NULL ResourceRecords so that rr->rdata contains
 * both the length and the data. This length will always be equal to
 * rr->rdlength... but it fits in more with the rest of the code.
 * rr->rdata should not need a separate length value to determine
 * how many bytes it will take.
 */

#include "internal.h"
#include "debug.h"

#include "mdns.h"
#include "mdns_cache.h"
#include "util.h"

/******************************************/
/* Functions for freeing a DNS structure  */
/******************************************/

/**
 * Free a given question
 *
 * @param q The question to free.
 */
static void
mdns_free_q(Question *q)
{
	g_free(q->name);
	g_free(q);
}

/**
 * Free a given list of questions.
 *
 * @param qs The list of questions to free.
 */
static void
mdns_free_qs(GSList *qs)
{
	Question *q;

	while (qs != NULL) {
		q = qs->data;
		mdns_free_q(q);
		qs = g_slist_remove(qs, q);
	}
}

/**
 * Free the rdata associated with a given resource record.
 *
 * @param type The type of the resource record you are freeing.
 * @param rdata The rdata of the resource record you are freeing.
 */
static void
mdns_free_rr_rdata(unsigned short type, void *rdata)
{
	if (rdata == NULL)
		return;

	switch (type) {
			case RENDEZVOUS_RRTYPE_A:
			case RENDEZVOUS_RRTYPE_NULL:
			case RENDEZVOUS_RRTYPE_PTR:
			case RENDEZVOUS_RRTYPE_AAAA:
				g_free(rdata);
			break;

			case RENDEZVOUS_RRTYPE_TXT: {
				GSList *cur = rdata;
				ResourceRecordRDataTXTNode *node;
				while (cur != NULL) {
					node = cur->data;
					cur = g_slist_remove(cur, node);
					g_free(node->name);
					g_free(node->value);
					g_free(node);
				}
			} break;

			case RENDEZVOUS_RRTYPE_SRV:
				g_free(((ResourceRecordRDataSRV *)rdata)->target);
				g_free(rdata);
			break;
	}
}

/**
 * Free a given resource record.
 *
 * @param rr The resource record to free.
 */
void
mdns_free_rr(ResourceRecord *rr)
{
	g_free(rr->name);
	mdns_free_rr_rdata(rr->type, rr->rdata);
	g_free(rr);
}

/**
 * Free a given list of resource records.
 *
 * @param rrs The list of resource records to free.
 */
void
mdns_free_rrs(GSList *rrs)
{
	ResourceRecord *rr;

	while (rrs != NULL) {
		rr = rrs->data;
		mdns_free_rr(rr);
		rrs = g_slist_remove(rrs, rr);
	}
}

/**
 * Free a given DNS packet.  This frees all the questions and all
 * the resource records.
 *
 * @param dns The DNS packet to free.
 */
void
mdns_free(DNSPacket *dns)
{
	mdns_free_qs(dns->questions);
	mdns_free_rrs(dns->answers);
	mdns_free_rrs(dns->authority);
	mdns_free_rrs(dns->additional);

	g_free(dns);
}

/**********************************************/
/* Functions for duplicating a DNS structure  */
/**********************************************/

#if 0
static Question *
mdns_copy_q(const Question *q)
{
	Question *ret;

	if (q == NULL)
		return NULL;

	ret = g_malloc(sizeof(Question));
	ret->name = g_strdup(q->name);
	ret->type = q->type;
	ret->class = q->class;

	return ret;
}

static GSList *
mdns_copy_qs(const GSList *qs)
{
	GSList *ret = NULL;
	GSList *cur;
	Question *new;

	for (cur = (GSList *)qs; cur != NULL; cur = cur->next) {
		new = mdns_copy_q(cur->data);
		ret = g_slist_append(ret, new);
	}

	return ret;
}
#endif

static void *
mdns_copy_rr_rdata_txt(const ResourceRecordRDataTXT *rdata)
{
	GSList *ret = NULL;
	GSList *cur;
	ResourceRecordRDataTXTNode *node, *copy;

	for (cur = (GSList *)rdata; cur != NULL; cur = cur->next) {
		node = cur->data;
		copy = g_malloc0(sizeof(ResourceRecordRDataTXTNode));
		copy->name = g_strdup(node->name);
		if (node->value != NULL)
			copy->value = g_strdup(node->value);
		ret = g_slist_append(ret, copy);
	}

	return ret;
}

static void *
mdns_copy_rr_rdata_srv(const ResourceRecordRDataSRV *rdata)
{
	ResourceRecordRDataSRV *ret;

	ret = g_malloc(sizeof(ResourceRecordRDataSRV));
	ret->priority = rdata->priority;
	ret->weight = rdata->weight;
	ret->port = rdata->port;
	ret->target = g_strdup(rdata->target);

	return ret;
}

void *
mdns_copy_rr_rdata(unsigned short type, const void *rdata, unsigned int rdlength)
{
	void *ret;

	if (rdata == NULL)
		return NULL;

	switch (type) {
		case RENDEZVOUS_RRTYPE_A:
			ret = g_memdup(rdata, rdlength);
		break;

		case RENDEZVOUS_RRTYPE_NULL:
			ret = g_memdup(rdata, rdlength);
		break;

		case RENDEZVOUS_RRTYPE_PTR:
			ret = g_strdup(rdata);
		break;

		case RENDEZVOUS_RRTYPE_TXT:
			ret = mdns_copy_rr_rdata_txt(rdata);
		break;

		case RENDEZVOUS_RRTYPE_AAAA:
			ret = g_memdup(rdata, rdlength);
		break;

		case RENDEZVOUS_RRTYPE_SRV:
			ret = mdns_copy_rr_rdata_srv(rdata);
		break;
	}

	return ret;
}

ResourceRecord *
mdns_copy_rr(const ResourceRecord *rr)
{
	ResourceRecord *ret;

	if (rr == NULL)
		return NULL;

	ret = g_malloc(sizeof(ResourceRecord));
	ret->name = g_strdup(rr->name);
	ret->type = rr->type;
	ret->class = rr->class;
	ret->ttl = rr->ttl;
	ret->rdlength = rr->rdlength;
	ret->rdata = mdns_copy_rr_rdata(rr->type, rr->rdata, rr->rdlength);

	return ret;
}

#if 0
static GSList *
mdns_copy_rrs(const GSList *rrs)
{
	GSList *ret = NULL;
	GSList *cur;
	ResourceRecord *new;

	for (cur = (GSList *)rrs; cur != NULL; cur = cur->next) {
		new = mdns_copy_rr(cur->data);
		ret = g_slist_append(ret, new);
	}

	return ret;
}

static DNSPacket *
mdns_copy(const DNSPacket *dns)
{
	DNSPacket *ret;

	if (dns == NULL)
		return NULL;

	ret = g_malloc0(sizeof(DNSPacket));
	ret->header.id = dns->header.id;
	ret->header.flags = dns->header.flags;
	ret->header.numquestions = dns->header.numquestions;
	ret->header.numanswers = dns->header.numanswers;
	ret->header.numauthority = dns->header.numauthority;
	ret->header.numadditional = dns->header.numadditional;
	ret->questions = mdns_copy_qs(dns->questions);
	ret->answers = mdns_copy_rrs(dns->answers);
	ret->authority = mdns_copy_rrs(dns->authority);
	ret->additional = mdns_copy_rrs(dns->additional);

	return ret;
}
#endif

/******************************************/
/* Functions for connection establishment */
/******************************************/

int
mdns_socket_establish()
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
		gaim_debug_error("mdns", "%s\n", strerror(errno));
		close(fd);
		return -1;
	}

	/* Ensure loopback is enabled (it should be enabled by default, but let's be sure) */
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

void
mdns_socket_close(int fd)
{
	if (fd >= 0)
		close(fd);

	mdns_cache_remove_all();
}

/**
 * Multicast raw data over a file descriptor.
 *
 * @param fd A file descriptor that is a socket bound to a UDP port.
 * @param datalen The length of the data you wish to send.
 * @param data The data you wish to send.
 * @return 0 on success, otherwise return -1.
 */
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

/**
 *
 */
static int
mdns_getlength_name(const void *name)
{
	return strlen((const char *)name) + 2;
}

/**
 *
 */
static int
mdns_getlength_q(const Question *q)
{
	return mdns_getlength_name(q->name) + 4;
}

/**
 *
 */
static int
mdns_getlength_qs(const GSList *qs)
{
	int length = 0;
	GSList *cur;

	for (cur = (GSList *)qs; cur != NULL; cur = cur->next)
		length += mdns_getlength_q(cur->data);

	return length;
}

/**
 *
 */
static int
mdns_getlength_rr_rdata(unsigned short type, const void *rdata)
{
	int rdlength = 0;

	switch (type) {
		case RENDEZVOUS_RRTYPE_A:
			rdlength = 4;
		break;

		case RENDEZVOUS_RRTYPE_PTR:
			rdlength = mdns_getlength_name(rdata);
		break;

		case RENDEZVOUS_RRTYPE_TXT: {
			GSList *cur;
			ResourceRecordRDataTXTNode *node;

			for (cur = (GSList *)rdata; cur != NULL; cur = cur->next) {
				node = (ResourceRecordRDataTXTNode *)cur->data;
				rdlength += 1 + strlen(node->name);
				if (node->value != NULL)
					rdlength += 1 + strlen(node->value);
			}
		} break;

		case RENDEZVOUS_RRTYPE_AAAA:
			rdlength = 16;
		break;

		case RENDEZVOUS_RRTYPE_SRV:
			rdlength = 6 + mdns_getlength_name(((const ResourceRecordRDataSRV *)rdata)->target);
		break;
	}

	return rdlength;
}

/**
 *
 */
static int
mdns_getlength_rr(const ResourceRecord *rr)
{
	int rdlength = mdns_getlength_rr_rdata(rr->type, rr->rdata);

	if ((rdlength == 0) && (rr->rdata != NULL))
		rdlength = rr->rdlength;

	return mdns_getlength_name(rr->name) + 10 + rdlength;
}

/**
 *
 */
static int
mdns_getlength_rrs(const GSList *rrs)
{
	int length = 0;
	GSList *cur;

	for (cur = (GSList *)rrs; cur != NULL; cur = cur->next)
		length += mdns_getlength_rr(cur->data);

	return length;
}

/**
 *
 */
static int
mdns_getlength_dns(const DNSPacket *dns)
{
	int length = 0;

	/* Header */
	length += 12;

	/* Questions */
	length += mdns_getlength_qs(dns->questions);

	/* Resource records */
	length += mdns_getlength_rrs(dns->answers);
	length += mdns_getlength_rrs(dns->authority);
	length += mdns_getlength_rrs(dns->additional);

	return length;
}

/**
 *
 */
static int
mdns_put_name(char *data, unsigned int datalen, unsigned int offset, const char *name)
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

/**
 *
 */
static int
mdns_put_q(char *data, unsigned int datalen, unsigned int offset, const Question *q)
{
	int i = 0;

	i += mdns_put_name(data, datalen, offset + i, q->name); /* QNAME */
	i += util_put16(&data[offset + i], q->type); /* QTYPE */
	i += util_put16(&data[offset + i], q->class); /* QCLASS */

	return i;
}

/**
 *
 */
static int
mdns_put_rr(char *data, unsigned int datalen, unsigned int offset, const ResourceRecord *rr)
{
	int i = 0;

	i += mdns_put_name(data, datalen, offset + i, rr->name);
	i += util_put16(&data[offset + i], rr->type);
	i += util_put16(&data[offset + i], rr->class);
	i += util_put32(&data[offset + i], rr->ttl);
	i += util_put16(&data[offset + i], rr->rdlength);

	switch (rr->type) {
		case RENDEZVOUS_RRTYPE_A:
			memcpy(&data[offset + i], rr->rdata, rr->rdlength);
			i += rr->rdlength;
		break;

		case RENDEZVOUS_RRTYPE_NULL:
			memcpy(&data[offset + i], rr->rdata, rr->rdlength);
			i += rr->rdlength;
		break;

		case RENDEZVOUS_RRTYPE_PTR:
			i += mdns_put_name(data, datalen, offset + i, (const char *)rr->rdata);
		break;

		case RENDEZVOUS_RRTYPE_TXT: {
			GSList *cur;
			ResourceRecordRDataTXTNode *node;
			int mylength;

			for (cur = (GSList *)rr->rdata; cur != NULL; cur = cur->next) {
				node = (ResourceRecordRDataTXTNode *)cur->data;
				mylength = 1 + strlen(node->name);
				if (node->value != NULL)
					mylength += 1 + strlen(node->value);
				i += util_put8(&data[offset + i], mylength - 1);
				memcpy(&data[offset + i], node->name, strlen(node->name));
				i += strlen(node->name);
				if (node->value != NULL) {
					data[offset + i] = '=';
					i++;
					memcpy(&data[offset + i], node->value, strlen(node->value));
					i += strlen(node->value);
				}
			}
		} break;

		case RENDEZVOUS_RRTYPE_AAAA:
			memcpy(&data[offset + i], rr->rdata, rr->rdlength);
			i += rr->rdlength;
		break;

		case RENDEZVOUS_RRTYPE_SRV: {
			ResourceRecordRDataSRV *srv = rr->rdata;
			i += util_put16(&data[offset + i], 0);
			i += util_put16(&data[offset + i], 0);
			i += util_put16(&data[offset + i], srv->port);
			i += mdns_put_name(data, datalen, offset + i, srv->target);
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
	unsigned int offset;
	GSList *cur;

	/* Calculate the length of the buffer we'll need to hold the DNS packet */
	datalen = mdns_getlength_dns(dns);

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
	for (cur = dns->questions; cur != NULL; cur = cur->next)
		offset += mdns_put_q(data, datalen, offset, cur->data);

	/* Resource records */
	for (cur = dns->answers; cur != NULL; cur = cur->next)
		offset += mdns_put_rr(data, datalen, offset, cur->data);
	for (cur = dns->authority; cur != NULL; cur = cur->next)
		offset += mdns_put_rr(data, datalen, offset, cur->data);
	for (cur = dns->additional; cur != NULL; cur = cur->next)
		offset += mdns_put_rr(data, datalen, offset, cur->data);

	/* Send the datagram */
	/* Offset can be shorter than datalen because of name compression */
	ret = mdns_send_raw(fd, offset, data);

	g_free(data);

	return ret;
}

int
mdns_query(int fd, const char *domain, unsigned short type)
{
	int ret;
	DNSPacket *dns;
	Question *q;

	if ((domain == NULL) || strlen(domain) > 255) {
		return -EINVAL;
	}

	dns = (DNSPacket *)g_malloc(sizeof(DNSPacket));
	dns->header.id = 0x0000;
	dns->header.flags = 0x0000;
	dns->header.numquestions = 0x0001;
	dns->header.numanswers = 0x0000;
	dns->header.numauthority = 0x0000;
	dns->header.numadditional = 0x0000;

	q = (Question *)g_malloc(sizeof(Question));
	q->name = g_strdup(domain);
	q->type = type;
	q->class = 0x0001;
	dns->questions = g_slist_append(NULL, q);

	dns->answers = NULL;
	dns->authority = NULL;
	dns->additional = NULL;

	ret = mdns_send_dns(fd, dns);

	mdns_free(dns);

	return ret;
}

int
mdns_send_rr(int fd, ResourceRecord *rr)
{
	int ret;
	DNSPacket *dns;

	g_return_val_if_fail(rr != NULL, -1);

	dns = (DNSPacket *)g_malloc(sizeof(DNSPacket));
	dns->header.id = 0x0000;
	dns->header.flags = 0x8400;
	dns->header.numquestions = 0x0000;
	dns->header.numanswers = 0x0001;
	dns->header.numauthority = 0x0000;
	dns->header.numadditional = 0x0000;
	dns->questions = NULL;
	dns->answers = g_slist_append(NULL, mdns_copy_rr(rr));
	dns->authority = NULL;
	dns->additional = NULL;

	ret = mdns_send_dns(fd, dns);

	mdns_free(dns);

	return ret;
}

int
mdns_advertise_a(int fd, const char *name, const unsigned char *ip)
{
	int ret;
	ResourceRecord *rr;
	ResourceRecordRDataA *rdata;
	int i;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);
	g_return_val_if_fail(ip != NULL, -EINVAL);

	rdata = g_malloc(4);
	for (i = 0; i < 4; i++)
		util_put8(&rdata[i], ip[i]);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_A;
	rr->class = 0x0001;
	rr->ttl = 0x000000f0;
	rr->rdlength = 4;
	rr->rdata = mdns_copy_rr_rdata(rr->type, rdata, rr->rdlength);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

int
mdns_advertise_null(int fd, const char *name, const char *rdata, unsigned short rdlength)
{
	int ret;
	ResourceRecord *rr;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_NULL;
	rr->class = 0x0001;
	rr->ttl = 0x00001c20;
	rr->rdlength = rdlength;
	rr->rdata = mdns_copy_rr_rdata(rr->type, rdata, rr->rdlength);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

int
mdns_advertise_ptr(int fd, const char *name, const char *domain)
{
	return mdns_advertise_ptr_with_ttl(fd, name, domain, 0x00001c20);
}

int
mdns_advertise_ptr_with_ttl(int fd, const char *name, const char *domain, int ttl)
{
	int ret;
	ResourceRecord *rr;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);
	g_return_val_if_fail(domain != NULL, -EINVAL);
	g_return_val_if_fail(strlen(domain) <= 255, -EINVAL);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_PTR;
	rr->class = 0x8001;
	rr->ttl = ttl;
	rr->rdata = (void *)g_strdup(domain);
	rr->rdlength = mdns_getlength_rr_rdata(rr->type, rr->rdata);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

int
mdns_advertise_txt(int fd, const char *name, const GSList *rdata)
{
	int ret;
	ResourceRecord *rr;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_TXT;
	rr->class = 0x8001;
	rr->ttl = 0x00001c20;
	rr->rdlength = mdns_getlength_rr_rdata(rr->type, rdata);
	rr->rdata = mdns_copy_rr_rdata(rr->type, rdata, rr->rdlength);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

int
mdns_advertise_aaaa(int fd, const char *name, const unsigned char *ip)
{
	int ret;
	ResourceRecord *rr;
	ResourceRecordRDataA *rdata;
	int i;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);

	rdata = g_malloc(16);
	for (i = 0; i < 16; i++)
		util_put8(&rdata[i], ip[i]);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_A;
	rr->class = 0x0001;
	rr->ttl = 0x000000f0;
	rr->rdlength = 16;
	rr->rdata = mdns_copy_rr_rdata(rr->type, rdata, rr->rdlength);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

int
mdns_advertise_srv(int fd, const char *name, unsigned short port, const char *target)
{
	int ret;
	ResourceRecord *rr;
	ResourceRecordRDataSRV *rdata;

	g_return_val_if_fail(name != NULL, -EINVAL);
	g_return_val_if_fail(strlen(name) <= 255, -EINVAL);

	rdata = g_malloc(sizeof(ResourceRecordRDataSRV));
	rdata->port = port;
	rdata->target = g_strdup(target);

	rr = (ResourceRecord *)g_malloc(sizeof(ResourceRecord));
	rr->name = g_strdup(name);
	rr->type = RENDEZVOUS_RRTYPE_SRV;
	rr->class = 0x8001;
	rr->ttl = 0x00001c20;
	rr->rdata = rdata;
	rr->rdlength = mdns_getlength_rr_rdata(rr->type, rr->rdata);

	ret = mdns_send_rr(fd, rr);

	mdns_free_rr(rr);

	return ret;
}

/***************************************/
/* Functions for parsing mDNS messages */
/***************************************/

/**
 * Read in a domain name from the given buffer starting at the given
 * offset.  This handles using domain name compression to jump around
 * the data buffer, if needed.
 *
 * @return A null-terminated string representation of the domain name.
 *         This should be g_free'd when no longer needed.
 */
static gchar *
mdns_read_name(const char *data, int datalen, unsigned int offset)
{
	GString *ret = g_string_new("");
	unsigned char tmp, newoffset;

	while ((offset <= datalen) && ((tmp = util_get8(&data[offset])) != 0)) {
		offset++;

		if ((tmp & 0xc0) == 0) { /* First two bits are 00 */
			if (offset + tmp > datalen)
				/* Attempt to read past end of data! Bailing! */
				return g_string_free(ret, TRUE);
			if (*ret->str != '\0')
				g_string_append_c(ret, '.');
			g_string_append_len(ret, &data[offset], tmp);
			offset += tmp;

		} else if ((tmp & 0x40) == 0) { /* First two bits are 10 */
			/* Reserved for future use */

		} else if ((tmp & 0x80) == 1) { /* First two bits are 01 */
			/* Reserved for future use */

		} else { /* First two bits are 11 */
			/* Jump to another position in the data */
			newoffset = util_get8(&data[offset]);
			if (newoffset >= offset)
				/* Invalid pointer!  Could lead to infinite recursion! Bailing! */
				return g_string_free(ret, TRUE);
			offset = newoffset;
		}
	}

	if (offset > datalen)
		return g_string_free(ret, TRUE);

	return g_string_free(ret, FALSE);
}

/**
 * Determine how many bytes long a portion of the domain name is
 * at the given offset.  This does NOT jump around the data array
 * in the case of domain name compression.
 *
 * @return The length of the portion of the domain name.
 */
static int
mdns_read_name_len(const char *data, unsigned int datalen, unsigned int offset)
{
	int startoffset = offset;
	unsigned char tmp;

	while ((offset <= datalen) && ((tmp = util_get8(&data[offset])) != 0)) {
		offset++;

		if ((tmp & 0xc0) == 0) { /* First two bits are 00 */
			if (offset + tmp > datalen)
				/* Attempt to read past end of data! Bailing! */
				return 0;
			offset += tmp;

		} else if ((tmp & 0x40) == 0) { /* First two bits are 10 */
			/* Reserved for future use */

		} else if ((tmp & 0x80) == 1) { /* First two bits are 01 */
			/* Reserved for future use */

		} else { /* First two bits are 11 */
			/* End of this portion of the domain name */
			break;

		}
	}

	return offset - startoffset + 1;
}

/**
 *
 *
 */
static Question *
mdns_read_q(const char *data, unsigned int datalen, int *offset)
{
	Question *q;

	q = (Question *)g_malloc0(sizeof(Question));
	q->name = mdns_read_name(data, datalen, *offset);
	*offset += mdns_read_name_len(data, datalen, *offset);
	if (*offset + 4 > datalen) {
		mdns_free_q(q);
		return NULL;
	}

	q->type = util_get16(&data[*offset]); /* QTYPE */
	*offset += 2;
	q->class = util_get16(&data[*offset]); /* QCLASS */
	*offset += 2;
	if (*offset > datalen) {
		mdns_free_q(q);
		return NULL;
	}

	return q;
}

/**
 *
 *
 */
static GSList *
mdns_read_questions(int numquestions, const char *data, unsigned int datalen, int *offset)
{
	GSList *ret = NULL;
	Question *q;
	int i;

	for (i = 0; i < numquestions; i++) {
		q = mdns_read_q(data, datalen, offset);
		if (q == NULL)
			break;
		ret = g_slist_append(ret, q);
	}

	/* Malformed packet check */
	if (i < numquestions) {
		mdns_free_qs(ret);
		return NULL;
	}

	return ret;
}

/**
 * Read in a chunk of data, probably a buddy icon.
 *
 */
static unsigned char *
mdns_read_rr_rdata_null(const char *data, unsigned int datalen, unsigned int offset, unsigned short rdlength)
{
	unsigned char *ret = NULL;

	if (offset + rdlength > datalen)
		return NULL;

	ret = (unsigned char *)g_malloc(rdlength);
	memcpy(ret, &data[offset], rdlength);

	return ret;
}

/**
 * Read in a compressed name.
 *
 */
static char *
mdns_read_rr_rdata_ptr(const char *data, unsigned int datalen, unsigned int offset)
{
	return mdns_read_name(data, datalen, offset);
}

ResourceRecordRDataTXTNode *
mdns_txt_find(const GSList *ret, const char *name)
{
	ResourceRecordRDataTXTNode *node;
	GSList *cur;

	for (cur = (GSList *)ret; cur != NULL; cur = cur->next) {
		node = cur->data;
		if (!strcasecmp(node->name, name))
			return node;
	}

	return NULL;
}

/**
 *
 *
 */
GSList *
mdns_txt_add(GSList *ret, const char *name, const char *value, gboolean replace)
{
	ResourceRecordRDataTXTNode *node;

	node = mdns_txt_find(ret, name);

	if (node == NULL) {
		/* Add a new node */
		node = g_malloc(sizeof(ResourceRecordRDataTXTNode));
		node->name = g_strdup(name);
		node->value = value != NULL ? g_strdup(value) : NULL;
		ret = g_slist_append(ret, node);
	} else {
		/* Replace the value of the old node, or do nothing */
		if (replace) {
			g_free(node->value);
			node->value = value != NULL ? g_strdup(value) : NULL;
		}
	}

	return ret;
}

/**
 * Read in a list of name=value pairs as a GSList of
 * ResourceRecordRDataTXTNodes.
 *
 */
static GSList *
mdns_read_rr_rdata_txt(const char *data, unsigned int datalen, unsigned int offset, unsigned short rdlength)
{
	GSList *ret = NULL;
	int endoffset = offset + rdlength;
	unsigned char tmp;
	char buf[256], *name, *value;

	while (offset < endoffset) {
		/* Read in the length of the next name/value pair */
		tmp = util_get8(&data[offset]);
		offset++;

		/* Malformed packet check */
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

		/* Make the name all lowercase */
		name = g_utf8_strdown(buf, -1);
		ret = mdns_txt_add(ret, name, value, FALSE);
		g_free(name);
	}

	/* Malformed packet check */
	if ((offset > datalen) || (offset != endoffset)) {
		mdns_free_rr_rdata(RENDEZVOUS_RRTYPE_TXT, ret);
		return NULL;
	}

	return ret;
}

/**
 *
 *
 */
static ResourceRecordRDataSRV *
mdns_read_rr_rdata_srv(const char *data, unsigned int datalen, unsigned int offset, unsigned short rdlength)
{
	ResourceRecordRDataSRV *ret = NULL;
	int endoffset = offset + rdlength;

	/* Malformed packet check */
	if (offset + 7 > endoffset)
		return NULL;

	ret = g_malloc(sizeof(ResourceRecordRDataSRV));

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
	offset += mdns_read_name_len(data, datalen, offset);

	/* Malformed packet check */
	if ((offset > endoffset) || (ret->target == NULL)) {
		g_free(ret->target);
		g_free(ret);
		return NULL;
	}

	return ret;
}

/**
 *
 *
 */
static ResourceRecord *
mdns_read_rr(const char *data, unsigned int datalen, int *offset)
{
	ResourceRecord *rr;

	rr = (ResourceRecord *)g_malloc0(sizeof(ResourceRecord));

	/* NAME */
	rr->name = mdns_read_name(data, datalen, *offset);
	*offset += mdns_read_name_len(data, datalen, *offset);

	/* Malformed packet check */
	if (*offset + 10 > datalen) {
		mdns_free_rr(rr);
		return NULL;
	}

	/* TYPE */
	rr->type = util_get16(&data[*offset]);
	*offset += 2;

	/* CLASS */
	rr->class = util_get16(&data[*offset]);
	*offset += 2;

	/* TTL */
	rr->ttl = util_get32(&data[*offset]);
	*offset += 4;

	/* RDLENGTH */
	rr->rdlength = util_get16(&data[*offset]);
	*offset += 2;

	/* RDATA */
	if (rr->type == RENDEZVOUS_RRTYPE_A) {
		rr->rdata = mdns_read_rr_rdata_null(data, datalen, *offset, rr->rdlength);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}

	} else if (rr->type == RENDEZVOUS_RRTYPE_NULL) {
		rr->rdata = mdns_read_rr_rdata_null(data, datalen, *offset, rr->rdlength);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}

	} else if (rr->type == RENDEZVOUS_RRTYPE_PTR) {
		rr->rdata = mdns_read_rr_rdata_ptr(data, datalen, *offset);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}
	
	} else if (rr->type == RENDEZVOUS_RRTYPE_TXT) {
		rr->rdata = mdns_read_rr_rdata_txt(data, datalen, *offset, rr->rdlength);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}

	} else if (rr->type == RENDEZVOUS_RRTYPE_AAAA) {
		rr->rdata = mdns_read_rr_rdata_null(data, datalen, *offset, rr->rdlength);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}

	} else if (rr->type == RENDEZVOUS_RRTYPE_SRV) {
		rr->rdata = mdns_read_rr_rdata_srv(data, datalen, *offset, rr->rdlength);
		if (rr->rdata == NULL) {
			mdns_free_rr(rr);
			return NULL;
		}

	}

	/* Malformed packet check */
	*offset += rr->rdlength;
	if (*offset > datalen) {
		mdns_free_rr(rr);
		return NULL;
	}

	return rr;
}

static GSList *
mdns_read_rrs(int numrecords, const char *data, unsigned int datalen, int *offset)
{
	GSList *ret = NULL;
	ResourceRecord *rr;
	int i;

	for (i = 0; i < numrecords; i++) {
		rr = mdns_read_rr(data, datalen, offset);
		if (rr == NULL)
			break;
		ret = g_slist_append(ret, rr);
	}

	/* Malformed packet check */
	if (i < numrecords) {
		mdns_free_rrs(ret);
		return NULL;
	}

	return ret;
}

/**
 * If invalid data is encountered at any point when parsing data
 * then the entire packet is discarded and NULL is returned.
 */
DNSPacket *
mdns_read(int fd)
{
	DNSPacket *dns = NULL;
	int offset; /* Current position in datagram */
	/* XXX - Find out what to use as a maximum incoming UDP packet size */
	/* XXX - Would making this static increase performance? */
	/* char data[512]; */
	char data[10096];
	unsigned int datalen;
	struct sockaddr_in addr;
	socklen_t addrlen;
	GSList *cur;

	/* Read in an mDNS packet */
	addrlen = sizeof(struct sockaddr_in);
	if ((datalen = recvfrom(fd, data, sizeof(data), 0, (struct sockaddr *)&addr, &addrlen)) == -1) {
		gaim_debug_error("mdns", "Error reading packet: %d\n", errno);
		return NULL;
	}

	dns = (DNSPacket *)g_malloc0(sizeof(DNSPacket));

	/* Parse the incoming packet, starting from 0 */
	offset = 0;

	if (offset + 12 > datalen) {
		g_free(dns);
		return NULL;
	}

	/* The header section */
	dns->header.id = util_get16(&data[offset]); /* ID */
	offset += 2;

	/* For the flags, some bits must be 0 and some must be 1, the rest are ignored */
	dns->header.flags = util_get16(&data[offset]); /* Flags (QR, OPCODE, AA, TC, RD, RA, Z, AD, CD, and RCODE */
	offset += 2;
	if ((dns->header.flags & 0x7800) != 0) {
		/* OPCODE should be all 0's */
		g_free(dns);
		return NULL;
	}

	/* Read in the number of other things in the packet */
	dns->header.numquestions = util_get16(&data[offset]);
	offset += 2;
	dns->header.numanswers = util_get16(&data[offset]);
	offset += 2;
	dns->header.numauthority = util_get16(&data[offset]);
	offset += 2;
	dns->header.numadditional = util_get16(&data[offset]);
	offset += 2;

	/* Read in all the questions */
	dns->questions = mdns_read_questions(dns->header.numquestions, data, datalen, &offset);

	/* Read in all resource records */
	dns->answers = mdns_read_rrs(dns->header.numanswers, data, datalen, &offset);

	/* Read in all authority records */
	dns->authority = mdns_read_rrs(dns->header.numauthority, data, datalen, &offset);

	/* Read in all additional records */
	dns->additional = mdns_read_rrs(dns->header.numadditional, data, datalen, &offset);

	/* Malformed packet check */
	if ((dns->header.numquestions > 0 && dns->questions == NULL) ||
		(dns->header.numanswers > 0 && dns->answers == NULL) ||
		(dns->header.numauthority > 0 && dns->authority == NULL) ||
		(dns->header.numadditional > 0 && dns->additional == NULL)) {
		gaim_debug_error("mdns", "Received an invalid DNS packet.\n");
		return NULL;
	}

	/* We should be at the end of the packet */
	if (offset != datalen) {
		gaim_debug_error("mdns", "Finished parsing before end of DNS packet!  Only parsed %d of %d bytes.", offset, datalen);
		g_free(dns);
		return NULL;
	}

	for (cur = dns->answers; cur != NULL; cur = cur->next)
		mdns_cache_add((ResourceRecord *)cur->data);
	for (cur = dns->authority; cur != NULL; cur = cur->next)
		mdns_cache_add((ResourceRecord *)cur->data);
	for (cur = dns->additional; cur != NULL; cur = cur->next)
		mdns_cache_add((ResourceRecord *)cur->data);
	for (cur = dns->questions; cur != NULL; cur = cur->next)
		mdns_cache_respond(fd, (Question *)cur->data);

	return dns;
}

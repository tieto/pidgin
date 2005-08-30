/**
 * @file stun.c STUN (RFC3489) Implementation
 * @ingroup core
 *
 * gaim
 *
 * STUN implementation inspired by jstun [http://jstun.javawi.de/]
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

#ifndef _WIN32
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <resolv.h>
#else
#include "libc_interface.h"
#endif

#include "internal.h"

#include "debug.h"
#include "account.h"
#include "stun.h"
#include "prefs.h"

struct stun_nattype nattype = {-1, 0, "\0"};

static GSList *callbacks = 0;
static int fd = -1;
static gint incb = -1;
static gint timeout = -1;
static struct stun_header *packet;
static int packetsize = 0;
static int test = 0;
static int retry = 0;
static struct sockaddr_in addr;

static void do_callbacks() {
	while(callbacks) {
		StunCallback cb = callbacks->data;
		if(cb)
			cb(&nattype);
		callbacks = g_slist_remove(callbacks, cb);
	}
}

static gboolean timeoutfunc(void *blah) {
	if(retry > 2) {
		if(test == 2) nattype.type = 5;
		/* remove input */
		gaim_input_remove(incb);
	
		/* set unknown */
		nattype.status = 0;

		/* callbacks */
		do_callbacks();

		return FALSE;
	}
	retry++;
	sendto(fd, packet, packetsize, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	return TRUE;
}

#ifdef NOTYET
static void do_test2() {
	struct stun_change data;
        data.hdr.type = htons(0x0001);
        data.hdr.len = 0;
        data.hdr.transid[0] = rand();
        data.hdr.transid[1] = ntohl(((int)'g' << 24) + ((int)'a' << 16) + ((int)'i' << 8) + (int)'m');
        data.hdr.transid[2] = rand();
        data.hdr.transid[3] = rand();							data.attrib.type = htons(0x003);
	data.attrib.len = htons(4);
	data.value[3] = 6;
	packet = (struct stun_header*)&data;
	packetsize = sizeof(struct stun_change);
	retry = 0;
	test = 2;
	sendto(fd, packet, packetsize, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
        timeout = gaim_timeout_add(500, (GSourceFunc)timeoutfunc, NULL);
}
#endif

static void reply_cb(gpointer data, gint source, GaimInputCondition cond) {
	char buffer[1024];
	char *tmp;
	int len;
	struct in_addr in;
	struct stun_attrib *attrib;
	struct stun_header *hdr;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_in *sinptr;
	
	len = recv(source, buffer, 1024, 0);

	hdr = (struct stun_header*)buffer;
	if(hdr->transid[0]!=packet->transid[0] || hdr->transid[1]!=packet->transid[1] || hdr->transid[2]!=packet->transid[2] || hdr->transid[3]!=packet->transid[3]) { /* wrong transaction */
		gaim_debug_info("stun", "got wrong transid\n");
		return;
	}
	if(test==1) {	
		tmp = buffer + sizeof(struct stun_header);
		while(buffer+len > tmp) {

			attrib = (struct stun_attrib*) tmp;
			if(attrib->type == htons(0x0001) && attrib->len == htons(8)) {
				memcpy(&in.s_addr, tmp+sizeof(struct stun_attrib)+2+2, 4);
				strcpy(nattype.publicip, inet_ntoa(in));
			}
			tmp += sizeof(struct stun_attrib) + attrib->len;
		}
		gaim_debug_info("stun", "got public ip %s\n", nattype.publicip);
		nattype.status = 2;
		nattype.type = 1;

		/* is it a NAT? */

		ifc.ifc_len = sizeof(buffer);
		ifc.ifc_req = (struct ifreq *) buffer;
		ioctl(source, SIOCGIFCONF, &ifc);

		tmp = buffer;
		while(tmp < buffer + ifc.ifc_len) {
			ifr = (struct ifreq *) tmp;

			tmp += sizeof(struct ifreq);

			if(ifr->ifr_addr.sa_family == AF_INET) {
				/* we only care about ipv4 interfaces */
				sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
				if(sinptr->sin_addr.s_addr == in.s_addr) {
					/* no NAT */
					gaim_debug_info("stun", "no nat");
					nattype.type = 0;
				}
			}
		}
		gaim_timeout_remove(timeout);

#ifdef NOTYET
		do_test2();
#endif
		return;
	} else if(test == 2) {
		do_callbacks();
		gaim_input_remove(incb);
		gaim_timeout_remove(timeout);
		nattype.type = 2;
	}
}
	
struct stun_nattype *gaim_stun_discover(StunCallback cb) {
	static struct stun_header data;
	int ret = 0;
	const char *ip = gaim_prefs_get_string("/core/network/stun_ip");
	int port = gaim_prefs_get_int("/core/network/stun_port");

	if(!ip || !port) {
		nattype.status = 0;
		if(cb) cb(&nattype);
		return &nattype;
	}
	
	if(nattype.status == 1) { /* currently discovering */
		if(cb) callbacks = g_slist_append(callbacks, cb);
		return NULL;
	}
	if(nattype.status != -1) { /* already discovered */
		if(cb) cb(&nattype);
		return &nattype;
	}
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		nattype.status = 0;
		if(cb) cb(&nattype);
		return &nattype;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(12108);
	addr.sin_addr.s_addr = INADDR_ANY;
	while( ((ret = bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) < 0 ) && ntohs(addr.sin_port) < 12208) {
		addr.sin_port = htons(ntohs(addr.sin_port)+1);
	}
	if( ret < 0 ) {
		nattype.status = 0;
		if(cb) cb(&nattype);
		return &nattype;
	}
	incb = gaim_input_add(fd, GAIM_INPUT_READ, reply_cb, NULL);
	
	if(port == 0 || ip == NULL || ip[0] == '\0') return NULL;
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	data.type = htons(0x0001);
	data.len = 0;
	data.transid[0] = rand();
	data.transid[1] = ntohl(((int)'g' << 24) + ((int)'a' << 16) + ((int)'i' << 8) + (int)'m');
	data.transid[2] = rand();
	data.transid[3] = rand();
	
	if( sendto(fd, &data, sizeof(struct stun_header), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < sizeof(struct stun_header)) {
			nattype.status = 0;
			if(cb) cb(&nattype);
			return &nattype;
	}
	test = 1;
	packet = &data;
	packetsize = sizeof(struct stun_header);
	if(cb) callbacks = g_slist_append(callbacks, cb);
	timeout = gaim_timeout_add(500, (GSourceFunc)timeoutfunc, NULL);
	return NULL;
}

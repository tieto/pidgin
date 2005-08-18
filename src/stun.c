/**
 * @file stun.c STUN (RFC3489) Implementation
 * @ingroup core
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
 * TODO: currently only detects if there is a NAT and not the type of NAT
 */

#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <resolv.h>

#include "internal.h"

#include "debug.h"
#include "account.h"
#include "stun.h"
#include "prefs.h"

struct stun_nattype nattype = {-1, 0, "\0"};

static gint transid[4];

static GSList *callbacks = 0;
static int fd = -1;
gint incb = -1;

static void do_callbacks() {
	while(callbacks) {
		StunCallback cb = callbacks->data;
		cb(&nattype);
		callbacks = g_slist_remove(callbacks, cb);
	}
}

static void reply_cb(gpointer data, gint source, GaimInputCondition cond) {
	char buffer[10240];
	char *tmp;
	int len;
	struct in_addr in;
	struct stun_attrib *attrib;
	struct stun_header *hdr;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr_in *sinptr;
	
	len = recv(source, buffer, 10240, 0);

	hdr = (struct stun_header*)buffer;
	if(hdr->transid[0]!=transid[0] || hdr->transid[1]!=transid[1] || hdr->transid[2]!=transid[2] || hdr->transid[3]!=transid[3]) { // wrong transaction
		gaim_debug_info("simple", "got wrong transid\n");
		return;
	}
	
	tmp = buffer + sizeof(struct stun_header);
	while(buffer+len > tmp) {

		attrib = (struct stun_attrib*) tmp;
		if(attrib->type == htons(0x0001) && attrib->len == htons(8)) {
			memcpy(&in.s_addr, tmp+sizeof(struct stun_attrib)+2+2, 4);
			strcpy(nattype.publicip, inet_ntoa(in));
		}
		tmp += sizeof(struct stun_attrib) + attrib->len;
	}
	gaim_debug_info("simple", "got public ip %s\n",nattype.publicip);
	nattype.status = 2;
	nattype.type = 1;
	
	// is it a NAT?

	ifc.ifc_len = sizeof(buffer);
	ifc.ifc_req = (struct ifreq *) buffer;
	ioctl(source, SIOCGIFCONF, &ifc);

	tmp = buffer;
	while(tmp < buffer + ifc.ifc_len) {
		ifr = (struct ifreq *) tmp;
		len = sizeof(struct sockaddr);

		tmp += sizeof(ifr->ifr_name) + len;

		if(ifr->ifr_addr.sa_family == AF_INET) {
			// we only care about ipv4 interfaces
			sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
			if(sinptr->sin_addr.s_addr == in.s_addr) {
				// no NAT
				gaim_debug_info("simple","no nat");
				nattype.type = 0;
			}
		}
	}
				
	do_callbacks();
	gaim_input_remove(incb);
}

struct stun_nattype *gaim_stun_discover(StunCallback cb) {
	struct sockaddr_in addr;
	struct stun_header data;
	int ret = 0;
	const char *ip = gaim_prefs_get_string("/core/network/stun_ip");
	int port = gaim_prefs_get_int("/core/network/stun_port");
	
	if(nattype.status == 1) { // currently discovering
		callbacks = g_slist_append(callbacks, cb);
		return NULL;
	}
	if(nattype.status == 2 || nattype.status == 0) { // already discovered
		cb(&nattype);
		return &nattype;
	}

	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		nattype.status = 0;
		cb(&nattype);
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
		cb(&nattype);
		return &nattype;
	}
	incb = gaim_input_add(fd, GAIM_INPUT_READ, reply_cb, NULL);
	
	if(port == 0 || ip == NULL || ip[0] == '\0') return NULL;
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	data.type = htons(0x0001);
	data.len = 0;
	transid[0] = data.transid[0] = rand();
	transid[1] = data.transid[1] = ntohl(((int)'g' << 24) + ((int)'a' << 16) + ((int)'i' << 8) + (int)'m');
	transid[2] = data.transid[2] = rand();
	transid[3] = data.transid[3] = rand();
	
	if( sendto(fd, &data, sizeof(struct stun_header), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < sizeof(struct stun_header)) {
			nattype.status = 0;
			cb(&nattype);
			return &nattype;
	}
	return NULL;
}

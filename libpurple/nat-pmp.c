/**
 * @file nat-pmp.c NAT-PMP Implementation
 * @ingroup core
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Most code in nat-pmp.c copyright (C) 2007, R. Tyler Ballance, bleep, LLC.
 * This file is distributed under the 3-clause (modified) BSD license:
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 * the following disclaimer.
 * Neither the name of the bleep. LLC nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include "nat-pmp.h"
#include "debug.h"

#include <arpa/inet.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <errno.h>
#include <sys/types.h>
#include <net/if.h>

#ifdef NET_RT_DUMP2

#define PMP_DEBUG 1

/*
 *	Thanks to R. Matthew Emerson for the fixes on this
 */

#define PMP_MAP_OPCODE_UDP	1
#define PMP_MAP_OPCODE_TCP	2

#define PMP_VERSION			0
#define PMP_PORT			5351
#define PMP_TIMEOUT			250000	//	250000 useconds

/* alignment constraint for routing socket */
#define ROUNDUP(a)			((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n)		(x += ROUNDUP((n)->sa_len))

static void
get_rtaddrs(int bitmask, struct sockaddr *sa, struct sockaddr *addrs[])
{
	int i;
	
	for (i = 0; i < RTAX_MAX; i++) 
	{
		if (bitmask & (1 << i)) 
		{
			addrs[i] = sa;
			sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
		} 
		else
		{
			addrs[i] = NULL;
		}
	}
}

static int
is_default_route(struct sockaddr *sa, struct sockaddr *mask)
{
    struct sockaddr_in *sin;
	
    if (sa->sa_family != AF_INET)
		return 0;
	
    sin = (struct sockaddr_in *)sa;
    if ((sin->sin_addr.s_addr == INADDR_ANY) &&
		mask &&
		(ntohl(((struct sockaddr_in *)mask)->sin_addr.s_addr) == 0L ||
		 mask->sa_len == 0))
		return 1;
    else
		return 0;
}

/*!
 * The return sockaddr_in must be g_free()'d when no longer needed
 */
static struct sockaddr_in *
default_gw()
{
	int mib[6];
    size_t needed;
    char *buf, *next, *lim;
    struct rt_msghdr2 *rtm;
    struct sockaddr *sa;
	struct sockaddr_in *sin = NULL;
	gboolean found = FALSE;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE; /* entire routing table or a subset of it */
    mib[2] = 0; /* protocol number - always 0 */
    mib[3] = 0; /* address family - 0 for all addres families */
    mib[4] = NET_RT_DUMP2;
    mib[5] = 0;
	
	/* Determine the buffer side needed to get the full routing table */
    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) 
	{
		purple_debug_warning("nat-pmp", "sysctl: net.route.0.0.dump estimate");
		return NULL;
    }
	
    if (!(buf = malloc(needed)))
	{
		purple_debug_warning("nat-pmp", "malloc");
		return NULL;
    }
	
	/* Read the routing table into buf */
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) 
	{
		purple_debug_warning("nat-pmp", "sysctl: net.route.0.0.dump");
		return NULL;
    }
	
    lim = buf + needed;
	
    for (next = buf; next < lim; next += rtm->rtm_msglen) 
	{
		rtm = (struct rt_msghdr2 *)next;
		sa = (struct sockaddr *)(rtm + 1);
		
		if (sa->sa_family == AF_INET) 
		{
			sin = (struct sockaddr_in*) sa;

			if ((rtm->rtm_flags & RTF_GATEWAY) && sin->sin_addr.s_addr == INADDR_ANY)
			{
				/* We found the default route. Now get the destination address and netmask. */
	            struct sockaddr *rti_info[RTAX_MAX];
				struct sockaddr addr, mask;

				get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
				bzero(&addr, sizeof(addr));
				
				if (rtm->rtm_addrs & RTA_DST)
					bcopy(rti_info[RTAX_DST], &addr, rti_info[RTAX_DST]->sa_len);
				
				bzero(&mask, sizeof(mask));
				
				if (rtm->rtm_addrs & RTA_NETMASK)
					bcopy(rti_info[RTAX_NETMASK], &mask, rti_info[RTAX_NETMASK]->sa_len);
				
				if (rtm->rtm_addrs & RTA_GATEWAY &&
					is_default_route(&addr, &mask)) 
				{					
					if (rti_info[RTAX_GATEWAY]) {
						struct sockaddr_in *rti_sin = (struct sockaddr_in *)rti_info[RTAX_GATEWAY];
						sin = g_new0(struct sockaddr_in, 1);
						sin->sin_family = rti_sin->sin_family;
						sin->sin_port = rti_sin->sin_port;
						sin->sin_addr.s_addr = rti_sin->sin_addr.s_addr;
						memcpy(sin, rti_info[RTAX_GATEWAY], sizeof(struct sockaddr_in));

						purple_debug_info("nat-pmp", "found a default gateway");
						found = TRUE;
						break;
					}
				}
			}
		}
    }

	return (found ? sin : NULL);
}

/*!
 * double_timeout(struct timeval *) will handle doubling a timeout for backoffs required by NAT-PMP
 */
static void
double_timeout(struct timeval *to)
{
	int second = 1000000; // number of useconds
	
	to->tv_sec = (to->tv_sec * 2);
	to->tv_usec = (to->tv_usec * 2);
	
	// Overflow useconds if necessary
	if (to->tv_usec >= second)
	{
		int overflow = (to->tv_usec / second);
		to->tv_usec  = (to->tv_usec - (overflow * second));
		to->tv_sec = (to->tv_sec + overflow);
	}
}

/*!
 *	purple_pmp_get_public_ip() will return the publicly facing IP address of the 
 *	default NAT gateway. The function will return NULL if:
 *		- The gateway doesn't support NAT-PMP
 *		- The gateway errors in some other spectacular fashion
 */
char *
purple_pmp_get_public_ip()
{
	struct sockaddr_in *gateway = default_gw();
	
	if (gateway == NULL)
	{
		purple_debug_info("nat-pmp", "Cannot request public IP from a NULL gateway!\n");
		return NULL;
	}
	if (gateway->sin_port != PMP_PORT)
	{
		gateway->sin_port = htons(PMP_PORT); //	Default port for NAT-PMP is 5351
	}

	int sendfd;
	int req_attempts = 1;	
	struct timeval req_timeout;
	PurplePmpIpRequest req;
	PurplePmpIpResponse resp;
	struct sockaddr_in *publicsockaddr = NULL;

	req_timeout.tv_sec = 0;
	req_timeout.tv_usec = PMP_TIMEOUT;

	sendfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	//	Clean out both req and resp structures
	bzero(&req, sizeof(PurplePmpIpRequest));
	bzero(&resp, sizeof(PurplePmpIpResponse));
	req.version = 0;
	req.opcode	= 0;
	
	//	Attempt to contact NAT-PMP device 9 times as per: draft-cheshire-nat-pmp-02.txt  
	while (req_attempts < 10)
	{	
#ifdef PMP_DEBUG
		purple_debug_info("nat-pmp", "Attempting to retrieve the public ip address for the NAT device at: %s\n", inet_ntoa(gateway->sin_addr));
		purple_debug_info("nat-pmp", "\tTimeout: %ds %dus, Request #: %d\n", req_timeout.tv_sec, req_timeout.tv_usec, req_attempts);
#endif
		struct sockaddr_in addr;
		socklen_t len = sizeof(struct sockaddr_in);

		/* TODO: Non-blocking! */
		if (sendto(sendfd, &req, sizeof(req), 0, (struct sockaddr *)(gateway), sizeof(struct sockaddr)) < 0)
		{
			purple_debug_info("nat-pmp", "There was an error sending the NAT-PMP public IP request! (%s)\n", strerror(errno));
			return NULL;
		}
		
		if (setsockopt(sendfd, SOL_SOCKET, SO_RCVTIMEO, &req_timeout, sizeof(req_timeout)) < 0)
		{
			purple_debug_info("nat-pmp", "There was an error setting the socket's options! (%s)\n", strerror(errno));
			return NULL;
		}		
		
		/* TODO: Non-blocking! */
		if (recvfrom(sendfd, &resp, sizeof(PurplePmpIpResponse), 0, (struct sockaddr *)(&addr), &len) < 0)
		{			
			if ( (errno != EAGAIN) || (req_attempts == 9) )
			{
				purple_debug_info("nat-pmp", "There was an error receiving the response from the NAT-PMP device! (%s)\n", strerror(errno));
				return NULL;
			}
			else
			{
				goto iterate;
			}
		}
		
		if (addr.sin_addr.s_addr != gateway->sin_addr.s_addr)
		{
			purple_debug_info("nat-pmp", "Response was not received from our gateway! Instead from: %s\n", inet_ntoa(addr.sin_addr));
			goto iterate;
		}
		else
		{
			publicsockaddr = &addr;
			break;
		}

iterate:
		++req_attempts;
		double_timeout(&req_timeout);
	}

	if (publicsockaddr == NULL) {
		g_free(gateway);
		return NULL;
	}
	
#ifdef PMP_DEBUG
	purple_debug_info("nat-pmp", "Response received from NAT-PMP device:\n");
	purple_debug_info("nat-pmp", "version: %d\n", resp.version);
	purple_debug_info("nat-pmp", "opcode: %d\n", resp.opcode);
	purple_debug_info("nat-pmp", "resultcode: %d\n", ntohs(resp.resultcode));
	purple_debug_info("nat-pmp", "epoch: %d\n", ntohl(resp.epoch));
	struct in_addr in;
	in.s_addr = resp.address;
	purple_debug_info("nat-pmp", "address: %s\n", inet_ntoa(in));
#endif	

	publicsockaddr->sin_addr.s_addr = resp.address;

	g_free(gateway);

	return inet_ntoa(publicsockaddr->sin_addr);
}

/*!
 *	will return NULL on error, or a pointer to the PurplePmpMapResponse type
 */
PurplePmpMapResponse *
purple_pmp_create_map(PurplePmpType type, uint16_t privateport, uint16_t publicport, uint32_t lifetime)
{
	struct sockaddr_in *gateway = default_gw();
	
	if (gateway == NULL)
	{
		purple_debug_info("nat-pmp", "Cannot create mapping on a NULL gateway!\n");
		return NULL;
	}
	if (gateway->sin_port != PMP_PORT)
	{
		gateway->sin_port = htons(PMP_PORT); //	Default port for NAT-PMP is 5351
	}
		
	int sendfd;
	int req_attempts = 1;	
	struct timeval req_timeout;
	PurplePmpMapRequest req;
	PurplePmpMapResponse *resp = (PurplePmpMapResponse *)(malloc(sizeof(PurplePmpMapResponse)));
	
	req_timeout.tv_sec = 0;
	req_timeout.tv_usec = PMP_TIMEOUT;
	
	sendfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	//	Clean out both req and resp structures
	bzero(&req, sizeof(PurplePmpMapRequest));
	bzero(resp, sizeof(PurplePmpMapResponse));
	req.version = 0;
	req.opcode	= ((type == PURPLE_PMP_TYPE_UDP) ? PMP_MAP_OPCODE_UDP : PMP_MAP_OPCODE_TCP);	
	req.privateport = htons(privateport); //	What a difference byte ordering makes...d'oh!
	req.publicport = htons(publicport);
	req.lifetime = htonl(lifetime);
	
	//	Attempt to contact NAT-PMP device 9 times as per: draft-cheshire-nat-pmp-02.txt  
	while (req_attempts < 10)
	{	
#ifdef PMP_DEBUG
		purple_debug_info("nat-pmp", "Attempting to create a NAT-PMP mapping the private port %d, and the public port %d\n", privateport, publicport);
		purple_debug_info("nat-pmp", "\tTimeout: %ds %dus, Request #: %d\n", req_timeout.tv_sec, req_timeout.tv_usec, req_attempts);
#endif

		/* TODO: Non-blocking! */
		if (sendto(sendfd, &req, sizeof(req), 0, (struct sockaddr *)(gateway), sizeof(struct sockaddr)) < 0)
		{
			purple_debug_info("nat-pmp", "There was an error sending the NAT-PMP mapping request! (%s)\n", strerror(errno));
			return NULL;
		}
		
		if (setsockopt(sendfd, SOL_SOCKET, SO_RCVTIMEO, &req_timeout, sizeof(req_timeout)) < 0)
		{
			purple_debug_info("nat-pmp", "There was an error setting the socket's options! (%s)\n", strerror(errno));
			return NULL;
		}		
		
		/* TODO: Non-blocking! */
		if (recvfrom(sendfd, resp, sizeof(PurplePmpMapResponse), 0, NULL, NULL) < 0)
		{			
			if ( (errno != EAGAIN) || (req_attempts == 9) )
			{
				purple_debug_info("nat-pmp", "There was an error receiving the response from the NAT-PMP device! (%s)\n", strerror(errno));
				return NULL;
			}
			else
			{
				goto iterate;
			}
		}
		
		if (resp->opcode != (req.opcode + 128))
		{
			purple_debug_info("nat-pmp", "The opcode for the response from the NAT device does not match the request opcode!\n");
			goto iterate;
		}
		
		break;
		
iterate:
		++req_attempts;
		double_timeout(&req_timeout);
	}
	
#ifdef PMP_DEBUG
	purple_debug_info("nat-pmp", "Response received from NAT-PMP device:\n");
	purple_debug_info("nat-pmp", "version: %d\n", resp->version);
	purple_debug_info("nat-pmp", "opcode: %d\n", resp->opcode);
	purple_debug_info("nat-pmp", "resultcode: %d\n", ntohs(resp->resultcode));
	purple_debug_info("nat-pmp", "epoch: %d\n", ntohl(resp->epoch));
	purple_debug_info("nat-pmp", "privateport: %d\n", ntohs(resp->privateport));
	purple_debug_info("nat-pmp", "publicport: %d\n", ntohs(resp->publicport));
	purple_debug_info("nat-pmp", "lifetime: %d\n", ntohl(resp->lifetime));
#endif	

	g_free(gateway);

	return resp;
}

/*!
 *	pmp_destroy_map(uint8_t,uint16_t) 
 *	will return NULL on error, or a pointer to the PurplePmpMapResponse type
 */
PurplePmpMapResponse *
purple_pmp_destroy_map(PurplePmpType type, uint16_t privateport)
{
	PurplePmpMapResponse *response;
	
	response = purple_pmp_create_map(((type == PURPLE_PMP_TYPE_UDP) ? PMP_MAP_OPCODE_UDP : PMP_MAP_OPCODE_TCP),
							privateport, 0, 0);
	if (!response)
	{
		purple_debug_info("nat-pmp", "Failed to properly destroy mapping for %d!\n", privateport);
		return NULL;
	}
	else
	{
		return response;
	}
}
#else /* #ifdef NET_RT_DUMP2 */
char *
purple_pmp_get_public_ip()
{
	return NULL;
}

PurplePmpMapResponse *
purple_pmp_create_map(PurplePmpType type, uint16_t privateport, uint16_t publicport, uint32_t lifetime)
{
	return NULL;
}

PurplePmpMapResponse *
purple_pmp_destroy_map(PurplePmpType type, uint16_t privateport)
{
	return NULL;
}
#endif /* #ifndef NET_RT_DUMP2 */

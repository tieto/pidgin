/*
 * gaim
 *
 * File: libc_internal.h
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
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
#ifndef _LIBC_INTERNAL_
#define _LIBC_INTERNAL_

/* fcntl.h */
#define F_SETFL 1
#define O_NONBLOCK 1

/* ioctl.h */
#define SIOCGIFCONF 0x8912 /* get iface list */

/* net/if.h */
struct ifreq
{
	union
	{
		char ifrn_name[6];	/* Interface name, e.g. "en0".  */
	} ifr_ifrn;

	union
	{
		struct sockaddr ifru_addr;
#if 0
		struct sockaddr ifru_dstaddr;
		struct sockaddr ifru_broadaddr;
		struct sockaddr ifru_netmask;
		struct sockaddr ifru_hwaddr;
		short int ifru_flags;
		int ifru_ivalue;
		int ifru_mtu;
#endif
		char *ifru_data;
	} ifr_ifru;
};
# define ifr_name	ifr_ifrn.ifrn_name	/* interface name       */
# define ifr_addr	ifr_ifru.ifru_addr      /* address              */
#if 0
# define ifr_hwaddr	ifr_ifru.ifru_hwaddr	/* MAC address          */
# define ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-p lnk */
# define ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address    */
# define ifr_netmask	ifr_ifru.ifru_netmask	/* interface net mask   */
# define ifr_flags	ifr_ifru.ifru_flags	/* flags                */
# define ifr_metric	ifr_ifru.ifru_ivalue	/* metric               */
# define ifr_mtu	ifr_ifru.ifru_mtu	/* mtu                  */
#endif
# define ifr_data	ifr_ifru.ifru_data	/* for use by interface */
#if 0
# define ifr_ifindex	ifr_ifru.ifru_ivalue	/* interface index      */
# define ifr_bandwidth	ifr_ifru.ifru_ivalue	/* link bandwidth       */
# define ifr_qlen	ifr_ifru.ifru_ivalue	/* queue length         */
#endif


struct ifconf
{
	int ifc_len;			/* Size of buffer.  */
	union
	{
		char *ifcu_buf;
		struct ifreq *ifcu_req;
	} ifc_ifcu;
};
# define ifc_buf ifc_ifcu.ifcu_buf /* Buffer address.  */
# define ifc_req ifc_ifcu.ifcu_req /* Array of structures.  */

/* sys/time.h */
struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};


#endif /* _LIBC_INTERNAL_ */

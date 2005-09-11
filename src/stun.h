/**
 * @file stun.h STUN API
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
 */
#ifndef _GAIM_STUN_H_
#define _GAIM_STUN_H_

/**************************************************************************/
/** @name STUN API                                                        */
/**************************************************************************/

struct stun_nattype {
	gint status;	// 0 - unknown (no STUN server reachable), 1 - discovering, 2 - discovered
	gint type;	// 0 - public ip
			// 1 - NAT (unknown type)
			// 2 - full cone
			// 3 - restricted cone
			// 4 - port restricted cone
			// 5 - symmetric
	char publicip[16];
};
	
struct stun_header {
	short	type;
	short	len;
	int	transid[4];
};

struct stun_attrib {
	short type;
	short len;
};

struct stun_change {
	struct stun_header hdr;
	struct stun_attrib attrib;
	char value[4];
};

typedef void (*StunCallback) (struct stun_nattype *);

/**
 * Starts a NAT discovery. It returns a struct stun_nattype if the discovery
 * is already done. Otherwise the callback is called when the discovery is over
 * and NULL is returned.
 *
 * @param cb A callback
 *
 * @return a struct stun_nattype which includes the public IP and the type
 *         of NAT or NULL is discovery would block
 */
struct stun_nattype *gaim_stun_discover(StunCallback cb);

void gaim_stun_init();
#endif /* _GAIM_STUN_H_ */

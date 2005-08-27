/**
 * @file ntlm.h
 * 
 * gaim
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
 *
 * ntlm structs are taken from NTLM description on 
 * http://www.innovation.ch/java/ntlm.html 
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

#ifndef _GAIM_NTLM_H
#define _GAIM_NTLM_H

#include <glib.h>
#include <stdlib.h>

struct type1_message {
	guint8    protocol[8];     // 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0'
	guint8    type;            // 0x01
	guint8    zero1[3];
	short   flags;           // 0xb203
	guint8    zero2[2];

	short   dom_len1;         // domain string length
	short   dom_len2;         // domain string length
	short   dom_off;         // domain string offset
	guint8    zero3[2];

	short   host_len1;        // host string length
	short   host_len2;        // host string length
	short   host_off;        // host string offset (always 0x20)
	guint8    zero4[2];

/*	guint8    host[*];         // host string (ASCII)
	guint8    dom[*];          // domain string (ASCII) */
};

struct type2_message {
	guint8    protocol[8];     // 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0'
	guint8    type;            // 0x02
	guint8    zero1[7];
	short   msg_len;         // 0x28
	guint8    zero2[2];
	short   flags;           // 0x8201
	guint8    zero3[2];

	guint8    nonce[8];        // nonce
	guint8    zero[8];
};

struct type3_message {
	guint8    protocol[8];     // 'N', 'T', 'L', 'M', 'S', 'S', 'P', '\0'
	guint8    type;            // 0x03
	guint8    zero1[3];

	short   lm_resp_len1;     // LanManager response length (always 0x18)
	short   lm_resp_len2;     // LanManager response length (always 0x18)
	short   lm_resp_off;     // LanManager response offset
	guint8    zero2[2];

	short   nt_resp_len1;     // NT response length (always 0x18)
	short   nt_resp_len2;     // NT response length (always 0x18)
	short   nt_resp_off;     // NT response offset
	guint8    zero3[2];

	short   dom_len1;         // domain string length
	short   dom_len2;         // domain string length
	short   dom_off;         // domain string offset (always 0x40)
	guint8    zero4[2];

	short   user_len1;        // username string length
	short   user_len2;        // username string length
	short   user_off;        // username string offset
	guint8    zero5[2];

	short   host_len1;        // host string length
	short   host_len2;        // host string length
	short   host_off;        // host string offset
	guint8    zero6[6];

	short   msg_len;         // message length
	guint8    zero7[2];

	short   flags;           // 0x8201
	guint8    zero8[2];

/*	guint8    dom[*];          // domain string (unicode UTF-16LE)
	guint8    user[*];         // username string (unicode UTF-16LE)
	guint8    host[*];         // host string (unicode UTF-16LE)
	guint8    lm_resp[*];      // LanManager response
	guint8    nt_resp[*];      // NT response*/
};

#endif /* _GAIM_NTLM_H */

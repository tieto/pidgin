/**
 * @file srvresolve.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
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

#include "srvresolve.h"

typedef union {
	HEADER hdr;
	u_char buf[1024];
} queryans;

struct getserver_return *getserver(const char *domain, const char *srv) {
	static struct getserver_return ret;
	queryans answer;
	int size;
	int qdcount;
	int ancount;
	gchar *end;
	gchar *cp;
	gchar name[256];
	gchar *bestname = NULL;
	int bestport = 5060;
	int bestpri=99999;
	int type, dlen, pref, weight, port;
	gchar *query = g_strdup_printf("%s.%s",srv,domain);

	
	size = res_query( query, C_IN, T_SRV, (u_char*)&answer, sizeof( answer));

	g_free(query);

	qdcount = ntohs(answer.hdr.qdcount);
	ancount = ntohs(answer.hdr.ancount);

	
	cp = (char*)&answer + sizeof(HEADER);
	end = (char*)&answer + size;

	// skip over unwanted stuff
	while (qdcount-- > 0 && cp < end) {
		size = dn_expand( (char*)&answer, end, cp, name, 256);
		if(size < 0) return NULL;
		cp += size + QFIXEDSZ;
	}

	while (ancount-- > 0 && cp < end) {
		size = dn_expand((char*)&answer, end, cp, name, 256);
		if(size < 0)
			return NULL;

		cp += size;
	
		NS_GET16(type,cp);
		cp += 6; // skip ttl and class

		NS_GET16(dlen,cp);

		if (type == T_SRV) {
			NS_GET16(pref,cp);

			NS_GET16(weight, cp);

			NS_GET16(port, cp);

			size = dn_expand( (char*)&answer, end, cp, name, 256);
			if(size < 0 )
				return NULL;

			cp += size;

			if(pref<bestpri) {
				if( bestname) g_free(bestname);
				bestname = g_strdup(name);
				bestpri = pref;
				bestport = port;
			}
		} else {
			cp += dlen;
		}
	}
	if(bestpri < 99999) {
		ret.name = bestname;
		ret.port = bestport;
	} else {
		ret.name = g_strdup(domain);
		ret.port = 5060;
	}
	return &ret;
}

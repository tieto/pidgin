/**
 * @file srvresolve.h
 * 
 * gaim
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
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

#ifndef _GAIM_SRVRESOLVE_H
#define _GAIM_SRVRESOLVE_H

#include <glib.h>
#include <resolv.h>
#include <stdlib.h>
#include <arpa/nameser_compat.h>
#ifndef T_SRV
#define T_SRV	33
#endif

struct getserver_return {
	char *name;
	int port;
};

struct getserver_return *getserver(const char *domain, const char *srv);

#endif /* _GAIM_SRVRESOLVE_H */
